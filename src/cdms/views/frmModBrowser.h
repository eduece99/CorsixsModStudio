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

#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/listctrl.h>
#include "views/interfaces/IModBrowserView.h"
#include <vector>

//! Modal dialog that displays mods discovered in the DoW:DE AppData folder.
/*!
    Scans for mods on construction, displays them in a list, and returns the
    selected .module file path when the user clicks Open or double-clicks an entry.
*/
class frmModBrowser : public wxDialog, public IModBrowserView
{
  public:
    frmModBrowser(wxWindow *pParent);

    //! Returns the full path to the selected .module file, or empty if none selected.
    [[nodiscard]] wxString GetSelectedModulePath() const;

    // IModBrowserView implementation
    void PopulateModList(const std::vector<ModBrowserEntry> &entries) override;
    void ShowError(const wxString &sMessage) override;

  private:
    void OnOpen(wxCommandEvent &event);
    void OnCancel(wxCommandEvent &event);
    void OnItemActivated(wxListEvent &event);
    void OnSelectionChanged(wxListEvent &event);
    void UpdateOpenButton();

    wxListCtrl *m_pList = nullptr;
    wxButton *m_pOpenBtn = nullptr;
    std::vector<ModBrowserEntry> m_entries;

    DECLARE_EVENT_TABLE()
};
