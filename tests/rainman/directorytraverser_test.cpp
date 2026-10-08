#include <gtest/gtest.h>
#include "rainman/io/CFileSystemStore.h"
#include "rainman/core/Exception.h"
#include <cstring>
#include <filesystem>
#include "../common/TestProcessId.h"
#include <string>

class DirectoryTraverserTest : public ::testing::Test
{
  protected:
	CFileSystemStore store;
	std::filesystem::path tempDir;

	void SetUp() override
	{
		tempDir = std::filesystem::temp_directory_path() /
		    ("dirtraverser_test_" + std::to_string(GetTestProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
		std::filesystem::create_directories(tempDir);
		store.VInit();
	}

	void TearDown() override { std::filesystem::remove_all(tempDir); }
};

TEST_F(DirectoryTraverserTest, VDirectoryExistsReturnsTrueForExistingDir)
{
	std::string sPath = tempDir.string();
	EXPECT_TRUE(store.VDirectoryExists(sPath.c_str()));
}

TEST_F(DirectoryTraverserTest, VDirectoryExistsReturnsTrueForCreatedSubdir)
{
	std::filesystem::path subDir = tempDir / "newly_created";
	std::filesystem::create_directories(subDir);
	std::string sPath = subDir.string();
	EXPECT_TRUE(store.VDirectoryExists(sPath.c_str()));
}
