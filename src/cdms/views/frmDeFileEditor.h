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

#ifndef _FRM_DE_FILE_EDITOR_H_
#define _FRM_DE_FILE_EDITOR_H_

#include "views/interfaces/ISaveable.h"
#include <wx/window.h>
#include <wx/string.h>

class wxStyledTextCtrl;
class wxCloseEvent;
class wxButton;

class frmDeFileEditor : public wxWindow, public ISaveable
{
  public:
    frmDeFileEditor(wxWindow *pParent, const wxString &sPath);

    [[nodiscard]] const wxString &GetPath() const { return m_sPath; }
    bool IsModified() const override;
    void DoSave() override;

  private:
    void OnCloseWindow(wxCloseEvent &event);

    wxString m_sPath;
    wxStyledTextCtrl *m_pText = nullptr;
    wxButton *m_pSaveButton = nullptr;
};

#endif
