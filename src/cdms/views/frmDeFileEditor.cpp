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
#include "views/frmDeFileEditor.h"
#include "tools/CDeArchiveTool.h"
#include "common/ThemeColours.h"
#include "frame/Construct.h"
#include <wx/button.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <stdexcept>

frmDeFileEditor::frmDeFileEditor(wxWindow *pParent, const wxString &sPath) : wxWindow(pParent, wxID_ANY), m_sPath(sPath)
{
    wxString content, error;
    if (!CDeArchiveTool::ReadText(sPath, content, error))
    {
        throw std::runtime_error(error.ToStdString());
    }

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    actions->Add(new wxStaticText(this, wxID_ANY, wxFileName(sPath).GetFullName()), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    actions->AddStretchSpacer();
    m_pSaveButton = new wxButton(this, wxID_SAVE, wxT("Save"));
    m_pSaveButton->Enable(false);
    actions->Add(m_pSaveButton, 0);
    sizer->Add(actions, 0, wxEXPAND | wxALL, 5);
    m_pSaveButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { DoSave(); });

    m_pText = new wxStyledTextCtrl(this, wxID_ANY);
    const bool burn = CDeArchiveTool::Classify(sPath) == CDeArchiveTool::EditorType::Burn;
    m_pText->SetLexer(burn ? wxSTC_LEX_LUA : wxSTC_LEX_NULL);
    ThemeColours::ApplyEditorTheme(m_pText);
    if (burn)
    {
        m_pText->SetKeyWords(0, wxT("and break do else elseif end false for function if in local nil not or "
                                    "repeat return then true until while"));
    }
    m_pText->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    m_pText->SetMarginWidth(0, 48);
    m_pText->SetText(content);
    m_pText->EmptyUndoBuffer();
    m_pText->SetSavePoint();
    sizer->Add(m_pText, 1, wxEXPAND);
    SetSizer(sizer);

    m_pText->Bind(wxEVT_STC_SAVEPOINTLEFT,
                  [this](wxStyledTextEvent &event)
                  {
                      m_pSaveButton->Enable(true);
                      TheConstruct->GetTabManager().UpdateDirtyState(this, true);
                      event.Skip();
                  });
    m_pText->Bind(wxEVT_STC_SAVEPOINTREACHED,
                  [this](wxStyledTextEvent &event)
                  {
                      m_pSaveButton->Enable(false);
                      TheConstruct->GetTabManager().UpdateDirtyState(this, false);
                      event.Skip();
                  });
    Bind(wxEVT_CLOSE_WINDOW, &frmDeFileEditor::OnCloseWindow, this);
}

bool frmDeFileEditor::IsModified() const { return m_pText->GetModify(); }

void frmDeFileEditor::DoSave()
{
    if (!IsModified())
    {
        return;
    }

    wxString error;
    if (!CDeArchiveTool::SaveText(m_sPath, m_pText->GetText(), error))
    {
        wxMessageBox(error, wxT("Save DE project file"), wxICON_ERROR | wxOK, this);
        return;
    }
    m_pText->SetSavePoint();
    if (CDeArchiveTool::Classify(m_sPath) == CDeArchiveTool::EditorType::Pipeline)
    {
        TheConstruct->GetStatusBar()->SetStatusText(wxT("Reload the mod to apply pipeline.ini changes."));
    }
}

void frmDeFileEditor::OnCloseWindow(wxCloseEvent &event)
{
    if (!IsModified())
    {
        return;
    }

    const int answer = ThemeColours::ShowMessageBox(wxT("Save changes before closing?"), m_sPath,
                                                    wxYES_NO | wxCANCEL | wxICON_WARNING, this);
    if (answer == wxYES)
    {
        DoSave();
        if (IsModified() && event.CanVeto())
        {
            event.Veto();
        }
    }
    else if (answer != wxNO && event.CanVeto())
    {
        event.Veto();
    }
}
