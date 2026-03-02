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
// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
// ----------------------------
#include "interfaces/IImageView.h"
#include "presenters/CImagePresenter.h"
#include "async/CWxTaskRunner.h"
#include <rainman/formats/CRgtFile.h>
#include <wx/treectrl.h>
#include <wx/propgrid/propgrid.h>

class frmImageViewer : public wxWindow, public IImageView
{
  protected:
    CRgtFile *m_pRgtFile;
    wxBitmap *m_pImageBitmap;
    wxRadioBox *m_pSaveFileExt, *m_pSaveFileCompression, *m_pCurrentExt, *m_pCurrentCompression;
    wxCheckBox *m_pSaveFileMips;
    wxString m_sFilename;
    wxTreeItemId m_oFileParent;
    bool m_bOwnRgt;
    wxStaticText *m_pLoadingLabel = nullptr;
    wxStaticBoxSizer *m_pImgContentSizer = nullptr;

  public:
    frmImageViewer(wxTreeItemId &oFileParent, wxString sFilename, wxWindow *parent, wxWindowID id, CRgtFile *pImage,
                   bool bOwnImage, const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);
    virtual ~frmImageViewer();

    void SetIsTga(bool bOnlyRGB);
    void SetIsDds();

    void OnSize(wxSizeEvent &event);
    void OnSave(wxCommandEvent &event);

    void OnRadioButtonSaveExt(wxCommandEvent &event);

    // IImageView implementation
    bool ConfirmOverwrite() override;
    void ShowError(const wxString &sMessage) override;
    void OnSaveComplete(const wxString &sNewFile) override;

    enum
    {
        IDC_SaveFileExt = wxID_HIGHEST + 1,
    };

    DECLARE_EVENT_TABLE()

  private:
    CImagePresenter m_presenter;
    CWxTaskRunner m_taskRunner; // Must be declared last — destroyed first to join background thread
};
