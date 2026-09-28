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

#include "CModuleLoadPresenter.h"
#include "async/CProgressChannel.h"
#include "services/ModuleService.h"
#include "common/strconv.h"

CModuleLoadPresenter::CModuleLoadPresenter(IMainFrameView &view, wxEvtHandler *pHandler)
    : m_view(view), m_taskRunner(pHandler)
{
}

bool CModuleLoadPresenter::LoadMod(const wxString &sFilePath, const wxString &sLocale, bool bIsCoH,
                                   const wxString &sMyDocumentsPath, const wxString &sGameInstallPath)
{
    if (m_taskRunner.IsRunning())
    {
        return false;
    }

    m_view.ShowLoadingDialog(wxT("Loading module..."));
    m_view.DisableLoadMenuItems();

    return m_taskRunner.RunAsync<CModuleFile *>(
        // Background thread
        [sFilePath, sLocale, bIsCoH, sMyDocumentsPath, sGameInstallPath](CProgressChannel &progress,
                                                                         CCancellationToken &cancel) -> CModuleFile *
        {
            auto sFile = wxStringToAscii(sFilePath);
            if (!sFile)
            {
                throw CRainmanException(__FILE__, __LINE__, "Memory allocation error");
            }

            auto *pMod = new CModuleFile;

            try
            {
                // Set locale before loading
                auto sLoc = wxStringToAscii(sLocale);
                if (sLoc)
                {
                    pMod->SetLocale(sLoc.get());
                }

                // Load the module file
                pMod->LoadModuleFile(sFile.get(), CProgressChannel::RainmanCallback, &progress);
                if (!sGameInstallPath.empty())
                {
                    auto sInstall = wxStringToAscii(sGameInstallPath);
                    if (!sInstall)
                    {
                        throw CRainmanException(__FILE__, __LINE__, "Memory allocation error");
                    }
                    pMod->SetGameInstallPath(sInstall.get());
                }

                if (cancel.IsCancelled())
                {
                    delete pMod;
                    return nullptr;
                }

                // Set CoH map pack root if needed
                if (bIsCoH && pMod->GetModuleType() == CModuleFile::MT_CompanyOfHeroes)
                {
                    pMod->SetMapPackRootFolder(sMyDocumentsPath.wc_str());
                }

                if (cancel.IsCancelled())
                {
                    delete pMod;
                    return nullptr;
                }

                // Reload all resources
                pMod->ReloadResources(CModuleFile::RR_All, CModuleFile::RR_All, CModuleFile::RR_All,
                                      CProgressChannel::RainmanCallback, &progress);

                if (cancel.IsCancelled())
                {
                    delete pMod;
                    return nullptr;
                }
            }
            catch (...)
            {
                delete pMod;
                throw;
            }

            return pMod;
        },
        // Progress (main thread)
        [this](const std::string &sMsg) { m_view.UpdateLoadingProgress(wxString::FromUTF8(sMsg)); },
        // Done (main thread)
        [this, sFilePath](Result<CModuleFile *> result)
        {
            m_view.HideLoadingDialog();
            m_view.EnableLoadMenuItems();

            if (!result.ok())
            {
                m_view.ShowError(result.error());
                return;
            }

            CModuleFile *pMod = result.value();
            if (pMod == nullptr)
            {
                return; // Cancelled
            }

            m_view.OnModuleLoaded(pMod, sFilePath, false);
        });
}

bool CModuleLoadPresenter::LoadSga(const wxString &sFilePath)
{
    if (m_taskRunner.IsRunning())
    {
        return false;
    }

    m_view.ShowLoadingDialog(wxT("Loading SGA archive..."));
    m_view.DisableLoadMenuItems();

    return m_taskRunner.RunAsync<CModuleFile *>(
        // Background thread
        [sFilePath](CProgressChannel &progress, CCancellationToken & /*cancel*/) -> CModuleFile *
        {
            auto sFile = wxStringToAscii(sFilePath);
            if (!sFile)
            {
                throw CRainmanException(__FILE__, __LINE__, "Memory allocation error");
            }

            auto *pMod = new CModuleFile;
            try
            {
                pMod->LoadSgaAsMod(sFile.get(), CProgressChannel::RainmanCallback, &progress);
            }
            catch (...)
            {
                delete pMod;
                throw;
            }
            return pMod;
        },
        // Progress (main thread)
        [this](const std::string &sMsg) { m_view.UpdateLoadingProgress(wxString::FromUTF8(sMsg)); },
        // Done (main thread)
        [this, sFilePath](Result<CModuleFile *> result)
        {
            m_view.HideLoadingDialog();
            m_view.EnableLoadMenuItems();

            if (!result.ok())
            {
                m_view.ShowError(result.error());
                return;
            }

            CModuleFile *pMod = result.value();
            if (pMod == nullptr)
            {
                return; // Cancelled
            }

            m_view.OnModuleLoaded(pMod, sFilePath, true);
        });
}

void CModuleLoadPresenter::Cancel() { m_taskRunner.Cancel(); }

bool CModuleLoadPresenter::IsLoading() const { return m_taskRunner.IsRunning(); }
