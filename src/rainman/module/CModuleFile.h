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

#pragma once

#include "rainman/core/gnuc_defines.h"
#include "rainman/io/IFileStore.h"
#include "rainman/io/IDirectoryTraverser.h"
#include "rainman/core/Api.h"
#include "rainman/core/Callbacks.h"
#include "rainman/module/CModuleMetadata.h"
#include <memory>
#include <string>
#include <vector>

class CSgaFile;
class CUcsFile;
class CFileMap;
class CFileSystemStore;
class CResourceLoader;

class RAINMAN_API CModuleFile : public IFileStore, public IDirectoryTraverser
{
  public:
    CModuleFile();
    ~CModuleFile();

    // IFileStore Interface
    virtual void VInit(void *pUnused = 0);
    virtual IStream *VOpenStream(const char *sFile);
    virtual IOutputStream *VOpenOutputStream(const char *sIdentifier, bool bEraseIfPresent);

    // IDirectoryTraverser Interface
    virtual IDirectoryTraverser::IIterator *VIterate(const char *sPath);
    virtual unsigned long VGetEntryPointCount();
    virtual const char *VGetEntryPoint(unsigned long iID);
    virtual void VCreateFolderIn(const char *sPath, const char *sNewFolderName);
    virtual tLastWriteTime VGetLastWriteTime(const char *sPath);
    virtual bool VDirectoryExists(const char *sPath);

    enum eModuleType
    {
        MT_DawnOfWar,            //!< Dawn of war / WA / DC / SS .module file
        MT_CompanyOfHeroesEarly, //!< Company of Heroes SP pre-release beta / early versions of MP beta
        MT_CompanyOfHeroes       //!< Company of Heroes MP beta / SP demo / release / Opposing Fronts
    };

    /*!
        Resets the CModuleFile's state and parses the directives the the specified .module file.
        After calling this function, call ReloadResources() to actually load the UCSs, SGAs, etc.
        \return Returns no value, but throws a CRainmanException on error
    */
    void LoadModuleFile(const char *sFileName, CALLBACK_ARG_OPT);
    void LoadSgaAsMod(const char *sFileName, CALLBACK_ARG_OPT);

    const static unsigned long RR_LocalisedText = 1; //!< Reload DoW UCS files / CoH UCS files
    const static unsigned long RR_DataFolders = 2;   //!< Reload all data folders
    const static unsigned long RR_DataArchives = 4;  //!< Reload all SGA archives
    const static unsigned long RR_RequiredMods = 8;  //!< Reparse all required mods
    const static unsigned long RR_Engines = 16;      //!< Reparse all engine settings
    const static unsigned long RR_DataGeneric = 32;  //!< Reload the datageneric folder
    const static unsigned long RR_MapArchives = 64;  //!< Reload all CoH map SGA packs
    const static unsigned long RR_All = RR_LocalisedText | RR_DataFolders | RR_DataArchives | RR_RequiredMods |
                                        RR_Engines | RR_DataGeneric | RR_MapArchives;

    /*!
        Unloads all loaded resources, and then reloads everything passed in iReloadWhat.
        If RequiredMods or Engines are re-parsed, then iReloadWhatRequiredMods and iReloadWhatEngines are used to
       specify what resources to load for these.
        \return Returns no value, but throws a CRainmanException on error
    */
    void ReloadResources(unsigned long iReloadWhat = RR_All, unsigned long iReloadWhatRequiredMods = RR_All,
                         unsigned long iReloadWhatEngines = RR_All, CALLBACK_ARG_OPT);

    // Get / Set mod settings

    const char *GetFileMapName() const;
    const char *GetFileMapName();
    void SetLocale(const char *sLocale);
    const char *GetLocale() const;
    eModuleType GetModuleType() const;
    void SetMapPackRootFolder(const wchar_t *sFolder);
    const wchar_t *GetMapPackRootFolder() const;
    const char *GetApplicationPath() const;
    //! Configure a separate install root for DoW dependencies and Engine. Call after LoadModuleFile().
    //! nullptr or an empty string restores legacy same-directory resolution.
    void SetGameInstallPath(const char *sPath);

    // Get / Set mod directives

#define GET_SET_DIRECTIVE(type, name)                                                                                  \
    type Get##name() const;                                                                                            \
    void Set##name(type value)

    GET_SET_DIRECTIVE(const char *, Name);
    GET_SET_DIRECTIVE(const char *, ModFolder);
    GET_SET_DIRECTIVE(const char *, LocaleFolder);
    GET_SET_DIRECTIVE(const char *, Description);
    GET_SET_DIRECTIVE(const char *, TextureFe);
    GET_SET_DIRECTIVE(const char *, TextureIcon);
    GET_SET_DIRECTIVE(const char *, UiName);
    GET_SET_DIRECTIVE(const char *, DllName);
    GET_SET_DIRECTIVE(long, VersionMajor);
    GET_SET_DIRECTIVE(long, VersionMinor);
    GET_SET_DIRECTIVE(long, VersionRevision);

#undef GET_SET_DIRECTIVE

    class RAINMAN_API CFolderHandler
    {
      public:
        signed long GetNumber() const;
        const char *GetName() const;

        CFolderHandler();
        ~CFolderHandler();

        friend class CModuleFile;
        friend class CResourceLoader;

      private:
        signed long m_iNumber;
        std::string m_sName;
    };

    size_t GetFolderCount();
    CFolderHandler *GetFolder(size_t iId);

    friend void CModuleFile_ArchiveForEach(IDirectoryTraverser::IIterator *, void *);
    friend void CModuleFile_ArchiveForEachNoErrors(IDirectoryTraverser::IIterator *, void *);
    friend class CResourceLoader;
    class RAINMAN_API CArchiveHandler
    {
      public:
        signed long GetNumber() const;
        const char *GetFileName() const;
        CSgaFile *GetFileHandle() const;

        CArchiveHandler();
        ~CArchiveHandler();

        friend class CModuleFile;
        friend class CResourceLoader;
        friend void CModuleFile_ArchiveForEach(IDirectoryTraverser::IIterator *, void *);
        friend void CModuleFile_ArchiveForEachNoErrors(IDirectoryTraverser::IIterator *, void *);

      private:
        signed long m_iNumber;
        std::string m_sName;
        CSgaFile *m_pHandle; //!< This is a "resource"
    };

    size_t GetArchiveCount();
    CArchiveHandler *GetArchive(size_t iId);

    //! Puts the full file path of an archive into a user supplied buffer
    /*!
        \param sOutput If non-NULL, the file path will be placed into this buffer
        \return The length of the file path, in characters (including the terminating 0)
    */
    size_t GetArchiveFullPath(size_t iId, char *sOutput);

    class RAINMAN_API CRequiredHandler
    {
      public:
        signed long GetNumber() const;
        const char *GetFileName() const;
        const CModuleFile *GetModHandle() const;
        CModuleFile *GetModHandle();

        CRequiredHandler();
        ~CRequiredHandler();

        friend class CModuleFile;
        friend class CResourceLoader;

      private:
        signed long m_iNumber;
        std::string m_sName;
        CModuleFile *m_pHandle; //!< This is a "resource"
    };

    size_t GetRequiredCount() const;
    CRequiredHandler *GetRequired(size_t iId);
    const CRequiredHandler *GetRequired(size_t iId) const;

    class RAINMAN_API CCompatibleHandler
    {
      public:
        signed long GetNumber() const;
        const char *GetFileName() const;

        CCompatibleHandler();
        ~CCompatibleHandler();

        friend class CModuleFile;

      private:
        signed long m_iNumber;
        std::string m_sName;
    };

    size_t GetCompatibleCount();
    CCompatibleHandler *GetCompatible(size_t iId);

    class RAINMAN_API CCohDataSource
    {
      public:
        const char *GetToc() const;
        void SetToc(const char *sToc);

        const char *GetOption() const;
        void SetOption(const char *sOption);

        signed long GetNumber() const;
        void SetNumber(signed long iNumber);

        const char *GetFolder() const;
        void SetFolder(const char *sFolder);

        size_t GetArchiveCount();
        CArchiveHandler *GetArchive(size_t iId);

        bool IsLoaded() const;

        inline bool IsFolderWritable() const { return m_bCanWriteToFolder; }

        CCohDataSource();
        ~CCohDataSource();

        friend class CModuleFile;
        friend class CResourceLoader;

      private:
        signed long m_iNumber;
        std::string m_sToc;
        std::string m_sOption;
        std::string m_sFolder;
        bool m_bIsLoaded;
        bool m_bCanWriteToFolder;
        std::vector<std::unique_ptr<CArchiveHandler>> m_vArchives;
    };

    size_t GetDataSourceCount();
    CCohDataSource *GetDataSource(size_t iId);

    friend void CModuleFile_UcsForEach(IDirectoryTraverser::IIterator *, void *);
    class RAINMAN_API CUcsHandler //!< This is a "resource"
    {
      public:
        const char *GetFileName() const;
        std::shared_ptr<CUcsFile> GetUcsHandle();
        std::shared_ptr<const CUcsFile> GetUcsHandle() const;

        CUcsHandler();
        ~CUcsHandler();

        friend class CModuleFile;
        friend class CResourceLoader;
        friend void CModuleFile_UcsForEach(IDirectoryTraverser::IIterator *, void *);

      private:
        std::string m_sName;                 //!< This is a "resource" (because CUcsHandler is)
        std::shared_ptr<CUcsFile> m_pHandle; //!< This is a "resource" (because CUcsHandler is)
    };

    void NewUCS(const char *sName, std::shared_ptr<CUcsFile> pUcs = nullptr);
    const wchar_t *ResolveUCS(const char *sDollarString);
    const wchar_t *ResolveUCS(const wchar_t *sDollarString);
    /*!
        \return Returns a valid pointer, or 0 if the key could not be found.
    */
    const wchar_t *ResolveUCS(unsigned long iStringID);
    size_t GetUcsCount() const;
    CUcsHandler *GetUcs(size_t iId);
    const CUcsHandler *GetUcs(size_t iId) const;
    void DeleteUcs(size_t iId);

    size_t GetEngineCount() const;
    CModuleFile *GetEngine(size_t iId);
    const CModuleFile *GetEngine(size_t iId) const;

    long GetSgaOutputVersion();

    inline CFileMap *GetFileMap() { return m_pNewFileMap; }
    inline CFileSystemStore *GetFileSystem() { return m_pFSS; }
    inline bool IsFauxModule() { return m_bIsFauxModule; }

  protected:
    // Functions
    void _Clean();
    void _CleanResources();
    /*!
        Will duplicate a string, replacing the following: <br>
        %LOCALE% -> Locale\{sLocale} <br>
        %TEXTURE-LEVEL% -> Full <br>
        %SOUND-LEVEL% -> Full <br>
        %MODEL-LEVEL% -> High <br>
        If sAlsoAppend is specified then that will be appended to the result.
        \return Returns a valid string, or 0 on error
    */
    std::string _DawnOfWarRemoveDynamics(const char *sStr, const char *sAlsoAppend = nullptr);

    // Module settings
    eModuleType m_eModuleType;
    std::string m_sLocale; // This is NOT cleaned by _Clean()
    std::string m_sApplicationPath;
    std::string m_sGameInstallPath; // Cleared by _Clean(); child modules inherit this during resource loading.
    std::string m_sFilename;
    std::string m_sFileMapName;
    CModuleFile *m_pParentModule;
    unsigned short int m_iFileMapModNumber;
    std::wstring m_sScenarioPackRootFolder;
    std::string m_saScenarioPackRootFolder;
    bool m_bIsFauxModule;

    // Parsed directive fields from .module file
    CModuleMetadata m_metadata;

    // Object collections for MT_DawnOfWar & MT_CompanyOfHeroesEarly
    std::vector<std::unique_ptr<CFolderHandler>> m_vFolders;
    std::vector<std::unique_ptr<CArchiveHandler>> m_vArchives;

    // Object collections for MT_DawnOfWar
    std::vector<std::unique_ptr<CRequiredHandler>> m_vRequireds;
    std::vector<std::unique_ptr<CCompatibleHandler>> m_vCompatibles;

    // Object collections for MT_CompanyOfHeroes
    std::vector<std::unique_ptr<CCohDataSource>> m_vDataSources;

    // Object collections for all
    std::vector<std::unique_ptr<CModuleFile>> m_vEngines;     //!< This is a "resource"
    std::vector<std::unique_ptr<CUcsHandler>> m_vLocaleTexts; //!< This is a "resource"

    // Internal usefuls
    CFileSystemStore *m_pFSS;
    std::string m_sCohThisModFolder;

    // Resource holders
    CFileMap *m_pNewFileMap;
};
