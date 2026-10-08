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

#include "Application.h"
#include "frame/Construct.h"
#include "strings.h"
#include "Utility.h"
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/sysopt.h>
#include <wx/image.h>
#include <wx/stdpaths.h>
#include <wx/version.h>
#include "Common.h"
#include "rainman/core/RainmanLog.h"

IMPLEMENT_APP(CDMSApplication)

bool CDMSApplication::OnInit()
{
    RainmanLog::init();
    CDMS_LOG_INFO("CorsixModStudio application starting");
    wxSystemOptions::SetOption(wxT("msw.remap"), 0);

    // wxFileConfig prepends APPDATA to relative paths, so resolve to absolute
    wxString sAbsConfigPath = wxFileName(AppStr(configfile_name)).GetAbsolutePath();
    wxConfigBase::Set(
        new wxFileConfig(wxEmptyString, wxEmptyString, sAbsConfigPath, wxEmptyString, wxCONFIG_USE_LOCAL_FILE));

    wxConfigBase::Get()->SetRecordDefaults(
        true); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks) — wxConfigBase::Set() takes ownership
    wxConfigBase::Get()->SetExpandEnvVars(false);
    wxConfigBase::Get()->SetPath(AppStr(config_initialpath));

#if wxCHECK_VERSION(3, 3, 0)
    // Apply saved appearance preference (0=system, 1=light, 2=dark)
    int iAppearance = 0;
    wxConfigBase::Get()->Read(AppStr(config_appearance), &iAppearance, 0);
    if (iAppearance == 1)
    {
        SetAppearance(Appearance::Light);
    }
    else if (iAppearance == 2)
    {
        SetAppearance(Appearance::Dark);
    }
    else
    {
        SetAppearance(Appearance::System);
    }
#endif

    wxImage::AddHandler(new wxTGAHandler);

    //_CrtSetBreakAlloc(5143);

    auto *pConstruct = new ConstructFrame(AppStr(app_name), wxDefaultPosition, wxSize(500, 500));
#ifdef _WIN32
    pConstruct->SetIcon(wxIcon(wxT("APPICON"), wxBITMAP_TYPE_ICO_RESOURCE));
#else
    const wxString sIconPath = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPathWithSep() + wxT("icon.ico");
    const wxIcon oIcon(sIconPath, wxBITMAP_TYPE_ICO);
    if (oIcon.IsOk())
    {
        pConstruct->SetIcon(oIcon);
    }
    else
    {
        wxLogError(wxT("Failed to load application icon: ") + sIconPath);
    }
#endif
    pConstruct->Show(true);
    pConstruct->Maximize(true);
    SetTopWindow(pConstruct);
    return true;
}

int CDMSApplication::OnExit()
{
    CDMS_LOG_INFO("CorsixModStudio application shutting down");
    auto *pConfig = (wxFileConfig *)wxConfigBase::Get();
    pConfig->Flush();
    return wxApp::OnExit();
}

#ifndef _DEBUG
bool CDMSApplication::OnExceptionInMainLoop() { throw; }
#endif

int CDMSApplication::OnRun()
{
#ifndef _DEBUG
    try
    {
#endif
        return wxApp::OnRun();
#ifndef _DEBUG
    }
    catch (const CRainmanException &e)
    {
        _ErrorBox(e, __FILE__, __LINE__, true);
        return -1;
    }
    catch (...)
    {
        _ErrorBox(wxT("Unhandled exception (not from Rainman)"), __FILE__, __LINE__);
    }
#endif
}
