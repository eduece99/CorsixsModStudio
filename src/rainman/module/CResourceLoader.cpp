/*
Rainman Library
Copyright (C) 2006 Corsix <corsix@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "rainman/module/CResourceLoader.h"
#include "rainman/module/CModuleFile.h"
#include "rainman/archive/CSgaFile.h"
#include "rainman/localization/CUcsFile.h"
#include "rainman/module/CFileMap.h"
#include "rainman/io/CFileSystemStore.h"
#include "rainman/core/CThreadPool.h"
#include "rainman/core/Internal_Util.h"
#include "rainman/core/Exception.h"
#include "rainman/core/RainmanLog.h"
#include <future>
#include <optional>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <direct.h>

// -- Parallel archive preloading infrastructure (Phase J) --

//! Metadata for one archive to be loaded in parallel.
struct CResourceLoader::ArchiveTask
{
    std::string archivePath;
    std::string uiName;
    unsigned short iNum;
    CSgaFile **ppSgaOut; // Where to store the loaded CSgaFile*
};

//! Result of preloading an SGA file on a worker thread.
struct CResourceLoader::SgaPreloadResult
{
    CSgaFile *pSga = nullptr;
    bool bIsThisMod = true;
    char *sActualModName = nullptr; // malloc'd if set; consumer must free

    SgaPreloadResult() = default;
    ~SgaPreloadResult()
    {
        if (sActualModName)
        {
            free(sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator)
        }
    }
    SgaPreloadResult(SgaPreloadResult &&o) noexcept
        : pSga(o.pSga), bIsThisMod(o.bIsThisMod), sActualModName(o.sActualModName)
    {
        o.pSga = nullptr;
        o.sActualModName = nullptr;
    }
    SgaPreloadResult &operator=(SgaPreloadResult &&o) noexcept
    {
        if (this != &o)
        {
            pSga = o.pSga;
            bIsThisMod = o.bIsThisMod;
            sActualModName = o.sActualModName;
            o.pSga = nullptr;
            o.sActualModName = nullptr;
        }
        return *this;
    }
    SgaPreloadResult(const SgaPreloadResult &) = delete;
    SgaPreloadResult &operator=(const SgaPreloadResult &) = delete;
};

//! Loads and initializes a single SGA file without registering it in the file map.
//! Safe to call concurrently for different archive paths — each call creates
//! independent CSgaFile and IStream instances.
CResourceLoader::SgaPreloadResult CResourceLoader::PreloadSga(CFileSystemStore *pFSS, const char *sFullPath,
                                                              CModuleFile::eModuleType eModuleType,
                                                              const char *sCohThisModFolder,
                                                              const char *sApplicationPath,
                                                              const char *saScenarioPackRootFolder)
{
    CResourceLoader::SgaPreloadResult result;
    auto *pSga = CHECK_MEM(new CSgaFile);
    std::unique_ptr<IFileStore::IStream> inputStream;
    try
    {
        inputStream = std::unique_ptr<IFileStore::IStream>(pFSS->VOpenStream(sFullPath));
    }
    catch (const CRainmanException &e)
    {
        delete pSga;
        return result; // File not found — return empty result
    }

    // CoH-specific: determine if archive belongs to this mod or a dependency
    if (eModuleType == CModuleFile::MT_CompanyOfHeroes)
    {
        if (strnicmp(sCohThisModFolder, sFullPath, strlen(sCohThisModFolder)) != 0)
        {
            if (saScenarioPackRootFolder && *saScenarioPackRootFolder &&
                strnicmp(saScenarioPackRootFolder, sFullPath, strlen(saScenarioPackRootFolder)) == 0)
            {
                result.bIsThisMod = false;
                result.sActualModName = strdup(sFullPath + strlen(saScenarioPackRootFolder) + 1);
                char *sSlash = strrchr(result.sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
            }
            else
            {
                result.bIsThisMod = false;
                result.sActualModName = strdup(sFullPath + strlen(sApplicationPath));
                char *sSlash = strrchr(result.sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
                sSlash = strrchr(result.sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
            }
        }
    }

    try
    {
        pSga->Load(inputStream.get(), pFSS->VGetLastWriteTime(sFullPath));
        pSga->VInit(inputStream.get());
    }
    catch (const CRainmanException &e)
    {
        delete pSga;
        if (result.sActualModName)
        {
            free(result.sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator)
            result.sActualModName = nullptr;
        }
        throw CRainmanException(e, __FILE__, __LINE__, "(rethrow for \'%s\')", sFullPath);
    }

    (void)inputStream.release(); // CSgaFile takes ownership of the stream
    result.pSga = pSga;
    return result;
}

//! Registers a preloaded SGA into the file map (must be called on the main thread).
void CResourceLoader::RegisterPreloadedSga(SgaPreloadResult &result, CModuleFile &module, unsigned short iNum,
                                           const char *sSlashChar, const char *sFullPath)
{
    IDirectoryTraverser::IIterator *pDirItr = nullptr;
    try
    {
        pDirItr = result.pSga->VIterate(result.pSga->VGetEntryPoint(0));
        if (pDirItr)
        {
            void *pSrc;
            if (result.bIsThisMod)
            {
                pSrc = module.m_pNewFileMap->RegisterSource(module.m_iFileMapModNumber, true, iNum,
                                                            module.GetFileMapName(), sSlashChar, result.pSga,
                                                            result.pSga, false, false);
            }
            else
            {
                pSrc = module.m_pNewFileMap->RegisterSource(15000, true, iNum, result.sActualModName, sSlashChar,
                                                            result.pSga, result.pSga, false, false);
            }
            module.m_pNewFileMap->MapSga(pSrc, result.pSga);
        }
        delete pDirItr;
    }
    catch (const CRainmanException &e)
    {
        delete pDirItr;
        delete result.pSga;
        result.pSga = nullptr;
        if (result.sActualModName)
        {
            free(result.sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator)
            result.sActualModName = nullptr;
        }
        throw CRainmanException(e, __FILE__, __LINE__, "(rethrow for \'%s\')", sFullPath);
    }
    if (result.sActualModName)
    {
        free(result.sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator)
        result.sActualModName = nullptr;
    }
}

//! Loads archives in parallel (Phase 1: async preload, Phase 2: sequential registration).
void CResourceLoader::LoadArchivesParallel(std::vector<ArchiveTask> &tasks, CModuleFile &module, CALLBACK_ARG)
{
    if (tasks.empty())
    {
        return;
    }

    auto tStart = std::chrono::steady_clock::now();

    // Phase 1: Launch parallel SGA preloads via thread pool
    auto &pool = CThreadPool::Instance();
    std::vector<std::future<SgaPreloadResult>> futures;
    futures.reserve(tasks.size());
    for (auto &task : tasks)
    {
        futures.push_back(pool.Submit(&CResourceLoader::PreloadSga, module.m_pFSS, task.archivePath.c_str(),
                                      module.m_eModuleType, module.m_sCohThisModFolder.c_str(),
                                      module.m_sApplicationPath.c_str(), module.m_saScenarioPackRootFolder.c_str()));
    }

    // Phase 2: Collect results and register sequentially
    std::optional<CRainmanException> pFirstError;
    for (size_t i = 0; i < futures.size(); ++i)
    {
        SgaPreloadResult result;
        try
        {
            result = futures[i].get();
        }
        catch (const CRainmanException &e)
        {
            if (!pFirstError)
            {
                pFirstError = e;
            }
            continue;
        }

        if (!result.pSga)
        {
            *tasks[i].ppSgaOut = nullptr;
            continue;
        }

        try
        {
            CallCallback(THE_CALLBACK, "Registering archive \'%s\' for mod \'%s\'", tasks[i].uiName.c_str(),
                         module.m_sFileMapName.c_str());
            RegisterPreloadedSga(result, module, tasks[i].iNum, tasks[i].uiName.c_str(), tasks[i].archivePath.c_str());
        }
        catch (const CRainmanException &e)
        {
            if (!pFirstError)
            {
                pFirstError = e;
            }
            *tasks[i].ppSgaOut = nullptr;
            continue;
        }

        *tasks[i].ppSgaOut = result.pSga;
        result.pSga = nullptr; // Ownership transferred
    }

    auto tEnd = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
    RAINMAN_LOG_INFO("Parallel archive loading: {} archives in {}ms", tasks.size(), ms);

    if (pFirstError)
    {
        throw *pFirstError;
    }
}

// -- Free-function callbacks used with Util_ForEach --
// These are declared as friends of the relevant nested handler classes in CModuleFile.h,
// so they can access protected members directly.

void CModuleFile_ArchiveForEach(IDirectoryTraverser::IIterator *pItr, void *pModuleFile)
{
    auto *pThis = static_cast<CModuleFile *>(pModuleFile);

    auto pArchEntry = std::make_unique<CModuleFile::CArchiveHandler>();
    pArchEntry->m_iNumber = -7291;
    pArchEntry->m_sName = pItr->VGetName();
    auto *pRaw = pArchEntry.get();
    pThis->m_vArchives.push_back(std::move(pArchEntry));

    CResourceLoader::DoLoadArchive(*pThis, pItr->VGetFullPath(), &pRaw->m_pHandle, 15000, pRaw->m_sName.c_str());
}

void CModuleFile_ArchiveForEachNoErrors(IDirectoryTraverser::IIterator *pItr, void *pModuleFile)
{
    auto *pThis = static_cast<CModuleFile *>(pModuleFile);

    CSgaFile *pFile = nullptr;
    std::string sName = pItr->VGetName();
    try
    {
        CResourceLoader::DoLoadArchive(*pThis, pItr->VGetFullPath(), &pFile, 15000, sName.c_str());
    }
    catch (const CRainmanException &e)
    {
        return;
    }

    auto pArchEntry = std::make_unique<CModuleFile::CArchiveHandler>();
    pArchEntry->m_iNumber = -7291;
    pArchEntry->m_sName = std::move(sName);
    pArchEntry->m_pHandle = pFile;
    pThis->m_vArchives.push_back(std::move(pArchEntry));
}

void CModuleFile_UcsForEach(IDirectoryTraverser::IIterator *pItr, void *pModuleFile)
{
    auto *pThis = static_cast<CModuleFile *>(pModuleFile);
    const char *sName = pItr->VGetName();
    const char *sDot = strrchr(sName, '.');
    if (sDot && ((stricmp(sDot, ".ucs") == 0) || (stricmp(sDot, ".dat") == 0)))
    {
        auto pUcsEntry = std::make_unique<CModuleFile::CUcsHandler>();
        pUcsEntry->m_sName = sName;
        pUcsEntry->m_pHandle = std::make_shared<CUcsFile>();

        std::unique_ptr<IFileStore::IStream> stream;
        try
        {
            stream = std::unique_ptr<IFileStore::IStream>(pItr->VOpenFile());
            if (stricmp(sDot, ".ucs") == 0)
            {
                pUcsEntry->m_pHandle->Load(stream.get());
            }
            else
            {
                pUcsEntry->m_pHandle->LoadDat(stream.get());
            }
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__, "Error loading UCS file \'%s\'", sName);
        }

        pThis->m_vLocaleTexts.push_back(std::move(pUcsEntry));
    }
}

// -- Parallel UCS loading infrastructure --

//! Metadata for one UCS/DAT file to be loaded in parallel.
struct CResourceLoader::UcsFileInfo
{
    std::string fullPath;
    std::string name;
    bool isUcs; // true = .ucs, false = .dat
};

void CResourceLoader::LoadUcsFilesParallel(CModuleFile &module, IDirectoryTraverser::IIterator *pItr)
{
    // Phase 1 (serial): Collect file info from the directory iterator
    std::vector<UcsFileInfo> files;
    while (pItr->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
    {
        if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
        {
            const char *sName = pItr->VGetName();
            const char *sDot = strrchr(sName, '.');
            if (sDot && ((stricmp(sDot, ".ucs") == 0) || (stricmp(sDot, ".dat") == 0)))
            {
                UcsFileInfo info;
                info.fullPath = pItr->VGetFullPath();
                info.name = sName;
                info.isUcs = (stricmp(sDot, ".ucs") == 0);
                files.push_back(std::move(info));
            }
        }
        if (pItr->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
        {
            break;
        }
    }

    if (files.empty())
    {
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Phase 2 (parallel): Load CUcsFile instances via thread pool
    struct UcsLoadResult
    {
        std::shared_ptr<CUcsFile> pHandle;
        std::string name;
        std::optional<CRainmanException> pError;
    };

    auto &pool = CThreadPool::Instance();
    std::vector<std::future<UcsLoadResult>> futures;
    futures.reserve(files.size());

    for (const auto &fileInfo : files)
    {
        futures.push_back(pool.Submit(
            [&fileInfo, pFSS = module.m_pFSS]() -> UcsLoadResult
            {
                UcsLoadResult result;
                result.name = fileInfo.name;
                result.pHandle = std::make_shared<CUcsFile>();
                try
                {
                    std::unique_ptr<IFileStore::IStream> stream(pFSS->VOpenStream(fileInfo.fullPath.c_str()));
                    if (fileInfo.isUcs)
                    {
                        result.pHandle->Load(stream.get());
                    }
                    else
                    {
                        result.pHandle->LoadDat(stream.get());
                    }
                }
                catch (const CRainmanException &e)
                {
                    result.pHandle.reset();
                    result.pError = e;
                }
                return result;
            }));
    }

    // Phase 3 (serial): Collect results and register in module
    std::optional<CRainmanException> pFirstError;
    std::string sFirstErrorFile;

    for (auto &fut : futures)
    {
        UcsLoadResult result = fut.get();
        if (result.pError)
        {
            if (!pFirstError)
            {
                pFirstError = std::move(result.pError);
                sFirstErrorFile = result.name;
            }
            continue;
        }

        auto pUcsEntry = std::make_unique<CModuleFile::CUcsHandler>();
        pUcsEntry->m_sName = std::move(result.name);
        pUcsEntry->m_pHandle = std::move(result.pHandle);
        module.m_vLocaleTexts.push_back(std::move(pUcsEntry));
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    RAINMAN_LOG_INFO("Parallel UCS loading: {} files in {} ms", files.size(), elapsedMs);

    if (pFirstError)
    {
        throw CRainmanException(*pFirstError, __FILE__, __LINE__, "Error loading UCS file '%s'",
                                sFirstErrorFile.c_str());
    }
}

// -- Parallel folder pre-scanning infrastructure --

//! Metadata for one folder to be pre-scanned in parallel.
struct CResourceLoader::FolderTask
{
    std::string folderPath;
    std::string uiName;
    std::string tocName;
    unsigned short iNum;
    bool bIsDefaultWrite;
    // CoH-specific: data source pointer (null for DoW/CoHEarly)
    CModuleFile::CCohDataSource *pDataSource = nullptr;
};

CFileMap::DirEntry CResourceLoader::ScanDirectory(IDirectoryTraverser::IIterator *pItr)
{
    CFileMap::DirEntry root;
    root.isFile = false;
    root.lastWriteTime = 0;
    if (pItr == nullptr)
    {
        return root;
    }

    root.directoryPath = pItr->VGetDirectoryPath();
    root.children.reserve(32);

    auto eType = pItr->VGetType();
    while (eType != IDirectoryTraverser::IIterator::T_Nothing)
    {
        CFileMap::DirEntry entry;

        if (eType == IDirectoryTraverser::IIterator::T_File)
        {
            entry.name = pItr->VGetName();
            entry.isFile = true;
            entry.lastWriteTime = pItr->VGetLastWriteTime();
        }
        else
        {
            std::string sName = pItr->VGetName();
            entry.isFile = false;
            entry.lastWriteTime = 0;
            auto *pSubItr = pItr->VOpenSubDir();
            if (pSubItr)
            {
                entry = ScanDirectory(pSubItr);
                delete pSubItr;
            }
            entry.name = std::move(sName);
        }
        root.children.push_back(std::move(entry));

        if (pItr->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
        {
            break;
        }
        eType = pItr->VGetType();
    }
    return root;
}

void CResourceLoader::LoadFoldersParallel(std::vector<FolderTask> &tasks, CModuleFile &module, CALLBACK_ARG)
{
    if (tasks.empty())
    {
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Phase 1 (parallel): Pre-scan directory trees into memory
    struct ScanResult
    {
        CFileMap::DirEntry snapshot;
        bool hasIterator = false;
    };

    auto &pool = CThreadPool::Instance();
    std::vector<std::future<ScanResult>> futures;
    futures.reserve(tasks.size());

    for (const auto &task : tasks)
    {
        futures.push_back(pool.Submit(
            [&task, pFSS = module.m_pFSS]() -> ScanResult
            {
                ScanResult result;
                IDirectoryTraverser::IIterator *pDirItr = nullptr;
                try
                {
                    pDirItr = pFSS->VIterate(task.folderPath.c_str());
                }
                catch (const CRainmanException &e)
                {
                    return result;
                }
                if (pDirItr)
                {
                    result.snapshot = ScanDirectory(pDirItr);
                    result.hasIterator = true;
                    delete pDirItr;
                }
                return result;
            }));
    }

    // Phase 2 (serial): Register sources and replay snapshots into CFileMap
    for (size_t i = 0; i < tasks.size(); ++i)
    {
        ScanResult result = futures[i].get();
        const auto &task = tasks[i];

        if (!result.hasIterator)
        {
            // Folder doesn't exist on disk. For the default-write folder (usually "Data"),
            // we still register the source so _MakeFolderWritable can find it when saving
            // files that currently live only in SGA archives. We also create the folder on
            // disk so that VCreateFolderIn can create subdirectories within it.
            if (task.bIsDefaultWrite)
            {
                RAINMAN_LOG_WARN("Default write folder '{}' does not exist on disk — creating and registering for "
                                 "save support",
                                 task.folderPath);
                _mkdir(task.folderPath.c_str());
                result.snapshot.directoryPath = task.folderPath;
                result.snapshot.isFile = false;
                result.snapshot.lastWriteTime = 0;
            }
            else
            {
                continue;
            }
        }

        const char *sSlashChar = task.uiName.empty() ? nullptr : task.uiName.c_str();
        if (!sSlashChar)
        {
            sSlashChar = strrchr(task.folderPath.c_str(), '\\');
            if (!sSlashChar)
            {
                sSlashChar = strrchr(task.folderPath.c_str(), '/');
            }
            if (sSlashChar)
            {
                ++sSlashChar;
            }
            else
            {
                sSlashChar = task.folderPath.c_str();
            }
        }

        CallCallback(THE_CALLBACK, "Registering data folder '%s' for mod '%s'", sSlashChar,
                     module.m_sFileMapName.c_str());

        bool bIsThisMod = true;
        char *sActualModName = nullptr;
        if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
        {
            if (strnicmp(module.m_sCohThisModFolder.c_str(), task.folderPath.c_str(),
                         module.m_sCohThisModFolder.size()) != 0)
            {
                bIsThisMod = false;
                sActualModName = strdup(task.folderPath.c_str() + module.m_sApplicationPath.size());
                char *sSlash = strrchr(sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
            }
        }

        void *pSrc;
        if (bIsThisMod)
        {
            pSrc = module.m_pNewFileMap->RegisterSource(
                module.m_iFileMapModNumber, false, task.iNum, module.GetFileMapName(), sSlashChar, module.m_pFSS,
                module.m_pFSS, module.m_pParentModule ? false : true, task.bIsDefaultWrite);
            if (task.pDataSource)
            {
                task.pDataSource->m_bCanWriteToFolder = module.m_pParentModule ? false : true;
            }
        }
        else
        {
            pSrc = module.m_pNewFileMap->RegisterSource(15000, false, task.iNum, sActualModName, sSlashChar,
                                                        module.m_pFSS, module.m_pFSS, false, false);
        }

        module.m_pNewFileMap->MapSnapshot(pSrc, task.tocName.c_str(), result.snapshot);

        if (sActualModName)
        {
            free(sActualModName);
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    RAINMAN_LOG_INFO("Parallel folder loading: {} folders in {} ms", tasks.size(), elapsedMs);
}

// -- Private helper methods --

bool CResourceLoader::IsToBeLoaded(const CModuleFile &module, CModuleFile::CCohDataSource *pDataSource)
{
    return ((pDataSource->m_sOption == "common") || (pDataSource->m_sOption == "sound_high") ||
            (pDataSource->m_sOption == "art_high") ||
            (stricmp(pDataSource->m_sOption.c_str(), module.m_sLocale.c_str()) == 0));
}

void CResourceLoader::LoadDataGeneric(CModuleFile &module, CALLBACK_ARG)
{
    const std::string sPipelineFile = module.m_sApplicationPath + "pipeline.ini";
    std::unique_ptr<FILE, decltype(&fclose)> fModule(fopen(sPipelineFile.c_str(), "rb"), &fclose);
    if (!fModule && !module.m_sGameInstallPath.empty())
    {
        fModule.reset(fopen((module.m_sGameInstallPath + "pipeline.ini").c_str(), "rb"));
    }
    if (!fModule)
    {
        return;
    }
    CallCallback(THE_CALLBACK, "Parsing file \'pipeline.ini\'");

    const std::string sSection =
        "project:" + (module.m_sGameInstallPath.empty() ? module.m_metadata.m_sModFolder : module.m_sFileMapName);
    std::string sDataGenericValue;
    bool bInSection = false;
    while (!feof(fModule.get()))
    {
        std::unique_ptr<char[]> sLine(Util_fgetline(fModule.get()));
        if (!sLine)
        {
            throw CRainmanException(__FILE__, __LINE__, "Error reading from file");
        }
        char *sCommentBegin = strchr(sLine.get(), ';');
        if (sCommentBegin)
        {
            *sCommentBegin = 0;
        }
        char *sEqualsChar = strchr(sLine.get(), '=');
        if (sEqualsChar)
        {
            char *sKey = sLine.get();
            char *sValue = sEqualsChar + 1;
            *sEqualsChar = 0;
            Util_TrimWhitespace(&sKey);
            Util_TrimWhitespace(&sValue);
            if (bInSection && stricmp(sKey, "DataGeneric") == 0)
            {
                sDataGenericValue = sValue;
            }
        }
        else
        {
            char *sLeftBraceChar = strchr(sLine.get(), '[');
            char *sRightBraceChar = sLeftBraceChar ? strchr(sLeftBraceChar, ']') : nullptr;
            if (sLeftBraceChar && sRightBraceChar)
            {
                *sRightBraceChar = 0;
                ++sLeftBraceChar;
                bInSection = stricmp(sLeftBraceChar, sSection.c_str()) == 0;
            }
        }
    }

    if (!sDataGenericValue.empty())
    {
        std::string sDataGenericFullPath;
        const bool bAppPath =
            strnicmp(sDataGenericValue.c_str(), "%app%", 5) == 0 &&
            (sDataGenericValue.size() == 5 || sDataGenericValue[5] == '\\' || sDataGenericValue[5] == '/');
        if (bAppPath)
        {
            const auto iRelative = sDataGenericValue.find_first_not_of("\\/", 5);
            sDataGenericFullPath =
                module.m_sGameInstallPath.empty() ? module.m_sApplicationPath : module.m_sGameInstallPath;
            if (iRelative != std::string::npos)
            {
                sDataGenericFullPath += sDataGenericValue.substr(iRelative);
            }
        }
        else
        {
            sDataGenericFullPath = module.m_sApplicationPath + sDataGenericValue;
        }
        const bool bDefaultWrite =
            module.m_pParentModule == nullptr && !(bAppPath && !module.m_sGameInstallPath.empty());
        CResourceLoader::DoLoadFolder(module, sDataGenericFullPath.c_str(), bDefaultWrite, 99, "Generic", "DataGeneric",
                                      THE_CALLBACK);
    }
}

void CResourceLoader::LoadCohEngine(CModuleFile &module, const char *sFolder, const char *sUiName,
                                    unsigned long iLoadWhat, unsigned short iNum, CALLBACK_ARG)
{
    auto pEngine = std::make_unique<CModuleFile>();

    pEngine->m_eModuleType = CModuleFile::MT_CompanyOfHeroesEarly;
    pEngine->m_metadata.m_sUiName = sUiName;
    pEngine->m_metadata.m_sDescription = sUiName;
    pEngine->m_sFileMapName = sUiName;
    pEngine->m_metadata.m_sModFolder = sFolder;

    pEngine->m_sLocale = module.m_sLocale;
    pEngine->m_pParentModule = &module;
    pEngine->m_pNewFileMap = module.m_pNewFileMap;
    pEngine->m_pFSS = module.m_pFSS;
    pEngine->m_sApplicationPath = module.m_sApplicationPath;
    pEngine->m_iFileMapModNumber = iNum;

    auto *pRawEngine = pEngine.get();
    module.m_vEngines.push_back(std::move(pEngine));

    pRawEngine->ReloadResources(iLoadWhat & (~CModuleFile::RR_Engines), 0, 0, THE_CALLBACK);
}

// -- Public entry point --

void CResourceLoader::Load(CModuleFile &module, unsigned long iReloadWhat, unsigned long iReloadWhatRequiredMods,
                           unsigned long iReloadWhatEngines, CALLBACK_ARG)
{
    auto tLoadStart = std::chrono::steady_clock::now();
    module._CleanResources();

    if (module.m_pFSS == nullptr)
    {
        module.m_pFSS = new CFileSystemStore;
    }
    if (module.m_pNewFileMap == nullptr)
    {
        module.m_pNewFileMap = new CFileMap;
    }

    // === Cross-phase parallelism: UCS, Folders, Archives ===
    // These three phases are independent:
    //   UCS writes to m_vLocaleTexts (no CFileMap interaction)
    //   Folders and Archives write to m_pNewFileMap (mutex-protected)
    // Run them concurrently when 2+ are active for maximum I/O overlap.

    bool bRunUcs = (iReloadWhat & CModuleFile::RR_LocalisedText) != 0;
    bool bRunFolders = (iReloadWhat & CModuleFile::RR_DataFolders) != 0;
    bool bRunArchives = (iReloadWhat & CModuleFile::RR_DataArchives) != 0;

    // Pre-compute CoH data source load flags to avoid write races between phases
    if ((bRunFolders || bRunArchives) && module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
    {
        for (auto &pDS : module.m_vDataSources)
        {
            pDS->m_bIsLoaded = IsToBeLoaded(module, pDS.get());
            pDS->m_bCanWriteToFolder = false;
        }
    }

    // --- UCS phase lambda ---
    auto ucsPhase = [&]() -> std::optional<CRainmanException>
    {
        if (!bRunUcs)
        {
            return std::nullopt;
        }
        auto tPhaseStart = std::chrono::steady_clock::now();

        std::string sUcsPath;
        if (!module.m_metadata.m_sLocFolder.empty())
        {
            sUcsPath =
                std::string(module.m_sApplicationPath) + module.m_metadata.m_sLocFolder + "\\" + module.m_sLocale;
        }
        else
        {
            sUcsPath = std::string(module.m_sApplicationPath) + module.m_metadata.m_sModFolder + "\\Locale\\" +
                       module.m_sLocale;
        }

        IDirectoryTraverser::IIterator *pItr = nullptr;
        try
        {
            pItr = module.m_pFSS->VIterate(sUcsPath.c_str());
        }
        IGNORE_EXCEPTIONS

        if (pItr != nullptr)
        {
            try
            {
                CallCallback(THE_CALLBACK, "Loading UCS files for mod \'%s\'", module.m_sFileMapName.c_str());
                LoadUcsFilesParallel(module, pItr);
            }
            catch (const CRainmanException &e)
            {
                delete pItr;
                return CRainmanException(e, __FILE__, __LINE__, "Error loading UCS from \'%s\'", sUcsPath.c_str());
            }
            delete pItr;
        }

        auto tPhaseEnd = std::chrono::steady_clock::now();
        RAINMAN_LOG_INFO("Load phase: UCS files = {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
        return std::nullopt;
    };

    // --- Folders phase lambda ---
    auto foldersPhase = [&]() -> std::optional<CRainmanException>
    {
        if (!bRunFolders)
        {
            return std::nullopt;
        }
        auto tPhaseStart = std::chrono::steady_clock::now();

        try
        {
            if (module.m_eModuleType == CModuleFile::MT_DawnOfWar)
            {
                size_t iDefaultWriteIdx = SIZE_MAX;
                for (size_t i = 0; i < module.m_vFolders.size(); ++i)
                {
                    if (stricmp(module.m_vFolders[i]->m_sName.c_str(), "data") == 0)
                    {
                        iDefaultWriteIdx = i;
                        break;
                    }
                }
                if (iDefaultWriteIdx == SIZE_MAX)
                {
                    for (size_t i = 0; i < module.m_vFolders.size(); ++i)
                    {
                        if (module.m_vFolders[i]->m_sName.find('%') == std::string::npos)
                        {
                            iDefaultWriteIdx = i;
                            break;
                        }
                    }
                }

                std::vector<FolderTask> folderTasks;
                for (size_t i = 0; i < module.m_vFolders.size(); ++i)
                {
                    auto &pFolder = module.m_vFolders[i];
                    std::string sWithoutDynamics = module._DawnOfWarRemoveDynamics(pFolder->m_sName.c_str());
                    FolderTask task;
                    task.folderPath =
                        module.m_sApplicationPath + module.m_metadata.m_sModFolder + "\\" + sWithoutDynamics;
                    task.uiName = sWithoutDynamics;
                    task.tocName = "Data";
                    task.iNum = static_cast<unsigned short>(15000 + pFolder->m_iNumber);
                    task.bIsDefaultWrite = (i == iDefaultWriteIdx);
                    folderTasks.push_back(std::move(task));
                }
                LoadFoldersParallel(folderTasks, module, THE_CALLBACK);
            }
            else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroesEarly)
            {
                std::string sBasePath = module.m_sApplicationPath + module.m_metadata.m_sModFolder;
                std::vector<FolderTask> folderTasks;
                folderTasks.push_back({sBasePath + "\\Data", "", "Data", 15000, true});
                folderTasks.push_back({sBasePath + "\\Movies", "", "Movies", 15000, true});
                LoadFoldersParallel(folderTasks, module, THE_CALLBACK);
            }
            else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
            {
                std::vector<FolderTask> folderTasks;
                for (auto &pDS : module.m_vDataSources)
                {
                    // m_bIsLoaded already pre-computed above
                    if (pDS->m_bIsLoaded && !pDS->m_sFolder.empty())
                    {
                        FolderTask task;
                        task.folderPath = module.m_sApplicationPath + pDS->m_sFolder;
                        task.uiName = pDS->m_sFolder;
                        task.tocName = pDS->m_sToc;
                        task.iNum = static_cast<unsigned short>(15000 + pDS->m_iNumber);
                        task.bIsDefaultWrite = ((pDS->m_sOption == "common") && (pDS->m_iNumber < 2));
                        task.pDataSource = pDS.get();
                        folderTasks.push_back(std::move(task));
                    }
                }
                LoadFoldersParallel(folderTasks, module, THE_CALLBACK);
            }
        }
        catch (const CRainmanException &e)
        {
            auto tPhaseEnd = std::chrono::steady_clock::now();
            RAINMAN_LOG_INFO("Load phase: Data folders = {} ms (FAILED)",
                             std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
            return e;
        }

        auto tPhaseEnd = std::chrono::steady_clock::now();
        RAINMAN_LOG_INFO("Load phase: Data folders = {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
        return std::nullopt;
    };

    // --- Archives phase lambda ---
    auto archivesPhase = [&]() -> std::optional<CRainmanException>
    {
        if (!bRunArchives)
        {
            return std::nullopt;
        }
        auto tPhaseStart = std::chrono::steady_clock::now();

        try
        {
            if (module.m_eModuleType == CModuleFile::MT_DawnOfWar)
            {
                std::vector<ArchiveTask> tasks;
                for (auto &pArch : module.m_vArchives)
                {
                    std::string sWithoutDynamics = module._DawnOfWarRemoveDynamics(pArch->m_sName.c_str());
                    ArchiveTask task;
                    task.archivePath =
                        module.m_sApplicationPath + module.m_metadata.m_sModFolder + "\\" + sWithoutDynamics;
                    task.uiName = pArch->m_sName;
                    task.iNum = static_cast<unsigned short>(15000 + pArch->m_iNumber);
                    task.ppSgaOut = &pArch->m_pHandle;
                    tasks.push_back(std::move(task));
                }
                CallCallback(THE_CALLBACK, "Loading data archives for mod \'%s\'", module.m_sFileMapName.c_str());
                LoadArchivesParallel(tasks, module, THE_CALLBACK);
            }
            else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroesEarly)
            {
                std::string sArchivesPath = module.m_sApplicationPath + module.m_metadata.m_sModFolder + "\\Archives";
                IDirectoryTraverser::IIterator *pItr = nullptr;
                try
                {
                    pItr = module.m_pFSS->VIterate(sArchivesPath.c_str());
                }
                catch (const CRainmanException &e)
                {
                    delete pItr;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error loading archives from \'%s\'",
                                            sArchivesPath.c_str());
                }

                // Collect archive entries from the directory listing
                std::vector<ArchiveTask> tasks;
                if (pItr && pItr->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
                {
                    do
                    {
                        if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
                        {
                            auto pArchEntry = std::make_unique<CModuleFile::CArchiveHandler>();
                            pArchEntry->m_iNumber = -7291;
                            pArchEntry->m_sName = pItr->VGetName();

                            ArchiveTask task;
                            task.archivePath = pItr->VGetFullPath();
                            task.uiName = pArchEntry->m_sName;
                            task.iNum = 15000;
                            task.ppSgaOut = &pArchEntry->m_pHandle;
                            tasks.push_back(std::move(task));
                            module.m_vArchives.push_back(std::move(pArchEntry));
                        }
                    } while (pItr->VNextItem() == IDirectoryTraverser::IIterator::E_OK);
                }
                delete pItr;

                if (!tasks.empty())
                {
                    CallCallback(THE_CALLBACK, "Loading data archives for mod \'%s\'", module.m_sFileMapName.c_str());
                    LoadArchivesParallel(tasks, module, THE_CALLBACK);
                }
            }
            else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
            {
                std::vector<ArchiveTask> tasks;
                for (auto &pDS : module.m_vDataSources)
                {
                    // m_bIsLoaded already pre-computed above
                    if (pDS->m_bIsLoaded)
                    {
                        for (auto &pArch : pDS->m_vArchives)
                        {
                            ArchiveTask task;
                            task.archivePath = module.m_sApplicationPath + pArch->m_sName;
                            task.uiName = pArch->m_sName;
                            task.iNum = static_cast<unsigned short>(15000 + (500 * pDS->m_iNumber) + pArch->m_iNumber);
                            task.ppSgaOut = &pArch->m_pHandle;
                            tasks.push_back(std::move(task));
                        }
                    }
                }
                CallCallback(THE_CALLBACK, "Loading data archives for mod \'%s\'", module.m_sFileMapName.c_str());
                LoadArchivesParallel(tasks, module, THE_CALLBACK);
            }
        }
        catch (const CRainmanException &e)
        {
            auto tPhaseEnd = std::chrono::steady_clock::now();
            RAINMAN_LOG_INFO("Load phase: Data archives = {} ms (FAILED)",
                             std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
            return e;
        }

        auto tPhaseEnd = std::chrono::steady_clock::now();
        RAINMAN_LOG_INFO("Load phase: Data archives = {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
        return std::nullopt;
    };

    // Execute phases — run concurrently if 2+ are active
    {
        int parallelCount = (bRunUcs ? 1 : 0) + (bRunFolders ? 1 : 0) + (bRunArchives ? 1 : 0);
        std::optional<CRainmanException> pUcsError;
        std::optional<CRainmanException> pFoldersError;
        std::optional<CRainmanException> pArchivesError;

        if (parallelCount >= 2)
        {
            RAINMAN_LOG_INFO("Cross-phase parallelism: launching {} phases concurrently", parallelCount);
            auto tParStart = std::chrono::steady_clock::now();

            auto &crossPool = CThreadPool::Instance();
            std::future<std::optional<CRainmanException>> ucsFut, foldersFut, archivesFut;
            if (bRunUcs)
            {
                ucsFut = crossPool.Submit(ucsPhase);
            }
            if (bRunFolders)
            {
                foldersFut = crossPool.Submit(foldersPhase);
            }
            if (bRunArchives)
            {
                archivesFut = crossPool.Submit(archivesPhase);
            }

            if (bRunUcs)
            {
                pUcsError = ucsFut.get();
            }
            if (bRunFolders)
            {
                pFoldersError = foldersFut.get();
            }
            if (bRunArchives)
            {
                pArchivesError = archivesFut.get();
            }

            auto tParEnd = std::chrono::steady_clock::now();
            RAINMAN_LOG_INFO("Cross-phase parallel: all {} phases completed in {} ms", parallelCount,
                             std::chrono::duration_cast<std::chrono::milliseconds>(tParEnd - tParStart).count());
        }
        else
        {
            pUcsError = ucsPhase();
            pFoldersError = foldersPhase();
            pArchivesError = archivesPhase();
        }

        // Propagate first error
        std::optional<CRainmanException> *pErrors[] = {&pUcsError, &pFoldersError, &pArchivesError};
        for (auto *pErr : pErrors)
        {
            if (*pErr)
            {
                CRainmanException toThrow = std::move(**pErr);
                throw toThrow;
            }
        }
    }
    if (iReloadWhat & CModuleFile::RR_MapArchives)
    {
        if (!module.m_sScenarioPackRootFolder.empty() && !module.m_metadata.m_sScenarioPackFolder.empty())
        {
            auto *sArchivesPath = new wchar_t[module.m_sScenarioPackRootFolder.size() +
                                              module.m_metadata.m_sScenarioPackFolder.size() + 2];
            swprintf(sArchivesPath, L"%s\\%S", module.m_sScenarioPackRootFolder.c_str(),
                     module.m_metadata.m_sScenarioPackFolder.c_str());
            IDirectoryTraverser::IIterator *pItr = nullptr;
            try
            {
                pItr = module.m_pFSS->IterateW(sArchivesPath);
            }
            catch (const CRainmanException &e)
            {
                pItr = nullptr;
            }
            if (pItr)
            {
                try
                {
                    CallCallback(THE_CALLBACK, "Loading map archives for mod \'%s\'", module.m_sFileMapName.c_str());
                    Util_ForEach(pItr, CModuleFile_ArchiveForEachNoErrors, static_cast<void *>(&module), false);
                }
                catch (const CRainmanException &e)
                {
                    PAUSE_THROW(e, __FILE__, __LINE__, "Error loading map archives from \'%S\'", sArchivesPath);
                    delete pItr;
                    delete[] sArchivesPath;
                    UNPAUSE_THROW;
                }
                delete pItr;
            }
            delete[] sArchivesPath;
        }
    }
    if (iReloadWhat & CModuleFile::RR_RequiredMods)
    {
        auto tPhaseStart = std::chrono::steady_clock::now();
        if (module.m_eModuleType == CModuleFile::MT_DawnOfWar)
        {
            for (auto &pReqHandler : module.m_vRequireds)
            {
                auto pReq = std::make_unique<CModuleFile>();
                pReq->m_sLocale = module.m_sLocale;

                std::string sFilePath = module.m_sApplicationPath + pReqHandler->m_sName + ".module";
                if (!module.m_sGameInstallPath.empty())
                {
                    std::error_code ec;
                    const bool bLocalModule = std::filesystem::is_regular_file(sFilePath, ec);
                    if (ec && ec != std::errc::no_such_file_or_directory)
                    {
                        throw CRainmanException(nullptr, __FILE__, __LINE__, "Cannot inspect required module '%s': %s",
                                                sFilePath.c_str(), ec.message().c_str());
                    }
                    if (!bLocalModule)
                    {
                        sFilePath = module.m_sGameInstallPath + pReqHandler->m_sName + ".module";
                    }
                }
                pReq->LoadModuleFile(sFilePath.c_str());

                pReq->m_pParentModule = &module;
                pReq->m_pNewFileMap = module.m_pNewFileMap;
                pReq->m_pFSS = module.m_pFSS;
                pReq->m_sGameInstallPath = module.m_sGameInstallPath;
                pReq->m_iFileMapModNumber = static_cast<unsigned short>(15000 + pReqHandler->m_iNumber);

                pReqHandler->m_pHandle = pReq.release();

                pReqHandler->m_pHandle->ReloadResources(iReloadWhatRequiredMods & (~CModuleFile::RR_Engines) &
                                                            (~CModuleFile::RR_RequiredMods),
                                                        0, 0, THE_CALLBACK);
            }
        }
        auto tPhaseEnd = std::chrono::steady_clock::now();
        RAINMAN_LOG_INFO("Load phase: Required mods = {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
    }
    if (iReloadWhat & CModuleFile::RR_Engines)
    {
        auto tPhaseStart = std::chrono::steady_clock::now();
        if (module.m_eModuleType == CModuleFile::MT_DawnOfWar)
        {
            auto pEngineOwner = std::make_unique<CModuleFile>();
            auto *pEngine = pEngineOwner.get();

            pEngine->m_eModuleType = CModuleFile::MT_DawnOfWar;
            pEngine->m_metadata.m_sUiName = "(Engine)";
            pEngine->m_sFileMapName = "(Engine)";
            pEngine->m_metadata.m_sDescription = "(Engine)";
            pEngine->m_metadata.m_sModFolder = "Engine";

            pEngine->m_sLocale = module.m_sLocale;
            pEngine->m_pParentModule = &module;
            pEngine->m_pNewFileMap = module.m_pNewFileMap;
            pEngine->m_pFSS = module.m_pFSS;
            pEngine->m_sApplicationPath =
                module.m_sGameInstallPath.empty() ? module.m_sApplicationPath : module.m_sGameInstallPath;
            pEngine->m_sGameInstallPath = module.m_sGameInstallPath;
            pEngine->m_iFileMapModNumber = static_cast<unsigned short>(30001);

            {
                auto pObject = std::make_unique<CModuleFile::CFolderHandler>();
                pObject->m_sName = "Data";
                pObject->m_iNumber = 1;
                pEngine->m_vFolders.push_back(std::move(pObject));
            }
            {
                auto pObject = std::make_unique<CModuleFile::CArchiveHandler>();
                pObject->m_sName = "Engine.sga";
                pObject->m_iNumber = 1;
                pEngine->m_vArchives.push_back(std::move(pObject));
            }

            module.m_vEngines.push_back(std::move(pEngineOwner));

            pEngine->ReloadResources(iReloadWhatEngines & (~CModuleFile::RR_Engines), 0, 0, THE_CALLBACK);
        }
        else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroesEarly)
        {
            LoadCohEngine(module, "Engine", "(Engine)", iReloadWhatEngines, 30001, THE_CALLBACK);
            LoadCohEngine(module, "RelicOnline", "(Relic Online)", iReloadWhatEngines, 30000, THE_CALLBACK);
        }
        // NOLINTNEXTLINE(bugprone-branch-clone) distinct module type, same engine loading
        else if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
        {
            LoadCohEngine(module, "Engine", "(Engine)", iReloadWhatEngines, 30001, THE_CALLBACK);
            LoadCohEngine(module, "RelicOnline", "(Relic Online)", iReloadWhatEngines, 30000, THE_CALLBACK);
        }
        auto tPhaseEnd = std::chrono::steady_clock::now();
        RAINMAN_LOG_INFO("Load phase: Engines = {} ms",
                         std::chrono::duration_cast<std::chrono::milliseconds>(tPhaseEnd - tPhaseStart).count());
    }
    if (iReloadWhat & CModuleFile::RR_DataGeneric)
    {
        LoadDataGeneric(module, THE_CALLBACK);
    }
    auto tLoadEnd = std::chrono::steady_clock::now();
    RAINMAN_LOG_INFO("Load TOTAL for '{}' = {} ms",
                     module.m_sFileMapName.empty() ? "(unknown)" : module.m_sFileMapName.c_str(),
                     std::chrono::duration_cast<std::chrono::milliseconds>(tLoadEnd - tLoadStart).count());
}

void CResourceLoader::DoLoadFolder(CModuleFile &module, const char *sFullPath, bool bIsDefaultWrite,
                                   unsigned short iNum, const char *sTOC, const char *sUiName, CALLBACK_ARG,
                                   bool *bIsWritable)
{
    if (bIsWritable != nullptr)
    {
        *bIsWritable = false;
    }
    const char *sSlashChar;
    if (sUiName)
    {
        sSlashChar = sUiName;
    }
    else
    {
        sSlashChar = strrchr(sFullPath, '\\');
        if (!sSlashChar)
        {
            sSlashChar = strrchr(sFullPath, '/');
        }
        ++sSlashChar;
    }

    bool bIsThisMod = true;
    char *sActualModName = nullptr;
    if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
    {
        if (strnicmp(module.m_sCohThisModFolder.c_str(), sFullPath, module.m_sCohThisModFolder.size()) != 0)
        {
            bIsThisMod = false;
            sActualModName = strdup(sFullPath + module.m_sApplicationPath.size());
            char *sSlash = strrchr(sActualModName, '\\');
            if (sSlash)
            {
                *sSlash = 0;
            }
        }
    }

    IDirectoryTraverser::IIterator *pDirItr = nullptr;
    try
    {
        CallCallback(THE_CALLBACK, "Loading data folder \'%s\' for mod \'%s\'", sSlashChar,
                     module.m_sFileMapName.c_str());
        pDirItr = module.m_pFSS->VIterate(sFullPath);
        if (pDirItr)
        {
            void *pSrc;
            if (bIsThisMod)
            {
                pSrc = module.m_pNewFileMap->RegisterSource(
                    module.m_iFileMapModNumber, false, iNum, module.GetFileMapName(), sSlashChar, module.m_pFSS,
                    module.m_pFSS, module.m_pParentModule ? false : true, bIsDefaultWrite);
                if (bIsWritable != nullptr)
                {
                    *bIsWritable = module.m_pParentModule ? false : true;
                }
            }
            else
            {
                pSrc = module.m_pNewFileMap->RegisterSource(15000, false, iNum, sActualModName, sSlashChar,
                                                            module.m_pFSS, module.m_pFSS, false, false);
            }

            module.m_pNewFileMap->MapIterator(pSrc, sTOC, pDirItr);
        }
    }
    catch (const CRainmanException &)
    {
        // Folder doesn't exist on disk. For the default-write folder, create the folder
        // and register an empty source so _MakeFolderWritable can route saves to it.
        if (bIsDefaultWrite && bIsThisMod)
        {
            RAINMAN_LOG_WARN("Default write folder '{}' does not exist — creating and registering for save support",
                             sFullPath);
            _mkdir(sFullPath);
            CFileMap::DirEntry emptySnapshot;
            emptySnapshot.directoryPath = sFullPath;
            emptySnapshot.isFile = false;
            emptySnapshot.lastWriteTime = 0;
            void *pSrc = module.m_pNewFileMap->RegisterSource(
                module.m_iFileMapModNumber, false, iNum, module.GetFileMapName(), sSlashChar, module.m_pFSS,
                module.m_pFSS, module.m_pParentModule ? false : true, bIsDefaultWrite);
            if (bIsWritable != nullptr)
            {
                *bIsWritable = module.m_pParentModule ? false : true;
            }
            module.m_pNewFileMap->MapSnapshot(pSrc, sTOC, emptySnapshot);
        }
    }
    if (pDirItr)
    {
        delete pDirItr;
    }
    if (sActualModName)
    {
        free(sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator) -- strdup uses malloc; free() is
                              // correct
    }
}

void CResourceLoader::DoLoadArchive(CModuleFile &module, const char *sFullPath, CSgaFile **ppSga, unsigned short iNum,
                                    const char *sUiName, CALLBACK_ARG)
{
    const char *sSlashChar;
    if (sUiName)
    {
        sSlashChar = sUiName;
    }
    else
    {
        sSlashChar = strrchr(sFullPath, '\\');
        if (!sSlashChar)
        {
            sSlashChar = strrchr(sFullPath, '/');
        }
        ++sSlashChar;
    }

    CSgaFile *pSga = CHECK_MEM(new CSgaFile);
    std::unique_ptr<IFileStore::IStream> inputStream;
    IDirectoryTraverser::IIterator *pDirItr = nullptr;
    try
    {
        inputStream = std::unique_ptr<IFileStore::IStream>(module.m_pFSS->VOpenStream(sFullPath));
    }
    catch (const CRainmanException &e)
    {
        delete pSga;
        pSga = nullptr;
        return;
    }

    bool bIsThisMod = true;
    char *sActualModName = nullptr;
    if (module.m_eModuleType == CModuleFile::MT_CompanyOfHeroes)
    {
        if (strnicmp(module.m_sCohThisModFolder.c_str(), sFullPath, module.m_sCohThisModFolder.size()) != 0)
        {
            if (!module.m_saScenarioPackRootFolder.empty() &&
                strnicmp(module.m_saScenarioPackRootFolder.c_str(), sFullPath,
                         module.m_saScenarioPackRootFolder.size()) == 0)
            {
                bIsThisMod = false;
                sActualModName = strdup(sFullPath + module.m_saScenarioPackRootFolder.size() + 1);
                char *sSlash = strrchr(sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
            }
            else
            {
                bIsThisMod = false;
                sActualModName = strdup(sFullPath + module.m_sApplicationPath.size());
                char *sSlash = strrchr(sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
                sSlash = strrchr(sActualModName, '\\');
                if (sSlash)
                {
                    *sSlash = 0;
                }
            }
        }
    }

    try
    {
        CallCallback(THE_CALLBACK, "Loading data archive \'%s\' for mod \'%s\'", sSlashChar,
                     module.m_sFileMapName.c_str());
        pSga->Load(inputStream.get(), module.m_pFSS->VGetLastWriteTime(sFullPath));
        pSga->VInit(inputStream.get());
        pDirItr = pSga->VIterate(pSga->VGetEntryPoint(0));
        if (pDirItr)
        {
            void *pSrc;
            if (bIsThisMod)
            {
                pSrc =
                    module.m_pNewFileMap->RegisterSource(module.m_iFileMapModNumber, true, iNum,
                                                         module.GetFileMapName(), sSlashChar, pSga, pSga, false, false);
            }
            else
            {
                pSrc = module.m_pNewFileMap->RegisterSource(15000, true, iNum, sActualModName, sSlashChar, pSga, pSga,
                                                            false, false);
            }
            module.m_pNewFileMap->MapSga(pSrc, pSga);
        }
        delete pDirItr;
        (void)inputStream.release();
    }
    catch (const CRainmanException &e)
    {
        delete pDirItr;
        delete pSga;
        if (sActualModName)
        {
            free(sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator) -- strdup uses malloc
        }
        throw CRainmanException(e, __FILE__, __LINE__, "(rethrow for \'%s\')", sFullPath);
    }
    *ppSga = pSga;
    if (sActualModName)
    {
        free(sActualModName); // NOLINT(clang-analyzer-unix.MismatchedDeallocator) -- strdup uses malloc
    }
}
