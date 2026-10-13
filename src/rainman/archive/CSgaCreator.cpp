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

#include "rainman/archive/CSgaCreator.h"
#include "rainman/core/CThreadPool.h"
#include "rainman/core/Internal_Util.h"
#include "rainman/io/CMemoryStore.h"
#include <chrono>
#include <future>
#include <memory>
#include <zlib.h>
#include <ctime>
extern "C"
{
#include "rainman/util/md5.h"
}
#include <queue>
#include <algorithm>
#include "rainman/core/Exception.h"
#include "rainman/core/memdebug.h"

bool CSgaCreator::CInputFile::OpLT(const std::unique_ptr<CSgaCreator::CInputFile> &oA,
                                   const std::unique_ptr<CSgaCreator::CInputFile> &oB)
{
    return (strcmp(oA->sFullPath.c_str() + oA->iNameOffset, oB->sFullPath.c_str() + oB->iNameOffset) < 0);
}

bool CSgaCreator::CInputDirectory::OpLT(const std::unique_ptr<CSgaCreator::CInputDirectory> &oA,
                                        const std::unique_ptr<CSgaCreator::CInputDirectory> &oB)
{
    return (oA->sNameFull < oB->sNameFull);
}

void CSgaCreator::CInputDirectory::FullCount(size_t &iDirCount, size_t &iFileCount)
{
    ++iDirCount;
    iFileCount += vFilesList.size();
    for (const auto &pDir : vDirsList)
    {
        if (pDir)
            pDir->FullCount(iDirCount, iFileCount);
    }
}

std::unique_ptr<CSgaCreator::CInputDirectory> CSgaCreator::_ScanDirectory(IDirectoryTraverser::IIterator *pDirectory,
                                                                          IFileStore *pStore, size_t iDirBaseL)
{
    if (pDirectory == nullptr)
        QUICK_THROW("No input directory")
    if (pStore == nullptr)
        QUICK_THROW("No input file store")

    auto pUs = std::make_unique<CSgaCreator::CInputDirectory>();
    const char *sDirectory;

    try
    {
        sDirectory = pDirectory->VGetDirectoryPath();
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Cannot get directory path");
    }

    size_t iDirLen = strlen(sDirectory);
    if (iDirBaseL != 0)
    {
        size_t iNameBegin = iDirBaseL;
        while (iNameBegin < iDirLen && (sDirectory[iNameBegin] == '\\' || sDirectory[iNameBegin] == '/'))
            ++iNameBegin;
        size_t iNameEnd = iDirLen;
        while (iNameEnd > iNameBegin && (sDirectory[iNameEnd - 1] == '\\' || sDirectory[iNameEnd - 1] == '/'))
            --iNameEnd;
        pUs->sNameFull.assign(sDirectory + iNameBegin, iNameEnd - iNameBegin);
    }
    else
    {
        iDirBaseL = iDirLen;
        pUs->sNameFull.clear();
    }
    Util_strtolower(pUs->sNameFull.data());
    try
    {
        while (pDirectory->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
        {
            if (pDirectory->VGetType() == IDirectoryTraverser::IIterator::T_Directory)
            {
                IDirectoryTraverser::IIterator *pChild;
                try
                {
                    pChild = pDirectory->VOpenSubDir();
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Cannot traverse sub-dir \'%s\'",
                                            pDirectory->VGetFullPath());
                }
                if (pChild != nullptr)
                {
                    std::unique_ptr<CSgaCreator::CInputDirectory> pChildIn;
                    try
                    {
                        pChildIn = _ScanDirectory(pChild, pStore, iDirBaseL);
                    }
                    catch (const CRainmanException &e)
                    {
                        delete pChild;
                        throw CRainmanException(e, __FILE__, __LINE__, "Cannot scan sub-dir \'%s\'",
                                                pDirectory->VGetFullPath());
                    }
                    delete pChild;
                    pUs->vDirsList.push_back(std::move(pChildIn));
                }
            }
            else
            {
                auto pFile = std::make_unique<CSgaCreator::CInputFile>();
                try
                {
                    pFile->oLastWriteTime = pDirectory->VGetLastWriteTime();
                    pFile->sFullPath = pDirectory->VGetFullPath();
                    Util_strtolower(pFile->sFullPath.data());
                }
                CATCH_THROW("pDirectory problem")
                pFile->iNameOffset = iDirLen;
                if (pFile->sFullPath[iDirLen] == '\\' || pFile->sFullPath[iDirLen] == '/')
                    pFile->iNameOffset += 1;
                pFile->pFileStore = pStore;
                pUs->vFilesList.push_back(std::move(pFile));
            }
            switch (pDirectory->VNextItem())
            {
            case IDirectoryTraverser::IIterator::E_AtEnd:
                goto endwhile;
            default:
                break;
            }
        }
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "(rethrow)");
    }
endwhile:

    std::sort(pUs->vDirsList.begin(), pUs->vDirsList.end(), CInputDirectory::OpLT);
    std::sort(pUs->vFilesList.begin(), pUs->vFilesList.end(), CInputFile::OpLT);

    return pUs;
}

// -- Phase J: Parallel per-file compression infrastructure --

namespace
{

//! Holds the result of compressing a single file (produced by worker threads).
struct CompressedFileData
{
    std::unique_ptr<char[]> pData; // Compressed or uncompressed data buffer
    unsigned long iDataSize = 0;   // Size of pData
    unsigned long iUncompressedSize = 0;
    unsigned long iUncompressedCRC = 0;
    long iFlags = 0;
};

//! Reads, compresses, and CRC32-checksums a single file. Thread-safe — each call
//! opens its own stream and allocates independent buffers.
CompressedFileData CompressOneFile(IFileStore *pFileStore, const char *sFullPath, long iVersion)
{
    CompressedFileData result;

    // Open and read the file
    std::unique_ptr<IFileStore::IStream> fileStream;
    try
    {
        fileStream = std::unique_ptr<IFileStore::IStream>(pFileStore->VOpenStream(sFullPath));
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Could not open \'%s\'", sFullPath);
    }
    try
    {
        fileStream->VSeek(0, IFileStore::IStream::SL_End);
        result.iUncompressedSize = static_cast<unsigned long>(fileStream->VTell());
        fileStream->VSeek(0, IFileStore::IStream::SL_Root);
        auto pFileBuffer = std::make_unique<char[]>(result.iUncompressedSize);
        fileStream->VRead(result.iUncompressedSize, 1, pFileBuffer.get());

        // CRC32 of uncompressed data
        result.iUncompressedCRC =
            crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef *>(pFileBuffer.get()), result.iUncompressedSize);

        // Compress
        unsigned long iCompBuffSize = compressBound(result.iUncompressedSize);
        auto pCompressedBuffer = std::make_unique<char[]>(iCompBuffSize);
        if (compress2(reinterpret_cast<Bytef *>(pCompressedBuffer.get()), &iCompBuffSize,
                      reinterpret_cast<const Bytef *>(pFileBuffer.get()), result.iUncompressedSize,
                      Z_BEST_COMPRESSION) != Z_OK)
            throw CRainmanException(__FILE__, __LINE__, "Compression error");

        // Determine flags based on compression ratio
        if (iCompBuffSize < result.iUncompressedSize)
        {
            if (iVersion == 4)
                result.iFlags = 0x100;
            else
                result.iFlags = (result.iUncompressedSize < 4096 ? 0x20 : 0x10);
        }
        else
        {
            result.iFlags = 0;
        }

        // Keep the smaller buffer
        if (result.iFlags == 0)
        {
            result.pData = std::move(pFileBuffer);
            result.iDataSize = result.iUncompressedSize;
        }
        else
        {
            result.pData = std::move(pCompressedBuffer);
            result.iDataSize = iCompBuffSize;
        }
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "File operation failed");
    }

    return result;
}

} // namespace

void CSgaCreator::CreateSga(IDirectoryTraverser::IIterator *pDirectory, IFileStore *pStore, const char *_sTocName,
                            const char *sOutputFile, long iVersion, const char *sArchiveTitle, const char *sTocTitle)
{
    RAINMAN_LOG_INFO("CSgaCreator — building SGA archive \"{}\" (version {})", sOutputFile ? sOutputFile : "(null)",
                     iVersion);
    if (iVersion != 2 && iVersion != 4)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Version %li not supported", iVersion);
    std::unique_ptr<CSgaCreator::CInputDirectory> pInput;
    try
    {
        pInput = _ScanDirectory(pDirectory, pStore);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "ScanDirectory failed");
    }

    FILE *fOut = nullptr;
    std::unique_ptr<IFileStore::IOutputStream> dataHeader;
    try
    {
        unsigned long iTmp = 0;
        unsigned short iSTmp = 0;
        fOut = fopen(sOutputFile, "w+b");
        if (fOut == nullptr)
            throw CRainmanException(
                nullptr, __FILE__, __LINE__,
                "Cold not open \'%s\' (does the directory exist, and do you have permission to create files there?)",
                sOutputFile);

        // File signature
        if (fwrite("_ARCHIVE", 1, 8, fOut) != 8)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // Version
        if (fwrite(&iVersion, sizeof(uint32_t), 1, fOut) != 1)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // MD5
        if (fwrite("-BUFFER FOR MD5-", 1, 16, fOut) != 16)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // Archive UI name
        wchar_t sUiName[65];
        memset(sUiName, 0, 65 * sizeof(wchar_t));
        if (sArchiveTitle)
        {
            size_t i = 0;
            do
            {
                sUiName[i] = sArchiveTitle[i];
                ++i;
            } while (sArchiveTitle[i] && i < 65);
        }
        else
        {
            wcscpy(sUiName, L"Made with Corsix\'s Rainman");
        }
        if (fwrite(sUiName, sizeof(uint16_t), 64, fOut) != 64)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // MD5
        if (fwrite("-BUFFER FOR MD5-", 1, 16, fOut) != 16)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // DataHeaderSize / DataOffset
        if (fwrite(&iTmp, sizeof(uint32_t), 1, fOut) != 1 || fwrite(&iTmp, sizeof(uint32_t), 1, fOut) != 1)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        // Platform [V4 only]
        if (iVersion == 4)
        {
            iTmp = 1;
            if (fwrite(&iTmp, sizeof(uint32_t), 1, fOut) != 1)
                throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        }

        const unsigned long iTocOutLength = 138;
        const unsigned long iDirOutLength = 12;
        unsigned long iFileOutLength;
        long iNamesStartLocation, iNamesLength;
        long iFilesLoc;
        std::vector<CInputFile *> vFilesList;
        size_t iDirCount = 0, iFileCount = 0;

        // Data header buffer
        CMemoryStore oMemoryStore;
        try
        {
            dataHeader = std::unique_ptr<IFileStore::IOutputStream>(oMemoryStore.VOpenOutputStream(nullptr, false));

            // TOC offset & count
            iTmp = 24;
            dataHeader->VWrite(1, sizeof(uint32_t), &iTmp);
            iSTmp = 1;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

            // Directory offset & count
            pInput->FullCount(iDirCount, iFileCount);
            iTmp = 24 + iTocOutLength;
            dataHeader->VWrite(1, sizeof(uint32_t), &iTmp);
            iSTmp = (unsigned short)iDirCount;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

            // File offset & count
            iTmp = 24 + iTocOutLength + (iDirOutLength * iDirCount);
            dataHeader->VWrite(1, sizeof(uint32_t), &iTmp);
            iSTmp = (unsigned short)iFileCount;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
            iFileOutLength = (iVersion == 4 ? 22 : 20);

            // Names offset & count
            iTmp = 24 + iTocOutLength + (iDirOutLength * iDirCount) + (iFileOutLength * iFileCount);
            dataHeader->VWrite(1, sizeof(uint32_t), &iTmp);
            iSTmp = (unsigned short)(iFileCount + iDirCount);
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

            // TOC Entry
            char sTocName[65];
            memset(sTocName, 0, 65);
            strncpy(sTocName, _sTocName, strlen(_sTocName) > 64 ? 64 : strlen(_sTocName));
            Util_strtolower(sTocName);
            dataHeader->VWrite(64, (unsigned long)sizeof(char), sTocName);

            if (sTocTitle)
            {
                memset(sTocName, 0, 65);
                strncpy(sTocName, sTocTitle, strlen(sTocTitle) > 64 ? 64 : strlen(sTocTitle));
                Util_strtolower(sTocName);
            }
            else
            {
                struct tm *newtime;
                time_t ltime;
                time(&ltime);
                newtime = gmtime(&ltime);
                sprintf(sTocName, "archive_%i_%i_%i_%i_%i_%i", newtime->tm_year + 1900, newtime->tm_mon,
                        newtime->tm_mday, newtime->tm_hour, newtime->tm_min, newtime->tm_sec);
            }

            dataHeader->VWrite(64, (unsigned long)sizeof(char), sTocName);
            iSTmp = 0;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
            iSTmp = (short)iDirCount;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
            iSTmp = 0;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
            iSTmp = (short)iFileCount;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
            iSTmp = 0;
            dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

            // Dir header space
            for (size_t i = 0; i < iDirCount; ++i)
                dataHeader->VWrite(12, (unsigned long)sizeof(char), "DIR H BUFFER");

            // File header space
            for (size_t i = 0; i < iFileCount; ++i)
                dataHeader->VWrite(iFileOutLength, (unsigned long)sizeof(char), "[-FILE HEADER BUFFER-]");

            // Dir headers properly
            iNamesStartLocation = dataHeader->VTell();
            iNamesLength = 0;
            dataHeader->VSeek(24 + iTocOutLength, IFileStore::IStream::SL_Root);

            std::queue<CInputDirectory *> qDirsTodo;
            qDirsTodo.push(pInput.get());
            vFilesList.reserve(iFileCount);
            size_t iDirC = 1, iFileC = 0;
            while (!qDirsTodo.empty())
            {
                CInputDirectory *pDir = qDirsTodo.front();
                qDirsTodo.pop();

                // Name offset
                dataHeader->VWrite(1, sizeof(uint32_t), &iNamesLength);

                // Name
                long iRestorePos = dataHeader->VTell();
                dataHeader->VSeek(iNamesStartLocation + iNamesLength, IFileStore::IStream::SL_Root);
                size_t iNameL = pDir->sNameFull.size() + 1;
                dataHeader->VWrite((unsigned long)iNameL, (unsigned long)sizeof(char), pDir->sNameFull.c_str());
                iNamesLength += (long)iNameL;
                dataHeader->VSeek(iRestorePos, IFileStore::IStream::SL_Root);

                // Start/End directory
                iSTmp = (short)iDirC;
                dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
                iDirC += pDir->vDirsList.size();
                iSTmp = (short)iDirC;
                dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

                // Start/End file
                iSTmp = (short)iFileC;
                dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
                iFileC += pDir->vFilesList.size();
                iSTmp = (short)iFileC;
                dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);

                // Identify child folders/files
                for (const auto &pChild : pDir->vDirsList)
                {
                    qDirsTodo.push(pChild.get());
                }
                for (const auto &pFile : pDir->vFilesList)
                {
                    vFilesList.push_back(pFile.get());
                }
            }

            // File headers properly phase 1
            iFilesLoc = dataHeader->VTell();
            for (auto itr = vFilesList.begin(); itr != vFilesList.end(); ++itr, iFilesLoc += iFileOutLength)
            {
                dataHeader->VSeek(iFilesLoc, IFileStore::IStream::SL_Root);

                // Name offset
                dataHeader->VWrite(1, sizeof(uint32_t), &iNamesLength);

                // Name
                dataHeader->VSeek(iNamesStartLocation + iNamesLength, IFileStore::IStream::SL_Root);
                const char *pFileName = (**itr).sFullPath.c_str() + (**itr).iNameOffset;
                size_t iNameL = strlen(pFileName) + 1;
                dataHeader->VWrite((unsigned long)iNameL, (unsigned long)sizeof(char), pFileName);
                iNamesLength += (long)iNameL;
            }
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__, "Memory operation failed");
        }

        // Get output file ready for writing file data
        auto *pOutStream = static_cast<CMemoryStore::COutStream *>(dataHeader.get());
        if (fwrite(pOutStream->GetData(), 1, pOutStream->GetDataLength(), fOut) != pOutStream->GetDataLength())
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");

        // Phase 1: Parallel per-file compression
        auto tCompressStart = std::chrono::steady_clock::now();
        auto &pool = CThreadPool::Instance();
        std::vector<std::future<CompressedFileData>> compressionFutures;
        compressionFutures.reserve(vFilesList.size());
        for (auto itr = vFilesList.begin(); itr != vFilesList.end(); ++itr)
        {
            compressionFutures.push_back(
                pool.Submit(CompressOneFile, (**itr).pFileStore, (**itr).sFullPath.c_str(), iVersion));
        }

        // Phase 2: Sequential header writing and data output
        iFilesLoc = 24 + iTocOutLength + (iDirOutLength * iDirCount) + 4;
        long iDataLengthTotal = 0;
        long iPreDataSize = 0;
        if (iVersion == 4)
            iPreDataSize = 260;
        size_t iFileIdx = 0;
        for (auto itr = vFilesList.begin(); itr != vFilesList.end(); ++itr, iFilesLoc += iFileOutLength, ++iFileIdx)
        {
            CompressedFileData compressed = compressionFutures[iFileIdx].get();

            iDataLengthTotal += iPreDataSize;
            try
            {
                dataHeader->VSeek(iFilesLoc, IFileStore::IStream::SL_Root);
                if (iVersion == 4)
                {
                    // Data offset
                    dataHeader->VWrite(1, sizeof(uint32_t), &iDataLengthTotal);

                    // Size compressed / decompressed
                    dataHeader->VWrite(1, sizeof(uint32_t), &compressed.iDataSize);
                    dataHeader->VWrite(1, sizeof(uint32_t), &compressed.iUncompressedSize);

                    // Time
                    iTmp = (unsigned long)(**itr).oLastWriteTime;
                    dataHeader->VWrite(1, sizeof(uint32_t), &iTmp);

                    // Flags
                    iSTmp = (unsigned short)compressed.iFlags;
                    dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &iSTmp);
                }
                else
                {
                    // Flags
                    dataHeader->VWrite(1, (unsigned long)sizeof(unsigned short), &compressed.iFlags);

                    // Data offset
                    dataHeader->VWrite(1, sizeof(uint32_t), &iDataLengthTotal);

                    // Size compressed / decompressed
                    dataHeader->VWrite(1, sizeof(uint32_t), &compressed.iDataSize);
                    dataHeader->VWrite(1, sizeof(uint32_t), &compressed.iUncompressedSize);
                }
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Memory operation failed");
            }

            if (iVersion == 4)
            {
                char sPreDataName[256];
                memset(sPreDataName, 0, 256);
                strcpy(sPreDataName, (**itr).sFullPath.c_str() + (**itr).iNameOffset);
                sPreDataName[255] = 0;

                if (fwrite(sPreDataName, 1, 256, fOut) != 256)
                    throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
                if (fwrite(&compressed.iUncompressedCRC, sizeof(uint32_t), 1, fOut) != 1)
                    throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
            }
            if (fwrite(compressed.pData.get(), 1, compressed.iDataSize, fOut) != compressed.iDataSize)
                throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
            iDataLengthTotal += compressed.iDataSize;
        }

        {
            auto tCompressEnd = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tCompressEnd - tCompressStart).count();
            RAINMAN_LOG_INFO("Parallel SGA compression: {} files in {}ms", compressionFutures.size(), ms);
        }

        // Header MD5
        MD5Context md5DataKey;
        unsigned char sMD5[16];
        MD5InitKey(&md5DataKey, "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF");
        MD5Update(&md5DataKey, (const unsigned char *)pOutStream->GetData(), pOutStream->GetDataLength());
        MD5Final(sMD5, &md5DataKey);
        if (fseek(fOut, 156, SEEK_SET) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek operation failed");
        if (fwrite(sMD5, 1, 16, fOut) != 16)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");

        // Data header size / offset
        iTmp = pOutStream->GetDataLength();
        if (fwrite(&iTmp, sizeof(uint32_t), 1, fOut) != 1)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
        iTmp += (iVersion == 4 ? 0xB8 : 0xB4);
        if (fwrite(&iTmp, sizeof(uint32_t), 1, fOut) != 1)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");

        // Write data header properly
        if (iVersion == 4)
        {
            if (fseek(fOut, 4, SEEK_CUR) != 0)
                throw CRainmanException(__FILE__, __LINE__, "Seek operation failed");
        }
        if (fwrite(pOutStream->GetData(), 1, pOutStream->GetDataLength(), fOut) != pOutStream->GetDataLength())
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");

        // Data MD5
        fflush(fOut);
        MD5InitKey(&md5DataKey, "E01519D6-2DB7-4640-AF54-0A23319C56C3");
        fseek(fOut, (iVersion == 4) ? 184 : 180, SEEK_SET);
        char *sBuffer = new char[1048576];
        while (!feof(fOut))
        {
            MD5Update(&md5DataKey, (const unsigned char *)sBuffer, (unsigned int)fread(sBuffer, 1, 1048576, fOut));
        }
        delete[] sBuffer;
        MD5Final(sMD5, &md5DataKey);
        if (fseek(fOut, 12, SEEK_SET) != 0)
            throw CRainmanException(__FILE__, __LINE__, "Seek operation failed");
        if (fwrite(sMD5, 1, 16, fOut) != 16)
            throw CRainmanException(__FILE__, __LINE__, "Write operation failed");
    }
    catch (const CRainmanException &e)
    {
        if (fOut)
            fclose(fOut);
        throw e;
    }
    if (fOut)
        fclose(fOut);
}
