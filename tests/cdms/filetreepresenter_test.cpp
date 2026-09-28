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

#include "presenters/CFileTreePresenter.h"
#include <gtest/gtest.h>

// --- ClassifyFileIcon tests ---

TEST(CFileTreePresenter, ClassifyFileIcon_AI)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("script.ai")), CFileTreePresenter::Icon_AI);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Lua)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("init.lua")), CFileTreePresenter::Icon_Lua);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Burn)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("_default.BURN")), CFileTreePresenter::Icon_Lua);
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("Data.sgaconfig")), CFileTreePresenter::Icon_Unknown);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Nil)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("data.nil")), CFileTreePresenter::Icon_Nil);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Rgd)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("attrib.rgd")), CFileTreePresenter::Icon_Rgd);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Scar)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("mission.scar")), CFileTreePresenter::Icon_Scar);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Tga)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("texture.tga")), CFileTreePresenter::Icon_Tga);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Rgt)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("texture.rgt")), CFileTreePresenter::Icon_Rgt);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Dds)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("texture.dds")), CFileTreePresenter::Icon_Dds);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Bfx)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("effect.bfx")), CFileTreePresenter::Icon_Bfx);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Abp)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("anim.abp")), CFileTreePresenter::Icon_Abp);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Rgm)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("model.rgm")), CFileTreePresenter::Icon_Rgm);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Nis)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("cutscene.nis")), CFileTreePresenter::Icon_Nis);
}

TEST(CFileTreePresenter, ClassifyFileIcon_Unknown)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("readme.txt")), CFileTreePresenter::Icon_Unknown);
}

TEST(CFileTreePresenter, ClassifyFileIcon_NoExtension)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("Makefile")), CFileTreePresenter::Icon_Unknown);
}

TEST(CFileTreePresenter, ClassifyFileIcon_CaseInsensitive)
{
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("DATA.RGD")), CFileTreePresenter::Icon_Rgd);
    EXPECT_EQ(CFileTreePresenter::ClassifyFileIcon(wxT("Script.LUA")), CFileTreePresenter::Icon_Lua);
}

// --- IsEngineFileMapName tests ---

TEST(CFileTreePresenter, IsEngineFileMapName_Match)
{
    std::vector<wxString> engines = {wxT("W40kData"), wxT("W40kXP1Data")};
    EXPECT_TRUE(CFileTreePresenter::IsEngineFileMapName(wxT("W40kData"), engines));
    EXPECT_TRUE(CFileTreePresenter::IsEngineFileMapName(wxT("W40kXP1Data"), engines));
}

TEST(CFileTreePresenter, IsEngineFileMapName_NoMatch)
{
    std::vector<wxString> engines = {wxT("W40kData"), wxT("W40kXP1Data")};
    EXPECT_FALSE(CFileTreePresenter::IsEngineFileMapName(wxT("MyModData"), engines));
}

TEST(CFileTreePresenter, IsEngineFileMapName_EmptyList)
{
    std::vector<wxString> engines;
    EXPECT_FALSE(CFileTreePresenter::IsEngineFileMapName(wxT("W40kData"), engines));
}

// --- CategorizeFileSource tests ---

TEST(CFileTreePresenter, CategorizeFileSource_ThisMod)
{
    std::vector<wxString> engines = {wxT("W40kData")};
    auto result = CFileTreePresenter::CategorizeFileSource(wxT("MyMod"), wxT("MyMod"), engines);
    EXPECT_EQ(result, CFileTreePresenter::FileSource::ThisMod);
}

TEST(CFileTreePresenter, CategorizeFileSource_Engine)
{
    std::vector<wxString> engines = {wxT("W40kData"), wxT("W40kXP1Data")};
    auto result = CFileTreePresenter::CategorizeFileSource(wxT("W40kData"), wxT("MyMod"), engines);
    EXPECT_EQ(result, CFileTreePresenter::FileSource::Engine);
}

TEST(CFileTreePresenter, CategorizeFileSource_OtherMod)
{
    std::vector<wxString> engines = {wxT("W40kData")};
    auto result = CFileTreePresenter::CategorizeFileSource(wxT("SomeOtherMod"), wxT("MyMod"), engines);
    EXPECT_EQ(result, CFileTreePresenter::FileSource::OtherMod);
}

TEST(CFileTreePresenter, CategorizeFileSource_ThisModTakesPrecedence)
{
    // If the current mod's file map name also appears in the engine list,
    // it should still be classified as ThisMod
    std::vector<wxString> engines = {wxT("MyMod")};
    auto result = CFileTreePresenter::CategorizeFileSource(wxT("MyMod"), wxT("MyMod"), engines);
    EXPECT_EQ(result, CFileTreePresenter::FileSource::ThisMod);
}
