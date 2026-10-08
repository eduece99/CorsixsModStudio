#include <gtest/gtest.h>
#include "rainman/lua/CLuaFile2.h"
#include "rainman/io/CMemoryStore.h"
#include "rainman/lua/CLuaFileCache.h"
#include <memory>
#include "rainman/core/Exception.h"
#include <cstring>
#include <string>
#include <optional>

class LuaReferenceMemoryStore : public CMemoryStore
{
  public:
    std::string requestedPath;
    std::string parentScript;

    IStream *VOpenStream(const char *sFile) override
    {
        requestedPath = sFile;
        return OpenStreamExt(parentScript.data(), static_cast<unsigned long>(parentScript.size()), false);
    }
};

class LuaFile2Test : public ::testing::Test {
protected:
	CLuaFile2 lua;
	LuaReferenceMemoryStore store;

	void SetUp() override { store.VInit(); }

	// Load a Lua script from a string
	void loadScript(const char *sScript, const char *sFileName = "test.lua")
	{
		std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
		    const_cast<char *>(sScript), (unsigned long)strlen(sScript), false));
		lua.loadFile(inStream.get(), &store, sFileName);
	}

	// Save and return the output as a string
	std::string saveToString(const char *sFileName = "test.lua")
	{
		auto *pOut = CMemoryStore::OpenOutputStreamExt();
		lua.saveFile(pOut, sFileName);
		std::string result(pOut->GetData(), pOut->GetDataLength());
		delete pOut;
		return result;
	}
};

TEST(LuaFile2Empty, DefaultConstructor)
{
	CLuaFile2 lua;
	// Should construct without crash
	SUCCEED();
}

TEST_F(LuaFile2Test, NewFileCreatesEmptyState)
{
	lua.newFile("empty.lua");

	auto *table = lua.asMetaTable();
	ASSERT_NE(table, nullptr);

	// Global table should have some entries (Lua builtins)
	EXPECT_GT(table->VGetChildCount(), 0u);
	delete table;
}

TEST_F(LuaFile2Test, NewFileRemovesDangerousFunctions)
{
	lua.newFile("safe.lua");

	auto *table = lua.asMetaTable();
	ASSERT_NE(table, nullptr);

	// Check that dangerous functions were removed
	bool foundDofile = false;
	bool foundLoadlib = false;
	for (unsigned long i = 0; i < table->VGetChildCount(); ++i)
	{
		auto *child = table->VGetChild(i);
		const char *name = child->VGetName();
		if (name && strcmp(name, "dofile") == 0)
			foundDofile = true;
		if (name && strcmp(name, "loadlib") == 0)
			foundLoadlib = true;
		delete child;
	}
	EXPECT_FALSE(foundDofile);
	EXPECT_FALSE(foundLoadlib);
	delete table;
}

TEST_F(LuaFile2Test, LoadSimpleGameData)
{
	const char *script = "GameData = { health = 100.0, name = \"soldier\" }";
	loadScript(script);

	auto *table = lua.asMetaTable();
	ASSERT_NE(table, nullptr);

	// Find GameData in globals
	bool foundGameData = false;
	for (unsigned long i = 0; i < table->VGetChildCount(); ++i)
	{
		auto *child = table->VGetChild(i);
		if (child->VGetName() && strcmp(child->VGetName(), "GameData") == 0)
		{
			foundGameData = true;
			EXPECT_EQ(child->VGetType(), IMetaNode::DT_Table);
			delete child;
			break;
		}
		delete child;
	}
	EXPECT_TRUE(foundGameData);
	delete table;
}

TEST_F(LuaFile2Test, LoadAndSaveRoundTrip)
{
	const char *script = "GameData = { damage = 25.5 }\nMetaData = {}";
	loadScript(script, "weapons.lua");

	std::string output = saveToString("weapons.lua");

	// Output should contain the file header
	EXPECT_NE(output.find("weapons.lua"), std::string::npos);
	EXPECT_NE(output.find("Corsix"), std::string::npos);

	// Output should contain GameData
	EXPECT_NE(output.find("GameData"), std::string::npos);
	EXPECT_NE(output.find("damage"), std::string::npos);
}

TEST_F(LuaFile2Test, LoadNestedTables)
{
	const char *script =
	    "GameData = {\n"
	    "  stats = {\n"
	    "    health = 100,\n"
	    "    armor = 50,\n"
	    "  },\n"
	    "}";
	loadScript(script);

	auto *table = lua.asMetaTable();
	ASSERT_NE(table, nullptr);

	// Find GameData and check it's a table
	for (unsigned long i = 0; i < table->VGetChildCount(); ++i)
	{
		auto *child = table->VGetChild(i);
		if (child->VGetName() && strcmp(child->VGetName(), "GameData") == 0)
		{
			EXPECT_EQ(child->VGetType(), IMetaNode::DT_Table);
			auto *gameData = child->VGetValueMetatable();
			ASSERT_NE(gameData, nullptr);
			EXPECT_GT(gameData->VGetChildCount(), 0u);
			delete gameData;
		}
		delete child;
	}
	delete table;
}

TEST_F(LuaFile2Test, SetCacheOwned)
{
	auto *cache = new CLuaFileCache();
	EXPECT_TRUE(lua.setCache(cache, true));
	// Second call should fail (cache already set)
	auto *cache2 = new CLuaFileCache();
	EXPECT_FALSE(lua.setCache(cache2, true));
	delete cache2;
	// The first cache is owned and will be deleted by CLuaFile2 destructor
}

TEST_F(LuaFile2Test, SetCacheNotOwned)
{
	CLuaFileCache cache;
	EXPECT_TRUE(lua.setCache(&cache, false));
}

TEST_F(LuaFile2Test, SetRootFolder)
{
	lua.setRootFolder("C:\\games\\dow");
	// Should not crash — just stores the folder
	lua.newFile("test.lua");
	SUCCEED();
}

TEST_F(LuaFile2Test, ParentReferenceUsesNativeSeparatorsOnLinux)
{
    store.parentScript = "GameData = { health = 100 }";
    lua.setRootFolder("generic\\attrib\\");
    loadScript("GameData = Inherit(\"parent.lua\")");

#ifdef RAINMAN_GNUC
    EXPECT_EQ(store.requestedPath, "generic/attrib/parent.lua");
#else
    EXPECT_EQ(store.requestedPath, "generic\\attrib\\parent.lua");
#endif
}

TEST_F(LuaFile2Test, LoadInvalidSyntaxThrows)
{
	const char *badScript = "GameData = { this is not valid lua !!!";
	std::optional<CRainmanException> caught;
	try
	{
		loadScript(badScript);
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	ASSERT_TRUE(caught.has_value());
}

TEST_F(LuaFile2Test, LoadNullStreamThrows)
{
	std::optional<CRainmanException> caught;
	try
	{
		lua.loadFile(nullptr, &store, "test.lua");
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	ASSERT_TRUE(caught.has_value());
}

TEST_F(LuaFile2Test, LoadNullStoreThrows)
{
	const char *script = "GameData = {}";
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(script), (unsigned long)strlen(script), false));

	std::optional<CRainmanException> caught;
	try
	{
		lua.loadFile(inStream.get(), nullptr, "test.lua");
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	ASSERT_TRUE(caught.has_value());
}

TEST_F(LuaFile2Test, LoadBoolAndStringTypes)
{
	const char *script =
	    "GameData = { enabled = true, label = \"hello\" }\nMetaData = {}";
	loadScript(script);

	std::string output = saveToString();
	EXPECT_NE(output.find("enabled"), std::string::npos);
	EXPECT_NE(output.find("label"), std::string::npos);
}
