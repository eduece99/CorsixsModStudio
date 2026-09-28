/*
    This file is part of Corsix's Mod Studio.

    Corsix's Mod Studio is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Corsix's Mod Studio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Corsix's Mod Studio; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "presenters/CModBrowserPresenter.h"
#include "common/config.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ModBrowserTest : public ::testing::Test
{
  protected:
    fs::path root;

    void SetUp() override
    {
        root = fs::temp_directory_path() / ("cdms_mod_browser_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::create_directories(root);
    }

    void TearDown() override { fs::remove_all(root); }
};

TEST_F(ModBrowserTest, DiscoversAnyModuleFilenameAndReportsMalformedFiles)
{
    fs::create_directories(root / "First");
    fs::create_directories(root / "Second");
    std::ofstream(root / "First" / "custom.module")
        << "[global]\nUIName = Working Mod\nModVersion = 2.1.3\nDataFolder.1 = Data\n";
    std::ofstream(root / "Second" / "broken.module") << "not a valid module";
    std::vector<wxString> errors;
    const auto entries = CModBrowserPresenter::ScanMods(root, &errors);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].sName, wxT("Working Mod"));
    EXPECT_EQ(entries[0].sVersion, wxT("2.1.3"));
    EXPECT_TRUE(entries[0].sModulePath.EndsWith(wxT("custom.module")));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_TRUE(errors[0].Contains(wxT("broken.module")));
}

TEST_F(ModBrowserTest, EmptyDirectoryAndInvalidRootAreDistinct)
{
    EXPECT_TRUE(CModBrowserPresenter::ScanMods(root).empty());
    EXPECT_THROW(static_cast<void>(CModBrowserPresenter::ScanMods(root / "missing")), fs::filesystem_error);
}

TEST_F(ModBrowserTest, RequiresGameModuleAtInstallRoot)
{
    EXPECT_FALSE(ConfIsDEInstallFolder(wxString(root.wstring())));
    std::ofstream(root / "DoWDE.module") << "[global]\nUIName = Base\n";
    EXPECT_FALSE(ConfIsDEInstallFolder(wxString(root.wstring())));
    std::ofstream(root / "W40k.exe") << "stub";
    EXPECT_TRUE(ConfIsDEInstallFolder(wxString(root.wstring())));
    std::filesystem::rename(root / "W40k.exe", root / "W40k_gog.exe");
    EXPECT_TRUE(ConfIsDEInstallFolder(wxString(root.wstring())));
    EXPECT_FALSE(ConfIsDEInstallFolder(wxString((root / "First").wstring())));
}
