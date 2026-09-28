/*
    Tests for RelicToolResolver — verifies tool discovery with temp directories.
*/

#include <gtest/gtest.h>
#include "frame/RelicToolResolver.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <fstream>
#include <process.h>

class RelicToolResolverTest : public ::testing::Test
{
  protected:
    wxString m_sTempDir;

    void SetUp() override
    {
        m_sTempDir = wxFileName::GetTempDir() + wxString::Format(wxT("\\RelicToolResolverTest_%d"), _getpid());
        wxFileName::Mkdir(m_sTempDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }

    void TearDown() override
    {
        // Clean up temp files
        wxDir::Remove(m_sTempDir, wxPATH_RMDIR_RECURSIVE);
    }

    // Create a fake exe file in a directory
    void CreateFakeExe(const wxString &sDir, const wxString &sExeName)
    {
        wxFileName::Mkdir(sDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        wxString sPath = sDir + wxT("\\") + sExeName;
        std::ofstream ofs(sPath.ToStdString());
        ofs << "fake";
        ofs.close();
    }
};

TEST_F(RelicToolResolverTest, FindToolInRootDir)
{
    CreateFakeExe(m_sTempDir, wxT("AudioEditor.exe"));
    wxString sResult = RelicToolResolver::FindToolIn(m_sTempDir, wxT("AudioEditor.exe"));
    EXPECT_FALSE(sResult.empty());
    EXPECT_TRUE(sResult.EndsWith(wxT("AudioEditor.exe")));
}

TEST_F(RelicToolResolverTest, FindToolInModToolsSubdir)
{
    wxString sModTools = m_sTempDir + wxT("\\ModTools");
    CreateFakeExe(sModTools, wxT("FxTool.exe"));

    wxString sResult = RelicToolResolver::FindToolIn(m_sTempDir, wxT("FxTool.exe"));
    EXPECT_FALSE(sResult.empty());
    EXPECT_TRUE(sResult.Contains(wxT("ModTools")));
}

TEST_F(RelicToolResolverTest, FindToolPrefersRoot)
{
    // Put tool in both root and ModTools — root should win
    CreateFakeExe(m_sTempDir, wxT("ObjectEditor.exe"));
    wxString sModTools = m_sTempDir + wxT("\\ModTools");
    CreateFakeExe(sModTools, wxT("ObjectEditor.exe"));

    wxString sResult = RelicToolResolver::FindToolIn(m_sTempDir, wxT("ObjectEditor.exe"));
    EXPECT_FALSE(sResult.Contains(wxT("ModTools")));
}

TEST_F(RelicToolResolverTest, FindToolNotFound)
{
    wxString sResult = RelicToolResolver::FindToolIn(m_sTempDir, wxT("NonExistent.exe"));
    EXPECT_TRUE(sResult.empty());
}

TEST_F(RelicToolResolverTest, FindToolInEmptyDir)
{
    wxString sResult = RelicToolResolver::FindToolIn(wxT(""), wxT("AudioEditor.exe"));
    EXPECT_TRUE(sResult.empty());
}

TEST_F(RelicToolResolverTest, ConstructorPopulatesToolList)
{
    RelicToolResolver resolver;
    EXPECT_EQ(resolver.GetToolCount(), 8u);
}

TEST_F(RelicToolResolverTest, ToolListHasUIEditor)
{
    RelicToolResolver resolver;
    bool bFound = false;
    for (size_t i = 0; i < resolver.GetToolCount(); ++i)
    {
        if (resolver.GetTool(i).sExeName == wxT("UIEditor.exe"))
        {
            bFound = true;
            break;
        }
    }
    EXPECT_TRUE(bFound);
}

TEST_F(RelicToolResolverTest, InitiallyNoneFound)
{
    RelicToolResolver resolver;
    for (size_t i = 0; i < resolver.GetToolCount(); ++i)
    {
        EXPECT_FALSE(resolver.GetTool(i).bFound) << "Tool " << i << " should not be found initially";
        EXPECT_TRUE(resolver.GetTool(i).sResolvedPath.empty());
    }
}

TEST_F(RelicToolResolverTest, UsesProjectDirectoryWhenModuleHasPipeline)
{
    const wxString sProject = m_sTempDir + wxT("\\My Mod");
    wxFileName::Mkdir(sProject, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    std::ofstream((sProject + wxT("\\pipeline.ini")).ToStdWstring()) << "[project:MyMod]\n";
    const wxString sExe = m_sTempDir + wxT("\\ObjectEditor.exe");
    EXPECT_EQ(RelicToolResolver::GetWorkingDirectory(sExe, sProject + wxT("\\MyMod.module")), sProject);
    EXPECT_EQ(RelicToolResolver::GetWorkingDirectory(sExe, sProject + wxT("\\missing\\other.module")), m_sTempDir);
    EXPECT_EQ(RelicToolResolver::GetWorkingDirectory(sExe, wxEmptyString), m_sTempDir);
}

TEST_F(RelicToolResolverTest, AudioEditorReceivesModAssistantWorkingDirectory)
{
    const wxString project = m_sTempDir + wxT("\\My Project");
    const wxString mod = project + wxT("\\Mod");
    wxFileName::Mkdir(mod, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    std::ofstream((project + wxT("\\pipeline.ini")).ToStdWstring()) << "[project:MyProject]\n";
    const wxString module = project + wxT("\\MyProject.module");

    EXPECT_EQ(RelicToolResolver::GetToolParameters(wxT("AudioEditor.exe"), module, wxT("Mod")),
              wxT("-wkdir \"") + mod + wxT("\""));
    EXPECT_TRUE(RelicToolResolver::GetToolParameters(wxT("ObjectEditor.exe"), module, wxT("Mod")).empty());
    EXPECT_TRUE(RelicToolResolver::GetToolParameters(wxT("AudioEditor.exe"),
                                                     project + wxT("\\other\\Other.module"), wxT("Mod")).empty());
    EXPECT_THROW(RelicToolResolver::GetToolParameters(wxT("AudioEditor.exe"), module, wxT("missing")),
                 std::invalid_argument);
    EXPECT_THROW(RelicToolResolver::GetToolParameters(wxT("AudioEditor.exe"), module, wxT("..\\Mod")),
                 std::invalid_argument);
}
