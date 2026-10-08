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

#include "rainman/io/CFileSystemStore.h"
#include "rainman/core/WideFile.h"
#ifndef RAINMAN_GNUC
#include <direct.h>
#endif
#include <algorithm>
#include <cstring>
#include <cerrno>
#ifdef RAINMAN_GNUC
#include <sys/stat.h>
#endif
#include "rainman/core/Exception.h"
#include "rainman/core/Internal_Util.h"
#include "rainman/core/memdebug.h"

#ifdef RAINMAN_GNUC
namespace
{
std::string ToNativePath(const char *sPath)
{
    std::string sNativePath(sPath);
    std::replace(sNativePath.begin(), sNativePath.end(), '\\', '/');
    return sNativePath;
}

std::wstring ToNativePath(const wchar_t *sPath)
{
    std::wstring sNativePath(sPath);
    std::replace(sNativePath.begin(), sNativePath.end(), L'\\', L'/');
    return sNativePath;
}
} // namespace
#endif

CFileSystemStore::CFileSystemStore()
{
#ifdef EXTEND_FILESTORE_WITH_TRAVERSE
    for (bool &i : m_bDrivePresent)
        i = false;
#endif
}

CFileSystemStore::~CFileSystemStore() {}

CFileSystemStore::CStream::CStream()
{
    m_fFile = nullptr;
#ifdef _DEBUG
    m_sFile = nullptr;
#endif
}

CFileSystemStore::CStream::~CStream()
{
    if (m_fFile)
        fclose(m_fFile);
#ifdef _DEBUG
    if (m_sFile)
        delete[] m_sFile;
#endif
}

void CFileSystemStore::VInit(void *pUnused)
{
    m_bInited = true;
    RAINMAN_LOG_INFO("CFileSystemStore initialised");
}

IFileStore::IStream *CFileSystemStore::VOpenStream(const char *sFile)
{
    RAINMAN_LOG_DEBUG("CFileSystemStore::VOpenStream(\"{}\")", sFile ? sFile : "(null)");
#ifdef RAINMAN_GNUC
    const std::string sNativePath = ToNativePath(sFile);
    sFile = sNativePath.c_str();
#endif
    FILE *fFile = fopen(sFile, "rb");
    if (fFile == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not open \'%s\': %s (errno %d)", sFile,
                                strerror(errno), errno);
    }

    auto *pStream = new CStream();
    if (pStream == nullptr)
    {
        fclose(fFile);
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }

    pStream->m_fFile = fFile;
#ifdef _DEBUG
    pStream->m_sFile = new char[strlen(sFile) + 1];
    strcpy(pStream->m_sFile, sFile);
#endif

    return pStream;
}

IFileStore::IStream *CFileSystemStore::OpenStreamW(const wchar_t *sFile)
{
#ifdef RAINMAN_GNUC
    const std::wstring sNativePath = ToNativePath(sFile);
    sFile = sNativePath.c_str();
#endif
    FILE *fFile = RainmanFOpen(sFile, L"rb");
    if (fFile == nullptr)
    {
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not open \'%S\'", sFile);
    }

    auto *pStream = new CStream();
    if (pStream == nullptr)
    {
        fclose(fFile);
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }

    pStream->m_fFile = fFile;

    return pStream;
}

#ifdef RAINMAN_MSVC
bool TryMakeDirectory(char *sPath)
{
    bool bReturn = false;
    char *sSep = strrchr(sPath, '\\');
    if (sSep && strlen(sPath) > 3)
    {
        *sSep = 0;
        DWORD ret = GetFileAttributesA(sPath);
        bool bIsDir = (ret != (DWORD)-1) && (ret & FILE_ATTRIBUTE_DIRECTORY);
        if (!bIsDir)
        {
            TryMakeDirectory(sPath);
            if (_mkdir(sPath) == 0)
                bReturn = true;
        }
        *sSep = '\\';
    }
    return bReturn;
}

bool TryMakeDirectoryW(wchar_t *sPath)
{
    bool bReturn = false;
    wchar_t *sSep = wcsrchr(sPath, '\\');
    if (sSep && wcslen(sPath) > 3)
    {
        *sSep = 0;
        DWORD ret = GetFileAttributesW(sPath);
        bool bIsDir = (ret != (DWORD)-1) && (ret & FILE_ATTRIBUTE_DIRECTORY);
        if (!bIsDir)
        {
            TryMakeDirectoryW(sPath);
            if (_wmkdir(sPath) == 0)
                bReturn = true;
        }
        *sSep = '\\';
    }
    return bReturn;
}
#endif

IFileStore::IOutputStream *CFileSystemStore::OpenOutputStreamW(const wchar_t *sFile, bool bEraseIfPresent)
{
#ifdef RAINMAN_GNUC
    const std::wstring sNativePath = ToNativePath(sFile);
    sFile = sNativePath.c_str();
#endif
    FILE *fFile = nullptr;
    if (!bEraseIfPresent)
        fFile = RainmanFOpen(sFile, L"r+b");
    if (fFile == nullptr)
        fFile = RainmanFOpen(sFile, L"w+b");
    if (fFile == nullptr)
    {
        // Maybe folder doesn't exist?
#ifdef RAINMAN_MSVC
        wchar_t *sFile2 = wcsdup(sFile);
        if (TryMakeDirectoryW(sFile2))
        {
            if (!bEraseIfPresent)
                fFile = RainmanFOpen(sFile, L"r+b");
            if (fFile == nullptr)
                fFile = RainmanFOpen(sFile, L"w+b");
        }
        free(sFile2);
#endif
    }
    if (fFile == nullptr)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not open \'%S\'", sFile);
    auto *pStream = new COutputStream();
    if (pStream == nullptr)
    {
        fclose(fFile);
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }

    pStream->m_fFile = fFile;
    return pStream;
}

IFileStore::IOutputStream *CFileSystemStore::VOpenOutputStream(const char *sFile, bool bEraseIfPresent)
{
    RAINMAN_LOG_DEBUG("CFileSystemStore::VOpenOutputStream(\"{}\")", sFile ? sFile : "(null)");
#ifdef RAINMAN_GNUC
    const std::string sNativePath = ToNativePath(sFile);
    sFile = sNativePath.c_str();
#endif
    FILE *fFile = nullptr;
    if (!bEraseIfPresent)
        fFile = fopen(sFile, "r+b");
    if (fFile == nullptr)
        fFile = fopen(sFile, "w+b");
    if (fFile == nullptr)
    {
        // Maybe folder doesn't exist?
#ifdef RAINMAN_MSVC
        char *sFile2 = strdup(sFile);
        if (TryMakeDirectory(sFile2))
        {
            if (!bEraseIfPresent)
                fFile = fopen(sFile, "r+b");
            if (fFile == nullptr)
                fFile = fopen(sFile, "w+b");
        }
        free(sFile2); // NOLINT(clang-analyzer-unix.MismatchedDeallocator) — strdup uses malloc; free() is correct
#endif
    }
    if (fFile == nullptr)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not open \'%s\' for writing", sFile);
    auto *pStream = new COutputStream();
    if (pStream == nullptr)
    {
        fclose(fFile);
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }

    pStream->m_fFile = fFile;
    return pStream;
}

CFileSystemStore::COutputStream::COutputStream() { m_fFile = nullptr; }

CFileSystemStore::COutputStream::~COutputStream()
{
    if (m_fFile)
        fclose(m_fFile);
}

void CFileSystemStore::COutputStream::VWrite(unsigned long iItemCount, unsigned long iItemSize, const void *pSource)
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");

    long iOldPos = ftell(m_fFile);
    size_t iItemsWritten = fwrite(pSource, iItemSize, iItemCount, m_fFile);

    if (iItemsWritten == iItemCount)
    {
        // return true;
    }
    else
    {
        fseek(m_fFile, iOldPos, SEEK_SET);
        throw CRainmanException(__FILE__, __LINE__, "Failed to write data");
    }
}

void CFileSystemStore::COutputStream::VRead(unsigned long iItemCount, unsigned long iItemSize, void *pDestination)
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");
    unsigned long iByteCount = iItemCount * iItemSize;

    auto *pTempDest = new unsigned char[iByteCount];
    if (pTempDest == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");

    long iOldPos = ftell(m_fFile);
    size_t iItemsRead = fread(pTempDest, iItemSize, iItemCount, m_fFile);

    if (iItemsRead == iItemCount)
    {
        memcpy(pDestination, pTempDest, iByteCount);
        delete[] pTempDest;
    }
    else
    {
        fseek(m_fFile, iOldPos, SEEK_SET);
        delete[] pTempDest;
        throw CRainmanException(__FILE__, __LINE__, "Failed to read data");
    }
}

void CFileSystemStore::COutputStream::VSeek(long iPosition, IFileStore::IOutputStream::SeekLocation SeekFrom)
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");

    switch (SeekFrom)
    {
    case IFileStore::IStream::SL_Current:
        if (fseek(m_fFile, iPosition, SEEK_CUR) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    case IFileStore::IStream::SL_End:
        if (fseek(m_fFile, iPosition, SEEK_END) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    case IFileStore::IStream::SL_Root:
        if (fseek(m_fFile, iPosition, SEEK_SET) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    default:
        throw CRainmanException(__FILE__, __LINE__, "Invalid SeekFrom argument");
    };
}

long CFileSystemStore::COutputStream::VTell()
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");
    return ftell(m_fFile);
}

void CFileSystemStore::CStream::VRead(unsigned long iItemCount, unsigned long iItemSize, void *pDestination)
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");

    long iOldPos = ftell(m_fFile);
    size_t iItemsRead = fread(pDestination, iItemSize, iItemCount, m_fFile);

    if (iItemsRead != iItemCount)
    {
        fseek(m_fFile, iOldPos, SEEK_SET);
        throw CRainmanException(nullptr, __FILE__, __LINE__,
                                "Failed to read items: got %zu of %lu items (%lu bytes each) at file offset %ld",
                                iItemsRead, iItemCount, iItemSize, iOldPos);
    }
}

void CFileSystemStore::CStream::VSeek(long iPosition, IFileStore::IStream::SeekLocation SeekFrom)
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");

    switch (SeekFrom)
    {
    case IFileStore::IStream::SL_Current:
        if (fseek(m_fFile, iPosition, SEEK_CUR) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    case IFileStore::IStream::SL_End:
        if (fseek(m_fFile, iPosition, SEEK_END) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    case IFileStore::IStream::SL_Root:
        if (fseek(m_fFile, iPosition, SEEK_SET) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek failed");
        break;

    default:
        throw CRainmanException(__FILE__, __LINE__, "Invalid SeekFrom argument");
    };
}

long CFileSystemStore::CStream::VTell()
{
    if (m_fFile == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "No file associated with stream");
    return ftell(m_fFile);
}

static char *mystrdup(const char *sStr)
{
    char *s = new char[strlen(sStr) + 1];
    if (s == nullptr)
        return nullptr;
    strcpy(s, sStr);
    return s;
}

static wchar_t *mystrdup(const wchar_t *sStr)
{
    auto *s = new wchar_t[wcslen(sStr) + 1];
    if (s == nullptr)
        return nullptr;
    wcscpy(s, sStr);
    return s;
}

#ifdef EXTEND_FILESTORE_WITH_TRAVERSE
tLastWriteTime CFileSystemStore::VGetLastWriteTime(const char *sPath)
{
#ifdef RAINMAN_GNUC
    const std::string sNativePath = ToNativePath(sPath);
    return GetLastWriteTime(sNativePath.c_str());
#else
    return GetLastWriteTime(sPath);
#endif
}

void CFileSystemStore::VCreateFolderIn(const char *sPath, const char *sNewFolderName)
{
#ifdef RAINMAN_GNUC
    const std::string sNativePath = ToNativePath(sPath);
    const std::string sNativeFolderName = ToNativePath(sNewFolderName);
    sPath = sNativePath.c_str();
    sNewFolderName = sNativeFolderName.c_str();
#endif
    char *sNewPath = new char[strlen(sPath) + strlen(sNewFolderName) + 2];
    strcpy(sNewPath, sPath);
#ifdef RAINMAN_GNUC
    strcat(sNewPath, "/");
#else
    strcat(sNewPath, "\\");
#endif
    strcat(sNewPath, sNewFolderName);
    bool bRet = (_mkdir(sNewPath) == 0);
    if (!bRet)
    {
#ifdef WIN32
        if (errno == EEXIST)
        {
            if (PathIsDirectory(sNewPath))
            {
                delete[] sNewPath;
                return;
            }
        }
#endif
        delete[] sNewPath;
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not create \'%s\'", sPath);
    }
    delete[] sNewPath;
}

CFileSystemStore::CIteratorW::CIteratorW(const wchar_t *sFolder, const CFileSystemStore *pStore)
{
#ifdef RAINMAN_GNUC
    m_pDirectory = nullptr;
    m_pDirEnt = nullptr;
    const std::wstring sNativePath = ToNativePath(sFolder);
    m_wParentPath = mystrdup(sNativePath.c_str());
    m_pStore = pStore;
    m_wFullPath = nullptr;
    m_sParentPath = nullptr;
    m_sFullPath = nullptr;
    m_sFileName = nullptr;

    try
    {
        const std::string sUtf8Folder = RainmanWideToUtf8(sNativePath.c_str());
        m_pDirectory = opendir(sUtf8Folder.c_str());
    }
    catch (const std::range_error &)
    {
        delete[] m_wParentPath;
        throw CRainmanException(__FILE__, __LINE__, "Could not convert directory path to UTF-8");
    }

    if (m_pDirectory == nullptr)
    {
        delete[] m_wParentPath;
        throw CRainmanException(nullptr, __FILE__, __LINE__, "opendir failed (%s)", strerror(errno));
    }

    while ((m_pDirEnt = readdir(m_pDirectory)) != nullptr)
    {
        if (strcmp(m_pDirEnt->d_name, ".") != 0 && strcmp(m_pDirEnt->d_name, "..") != 0)
            break;
    }

    if (m_pDirEnt != nullptr)
    {
        try
        {
            m_wFileName = RainmanUtf8ToWide(m_pDirEnt->d_name);
        }
        catch (const std::range_error &)
        {
            closedir(m_pDirectory);
            m_pDirectory = nullptr;
            delete[] m_wParentPath;
            throw CRainmanException(__FILE__, __LINE__, "Could not convert directory entry name from UTF-8");
        }
        RebuildFullPath(m_wFileName.c_str());
    }
#else
    m_HandFD = nullptr;
    m_wParentPath = mystrdup(sFolder);
    m_wFullPath = nullptr;
    m_pStore = pStore;
    m_sParentPath = nullptr;
    m_sFullPath = nullptr;
    m_sFileName = nullptr;

    wchar_t *sTmp;
    sTmp = new wchar_t[wcslen(sFolder) + 3];
    if (sTmp == nullptr)
    {
        delete[] m_wParentPath;
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }
    wcscpy(sTmp, sFolder);
    wcscat(sTmp, L"\\*");

    m_HandFD = FindFirstFileW(sTmp, &m_W32FD);
    delete[] sTmp;
    if (m_HandFD == INVALID_HANDLE_VALUE)
    {
        m_HandFD = nullptr;
        delete[] m_wParentPath;
        throw CRainmanException(nullptr, __FILE__, __LINE__, "FindFirstFile failed (%S)", sFolder);
    }
    while (wcscmp(m_W32FD.cFileName, L".") == 0 || wcscmp(m_W32FD.cFileName, L"..") == 0)
    {
        if (!FindNextFileW(m_HandFD, &m_W32FD))
        {
            FindClose(m_HandFD);
            m_HandFD = nullptr;
            return; // no exception; an empty folder is not an error
        }
    }

    m_wFullPath = new wchar_t[wcslen(m_wParentPath) + wcslen(m_W32FD.cFileName) + 2];
    if (m_wFullPath == nullptr)
    {
        delete[] m_wParentPath;
        FindClose(m_HandFD);
        m_HandFD = nullptr;
        throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
    }
    wcscpy(m_wFullPath, m_wParentPath);
    wcscat(m_wFullPath, L"\\");
    wcscat(m_wFullPath, m_W32FD.cFileName);
#endif
}

CFileSystemStore::CIterator::CIterator(const char *sFolder, const CFileSystemStore *pStore)
{
#ifdef RAINMAN_GNUC

    m_pDirectory = 0;
    m_pDirEnt = 0;
    const std::string sNativePath = ToNativePath(sFolder);
    m_sParentPath = new char[sNativePath.size() + 2];
    strcpy(m_sParentPath, sNativePath.c_str());
    const size_t iPathLength = sNativePath.size();
    if (iPathLength > 0 && m_sParentPath[iPathLength - 1] != '/')
    {
        m_sParentPath[iPathLength] = '/';
        m_sParentPath[iPathLength + 1] = '\0';
    }
    m_iParentPathLen = strlen(m_sParentPath);
    m_pStore = pStore;

    if ((m_pDirectory = opendir(m_sParentPath)) == 0)
    {
        const int iError = errno;
        const std::string sFailedPath(m_sParentPath);
        delete[] m_sParentPath;
        throw CRainmanException(nullptr, __FILE__, __LINE__, "opendir failed on '%s': %s", sFailedPath.c_str(),
                                strerror(iError));
    }

    while ((m_pDirEnt = readdir(m_pDirectory)) != 0)
    {
        if ((!strcmp(m_pDirEnt->d_name, ".")) || (!strcmp(m_pDirEnt->d_name, "..")))
        {
            continue;
        }
        break;
    }

    if (m_pDirEnt != 0)
    {
        RebuildFullPath(m_pDirEnt->d_name);
    }

#else
    m_HandFD = nullptr;
    m_sParentPath = mystrdup(sFolder);
    m_iParentPathLen = strlen(m_sParentPath);
    m_pStore = pStore;

    // Pre-reserve capacity for full path: parent + '\' + MAX_PATH filename
    m_sFullPathBuf.reserve(m_iParentPathLen + 2 + MAX_PATH);

    std::string sPattern(sFolder);
    sPattern += "\\*";

    m_HandFD = FindFirstFileExA(sPattern.c_str(), FindExInfoBasic, &m_W32FD, FindExSearchNameMatch, nullptr,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (m_HandFD == INVALID_HANDLE_VALUE)
    {
        DWORD dwErr = GetLastError();
        m_HandFD = nullptr;
        delete[] m_sParentPath;
        m_sParentPath = nullptr;
        throw CRainmanException(nullptr, __FILE__, __LINE__, "FindFirstFile failed on '%s' (Windows error %lu)",
                                sFolder, dwErr);
    }
    while (strcmp(m_W32FD.cFileName, ".") == 0 || strcmp(m_W32FD.cFileName, "..") == 0)
    {
        if (!FindNextFileA(m_HandFD, &m_W32FD))
        {
            FindClose(m_HandFD);
            m_HandFD = nullptr;
            return; // no exception; an empty folder is not an error
        }
    }

    RebuildFullPath(m_W32FD.cFileName);
#endif
}

CFileSystemStore::CIteratorW::~CIteratorW()
{
    delete[] m_sParentPath;
    delete[] m_sFullPath;
    delete[] m_sFileName;
    delete[] m_wParentPath;
    delete[] m_wFullPath;
#ifdef RAINMAN_GNUC
    if (m_pDirectory)
        closedir(m_pDirectory);
#else
    if (m_HandFD)
        FindClose(m_HandFD);
#endif
}

CFileSystemStore::CIterator::~CIterator()
{
    delete[] m_sParentPath;
#ifdef RAINMAN_GNUC
    if (m_pDirectory)
        closedir(m_pDirectory);
#else
    if (m_HandFD)
        FindClose(m_HandFD);
#endif
}

IDirectoryTraverser::IIterator::eTypes CFileSystemStore::CIteratorW::VGetType()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == nullptr)
        return IDirectoryTraverser::IIterator::T_Nothing;
    struct stat sFileInfo
    {
    };
    std::string sUtf8Path;
    try
    {
        sUtf8Path = RainmanWideToUtf8(m_wFullPath);
    }
    catch (const std::range_error &)
    {
        throw CRainmanException(__FILE__, __LINE__, "Could not convert file path to UTF-8");
    }
    if (stat(sUtf8Path.c_str(), &sFileInfo) != 0)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Could not inspect '%s': %s", sUtf8Path.c_str(),
                                strerror(errno));
    return S_ISDIR(sFileInfo.st_mode) ? IDirectoryTraverser::IIterator::T_Directory
                                      : IDirectoryTraverser::IIterator::T_File;
#else
    if (m_HandFD == nullptr)
        return IDirectoryTraverser::IIterator::T_Nothing;
    return (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? IDirectoryTraverser::IIterator::T_Directory
                                                                : IDirectoryTraverser::IIterator::T_File);
#endif
}

IDirectoryTraverser::IIterator::eTypes CFileSystemStore::CIterator::VGetType()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0)
        return IDirectoryTraverser::IIterator::T_Nothing;
    return (m_pDirEnt->d_type == DT_DIR) ? IDirectoryTraverser::IIterator::T_Directory
                                         : IDirectoryTraverser::IIterator::T_File;
#else
    if (m_HandFD == nullptr)
        return IDirectoryTraverser::IIterator::T_Nothing;
    return (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? IDirectoryTraverser::IIterator::T_Directory
                                                                : IDirectoryTraverser::IIterator::T_File);
#endif
}

IDirectoryTraverser::IIterator *CFileSystemStore::CIteratorW::VOpenSubDir()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Nothing to open");
    if (VGetType() == IDirectoryTraverser::IIterator::T_Directory)
        return new CIteratorW(m_wFullPath, m_pStore);
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Nothing to open");
    if (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return new CIteratorW(m_wFullPath, m_pStore);
#endif
    throw CRainmanException(__FILE__, __LINE__, "Cannot iterate something which is not a folder");
}

IDirectoryTraverser::IIterator *CFileSystemStore::CIterator::VOpenSubDir()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0)
        throw CRainmanException(__FILE__, __LINE__, "Nothing to open");
    if (m_pDirEnt->d_type == DT_DIR)
        return new CIterator(m_sFullPathBuf.c_str(), m_pStore);
    throw CRainmanException(__FILE__, __LINE__, "Cannot iterate something which is not a folder");
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Nothing to open");
    if (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return new CIterator(m_sFullPathBuf.c_str(), m_pStore);
    throw CRainmanException(__FILE__, __LINE__, "Cannot iterate something which is not a folder");
#endif
}

IFileStore::IStream *CFileSystemStore::CIteratorW::VOpenFile()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == nullptr || m_pStore == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    if (VGetType() == IDirectoryTraverser::IIterator::T_Directory)
        throw CRainmanException(__FILE__, __LINE__, "Cannot open a folder");
#else
    if (m_HandFD == nullptr || m_pStore == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    if (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        throw CRainmanException(__FILE__, __LINE__, "Cannot open a folder");
#endif
    return ((CFileSystemStore *)m_pStore)->OpenStreamW(m_wFullPath);
}

IFileStore::IStream *CFileSystemStore::CIterator::VOpenFile()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0 || m_pStore == 0)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    return ((CFileSystemStore *)m_pStore)->VOpenStream(m_sFullPathBuf.c_str());
#else
    if (m_HandFD == nullptr || m_pStore == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    if (m_W32FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        throw CRainmanException(__FILE__, __LINE__, "Cannot open a folder");
    return ((CFileSystemStore *)m_pStore)->VOpenStream(m_sFullPathBuf.c_str());
#endif
}

void CFileSystemStore::CIteratorW::_ensureAsciiVersionOf(const wchar_t *wString, char *&sString)
{
    if (sString == nullptr)
    {
#ifdef RAINMAN_GNUC
        std::string sUtf8;
        try
        {
            sUtf8 = RainmanWideToUtf8(wString);
        }
        catch (const std::range_error &)
        {
            throw CRainmanException(__FILE__, __LINE__, "Could not convert path to UTF-8");
        }
        sString = new char[sUtf8.size() + 1];
        std::memcpy(sString, sUtf8.c_str(), sUtf8.size() + 1);
#else
        sString = new char[wcslen(wString) + 1];
        int i = -1;
        do
        {
            ++i;
            sString[i] = (char)wString[i];
        } while (wString[i]);
#endif
    }
}

const char *CFileSystemStore::CIteratorW::VGetName()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    _ensureAsciiVersionOf(m_wFileName.c_str(), m_sFileName);
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    _ensureAsciiVersionOf(m_W32FD.cFileName, m_sFileName);
#endif
    return m_sFileName;
}

const char *CFileSystemStore::CIterator::VGetName()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    return m_pDirEnt->d_name;
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    return m_W32FD.cFileName;
#endif
}

const char *CFileSystemStore::CIteratorW::VGetFullPath()
{
    _ensureAsciiVersionOf(m_wFullPath, m_sFullPath);
    return m_sFullPath;
}

const char *CFileSystemStore::CIterator::VGetFullPath() { return m_sFullPathBuf.c_str(); }

const char *CFileSystemStore::CIteratorW::VGetDirectoryPath()
{
    _ensureAsciiVersionOf(m_wParentPath, m_sParentPath);
    return m_sParentPath;
}

const char *CFileSystemStore::CIterator::VGetDirectoryPath() { return m_sParentPath; }

tLastWriteTime CFileSystemStore::CIterator::VGetLastWriteTime()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0 || m_pStore == 0)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    return ((CFileSystemStore *)m_pStore)->VGetLastWriteTime(m_sFullPathBuf.c_str());
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    tLastWriteTime oRet = m_W32FD.ftLastWriteTime.dwHighDateTime;
    oRet <<= 32;
    oRet += m_W32FD.ftLastWriteTime.dwLowDateTime;
    oRet /= 10;
    oRet -= 0x2B6109100;
    return oRet;
#endif
}

tLastWriteTime CFileSystemStore::CIteratorW::VGetLastWriteTime()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == nullptr || m_pStore == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Handle or store invalid");
    std::string sUtf8Path;
    try
    {
        sUtf8Path = RainmanWideToUtf8(m_wFullPath);
    }
    catch (const std::range_error &)
    {
        throw CRainmanException(__FILE__, __LINE__, "Could not convert file path to UTF-8");
    }
    return GetLastWriteTime(sUtf8Path.c_str());
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    tLastWriteTime oRet = m_W32FD.ftLastWriteTime.dwHighDateTime;
    oRet <<= 32;
    oRet += m_W32FD.ftLastWriteTime.dwLowDateTime;
    oRet /= 10;
    oRet -= 0x2B6109100;
    return oRet;
#endif
}

void CFileSystemStore::CIteratorW::RebuildFullPath(const wchar_t *sFileName)
{
#ifdef RAINMAN_GNUC
    const size_t iParentLength = wcslen(m_wParentPath);
    const size_t iNameLength = wcslen(sFileName);
    auto pFullPath = std::make_unique<wchar_t[]>(iParentLength + iNameLength + 2);
    wcscpy(pFullPath.get(), m_wParentPath);
    if (iParentLength > 0 && pFullPath[iParentLength - 1] != L'/')
        wcscat(pFullPath.get(), L"/");
    wcscat(pFullPath.get(), sFileName);
    delete[] m_wFullPath;
    m_wFullPath = pFullPath.release();
    delete[] m_sFullPath;
    m_sFullPath = nullptr;
    delete[] m_sFileName;
    m_sFileName = nullptr;
#else
    (void)sFileName;
#endif
}

void CFileSystemStore::CIterator::RebuildFullPath(const char *sFileName)
{
    m_sFullPathBuf.resize(m_iParentPathLen);
    m_sFullPathBuf.assign(m_sParentPath, m_iParentPathLen);
#ifdef RAINMAN_GNUC
    // GNUC parent path already ends with '/'
#else
    m_sFullPathBuf += '\\';
#endif
    m_sFullPathBuf += sFileName;
}

IDirectoryTraverser::IIterator::eErrors CFileSystemStore::CIterator::VNextItem()
{
#ifdef RAINMAN_GNUC
    if (m_pDirEnt == 0)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");

    do
    {
        m_pDirEnt = readdir(m_pDirectory);
    } while (m_pDirEnt != nullptr &&
             (strcmp(m_pDirEnt->d_name, ".") == 0 || strcmp(m_pDirEnt->d_name, "..") == 0));

    if (m_pDirEnt != nullptr)
    {
        RebuildFullPath(m_pDirEnt->d_name);
        return E_OK;
    }
    else
    {
        closedir(m_pDirectory);
        m_pDirectory = 0;
        return E_AtEnd;
    }

#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    if (FindNextFileA(m_HandFD, &m_W32FD) == TRUE)
    {
        RebuildFullPath(m_W32FD.cFileName);
        return E_OK;
    }
    else
    {
        if (GetLastError() == ERROR_NO_MORE_FILES)
        {
            return E_AtEnd;
        }
        else
        {
            FindClose(m_HandFD);
            m_HandFD = nullptr;
            throw CRainmanException(__FILE__, __LINE__, "Unknown error");
        }
    }
#endif
}

IDirectoryTraverser::IIterator::eErrors CFileSystemStore::CIteratorW::VNextItem()
{
#ifdef RAINMAN_GNUC
    if (m_pDirectory == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    if (m_pDirEnt == nullptr)
        return E_AtEnd;

    do
    {
        m_pDirEnt = readdir(m_pDirectory);
    } while (m_pDirEnt != nullptr &&
             (strcmp(m_pDirEnt->d_name, ".") == 0 || strcmp(m_pDirEnt->d_name, "..") == 0));

    if (m_pDirEnt == nullptr)
    {
        closedir(m_pDirectory);
        m_pDirectory = nullptr;
        return E_AtEnd;
    }

    try
    {
        m_wFileName = RainmanUtf8ToWide(m_pDirEnt->d_name);
    }
    catch (const std::range_error &)
    {
        throw CRainmanException(__FILE__, __LINE__, "Could not convert directory entry name from UTF-8");
    }
    RebuildFullPath(m_wFileName.c_str());
    return E_OK;
#else
    if (m_HandFD == nullptr)
        throw CRainmanException(__FILE__, __LINE__, "Invalid handle");
    if (FindNextFileW(m_HandFD, &m_W32FD) == TRUE)
    {
        delete[] m_wFullPath;
        m_wFullPath = new wchar_t[wcslen(m_wParentPath) + wcslen(m_W32FD.cFileName) + 2];
        if (m_sFullPath == nullptr)
        {
            FindClose(m_HandFD);
            m_HandFD = nullptr;
            throw CRainmanException(__FILE__, __LINE__, "Failed to allocate memory");
        }
        wcscpy(m_wFullPath, m_wParentPath);
        wcscat(m_wFullPath, L"\\");
        wcscat(m_wFullPath, m_W32FD.cFileName);

        delete[] m_sFullPath;
        m_sFullPath = nullptr;
        delete[] m_sFileName;
        m_sFileName = nullptr;

        return E_OK;
    }
    else
    {
        if (GetLastError() == ERROR_NO_MORE_FILES)
        {
            return E_AtEnd;
        }
        else
        {
            FindClose(m_HandFD);
            m_HandFD = nullptr;
            throw CRainmanException(__FILE__, __LINE__, "Unknown error");
        }
    }
#endif
}

IDirectoryTraverser::IIterator *CFileSystemStore::VIterate(const char *sPath) { return new CIterator(sPath, this); }

CFileSystemStore::CIteratorW *CFileSystemStore::IterateW(const wchar_t *sPath) { return new CIteratorW(sPath, this); }

unsigned long CFileSystemStore::VGetEntryPointCount()
{
#ifdef RAINMAN_GNUC
    m_sDriveStrings[0][0] = '/';
    m_sDriveStrings[0][1] = 0;
    return 1;
#else
    unsigned long iCount = 0;
    int curdrive = _getdrive();
    for (int i = 1; i <= 26; ++i)
    {
        if (_chdrive(i) == 0)
        {
            m_sDriveStrings[iCount][0] = i + 'A' - 1;
            m_sDriveStrings[iCount][1] = ':';
            m_sDriveStrings[iCount][2] = 0;
            ++iCount;
        }
    }
    _chdrive(curdrive);
    return iCount;
#endif
}

const char *CFileSystemStore::VGetEntryPoint(unsigned long iID) { return m_sDriveStrings[iID]; }
#endif
