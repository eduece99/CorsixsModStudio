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

#include "RelicToolResolver.h"
#include "common/config.h"
#include "common/strings.h"
#include <wx/filename.h>
#include <filesystem>
#include <stdexcept>

RelicToolResolver::RelicToolResolver() { InitToolList(); }

void RelicToolResolver::InitToolList()
{
    m_vTools = {
        {AppStr(attr_editor_menu), AppStr(attr_editor_help), wxT("AttributeEditor.exe"), {}, false},
        {AppStr(audio_editor_menu), AppStr(audio_editor_help), wxT("AudioEditor.exe"), {}, false},
        {AppStr(chunky_view_menu), AppStr(chunky_view_help), wxT("ChunkyViewer.exe"), {}, false},
        {AppStr(fx_tools_menu), AppStr(fx_tools_help), wxT("FxTool.exe"), {}, false},
        {AppStr(mission_edit_menu), AppStr(mission_edit_help), wxT("W40kME.exe"), {}, false},
        {AppStr(mod_packager_menu), AppStr(mod_packager_help), wxT("ModPackager.exe"), {}, false},
        {AppStr(object_editor_menu), AppStr(object_editor_help), wxT("ObjectEditor.exe"), {}, false},
        {AppStr(ui_editor_menu), AppStr(ui_editor_help), wxT("UIEditor.exe"), {}, false},
    };
}

void RelicToolResolver::ScanNoModule()
{
    auto vDirs = GetGameInstallDirs();
    ResolveAll(vDirs);
}

void RelicToolResolver::ScanWithModule(const wxString &sModuleFile, CModuleFile::eModuleType eType)
{
    std::vector<wxString> vDirs;

    // Priority 1: module's own directory
    wxString sModDir = sModuleFile.BeforeLast('\\');
    if (!sModDir.empty())
    {
        vDirs.push_back(sModDir);
    }

    // Priority 2: game install directories (DE, SS, DoW, DC, CoH)
    auto vGameDirs = GetGameInstallDirs();
    vDirs.insert(vDirs.end(), vGameDirs.begin(), vGameDirs.end());

    ResolveAll(vDirs);
}

void RelicToolResolver::ResolveAll(const std::vector<wxString> &vSearchDirs)
{
    for (auto &tool : m_vTools)
    {
        tool.sResolvedPath.clear();
        tool.bFound = false;

        for (const auto &sDir : vSearchDirs)
        {
            wxString sPath = FindToolIn(sDir, tool.sExeName);
            if (!sPath.empty())
            {
                tool.sResolvedPath = sPath;
                tool.bFound = true;
                break;
            }
        }
    }
}

wxString RelicToolResolver::FindToolIn(const wxString &sDir, const wxString &sExeName)
{
    if (sDir.empty())
    {
        return {};
    }

    // Check root directory
    wxString sPath = sDir + wxT("\\") + sExeName;
    if (wxFileName::FileExists(sPath))
    {
        return sPath;
    }

    // Check ModTools subdirectory
    sPath = sDir + wxT("\\ModTools\\") + sExeName;
    if (wxFileName::FileExists(sPath))
    {
        return sPath;
    }

    return {};
}

wxString RelicToolResolver::GetWorkingDirectory(const wxString &sToolPath, const wxString &sModuleFile)
{
    const wxString sProjectDir = wxFileName(sModuleFile).GetPath();
    if (!sProjectDir.empty() && wxFileName::FileExists(wxFileName(sProjectDir, wxT("pipeline.ini")).GetFullPath()))
    {
        return sProjectDir;
    }
    return wxFileName(sToolPath).GetPath();
}

wxString RelicToolResolver::GetToolParameters(const wxString &sExeName, const wxString &sModuleFile,
                                              const wxString &sModFolder)
{
    if (!sExeName.IsSameAs(wxT("AudioEditor.exe"), false) || sModuleFile.empty())
    {
        return {};
    }

    const std::filesystem::path projectRoot(wxFileName(sModuleFile).GetPath().ToStdWstring());
    if (!std::filesystem::is_regular_file(projectRoot / "pipeline.ini"))
    {
        return {};
    }

    const std::filesystem::path relativeFolder(sModFolder.ToStdWstring());
    if (sModFolder.empty() || !relativeFolder.is_relative() || sModFolder.Contains(wxT("\"")) ||
        relativeFolder.lexically_normal() != relativeFolder ||
        !std::filesystem::is_directory(projectRoot / relativeFolder))
    {
        throw std::invalid_argument("The selected mod's ModFolder is not a usable folder beside its pipeline.ini.");
    }

    const wxString workDir((projectRoot / relativeFolder).wstring());
    return wxT("-wkdir \"") + workDir + wxT("\"");
}

std::vector<wxString> RelicToolResolver::GetGameInstallDirs()
{
    std::vector<wxString> vDirs;

    // DE first — most likely to have tools
    try
    {
        wxString sDE = ConfGetDEFolder();
        if (!sDE.empty())
        {
            vDirs.push_back(sDE);
        }
    }
    catch (...)
    {
    }

    // Soulstorm
    try
    {
        wxString sSS = ConfGetSSFolder();
        if (!sSS.empty())
        {
            vDirs.push_back(sSS);
        }
    }
    catch (...)
    {
    }

    // Dark Crusade
    try
    {
        wxString sDC = ConfGetDCFolder();
        if (!sDC.empty())
        {
            vDirs.push_back(sDC);
        }
    }
    catch (...)
    {
    }

    // Dawn of War / Winter Assault
    try
    {
        wxString sDoW = ConfGetDoWFolder();
        if (!sDoW.empty())
        {
            vDirs.push_back(sDoW);
        }
    }
    catch (...)
    {
    }

    return vDirs;
}

size_t RelicToolResolver::GetToolCount() const { return m_vTools.size(); }

const RelicToolResolver::ToolInfo &RelicToolResolver::GetTool(size_t i) const { return m_vTools[i]; }
