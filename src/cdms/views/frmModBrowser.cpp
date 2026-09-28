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
#include "views/frmModBrowser.h"
#include "presenters/CModBrowserPresenter.h"
#include "common/strings.h"
#include "common/strconv.h"
#include <wx/filename.h>
#include <filesystem>

enum
{
    ID_ModBrowserList = wxID_HIGHEST + 500,
    ID_ModBrowserOpen,
};

BEGIN_EVENT_TABLE(frmModBrowser, wxDialog)
EVT_BUTTON(ID_ModBrowserOpen, frmModBrowser::OnOpen)
EVT_BUTTON(wxID_CANCEL, frmModBrowser::OnCancel)
EVT_LIST_ITEM_ACTIVATED(ID_ModBrowserList, frmModBrowser::OnItemActivated)
EVT_LIST_ITEM_SELECTED(ID_ModBrowserList, frmModBrowser::OnSelectionChanged)
EVT_LIST_ITEM_DESELECTED(ID_ModBrowserList, frmModBrowser::OnSelectionChanged)
END_EVENT_TABLE()

frmModBrowser::frmModBrowser(wxWindow *pParent)
    : wxDialog(pParent, wxID_ANY, AppStr(browse_modde_title), wxDefaultPosition, wxSize(700, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto *pSizer = new wxBoxSizer(wxVERTICAL);

    // List control
    m_pList = new wxListCtrl(this, ID_ModBrowserList, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_pList->AppendColumn(AppStr(browse_modde_col_name), wxLIST_FORMAT_LEFT, 180);
    m_pList->AppendColumn(AppStr(browse_modde_col_desc), wxLIST_FORMAT_LEFT, 280);
    m_pList->AppendColumn(AppStr(browse_modde_col_version), wxLIST_FORMAT_LEFT, 70);
    m_pList->AppendColumn(AppStr(browse_modde_col_location), wxLIST_FORMAT_LEFT, 140);
    pSizer->Add(m_pList, 1, wxEXPAND | wxALL, 8);

    // Buttons
    auto *pBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    pBtnSizer->AddStretchSpacer();
    m_pOpenBtn = new wxButton(this, ID_ModBrowserOpen, AppStr(browse_modde_open));
    m_pOpenBtn->SetDefault();
    m_pOpenBtn->Enable(false);
    pBtnSizer->Add(m_pOpenBtn, 0, wxRIGHT, 4);
    pBtnSizer->Add(new wxButton(this, wxID_CANCEL, AppStr(browse_modde_cancel)), 0);
    pSizer->Add(pBtnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizer(pSizer);
    SetMinSize(wxSize(500, 300));
    Centre();

    try
    {
        std::vector<wxString> errors;
        PopulateModList(CModBrowserPresenter::ScanMods(&errors));
        if (!errors.empty())
        {
            wxString message = wxT("Some DE mods could not be read:\n");
            for (const auto &error : errors)
            {
                message += wxT("\n") + error;
            }
            ShowError(message);
        }
    }
    catch (const CRainmanException &e)
    {
        ShowError(AsciiTowxString(e.getMessage()));
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        ShowError(wxString::FromUTF8(e.what()));
    }
}

wxString frmModBrowser::GetSelectedModulePath() const
{
    long iSel = m_pList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (iSel < 0 || static_cast<std::size_t>(iSel) >= m_entries.size())
    {
        return wxString();
    }
    return m_entries[static_cast<std::size_t>(iSel)].sModulePath;
}

void frmModBrowser::PopulateModList(const std::vector<ModBrowserEntry> &entries)
{
    m_entries = entries;
    m_pList->DeleteAllItems();

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const auto &e = entries[i];
        long iItem = m_pList->InsertItem(static_cast<long>(i), e.sName);
        m_pList->SetItem(iItem, 1, e.sDescription);
        m_pList->SetItem(iItem, 2, e.sVersion);

        // Show the mod subdirectory name as location
        wxFileName fn(e.sModulePath);
        wxString sDir = fn.GetPath();
        wxString sDirName = sDir.AfterLast(wxFileName::GetPathSeparator());
        m_pList->SetItem(iItem, 3, sDirName);
    }

    UpdateOpenButton();
}

void frmModBrowser::ShowError(const wxString &sMessage)
{
    wxMessageBox(sMessage, AppStr(browse_modde_title), wxOK | wxICON_ERROR, this);
}

void frmModBrowser::OnOpen(wxCommandEvent &event)
{
    UNUSED(event);
    if (!GetSelectedModulePath().IsEmpty())
    {
        EndModal(wxID_OK);
    }
}

void frmModBrowser::OnCancel(wxCommandEvent &event)
{
    UNUSED(event);
    EndModal(wxID_CANCEL);
}

void frmModBrowser::OnItemActivated(wxListEvent &event)
{
    UNUSED(event);
    if (!GetSelectedModulePath().IsEmpty())
    {
        EndModal(wxID_OK);
    }
}

void frmModBrowser::OnSelectionChanged(wxListEvent &event)
{
    UNUSED(event);
    UpdateOpenButton();
}

void frmModBrowser::UpdateOpenButton()
{
    bool bHasSelection = m_pList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) >= 0;
    m_pOpenBtn->Enable(bHasSelection);
}
