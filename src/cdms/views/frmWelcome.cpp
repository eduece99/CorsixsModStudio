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
#include "frmWelcome.h"
#include "frame/Construct.h"
#include "common/config.h"
#include "common/ThemeColours.h"
#include <wx/generic/msgdlgg.h>
#include "CtrlStatusText.h"
#include "common/strings.h"
#include "common/Common.h"
#include "common/BitmapResources.h"

BEGIN_EVENT_TABLE(frmWelcome, wxWindow)
EVT_SIZE(frmWelcome::OnSize)
EVT_BUTTON(IDC_Quit, frmWelcome::OnQuit)
EVT_BUTTON(IDC_NewMod, frmWelcome::OnNewMod)
EVT_BUTTON(IDC_LoadSga, frmWelcome::OnLoadSga)
EVT_BUTTON(IDC_LoadModDE, frmWelcome::OnLoadModDE)
EVT_BUTTON(IDC_LoadModSS, frmWelcome::OnLoadModSS)
EVT_BUTTON(IDC_Help, ConstructFrame::LaunchHelp)
EVT_BUTTON(IDC_Donate, ConstructFrame::LaunchDonate)
EVT_ENTER_WINDOW(frmWelcome::OnMouseEvent)
END_EVENT_TABLE()

frmWelcome::frmWelcome(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : wxWindow(parent, id, pos, size)
{
    bFirstMouseEvent = true;

    m_pDonateBitmap = std::make_unique<wxBitmap>(CDMSBitmapResources::LoadBitmap(wxT("IDB_DONATE"), wxT("donate.bmp")));
    if (m_pDonateBitmap->IsOk())
    {
        m_pDonateBitmap->SetMask(new wxMask(*m_pDonateBitmap, wxColour(255, 0, 255)));
    }
    else
    {
        wxLogError(wxT("Failed to load the donate bitmap."));
    }

    SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    auto *pTopSizer = new wxBoxSizer(wxVERTICAL);
    auto *pNewMod = new wxButton(this, IDC_NewMod, AppStr(new_mod), wxDefaultPosition,
                                 wxSize(FromDIP(150), wxDefaultCoord));
    SetBackgroundColour(pNewMod->GetBackgroundColour());

    srand((unsigned)time(nullptr));

    int iShowDonate = 1;
    TheConfig->Read(AppStr(config_donate), &iShowDonate, iShowDonate);

    pTopSizer->Add(new wxStaticText(this, -1, AppStr(welcome_caption)), 0, wxALIGN_CENTER | wxALL, 3);
    pTopSizer->Add(SBT(pNewMod, AppStr(new_mod_help)), 0, wxALL | wxALIGN_CENTER, 3);
    pTopSizer->Add(
        SBT(new wxButton(this, IDC_LoadModDE, AppStr(open_modde), wxDefaultPosition,
                         wxSize(FromDIP(150), wxDefaultCoord)),
            AppStr(open_modde_help)),
        0, wxALL | wxALIGN_CENTER, 3);
    pTopSizer->Add(
        SBT(new wxButton(this, IDC_LoadModSS, AppStr(open_modss), wxDefaultPosition,
                         wxSize(FromDIP(150), wxDefaultCoord)),
            AppStr(open_modss_help)),
        0, wxALL | wxALIGN_CENTER, 3);
    pTopSizer->Add(SBT(new wxButton(this, IDC_LoadSga, AppStr(open_sga), wxDefaultPosition,
                                    wxSize(FromDIP(150), wxDefaultCoord)),
                       AppStr(open_sga_help)),
                   0, wxALL | wxALIGN_CENTER, 3);
    pTopSizer->Add(SBT(new wxButton(this, IDC_Help, AppStr(help_index), wxDefaultPosition,
                                    wxSize(FromDIP(150), wxDefaultCoord)),
                       AppStr(help_index_help)),
                   0, wxALL | wxALIGN_CENTER, 3);
    pTopSizer->Add(
        SBT(new wxButton(this, IDC_Quit, AppStr(exit), wxDefaultPosition, wxSize(FromDIP(150), wxDefaultCoord)),
            AppStr(exit_help)),
        0, wxALL | wxALIGN_CENTER, 3);
    if (iShowDonate == 1 && m_pDonateBitmap->IsOk())
    {
        pTopSizer->Add(SBT(new wxBitmapButton(this, IDC_Donate, *m_pDonateBitmap, wxDefaultPosition, wxDefaultSize, 0),
                           AppStr(donate_help)),
                       0, wxALL | wxALIGN_CENTER, 3);
    }

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
}

void frmWelcome::OnMouseEvent(wxMouseEvent &event)
{
    UNUSED(event);
    if (bFirstMouseEvent)
    {
        bFirstMouseEvent = false;

        wxString sVal;
        TheConfig->Read(AppStr(config_firstrun), &sVal, (const wxString &)wxT("1"));
        if (sVal == wxT("1"))
        {
            wxGenericMessageDialog oMsg(this, AppStr(welcome_firstrun), AppStr(app_name), wxYES_NO);
            if (oMsg.ShowModal() == wxID_YES)
            {
                ConstructFrame::StaticLaunchHelp();
            }
        }

        TheConfig->Write(AppStr(config_firstrun), (const wxString &)wxT("0"));
    }
}

void frmWelcome::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

void frmWelcome::OnQuit(wxCommandEvent &event)
{
    UNUSED(event);
    TheConstruct->Close(true);
}

void frmWelcome::OnNewMod(wxCommandEvent &event)
{
    UNUSED(event);
    TheConstruct->DoNewMod();
}

void frmWelcome::OnLoadSga(wxCommandEvent &event)
{
    UNUSED(event);
    TheConstruct->DoLoadSga();
}

void frmWelcome::OnLoadModSS(wxCommandEvent &event)
{
    UNUSED(event);
    TheConstruct->DoLoadMod(wxT(""), ConstructFrame::LM_SS);
}

void frmWelcome::OnLoadModDE(wxCommandEvent &event)
{
    UNUSED(event);
    TheConstruct->DoLoadMod(wxT(""), ConstructFrame::LM_DE);
}

frmWelcome::~frmWelcome() = default;
