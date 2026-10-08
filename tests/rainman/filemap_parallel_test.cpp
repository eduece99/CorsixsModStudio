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
#include "rainman/core/Exception.h"
#include <filesystem>
#include <fstream>
#include <future>
#include "../common/TestProcessId.h"
#include <string>
#include <thread>
#include <vector>

class FileMapParallelTest : public ::testing::Test
{
  protected:
    std::filesystem::path tempDir;

    void SetUp() override
    {
        tempDir = std::filesystem::temp_directory_path() /
                  ("filemap_parallel_test_" + std::to_string(GetTestProcessId()) + "_" +
                   std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override { std::filesystem::remove_all(tempDir); }

    std::string filePath(const std::string &relative) { return (tempDir / relative).string(); }

    void createFile(const std::string &relative, const std::string &content)
    {
        auto path = tempDir / relative;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
};

// Verify that concurrent RegisterSource calls don't corrupt CFileMap's internal state.
TEST_F(FileMapParallelTest, ConcurrentRegisterSourceNoCorruption)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    constexpr int kNumSources = 32;
    std::vector<std::future<void *>> futures;
    futures.reserve(kNumSources);

    for (int i = 0; i < kNumSources; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&fileMap, &fsStore, i]() {
            std::string modName = "mod_" + std::to_string(i);
            std::string sourceName = "source_" + std::to_string(i);
            return fileMap.RegisterSource(static_cast<unsigned short>(15000 + i), (i % 2) == 0,
                                          static_cast<unsigned short>(i), modName.c_str(), sourceName.c_str(), &fsStore,
                                          &fsStore, false, false);
        }));
    }

    // Collect all results — no crash or deadlock expected
    std::vector<void *> sources;
    for (auto &f : futures)
    {
        void *pSrc = f.get();
        ASSERT_NE(pSrc, nullptr);
        sources.push_back(pSrc);
    }

    // All sources should be unique
    std::sort(sources.begin(), sources.end());
    auto it = std::unique(sources.begin(), sources.end());
    EXPECT_EQ(it, sources.end()) << "RegisterSource returned duplicate pointers";
}

// Verify that concurrent RegisterSource followed by sequential MapIterator works.
TEST_F(FileMapParallelTest, RegisterSourceThenMapIteratorSequential)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    // Create a small directory to iterate
    createFile("testfolder\\file1.txt", "content1");
    createFile("testfolder\\file2.txt", "content2");

    // Register two sources in parallel
    auto f1 = std::async(std::launch::async, [&]() {
        return fileMap.RegisterSource(15000, false, 0, "Mod1", "Source1", &fsStore, &fsStore, false, false);
    });
    auto f2 = std::async(std::launch::async, [&]() {
        return fileMap.RegisterSource(15001, false, 1, "Mod2", "Source2", &fsStore, &fsStore, false, false);
    });

    void *pSrc1 = f1.get();
    void *pSrc2 = f2.get();
    ASSERT_NE(pSrc1, nullptr);
    ASSERT_NE(pSrc2, nullptr);
    EXPECT_NE(pSrc1, pSrc2);

    // Map an iterator into the first source (sequential)
    auto *pIter = fsStore.VIterate(filePath("testfolder").c_str());
    if (pIter)
    {
        fileMap.MapIterator(pSrc1, "TestToc", pIter);
        delete pIter;
    }

    // Verify the file map has content
    fileMap.VInit();
    EXPECT_GE(fileMap.VGetEntryPointCount(), 1u);
}

// Stress test: many threads registering sources simultaneously
TEST_F(FileMapParallelTest, StressTestManyThreads)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    constexpr int kNumThreads = 100;
    std::vector<std::future<void *>> futures;
    futures.reserve(kNumThreads);

    for (int i = 0; i < kNumThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&fileMap, &fsStore, i]() {
            std::string modName = "stress_mod_" + std::to_string(i);
            std::string sourceName = "stress_source_" + std::to_string(i);
            return fileMap.RegisterSource(static_cast<unsigned short>(i % 30000), (i % 3) == 0,
                                          static_cast<unsigned short>(i), modName.c_str(), sourceName.c_str(), &fsStore,
                                          &fsStore, false, false);
        }));
    }

    int successCount = 0;
    for (auto &f : futures)
    {
        void *pSrc = f.get();
        if (pSrc)
            ++successCount;
    }
    EXPECT_EQ(successCount, kNumThreads);
}
