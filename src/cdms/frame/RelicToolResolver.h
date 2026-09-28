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

#pragma once

#include <rainman/module/CModuleFile.h>
#include <vector>
#include <wx/string.h>

// Discovers Relic tool executables across game install directories.
// Each tool resolves to a full path or remains empty (not found).
class RelicToolResolver
{
  public:
    struct ToolInfo
    {
        wxString sMenuLabel;
        wxString sHelpString;
        wxString sExeName;
        wxString sResolvedPath; // Full path if found, empty if not
        bool bFound = false;
    };

    RelicToolResolver();

    // Scan known game install directories (no module loaded)
    void ScanNoModule();

    // Scan module directory first, then fall back to game install directories
    void ScanWithModule(const wxString &sModuleFile, CModuleFile::eModuleType eType);

    size_t GetToolCount() const;
    const ToolInfo &GetTool(size_t i) const;

    // Returns full path if sExeName exists in sDir or sDir\ModTools, else empty
    static wxString FindToolIn(const wxString &sDir, const wxString &sExeName);

    // Pipeline tools read pipeline.ini from the current working directory.
    static wxString GetWorkingDirectory(const wxString &sToolPath, const wxString &sModuleFile);

    // Modern Audio Editor takes -wkdir <project>/<ModFolder>, not just a process cwd.
    // Throws std::invalid_argument when the selected module's ModFolder is unusable.
    static wxString GetToolParameters(const wxString &sExeName, const wxString &sModuleFile,
                                      const wxString &sModFolder);

  private:
    std::vector<ToolInfo> m_vTools;

    void InitToolList();
    void ResolveAll(const std::vector<wxString> &vSearchDirs);

    // Collect game install directories, ignoring failures
    static std::vector<wxString> GetGameInstallDirs();
};
