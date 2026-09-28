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

#include "tools/CDeArchiveTool.h"
#include <gtest/gtest.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/init.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace
{
class CDeArchiveToolTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_initializer.IsOk());
        m_sRoot = wxFileName::CreateTempFileName(wxT("cdms-de-archive"));
        ASSERT_FALSE(m_sRoot.empty());
        ASSERT_TRUE(wxRemoveFile(m_sRoot));
        ASSERT_TRUE(wxFileName::Mkdir(m_sRoot + wxT("\\My Mod"), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
        m_sFolder = m_sRoot + wxT("\\My Mod");
        m_sConfig = m_sFolder + wxT("\\Data.sgaconfig");
        m_sSource = m_sFolder + wxT("\\Data");
        ASSERT_TRUE(wxFileName::Mkdir(m_sSource, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
        m_sOutput = m_sFolder + wxT("\\Data.sga");
        m_sExecutable = m_sRoot + wxT("\\Archive.exe");
        wxFile oConfig(m_sConfig, wxFile::write);
        ASSERT_TRUE(oConfig.IsOpened());
        ASSERT_TRUE(oConfig.Write("Archive\nTOCStart alias=\"Data\" relativeroot=\"Data\"\n"
                                  "FileSettingsStart defcompression=\"1\"\nFileSettingsEnd\nTOCEnd\n"));
        oConfig.Close();
        ASSERT_TRUE(wxCopyFile(wxString::FromUTF8(DE_ARCHIVE_STUB_PATH), m_sExecutable));
    }

    void TearDown() override
    {
        if (!m_sRoot.empty())
        {
            std::filesystem::remove_all(std::filesystem::path(m_sRoot.ToStdWstring()));
        }
    }

    wxInitializer m_initializer;
    wxString m_sRoot, m_sFolder, m_sSource, m_sConfig, m_sOutput, m_sExecutable;
};
} // namespace

TEST(CDeArchiveTool, ClassifyEditors)
{
    EXPECT_EQ(CDeArchiveTool::Classify(wxT("name.tga.BURN")), CDeArchiveTool::EditorType::Burn);
    EXPECT_EQ(CDeArchiveTool::Classify(wxT("Data.SGACONFIG")), CDeArchiveTool::EditorType::SgaConfig);
    EXPECT_EQ(CDeArchiveTool::Classify(wxT("PIPELINE.INI")), CDeArchiveTool::EditorType::Pipeline);
    EXPECT_EQ(CDeArchiveTool::Classify(wxT("other.ini")), CDeArchiveTool::EditorType::Unsupported);
    EXPECT_EQ(CDeArchiveTool::Classify(wxT("Data.sga")), CDeArchiveTool::EditorType::Unsupported);
}

TEST_F(CDeArchiveToolTest, ExactInstalledArchiveCreateContract)
{
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource, m_sOutput, vArgs, sError)) << sError;
    const std::vector<wxString> vExpected = {m_sExecutable, wxT("-a"), m_sFolder + wxT("\\Data.sga"),
                                             wxT("-c"), m_sConfig, wxT("-r"), m_sFolder};
    ASSERT_EQ(vArgs.size(), vExpected.size());
    for (size_t i = 0; i < vArgs.size(); ++i)
    {
        EXPECT_EQ(vArgs[i].ToStdString(), vExpected[i].ToStdString()) << "argv[" << i << "]";
    }
}

TEST_F(CDeArchiveToolTest, AcceptsConfigOutsideSourceRoot)
{
    const wxString sOther = m_sRoot + wxT("\\outside.sgaconfig");
    wxFile oFile(sOther, wxFile::write);
    ASSERT_TRUE(oFile.IsOpened());
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(oFile.Write("Archive\nTOCStart alias=\"Data\" relativeroot=\"Data\"\n"));
    oFile.Close();
    EXPECT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, sOther, m_sSource, m_sOutput, vArgs, sError)) << sError;
    EXPECT_EQ(vArgs[4], sOther);
}

TEST_F(CDeArchiveToolTest, RejectsMissingExecutableAndNonConfigFile)
{
    std::vector<wxString> vArgs;
    wxString sError;
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sRoot + wxT("\\missing.exe"), m_sConfig, m_sSource, m_sOutput, vArgs, sError));
    EXPECT_FALSE(sError.empty());
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sExecutable, m_sSource, m_sOutput, vArgs, sError));
    EXPECT_FALSE(sError.empty());
}

TEST_F(CDeArchiveToolTest, FindsBurnAndArchiveConfigOutsideVirtualGameData)
{
    const wxString sGeneric = m_sRoot + wxT("\\DataGeneric");
    ASSERT_TRUE(wxFileName::Mkdir(sGeneric, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    const wxString sBurn = sGeneric + wxT("\\_default.burn");
    wxFile oFile(sBurn, wxFile::write);
    ASSERT_TRUE(oFile.IsOpened());
    const auto vBurns = CDeArchiveTool::FindFiles(m_sRoot, CDeArchiveTool::EditorType::Burn);
    const auto vConfigs = CDeArchiveTool::FindFiles(m_sRoot, CDeArchiveTool::EditorType::SgaConfig);
    ASSERT_EQ(vBurns.size(), 1u);
    EXPECT_EQ(vBurns.front(), sBurn);
    ASSERT_EQ(vConfigs.size(), 1u);
    EXPECT_EQ(vConfigs.front(), m_sConfig);
}

TEST_F(CDeArchiveToolTest, FindsNestedProjectFiles)
{
    const wxString sBurnFolder = m_sRoot + wxT("\\DataGeneric\\Nested");
    const wxString sConfigFolder = m_sFolder + wxT("\\Nested");
    ASSERT_TRUE(wxFileName::Mkdir(sBurnFolder, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    ASSERT_TRUE(wxFileName::Mkdir(sConfigFolder, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    const wxString sBurn = sBurnFolder + wxT("\\_default.burn");
    const wxString sConfig = sConfigFolder + wxT("\\Other.sgaconfig");
    wxFile burn(sBurn, wxFile::write), config(sConfig, wxFile::write);
    ASSERT_TRUE(burn.IsOpened());
    ASSERT_TRUE(config.IsOpened());
    burn.Close();
    config.Close();

    const auto burns = CDeArchiveTool::FindFiles(m_sRoot, CDeArchiveTool::EditorType::Burn);
    const auto configs = CDeArchiveTool::FindFiles(m_sRoot, CDeArchiveTool::EditorType::SgaConfig);
    ASSERT_EQ(burns.size(), 1u);
    EXPECT_EQ(burns.front(), sBurn);
    EXPECT_NE(std::find(configs.begin(), configs.end(), sConfig), configs.end());
    EXPECT_NE(std::find(configs.begin(), configs.end(), m_sConfig), configs.end());
}

TEST_F(CDeArchiveToolTest, FindsAndEditsOnlyProjectRootPipeline)
{
    const wxString pipeline = m_sRoot + wxT("\\pipeline.ini");
    const wxString nested = m_sFolder + wxT("\\pipeline.ini");
    wxFile projectFile(pipeline, wxFile::write), nestedFile(nested, wxFile::write);
    ASSERT_TRUE(projectFile.IsOpened());
    ASSERT_TRUE(nestedFile.IsOpened());
    ASSERT_TRUE(projectFile.Write("[project:Test]\n"));
    ASSERT_TRUE(nestedFile.Write("[project:Unrelated]\n"));
    projectFile.Close();
    nestedFile.Close();

    const auto files = CDeArchiveTool::FindFiles(m_sRoot, CDeArchiveTool::EditorType::Pipeline);
    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files.front(), pipeline);
    wxString text, error;
    ASSERT_TRUE(CDeArchiveTool::ReadText(pipeline, text, error)) << error;
    EXPECT_EQ(text, wxT("[project:Test]\n"));
    ASSERT_TRUE(CDeArchiveTool::SaveText(pipeline, wxT("[project:Changed]\n"), error)) << error;
    ASSERT_TRUE(CDeArchiveTool::ReadText(pipeline, text, error)) << error;
    EXPECT_EQ(text, wxT("[project:Changed]\n"));
    std::ifstream untouched(std::filesystem::path(nested.ToStdWstring()), std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(untouched), {}), "[project:Unrelated]\n");

    std::vector<wxString> args;
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, pipeline, m_sSource, m_sOutput, args, error));
    EXPECT_NE(error.Find(wxT(".sgaconfig")), wxNOT_FOUND);
}

TEST_F(CDeArchiveToolTest, TextEditsPersistToDisk)
{
    wxString sContent, sError;
    ASSERT_TRUE(CDeArchiveTool::ReadText(m_sConfig, sContent, sError)) << sError;
    EXPECT_NE(sContent.Find(wxT("relativeroot=\"Data\"")), wxNOT_FOUND);
    ASSERT_TRUE(CDeArchiveTool::SaveText(m_sConfig, wxT("Archive\nTOCEnd\n"), sError)) << sError;
    ASSERT_TRUE(CDeArchiveTool::ReadText(m_sConfig, sContent, sError)) << sError;
    EXPECT_EQ(sContent, wxT("Archive\nTOCEnd\n"));
    EXPECT_FALSE(CDeArchiveTool::SaveText(m_sRoot + wxT("\\missing.burn"), wxT("x"), sError));
    EXPECT_FALSE(sError.empty());
}

TEST_F(CDeArchiveToolTest, DisplaysModAssistantBurnLineEndingsWithoutChangingSource)
{
    const wxString path = m_sRoot + wxT("\\_default.burn");
    const std::string original = "burn_targets = {}\r\r\nburn_params = {}\r\r\n";
    wxFile file(path, wxFile::write);
    ASSERT_TRUE(file.IsOpened());
    ASSERT_TRUE(file.Write(original));
    file.Close();

    wxString content, error;
    ASSERT_TRUE(CDeArchiveTool::ReadText(path, content, error)) << error;
    EXPECT_EQ(content, wxT("burn_targets = {}\r\nburn_params = {}\r\n"));
    std::ifstream saved(std::filesystem::path(path.ToStdWstring()), std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(saved), {}), original);
}

TEST_F(CDeArchiveToolTest, LaunchesWithPathsContainingSpaces)
{
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource, m_sOutput, vArgs, sError)) << sError;
    EXPECT_TRUE(CDeArchiveTool::Run(vArgs, sError)) << sError;
    EXPECT_TRUE(wxFileName::FileExists(m_sOutput));
}

TEST_F(CDeArchiveToolTest, PassesDistinctConfigSourceAndOutputPathsWithSpaces)
{
    const wxString sRoot = m_sRoot + wxT("\\Different Input Parent");
    const wxString sSource = sRoot + wxT("\\Data");
    const wxString sOutputDir = m_sRoot + wxT("\\Different Output Parent");
    ASSERT_TRUE(wxFileName::Mkdir(sSource, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    ASSERT_TRUE(wxFileName::Mkdir(sOutputDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    const wxString sOutput = sOutputDir + wxT("\\my custom.sga");
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, sSource, sOutput, vArgs, sError)) << sError;
    EXPECT_EQ(vArgs[2], sOutput);
    EXPECT_EQ(vArgs[4], m_sConfig);
    EXPECT_EQ(vArgs[6], sRoot);
    ASSERT_TRUE(CDeArchiveTool::Run(vArgs, sError)) << sError;
    EXPECT_TRUE(wxFileName::FileExists(sOutput));
    std::ifstream oArgs(std::filesystem::path((m_sConfig + wxT(".args")).ToStdWstring()));
    ASSERT_TRUE(oArgs.is_open());
    std::string sLine;
    for (const auto &sArg : vArgs)
    {
        ASSERT_TRUE(static_cast<bool>(std::getline(oArgs, sLine)));
        EXPECT_EQ(sLine, sArg.ToStdString());
    }
}

TEST_F(CDeArchiveToolTest, RejectsSourceThatWouldResolveToDataData)
{
    std::vector<wxString> vArgs;
    wxString sError;
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sFolder,
                                         m_sRoot + wxT("\\outside.sga"), vArgs, sError));
    EXPECT_NE(sError.Find(wxT("relativeroot")), wxNOT_FOUND);
    EXPECT_TRUE(vArgs.empty());
}

TEST_F(CDeArchiveToolTest, RejectsMissingOutputFolderAndNonSgaOutput)
{
    std::vector<wxString> vArgs;
    wxString sError;
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource,
                                         m_sRoot + wxT("\\missing\\Data.sga"), vArgs, sError));
    EXPECT_FALSE(sError.empty());
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource,
                                         m_sRoot + wxT("\\other.txt"), vArgs, sError));
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource,
                                         m_sSource + wxT("\\inside.sga"), vArgs, sError));
    EXPECT_NE(sError.Find(wxT("outside the source")), wxNOT_FOUND);
}

TEST_F(CDeArchiveToolTest, ResolvesNestedRelativeRootAgainstSelectedSource)
{
    const wxString sNested = m_sSource + wxT("\\Sub Folder");
    ASSERT_TRUE(wxFileName::Mkdir(sNested, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL));
    wxFile oConfig(m_sConfig, wxFile::write);
    ASSERT_TRUE(oConfig.IsOpened());
    ASSERT_TRUE(oConfig.Write("Archive\nTOCStart alias=\"Data\" relativeroot=\"Data/Sub Folder\"\nTOCEnd\n"));
    oConfig.Close();
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, sNested, m_sOutput, vArgs, sError)) << sError;
    EXPECT_EQ(vArgs[6], m_sFolder);
}

TEST_F(CDeArchiveToolTest, RejectsConflictingOrUnsafeRelativeRoots)
{
    wxFile oConfig(m_sConfig, wxFile::write);
    ASSERT_TRUE(oConfig.IsOpened());
    ASSERT_TRUE(oConfig.Write("TOCStart alias=\"A\" relativeroot=\"Data\"\n"
                              "TOCStart alias=\"B\" relativeroot=\"Other\"\n"));
    oConfig.Close();
    std::vector<wxString> vArgs;
    wxString sError;
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource, m_sOutput, vArgs, sError));
    EXPECT_NE(sError.Find(wxT("different")), wxNOT_FOUND);
    ASSERT_TRUE(oConfig.Open(m_sConfig, wxFile::write));
    ASSERT_TRUE(oConfig.Write("TOCStart alias=\"Data\" relativeroot=\"../Data\"\n"));
    oConfig.Close();
    EXPECT_FALSE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource, m_sOutput, vArgs, sError));
    EXPECT_NE(sError.Find(wxT("relativeroot")), wxNOT_FOUND);
}

TEST_F(CDeArchiveToolTest, ReportsProcessExitFailure)
{
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource,
                                        m_sFolder + wxT("\\failure.sga"), vArgs, sError)) << sError;
    EXPECT_FALSE(CDeArchiveTool::Run(vArgs, sError));
    EXPECT_NE(sError.Find(wxT("17")), wxNOT_FOUND);
}

TEST_F(CDeArchiveToolTest, ReportsSuccessWithoutArchiveAsFailure)
{
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource,
                                        m_sFolder + wxT("\\missing.sga"), vArgs, sError)) << sError;
    EXPECT_FALSE(CDeArchiveTool::Run(vArgs, sError));
    EXPECT_NE(sError.Find(wxT("did not create")), wxNOT_FOUND);
}

TEST_F(CDeArchiveToolTest, ReportsUnchangedExistingArchiveAsFailure)
{
    const wxString sExisting = m_sFolder + wxT("\\missing.sga");
    wxFile oExisting(sExisting, wxFile::write);
    ASSERT_TRUE(oExisting.IsOpened());
    ASSERT_TRUE(oExisting.Write("old archive"));
    oExisting.Close();
    std::vector<wxString> vArgs;
    wxString sError;
    ASSERT_TRUE(CDeArchiveTool::Prepare(m_sExecutable, m_sConfig, m_sSource, sExisting, vArgs, sError)) << sError;
    EXPECT_FALSE(CDeArchiveTool::Run(vArgs, sError));
    EXPECT_NE(sError.Find(wxT("did not update")), wxNOT_FOUND);
}

TEST(CDeArchiveTool, RejectsInvalidInvocationWithoutLaunching)
{
    wxString sError;
    EXPECT_FALSE(CDeArchiveTool::Run({}, sError));
    EXPECT_FALSE(sError.empty());
}
