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
#include "services/CDEModGenerator.h"
#include "rainman/module/CModuleFile.h"
#include "rainman/lua/Lua51.h"
#include "DefaultBurnTemplate.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <process.h>
#include <utility>

namespace
{
std::string ReadFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, {}};
}

class DEModGeneratorTest : public ::testing::Test
{
  protected:
    std::filesystem::path m_root;

    void SetUp() override
    {
        m_root = std::filesystem::temp_directory_path() /
                 ("de_mod_generator_" + std::to_string(_getpid()) + "_" +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(m_root);
    }

    void TearDown() override { std::filesystem::remove_all(m_root); }

    CDEModGenerator::Options StarterOptions() const
    {
        return {"ExampleMod", "Example Mod", "An example mod", "DoWDE"};
    }
};
} // namespace

TEST_F(DEModGeneratorTest, CreatesCompleteProjectAndLoadableModule)
{
    const auto options = StarterOptions();
    const auto modulePath = CDEModGenerator::Create(m_root, options);
    const auto root = m_root / "ExampleMod";
    EXPECT_EQ(modulePath, root / "ExampleMod.module");
    for (const auto &directory : {"DataGeneric", "DataGeneric/Sound", "DataSrc", "DataIntermediate", "Mod/Data",
                                  "Mod/Locale/English"})
    {
        EXPECT_TRUE(std::filesystem::is_directory(root / directory)) << directory;
    }
    EXPECT_EQ(ReadFile(root / "DataGeneric" / "_default.burn"), kDefaultBurnTemplate);
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "DataGeneric" / "Sound" / "_default.rat"));
    EXPECT_EQ(std::filesystem::file_size(root / "DataGeneric" / "Sound" / "_default.rat"), 0u);
    EXPECT_TRUE(std::filesystem::is_regular_file(root / "Mod" / "Data.sgaconfig"));
    const auto config = ReadFile(root / "Mod" / "Data.sgaconfig");
    EXPECT_NE(config.find("TOCStart alias=\"Data\" relativeroot=\"Data\""), std::string::npos);
    EXPECT_NE(config.find("FileSettingsStart defcompression=\"1\""), std::string::npos);
    EXPECT_NE(config.find("Override wildcard=\".*$\" minsize=\"-1\" maxsize=\"100\" ct=\"0\""),
              std::string::npos);
    EXPECT_NE(config.find("Override wildcard=\".*$\" minsize=\"100\" maxsize=\"4096\" ct=\"2\""),
              std::string::npos);
    EXPECT_NE(config.find("Override wildcard=\".*(ttf)|(rgt)$\" minsize=\"-1\" maxsize=\"-1\" ct=\"0\""),
              std::string::npos);
    EXPECT_NE(config.find("Override wildcard=\".*(lua)$\" minsize=\"-1\" maxsize=\"-1\" ct=\"2\""),
              std::string::npos);

    const auto pipeline = ReadFile(root / "pipeline.ini");
    EXPECT_NE(pipeline.find("[project:ExampleMod]\r\n"), std::string::npos);
    EXPECT_NE(pipeline.find("Parent = DoWDE\r\n"), std::string::npos);
    EXPECT_NE(pipeline.find("DataFinal = Mod\\Data\r\n"), std::string::npos);
    EXPECT_NE(pipeline.find("[project:DoWDE]\r\n"), std::string::npos);
    EXPECT_NE(pipeline.find("[project:Engine]\r\n"), std::string::npos);

    CModuleFile mod;
    const auto path = modulePath.string();
    mod.LoadModuleFile(path.c_str());
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
    EXPECT_STREQ(mod.GetUiName(), "Example Mod");
    EXPECT_STREQ(mod.GetDescription(), "An example mod");
    EXPECT_STREQ(mod.GetModFolder(), "Mod");
    ASSERT_EQ(mod.GetFolderCount(), 1u);
    EXPECT_STREQ(mod.GetFolder(0)->GetName(), "Data");
    ASSERT_EQ(mod.GetArchiveCount(), 1u);
    EXPECT_STREQ(mod.GetArchive(0)->GetFileName(), "Data.sga");
    ASSERT_EQ(mod.GetRequiredCount(), 5u);
    for (const auto &[index, name] : {std::pair{0u, "DoWDE"}, {1u, "DXP3"}, {2u, "DXP2"},
                                     {3u, "WXP"}, {4u, "W40k"}})
    {
        EXPECT_STREQ(mod.GetRequired(index)->GetFileName(), name);
    }
}

TEST_F(DEModGeneratorTest, EachParentMatchesRequiredModChain)
{
    auto options = StarterOptions();
    const char *parents[] = {"DXP3", "DXP2", "WXP", "W40k"};
    for (std::size_t index = 0; index < 4; ++index)
    {
        options.identifier = "Child" + std::to_string(index);
        options.parent = parents[index];
        const auto module = CDEModGenerator::Create(m_root, options);
        const auto pipeline = ReadFile(module.parent_path() / "pipeline.ini");
        EXPECT_NE(pipeline.find("Parent = " + options.parent + "\r\n"), std::string::npos);
        CModuleFile parsed;
        const auto path = module.string();
        parsed.LoadModuleFile(path.c_str());
        ASSERT_EQ(parsed.GetRequiredCount(), 4 - index);
        for (std::size_t required = 0; required < 4 - index; ++required)
        {
            EXPECT_STREQ(parsed.GetRequired(required)->GetFileName(), parents[index + required]);
        }
    }
}

TEST_F(DEModGeneratorTest, RejectsInvalidNamesAndDescriptionsWithoutCreatingFiles)
{
    CDEModGenerator::Options options{"Valid", "Display", "", "DoWDE"};
    for (const auto &invalid : {"", "A B", "../escape", "name.dot", "CON", "com1", "Engine",
                                "DXP3", "a name", "toolong123456789012345678901234567890123456789012345678901234567890123"})
    {
        options.identifier = invalid;
        EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error) << invalid;
    }
    options.identifier = "Valid";
    options.displayName = "bad\nname";
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    options.displayName = std::string(65, 'x');
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    options.displayName = "Display";
    options.description = std::string(257, 'x');
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    options.description.clear();
    options.parent = "Unknown";
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    EXPECT_TRUE(std::filesystem::is_empty(m_root));
}

TEST_F(DEModGeneratorTest, IgnoresIncompleteTemplateInAnotherMod)
{
    const auto source = m_root / "Existing" / "DataGeneric" / "_default.burn";
    std::filesystem::create_directories(source.parent_path());
    std::ofstream(source, std::ios::binary) << "burn_targets = {}\nburn_params = {}\n";
    const auto module = CDEModGenerator::Create(m_root, StarterOptions());
    EXPECT_EQ(ReadFile(module.parent_path() / "DataGeneric" / "_default.burn"), kDefaultBurnTemplate);
    EXPECT_EQ(ReadFile(source), "burn_targets = {}\nburn_params = {}\n");
}

TEST_F(DEModGeneratorTest, RejectsExistingDirectoryAndModuleWithoutOverwriting)
{
    auto options = StarterOptions();
    std::filesystem::create_directory(m_root / "ExampleMod");
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    EXPECT_TRUE(std::filesystem::is_empty(m_root / "ExampleMod"));
    std::filesystem::remove(m_root / "ExampleMod");
    std::ofstream(m_root / "ExampleMod.module") << "keep";
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(m_root, options)), std::runtime_error);
    EXPECT_EQ(ReadFile(m_root / "ExampleMod.module"), "keep");
    EXPECT_FALSE(std::filesystem::exists(m_root / "ExampleMod"));
}

TEST_F(DEModGeneratorTest, BundledRulesAreValidLuaWithAllDefaultTargets)
{
    auto lua = std::unique_ptr<lua_State, decltype(&lua51_close)>(lua51L_newstate(), &lua51_close);
    ASSERT_NE(lua, nullptr);
    ASSERT_EQ(lua51L_loadbuffer(lua.get(), kDefaultBurnTemplate.data(), kDefaultBurnTemplate.size(), "_default.burn"), 0)
        << lua51_tolstring(lua.get(), -1, nullptr);
    ASSERT_EQ(lua51_pcall(lua.get(), 0, 0, 0), 0) << lua51_tolstring(lua.get(), -1, nullptr);
    lua51_getglobal(lua.get(), "burn_targets");
    ASSERT_STREQ(lua51_typename(lua.get(), lua51_type(lua.get(), -1)), "table");
    EXPECT_EQ(lua51_objlen(lua.get(), -1), 14u);
    lua51_rawgeti(lua.get(), -1, 1);
    lua51_getfield(lua.get(), -1, "targets");
    lua51_rawgeti(lua.get(), -1, 1);
    lua51_getfield(lua.get(), -1, "plugin");
    ASSERT_NE(lua51_tolstring(lua.get(), -1, nullptr), nullptr);
    EXPECT_STREQ(lua51_tolstring(lua.get(), -1, nullptr), "EBP to WHE");
    lua51_settop(lua.get(), 0);
    lua51_getglobal(lua.get(), "burn_targets");
    lua51_rawgeti(lua.get(), -1, 14);
    lua51_getfield(lua.get(), -1, "name");
    EXPECT_STREQ(lua51_tolstring(lua.get(), -1, nullptr), "ignore");
    lua51_settop(lua.get(), 0);
    lua51_getglobal(lua.get(), "burn_params");
    ASSERT_STREQ(lua51_typename(lua.get(), lua51_type(lua.get(), -1)), "table");
    EXPECT_EQ(lua51_objlen(lua.get(), -1), 14u);
}

TEST_F(DEModGeneratorTest, CreatesMissingModsRootWithBundledTemplate)
{
    const auto modsRoot = m_root / "AppData" / "Relic Entertainment" / "Dawn of War" / "mods";
    const auto module = CDEModGenerator::Create(modsRoot, StarterOptions());
    EXPECT_TRUE(std::filesystem::is_regular_file(module));
    EXPECT_EQ(module, modsRoot / "ExampleMod" / "ExampleMod.module");
}

TEST_F(DEModGeneratorTest, DoesNotCreateMissingModsRootForInvalidIdentifier)
{
    const auto modsRoot = m_root / "new" / "mods";
    CDEModGenerator::Options options{"bad/name", "My Mod", "", "DXP3"};
    EXPECT_THROW(static_cast<void>(CDEModGenerator::Create(modsRoot, options)), std::runtime_error);
    EXPECT_FALSE(std::filesystem::exists(modsRoot));
}

TEST_F(DEModGeneratorTest, GeneratedProjectLoadsOwnAndInstalledResources)
{
    const auto modulePath = CDEModGenerator::Create(m_root, StarterOptions());
    const auto projectRoot = modulePath.parent_path();
    const auto installRoot = m_root / "GameInstall";
    std::filesystem::create_directories(projectRoot / "Mod" / "Data");
    std::ofstream(projectRoot / "Mod" / "Data" / "own.txt") << "own";
    for (const auto name : {"DoWDE", "DXP3", "DXP2", "WXP", "W40k"})
    {
        const std::string modName(name);
        std::filesystem::create_directories(installRoot / modName / "Data");
        std::filesystem::create_directories(installRoot / modName / "DataGeneric");
        std::ofstream(installRoot / (modName + ".module"))
            << "[global]\nUIName = " << modName << "\nModFolder = " << modName << "\nDataFolder.1 = Data\n";
        std::ofstream(installRoot / modName / "Data" / (modName + ".txt")) << modName;
    }

    CModuleFile mod;
    const auto path = modulePath.string();
    mod.LoadModuleFile(path.c_str());
    const auto install = installRoot.string();
    mod.SetGameInstallPath(install.c_str());
    mod.ReloadResources(CModuleFile::RR_DataFolders | CModuleFile::RR_RequiredMods | CModuleFile::RR_DataGeneric,
                        CModuleFile::RR_DataFolders, 0);
    mod.VInit();

    const auto read = [&mod](const char *name, std::size_t length) {
        auto stream = std::unique_ptr<IFileStore::IStream>(mod.VOpenStream(name));
        std::string contents(length, '\0');
        stream->VRead(static_cast<unsigned long>(length), 1, contents.data());
        return contents;
    };
    EXPECT_EQ(read("Data\\own.txt", 3), "own");
    EXPECT_EQ(read("Data\\DoWDE.txt", 5), "DoWDE");
    const auto burn = ReadFile(projectRoot / "DataGeneric" / "_default.burn");
    ASSERT_GE(burn.size(), 8u);
    EXPECT_EQ(read("Generic\\_default.burn", 8), burn.substr(0, 8));
}
