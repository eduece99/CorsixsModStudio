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

#include <wx/string.h>
#include <string>
#include <vector>

//! A single discovered mod entry for the mod browser.
struct ModBrowserEntry
{
    wxString sName;        //!< Display name (UIName or Name from .module)
    wxString sDescription; //!< Mod description
    wxString sVersion;     //!< Formatted version string (e.g. "1.0.0")
    wxString sModulePath;  //!< Full path to the .module file
};

//! Abstract interface for the mod browser view.
/*!
    Decouples CModBrowserPresenter from the concrete wxDialog so that
    the presenter can be tested independently.
*/
class IModBrowserView
{
  public:
    virtual ~IModBrowserView() = default;

    //! Populate the mod list with discovered entries.
    virtual void PopulateModList(const std::vector<ModBrowserEntry> &entries) = 0;

    //! Display an error message to the user.
    virtual void ShowError(const wxString &sMessage) = 0;
};
