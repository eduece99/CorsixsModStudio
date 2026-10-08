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
#include "rainman/io/IDirectoryTraverser.h"
#include "rainman/core/Api.h"
#include "rainman/io/IFileStore.h"

class CSgaFile;

#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class RAINMAN_API CFileMap : public IFileStore, public IDirectoryTraverser
{
  public:
    CFileMap();
    ~CFileMap();

    typedef IFileStore::IOutputStream *(*tAuxOutputSupply)(const char *, bool, void *);
    void SetAuxOutputSupply(tAuxOutputSupply fAuxOutputSupply, void *pContext);

    /*!
        Registers a files source with the map.
        \param[in] iModNum 0 = this mod, 15000 +/- val = req. mod (eg. RequiredMod.4 = 15004), 30000 and above = engine
        \param[in] bIsSga True if the source is an archive, false otherwise
        \param[in] iSourceNum eg. Archive.05 = 5
        \param[in] sModName eg. "RelicCOH" or "WXP"
        \param[in] sSourceName eg. "WW2\Archives\Attrib.sga" or "Data"
        \param[in] pStore Filestore for this source
        \param[in] pTraverser Traversing interface for pStore
        \param[in] bIsWritable If true, files in this source which are edited are saved into the same location
        \param[in] bIsOutput If true, new files can be saved into this source

        \return Returns a value which can be passed to MapSga() and MapIterator(). Will throw a CRainmanException on
       error.
    */
    void *RegisterSource(unsigned short iModNum, bool bIsSga, unsigned short iSourceNum, const char *sModName,
                         const char *sSourceName, IFileStore *pStore, IDirectoryTraverser *pTraverser, bool bIsWritable,
                         bool bIsOutput, bool bIsPureOutput = false);

    void EraseSource(void *pSource);

    void RewriteToC(const char *sOld, const char *sNew);
    void UnrewriteToC(const char *sOld);

    /*!
        Maps an SGA archive into the file map.
        \param[in] pSource Files source registered with RegisterSource()
        \param[in] pSga SGA archive to map in

        \return Returns no value but will throw a CRainmanException on error.
    */
    void MapSga(void *pSource, CSgaFile *pSga);

    /*!
        Maps a directory iterator into the file map as a ToC root folder
        \param[in] pSource Files source registered with RegisterSource()
        \param[in] sTocName Name of ToC to add this folder to
        \param[in] pItr Directory to map in

        \return Returns no value but will throw a CRainmanException on error.
    */
    void MapIterator(void *pSource, const char *sTocName, IDirectoryTraverser::IIterator *pItr);

    /*!
        Maps a directory iterator into the file map
        \param[in] pSource Files source registered with RegisterSource()
        \param[in] sPath Virtual path to map into
        \param[in] pItr Directory to map in

        \return Returns no value but will throw a CRainmanException on error.
    */
    void MapIteratorDeep(void *pSource, const char *sPath, IDirectoryTraverser::IIterator *pItr);

    /*!
        Maps a single file into the file map
        \param[in] pSource Files source registered with RegisterSource()
        \param[in] sTocName Name of ToC to add this file to
        \param[in] sFullPath The full path of the file that can be used to open the file using the file source
        \param[in] sPathPartial A position in the full path where the mapped path starts

        \return Returns no value but will throw a CRainmanException on error.
    */
    void MapSingleFile(void *pSource, const char *sTocName, const char *sFullPath, const char *sPathPartial);

    virtual unsigned long VGetEntryPointCount();
    virtual const char *VGetEntryPoint(unsigned long iID);
    //! Returns filesystem-backed source directories for a virtual folder path.
    std::vector<std::string> GetSourceFolderPaths(const char *sPath);

    // IFileStore Interface
    //! Initialize the IFileStore interface
    /*!
        \param[in] pUnused Unused
    */
    virtual void VInit(void *pUnused = 0);

    //! Open a readable stream
    virtual IStream *VOpenStream(const char *sFile);

    virtual IOutputStream *VOpenOutputStream(const char *sFile, bool bEraseIfPresent);

    //! Get the last modification date of a file
    virtual tLastWriteTime VGetLastWriteTime(const char *sPath);

    virtual void VCreateFolderIn(const char *sPath, const char *sNewFolderName);

    virtual IIterator *VIterate(const char *sPath);

  protected:
    struct _DataSource
    {
        //! 32 bit unsigned int arranged as so: <br/>
        //! Top 15 bits are the mod: 0 = this mod, 15000 +/- val = req. mod (eg. RequiredMod.4 = 15004), 30000 and above
        //! = engine <br/> Next 1 bit is the source type: 0 = folder, 1 = SGA <br/> Bottom 16 bits are the source number
        //! (eg. Archive.05 = 5) <br/> The lower the value, the higher priority the data source is
        unsigned long iSortNumber;

        inline unsigned short int GetMod() const { return (unsigned short int)(iSortNumber >> 17); }
        inline unsigned short int GetSourceType() const { return (unsigned short int)((iSortNumber >> 16) & 1); }
        inline unsigned short int GetSourceNumber() const { return (unsigned short int)(iSortNumber & 0xffff); }

        inline void SetMod(unsigned short int iN)
        {
            iSortNumber |= 0xfffe0000;
            iSortNumber &= (((iN & 0x7fff) << 17) | 0x1ffff);
        }
        inline void SetSourceType(unsigned short int iN)
        {
            iSortNumber |= 0x10000;
            iSortNumber &= (((iN & 1) << 16) | 0xfffeffff);
        }
        inline void SetSourceNumber(unsigned short int iN)
        {
            iSortNumber |= 0xffff;
            iSortNumber &= (0xffff0000 | iN);
        }

        std::string sModName;    //!< eg. "RelicCOH" or "WXP"
        std::string sSourceName; //!< eg. "WW2\Archives\Attrib.sga" or "Data"

        IFileStore *pStore;
        IDirectoryTraverser *pTraverser;

        bool bIsWritable;      //!< If true, files in this source which are edited are saved into the same location
        bool bIsDefaultOutput; //!< If true, new files can be saved into this source
        bool bIsPureOutput;

        inline bool operator()(_DataSource *a, _DataSource *b) const { return a->iSortNumber < b->iSortNumber; }
    };

    struct _Folder;

    struct _File
    {
        std::string sName; //!< Short name of this file
        _Folder *pParent;  //!< Parent of this file

        //! Sorted list of sources which have this file, also stores the last write time given by that source
        //! First item in this map (eg .begin() ) is the highest priority source
        std::map<_DataSource *, tLastWriteTime, _DataSource> mapSources;
    };

    struct _Folder
    {
        _Folder *pParent;      //!< Parent of this folder (set to 0 on ToC Root folders)
        std::string sName;     //!< Short name of this folder
        std::string sFullName; //!< Full name of this folder

        std::map<_DataSource *, std::string> mapSourceNames; //!< maps DataSources to their names for this folder

        std::vector<std::unique_ptr<_Folder>> vChildFolders;
        std::vector<std::unique_ptr<_File>> vChildFiles;
    };

    struct _TOC
    {
        std::string sName; //!< Name of this ToC
        std::unique_ptr<_Folder> pRootFolder;
    };

    std::map<_TOC *, _TOC *> m_mapTOCRewrites;

    std::vector<std::unique_ptr<_TOC>> m_vTOCs;
    std::vector<std::unique_ptr<_DataSource>> m_vDataSources;

    /*!
        Makes sure that pFolder has a data source suitable for saving pFile.
        \param[in] pFolder
        \param[in] pFile
        \return Returns the data source that is suitable, or throws a CRainmanException if it cannot.
    */
    _DataSource *_MakeFolderWritable(_Folder *pFolder, _File *pFile);

    void _RawMap(_DataSource *pSource, IDirectoryTraverser::IIterator *pItr, _Folder *pFolder);
    void _Clean();

  public:
    //! In-memory representation of a directory entry for parallel pre-scanning
    struct DirEntry
    {
        std::string name;
        std::string directoryPath; // for folders: the path the iterator reports
        bool isFile;
        tLastWriteTime lastWriteTime;   // only meaningful for files
        std::vector<DirEntry> children; // only meaningful for directories
    };

    /*!
        Maps a pre-scanned directory snapshot into the file map as a ToC root folder.
        This is the parallel-friendly alternative to MapIterator(): the directory tree
        has already been walked into memory (no I/O), so the lock is held only briefly.
        \param[in] pSource Files source registered with RegisterSource()
        \param[in] sTocName Name of ToC to add this folder to
        \param[in] snapshot The pre-scanned directory tree
    */
    void MapSnapshot(void *pSource, const char *sTocName, const DirEntry &snapshot);

  protected:
    void _RawMapFromSnapshot(_DataSource *pSource, const DirEntry &snapshot, _Folder *pFolder);
    void _EraseSourceFromFolder(_DataSource *pSource, _Folder *pFolder);
    void _FolderSetupSourceNameFromSingleFileMap(_Folder *pFolder, _DataSource *pDataSource, const char *sPathFull,
                                                 const char *sPathPartLeft);

    /*!
        Finds the file of the given name in the files tree, and also returns the folder that it is in.

        \param[in] sName Full name of a file to find
        \param[out] ppFolder If non-zero, will store the folder the file is in here
        \return Returns the file, or if no file is found then will return 0, but will still return the folder (will
       create the folder if necessary). Will throw a CRainmanException in extreme cases.
    */
    _File *_FindFile(const char *sName, _Folder **ppFolder, bool bIsFolder = false);

    /*!
        Opens the specified file as an input stream. Designed to have the return values from _FileFile() passed
        right to it, eg. gives suitable error messages if the return values from _FindFile() are sent to this
        without any checking.

        \param[in] pFile The identifier of the file to open
        \param[in] pFolder The folder the file is in (if is 0 and pFile isn't then this value will be auto-set)
        \param[in] sName Full name of the file (only used in error messages)
        \return Returns the file as an input stream, or throws a CRainmanException.
    */
    IFileStore::IStream *_OpenFile(CFileMap::_File *pFile, CFileMap::_Folder *pFolder, const char *sFile);

    static bool _SortFolds(const std::unique_ptr<_Folder> &a, const std::unique_ptr<_Folder> &b);
    static bool _SortFiles(const std::unique_ptr<_File> &a, const std::unique_ptr<_File> &b);

    std::recursive_mutex m_mtxMap; //!< Guards mutation of m_vDataSources, m_vTOCs, and the folder/file tree
    tAuxOutputSupply m_fAuxOutputSupply;
    void *m_pAuxOutputContext;

  public:
    class CIterator : public IDirectoryTraverser::IIterator
    {
      protected:
        friend class CFileMap;
        CIterator(CFileMap::_Folder *pFolder, CFileMap *pFileMap);

        char *m_sParentPath;
        char *m_sFullPath;
        CFileMap *m_pFileMap;

        enum E_ItrWhat
        {
            IW_Folders,
            IW_Files,
            IW_None
        };
        E_ItrWhat m_eWhat;

        CFileMap::_Folder *m_pDirectory;

        std::vector<std::unique_ptr<CFileMap::_Folder>>::iterator m_FoldIter;
        std::vector<std::unique_ptr<CFileMap::_File>>::iterator m_FileIter;

        void _MakeFullName();

      public:
        ~CIterator();

        virtual IDirectoryTraverser::IIterator::eTypes VGetType();
        virtual IDirectoryTraverser::IIterator *VOpenSubDir();
        virtual IFileStore::IStream *VOpenFile();
        virtual const char *VGetName();
        virtual const char *VGetFullPath();
        virtual const char *VGetDirectoryPath();
        virtual IDirectoryTraverser::IIterator::eErrors VNextItem();
        virtual tLastWriteTime VGetLastWriteTime();

        /*!
            Files:<br>
            iTag of 0 = Mod Name (const char*)<br>
            iTag of 1 = Source Name (const char*)<br>
            iTag of 2 = Mod code (0 = this, 30000 and above = engine, etc.) (unsigned long)<br>
            iTag of 3 = Is SGA? (1 / 0)<br>
            Returns 0 for everything else.
        */
        virtual void *VGetTag(long iTag);
    };
};
