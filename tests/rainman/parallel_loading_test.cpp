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

#include <gtest/gtest.h>
#include "rainman/module/CFileMap.h"
#include "rainman/io/CFileSystemStore.h"
#include "rainman/io/CMemoryStore.h"
#include "rainman/localization/CUcsFile.h"
#include "rainman/core/Exception.h"
#include <filesystem>
#include <fstream>
#include <future>
#include "../common/TestProcessId.h"
#include <string>
#include <vector>

// -- MapSnapshot tests --

class MapSnapshotTest : public ::testing::Test
{
  protected:
    std::filesystem::path tempDir;

    void SetUp() override
    {
        tempDir = std::filesystem::temp_directory_path() /
                  ("mapsnapshot_test_" + std::to_string(GetTestProcessId()) + "_" +
                   std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override { std::filesystem::remove_all(tempDir); }

    void createFile(const std::string &relative, const std::string &content)
    {
        auto path = tempDir / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
};

TEST_F(MapSnapshotTest, MapSnapshotProducesSameResultAsMapIterator)
{
    // Create a directory tree
    createFile("data\\attrib\\file1.lua", "-- lua1");
    createFile("data\\attrib\\file2.lua", "-- lua2");
    createFile("data\\art\\icon.tga", "TGA");
    createFile("data\\readme.txt", "hello");

    CFileSystemStore fsStore;
    fsStore.VInit();

    std::string dataPath = (tempDir / "data").string();

    // Method 1: MapIterator (the old way)
    CFileMap map1;
    {
        auto *pSrc = map1.RegisterSource(15000, false, 0, "TestMod", "Data", &fsStore, &fsStore, false, false);
        auto *pItr = fsStore.VIterate(dataPath.c_str());
        ASSERT_NE(pItr, nullptr);
        map1.MapIterator(pSrc, "Data", pItr);
        delete pItr;
    }

    // Method 2: MapSnapshot (the new way — pre-scan then replay)
    CFileMap map2;
    CFileMap::DirEntry snapshot;
    {
        auto *pItr = fsStore.VIterate(dataPath.c_str());
        ASSERT_NE(pItr, nullptr);

        // Manually build snapshot from iterator (simulating ScanDirectory)
        snapshot.isFile = false;
        snapshot.lastWriteTime = 0;
        snapshot.directoryPath = pItr->VGetDirectoryPath();

        std::function<CFileMap::DirEntry(IDirectoryTraverser::IIterator *)> scanDir =
            [&scanDir](IDirectoryTraverser::IIterator *it) -> CFileMap::DirEntry {
            CFileMap::DirEntry dir;
            dir.isFile = false;
            dir.lastWriteTime = 0;
            dir.directoryPath = it->VGetDirectoryPath();
            while (it->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
            {
                CFileMap::DirEntry entry;
                entry.name = it->VGetName();
                if (it->VGetType() == IDirectoryTraverser::IIterator::T_File)
                {
                    entry.isFile = true;
                    entry.lastWriteTime = it->VGetLastWriteTime();
                }
                else
                {
                    entry.isFile = false;
                    entry.lastWriteTime = 0;
                    auto *sub = it->VOpenSubDir();
                    if (sub)
                    {
                        entry = scanDir(sub);
                        entry.name = it->VGetName();
                        delete sub;
                    }
                }
                dir.children.push_back(std::move(entry));
                if (it->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
                    break;
            }
            return dir;
        };

        snapshot = scanDir(pItr);
        delete pItr;

        auto *pSrc = map2.RegisterSource(15000, false, 0, "TestMod", "Data", &fsStore, &fsStore, false, false);
        map2.MapSnapshot(pSrc, "Data", snapshot);
    }

    // Both maps should produce the same file listing
    map1.VInit();
    map2.VInit();

    EXPECT_EQ(map1.VGetEntryPointCount(), map2.VGetEntryPointCount());

    // Iterate the first ToC and verify files exist in both
    if (map1.VGetEntryPointCount() > 0)
    {
        const char *sToc = map1.VGetEntryPoint(0);
        auto *pItr1 = map1.VIterate(sToc);
        auto *pItr2 = map2.VIterate(sToc);
        ASSERT_NE(pItr1, nullptr);
        ASSERT_NE(pItr2, nullptr);

        // Count entries in both
        int count1 = 0, count2 = 0;
        while (pItr1->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
        {
            ++count1;
            if (pItr1->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
                break;
        }
        while (pItr2->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
        {
            ++count2;
            if (pItr2->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
                break;
        }
        EXPECT_EQ(count1, count2);

        delete pItr1;
        delete pItr2;
    }
}

TEST_F(MapSnapshotTest, MapSnapshotEmptyDirectory)
{
    std::filesystem::create_directories(tempDir / "empty");

    CFileSystemStore fsStore;
    fsStore.VInit();

    CFileMap fileMap;
    auto *pSrc = fileMap.RegisterSource(15000, false, 0, "Mod", "Empty", &fsStore, &fsStore, false, false);

    CFileMap::DirEntry snapshot;
    snapshot.isFile = false;
    snapshot.lastWriteTime = 0;
    snapshot.directoryPath = (tempDir / "empty").string();
    // No children

    fileMap.MapSnapshot(pSrc, "Data", snapshot);
    fileMap.VInit();

    EXPECT_GE(fileMap.VGetEntryPointCount(), 1u);
}

TEST_F(MapSnapshotTest, MapSnapshotDeepNesting)
{
    createFile("deep\\a\\b\\c\\d\\leaf.txt", "deep content");

    CFileSystemStore fsStore;
    fsStore.VInit();

    CFileMap fileMap;
    auto *pSrc = fileMap.RegisterSource(15000, false, 0, "Mod", "Deep", &fsStore, &fsStore, false, false);

    std::string deepPath = (tempDir / "deep").string();

    // Build nested snapshot with correct directoryPath values for each folder
    CFileMap::DirEntry leaf;
    leaf.name = "leaf.txt";
    leaf.isFile = true;
    leaf.lastWriteTime = 0;

    CFileMap::DirEntry d;
    d.name = "d";
    d.isFile = false;
    d.lastWriteTime = 0;
    d.directoryPath = (tempDir / "deep" / "a" / "b" / "c" / "d").string();
    d.children.push_back(leaf);

    CFileMap::DirEntry c;
    c.name = "c";
    c.isFile = false;
    c.lastWriteTime = 0;
    c.directoryPath = (tempDir / "deep" / "a" / "b" / "c").string();
    c.children.push_back(d);

    CFileMap::DirEntry b;
    b.name = "b";
    b.isFile = false;
    b.lastWriteTime = 0;
    b.directoryPath = (tempDir / "deep" / "a" / "b").string();
    b.children.push_back(c);

    CFileMap::DirEntry a;
    a.name = "a";
    a.isFile = false;
    a.lastWriteTime = 0;
    a.directoryPath = (tempDir / "deep" / "a").string();
    a.children.push_back(b);

    CFileMap::DirEntry root;
    root.isFile = false;
    root.lastWriteTime = 0;
    root.directoryPath = deepPath;
    root.children.push_back(a);

    fileMap.MapSnapshot(pSrc, "Data", root);
    fileMap.VInit();

    // Should be able to find the deep file
    auto *pStream = fileMap.VOpenStream("Data\\a\\b\\c\\d\\leaf.txt");
    ASSERT_NE(pStream, nullptr);
    delete pStream;
}

// -- Parallel UCS loading tests --

TEST(ParallelUcsLoading, IndependentCUcsFileInstancesLoadConcurrently)
{
    // Create several independent CUcsFile instances loading different data in parallel
    constexpr int kNumFiles = 8;

    // Build UCS data buffers (each with BOM + one line: "<id>\tValue_<id>\r\n")
    struct UcsData
    {
        std::vector<char> buffer;
    };
    std::vector<UcsData> dataBuffers(kNumFiles);

    for (int i = 0; i < kNumFiles; ++i)
    {
        CMemoryStore store;
        store.VInit();
        auto *out = CMemoryStore::OpenOutputStreamExt();

        unsigned short bom = 0xFEFF;
        out->VWrite(1, sizeof(unsigned short), &bom);

        std::wstring line = std::to_wstring(i + 100) + L"\tValue_" + std::to_wstring(i) + L"\r\n";
        out->VWrite(static_cast<unsigned long>(line.size()), sizeof(wchar_t), line.c_str());

        dataBuffers[i].buffer.assign(out->GetData(), out->GetData() + out->GetDataLength());
        delete out;
    }

    // Launch parallel loads
    std::vector<std::future<std::unique_ptr<CUcsFile>>> futures;
    for (int i = 0; i < kNumFiles; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&dataBuffers, i]() {
            auto ucs = std::make_unique<CUcsFile>();
            auto *stream = CMemoryStore::OpenStreamExt(
                const_cast<char *>(dataBuffers[i].buffer.data()),
                static_cast<unsigned long>(dataBuffers[i].buffer.size()), false);
            ucs->Load(stream);
            delete stream;
            return ucs;
        }));
    }

    // Collect and verify
    for (int i = 0; i < kNumFiles; ++i)
    {
        auto ucs = futures[i].get();
        ASSERT_NE(ucs, nullptr);
        const wchar_t *val = ucs->ResolveStringID(static_cast<unsigned long>(i + 100));
        ASSERT_NE(val, nullptr) << "Missing string ID " << (i + 100);
        std::wstring expected = L"Value_" + std::to_wstring(i);
        EXPECT_EQ(std::wstring(val), expected);
    }
}

TEST(ParallelUcsLoading, ConcurrentLoadDoesNotCorruptOtherInstances)
{
    // Stress test: 32 UCS instances loaded in parallel, each with multiple entries
    constexpr int kNumFiles = 32;
    constexpr int kEntriesPerFile = 10;

    struct UcsData
    {
        std::vector<char> buffer;
    };
    std::vector<UcsData> dataBuffers(kNumFiles);

    for (int i = 0; i < kNumFiles; ++i)
    {
        auto *out = CMemoryStore::OpenOutputStreamExt();

        unsigned short bom = 0xFEFF;
        out->VWrite(1, sizeof(unsigned short), &bom);

        for (int j = 0; j < kEntriesPerFile; ++j)
        {
            unsigned long id = static_cast<unsigned long>(i * 1000 + j);
            std::wstring line = std::to_wstring(id) + L"\tEntry_" + std::to_wstring(i) + L"_" +
                                std::to_wstring(j) + L"\r\n";
            out->VWrite(static_cast<unsigned long>(line.size()), sizeof(wchar_t), line.c_str());
        }

        dataBuffers[i].buffer.assign(out->GetData(), out->GetData() + out->GetDataLength());
        delete out;
    }

    std::vector<std::future<std::unique_ptr<CUcsFile>>> futures;
    for (int i = 0; i < kNumFiles; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&dataBuffers, i]() {
            auto ucs = std::make_unique<CUcsFile>();
            auto *stream = CMemoryStore::OpenStreamExt(
                const_cast<char *>(dataBuffers[i].buffer.data()),
                static_cast<unsigned long>(dataBuffers[i].buffer.size()), false);
            ucs->Load(stream);
            delete stream;
            return ucs;
        }));
    }

    for (int i = 0; i < kNumFiles; ++i)
    {
        auto ucs = futures[i].get();
        ASSERT_NE(ucs, nullptr);
        for (int j = 0; j < kEntriesPerFile; ++j)
        {
            unsigned long id = static_cast<unsigned long>(i * 1000 + j);
            const wchar_t *val = ucs->ResolveStringID(id);
            ASSERT_NE(val, nullptr) << "Missing string ID " << id << " in file " << i;
            std::wstring expected = L"Entry_" + std::to_wstring(i) + L"_" + std::to_wstring(j);
            EXPECT_EQ(std::wstring(val), expected);
        }
    }
}

// -- VRead direct-to-destination tests (P2) --

TEST(VReadDirect, ReadsDirectlyIntoDestinationBuffer)
{
    // Verify that VRead reads into the provided destination buffer correctly
    // (no intermediate temp buffer allocation)
    auto tempDir = std::filesystem::temp_directory_path() /
                   ("vread_test_" + std::to_string(GetTestProcessId()));
    std::filesystem::create_directories(tempDir);

    // Create a test file with known content
    auto filePath = tempDir / "testdata.bin";
    {
        std::ofstream ofs(filePath, std::ios::binary);
        const char data[] = "Hello, World! VRead test data 1234567890";
        ofs.write(data, sizeof(data) - 1);
    }

    CFileSystemStore fsStore;
    fsStore.VInit();

    auto *pStream = fsStore.VOpenStream(filePath.string().c_str());
    ASSERT_NE(pStream, nullptr);

    // Read in several chunk sizes to exercise VRead
    char buf1[5] = {};
    pStream->VRead(5, 1, buf1);
    EXPECT_EQ(std::string(buf1, 5), "Hello");

    char buf2[8] = {};
    pStream->VRead(8, 1, buf2);
    EXPECT_EQ(std::string(buf2, 8), ", World!");

    // Read with item size > 1 (simulating uint16_t reads)
    char buf3[4] = {};
    pStream->VRead(2, 2, buf3);
    EXPECT_EQ(std::string(buf3, 4), " VRe");

    delete pStream;
    std::filesystem::remove_all(tempDir);
}

TEST(VReadDirect, LargeReadWorksCorrectly)
{
    auto tempDir = std::filesystem::temp_directory_path() /
                   ("vread_large_" + std::to_string(GetTestProcessId()));
    std::filesystem::create_directories(tempDir);

    auto filePath = tempDir / "large.bin";
    {
        std::ofstream ofs(filePath, std::ios::binary);
        // Write 64KB of sequential bytes
        std::vector<char> data(65536);
        for (size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<char>(i & 0xFF);
        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    CFileSystemStore fsStore;
    fsStore.VInit();

    auto *pStream = fsStore.VOpenStream(filePath.string().c_str());
    ASSERT_NE(pStream, nullptr);

    std::vector<char> result(65536);
    pStream->VRead(65536, 1, result.data());

    for (size_t i = 0; i < result.size(); ++i)
    {
        ASSERT_EQ(static_cast<unsigned char>(result[i]), static_cast<unsigned char>(i & 0xFF))
            << "Mismatch at byte " << i;
    }

    delete pStream;
    std::filesystem::remove_all(tempDir);
}

// -- Bulk UCS loading correctness tests (P1) --

TEST(BulkUcsLoading, LoadProducesCorrectEntries)
{
    // Verify bulk loading parses UCS file correctly with multiple entries
    auto *out = CMemoryStore::OpenOutputStreamExt();

    unsigned short bom = 0xFEFF;
    out->VWrite(1, sizeof(unsigned short), &bom);

    // Write multiple UCS entries with varying content
    struct TestEntry
    {
        unsigned long id;
        std::wstring value;
    };
    std::vector<TestEntry> entries = {
        {1, L"Hello World"},
        {42, L"Test Value"},
        {100, L"Unicode: \u00E9\u00E8\u00EA"}, // e-acute, e-grave, e-circumflex
        {9999, L"Last entry"},
    };

    for (const auto &entry : entries)
    {
        std::wstring line = std::to_wstring(entry.id) + L"\t" + entry.value + L"\r\n";
        out->VWrite(static_cast<unsigned long>(line.size()), sizeof(wchar_t), line.c_str());
    }

    CUcsFile ucs;
    auto *stream = CMemoryStore::OpenStreamExt(const_cast<char*>(out->GetData()),
                                                static_cast<unsigned long>(out->GetDataLength()), false);
    ucs.Load(stream);
    delete stream;
    delete out;

    // Verify all entries
    for (const auto &entry : entries)
    {
        const wchar_t *val = ucs.ResolveStringID(entry.id);
        ASSERT_NE(val, nullptr) << "Missing ID " << entry.id;
        EXPECT_EQ(std::wstring(val), entry.value);
    }

    // Verify GetRawMap has correct count
    EXPECT_EQ(ucs.GetRawMap().Size(), entries.size());
}

TEST(BulkUcsLoading, LoadHandlesEmptyFile)
{
    // UCS file with only BOM, no entries
    auto *out = CMemoryStore::OpenOutputStreamExt();

    unsigned short bom = 0xFEFF;
    out->VWrite(1, sizeof(unsigned short), &bom);

    CUcsFile ucs;
    auto *stream = CMemoryStore::OpenStreamExt(const_cast<char*>(out->GetData()),
                                                static_cast<unsigned long>(out->GetDataLength()), false);
    ucs.Load(stream);
    delete stream;
    delete out;

    EXPECT_EQ(ucs.GetRawMap().Size(), 0u);
}

TEST(BulkUcsLoading, LoadHandlesLineWithoutTab)
{
    // Lines without a tab should be skipped (no ID/value separator)
    auto *out = CMemoryStore::OpenOutputStreamExt();

    unsigned short bom = 0xFEFF;
    out->VWrite(1, sizeof(unsigned short), &bom);

    // Valid entry
    std::wstring line1 = L"1\tValid Entry\r\n";
    out->VWrite(static_cast<unsigned long>(line1.size()), sizeof(wchar_t), line1.c_str());

    // Invalid line (no tab)
    std::wstring line2 = L"NoTabHere\r\n";
    out->VWrite(static_cast<unsigned long>(line2.size()), sizeof(wchar_t), line2.c_str());

    // Another valid entry
    std::wstring line3 = L"2\tAnother Entry\r\n";
    out->VWrite(static_cast<unsigned long>(line3.size()), sizeof(wchar_t), line3.c_str());

    CUcsFile ucs;
    auto *stream = CMemoryStore::OpenStreamExt(const_cast<char*>(out->GetData()),
                                                static_cast<unsigned long>(out->GetDataLength()), false);
    ucs.Load(stream);
    delete stream;
    delete out;

    EXPECT_NE(ucs.ResolveStringID(1), nullptr);
    EXPECT_NE(ucs.ResolveStringID(2), nullptr);
    // The invalid line should not crash or corrupt other entries
    EXPECT_GE(ucs.GetRawMap().Size(), 2u);
}

TEST(BulkUcsLoading, LoadHandlesLargeFile)
{
    // Stress test: 10,000 entries to verify bulk reading at scale
    constexpr int kNumEntries = 10000;

    auto *out = CMemoryStore::OpenOutputStreamExt();

    unsigned short bom = 0xFEFF;
    out->VWrite(1, sizeof(unsigned short), &bom);

    for (int i = 0; i < kNumEntries; ++i)
    {
        std::wstring line = std::to_wstring(i) + L"\tEntry number " + std::to_wstring(i) + L"\r\n";
        out->VWrite(static_cast<unsigned long>(line.size()), sizeof(wchar_t), line.c_str());
    }

    CUcsFile ucs;
    auto *stream = CMemoryStore::OpenStreamExt(const_cast<char*>(out->GetData()),
                                                static_cast<unsigned long>(out->GetDataLength()), false);
    ucs.Load(stream);
    delete stream;
    delete out;

    EXPECT_EQ(ucs.GetRawMap().Size(), static_cast<size_t>(kNumEntries));

    // Spot-check a few entries
    for (int i : {0, 1, 999, 5000, 9999})
    {
        const wchar_t *val = ucs.ResolveStringID(static_cast<unsigned long>(i));
        ASSERT_NE(val, nullptr) << "Missing ID " << i;
        std::wstring expected = L"Entry number " + std::to_wstring(i);
        EXPECT_EQ(std::wstring(val), expected);
    }
}
