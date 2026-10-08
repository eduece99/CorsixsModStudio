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

#include "rainman/module/CModuleFile.h"
#include "rainman/module/CModuleParser.h"
#include "rainman/module/CResourceLoader.h"
#include "rainman/archive/CSgaFile.h"
#include "rainman/localization/CUcsFile.h"
#include "rainman/module/CFileMap.h"
#include "rainman/io/CFileSystemStore.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include "rainman/core/Internal_Util.h"
#include "rainman/core/Exception.h"
#include <new>
#include "rainman/core/memdebug.h"

CModuleFile::CModuleFile()
{
    m_eModuleType = MT_DawnOfWar;
    m_pParentModule = nullptr;

    m_pFSS = nullptr;
    m_pNewFileMap = nullptr;

    m_iFileMapModNumber = 0;

    m_bIsFauxModule = false;
}

CModuleFile::~CModuleFile()
{
    bool bOwnsFileSystem = (m_pParentModule == nullptr);
    _Clean();
    if (bOwnsFileSystem)
    {
        delete m_pFSS;
        m_pFSS = nullptr;
    }
}

void CModuleFile::SetLocale(const char *sLocale) { m_sLocale = sLocale; }

const char *CModuleFile::GetLocale() const { return m_sLocale.c_str(); }

void CModuleFile::SetMapPackRootFolder(const wchar_t *sFolder)
{
    m_sScenarioPackRootFolder = sFolder;
    m_saScenarioPackRootFolder.resize(m_sScenarioPackRootFolder.size());
    for (size_t i = 0; i < m_sScenarioPackRootFolder.size(); ++i)
    {
        m_saScenarioPackRootFolder[i] = static_cast<char>(m_sScenarioPackRootFolder[i]);
    }
}

const wchar_t *CModuleFile::GetMapPackRootFolder() const { return m_sScenarioPackRootFolder.c_str(); }

#define GET_SET_DIRECTIVE(name, int_name)                                                                              \
    const char *CModuleFile::Get##name() const { return (int_name).c_str(); }                                          \
    void CModuleFile::Set##name(const char *value) { (int_name) = value ? value : ""; }

GET_SET_DIRECTIVE(Name, m_metadata.m_sName)
GET_SET_DIRECTIVE(ModFolder, m_metadata.m_sModFolder)
GET_SET_DIRECTIVE(LocaleFolder, m_metadata.m_sLocFolder)
GET_SET_DIRECTIVE(Description, m_metadata.m_sDescription)
GET_SET_DIRECTIVE(TextureFe, m_metadata.m_sTextureFe)
GET_SET_DIRECTIVE(TextureIcon, m_metadata.m_sTextureIcon)
GET_SET_DIRECTIVE(UiName, m_metadata.m_sUiName)
GET_SET_DIRECTIVE(DllName, m_metadata.m_sDllName)

#undef GET_SET_DIRECTIVE
#define GET_SET_DIRECTIVE(name, int_name)                                                                              \
    long CModuleFile::Get##name() const { return int_name; }                                                           \
    void CModuleFile::Set##name(long value) { (int_name) = value; }

GET_SET_DIRECTIVE(VersionMajor, m_metadata.m_iModVersionMajor)
GET_SET_DIRECTIVE(VersionMinor, m_metadata.m_iModVersionMinor)
GET_SET_DIRECTIVE(VersionRevision, m_metadata.m_iModVersionRevision)

#undef GET_SET_DIRECTIVE

size_t CModuleFile::GetEngineCount() const { return m_vEngines.size(); }

CModuleFile *CModuleFile::GetEngine(size_t iId)
{
    if (iId < 0 || iId >= GetEngineCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetEngineCount());
    }
    return m_vEngines[iId].get();
}

const CModuleFile *CModuleFile::GetEngine(size_t iId) const
{
    if (iId < 0 || iId >= GetEngineCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetEngineCount());
    }
    return m_vEngines[iId].get();
}

CModuleFile::eModuleType CModuleFile::GetModuleType() const { return m_eModuleType; }

#define QUICK_TRYCAT(op)                                                                                               \
    try                                                                                                                \
    {                                                                                                                  \
        op                                                                                                             \
    }                                                                                                                  \
    CATCH_THROW("File view error")

// IFileStore Interface
void CModuleFile::VInit(void *pUnused)
{
    RAINMAN_LOG_INFO("CModuleFile::VInit() — initialising module file");
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(m_pNewFileMap->VInit(pUnused);)
}

IFileStore::IStream *CModuleFile::VOpenStream(const char *sFile)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VOpenStream(sFile);)
}

IFileStore::IOutputStream *CModuleFile::VOpenOutputStream(const char *sIdentifier, bool bEraseIfPresent)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VOpenOutputStream(sIdentifier, bEraseIfPresent);)
}

// IDirectoryTraverser Interface
tLastWriteTime CModuleFile::VGetLastWriteTime(const char *sPath)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VGetLastWriteTime(sPath);)
}

void CModuleFile::VCreateFolderIn(const char *sPath, const char *sNewFolderName)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VCreateFolderIn(sPath, sNewFolderName);)
}

IDirectoryTraverser::IIterator *CModuleFile::VIterate(const char *sPath)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    try
    {
        return m_pNewFileMap->VIterate(sPath);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "File view error iterating path");
    }
}

unsigned long CModuleFile::VGetEntryPointCount()
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VGetEntryPointCount();)
}

const char *CModuleFile::VGetEntryPoint(unsigned long iID)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }
    QUICK_TRYCAT(return m_pNewFileMap->VGetEntryPoint(iID);)
}

#undef QUICK_TRYCAT

bool CModuleFile::VDirectoryExists(const char *sPath)
{
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }

    // Extract the TOC name (everything before the first separator)
    const char *sSlash = strchr(sPath, '\\');
    if (sSlash == nullptr)
    {
        sSlash = strchr(sPath, '/');
    }

    size_t iTocLen = sSlash ? (size_t)(sSlash - sPath) : strlen(sPath);
    if (iTocLen == 0)
    {
        return false;
    }

    unsigned long iCount = m_pNewFileMap->VGetEntryPointCount();
    for (unsigned long i = 0; i < iCount; ++i)
    {
        const char *sEntry = m_pNewFileMap->VGetEntryPoint(i);
        if (strlen(sEntry) == iTocLen && strnicmp(sEntry, sPath, iTocLen) == 0)
        {
            return true;
        }
    }
    return false;
}

long CModuleFile::GetSgaOutputVersion()
{
    if (m_eModuleType == MT_DawnOfWar)
    {
        return 2;
    }
    else if (m_eModuleType == MT_CompanyOfHeroesEarly)
    {
        return 4;
    }
    else if (m_eModuleType == MT_CompanyOfHeroes) // NOLINT(bugprone-branch-clone) distinct type, same version
    {
        return 4;
    }
    return 2;
}

CModuleFile::CUcsHandler::CUcsHandler() {}

CModuleFile::CUcsHandler::~CUcsHandler() {}

const char *CModuleFile::CUcsHandler::GetFileName() const { return m_sName.c_str(); }

std::shared_ptr<const CUcsFile> CModuleFile::CUcsHandler::GetUcsHandle() const { return m_pHandle; }

std::shared_ptr<CUcsFile> CModuleFile::CUcsHandler::GetUcsHandle() { return m_pHandle; }

CModuleFile::CFolderHandler::CFolderHandler() { m_iNumber = 0; }

const char *CModuleFile::CFolderHandler::GetName() const { return m_sName.c_str(); }

long CModuleFile::CFolderHandler::GetNumber() const { return m_iNumber; }

CModuleFile::CFolderHandler::~CFolderHandler() {}

CModuleFile::CArchiveHandler::CArchiveHandler()
{
    m_iNumber = 0;
    m_pHandle = nullptr;
}

const char *CModuleFile::CArchiveHandler::GetFileName() const { return m_sName.c_str(); }

CSgaFile *CModuleFile::CArchiveHandler::GetFileHandle() const { return m_pHandle; }

long CModuleFile::CArchiveHandler::GetNumber() const { return m_iNumber; }

CModuleFile::CArchiveHandler::~CArchiveHandler() {}

CModuleFile::CRequiredHandler::CRequiredHandler()
{
    m_iNumber = 0;
    m_pHandle = nullptr;
}

const char *CModuleFile::CRequiredHandler::GetFileName() const { return m_sName.c_str(); }

CModuleFile *CModuleFile::CRequiredHandler::GetModHandle() { return m_pHandle; }

const CModuleFile *CModuleFile::CRequiredHandler::GetModHandle() const { return m_pHandle; }

long CModuleFile::CRequiredHandler::GetNumber() const { return m_iNumber; }

CModuleFile::CRequiredHandler::~CRequiredHandler() {}

CModuleFile::CCompatibleHandler::CCompatibleHandler() { m_iNumber = 0; }

const char *CModuleFile::CCompatibleHandler::GetFileName() const { return m_sName.c_str(); }

long CModuleFile::CCompatibleHandler::GetNumber() const { return m_iNumber; }

CModuleFile::CCompatibleHandler::~CCompatibleHandler() {}

CModuleFile::CCohDataSource::CCohDataSource()
{
    m_iNumber = 0;
    m_bIsLoaded = false;
    m_bCanWriteToFolder = true;
}

CModuleFile::CCohDataSource::~CCohDataSource() {}

void CModuleFile::_Clean()
{
    m_eModuleType = MT_DawnOfWar;
    m_sApplicationPath.clear();
    m_sGameInstallPath.clear();
    m_sFilename.clear();
    m_sScenarioPackRootFolder.clear();
    m_saScenarioPackRootFolder.clear();

    m_sFileMapName.clear();
    m_metadata.Reset();

    m_sCohThisModFolder.clear();

    m_iFileMapModNumber = 0;

    _CleanResources();

    m_pParentModule = nullptr;

    m_vFolders.clear();
    m_vArchives.clear();
    m_vRequireds.clear();
    m_vCompatibles.clear();
    m_vDataSources.clear();

    m_vEngines.clear();

    m_bIsFauxModule = false;
}

void CModuleFile::_CleanResources()
{
    if (!m_pParentModule)
    {
        delete m_pNewFileMap;
        m_pNewFileMap = nullptr;
    }

    for (auto &pArch : m_vArchives)
    {
        if (pArch->m_pHandle)
        {
            if (pArch->m_pHandle->GetInputStream())
            {
                delete pArch->m_pHandle->GetInputStream();
            }
            delete pArch->m_pHandle;
        }
        pArch->m_pHandle = nullptr;
    }

    for (auto &pReq : m_vRequireds)
    {
        delete pReq->m_pHandle;
        pReq->m_pHandle = nullptr;
    }

    m_vLocaleTexts.clear();

    for (auto &pDS : m_vDataSources)
    {
        for (auto &pArch : pDS->m_vArchives)
        {
            if (pArch->m_pHandle)
            {
                if (pArch->m_pHandle->GetInputStream())
                {
                    delete pArch->m_pHandle->GetInputStream();
                }
                delete pArch->m_pHandle;
            }
            pArch->m_pHandle = nullptr;
        }
    }

    m_vEngines.clear();
}

void CModuleFile::LoadSgaAsMod(const char *sFileName, CALLBACK_ARG)
{
    RAINMAN_LOG_INFO("CModuleFile::LoadSgaAsMod(\"{}\")", sFileName ? sFileName : "(null)");
    _Clean();

    m_bIsFauxModule = true;

    if (m_pFSS == nullptr)
    {
        m_pFSS = new CFileSystemStore;
    }
    if (m_pNewFileMap == nullptr)
    {
        m_pNewFileMap = new CFileMap;
    }

    { // Set m_sApplicationPath and m_sFilename from sFileName
        m_sApplicationPath = sFileName;
        auto slashPos = m_sApplicationPath.find_last_of("\\/");
        if (slashPos != std::string::npos)
        {
            m_sFilename = m_sApplicationPath.substr(slashPos + 1);
            m_sFileMapName = m_sFilename;
            m_sApplicationPath.resize(slashPos + 1);
        }
    }

    CSgaFile *pSga;
    CResourceLoader::DoLoadArchive(*this, sFileName, &pSga, 0, m_sFileMapName.c_str(), THE_CALLBACK);

    switch (pSga->GetVersion())
    {
    case 2:
        m_eModuleType = MT_DawnOfWar;
        break;

    case 4:
        m_eModuleType = MT_CompanyOfHeroes;
        break;
    default:
        break;
    };

    m_metadata.m_sModFolder = "";

    auto pHandler = std::make_unique<CArchiveHandler>();
    pHandler->m_iNumber = 0;
    pHandler->m_pHandle = pSga;
    pHandler->m_sName = m_sFilename;
    m_vArchives.push_back(std::move(pHandler));
}

void CModuleFile::LoadModuleFile(const char *sFileName, CALLBACK_ARG)
{
    RAINMAN_LOG_INFO("CModuleFile::LoadModuleFile(\"{}\")", sFileName ? sFileName : "(null)");
    _Clean();

    std::string sNativeModulePath = sFileName ? sFileName : "";
#ifdef RAINMAN_GNUC
    std::replace(sNativeModulePath.begin(), sNativeModulePath.end(), '\\', '/');
#endif

    // Set m_sApplicationPath and m_sFilename from sFileName
    m_sApplicationPath = sNativeModulePath;
    auto slashPos = m_sApplicationPath.find_last_of("\\/");
    if (slashPos != std::string::npos)
    {
        m_sFilename = m_sApplicationPath.substr(slashPos + 1);
        m_sApplicationPath.resize(slashPos + 1);
    }

    m_sFileMapName = m_sFilename;
    auto dotPos = m_sFileMapName.rfind('.');
    if (dotPos != std::string::npos)
    {
        m_sFileMapName.resize(dotPos);
    }

    // Parse the module file
    auto result = CModuleParser::Parse(sNativeModulePath.c_str());
    m_metadata = std::move(result.metadata);
    m_eModuleType = static_cast<eModuleType>(result.iModuleType);

    // Populate folder handlers from parse result
    for (auto &entry : result.folders)
    {
        auto pHandler = std::make_unique<CFolderHandler>();
        pHandler->m_iNumber = entry.iNumber;
        pHandler->m_sName = entry.sName;
        m_vFolders.push_back(std::move(pHandler));
    }

    // Populate archive handlers
    for (auto &entry : result.archives)
    {
        auto pHandler = std::make_unique<CArchiveHandler>();
        pHandler->m_iNumber = entry.iNumber;
        pHandler->m_sName = entry.sName;
        m_vArchives.push_back(std::move(pHandler));
    }

    // Populate required handlers
    for (auto &entry : result.requireds)
    {
        auto pHandler = std::make_unique<CRequiredHandler>();
        pHandler->m_iNumber = entry.iNumber;
        pHandler->m_sName = entry.sName;
        m_vRequireds.push_back(std::move(pHandler));
    }

    // Populate compatible handlers
    for (auto &entry : result.compatibles)
    {
        auto pHandler = std::make_unique<CCompatibleHandler>();
        pHandler->m_iNumber = entry.iNumber;
        pHandler->m_sName = entry.sName;
        m_vCompatibles.push_back(std::move(pHandler));
    }

    // Populate data source handlers
    for (auto &ds : result.dataSources)
    {
        auto pDS = std::make_unique<CCohDataSource>();
        pDS->m_sToc = ds.sToc;
        pDS->m_sOption = ds.sOption;
        pDS->m_iNumber = ds.iNumber;
        pDS->m_sFolder = ds.sFolder;
        for (auto &arch : ds.archives)
        {
            auto pHandler = std::make_unique<CArchiveHandler>();
            pHandler->m_iNumber = arch.iNumber;
            pHandler->m_sName = arch.sName;
            pDS->m_vArchives.push_back(std::move(pHandler));
        }
        m_vDataSources.push_back(std::move(pDS));
    }

    // CoH post-processing
    if (m_eModuleType == MT_CompanyOfHeroes)
    {
        m_sCohThisModFolder = m_sApplicationPath + m_metadata.m_sModFolder;
        auto sSlashLoc = m_sCohThisModFolder.rfind('\\');
        if (sSlashLoc != std::string::npos)
        {
            m_sCohThisModFolder.resize(sSlashLoc + 1);
        }
    }
}

std::string CModuleFile::_DawnOfWarRemoveDynamics(const char *sStr, const char *sAlsoAppend)
{
    std::string result;
    result.reserve(strlen(sStr) + (sAlsoAppend ? strlen(sAlsoAppend) : 0) + 64);

    while (*sStr)
    {
        if (*sStr == '%')
        {
            if (strncmp(sStr, "%LOCALE%", 8) == 0)
            {
                result += "Locale\\";
                result += m_sLocale;
                sStr += 8;
                continue;
            }
            if (strncmp(sStr, "%TEXTURE-LEVEL%", 15) == 0)
            {
                result += "Full";
                sStr += 15;
                continue;
            }
            if (strncmp(sStr, "%SOUND-LEVEL%", 13) == 0)
            {
                result += "Full";
                sStr += 13;
                continue;
            }
            if (strncmp(sStr, "%MODEL-LEVEL%", 13) == 0)
            {
                result += "High";
                sStr += 13;
                continue;
            }
        }
        result += *sStr;
        ++sStr;
    }
    if (sAlsoAppend)
    {
        result += sAlsoAppend;
    }
    return result;
}

const char *CModuleFile::GetFileMapName() const { return m_sFileMapName.c_str(); }

const char *CModuleFile::GetFileMapName() { return m_sFileMapName.c_str(); }

void CModuleFile::ReloadResources(unsigned long iReloadWhat, unsigned long iReloadWhatRequiredMods,
                                  unsigned long iReloadWhatEngines, CALLBACK_ARG)
{
    CResourceLoader::Load(*this, iReloadWhat, iReloadWhatRequiredMods, iReloadWhatEngines, THE_CALLBACK);
}

const char *CModuleFile::GetApplicationPath() const { return m_sApplicationPath.c_str(); }

void CModuleFile::SetGameInstallPath(const char *sPath)
{
    m_sGameInstallPath = sPath ? sPath : "";
#ifdef RAINMAN_GNUC
    std::replace(m_sGameInstallPath.begin(), m_sGameInstallPath.end(), '\\', '/');
#endif
    if (!m_sGameInstallPath.empty() && m_sGameInstallPath.back() != '\\' && m_sGameInstallPath.back() != '/')
    {
#ifdef RAINMAN_GNUC
        m_sGameInstallPath += '/';
#else
        m_sGameInstallPath += '\\';
#endif
    }
}

void CModuleFile::NewUCS(const char *sName, std::shared_ptr<CUcsFile> pUcs)
{
    auto pHandler = std::make_unique<CUcsHandler>();
    pHandler->m_sName = sName;
    pHandler->m_pHandle = std::move(pUcs);

    m_vLocaleTexts.push_back(std::move(pHandler));
}

const wchar_t *CModuleFile::ResolveUCS(const char *sDollarString)
{
    if (!CUcsFile::IsDollarString(sDollarString))
    {
        throw CRainmanException(__FILE__, __LINE__, "sDollarString must be a dollar string");
    }
    try
    {
        return ResolveUCS((unsigned long)atol(sDollarString + 1));
    }
    CATCH_THROW("Cannot resolve value as integer")
}

const wchar_t *CModuleFile::ResolveUCS(const wchar_t *sDollarString)
{
    if (!CUcsFile::IsDollarString(sDollarString))
    {
        throw CRainmanException(__FILE__, __LINE__, "sDollarString must be a dollar string");
    }
    try
    {
        return ResolveUCS(wcstoul(sDollarString + 1, nullptr, 10));
    }
    CATCH_THROW("Cannot resolve value as integer")
}

const wchar_t *CModuleFile::ResolveUCS(unsigned long iStringID)
{
    const wchar_t *t;

    for (auto &pUcs : m_vLocaleTexts)
    {
        if (pUcs && pUcs->m_pHandle &&
            (t = pUcs->m_pHandle->ResolveStringID(iStringID))) // NOLINT(bugprone-assignment-in-if-condition)
        {
            return t;
        }
    }

    for (auto &pReq : m_vRequireds)
    {
        if (pReq && pReq->m_pHandle &&
            (t = pReq->m_pHandle->ResolveUCS(iStringID))) // NOLINT(bugprone-assignment-in-if-condition)
        {
            return t;
        }
    }

    for (auto &pEngine : m_vEngines)
    {
        if (pEngine && (t = pEngine->ResolveUCS(iStringID))) // NOLINT(bugprone-assignment-in-if-condition)
        {
            return t;
        }
    }

    return nullptr;
}

size_t CModuleFile::GetUcsCount() const { return m_vLocaleTexts.size(); }

CModuleFile::CUcsHandler *CModuleFile::GetUcs(size_t iId)
{
    if (iId < 0 || iId >= GetUcsCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetUcsCount());
    }
    return m_vLocaleTexts[iId].get();
}

const CModuleFile::CUcsHandler *CModuleFile::GetUcs(size_t iId) const
{
    if (iId < 0 || iId >= GetUcsCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetUcsCount());
    }
    return m_vLocaleTexts[iId].get();
}

void CModuleFile::DeleteUcs(size_t iId)
{
    if (iId < 0 || iId >= GetUcsCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetUcsCount());
    }
    m_vLocaleTexts.erase(m_vLocaleTexts.begin() + iId);
}

size_t CModuleFile::GetRequiredCount() const { return m_vRequireds.size(); }

CModuleFile::CRequiredHandler *CModuleFile::GetRequired(size_t iId)
{
    if (iId < 0 || iId >= GetRequiredCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetRequiredCount());
    }
    return m_vRequireds[iId].get();
}

const CModuleFile::CRequiredHandler *CModuleFile::GetRequired(size_t iId) const
{
    if (iId < 0 || iId >= GetRequiredCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetRequiredCount());
    }
    return m_vRequireds[iId].get();
}

size_t CModuleFile::GetFolderCount() { return m_vFolders.size(); }

CModuleFile::CFolderHandler *CModuleFile::GetFolder(size_t iId)
{
    if (iId < 0 || iId >= GetFolderCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetFolderCount());
    }
    return m_vFolders[iId].get();
}

size_t CModuleFile::GetArchiveCount() { return m_vArchives.size(); }

CModuleFile::CArchiveHandler *CModuleFile::GetArchive(size_t iId)
{
    if (iId < 0 || iId >= GetArchiveCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetArchiveCount());
    }
    return m_vArchives[iId].get();
}

size_t CModuleFile::GetArchiveFullPath(size_t iId, char *sOutput)
{
    if (iId < 0 || iId >= GetArchiveCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetArchiveCount());
    }
    auto &pHandler = m_vArchives[iId];

    size_t iLen;
    if (m_eModuleType == MT_DawnOfWar)
    {
        std::string sWithoutDynamics = _DawnOfWarRemoveDynamics(pHandler->m_sName.c_str());
        iLen = m_sApplicationPath.size() + m_metadata.m_sModFolder.size() + sWithoutDynamics.size() + 2;
        if (sOutput)
        {
            sprintf(sOutput, "%s%s\\%s", m_sApplicationPath.c_str(), m_metadata.m_sModFolder.c_str(),
                    sWithoutDynamics.c_str());
        }
    }
    else if (m_eModuleType == MT_CompanyOfHeroesEarly)
    {
        iLen = m_sApplicationPath.size() + m_metadata.m_sModFolder.size() + pHandler->m_sName.size() + 11;
        if (sOutput)
        {
            sprintf(sOutput, "%s%s\\Archives\\%s", m_sApplicationPath.c_str(), m_metadata.m_sModFolder.c_str(),
                    pHandler->m_sName.c_str());
        }
    }
    else if (m_eModuleType == MT_CompanyOfHeroes)
    {
        iLen = m_sApplicationPath.size() + pHandler->m_sName.size() + 1;
        if (sOutput)
        {
            sprintf(sOutput, "%s%s", m_sApplicationPath.c_str(), pHandler->m_sName.c_str());
        }
    }
    else
    {
        iLen = 1;
        if (sOutput)
        {
            *sOutput = 0;
        }
    }
#ifdef RAINMAN_GNUC
    if (sOutput)
    {
        std::replace(sOutput, sOutput + std::strlen(sOutput), '\\', '/');
    }
#endif
    return iLen;
}

size_t CModuleFile::GetCompatibleCount() { return m_vCompatibles.size(); }

CModuleFile::CCompatibleHandler *CModuleFile::GetCompatible(size_t iId)
{
    if (iId < 0 || iId >= GetCompatibleCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetCompatibleCount());
    }
    return m_vCompatibles[iId].get();
}

size_t CModuleFile::GetDataSourceCount() { return m_vDataSources.size(); }

CModuleFile::CCohDataSource *CModuleFile::GetDataSource(size_t iId)
{
    if (iId < 0 || iId >= GetDataSourceCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetDataSourceCount());
    }
    return m_vDataSources[iId].get();
}

size_t CModuleFile::CCohDataSource::GetArchiveCount() { return m_vArchives.size(); }

CModuleFile::CArchiveHandler *CModuleFile::CCohDataSource::GetArchive(size_t iId)
{
    if (iId < 0 || iId >= GetArchiveCount())
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "ID %lu is outside of range 0-%lu", (unsigned long)iId,
                                (unsigned long)GetArchiveCount());
    }
    return m_vArchives[iId].get();
}

const char *CModuleFile::CCohDataSource::GetToc() const { return m_sToc.c_str(); }

void CModuleFile::CCohDataSource::SetToc(const char *sToc) { m_sToc = sToc; }

const char *CModuleFile::CCohDataSource::GetOption() const { return m_sOption.c_str(); }

void CModuleFile::CCohDataSource::SetOption(const char *sOption) { m_sOption = sOption; }

signed long CModuleFile::CCohDataSource::GetNumber() const { return m_iNumber; }

void CModuleFile::CCohDataSource::SetNumber(signed long iNumber) { m_iNumber = iNumber; }

const char *CModuleFile::CCohDataSource::GetFolder() const { return m_sFolder.c_str(); }

void CModuleFile::CCohDataSource::SetFolder(const char *sFolder) { m_sFolder = sFolder; }

bool CModuleFile::CCohDataSource::IsLoaded() const { return m_bIsLoaded; }
