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

#include "common/Common.h"
#include "views/frmDeArchive.h"
#include "tools/CDeArchiveTool.h"
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/dirdlg.h>
#include <wx/clntdata.h>
#include <algorithm>

frmDeArchive::frmDeArchive(wxWindow *pParent, const wxString &sModulePath, const wxString &sModFolderName,
                           const wxString &sGameInstall)
    : wxDialog(pParent, wxID_ANY, wxT("DoW:DE archive"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_sModFolder(wxFileName(sModulePath).GetPathWithSep() + sModFolderName),
      m_sProjectRoot(wxFileName(sModulePath).GetPath())
{
    auto *pSizer = new wxBoxSizer(wxVERTICAL);
    auto *pModLabel = new wxStaticText(this, wxID_ANY, wxT("Mod folder: ") + m_sModFolder, wxDefaultPosition,
                                       wxSize(FromDIP(680), wxDefaultCoord), wxST_ELLIPSIZE_MIDDLE);
    pModLabel->SetToolTip(m_sModFolder);
    pSizer->Add(pModLabel, 0, wxEXPAND | wxALL, 6);
    pSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Archive configuration (.sgaconfig):")), 0, wxLEFT, 6);
    auto *pConfigRow = new wxBoxSizer(wxHORIZONTAL);
    m_pConfigs = new wxChoice(this, wxID_ANY);
    pConfigRow->Add(m_pConfigs, 1, wxEXPAND | wxALL, 5);
    auto *pBrowseConfig = new wxButton(this, wxID_ANY, wxT("Browse..."));
    pConfigRow->Add(pBrowseConfig, 0, wxALL, 5);
    pSizer->Add(pConfigRow, 0, wxEXPAND);

    pSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Source folder (matches config relativeroot):")), 0, wxLEFT, 6);
    auto *pSourceRow = new wxBoxSizer(wxHORIZONTAL);
    m_pSourceFolder = new wxTextCtrl(this, wxID_ANY);
    pSourceRow->Add(m_pSourceFolder, 1, wxEXPAND | wxALL, 5);
    auto *pBrowseSource = new wxButton(this, wxID_ANY, wxT("Browse..."));
    pSourceRow->Add(pBrowseSource, 0, wxALL, 5);
    pSizer->Add(pSourceRow, 0, wxEXPAND);

    pSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Output archive (.sga):")), 0, wxLEFT, 6);
    auto *pOutputRow = new wxBoxSizer(wxHORIZONTAL);
    m_pOutputFile = new wxTextCtrl(this, wxID_ANY);
    pOutputRow->Add(m_pOutputFile, 1, wxEXPAND | wxALL, 5);
    auto *pBrowseOutput = new wxButton(this, wxID_ANY, wxT("Browse..."));
    pOutputRow->Add(pBrowseOutput, 0, wxALL, 5);
    pSizer->Add(pOutputRow, 0, wxEXPAND);

    pSizer->Add(new wxStaticText(this, wxID_ANY, wxT("Relic Archive.exe:")), 0, wxLEFT, 6);
    auto *pToolRow = new wxBoxSizer(wxHORIZONTAL);
    m_pExecutable =
        new wxTextCtrl(this, wxID_ANY, wxFileName::DirName(sGameInstall).GetPathWithSep() + wxT("Archive.exe"));
    pToolRow->Add(m_pExecutable, 1, wxEXPAND | wxALL, 5);
    auto *pBrowseExecutable = new wxButton(this, wxID_ANY, wxT("Browse..."));
    pToolRow->Add(pBrowseExecutable, 0, wxALL, 5);
    pSizer->Add(pToolRow, 0, wxEXPAND);

    auto *pButtons = new wxBoxSizer(wxHORIZONTAL);
    auto *pArchive = new wxButton(this, wxID_ANY, wxT("Create archive"));
    pButtons->Add(pArchive, 0, wxALL, 5);
    pButtons->Add(new wxButton(this, wxID_CANCEL, wxT("Close")), 0, wxALL, 5);
    pSizer->Add(pButtons, 0, wxALIGN_RIGHT);
    SetSizerAndFit(pSizer);
    const wxSize minSize = GetSize();
    SetMinSize(wxSize(std::max(minSize.x, FromDIP(760)), std::max(minSize.y, FromDIP(430))));
    SetSize(wxSize(std::max(minSize.x, FromDIP(900)), std::max(minSize.y, FromDIP(430))));
    CentreOnParent();
    RefreshConfigs();
    pBrowseConfig->Bind(wxEVT_BUTTON, &frmDeArchive::OnBrowseConfig, this);
    m_pConfigs->Bind(wxEVT_CHOICE, &frmDeArchive::OnConfigChanged, this);
    pBrowseSource->Bind(wxEVT_BUTTON, &frmDeArchive::OnBrowseSource, this);
    pBrowseOutput->Bind(wxEVT_BUTTON, &frmDeArchive::OnBrowseOutput, this);
    pBrowseExecutable->Bind(wxEVT_BUTTON, &frmDeArchive::OnBrowseExecutable, this);
    pArchive->Bind(wxEVT_BUTTON, &frmDeArchive::OnArchive, this);
}

void frmDeArchive::RefreshConfigs()
{
    m_pConfigs->Clear();
    if (!wxDirExists(m_sProjectRoot) || !wxDirExists(m_sModFolder))
    {
        wxMessageBox(wxT("Mod folder not found: ") + m_sModFolder, wxT("DE archive"), wxICON_ERROR | wxOK, this);
        return;
    }
    for (const auto &sConfig : CDeArchiveTool::FindFiles(m_sModFolder, CDeArchiveTool::EditorType::SgaConfig))
    {
        m_pConfigs->Append(sConfig.Mid(wxFileName::DirName(m_sModFolder).GetPathWithSep().length()),
                           new wxStringClientData(sConfig));
    }
    if (m_pConfigs->GetCount())
    {
        m_pConfigs->SetSelection(0);
        wxCommandEvent oEvent;
        OnConfigChanged(oEvent);
    }
}

wxString frmDeArchive::SelectedConfigPath() const
{
    const int index = m_pConfigs->GetSelection();
    if (index == wxNOT_FOUND)
    {
        return {};
    }
    const auto *data = static_cast<wxStringClientData *>(m_pConfigs->GetClientObject(index));
    return data->GetData();
}

void frmDeArchive::OnBrowseConfig(wxCommandEvent &)
{
    wxFileDialog oDialog(this, wxT("Select archive configuration"), m_sModFolder, wxEmptyString,
                         wxT("Archive configurations (*.sgaconfig)|*.sgaconfig"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (oDialog.ShowModal() == wxID_OK)
    {
        const wxString sPath = oDialog.GetPath();
        int index = wxNOT_FOUND;
        for (unsigned int i = 0; i < m_pConfigs->GetCount(); ++i)
        {
            const auto *data = static_cast<wxStringClientData *>(m_pConfigs->GetClientObject(i));
            if (data->GetData().IsSameAs(sPath, false))
            {
                index = static_cast<int>(i);
                break;
            }
        }
        if (index == wxNOT_FOUND)
        {
            index =
                m_pConfigs->Append(wxFileName(sPath).GetFullName() + wxT(" (external)"), new wxStringClientData(sPath));
        }
        m_pConfigs->SetSelection(index);
        wxCommandEvent oEvent;
        OnConfigChanged(oEvent);
    }
}

void frmDeArchive::OnConfigChanged(wxCommandEvent &)
{
    if (m_pConfigs->GetSelection() == wxNOT_FOUND)
    {
        return;
    }
    const wxFileName oConfig(SelectedConfigPath());
    m_pConfigs->SetToolTip(oConfig.GetFullPath());
    wxFileName oSource = wxFileName::DirName(oConfig.GetPath());
    oSource.AppendDir(oConfig.GetName());
    m_pSourceFolder->SetValue(oSource.GetPath());
    wxFileName oOutput(oConfig);
    oOutput.SetExt(wxT("sga"));
    m_pOutputFile->SetValue(oOutput.GetFullPath());
}

void frmDeArchive::OnBrowseSource(wxCommandEvent &)
{
    wxDirDialog oDialog(this, wxT("Select the source folder named by config relativeroot"), m_pSourceFolder->GetValue(),
                        wxDD_DIR_MUST_EXIST);
    if (oDialog.ShowModal() == wxID_OK)
    {
        m_pSourceFolder->SetValue(oDialog.GetPath());
    }
}

void frmDeArchive::OnBrowseOutput(wxCommandEvent &)
{
    wxFileDialog oDialog(this, wxT("Select output archive"), wxFileName(m_pOutputFile->GetValue()).GetPath(),
                         wxFileName(m_pOutputFile->GetValue()).GetFullName(), wxT("SGA archives (*.sga)|*.sga"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (oDialog.ShowModal() == wxID_OK)
    {
        m_pOutputFile->SetValue(oDialog.GetPath());
    }
}

void frmDeArchive::OnBrowseExecutable(wxCommandEvent &)
{
    wxFileDialog oDialog(this, wxT("Select Relic Archive.exe"), wxFileName(m_pExecutable->GetValue()).GetPath(),
                         wxT("Archive.exe"), wxT("Executable (*.exe)|*.exe"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (oDialog.ShowModal() == wxID_OK)
    {
        m_pExecutable->SetValue(oDialog.GetPath());
    }
}

void frmDeArchive::OnArchive(wxCommandEvent &)
{
    std::vector<wxString> vArguments;
    wxString sError;
    const wxString sConfig = SelectedConfigPath();
    if (!CDeArchiveTool::Prepare(m_pExecutable->GetValue(), sConfig, m_pSourceFolder->GetValue(),
                                 m_pOutputFile->GetValue(), vArguments, sError))
    {
        wxMessageBox(sError, wxT("DE archive"), wxICON_ERROR | wxOK, this);
        return;
    }
    const wxString sOutput = vArguments[2];
    if (wxFileName::FileExists(sOutput) && wxMessageBox(wxT("Replace existing archive?\n") + sOutput, wxT("DE archive"),
                                                        wxYES_NO | wxICON_WARNING, this) != wxYES)
    {
        return;
    }
    if (!CDeArchiveTool::Run(vArguments, sError))
    {
        wxMessageBox(sError, wxT("DE archive"), wxICON_ERROR | wxOK, this);
        return;
    }
    wxMessageBox(wxT("Archive created:\n") + sOutput, wxT("DE archive"), wxICON_INFORMATION | wxOK, this);
}
