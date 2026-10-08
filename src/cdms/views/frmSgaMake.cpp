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
#include "frmSgaMake.h"
#include "frmMessage.h"
#include "common/strings.h"
#include "common/strconv.h"
#include "common/config.h"
#include "frame/Construct.h"
#include "CtrlStatusText.h"
#include <cerrno>
#include <wx/textdlg.h>
#include <wx/filename.h>
#include <rainman/io/CFileSystemStore.h>
#include <rainman/archive/CSgaCreator.h>
#include "common/Common.h"
#include "common/ThemeColours.h"
#include "rainman/core/RainmanLog.h"

BEGIN_EVENT_TABLE(frmSgaMake, wxDialog)
EVT_BUTTON(IDC_BrowseIn, frmSgaMake::OnBrowseInClick)
EVT_BUTTON(IDC_BrowseOut, frmSgaMake::OnBrowseOutClick)
EVT_BUTTON(IDC_Cancel, frmSgaMake::OnCancelClick)
EVT_BUTTON(IDC_Go, frmSgaMake::OnGoClick)
EVT_COMBOBOX(IDC_FileOut, frmSgaMake::OnFileOutUpdated)
EVT_TEXT(IDC_FileOut, frmSgaMake::OnFileOutUpdated)
END_EVENT_TABLE()

frmSgaMake::frmSgaMake()
    : wxDialog(wxTheApp->GetTopWindow(), -1, AppStr(sgapack_title), wxPoint(0, 0), wxDefaultSize,
               wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW | wxCAPTION)
{
    CentreOnParent();
    auto *pTopSizer = new wxFlexGridSizer(3);
    pTopSizer->SetFlexibleDirection(wxHORIZONTAL);
    pTopSizer->AddGrowableCol(1, 1);

    wxArrayString lstToCs;
    lstToCs.Add(wxT("Data"));
    lstToCs.Add(wxT("Attrib"));

    wxArrayString lstArchives;
    CModuleFile *pMod = TheConstruct->GetModuleService().GetModule();
    for (size_t i = 0; i < pMod->GetArchiveCount(); ++i)
    {
        char *sFullpath = new char[pMod->GetArchiveFullPath(i, nullptr)];
        pMod->GetArchiveFullPath(i, sFullpath);
        lstArchives.Add(AsciiTowxString(sFullpath));
        delete[] sFullpath;
    }

    char *sThisMod = new char[strlen(pMod->GetModFolder()) + 3];
    sprintf(sThisMod, "\\%s", pMod->GetModFolder());
    {
        char *sSlash = strchr(sThisMod + 1, '\\');
        if (sSlash)
        {
            sSlash[1] = 0;
        }
        else
        {
            strcat(sThisMod, "\\");
        }
    }

    for (size_t i = 0; i < pMod->GetDataSourceCount(); ++i)
    {
        CModuleFile::CCohDataSource *pDataSrc = pMod->GetDataSource(i);
        if (pDataSrc->IsLoaded())
        {
            for (size_t j = 0; j < pDataSrc->GetArchiveCount(); ++j)
            {
                CModuleFile::CArchiveHandler *pArch = pDataSrc->GetArchive(j);
                char *sFullpath = new char[strlen(pMod->GetApplicationPath()) + strlen(pArch->GetFileName()) + 1];
                sprintf(sFullpath, "%s%s", pMod->GetApplicationPath(), pArch->GetFileName());
                if (strstr(sFullpath, sThisMod))
                {
                    lstArchives.Add(AsciiTowxString(sFullpath));
                }
                delete[] sFullpath;
            }
        }
    }
    delete[] sThisMod;

    wxWindow *pBgTemp;
    pTopSizer->Add(SBT(pBgTemp = new wxStaticText(this, -1, AppStr(sgapack_dirselect_label)),
                       AppStr(sgapack_dirselect_label_help)),
                   0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(m_pInDir = new wxTextCtrl(this, IDC_DirIn, wxT("")), AppStr(sgapack_dirselect_label_help)), 1,
                   wxALL | wxEXPAND, 3);
    pTopSizer->Add(SBT(new wxButton(this, IDC_BrowseIn, AppStr(sgapack_browse)), AppStr(sgapack_dirselect_label_help)),
                   0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxFIXED_MINSIZE | wxALL, 3);

    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(sgapack_outselect)), AppStr(sgapack_outselect_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(
        SBT(m_pOutFile = new wxComboBox(this, IDC_FileOut, wxT(""), wxDefaultPosition, wxDefaultSize, lstArchives),
            AppStr(sgapack_outselect_help)),
        1, wxALL | wxEXPAND, 3);
    pTopSizer->Add(SBT(new wxButton(this, IDC_BrowseOut, AppStr(sgapack_browse)), AppStr(sgapack_outselect_help)), 0,
                   wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxFIXED_MINSIZE | wxALL, 3);

    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(sgapack_toc)), AppStr(sgapack_toc_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(m_pTocName = new wxComboBox(this, IDC_TocName, wxT("Data"), wxDefaultPosition,
                                               wxSize(FromDIP(300), wxDefaultCoord), lstToCs),
                       AppStr(sgapack_toc_help)),
                   1, wxALL | wxEXPAND, 3);
    pTopSizer->AddSpacer(0);

    auto *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    pButtonSizer->Add(m_pCancelBtn = new wxButton(this, IDC_Cancel, AppStr(newmod_cancel)), 0, wxEXPAND | wxALL, 3);
    pButtonSizer->Add(m_pGoBtn = new wxButton(this, IDC_Go, AppStr(newmod_create)), 0, wxEXPAND | wxALL, 3);

    pTopSizer->AddSpacer(0);
    pTopSizer->Add(pButtonSizer, 0, wxALIGN_CENTER);
    pTopSizer->AddSpacer(0);

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
    SetBackgroundColour(pBgTemp->GetBackgroundColour());

    m_pPresenter = std::make_unique<CSgaMakePresenter>(*this, this);
}

void frmSgaMake::OnFileOutUpdated(wxCommandEvent &event)
{
    UNUSED(event);
    // NOLINT(bugprone-branch-clone): first and last branches intentionally both default to "Data"
    if (m_pOutFile->GetValue().Lower().Find(wxT("data")) != wxNOT_FOUND)
    {
        m_pTocName->SetValue(wxT("Data"));
    }
    else if (TheConstruct->GetModuleService().GetModuleType() != CModuleFile::MT_DawnOfWar &&
             m_pOutFile->GetValue().Lower().Find(wxT("attrib")) != wxNOT_FOUND)
    {
        m_pTocName->SetValue(wxT("Attrib"));
    }
    else
    {
        m_pTocName->SetValue(wxT("Data")); // NOLINT(bugprone-branch-clone)
    }
}

void frmSgaMake::OnCancelClick(wxCommandEvent &event)
{
    UNUSED(event);
    EndModal(wxID_CLOSE);
}

void frmSgaMake::OnBrowseInClick(wxCommandEvent &event)
{
    UNUSED(event);
    m_pInDir->SetValue(
        wxDirSelector(AppStr(sgapack_dirselect), m_pInDir->GetValue(), 0, wxDefaultPosition, TheConstruct));
}

void frmSgaMake::OnBrowseOutClick(wxCommandEvent &event)
{
    m_pOutFile->SetValue(wxFileSelector(AppStr(sgapack_outselect), wxT(""), m_pOutFile->GetValue(), wxT("sga"),
                                        AppStr(sgapack_filter), wxFD_SAVE | wxFD_OVERWRITE_PROMPT, TheConstruct));
    OnFileOutUpdated(event);
}

void frmSgaMake::OnGoClick(wxCommandEvent &event)
{
    UNUSED(event);
    CDMS_LOG_INFO("Starting SGA archive creation from UI");
    if (m_pInDir->GetValue().empty() || m_pOutFile->GetValue().empty() || m_pTocName->GetValue().empty())
    {
        ThemeColours::ShowMessageBox(AppStr(sgapack_novalue), wxT("Error"), wxICON_ERROR, wxTheApp->GetTopWindow());
    }
    else
    {
        // Compensate for silly users (hi Zach :p)
        if (!m_pTocName->GetValue().IsSameAs(wxT("Data")))
        {
            if (TheConstruct->GetModuleService().GetModuleType() != CModuleFile::MT_DawnOfWar &&
                m_pTocName->GetValue().IsSameAs(wxT("Attrib")))
            {
            }
            else
            {
                if (ThemeColours::ShowMessageBox(
                        wxT("The ToC name you have chosen is not a common one.\nDoW only usually needs \"Data\", and "
                            "CoH only usually needs \"Data\" and \"Attrib\".\nContinue anyway?"),
                        AppStr(sgapack_title), wxYES_NO | wxICON_QUESTION) == wxNO)
                {
                    return;
                }
            }
        }
        wxFileName oInputName(m_pInDir->GetValue());
        if (!oInputName.IsAbsolute())
        {
            if (ThemeColours::ShowMessageBox(
                    wxT("The input folder specified is not an absolute path.\nContinue anyway?"), AppStr(sgapack_title),
                    wxYES_NO | wxICON_QUESTION) == wxNO)
            {
                return;
            }
        }

        wxFileName oOutputName(m_pOutFile->GetValue());
        if (!oOutputName.IsAbsolute())
        {
            if (ThemeColours::ShowMessageBox(
                    wxT("The output file specified is not an absolute path.\nContinue anyway?"), AppStr(sgapack_title),
                    wxYES_NO | wxICON_QUESTION) == wxNO)
            {
                return;
            }
        }
        if (!oOutputName.GetExt().IsSameAs(wxT("sga"), false))
        {
            if (ThemeColours::ShowMessageBox(
                    wxT("The output file specified does not have the .SGA extension.\nContinue anyway?"),
                    AppStr(sgapack_title), wxYES_NO | wxICON_QUESTION) == wxNO)
            {
                return;
            }
        }

        // Start async SGA creation
        m_pPresenter->CreateSga(m_pInDir->GetValue(), m_pOutFile->GetValue(), m_pTocName->GetValue(),
                                TheConstruct->GetModuleService().GetSgaOutputVersion());
    }
}

// --- ISgaMakeView implementation ---

void frmSgaMake::ShowProgress(const wxString &sMessage)
{
    if (m_pGoBtn)
    {
        m_pGoBtn->SetLabel(sMessage);
    }
}

void frmSgaMake::HideProgress()
{
    if (m_pGoBtn)
    {
        m_pGoBtn->SetLabel(AppStr(newmod_create));
    }
}

void frmSgaMake::ShowError(const wxString &sMessage)
{
    ThemeColours::ShowMessageBox(sMessage, AppStr(sgapack_title), wxICON_ERROR, this);
}

void frmSgaMake::OnSgaCreated()
{
    ThemeColours::ShowMessageBox(AppStr(sgapack_done), AppStr(sgapack_title), wxICON_INFORMATION, TheConstruct);
    EndModal(wxID_OK);
}

void frmSgaMake::DisableControls()
{
    if (m_pGoBtn)
    {
        m_pGoBtn->Enable(false);
    }
    if (m_pCancelBtn)
    {
        m_pCancelBtn->Enable(false);
    }
    if (m_pInDir)
    {
        m_pInDir->Enable(false);
    }
    if (m_pOutFile)
    {
        m_pOutFile->Enable(false);
    }
    if (m_pTocName)
    {
        m_pTocName->Enable(false);
    }
}

void frmSgaMake::EnableControls()
{
    if (m_pGoBtn)
    {
        m_pGoBtn->Enable(true);
    }
    if (m_pCancelBtn)
    {
        m_pCancelBtn->Enable(true);
    }
    if (m_pInDir)
    {
        m_pInDir->Enable(true);
    }
    if (m_pOutFile)
    {
        m_pOutFile->Enable(true);
    }
    if (m_pTocName)
    {
        m_pTocName->Enable(true);
    }
}
