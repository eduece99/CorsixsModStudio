#include <gtest/gtest.h>
#include "rainman/archive/CSgaCreator.h"
#include "rainman/archive/CSgaFile.h"
#include "rainman/io/CFileSystemStore.h"
#include "rainman/io/CMemoryStore.h"
#include <memory>
#include "rainman/core/Exception.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include "../common/TestProcessId.h"
#include <string>
#include <optional>

class SgaCreatorTest : public ::testing::Test {
protected:
	std::filesystem::path tempDir;

	void SetUp() override
	{
		tempDir = std::filesystem::temp_directory_path() /
		          ("sgacreator_test_" + std::to_string(GetTestProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
		std::filesystem::create_directories(tempDir);
	}

	void TearDown() override { std::filesystem::remove_all(tempDir); }

	std::string filePath(const std::string &relative)
	{
		return (tempDir / relative).string();
	}

	// Create a file with the given content
	void createFile(const std::string &relative, const std::string &content)
	{
		auto path = tempDir / relative;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream ofs(path, std::ios::binary);
		ofs.write(content.data(), content.size());
	}

	// Read all contents from an IStream
	std::string readStream(IFileStore::IStream *pStream)
	{
		std::string result;
		char buf[256];
		try
		{
			while (true)
			{
				pStream->VRead(1, 1, buf);
				result.push_back(buf[0]);
			}
		}
		catch (const CRainmanException &pE)
		{
		}
		return result;
	}
};

TEST_F(SgaCreatorTest, CreateAndReadBackVersion4)
{
	// Create test files
	createFile("input\\readme.txt", "Hello SGA");
	createFile("input\\subdir\\data.bin", "BinaryContent");

	// Create SGA
	CFileSystemStore fsStore;
	fsStore.VInit();
	auto *pIter = fsStore.VIterate(filePath("input").c_str());
	ASSERT_NE(pIter, nullptr);

	std::string sgaPath = filePath("test_v4.sga");
	CSgaCreator::CreateSga(pIter, &fsStore, "data", sgaPath.c_str(), 4, "TestArchive", "testtoc");
	delete pIter;

	// Load the SGA
	CFileSystemStore readStore;
	readStore.VInit();
	std::unique_ptr<IFileStore::IStream> sgaStream(readStore.VOpenStream(sgaPath.c_str()));
	ASSERT_TRUE(sgaStream);

	CSgaFile sga;
	sga.Load(sgaStream.get());
	sga.VInit(sgaStream.get());

	EXPECT_EQ(sga.GetVersion(), 4u);

	// Iterate and verify contents
	auto *pSgaIter = sga.VIterate("data");
	ASSERT_NE(pSgaIter, nullptr);

	bool foundReadme = false;
	bool foundSubdir = false;

	do
	{
		if (pSgaIter->VGetType() == IDirectoryTraverser::IIterator::T_File)
		{
			if (strcmp(pSgaIter->VGetName(), "readme.txt") == 0)
			{
				foundReadme = true;
				std::unique_ptr<IFileStore::IStream> fileStream(pSgaIter->VOpenFile());
				std::string content = readStream(fileStream.get());
				EXPECT_EQ(content, "Hello SGA");
			}
		}
		else if (pSgaIter->VGetType() == IDirectoryTraverser::IIterator::T_Directory)
		{
			if (strcmp(pSgaIter->VGetName(), "subdir") == 0)
			{
				foundSubdir = true;
				auto *pSubIter = pSgaIter->VOpenSubDir();
				ASSERT_NE(pSubIter, nullptr);

				bool foundDataBin = false;
				do
				{
					if (pSubIter->VGetType() == IDirectoryTraverser::IIterator::T_File &&
					    strcmp(pSubIter->VGetName(), "data.bin") == 0)
					{
						foundDataBin = true;
						std::unique_ptr<IFileStore::IStream> fileStream(pSubIter->VOpenFile());
						std::string content = readStream(fileStream.get());
						EXPECT_EQ(content, "BinaryContent");
					}
				} while (pSubIter->VNextItem() == IDirectoryTraverser::IIterator::E_OK);
				delete pSubIter;

				EXPECT_TRUE(foundDataBin);
			}
		}
	} while (pSgaIter->VNextItem() == IDirectoryTraverser::IIterator::E_OK);
	delete pSgaIter;

	EXPECT_TRUE(foundReadme);
	EXPECT_TRUE(foundSubdir);
}

TEST_F(SgaCreatorTest, CreateAndReadBackVersion2)
{
	createFile("input\\hello.txt", "DoW Archive");

	CFileSystemStore fsStore;
	fsStore.VInit();
	auto *pIter = fsStore.VIterate(filePath("input").c_str());
	ASSERT_NE(pIter, nullptr);

	std::string sgaPath = filePath("test_v2.sga");
	CSgaCreator::CreateSga(pIter, &fsStore, "data", sgaPath.c_str(), 2, "DoWArchive", "dowtoc");
	delete pIter;

	// Verify file was created
	EXPECT_TRUE(std::filesystem::exists(sgaPath));
	EXPECT_GT(std::filesystem::file_size(sgaPath), 0u);

	// Load header and verify version
	CFileSystemStore readStore;
	readStore.VInit();
	std::unique_ptr<IFileStore::IStream> sgaStream(readStore.VOpenStream(sgaPath.c_str()));
	ASSERT_TRUE(sgaStream);

	CSgaFile sga;
	sga.Load(sgaStream.get());
	EXPECT_EQ(sga.GetVersion(), 2u);
}

TEST_F(SgaCreatorTest, InvalidVersionThrows)
{
	createFile("input\\file.txt", "test");

	CFileSystemStore fsStore;
	fsStore.VInit();
	auto *pIter = fsStore.VIterate(filePath("input").c_str());
	ASSERT_NE(pIter, nullptr);

	std::string sgaPath = filePath("test_bad.sga");
	std::optional<CRainmanException> caught;
	try
	{
		CSgaCreator::CreateSga(pIter, &fsStore, "data", sgaPath.c_str(), 99);
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	delete pIter;

	ASSERT_TRUE(caught.has_value());
}

TEST_F(SgaCreatorTest, NullDirectoryThrows)
{
	CFileSystemStore fsStore;
	fsStore.VInit();

	std::optional<CRainmanException> caught;
	try
	{
		CSgaCreator::CreateSga(nullptr, &fsStore, "data", "output.sga", 4);
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	ASSERT_TRUE(caught.has_value());
}

TEST_F(SgaCreatorTest, NullStoreThrows)
{
	createFile("input\\file.txt", "test");

	CFileSystemStore fsStore;
	fsStore.VInit();
	auto *pIter = fsStore.VIterate(filePath("input").c_str());
	ASSERT_NE(pIter, nullptr);

	std::optional<CRainmanException> caught;
	try
	{
		CSgaCreator::CreateSga(pIter, nullptr, "data", "output.sga", 4);
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	delete pIter;

	ASSERT_TRUE(caught.has_value());
}
