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

#include "frmCredits.h"
#include "resource.h"
#include "common/Common.h"
#include "common/ThemeColours.h"
#include "common/BitmapResources.h"

BEGIN_EVENT_TABLE(frmCredits, wxDialog)
EVT_PAINT(frmCredits::OnPaint)
END_EVENT_TABLE()

frmCredits::frmCredits()
    : wxDialog(wxTheApp->GetTopWindow(), -1, wxT("Credits"), wxPoint(0, 0),
               wxWindow::FromDIP(wxSize(384, 474), wxTheApp->GetTopWindow()),
               wxFRAME_FLOAT_ON_PARENT | wxDEFAULT_DIALOG_STYLE)
{
    CentreOnParent();
    m_pText = nullptr;

    wxString sCredits;

    sCredits.Append(wxT("Programming: Corsix, uses squish library (C) 2006 Simon Brown\n"));
    sCredits.Append(wxT("Modernization: Jack Belford\n"));
    sCredits.Append(wxT("Artwork: Silvestre Herrera (Nuovo iconset), Corsix, GPL-ed artwork\n"));
    sCredits.Append(wxT("Beta testing: Compiler, davisbe, Delphy, Excedrin, Finaldeath, kjon, Kresjah, "
                        "irc.hwcommunity.com #dowmods channel\n"));
    sCredits.Append(wxT("Donators: snake_risken, Delphy, Mannerheim, KoGar, Julian Harris, CrizeCaldron, Kutter, "
                        "MasterofOblivion, Rick Funk\n"));
    sCredits.Append(
        wxT("\nThis program is free software licensed under the GNU GPL.  Email modstudio@corsix.org for details\n"
            "Definitive Edition code is located at github.com/jbelford/CorsixModStudio"));

    m_pLoadingImage =
        std::make_unique<wxBitmap>(CDMSBitmapResources::LoadBitmap(wxT("RIDB_LOADING"), wxT("logo_smaller.bmp")));
    if (!m_pLoadingImage->IsOk())
    {
        wxLogError(wxT("Failed to load the credits bitmap."));
        m_pLoadingImage.reset();
    }
    m_pText = new wxStaticText(this, -1, sCredits, FromDIP(wxPoint(0, 317)), FromDIP(wxSize(384, 167)),
                               wxST_NO_AUTORESIZE | wxALIGN_LEFT);
    // m_pText->Wrap(384);
    m_pText->SetBackgroundStyle(wxBG_STYLE_COLOUR);
    m_pText->SetBackgroundColour(ThemeColours::DialogBg());
    wxFont f = m_pText->GetFont();
    f.SetWeight(wxFONTWEIGHT_BOLD);
    m_pText->SetFont(f);
}

frmCredits::~frmCredits() = default;

void frmCredits::OnPaint(wxPaintEvent &event)
{
    UNUSED(event);
    wxPaintDC dc(this);
    if (m_pLoadingImage)
    {
        wxMemoryDC temp_dc;
        temp_dc.SelectObject(*m_pLoadingImage);
        dc.Blit(0, 0, 384, 384, &temp_dc, 0, 0);
    }
}

void frmCredits::SetMessage(wxString &sMsg) { m_pText->SetLabel(sMsg); }
