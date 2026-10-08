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

#include "rainman/module/CFileMap.h"
#include "rainman/archive/CSgaFile.h"
#include "rainman/core/Exception.h"
#include "rainman/core/RainmanLog.h"
#include <memory>
#include <algorithm>
#include <string>
#include "rainman/core/memdebug.h"

CFileMap::CFileMap()
{
    m_fAuxOutputSupply = nullptr;
    m_pAuxOutputContext = nullptr;
}

void CFileMap::SetAuxOutputSupply(tAuxOutputSupply fAuxOutputSupply, void *pContext)
{
    m_fAuxOutputSupply = fAuxOutputSupply;
    m_pAuxOutputContext = pContext;
}

CFileMap::~CFileMap() { _Clean(); }

void CFileMap::VInit(void *pUnused) {}

IFileStore::IStream *CFileMap::_OpenFile(CFileMap::_File *pFile, CFileMap::_Folder *pFolder, const char *sFile)
{
    if (pFile == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not find \'%s\'", sFile);
    }
    if (pFolder == nullptr)
    {
        pFolder = pFile->pParent;
    }
    if (pFolder == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Found \'%s\', but did not find folder", sFile);
    }

    if (pFile->mapSources.empty())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Found \'%s\', but it is not mapped to a source", sFile);
    }
    _DataSource *pSrc = pFile->mapSources.begin()->first;
    const auto &sSourceFolder = pFolder->mapSourceNames[pSrc];
    if (sSourceFolder.empty())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Found \'%s\', but folder is not mapped to the source",
                                sFile);
    }

    std::string sFullPath = sSourceFolder;
    if (!sFullPath.empty() && sFullPath.back() != '\\')
    {
        sFullPath += '\\';
    }
    sFullPath += pFile->sName;

    try
    {
        return pSrc->pStore->VOpenStream(sFullPath.c_str());
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error opening \'%s\' from file source", sFile);
    }
}

void CFileMap::VCreateFolderIn(const char *sPath, const char *sNewFolderName)
{
    throw CRainmanException(__FILE__, __LINE__, "Not coded yet ^.^");
}

tLastWriteTime CFileMap::VGetLastWriteTime(const char *sFile)
{
    _File *pFile;
    try
    {
        pFile = _FindFile(sFile, nullptr);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Could not find \'%s\'", sFile);
    }
    if (!pFile)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not find \'%s\'", sFile);
    }
    if (pFile->mapSources.empty())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Found \'%s\', but it is not mapped to anything", sFile);
    }

    return pFile->mapSources.begin()->second;
}

CFileMap::_DataSource *CFileMap::_MakeFolderWritable(CFileMap::_Folder *pFolder, CFileMap::_File *pFile)
{
    RAINMAN_LOG_DEBUG("_MakeFolderWritable: folder='{}', file='{}'", pFolder->sFullName, pFile->sName);

    // Work out the source sort number
    unsigned long iSortNumExisting = 0x0000ffff; // DS sort num. must be less than or equal to this
    if (!pFile->mapSources.empty())
    {
        _DataSource *pDS = pFile->mapSources.begin()->first;
        if (pDS->iSortNumber < iSortNumExisting)
        {
            iSortNumExisting = pDS->iSortNumber;
        }
    }

    // Check if the file already uses a suitable source
    if (pFile->pParent == pFolder && !pFile->mapSources.empty())
    {
        _DataSource *pDS = pFile->mapSources.begin()->first;
        if (pDS->iSortNumber <= iSortNumExisting && (pDS->bIsWritable || pDS->bIsDefaultOutput))
        {
            return pDS;
        }
    }

    // Check the folder for siutable sources
    for (auto itr = pFolder->mapSourceNames.begin(); itr != pFolder->mapSourceNames.end(); ++itr)
    {
        _DataSource *pDS = itr->first;
        if (pDS->iSortNumber <= iSortNumExisting && pDS->bIsDefaultOutput)
        {
            return pDS;
        }
    }

    // Look in parent for a suitable source
    if (pFolder->pParent)
    {
        _DataSource *pDS = nullptr;
        try
        {
            pDS = _MakeFolderWritable(pFolder->pParent, pFile);
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__, "Unable to make \'%s\' writable for \'%s\'",
                                    pFolder->sFullName.c_str(), pFile->sName.c_str());
        }

        const auto &sSourceName = pFolder->pParent->mapSourceNames[pDS];

        try
        {
            pDS->pTraverser->VCreateFolderIn(sSourceName.c_str(), pFolder->sName.c_str());
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__,
                                    "Cannot create folder in \'%s\' to make \'%s\' writable for \'%s\'",
                                    sSourceName.c_str(), pFolder->sFullName.c_str(), pFile->sName.c_str());
        }

        std::string sNewFolderName;
        if (!sSourceName.empty() && (sSourceName.back() == '/' || sSourceName.back() == '\\'))
        {
            sNewFolderName = sSourceName + pFolder->sName + sSourceName.back();
        }
        else
        {
            sNewFolderName = sSourceName + "\\" + pFolder->sName;
        }

        pFolder->mapSourceNames[pDS] = std::move(sNewFolderName);

        return pDS;
    }

    throw CRainmanException(nullptr, __FILE__, __LINE__, "Unable to make \'%s\' writable for \'%s\'",
                            pFolder->sFullName.c_str(), pFile->sName.c_str());
}

IFileStore::IOutputStream *CFileMap::VOpenOutputStream(const char *sFile, bool bEraseIfPresent)
{
    bool bFileWasCreated = false;
    _File *pFile;
    _Folder *pFolder = nullptr;
    std::unique_ptr<_File> pNewFile;

    try
    {
        pFile = _FindFile(sFile, &pFolder);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Could not find \'%s\'", sFile);
    }

    if (pFolder == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "No folder found for \'%s\'", sFile);
    }

    if (pFile == nullptr)
    {
        pNewFile = std::make_unique<_File>();
        pFile = pNewFile.get();
        bFileWasCreated = true;

        pFile->pParent = pFolder;
        pFile->sName = std::string(sFile + pFolder->sFullName.size() + 1);
    }

    _DataSource *pDS = nullptr;

    try
    {
        pDS = _MakeFolderWritable(pFolder, pFile);
    }
    catch (const CRainmanException &e)
    {
        pNewFile.reset();
        if (m_fAuxOutputSupply)
        {
            try
            {
                IFileStore::IOutputStream *pOutStr = m_fAuxOutputSupply(sFile, bEraseIfPresent, m_pAuxOutputContext);
                if (pOutStr)
                {
                    return pOutStr;
                }
            }
            IGNORE_EXCEPTIONS
        }
        RAINMAN_LOG_WARN("_MakeFolderWritable failed for '{}': {}", sFile, e.getMessage());
        if (strnicmp(sFile, "data", 4) == 0)
        {
            throw CRainmanException(
                e, __FILE__, __LINE__,
                "Could not find suitable output location for \'%s\' (could be because the Data folder is missing)",
                sFile);
        }
        else
        {
            throw CRainmanException(
                e, __FILE__, __LINE__,
                "Could not find suitable output location for \'%s\' (could be because you\'re trying to mod RelicCOH)",
                sFile);
        }
    }

    if (!bEraseIfPresent)
    {
        if (!pFile->mapSources.empty() && pDS != pFile->mapSources.begin()->first)
        {
            throw CRainmanException(nullptr, __FILE__, __LINE__,
                                    "The output location for \'%s\' is incompatible with \'bEraseIfPresent = false\'",
                                    sFile);
        }
    }

    const auto &sFolderPath = pFolder->mapSourceNames[pDS];
    std::string sFullFilePath = sFolderPath;
    if (!sFolderPath.empty() && sFolderPath.back() != '\\' && sFolderPath.back() != '/')
    {
        sFullFilePath += '\\';
    }
    sFullFilePath += pFile->sName;

    std::unique_ptr<IFileStore::IOutputStream> pOutStr;

    try
    {
        pOutStr = std::unique_ptr<IFileStore::IOutputStream>(
            pDS->pStore->VOpenOutputStream(sFullFilePath.c_str(), bEraseIfPresent));
        if (!pDS->bIsPureOutput)
        {
            pFile->mapSources[pDS] = pDS->pTraverser->VGetLastWriteTime(sFullFilePath.c_str());
        }

        if (bFileWasCreated)
        {
            pFolder->vChildFiles.push_back(std::move(pNewFile));
        }
        bFileWasCreated = false;
        std::sort(pFolder->vChildFiles.begin(), pFolder->vChildFiles.end(), _SortFiles);
    }
    catch (const CRainmanException &e)
    {
        PAUSE_THROW(e, __FILE__, __LINE__, "Error opening \'%s\' from file store (\'%s\')", sFile,
                    sFullFilePath.c_str());

        UNPAUSE_THROW;
    }

    return pOutStr.release();
}

IFileStore::IStream *CFileMap::VOpenStream(const char *sFile)
{
    _File *pFile;
    _Folder *pFolder = nullptr;

    try
    {
        pFile = _FindFile(sFile, &pFolder);
        return _OpenFile(pFile, pFolder, sFile);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Could not find \'%s\'", sFile);
    }
}

unsigned long CFileMap::VGetEntryPointCount() { return static_cast<unsigned long>(m_vTOCs.size()); }

const char *CFileMap::VGetEntryPoint(unsigned long iID)
{
    if (iID >= VGetEntryPointCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "TOC %lu is beyond maximum of %lu", iID,
                                VGetEntryPointCount());
    }
    return m_vTOCs[iID]->sName.c_str();
}

std::vector<std::string> CFileMap::GetSourceFolderPaths(const char *sPath)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    _Folder *pFolder = nullptr;
    _FindFile(sPath, &pFolder, true);

    std::vector<std::string> vPaths;
    for (const auto &[pSource, sSourcePath] : pFolder->mapSourceNames)
    {
        if (pSource->GetSourceType() == 0 && !sSourcePath.empty())
        {
            vPaths.push_back(sSourcePath);
        }
    }
    return vPaths;
}

void CFileMap::CIterator::_MakeFullName()
{
    const char *sPartB = "";
    if (m_eWhat == IW_Folders)
    {
        sPartB = (*m_FoldIter)->sName.c_str();
    }
    else if (m_eWhat == IW_Files)
    {
        sPartB = (*m_FileIter)->sName.c_str();
    }

    if (m_sFullPath)
    {
        delete[] m_sFullPath;
    }
    m_sFullPath = CHECK_MEM(new char[strlen(m_sParentPath) + strlen(sPartB) + 1]);
    sprintf(m_sFullPath, "%s%s", m_sParentPath, sPartB);
}

CFileMap::CIterator::CIterator(CFileMap::_Folder *pFolder, CFileMap *pFileMap)
{
    if (!pFolder)
    {
        throw CRainmanException(__FILE__, __LINE__, "No folder specified");
    }
    if (!pFileMap)
    {
        throw CRainmanException(__FILE__, __LINE__, "No file map specified");
    }
    m_pDirectory = pFolder;
    m_pFileMap = pFileMap;
    m_sFullPath = nullptr;

    m_sParentPath = CHECK_MEM(new char[pFolder->sFullName.size() + 2]);
    sprintf(m_sParentPath, "%s\\", pFolder->sFullName.c_str());

    m_eWhat = IW_Folders;
    m_FoldIter = pFolder->vChildFolders.begin();
    if (m_FoldIter == pFolder->vChildFolders.end())
    {
        m_eWhat = IW_Files;
        m_FileIter = pFolder->vChildFiles.begin();
        if (m_FileIter == pFolder->vChildFiles.end())
        {
            m_eWhat = IW_None;
        }
    }

    _MakeFullName();
}

CFileMap::CIterator::~CIterator()
{
    delete[] m_sParentPath;
    if (m_sFullPath)
    {
        delete[] m_sFullPath;
    }
}

IDirectoryTraverser::IIterator::eTypes CFileMap::CIterator::VGetType()
{
    switch (m_eWhat)
    {
    case IW_Folders:
        return IDirectoryTraverser::IIterator::T_Directory;

    case IW_Files:
        return IDirectoryTraverser::IIterator::T_File;

    default:
        return IDirectoryTraverser::IIterator::T_Nothing;
    }
}

IDirectoryTraverser::IIterator *CFileMap::CIterator::VOpenSubDir()
{
    if (m_eWhat != IW_Folders)
    {
        throw CRainmanException(__FILE__, __LINE__, "Current item is not a folder");
    }
    try
    {
        return new CIterator(m_FoldIter->get(), m_pFileMap);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Unable to iterate sub-folder");
    }
}

IFileStore::IStream *CFileMap::CIterator::VOpenFile()
{
    if (m_eWhat != IW_Files)
    {
        throw CRainmanException(__FILE__, __LINE__, "Current item is not a file");
    }
    try
    {
        return m_pFileMap->_OpenFile(m_FileIter->get(), m_pDirectory, m_sFullPath);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Unable to open file");
    }
}

IDirectoryTraverser::IIterator *CFileMap::VIterate(const char *sPath)
{
    _Folder *pFold = nullptr;
    try
    {
        _FindFile(sPath, &pFold, true);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Could not find folder \'%s\'", sPath);
    }
    if (pFold == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Folder \'%s\' not found (resolved to null)", sPath);
    }

    return new CIterator(pFold, (CFileMap *)this);
}

const char *CFileMap::CIterator::VGetName()
{
    switch (m_eWhat)
    {
    case IW_Folders:
        return (*m_FoldIter)->sName.c_str();

    case IW_Files:
        return (*m_FileIter)->sName.c_str();

    default:
        throw CRainmanException(__FILE__, __LINE__, "Only files and folders can have names");
    }
}

const char *CFileMap::CIterator::VGetFullPath() { return m_sFullPath; }

const char *CFileMap::CIterator::VGetDirectoryPath() { return m_sParentPath; }

IDirectoryTraverser::IIterator::eErrors CFileMap::CIterator::VNextItem()
{
    if (m_eWhat == IW_Files)
    {
        ++m_FileIter;
        if (m_FileIter == m_pDirectory->vChildFiles.end())
        {
            m_eWhat = IW_None;
        }
    }
    else if (m_eWhat == IW_Folders)
    {
        ++m_FoldIter;
        if (m_FoldIter == m_pDirectory->vChildFolders.end())
        {
            m_eWhat = IW_Files;
            m_FileIter = m_pDirectory->vChildFiles.begin();
            if (m_FileIter == m_pDirectory->vChildFiles.end())
            {
                m_eWhat = IW_None;
            }
        }
    }

    _MakeFullName();

    if (m_eWhat == IW_None)
    {
        return E_AtEnd;
    }

    return E_OK;
}

tLastWriteTime CFileMap::CIterator::VGetLastWriteTime()
{
    if (m_eWhat != IW_Files)
    {
        throw CRainmanException(__FILE__, __LINE__, "Current item is not a file");
    }
    if ((**m_FileIter).mapSources.empty())
    {
        throw CRainmanException(__FILE__, __LINE__, "File is not mapped");
    }
    return (**m_FileIter).mapSources.begin()->second;
}

void *CFileMap::CIterator::VGetTag(long iTag)
{
    if (m_eWhat == IW_Files)
    {
        CFileMap::_DataSource *pSrc = nullptr;
        if ((**m_FileIter).mapSources.empty())
        {
            return nullptr;
        }
        pSrc = (**m_FileIter).mapSources.begin()->first;

        if (iTag == 0)
        {
            return (void *)pSrc->sModName.c_str();
        }
        if (iTag == 1)
        {
            return (void *)pSrc->sSourceName.c_str();
        }
        if (iTag == 2)
        {
            return (void *)(uintptr_t)pSrc->GetMod();
        }
        if (iTag == 3)
        {
            return (void *)(uintptr_t)(pSrc->GetSourceType() ? 1 : 0);
        }
    }

    return nullptr;
}

void CFileMap::RewriteToC(const char *sOld, const char *sNew)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    _TOC *pTocKey = nullptr;

    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if (stricmp((**itr).sName.c_str(), sOld) == 0)
        {
            pTocKey = itr->get();
            break;
        }
    }

    if (pTocKey == nullptr)
    {
        auto pNewToc = std::make_unique<_TOC>();
        pNewToc->sName = sOld;
        pNewToc->pRootFolder = std::make_unique<_Folder>();
        pNewToc->pRootFolder->pParent = nullptr;
        pNewToc->pRootFolder->sFullName = sOld;
        pNewToc->pRootFolder->sName = sOld;

        pTocKey = pNewToc.get();
        m_vTOCs.push_back(std::move(pNewToc));
    }

    if (sNew == nullptr)
    {
        m_mapTOCRewrites.erase(pTocKey);
    }
    else
    {
        _TOC *pTocVal = nullptr;

        for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
        {
            if (stricmp((**itr).sName.c_str(), sNew) == 0)
            {
                pTocVal = itr->get();
                break;
            }
        }

        if (pTocVal == nullptr)
        {
            auto pNewToc = std::make_unique<_TOC>();
            pNewToc->sName = sNew;
            pNewToc->pRootFolder = std::make_unique<_Folder>();
            pNewToc->pRootFolder->pParent = nullptr;
            pNewToc->pRootFolder->sFullName = sNew;
            pNewToc->pRootFolder->sName = sNew;

            pTocVal = pNewToc.get();
            m_vTOCs.push_back(std::move(pNewToc));
        }

        m_mapTOCRewrites[pTocKey] = pTocVal;
    }
}

void CFileMap::UnrewriteToC(const char *sOld) { RewriteToC(sOld, nullptr); }

CFileMap::_File *CFileMap::_FindFile(const char *sName, CFileMap::_Folder **ppFolder, bool bIsFolder)
{
    const char *sSlashLoc = strchr(sName, '\\');
    if (sSlashLoc == nullptr)
    {
        sSlashLoc = strchr(sName, '/');
    }
    // if(sSlashLoc == 0) throw CRainmanException(0, __FILE__, __LINE__, "No TOC in path \'%s\'", sName);

    size_t iPartLen = sSlashLoc ? sSlashLoc - sName : strlen(sName);
    _TOC *pToc = nullptr;

    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if (((**itr).sName.size() == iPartLen) && (strnicmp((**itr).sName.c_str(), sName, iPartLen) == 0))
        {
            pToc = itr->get();
            break;
        }
    }
    if (m_mapTOCRewrites[pToc])
    {
        pToc = m_mapTOCRewrites[pToc];
    }

    if (pToc == nullptr)
    {
        // Build list of available TOC names for diagnostic output
        std::string sAvailable;
        for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
        {
            if (!sAvailable.empty())
            {
                sAvailable += ", ";
            }
            sAvailable += (**itr).sName;
        }
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Unknown TOC \'%.*s\' in path \'%s\' (available TOCs: %s)",
                                (int)iPartLen, sName, sName, sAvailable.empty() ? "(none)" : sAvailable.c_str());
    }

    _Folder *pFolder = pToc->pRootFolder.get();
    const char *sNameLeft = nullptr;
    while (sSlashLoc)
    {
        sNameLeft = sSlashLoc + 1;
        sSlashLoc = strchr(sNameLeft, '\\');
        if (sSlashLoc == nullptr)
        {
            sSlashLoc = strchr(sNameLeft, '/');
        }
        if (!bIsFolder && sSlashLoc == nullptr)
        {
            break;
        }
        iPartLen = sSlashLoc ? sSlashLoc - sNameLeft : strlen(sNameLeft);

        if (iPartLen == 0)
        {
            break;
        }

        bool bFound = false;
        size_t iBSL = 0;
        size_t iBSH = pFolder->vChildFolders.size();
        size_t iBSM = (iBSL + iBSH) >> 1;
        while (!bFound && iBSH > iBSL)
        {
            int iNiRes = strnicmp(pFolder->vChildFolders[iBSM]->sName.c_str(), sNameLeft, iPartLen);
            if (iNiRes > 0)
            {
                iBSH = iBSM;
                iBSM = (iBSL + iBSH) >> 1;
            }
            else if (iNiRes < 0)
            {
                iBSL = iBSM + 1;
                iBSM = (iBSL + iBSH) >> 1;
            }
            else
            {
                if (pFolder->vChildFolders[iBSM]->sName.size() != iPartLen)
                {
                    iBSH = iBSM;
                    iBSM = (iBSL + iBSH) >> 1;
                }
                else
                {
                    pFolder = pFolder->vChildFolders[iBSM].get();
                    bFound = true;
                }
            }
        }
        if (!bFound)
        {
            auto pNewFolder = std::make_unique<_Folder>();
            pNewFolder->pParent = pFolder;
            pNewFolder->sName = std::string(sNameLeft, iPartLen);
            pNewFolder->sFullName = pFolder->sFullName + "\\" + pNewFolder->sName;

            auto *pRaw = pNewFolder.get();
            pFolder->vChildFolders.push_back(std::move(pNewFolder));
            std::sort(pFolder->vChildFolders.begin(), pFolder->vChildFolders.end(), _SortFolds);

            pFolder = pRaw;
        }

        if (bIsFolder && sSlashLoc == nullptr)
        {
            sNameLeft = nullptr;
            break;
        }
    }

    if (ppFolder)
    {
        *ppFolder = pFolder;
    }

    if (sNameLeft == nullptr || *sNameLeft == 0)
    {
        // Nothing after the final slash, thus is a folder name only
        return nullptr;
    }

    size_t iBSL = 0;
    size_t iBSH = pFolder->vChildFiles.size();
    size_t iBSM = (iBSL + iBSH) >> 1;
    while (iBSH > iBSL)
    {
        int iNiRes = stricmp(pFolder->vChildFiles[iBSM]->sName.c_str(), sNameLeft);
        if (iNiRes > 0)
        {
            iBSH = iBSM;
            iBSM = (iBSL + iBSH) >> 1;
        }
        else if (iNiRes < 0)
        {
            iBSL = iBSM + 1;
            iBSM = (iBSL + iBSH) >> 1;
        }
        else
        {
            return pFolder->vChildFiles[iBSM].get();
        }
    }

    // file not found
    return nullptr;
}

bool CFileMap::_SortFolds(const std::unique_ptr<CFileMap::_Folder> &a, const std::unique_ptr<CFileMap::_Folder> &b)
{
    return (stricmp(a->sName.c_str(), b->sName.c_str()) < 0);
}

bool CFileMap::_SortFiles(const std::unique_ptr<CFileMap::_File> &a, const std::unique_ptr<CFileMap::_File> &b)
{
    return (stricmp(a->sName.c_str(), b->sName.c_str()) < 0);
}

void *CFileMap::RegisterSource(unsigned short iModNum, bool bIsSga, unsigned short iSourceNum, const char *sModName,
                               const char *sSourceName, IFileStore *pStore, IDirectoryTraverser *pTraverser,
                               bool bIsWritable, bool bIsOutput, bool bIsPureOutput)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    auto pSource = std::make_unique<_DataSource>();

    pSource->iSortNumber = 0;
    pSource->SetMod(iModNum);
    pSource->SetSourceType(bIsSga ? 1 : 0);
    pSource->SetSourceNumber(iSourceNum);
    pSource->sModName = sModName;
    pSource->sSourceName = sSourceName;
    pSource->pStore = pStore;
    pSource->pTraverser = pTraverser;
    pSource->bIsWritable = bIsWritable;
    pSource->bIsDefaultOutput = bIsOutput;
    pSource->bIsPureOutput = bIsPureOutput;

    auto *pRaw = pSource.get();
    m_vDataSources.push_back(std::move(pSource));

    return static_cast<void *>(pRaw);
}

void CFileMap::MapSga(void *pSource, CSgaFile *pSga)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    unsigned long iL = pSga->VGetEntryPointCount();
    for (unsigned long i = 0; i < iL; ++i)
    {
        const char *sToc = pSga->VGetEntryPoint(i);
        IDirectoryTraverser::IIterator *pItr = pSga->VIterate(sToc);
        MapIterator(pSource, sToc, pItr);
        delete pItr;
    }
}

void CFileMap::_Clean()
{
    m_mapTOCRewrites.clear();
    m_vTOCs.clear();
    m_vDataSources.clear();
}

void CFileMap::MapSingleFile(void *pSource, const char *sTocName, const char *sFullPath, const char *sPathPartial)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    // Get ToC
    _TOC *pToc = nullptr;
    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if (stricmp((**itr).sName.c_str(), sTocName) == 0)
        {
            pToc = itr->get();
            break;
        }
    }
    if (!pToc)
    {
        auto pNewToc = std::make_unique<_TOC>();
        pNewToc->sName = sTocName;
        pNewToc->pRootFolder = std::make_unique<_Folder>();
        pNewToc->pRootFolder->pParent = nullptr;
        pNewToc->pRootFolder->sFullName = sTocName;
        pNewToc->pRootFolder->sName = sTocName;

        pToc = pNewToc.get();
        m_vTOCs.push_back(std::move(pNewToc));
    }

    // Get Folder
    if (*sPathPartial == '\\')
    {
        ++sPathPartial;
    }
    _Folder *pCurrentFolder = pToc->pRootFolder.get();
    _FolderSetupSourceNameFromSingleFileMap(pCurrentFolder, (_DataSource *)pSource, sFullPath, sPathPartial);
    const char *sPathSeperator = strchr(sPathPartial, '\\');
    while (sPathSeperator)
    {
        size_t iPartL = sPathSeperator - sPathPartial;

        _Folder *pTheFolder = nullptr;
        for (auto itr = pCurrentFolder->vChildFolders.begin(); itr != pCurrentFolder->vChildFolders.end(); ++itr)
        {
            if ((**itr).sName.size() == iPartL && strnicmp((**itr).sName.c_str(), sPathPartial, iPartL) == 0)
            {
                pTheFolder = itr->get();
                break;
            }
        }
        if (pTheFolder == nullptr)
        {
            auto pNewFolder = std::make_unique<_Folder>();
            pNewFolder->pParent = pCurrentFolder;
            pNewFolder->sName = std::string(sPathPartial, iPartL);
            pNewFolder->sFullName = pCurrentFolder->sFullName + "\\" + pNewFolder->sName;

            pTheFolder = pNewFolder.get();
            pCurrentFolder->vChildFolders.push_back(std::move(pNewFolder));
            std::sort(pCurrentFolder->vChildFolders.begin(), pCurrentFolder->vChildFolders.end(), _SortFolds);

            pCurrentFolder = pTheFolder;
            sPathPartial = sPathSeperator + 1;
            sPathSeperator = strchr(sPathPartial, '\\');
            _FolderSetupSourceNameFromSingleFileMap(pCurrentFolder, (_DataSource *)pSource, sFullPath, sPathPartial);
        }
        else
        {
            pCurrentFolder = pTheFolder;
            sPathPartial = sPathSeperator + 1;
            sPathSeperator = strchr(sPathPartial, '\\');
        }
    }

    // Do File
    _File *pTheFile = nullptr;
    for (auto itr = pCurrentFolder->vChildFiles.begin(); itr != pCurrentFolder->vChildFiles.end(); ++itr)
    {
        if (stricmp(sPathPartial, (**itr).sName.c_str()) == 0)
        {
            pTheFile = itr->get();
            break;
        }
    }
    if (pTheFile == nullptr)
    {
        auto pNewFile = std::make_unique<_File>();
        pNewFile->pParent = pCurrentFolder;
        pNewFile->sName = sPathPartial;
        pTheFile = pNewFile.get();
        pCurrentFolder->vChildFiles.push_back(std::move(pNewFile));
        std::sort(pCurrentFolder->vChildFiles.begin(), pCurrentFolder->vChildFiles.end(), _SortFiles);
    }
    pTheFile->mapSources[(_DataSource *)pSource] = ((_DataSource *)pSource)->pTraverser->VGetLastWriteTime(sFullPath);
}

void CFileMap::MapIteratorDeep(void *pSource, const char *sPath, IDirectoryTraverser::IIterator *pItr)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    const char *sPathSplit = strchr(sPath, '\\');
    size_t iPartLen = sPathSplit ? (sPathSplit - sPath) : strlen(sPath);

    _TOC *pToc = nullptr;
    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if ((**itr).sName.size() == iPartLen && strnicmp((**itr).sName.c_str(), sPath, iPartLen) == 0)
        {
            pToc = itr->get();
            break;
        }
    }
    if (!pToc)
    {
        auto pNewToc = std::make_unique<_TOC>();
        pNewToc->sName = std::string(sPath, iPartLen);
        pNewToc->pRootFolder = std::make_unique<_Folder>();
        pNewToc->pRootFolder->pParent = nullptr;
        pNewToc->pRootFolder->sFullName = pNewToc->sName;
        pNewToc->pRootFolder->sName = pNewToc->sName;

        pToc = pNewToc.get();
        m_vTOCs.push_back(std::move(pNewToc));
    }
    _Folder *pFolder = pToc->pRootFolder.get(), *pChild = nullptr;
    while (sPathSplit)
    {
        sPath = sPathSplit + 1;
        sPathSplit = strchr(sPath, '\\');
        iPartLen = sPathSplit ? (sPathSplit - sPath) : strlen(sPath);

        pChild = nullptr;
        for (auto itr = pFolder->vChildFolders.begin(); itr != pFolder->vChildFolders.end(); ++itr)
        {
            if ((**itr).sName.size() == iPartLen && strnicmp((**itr).sName.c_str(), sPath, iPartLen) == 0)
            {
                pChild = itr->get();
                break;
            }
        }
        if (pChild == nullptr)
        {
            auto pNewChild = std::make_unique<_Folder>();
            pNewChild->pParent = pFolder;
            pNewChild->sName = std::string(sPath, iPartLen);
            pNewChild->sFullName = pFolder->sFullName + "\\" + pNewChild->sName;

            pChild = pNewChild.get();
            pFolder->vChildFolders.push_back(std::move(pNewChild));
            std::sort(pFolder->vChildFolders.begin(), pFolder->vChildFolders.end(), _SortFolds);
        }

        pFolder = pChild;
    }
    _RawMap((_DataSource *)pSource, pItr, pFolder);
}

void CFileMap::MapIterator(void *pSource, const char *sTocName, IDirectoryTraverser::IIterator *pItr)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    _TOC *pToc = nullptr;
    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if (stricmp((**itr).sName.c_str(), sTocName) == 0)
        {
            pToc = itr->get();
            break;
        }
    }
    if (!pToc)
    {
        auto pNewToc = std::make_unique<_TOC>();
        pNewToc->sName = sTocName;
        pNewToc->pRootFolder = std::make_unique<_Folder>();
        pNewToc->pRootFolder->pParent = nullptr;
        pNewToc->pRootFolder->sFullName = sTocName;
        pNewToc->pRootFolder->sName = sTocName;

        pToc = pNewToc.get();
        m_vTOCs.push_back(std::move(pNewToc));
    }

    _RawMap((_DataSource *)pSource, pItr, pToc->pRootFolder.get());
}

void CFileMap::MapSnapshot(void *pSource, const char *sTocName, const DirEntry &snapshot)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    _TOC *pToc = nullptr;
    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        if (stricmp((**itr).sName.c_str(), sTocName) == 0)
        {
            pToc = itr->get();
            break;
        }
    }
    if (!pToc)
    {
        auto pNewToc = std::make_unique<_TOC>();
        pNewToc->sName = sTocName;
        pNewToc->pRootFolder = std::make_unique<_Folder>();
        pNewToc->pRootFolder->pParent = nullptr;
        pNewToc->pRootFolder->sFullName = sTocName;
        pNewToc->pRootFolder->sName = sTocName;

        pToc = pNewToc.get();
        m_vTOCs.push_back(std::move(pNewToc));
    }

    _RawMapFromSnapshot((_DataSource *)pSource, snapshot, pToc->pRootFolder.get());
}

void CFileMap::_RawMapFromSnapshot(_DataSource *pSource, const DirEntry &snapshot, _Folder *pFolder)
{
    if (pFolder->mapSourceNames[pSource].empty() && !snapshot.directoryPath.empty())
    {
        pFolder->mapSourceNames[pSource] = snapshot.directoryPath;
    }

    for (const auto &entry : snapshot.children)
    {
        if (entry.isFile)
        {
            if (!pSource->bIsPureOutput)
            {
                auto it = std::lower_bound(pFolder->vChildFiles.begin(), pFolder->vChildFiles.end(), entry.name,
                                           [](const std::unique_ptr<_File> &f, const std::string &name)
                                           { return stricmp(f->sName.c_str(), name.c_str()) < 0; });
                _File *pTheFile;
                if (it != pFolder->vChildFiles.end() && stricmp((*it)->sName.c_str(), entry.name.c_str()) == 0)
                {
                    pTheFile = it->get();
                }
                else
                {
                    auto pNewFile = std::make_unique<_File>();
                    pNewFile->pParent = pFolder;
                    pNewFile->sName = entry.name;
                    pTheFile = pNewFile.get();
                    pFolder->vChildFiles.insert(it, std::move(pNewFile));
                }
                pTheFile->mapSources[pSource] = entry.lastWriteTime;
            }
        }
        else
        {
            auto it = std::lower_bound(pFolder->vChildFolders.begin(), pFolder->vChildFolders.end(), entry.name,
                                       [](const std::unique_ptr<_Folder> &f, const std::string &name)
                                       { return stricmp(f->sName.c_str(), name.c_str()) < 0; });
            _Folder *pTheFolder;
            if (it != pFolder->vChildFolders.end() && stricmp((*it)->sName.c_str(), entry.name.c_str()) == 0)
            {
                pTheFolder = it->get();
            }
            else
            {
                auto pNewFolder = std::make_unique<_Folder>();
                pNewFolder->pParent = pFolder;
                pNewFolder->sName = entry.name;
                pNewFolder->sFullName = pFolder->sFullName + "\\" + pNewFolder->sName;

                pTheFolder = pNewFolder.get();
                pFolder->vChildFolders.insert(it, std::move(pNewFolder));
            }

            _RawMapFromSnapshot(pSource, entry, pTheFolder);
        }
    }
}

void CFileMap::_FolderSetupSourceNameFromSingleFileMap(_Folder *pFolder, _DataSource *pDataSource,
                                                       const char *sPathFull, const char *sPathPartLeft)
{
    if (pFolder->mapSourceNames[pDataSource].empty())
    {
        size_t iLFull = strlen(sPathFull);
        size_t iLPart = strlen(sPathPartLeft);
        size_t iL = iLFull - iLPart - 1;
        pFolder->mapSourceNames[pDataSource] = std::string(sPathFull, iL);
    }
}

void CFileMap::EraseSource(void *_pSource)
{
    std::lock_guard<std::recursive_mutex> lock(m_mtxMap);
    auto *pSource = (_DataSource *)_pSource;
    for (auto itr = m_vTOCs.begin(); itr != m_vTOCs.end(); ++itr)
    {
        _EraseSourceFromFolder(pSource, (**itr).pRootFolder.get());
    }

    for (auto itr = m_vDataSources.begin(); itr != m_vDataSources.end(); ++itr)
    {
        if (itr->get() == pSource)
        {
            m_vDataSources.erase(itr);
            break;
        }
    }
}

void CFileMap::_EraseSourceFromFolder(_DataSource *pSource, _Folder *pFolder)
{
    pFolder->mapSourceNames.erase(pSource);

    for (auto itr = pFolder->vChildFolders.begin(); itr != pFolder->vChildFolders.end(); ++itr)
    {
        _EraseSourceFromFolder(pSource, itr->get());
    }

    for (auto itr = pFolder->vChildFiles.begin(); itr != pFolder->vChildFiles.end(); ++itr)
    {
        if ((**itr).mapSources.find(pSource) != (**itr).mapSources.end())
        {
            (**itr).mapSources.erase(pSource);
        }
    }
}

void CFileMap::_RawMap(_DataSource *pSource, IDirectoryTraverser::IIterator *pItr, _Folder *pFolder)
{
    if (pFolder->mapSourceNames[pSource].empty())
    {
        pFolder->mapSourceNames[pSource] = pItr->VGetDirectoryPath();
    }

    while (pItr->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
    {
        if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
        {
            if (!pSource->bIsPureOutput)
            {
                const char *sName = pItr->VGetName();
                auto it = std::lower_bound(pFolder->vChildFiles.begin(), pFolder->vChildFiles.end(), sName,
                                           [](const std::unique_ptr<_File> &f, const char *name)
                                           { return stricmp(f->sName.c_str(), name) < 0; });
                _File *pTheFile;
                if (it != pFolder->vChildFiles.end() && stricmp((*it)->sName.c_str(), sName) == 0)
                {
                    pTheFile = it->get();
                }
                else
                {
                    auto pNewFile = std::make_unique<_File>();
                    pNewFile->pParent = pFolder;
                    pNewFile->sName = sName;
                    pTheFile = pNewFile.get();
                    pFolder->vChildFiles.insert(it, std::move(pNewFile));
                }
                pTheFile->mapSources[pSource] = pItr->VGetLastWriteTime();
            }
        }
        else
        {
            const char *sName = pItr->VGetName();
            auto it = std::lower_bound(pFolder->vChildFolders.begin(), pFolder->vChildFolders.end(), sName,
                                       [](const std::unique_ptr<_Folder> &f, const char *name)
                                       { return stricmp(f->sName.c_str(), name) < 0; });
            _Folder *pTheFolder;
            if (it != pFolder->vChildFolders.end() && stricmp((*it)->sName.c_str(), sName) == 0)
            {
                pTheFolder = it->get();
            }
            else
            {
                auto pNewFolder = std::make_unique<_Folder>();
                pNewFolder->pParent = pFolder;
                pNewFolder->sName = sName;
                pNewFolder->sFullName = pFolder->sFullName + "\\" + pNewFolder->sName;

                pTheFolder = pNewFolder.get();
                pFolder->vChildFolders.insert(it, std::move(pNewFolder));
            }

            IDirectoryTraverser::IIterator *pSubItr = pItr->VOpenSubDir();
            try
            {
                _RawMap(pSource, pSubItr, pTheFolder);
            }
            catch (const CRainmanException &e)
            {
                delete pSubItr;
                throw CRainmanException(e, __FILE__, __LINE__, "Raw map of \'%s\' failed", pFolder->sFullName.c_str());
            }
            delete pSubItr;
        }

        if (pItr->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
        {
            break;
        }
    }
}
