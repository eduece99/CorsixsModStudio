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
#include <memory>
#include "../common/TestProcessId.h"
#include <string>

// Test fixture for CFileMap output stream (save) scenarios.
// Simulates the scenario where files exist only in SGA sources and the
// Data folder source may or may not be registered.
class FileMapOutputTest : public ::testing::Test
{
  protected:
    std::filesystem::path tempDir;

    void SetUp() override
    {
        tempDir = std::filesystem::temp_directory_path() /
                  ("filemap_output_test_" + std::to_string(GetTestProcessId()) + "_" +
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

    // Build a DirEntry snapshot with a single file at a nested path.
    // e.g. buildSnapshot("C:\\path", "subdir", "file.rgd") creates:
    //   root (directoryPath="C:\\path")
    //     └── subdir (directoryPath="C:\\path\\subdir")
    //           └── file.rgd
    CFileMap::DirEntry buildNestedSnapshot(const std::string &rootPath, const std::string &subdir,
                                           const std::string &filename)
    {
        CFileMap::DirEntry fileEntry;
        fileEntry.name = filename;
        fileEntry.isFile = true;
        fileEntry.lastWriteTime = 1000;

        CFileMap::DirEntry subdirEntry;
        subdirEntry.name = subdir;
        subdirEntry.isFile = false;
        subdirEntry.directoryPath = rootPath + "\\" + subdir;
        subdirEntry.lastWriteTime = 0;
        subdirEntry.children.push_back(std::move(fileEntry));

        CFileMap::DirEntry root;
        root.isFile = false;
        root.directoryPath = rootPath;
        root.lastWriteTime = 0;
        root.children.push_back(std::move(subdirEntry));

        return root;
    }
};

// When a file is mapped from an SGA source and a default-output folder source is
// registered (even with an empty snapshot), VOpenOutputStream should succeed by
// using _MakeFolderWritable to route the save to the Data folder and creating
// subdirectories on demand.
TEST_F(FileMapOutputTest, SaveToEmptyDataFolderSourceSucceeds)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    // Create the Data folder (empty) on disk so CFileSystemStore can write to it
    std::string dataFolderPath = filePath("Data");
    std::filesystem::create_directories(dataFolderPath);

    // 1. Register the Data folder source with bIsDefaultOutput=true, using an empty snapshot
    //    (simulates the case where the Data folder exists but is empty on disk)
    void *pDataSrc = fileMap.RegisterSource(
        0,     // modNumber (main mod)
        false, // bIsSga = false (folder source)
        15000, // iSourceNum
        "TestMod", "Data", &fsStore, &fsStore,
        true,  // bIsWritable
        true,  // bIsOutput (bIsDefaultOutput)
        false  // bIsPureOutput
    );
    ASSERT_NE(pDataSrc, nullptr);

    // Map the empty snapshot — this registers the root folder's mapSourceNames
    CFileMap::DirEntry emptySnapshot;
    emptySnapshot.isFile = false;
    emptySnapshot.directoryPath = dataFolderPath;
    emptySnapshot.lastWriteTime = 0;
    fileMap.MapSnapshot(pDataSrc, "Data", emptySnapshot);

    // 2. Register an "SGA" source with a file in a subdirectory
    //    (simulates content that exists only in the SGA archive)
    void *pSgaSrc = fileMap.RegisterSource(
        0,     // modNumber
        true,  // bIsSga = true
        15001, // iSourceNum
        "TestMod", "W40k.sga", &fsStore, &fsStore,
        false, // bIsWritable
        false, // bIsOutput
        false  // bIsPureOutput
    );
    ASSERT_NE(pSgaSrc, nullptr);

    // Create a fake "SGA directory" on disk so the file store can iterate it
    createFile("SgaData\\attrib\\test_file.rgd", "sga_content");
    std::string sgaDataPath = filePath("SgaData");

    CFileMap::DirEntry sgaSnapshot = buildNestedSnapshot(sgaDataPath, "attrib", "test_file.rgd");
    fileMap.MapSnapshot(pSgaSrc, "Data", sgaSnapshot);

    // 3. Try to save the file — this should succeed
    //    CFileMap routes through _MakeFolderWritable which finds the Data folder source,
    //    recursively creates "attrib" inside the Data folder, then VOpenOutputStream
    //    writes the file.
    IFileStore::IOutputStream *pOutStream = nullptr;
    ASSERT_NO_THROW(pOutStream = fileMap.VOpenOutputStream("Data\\attrib\\test_file.rgd", true));
    ASSERT_NE(pOutStream, nullptr);

    // Write some data to verify the stream works
    const char *testData = "saved_data";
    pOutStream->VWrite(1, static_cast<unsigned long>(strlen(testData)),
                       const_cast<void *>(static_cast<const void *>(testData)));
    delete pOutStream;

    // Verify the file was created in the Data folder (not the SGA folder)
    std::string savedFilePath = dataFolderPath + "\\attrib\\test_file.rgd";
    EXPECT_TRUE(std::filesystem::exists(savedFilePath)) << "File should be saved in Data folder: " << savedFilePath;
}

TEST_F(FileMapOutputTest, ReportsPhysicalFolderPathForVirtualFolder)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    const auto dataGenericPath = tempDir / "DataGeneric";
    const auto attribPath = dataGenericPath / "attrib";
    std::filesystem::create_directories(attribPath);
    std::ofstream(attribPath / "example.lua") << "GameData = {}";

    auto sourceIterator = std::unique_ptr<IDirectoryTraverser::IIterator>(
        fsStore.VIterate(dataGenericPath.string().c_str()));
    void *pSource = fileMap.RegisterSource(0, false, 0, "TestMod", "DataGeneric", &fsStore, &fsStore, false, false);
    fileMap.MapIterator(pSource, "Generic", sourceIterator.get());

    const auto sourcePaths = fileMap.GetSourceFolderPaths("Generic\\attrib\\");
    ASSERT_EQ(sourcePaths.size(), 1u);
    EXPECT_TRUE(std::filesystem::path(sourcePaths.front()).is_absolute());
    EXPECT_EQ(std::filesystem::path(sourcePaths.front()).lexically_normal(), attribPath.lexically_normal());
}

// When no default-output source is registered, VOpenOutputStream should throw
// with a meaningful error message.
TEST_F(FileMapOutputTest, SaveFailsWithoutDefaultOutputSource)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    // Register only an SGA source (no writable data folder)
    void *pSgaSrc = fileMap.RegisterSource(
        0, true, 15001, "TestMod", "W40k.sga", &fsStore, &fsStore,
        false, false, false);

    createFile("SgaData\\attrib\\test_file.rgd", "sga_content");
    CFileMap::DirEntry sgaSnapshot = buildNestedSnapshot(filePath("SgaData"), "attrib", "test_file.rgd");
    fileMap.MapSnapshot(pSgaSrc, "Data", sgaSnapshot);

    // Attempt to save should throw because no default output source exists
    EXPECT_THROW(fileMap.VOpenOutputStream("Data\\attrib\\test_file.rgd", true), CRainmanException);
}

// When the Data folder source is registered with an empty snapshot and the
// Data folder is created on disk (as LoadFoldersParallel now does), saving files
// that exist only in SGA sources should succeed. _MakeFolderWritable creates
// subdirectories within the Data folder via VCreateFolderIn.
TEST_F(FileMapOutputTest, SaveCreatesSubdirectoriesOnDemand)
{
    CFileMap fileMap;
    CFileSystemStore fsStore;
    fsStore.VInit();

    // Create the Data folder on disk (simulating what LoadFoldersParallel now does
    // via _mkdir when the folder was missing during VIterate)
    std::string dataFolderPath = filePath("Data");
    std::filesystem::create_directories(dataFolderPath);

    // Register the Data folder source with an empty snapshot pointing to non-existent path
    void *pDataSrc = fileMap.RegisterSource(
        0, false, 15000, "TestMod", "Data", &fsStore, &fsStore,
        true, true, false);

    CFileMap::DirEntry emptySnapshot;
    emptySnapshot.isFile = false;
    emptySnapshot.directoryPath = dataFolderPath;
    emptySnapshot.lastWriteTime = 0;
    fileMap.MapSnapshot(pDataSrc, "Data", emptySnapshot);

    // Register SGA source with a file
    void *pSgaSrc = fileMap.RegisterSource(
        0, true, 15001, "TestMod", "W40k.sga", &fsStore, &fsStore,
        false, false, false);

    createFile("SgaData\\attrib\\test_file.rgd", "sga_content");
    CFileMap::DirEntry sgaSnapshot = buildNestedSnapshot(filePath("SgaData"), "attrib", "test_file.rgd");
    fileMap.MapSnapshot(pSgaSrc, "Data", sgaSnapshot);

    // Save should succeed — CFileSystemStore creates missing directories
    IFileStore::IOutputStream *pOutStream = nullptr;
    ASSERT_NO_THROW(pOutStream = fileMap.VOpenOutputStream("Data\\attrib\\test_file.rgd", true));
    ASSERT_NE(pOutStream, nullptr);

    const char *testData = "on_demand_save";
    pOutStream->VWrite(1, static_cast<unsigned long>(strlen(testData)),
                       const_cast<void *>(static_cast<const void *>(testData)));
    delete pOutStream;

    // Verify the entire path was created
    std::string savedFilePath = dataFolderPath + "\\attrib\\test_file.rgd";
    EXPECT_TRUE(std::filesystem::exists(savedFilePath)) << "File should be created on demand: " << savedFilePath;
}
