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
#include "rainman/core/Exception.h"
#include "rainman/module/CModuleFile.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <process.h>
#include <string>

class DowDeModuleLoadingTest : public ::testing::Test
{
  protected:
    std::filesystem::path m_tempDir;
    std::filesystem::path m_modsDir;
    std::filesystem::path m_installDir;

    void SetUp() override
    {
        m_tempDir = std::filesystem::temp_directory_path() /
                    ("dowde_loading_" + std::to_string(_getpid()) + "_" +
                     std::to_string(reinterpret_cast<uintptr_t>(this)));
        m_modsDir = m_tempDir / "AppData" / "mods";
        m_installDir = m_tempDir / "install";
        std::filesystem::create_directories(m_modsDir);
        std::filesystem::create_directories(m_installDir);
    }

    void TearDown() override { std::filesystem::remove_all(m_tempDir); }

    void Write(const std::filesystem::path &path, const std::string &content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream << content;
        ASSERT_TRUE(stream.good()) << path.string();
    }

    static std::string Read(CModuleFile &module, const char *path, size_t length)
    {
        auto stream = std::unique_ptr<IFileStore::IStream>(module.VOpenStream(path));
        std::string content(length, '\0');
        stream->VRead(static_cast<unsigned long>(length), 1, content.data());
        return content;
    }
};

TEST_F(DowDeModuleLoadingTest, AppDataModLoadsOwnDataInstalledRequiredAndEngine)
{
    Write(m_modsDir / "Custom.module",
          "[global]\nUIName = Custom\nModFolder = UserFolder\nDataFolder.1 = Data\n"
          "RequiredMod.1 = W40k\nArchiveFile.1 = MyArchive\n");
    Write(m_modsDir / "UserFolder" / "Data" / "own.txt", "own");
    Write(m_installDir / "W40k.module",
          "[global]\nUIName = Base\nModFolder = InstalledFolder\nDataFolder.1 = Data\n"
          "ArchiveFile.1 = InstalledArchive\n");
    Write(m_installDir / "InstalledFolder" / "Data" / "base.txt", "base");
    Write(m_installDir / "Engine" / "Data" / "engine.txt", "engine");
    Write(m_installDir / "pipeline.ini",
          "[project:W40k]\nDataGeneric=%app%\\ModTools\\DataGeneric\\W40k\n");
    Write(m_installDir / "ModTools" / "DataGeneric" / "W40k" / "base-generic.txt", "generic");

    CModuleFile module;
    const auto modPath = (m_modsDir / "Custom.module").string();
    module.LoadModuleFile(modPath.c_str());
    const auto installPath = m_installDir.string();
    module.SetGameInstallPath(installPath.c_str());
    module.ReloadResources(CModuleFile::RR_DataFolders | CModuleFile::RR_RequiredMods | CModuleFile::RR_Engines,
                           CModuleFile::RR_DataFolders | CModuleFile::RR_DataGeneric,
                           CModuleFile::RR_DataFolders);
    module.VInit();

    ASSERT_EQ(module.GetRequiredCount(), 1u);
    auto *required = module.GetRequired(0)->GetModHandle();
    ASSERT_NE(required, nullptr);
    EXPECT_EQ(std::filesystem::path(required->GetApplicationPath()).parent_path(), m_installDir);
    EXPECT_EQ(std::filesystem::path(module.GetApplicationPath()).parent_path(), m_modsDir);
    EXPECT_EQ(Read(module, "Data\\own.txt", 3), "own");
    EXPECT_EQ(Read(module, "Data\\base.txt", 4), "base");
    EXPECT_EQ(Read(module, "Data\\engine.txt", 6), "engine");
    EXPECT_EQ(Read(module, "Generic\\base-generic.txt", 7), "generic");
    std::string archivePath(required->GetArchiveFullPath(0, nullptr), '\0');
    required->GetArchiveFullPath(0, archivePath.data());
    EXPECT_EQ(std::filesystem::path(archivePath.c_str()),
              m_installDir / "InstalledFolder" / "InstalledArchive.sga");
}

TEST_F(DowDeModuleLoadingTest, LocalRequiredTakesPrecedenceAndOwnPipelineMatchesModuleIdentifier)
{
    Write(m_modsDir / "Custom.module",
          "[global]\nUIName = Custom\nModFolder = UserFolder\nDataFolder.1 = Data\nRequiredMod.1 = LocalBase\n");
    Write(m_modsDir / "UserFolder" / "Data" / "own.txt", "own");
    Write(m_modsDir / "LocalBase.module",
          "[global]\nUIName = Local\nModFolder = LocalFolder\nDataFolder.1 = Data\n");
    Write(m_modsDir / "LocalFolder" / "Data" / "local.txt", "local");
    Write(m_installDir / "LocalBase.module",
          "[global]\nUIName = Wrong\nModFolder = WrongFolder\nDataFolder.1 = Data\n");
    Write(m_installDir / "WrongFolder" / "Data" / "wrong.txt", "wrong");
    Write(m_modsDir / "pipeline.ini",
          "[project:UserFolder]\nDataGeneric=WrongGeneric\n"
          "[project:Custom]\nDataGeneric=%app%\\ModTools\\DataGeneric\\Custom\n");
    Write(m_modsDir / "WrongGeneric" / "wrong-generic.txt", "wrong");
    Write(m_installDir / "ModTools" / "DataGeneric" / "Custom" / "generic.txt", "generic");

    CModuleFile module;
    const auto path = (m_modsDir / "Custom.module").string();
    module.LoadModuleFile(path.c_str());
    const auto installPath = m_installDir.string();
    module.SetGameInstallPath(installPath.c_str());
    module.ReloadResources(CModuleFile::RR_DataFolders | CModuleFile::RR_RequiredMods | CModuleFile::RR_DataGeneric,
                           CModuleFile::RR_DataFolders, 0);
    module.VInit();

    ASSERT_NE(module.GetRequired(0)->GetModHandle(), nullptr);
    EXPECT_EQ(std::filesystem::path(module.GetRequired(0)->GetModHandle()->GetApplicationPath()).parent_path(),
              m_modsDir);
    EXPECT_EQ(Read(module, "Data\\own.txt", 3), "own");
    EXPECT_EQ(Read(module, "Data\\local.txt", 5), "local");
    EXPECT_EQ(Read(module, "Generic\\generic.txt", 7), "generic");
    EXPECT_THROW(module.VOpenStream("Data\\wrong.txt"), CRainmanException);
    EXPECT_THROW(module.VOpenStream("Generic\\wrong-generic.txt"), CRainmanException);
}

TEST_F(DowDeModuleLoadingTest, InstalledPipelineSuppliesDataGenericWhenAppDataHasNone)
{
    Write(m_modsDir / "Custom.module",
          "[global]\nUIName = Custom\nModFolder = UserFolder\nDataFolder.1 = Data\n");
    Write(m_installDir / "pipeline.ini",
          "[project:UserFolder]\nDataGeneric=WrongGeneric\n"
          "[project:Custom]\nDataGeneric=%APP%/ModTools/DataGeneric/Custom\n");
    Write(m_installDir / "ModTools" / "DataGeneric" / "Custom" / "generic.txt", "generic");

    CModuleFile module;
    const auto path = (m_modsDir / "Custom.module").string();
    module.LoadModuleFile(path.c_str());
    const auto installPath = m_installDir.string();
    module.SetGameInstallPath(installPath.c_str());
    module.ReloadResources(CModuleFile::RR_DataGeneric, 0, 0);
    module.VInit();
    EXPECT_EQ(Read(module, "Generic\\generic.txt", 7), "generic");
}

TEST_F(DowDeModuleLoadingTest, LegacySameDirectoryResolutionAndReloadClearsInstallPath)
{
    Write(m_modsDir / "Custom.module",
          "[global]\nUIName = Custom\nModFolder = UserFolder\nDataFolder.1 = Data\nRequiredMod.1 = Base\n");
    Write(m_modsDir / "UserFolder" / "Data" / "own.txt", "own");
    Write(m_modsDir / "Base.module", "[global]\nUIName = Base\nModFolder = BaseFolder\nDataFolder.1 = Data\n");
    Write(m_modsDir / "BaseFolder" / "Data" / "base.txt", "base");
    Write(m_modsDir / "pipeline.ini", "[project:UserFolder]\nDataGeneric=LocalGeneric\n");
    Write(m_modsDir / "LocalGeneric" / "generic.txt", "generic");
    Write(m_installDir / "Base.module", "[global]\nUIName = Wrong\nModFolder = WrongFolder\n");

    CModuleFile module;
    const auto path = (m_modsDir / "Custom.module").string();
    module.LoadModuleFile(path.c_str());
    module.SetGameInstallPath(m_installDir.string().c_str());
    module.LoadModuleFile(path.c_str());
    module.ReloadResources(CModuleFile::RR_DataFolders | CModuleFile::RR_RequiredMods | CModuleFile::RR_DataGeneric,
                           CModuleFile::RR_DataFolders, 0);
    module.VInit();

    ASSERT_NE(module.GetRequired(0)->GetModHandle(), nullptr);
    EXPECT_EQ(std::filesystem::path(module.GetRequired(0)->GetModHandle()->GetApplicationPath()).parent_path(),
              m_modsDir);
    EXPECT_EQ(Read(module, "Data\\own.txt", 3), "own");
    EXPECT_EQ(Read(module, "Data\\base.txt", 4), "base");
    EXPECT_EQ(Read(module, "Generic\\generic.txt", 7), "generic");
}
