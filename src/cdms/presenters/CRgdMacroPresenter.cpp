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

#include "CRgdMacroPresenter.h"
#include "async/CProgressChannel.h"
#include "common/strconv.h"
#include <algorithm>
#include <cstring>
#include <future>
#include <map>

extern "C"
{
#include "rainman/util/crc32_case_idt.h"
}

CRgdMacroPresenter::CRgdMacroPresenter(IRgdMacroView &view, wxEvtHandler *pHandler)
    : m_view(view), m_pHandler(pHandler), m_taskRunner(pHandler)
{
}

//! Context struct passed as pTag to CRgdFileMacro callbacks on the background thread.
struct MacroCallbackContext
{
    wxEvtHandler *pHandler;
    IRgdMacroView *pView;

    // Permission flags (background-thread-local, no sync needed)
    bool bAllowDebug = false;
    bool bAllowIO = false;
    bool bAllowOS = false;
    bool bAllowSave = false;
    bool bAllowLoad = false;

    // Sandbox path info
    unsigned long iPathLen = 0;
    unsigned long iPathHash = 0;

    // Folder tracking for post-run tree refresh
    std::map<unsigned long, std::string> mapToUpdate;
};

// Blocks the background thread while the main thread shows a permission dialog.
static bool RequestPermissionFromMainThread(MacroCallbackContext *pCtx, const wxString &sQuestion)
{
    std::promise<bool> promise;
    auto future = promise.get_future();
    pCtx->pHandler->CallAfter(
        [pView = pCtx->pView, sQuestion, pPromise = &promise]()
        {
            bool bResult = pView->RequestPermission(sQuestion);
            pPromise->set_value(bResult);
        });
    return future.get();
}

static void CallbackPrint(void *pTag, const char *sMsg)
{
    auto *pCtx = static_cast<MacroCallbackContext *>(pTag);
    wxString sText = AsciiTowxString(sMsg);
    pCtx->pHandler->CallAfter([pView = pCtx->pView, sText]() { pView->AppendOutput(sText); });
}

static bool CallbackSecurity(void *pTag, CRgdFileMacro::eSecurityTypes eType)
{
    auto *pCtx = static_cast<MacroCallbackContext *>(pTag);
    switch (eType)
    {
    case CRgdFileMacro::ST_DebugLib:
        if (!pCtx->bAllowDebug)
        {
            if (!(pCtx->bAllowDebug =
                      RequestPermissionFromMainThread(pCtx, wxT("to use the LUA reflexive debug library"))))
                return false;
        }
        break;
    case CRgdFileMacro::ST_IOLib:
        if (!pCtx->bAllowIO)
        {
            if (!(pCtx->bAllowIO = RequestPermissionFromMainThread(
                      pCtx, wxT("to use the LUA I/O library.\nWarning: This can be used to read and write any file on "
                                "your computer"))))
                return false;
        }
        break;
    case CRgdFileMacro::ST_OSLib:
        if (!pCtx->bAllowOS)
        {
            if (!(pCtx->bAllowOS = RequestPermissionFromMainThread(
                      pCtx, wxT("to use the LUA OS library.\nWarning: This can be used to run programs on your "
                                "computer, change computer settings and delete files from disk"))))
                return false;
        }
        break;
    }
    return true;
}

static bool CallbackLoad(void *pTag, const char *sFile)
{
    auto *pCtx = static_cast<MacroCallbackContext *>(pTag);
    if (!pCtx->bAllowLoad)
    {
        std::string sFile2(sFile);
        for (char &s : sFile2)
        {
            if (s == '/')
                s = '\\';
        }
        unsigned long iHashRoot =
            crc32_case_idt(0, reinterpret_cast<const unsigned char *>(sFile2.c_str()),
                           std::min(pCtx->iPathLen, static_cast<unsigned long>(sFile2.size())));
        if (iHashRoot != pCtx->iPathHash)
        {
            if (!(pCtx->bAllowLoad = RequestPermissionFromMainThread(
                      pCtx,
                      wxT("to load RGD files from locations outside of the folder that the macro is being run over"))))
            {
                return false;
            }
        }
    }
    return true;
}

static bool CallbackSave(void *pTag, const char *sFile)
{
    auto *pCtx = static_cast<MacroCallbackContext *>(pTag);
    std::string sFile2(sFile);
    for (char &s : sFile2)
    {
        if (s == '/')
            s = '\\';
    }

    if (!pCtx->bAllowSave)
    {
        unsigned long iHashRoot = crc32_case_idt(0, reinterpret_cast<const unsigned char *>(sFile2.c_str()),
                                                 std::min(pCtx->iPathLen, static_cast<unsigned long>(sFile2.size())));
        if (iHashRoot != pCtx->iPathHash)
        {
            if (!(pCtx->bAllowSave = RequestPermissionFromMainThread(
                      pCtx,
                      wxT("to save RGD files to locations outside of the folder that the macro is being run over"))))
            {
                return false;
            }
        }
    }

    const size_t iSlashLocation = sFile2.find_last_of('\\');
    if (iSlashLocation != std::string::npos)
        sFile2.resize(iSlashLocation);

    unsigned long iHash =
        crc32_case_idt(0, reinterpret_cast<const unsigned char *>(sFile2.c_str()), static_cast<unsigned int>(sFile2.size()));
    if (pCtx->mapToUpdate.find(iHash) == pCtx->mapToUpdate.end())
        pCtx->mapToUpdate[iHash] = sFile2;

    return true;
}

bool CRgdMacroPresenter::RunMacro(const wxString &sMacroText, std::vector<std::string> vFiles, IFileStore *pFileStore,
                                  CRgdHashTable *pHashTable, CModuleFile *pModule, bool bIsDowMod,
                                  unsigned long iPathLen, unsigned long iPathHash)
{
    if (m_taskRunner.IsRunning())
        return false;

    m_view.DisableControls();
    m_view.ShowProgress(wxT("Running macro..."));

    auto *pHandler = m_pHandler;
    auto *pView = &m_view;

    return m_taskRunner.RunAsync<void>(
        // Background thread
        [sMacroText, vFiles = std::move(vFiles), pFileStore, pHashTable, pModule, bIsDowMod, iPathLen, iPathHash,
         pHandler, pView](CProgressChannel & /*progress*/, CCancellationToken & /*cancel*/)
        {
            MacroCallbackContext ctx;
            ctx.pHandler = pHandler;
            ctx.pView = pView;
            ctx.iPathLen = iPathLen;
            ctx.iPathHash = iPathHash;

            CRgdFileMacro oMacro;
            oMacro.setHashTable(pHashTable);
            oMacro.setCallbackTag(&ctx);
            oMacro.setCallbackPrint(CallbackPrint);
            oMacro.setCallbackSave(CallbackSave);
            oMacro.setCallbackLoad(CallbackLoad);
            oMacro.setCallbackSecurity(CallbackSecurity);
            oMacro.setUcsResolver(pModule);
            oMacro.setIsDowMod(bIsDowMod);

            auto saContent = wxStringToAscii(sMacroText);
            oMacro.loadMacro(saContent.get());

            // Run macro on each file, collecting errors
            std::vector<wxString> vErrors;
            for (const auto &sFile : vFiles)
            {
                try
                {
                    oMacro.runMacro(sFile.c_str(), pFileStore);
                }
                catch (const CRainmanException &e)
                {
                    wxString sMsg;
                    const CRainmanException *pCur = &e;
                    while (pCur)
                    {
                        if (!sMsg.empty())
                            sMsg += wxT("\n  caused by: ");
                        sMsg += wxString::FromUTF8(pCur->getMessage());
                        pCur = pCur->getPrecursor();
                    }
                    vErrors.push_back(std::move(sMsg));
                }
            }

            try
            {
                oMacro.runAtEnd();
            }
            catch (const CRainmanException &e)
            {
                wxString sMsg;
                const CRainmanException *pCur = &e;
                while (pCur)
                {
                    if (!sMsg.empty())
                        sMsg += wxT("\n  caused by: ");
                    sMsg += wxString::FromUTF8(pCur->getMessage());
                    pCur = pCur->getPrecursor();
                }
                vErrors.push_back(std::move(sMsg));
            }

            oMacro.unloadMacro();

            // Report per-file errors on main thread
            if (!vErrors.empty())
            {
                wxString sCombined;
                for (const auto &sErr : vErrors)
                {
                    if (!sCombined.empty())
                        sCombined += wxT("\n\n");
                    sCombined += sErr;
                }
                pHandler->CallAfter([pView, sCombined]() { pView->ShowError(sCombined); });
            }

            // Collect folder paths to update
            std::vector<wxString> vFolders;
            vFolders.reserve(ctx.mapToUpdate.size());
            for (const auto &pair : ctx.mapToUpdate)
                vFolders.push_back(AsciiTowxString(pair.second.c_str()));

            // Post folder list to main thread for tree refresh
            pHandler->CallAfter([pView, vFolders = std::move(vFolders)]() { pView->OnMacroComplete(vFolders); });
        },
        // Progress (main thread)
        [this](const std::string &sMsg) { m_view.ShowProgress(wxString::FromUTF8(sMsg)); },
        // Done (main thread)
        [this](Result<void> result)
        {
            m_view.HideProgress();
            m_view.EnableControls();

            if (!result.ok())
                m_view.ShowError(result.error());
        });
}

void CRgdMacroPresenter::Cancel() { m_taskRunner.Cancel(); }

bool CRgdMacroPresenter::IsRunning() const { return m_taskRunner.IsRunning(); }
