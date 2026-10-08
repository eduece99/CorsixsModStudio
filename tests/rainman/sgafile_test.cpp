#include <gtest/gtest.h>
#include "rainman/archive/CSgaFile.h"
#include "rainman/io/CMemoryStore.h"
#include "rainman/core/Exception.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include "../common/TestProcessId.h"
#include <optional>
#include <zlib.h>
extern "C"
{
#include "rainman/util/md5.h"
}

// --- SGA v2 binary builder helpers ---
namespace
{

void AppendU32(std::vector<unsigned char> &buf, uint32_t val)
{
    buf.push_back(static_cast<unsigned char>(val & 0xFF));
    buf.push_back(static_cast<unsigned char>((val >> 8) & 0xFF));
    buf.push_back(static_cast<unsigned char>((val >> 16) & 0xFF));
    buf.push_back(static_cast<unsigned char>((val >> 24) & 0xFF));
}

void AppendU16(std::vector<unsigned char> &buf, uint16_t val)
{
    buf.push_back(static_cast<unsigned char>(val & 0xFF));
    buf.push_back(static_cast<unsigned char>((val >> 8) & 0xFF));
}

void AppendPadded(std::vector<unsigned char> &buf, const char *str, size_t totalSize)
{
    size_t len = std::strlen(str);
    size_t toCopy = (std::min)(len, totalSize);
    buf.insert(buf.end(), reinterpret_cast<const unsigned char *>(str),
               reinterpret_cast<const unsigned char *>(str) + toCopy);
    buf.resize(buf.size() + (totalSize - toCopy), 0);
}

void AppendZeros(std::vector<unsigned char> &buf, size_t count) { buf.resize(buf.size() + count, 0); }

std::vector<unsigned char> ZlibCompress(const std::string &data)
{
    uLong compSize = compressBound(static_cast<uLong>(data.size()));
    std::vector<unsigned char> result(compSize);
    compress(result.data(), &compSize, reinterpret_cast<const Bytef *>(data.data()),
             static_cast<uLong>(data.size()));
    result.resize(compSize);
    return result;
}

struct SgaTestFile
{
    std::string leafName;
    std::string content;
    bool compressed = false;
};

struct SgaTestDir
{
    std::string fullName; // "" for root, "subdir" for a child
    uint16_t subDirBegin = 0, subDirEnd = 0;
    uint16_t fileBegin = 0, fileEnd = 0;
};

// Builds a valid SGA v2 binary with the given directory/file structure.
// All files are placed under a single ToC with the given name.
std::vector<unsigned char> BuildSgaV2(const std::string &tocName, const std::vector<SgaTestDir> &dirs,
                                      const std::vector<SgaTestFile> &files)
{
    // --- Build string table ---
    std::vector<unsigned char> stringTable;
    std::vector<uint32_t> dirNameOffsets;
    for (const auto &d : dirs)
    {
        dirNameOffsets.push_back(static_cast<uint32_t>(stringTable.size()));
        stringTable.insert(stringTable.end(), d.fullName.begin(), d.fullName.end());
        stringTable.push_back(0);
    }
    std::vector<uint32_t> fileNameOffsets;
    for (const auto &f : files)
    {
        fileNameOffsets.push_back(static_cast<uint32_t>(stringTable.size()));
        stringTable.insert(stringTable.end(), f.leafName.begin(), f.leafName.end());
        stringTable.push_back(0);
    }

    // --- Compute data header layout ---
    uint32_t iToCOffset = 24;
    uint16_t iToCCount = 1;
    uint32_t iDirOffset = iToCOffset + 140;
    auto iDirCount = static_cast<uint16_t>(dirs.size());
    uint32_t iFileOffset = iDirOffset + 12 * iDirCount;
    auto iFileCount = static_cast<uint16_t>(files.size());
    uint32_t iItemOffset = iFileOffset + 20 * iFileCount;
    uint16_t iItemCount = iFileCount;
    uint32_t dataHeaderSize = iItemOffset + static_cast<uint32_t>(stringTable.size());

    // --- Prepare compressed payloads and compute file data offsets ---
    std::vector<std::vector<unsigned char>> payloads;
    std::vector<uint32_t> fileDataOffsets;
    uint32_t dataRunning = 0;
    for (const auto &f : files)
    {
        std::vector<unsigned char> payload;
        if (f.compressed)
            payload = ZlibCompress(f.content);
        else
            payload.assign(f.content.begin(), f.content.end());

        dataRunning += 264; // pre-data: 256 name + 4 date + 4 CRC (v2)
        fileDataOffsets.push_back(dataRunning);
        dataRunning += static_cast<uint32_t>(payload.size());
        payloads.push_back(std::move(payload));
    }

    // --- Build data header blob ---
    std::vector<unsigned char> dataHeader;
    dataHeader.reserve(dataHeaderSize);

    // _SgaDataHeaderInfo (24 bytes packed)
    AppendU32(dataHeader, iToCOffset);
    AppendU16(dataHeader, iToCCount);
    AppendU32(dataHeader, iDirOffset);
    AppendU16(dataHeader, iDirCount);
    AppendU32(dataHeader, iFileOffset);
    AppendU16(dataHeader, iFileCount);
    AppendU32(dataHeader, iItemOffset);
    AppendU16(dataHeader, iItemCount);

    // _SgaToC (140 bytes packed)
    AppendPadded(dataHeader, tocName.c_str(), 64);
    AppendPadded(dataHeader, "", 64); // sBaseDirName
    AppendU16(dataHeader, 0);         // iStartDir
    AppendU16(dataHeader, iDirCount); // iEndDir
    AppendU16(dataHeader, 0);         // iStartFile
    AppendU16(dataHeader, iFileCount); // iEndFile
    AppendU32(dataHeader, 0);          // iFolderOffset

    // _SgaDirInfo entries (12 bytes each)
    for (size_t i = 0; i < dirs.size(); ++i)
    {
        AppendU32(dataHeader, dirNameOffsets[i]);
        AppendU16(dataHeader, dirs[i].subDirBegin);
        AppendU16(dataHeader, dirs[i].subDirEnd);
        AppendU16(dataHeader, dirs[i].fileBegin);
        AppendU16(dataHeader, dirs[i].fileEnd);
    }

    // _SgaFileInfo entries (20 bytes each, v2)
    for (size_t i = 0; i < files.size(); ++i)
    {
        AppendU32(dataHeader, fileNameOffsets[i]);
        AppendU32(dataHeader, files[i].compressed ? 0x10u : 0x00u);
        AppendU32(dataHeader, fileDataOffsets[i]);
        AppendU32(dataHeader, static_cast<uint32_t>(payloads[i].size()));
        AppendU32(dataHeader, static_cast<uint32_t>(files[i].content.size()));
    }

    // String table
    dataHeader.insert(dataHeader.end(), stringTable.begin(), stringTable.end());
    EXPECT_EQ(dataHeader.size(), dataHeaderSize);

    // --- Compute keyed MD5 of data header ---
    MD5Context md5ctx;
    MD5InitKey(&md5ctx, "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF");
    MD5Update(&md5ctx, dataHeader.data(), static_cast<unsigned int>(dataHeader.size()));
    unsigned char md5Result[16];
    MD5Final(md5Result, &md5ctx);

    // --- Assemble complete SGA file ---
    uint32_t fileHeaderSize = 180; // v2 fixed header
    uint32_t dataOffset = fileHeaderSize + dataHeaderSize;

    std::vector<unsigned char> sga;
    sga.reserve(dataOffset + dataRunning);

    // File header (180 bytes)
    sga.insert(sga.end(), {'_', 'A', 'R', 'C', 'H', 'I', 'V', 'E'}); // identifier
    AppendU32(sga, 2);                                                  // version
    AppendZeros(sga, 16);                                               // tool MD5
    AppendZeros(sga, 128);                                              // archive type
    sga.insert(sga.end(), md5Result, md5Result + 16);                   // data header MD5
    AppendU32(sga, dataHeaderSize);
    AppendU32(sga, dataOffset);

    // Data header blob
    sga.insert(sga.end(), dataHeader.begin(), dataHeader.end());

    // File data section
    for (size_t i = 0; i < files.size(); ++i)
    {
        // Pre-data block: 256-byte name + 4-byte date + 4-byte CRC
        AppendPadded(sga, files[i].leafName.c_str(), 256);
        AppendU32(sga, 0); // date
        uint32_t crc = crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef *>(files[i].content.data()),
                             static_cast<uInt>(files[i].content.size()));
        AppendU32(sga, crc);
        // Payload
        sga.insert(sga.end(), payloads[i].begin(), payloads[i].end());
    }

    return sga;
}

// Wraps raw bytes in a CMemoryStore-backed readable stream.
// Caller must delete the returned stream. The store and data must outlive the stream.
IFileStore::IStream *WrapAsStream(CMemoryStore &store, std::vector<unsigned char> &data)
{
    char *range = store.MemoryRange(data.data(), data.size());
    return store.VOpenStream(range);
}

} // namespace

class SgaFileTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() /
            ("sga_test_" + std::to_string(GetTestProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }
};

TEST_F(SgaFileTest, LoadFromNullStreamThrows) {
    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.Load(nullptr);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(SgaFileTest, LoadInvalidHeaderThrows) {
    // Create a stream with garbage data
    CMemoryStore store;
    store.VInit();

    const char garbage[] = "THIS_IS_NOT_AN_SGA_FILE_HEADER__";
    char* range = store.MemoryRange((void*)garbage, sizeof(garbage) - 1);
    auto* stream = store.VOpenStream(range);
    ASSERT_NE(stream, nullptr);

    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.Load(stream);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
    delete stream;
}

TEST_F(SgaFileTest, LoadTruncatedStreamThrows) {
    // Only 4 bytes - too short even for identifier
    CMemoryStore store;
    store.VInit();

    const char data[] = "_ARC";
    char* range = store.MemoryRange((void*)data, 4);
    auto* stream = store.VOpenStream(range);
    ASSERT_NE(stream, nullptr);

    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.Load(stream);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
    delete stream;
}

TEST_F(SgaFileTest, LoadValidIdentifierBadVersionThrows) {
    // Write "_ARCHIVE" + unsupported version (99)
    CMemoryStore store;
    store.VInit();
    auto* out = CMemoryStore::OpenOutputStreamExt();

    const char ident[] = "_ARCHIVE";
    out->VWrite(8, 1, ident);
    unsigned long version = 99;
    out->VWrite(1, sizeof(unsigned long), &version);

    auto* stream = CMemoryStore::OpenStreamExt(
        const_cast<char*>(out->GetData()), out->GetDataLength(), false);

    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.Load(stream);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
    EXPECT_NE(std::strstr(caught->getMessage(), "not supported"), nullptr);
    delete stream;
    delete out;
}

TEST_F(SgaFileTest, VCreateFolderInThrows) {
    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.VCreateFolderIn("some/path", "folder");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(SgaFileTest, GetInputStreamWithoutInitThrows) {
    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try {
        sga.GetInputStream();
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

// --- Success-path tests using in-memory SGA v2 archives ---

class SgaFileLoadTest : public ::testing::Test
{
  protected:
    CMemoryStore store;
    std::vector<unsigned char> sgaData;

    void SetUp() override { store.VInit(); }

    // Build a simple archive: 1 ToC "data", root dir with 1 uncompressed file
    void BuildSimpleArchive()
    {
        std::vector<SgaTestDir> dirs = {
            {"", 1, 1, 0, 1}, // root: no subdirs, 1 file
        };
        std::vector<SgaTestFile> files = {
            {"test.txt", "Hello World!", false},
        };
        sgaData = BuildSgaV2("data", dirs, files);
    }

    // Build an archive with subdirectories and compressed files
    void BuildComplexArchive()
    {
        std::vector<SgaTestDir> dirs = {
            {"", 1, 2, 0, 1},            // root: 1 subdir (dir 1), 1 file (file 0)
            {"subdir", 2, 2, 1, 2},       // subdir: no subdirs, 1 file (file 1)
        };
        std::vector<SgaTestFile> files = {
            {"test.txt", "Hello World!", false},
            {"hello.txt", "This is compressed content for testing zlib decompression!", true},
        };
        sgaData = BuildSgaV2("data", dirs, files);
    }

    // Loads the archive from sgaData. Returns a heap-allocated CSgaFile.
    // The CMemoryStore and sgaData must outlive the returned object.
    std::unique_ptr<CSgaFile> LoadArchive()
    {
        auto *stream = WrapAsStream(store, sgaData);
        auto sga = std::make_unique<CSgaFile>();
        sga->Load(stream);
        sga->VInit(stream);
        return sga;
    }
};

TEST_F(SgaFileLoadTest, LoadAndGetVersion)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();
    EXPECT_EQ(pSga->GetVersion(), 2u);
}

TEST_F(SgaFileLoadTest, EntryPointCount)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();
    EXPECT_EQ(pSga->VGetEntryPointCount(), 1u);
}

TEST_F(SgaFileLoadTest, EntryPointName)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();
    EXPECT_STREQ(pSga->VGetEntryPoint(0), "data");
}

TEST_F(SgaFileLoadTest, OpenUncompressedFile)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();

    auto *stream = pSga->VOpenStream("data\\test.txt");
    ASSERT_NE(stream, nullptr);

    char buf[64] = {};
    stream->VRead(12, 1, buf);
    EXPECT_STREQ(buf, "Hello World!");
    delete stream;
}

TEST_F(SgaFileLoadTest, OpenCompressedFile)
{
    BuildComplexArchive();
    auto pSga = LoadArchive();

    auto *stream = pSga->VOpenStream("data\\subdir\\hello.txt");
    ASSERT_NE(stream, nullptr);

    const std::string expected = "This is compressed content for testing zlib decompression!";
    std::vector<char> buf(expected.size() + 1, 0);
    stream->VRead(static_cast<unsigned long>(expected.size()), 1, buf.data());
    EXPECT_EQ(std::string(buf.data()), expected);
    delete stream;
}

TEST_F(SgaFileLoadTest, OpenNonexistentFileThrows)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();

    std::optional<CRainmanException> caught;
    try
    {
        pSga->VOpenStream("data\\nosuchfile.txt");
    }
    catch (const CRainmanException &e)
    {
        caught = e;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(SgaFileLoadTest, IterateRootDirectory)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();

    auto *itr = pSga->VIterate("data");
    ASSERT_NE(itr, nullptr);

    // Should find "test.txt" as a file
    EXPECT_EQ(itr->VGetType(), IDirectoryTraverser::IIterator::T_File);
    EXPECT_STREQ(itr->VGetName(), "test.txt");

    // No more items
    EXPECT_NE(itr->VNextItem(), IDirectoryTraverser::IIterator::E_OK);

    delete itr;
}

TEST_F(SgaFileLoadTest, IterateWithSubdirectory)
{
    BuildComplexArchive();
    auto pSga = LoadArchive();

    auto *itr = pSga->VIterate("data");
    ASSERT_NE(itr, nullptr);

    // First item should be the subdirectory "subdir"
    EXPECT_EQ(itr->VGetType(), IDirectoryTraverser::IIterator::T_Directory);
    EXPECT_STREQ(itr->VGetName(), "subdir");

    // Open the subdirectory and check its contents
    auto *subItr = itr->VOpenSubDir();
    ASSERT_NE(subItr, nullptr);
    EXPECT_EQ(subItr->VGetType(), IDirectoryTraverser::IIterator::T_File);
    EXPECT_STREQ(subItr->VGetName(), "hello.txt");
    delete subItr;

    // Next item: the file "test.txt"
    EXPECT_EQ(itr->VNextItem(), IDirectoryTraverser::IIterator::E_OK);
    EXPECT_EQ(itr->VGetType(), IDirectoryTraverser::IIterator::T_File);
    EXPECT_STREQ(itr->VGetName(), "test.txt");

    // No more items
    EXPECT_NE(itr->VNextItem(), IDirectoryTraverser::IIterator::E_OK);

    delete itr;
}

TEST_F(SgaFileLoadTest, IteratorOpenFile)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();

    auto *itr = pSga->VIterate("data");
    ASSERT_NE(itr, nullptr);
    ASSERT_EQ(itr->VGetType(), IDirectoryTraverser::IIterator::T_File);

    auto *stream = itr->VOpenFile();
    ASSERT_NE(stream, nullptr);
    char buf[64] = {};
    stream->VRead(12, 1, buf);
    EXPECT_STREQ(buf, "Hello World!");
    delete stream;
    delete itr;
}

TEST_F(SgaFileLoadTest, IteratorFullPath)
{
    BuildComplexArchive();
    auto pSga = LoadArchive();

    auto *itr = pSga->VIterate("data");
    ASSERT_NE(itr, nullptr);

    // First item is "subdir" directory
    EXPECT_EQ(itr->VGetType(), IDirectoryTraverser::IIterator::T_Directory);
    std::string fullPath = itr->VGetFullPath();
    EXPECT_NE(fullPath.find("subdir"), std::string::npos);

    delete itr;
}

TEST_F(SgaFileLoadTest, GetLastWriteTime)
{
    BuildSimpleArchive();
    auto *stream = WrapAsStream(store, sgaData);
    CSgaFile sga;
    tLastWriteTime writeTime = 42;
    sga.Load(stream, writeTime);
    sga.VInit(stream);

    EXPECT_EQ(sga.VGetLastWriteTime("data\\test.txt"), writeTime);
}

TEST_F(SgaFileLoadTest, MD5MismatchThrows)
{
    BuildSimpleArchive();

    // Corrupt a byte in the data header (after the fixed header, offset 180+10)
    sgaData[190] ^= 0xFF;

    auto *stream = WrapAsStream(store, sgaData);
    CSgaFile sga;
    std::optional<CRainmanException> caught;
    try
    {
        sga.Load(stream);
    }
    catch (const CRainmanException &e)
    {
        caught = e;
    }
    ASSERT_TRUE(caught.has_value());
    EXPECT_NE(std::strstr(caught->getMessage(), "MD5"), nullptr);
    delete stream;
}

TEST_F(SgaFileLoadTest, GetInputStreamAfterInit)
{
    BuildSimpleArchive();
    auto pSga = LoadArchive();
    EXPECT_NE(pSga->GetInputStream(), nullptr);
}
