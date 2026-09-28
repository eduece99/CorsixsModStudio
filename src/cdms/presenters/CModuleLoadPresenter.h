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

#include "async/CWxTaskRunner.h"
#include "views/interfaces/IMainFrameView.h"
#include <wx/string.h>

//! Presenter for asynchronous module loading.
/*!
    Owns a CWxTaskRunner and coordinates background loading of .module
    and .sga files. All heavy Rainman operations execute on a background
    thread; the IMainFrameView callbacks are invoked on the main thread.

    The caller (ConstructFrame) is responsible for all UI dialogs (file
    picker, locale selection, config) before calling LoadMod/LoadSga.
*/
class CModuleLoadPresenter
{
  public:
    CModuleLoadPresenter(IMainFrameView &view, wxEvtHandler *pHandler);

    //! Load a .module file asynchronously.
    /*!
        \param sFilePath        Resolved path to the .module file.
        \param sLocale          Locale string (e.g. "english").
        \param bIsCoH           true if loading a Company of Heroes module.
        \param sMyDocumentsPath  Path to My Documents\My Games\Company of Heroes (CoH only).
        \param sGameInstallPath  Validated DoW:DE installation root, or empty for legacy loading.
        \return true if the load was started, false if a load is already running.
    */
    bool LoadMod(const wxString &sFilePath, const wxString &sLocale, bool bIsCoH,
                 const wxString &sMyDocumentsPath = wxT(""), const wxString &sGameInstallPath = wxT(""));

    //! Load an .sga file as a faux module asynchronously.
    /*!
        \param sFilePath  Path to the .sga file.
        \return true if the load was started, false if a load is already running.
    */
    bool LoadSga(const wxString &sFilePath);

    //! Cancel the currently running load operation.
    void Cancel();

    //! Check whether a load operation is in progress.
    bool IsLoading() const;

  private:
    IMainFrameView &m_view;
    CWxTaskRunner m_taskRunner;
};
