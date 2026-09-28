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

#include "common/Common.h"
#include "presenters/CModBrowserPresenter.h"
#include "common/strconv.h"
#include <rainman/module/CModuleParser.h>
#include <rainman/util/Util.h>
#include <rainman/core/Exception.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <wx/filename.h>

namespace fs = std::filesystem;

std::vector<ModBrowserEntry> CModBrowserPresenter::ScanMods(std::vector<wxString> *pErrors)
{
    std::unique_ptr<char[]> pModsPath(Rainman_GetDEModsPath());
    return ScanMods(fs::path(pModsPath.get()), pErrors);
}

std::vector<ModBrowserEntry> CModBrowserPresenter::ScanMods(const fs::path &modsDir, std::vector<wxString> *pErrors)
{
    std::vector<ModBrowserEntry> entries;
    if (!fs::is_directory(modsDir))
    {
        throw fs::filesystem_error("DE mods root is not a directory", modsDir,
                                   std::make_error_code(std::errc::not_a_directory));
    }

    for (const auto &dirEntry : fs::directory_iterator(modsDir))
    {
        if (!dirEntry.is_directory())
        {
            continue;
        }

        for (const auto &moduleEntry : fs::directory_iterator(dirEntry.path()))
        {
            const auto modulePath = moduleEntry.path();
            if (!moduleEntry.is_regular_file() ||
                wxString(modulePath.extension().wstring()).CmpNoCase(wxT(".module")) != 0)
            {
                continue;
            }

            try
            {
                auto result = CModuleParser::Parse(modulePath.string().c_str());
                if (result.iModuleType != 0)
                {
                    if (pErrors)
                    {
                        pErrors->push_back(wxString(modulePath.wstring()) + wxT(": Not a Dawn of War .module file"));
                    }
                    continue;
                }
                const auto &meta = result.metadata;
                ModBrowserEntry entry;

                if (!meta.m_sUiName.empty())
                {
                    entry.sName = AsciiTowxString(meta.m_sUiName.c_str());
                }
                else if (!meta.m_sName.empty())
                {
                    entry.sName = AsciiTowxString(meta.m_sName.c_str());
                }
                else
                {
                    entry.sName = wxString(modulePath.stem().wstring());
                }

                if (!meta.m_sDescription.empty())
                {
                    entry.sDescription = AsciiTowxString(meta.m_sDescription.c_str());
                }

                entry.sVersion = wxString::Format(wxT("%ld.%ld.%ld"), meta.m_iModVersionMajor, meta.m_iModVersionMinor,
                                                  meta.m_iModVersionRevision);
                if (entry.sVersion == wxT("0.0.0"))
                {
                    entry.sVersion.clear();
                }

                entry.sModulePath = wxString(modulePath.wstring());
                entries.push_back(std::move(entry));
            }
            catch (const CRainmanException &e)
            {
                if (pErrors)
                {
                    pErrors->push_back(wxString(modulePath.wstring()) + wxT(": ") + AsciiTowxString(e.getMessage()));
                }
            }
        }
    }

    // Sort by display name
    std::sort(entries.begin(), entries.end(),
              [](const ModBrowserEntry &a, const ModBrowserEntry &b) { return a.sName.CmpNoCase(b.sName) < 0; });

    return entries;
}
