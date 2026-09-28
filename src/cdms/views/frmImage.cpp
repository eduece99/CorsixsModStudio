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

#include "frmImage.h"
#include "frame/Construct.h"
#include "common/strings.h"
#include "common/config.h"
#include "common/Utility.h"
#include <wx/msgdlg.h>
#include <wx/mstream.h>

#include <utility>
#include <memory>
#include "common/Common.h"
#include "common/ThemeColours.h"

BEGIN_EVENT_TABLE(frmImageViewer, wxWindow)
EVT_SIZE(frmImageViewer::OnSize)
EVT_RADIOBOX(IDC_SaveFileExt, frmImageViewer::OnRadioButtonSaveExt)
EVT_BUTTON(wxID_SAVE, frmImageViewer::OnSave)
END_EVENT_TABLE()

class wxMyScrolledWindow : public wxScrolledWindow
{
  public:
    wxMyScrolledWindow(wxWindow *parent, wxBitmap *pBitmap, wxWindowID id = -1, const wxPoint &pos = wxDefaultPosition,
                       const wxSize &size = wxDefaultSize, long style = wxHSCROLL | wxVSCROLL,
                       const wxString &name = wxT("scrolledWindow"))
        : wxScrolledWindow(parent, id, pos, size, style, name), m_pBitmap(pBitmap)
    {
        m_pDC = std::make_unique<wxMemoryDC>(*m_pBitmap);

        SetVirtualSize(pBitmap->GetWidth(), pBitmap->GetHeight());
        SetScrollRate(8, 8);
    }

    ~wxMyScrolledWindow() override = default;

    void OnDraw(wxDC &dc) override { dc.Blit(0, 0, m_pBitmap->GetWidth(), m_pBitmap->GetHeight(), m_pDC.get(), 0, 0); }

  protected:
    wxBitmap *m_pBitmap;
    std::unique_ptr<wxMemoryDC> m_pDC;
};

/*
BEGIN_EVENT_TABLE(wxMyScrolledWindow, wxScrolledWindow)
    EVT_PAINT(wxMyScrolledWindow::OnPaint)
END_EVENT_TABLE()
*/

frmImageViewer::frmImageViewer(wxTreeItemId &oFileParent, wxString sFilename, wxWindow *parent, wxWindowID id,
                               CRgtFile *pImage, bool bOwnImage, const wxPoint &pos, const wxSize &size)
    : m_oFileParent(oFileParent), m_sFilename(std::move(sFilename)), wxWindow(parent, id, pos, size),
      m_presenter(*this), m_taskRunner(this)
{
    auto *pTopSizer = new wxBoxSizer(wxVERTICAL);

    m_pRgtFile = pImage;
    m_pImageBitmap = nullptr;
    m_bOwnRgt = bOwnImage;

    m_pImgContentSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Current Image"));

    wxArrayString aFileTypes, aCompressionTypes;

    aFileTypes.Add(wxT("RGT"));
    aFileTypes.Add(wxT("DDS"));
    aFileTypes.Add(wxT("TGA"));

    aCompressionTypes.Add(wxT("RGB (no alpha)"));
    aCompressionTypes.Add(wxT("RGBA (8 bit alpha)"));
    aCompressionTypes.Add(wxT("DXT1 (1 bit alpha)"));
    aCompressionTypes.Add(wxT("DXT3 (4 bit alpha)"));
    aCompressionTypes.Add(wxT("DXT5 (4 bit alpha)"));

    wxRadioBox *pRadioBox;
    m_pImgContentSizer->Add(m_pCurrentExt = pRadioBox =
                                new wxRadioBox(this, -1, wxT("File"), wxDefaultPosition, wxDefaultSize, aFileTypes),
                            0, wxEXPAND | wxALL, 0);

    pRadioBox->SetSelection(0); // RGT
    pRadioBox->Enable(false);

    m_pImgContentSizer->Add(m_pCurrentCompression = pRadioBox = new wxRadioBox(
                                this, -1, wxT("Compression"), wxDefaultPosition, wxDefaultSize, aCompressionTypes),
                            0, wxEXPAND | wxALL, 0);

    switch (pImage->GetImageFormat())
    {
    case CRgtFile::IF_Tga:
        pRadioBox->SetSelection(1); // RGBA
        break;
    case CRgtFile::IF_Dxtc:
        switch (pImage->GetProperty(CRgtFile::IP_CompressionLevel))
        {
        case 1:
            pRadioBox->SetSelection(2);
            break;
        case 3:
            pRadioBox->SetSelection(3);
            break;
        case 5:
            pRadioBox->SetSelection(4);
            break;
        default:
            break;
        }
        break;
    }
    pRadioBox->Enable(false);

    wxCheckBox *pCheckBox;
    m_pImgContentSizer->Add(pCheckBox = new wxCheckBox(this, -1, wxT("Mip Levels")), 0, wxEXPAND | wxALL, 3);

    pCheckBox->SetValue(pImage->GetProperty(CRgtFile::IP_MipLevelCount) > 1);
    pCheckBox->Enable(false);

    // Placeholder while image decodes on background thread
    m_pLoadingLabel = new wxStaticText(this, wxID_ANY, wxT("Loading image..."), wxDefaultPosition, wxDefaultSize,
                                       wxALIGN_CENTER_HORIZONTAL);
    m_pImgContentSizer->Add(m_pLoadingLabel, 1, wxALIGN_CENTER | wxALL, 20);

    pTopSizer->Add(m_pImgContentSizer, 1, wxEXPAND | wxALL, 3);

    auto *pSaveSizer = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Save Copy"));

    pSaveSizer->Add(m_pSaveFileExt = new wxRadioBox(this, IDC_SaveFileExt, wxT("File"), wxDefaultPosition,
                                                    wxDefaultSize, aFileTypes),
                    0, wxEXPAND | wxALL, 0);
    pSaveSizer->Add(m_pSaveFileCompression = new wxRadioBox(this, -1, wxT("Compression"), wxDefaultPosition,
                                                            wxDefaultSize, aCompressionTypes),
                    0, wxEXPAND | wxALL, 0);
    pSaveSizer->Add(m_pSaveFileMips = new wxCheckBox(this, -1, wxT("Mip Levels")), 0, wxEXPAND | wxALL, 3);

    pSaveSizer->Add(new wxButton(this, wxID_SAVE, wxT("Save Copy")), 0, wxALL, 3);

    switch (pImage->GetImageFormat())
    {
    case CRgtFile::IF_Tga:
        m_pSaveFileExt->SetSelection(2);
        m_pSaveFileCompression->SetSelection(1);
        m_pSaveFileCompression->Enable(2, false);
        m_pSaveFileCompression->Enable(3, false);
        m_pSaveFileCompression->Enable(4, false);
        m_pSaveFileMips->Enable(false);
        break;
    case CRgtFile::IF_Dxtc:
        m_pSaveFileExt->SetSelection(1);
        m_pSaveFileCompression->Enable(0, false);
        m_pSaveFileCompression->Enable(1, false);
        m_pSaveFileCompression->SetSelection(pRadioBox->GetSelection());
        break;
    }

    pTopSizer->Add(pSaveSizer, 0, wxEXPAND | wxALL, 3);

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);

    // Decode image on background thread (wxImage is thread-safe, wxBitmap is not)
    auto *pRgt = pImage;
    m_taskRunner.RunAsync<wxImage>(
        [pRgt](CProgressChannel &, CCancellationToken &) -> wxImage
        {
            CMemoryStore::COutStream *pTgaSpace = CMemoryStore::OpenOutputStreamExt();
            pRgt->SaveTGA(pTgaSpace);
            wxMemoryInputStream oTgaSpace(pTgaSpace->GetData(), pTgaSpace->GetDataLength());
            wxImage oTga(oTgaSpace, wxBITMAP_TYPE_TGA);
            delete pTgaSpace;
            return oTga;
        },
        [](const std::string &) {},
        [this](Result<wxImage> result)
        {
            if (!result.ok())
            {
                if (m_pLoadingLabel)
                {
                    m_pLoadingLabel->SetLabel(wxT("Failed to load image"));
                }
                return;
            }

            m_pImageBitmap = new wxBitmap(std::move(result).value());

            // Swap placeholder for scrolled image window
            if (m_pLoadingLabel)
            {
                m_pImgContentSizer->Detach(m_pLoadingLabel);
                m_pLoadingLabel->Destroy();
                m_pLoadingLabel = nullptr;
            }
            m_pImgContentSizer->Add(new wxMyScrolledWindow(this, m_pImageBitmap), 1, wxEXPAND | wxALL, 0);
            Layout();
        });
}

void frmImageViewer::SetIsTga(bool bOnlyRGB)
{
    m_pCurrentExt->SetSelection(2);
    m_pCurrentCompression->SetSelection(bOnlyRGB ? 0 : 1);
    m_pSaveFileExt->SetSelection(0);

    m_pSaveFileCompression->Enable(0, false);
    m_pSaveFileCompression->Enable(1, true);
    m_pSaveFileCompression->Enable(2, true);
    m_pSaveFileCompression->Enable(3, true);
    m_pSaveFileCompression->Enable(4, true);
    if (m_pSaveFileCompression->GetSelection() == 0)
    {
        m_pSaveFileCompression->SetSelection(1);
    }
    m_pSaveFileMips->Enable(true);
}

void frmImageViewer::SetIsDds()
{
    m_pCurrentExt->SetSelection(1);
    m_pSaveFileExt->SetSelection(0);

    m_pSaveFileCompression->Enable(0, false);
    m_pSaveFileCompression->Enable(1, false);
    m_pSaveFileCompression->Enable(2, true);
    m_pSaveFileCompression->Enable(3, true);
    m_pSaveFileCompression->Enable(4, true);
    if (m_pSaveFileCompression->GetSelection() == 0)
    {
        m_pSaveFileCompression->SetSelection(2);
    }
    if (m_pSaveFileCompression->GetSelection() == 1)
    {
        m_pSaveFileCompression->SetSelection(3);
    }
    m_pSaveFileMips->Enable(true);
}

frmImageViewer::~frmImageViewer()
{
    delete m_pImageBitmap;
    m_pImageBitmap = nullptr;
    if (m_bOwnRgt && m_pRgtFile)
    {
        delete m_pRgtFile;
    }
}

void frmImageViewer::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

void frmImageViewer::OnRadioButtonSaveExt(wxCommandEvent &event)
{
    switch (event.GetSelection())
    {
    case 0: // RGT
        m_pSaveFileCompression->Enable(0, false);
        m_pSaveFileCompression->Enable(1, true);
        m_pSaveFileCompression->Enable(2, true);
        m_pSaveFileCompression->Enable(3, true);
        m_pSaveFileCompression->Enable(4, true);
        if (m_pSaveFileCompression->GetSelection() == 0)
        {
            m_pSaveFileCompression->SetSelection(1);
        }
        m_pSaveFileMips->Enable(true);
        break;
    case 1: // DDS
        m_pSaveFileCompression->Enable(0, false);
        m_pSaveFileCompression->Enable(1, false);
        m_pSaveFileCompression->Enable(2, true);
        m_pSaveFileCompression->Enable(3, true);
        m_pSaveFileCompression->Enable(4, true);
        if (m_pSaveFileCompression->GetSelection() == 0)
        {
            m_pSaveFileCompression->SetSelection(2);
        }
        if (m_pSaveFileCompression->GetSelection() == 1)
        {
            m_pSaveFileCompression->SetSelection(3);
        }
        m_pSaveFileMips->Enable(true);
        break;
    case 2: // TGA
        m_pSaveFileCompression->Enable(0, true);
        m_pSaveFileCompression->Enable(1, true);
        m_pSaveFileCompression->Enable(2, false);
        m_pSaveFileCompression->Enable(3, false);
        m_pSaveFileCompression->Enable(4, false);
        if (m_pSaveFileCompression->GetSelection() > 1)
        {
            m_pSaveFileCompression->SetSelection(1);
        }
        m_pSaveFileMips->Enable(false);
        m_pSaveFileMips->SetValue(false);
        break;
    default:
        break;
    }
}

void frmImageViewer::OnSave(wxCommandEvent &event)
{
    UNUSED(event);

    auto eTargetFormat = static_cast<CImagePresenter::SaveFormat>(m_pSaveFileExt->GetSelection());

    if (CImagePresenter::WouldOverwrite(m_pCurrentExt->GetSelection(), eTargetFormat))
    {
        if (!ConfirmOverwrite())
        {
            return;
        }
    }

    wxString sNewFile = CImagePresenter::ComputeOutputPath(m_sFilename, eTargetFormat);

    // Open output stream via FileService
    auto streamResult = TheConstruct->GetFileService().OpenOutputStream(sNewFile, true);
    if (!streamResult)
    {
        ShowError(streamResult.error());
        return;
    }

    IFileStore::IOutputStream *pStream = streamResult.value().get();

    if (m_presenter.SaveToStream(m_pRgtFile, pStream, eTargetFormat, m_pSaveFileCompression->GetSelection(),
                                 m_pSaveFileMips->GetValue()))
    {
        OnSaveComplete(sNewFile);
    }
}

// --- IImageView implementation ---

bool frmImageViewer::ConfirmOverwrite()
{
    return ThemeColours::ShowMessageBox(
               wxT("The options selected will result in the original file being overwritten. Continue?"), wxT("Save"),
               wxOK | wxCANCEL) != wxCANCEL;
}

void frmImageViewer::ShowError(const wxString &sMessage) { ErrorBoxS(sMessage); }

void frmImageViewer::OnSaveComplete(const wxString &sNewFile)
{
    wxString sDir = sNewFile.BeforeLast('\\');
    auto dirResult = TheConstruct->GetFileService().Iterate(sDir);
    if (!dirResult)
    {
        ShowError(dirResult.error());
        return;
    }

    try
    {
        TheConstruct->GetFilesList()->UpdateDirectoryChildren(m_oFileParent, dirResult.value().get());
    }
    catch (const CRainmanException &e)
    {
        ErrorBoxE(e);
    }
}
