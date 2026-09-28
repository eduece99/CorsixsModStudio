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

#ifndef _FRM_DE_ARCHIVE_H_
#define _FRM_DE_ARCHIVE_H_

#include <wx/dialog.h>
#include <wx/choice.h>
#include <wx/textctrl.h>

// Standalone path-based entry point: .burn and .sgaconfig do not need to be in the VFS.
class frmDeArchive : public wxDialog
{
  public:
    // Pass the loaded .module path, its ModFolder value (e.g. "Mod"), and the
    // DE game install directory. Finds .burn in the project and .sgaconfig in ModFolder.
    frmDeArchive(wxWindow *pParent, const wxString &sModulePath, const wxString &sModFolderName,
                 const wxString &sGameInstall);

  private:
    void RefreshConfigs();
    wxString SelectedConfigPath() const;
    void OnBrowseConfig(wxCommandEvent &event);
    void OnBrowseSource(wxCommandEvent &event);
    void OnBrowseOutput(wxCommandEvent &event);
    void OnConfigChanged(wxCommandEvent &event);
    void OnBrowseExecutable(wxCommandEvent &event);
    void OnArchive(wxCommandEvent &event);

    wxString m_sModFolder;
    wxString m_sProjectRoot;
    wxChoice *m_pConfigs;
    wxTextCtrl *m_pExecutable;
    wxTextCtrl *m_pSourceFolder;
    wxTextCtrl *m_pOutputFile;
};

#endif
