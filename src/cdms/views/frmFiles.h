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
#include <rainman/io/IDirectoryTraverser.h>
#include <rainman/io/IFileStore.h>
#include <rainman/module/CModuleFile.h>
#include <rainman/lua/CLuaFile.h>
#include <rainman/lua/CLuaFile2.h>
#include <rainman/lua/CLuaFileCache.h>
#include <wx/treectrl.h>
#include <wx/aui/auibook.h>
#include <wx/listctrl.h>
#include <vector>
#include <memory>
#include "frmLuaInheritTree.h"

class CFilesTreeItemData : public wxTreeItemData
{
  public:
    //! Construct from a positioned iterator (file or directory item)
    CFilesTreeItemData(IDirectoryTraverser::IIterator *pItr);

    //! Construct a lazy directory node with a deferred sub-iterator
    CFilesTreeItemData(IDirectoryTraverser::IIterator *pItr, IDirectoryTraverser::IIterator *pSubDirItr);
    explicit CFilesTreeItemData(const wxString &sPath);

    ~CFilesTreeItemData();

    const char *sMod;
    const char *sSource;
    wxString sPhysicalPath;

    //! Sub-directory iterator for lazy child population; owned by this object
    std::unique_ptr<IDirectoryTraverser::IIterator> pToFillWith;
};

class frmFiles : public wxWindow
{
  protected:
    wxTreeCtrl *m_pTree;
    wxAuiNotebook *m_pTabs;
    frmLuaInheritTree *m_pLuaTree;
    wxListCtrl *m_pTools;
    bool _FillPartialFromIDirectoryTraverserIIterator(wxTreeItemId oParent, IDirectoryTraverser::IIterator *pChildren,
                                                      int iExpandLevel, bool bOnlyOne = false,
                                                      wxTreeItemId *pAfter = 0);
    void _FillOneLevelFromIterator(wxTreeItemId oParent, IDirectoryTraverser::IIterator *pChildren);

    wxColour m_cThisMod, m_cOtherMod, m_cEngine;
    wxString m_sPopupFileName;
    wxTreeItemId m_oPopupTreeParent, m_oPopupTreeItem;
    bool m_bPopupIsFolder;

  public:
    frmFiles(wxWindow *parent, wxWindowID id, const wxPoint &pos = wxDefaultPosition,
             const wxSize &size = wxDefaultSize);
    ~frmFiles();

    static bool _IsEngineFileMapName(const char *sName, CModuleFile *pMod);

    void OnSize(wxSizeEvent &event);
    void OnNodeTooltip(wxTreeEvent &event);
    void OnNodeActivate(wxTreeEvent &event);
    void OnNodeSelected(wxTreeEvent &event);
    void OnNodeRightClick(wxTreeEvent &event);
    void OnTreeExpanding(wxTreeEvent &event);
    void OnPageChange(wxAuiNotebookEvent &event);
    void OnMenu(wxCommandEvent &event);
    void LaunchTool(wxListEvent &event);
    bool FillFromIDirectoryTraverser(IDirectoryTraverser *pTraverser);
    bool UpdateDirectoryChildren(const wxTreeItemId &oDirectory, IDirectoryTraverser::IIterator *pChildren);
    wxTreeCtrl *GetTree();
    wxTreeItemId FindFile(wxString sFile, bool bParent);

    //! Populate lazy-loaded children of a directory node
    void PopulateChildren(const wxTreeItemId &oParent);

    //! Handler for an action on a file / folder
    class IHandler
    {
      public:
        //! Get the file extension this action applies to
        /*!
            Only applies to file actions, ignored for folder actions.
            If it is blank then it applies to all files, regardless
            of the file extension.
            For example "rgd" applies to xyz.rgd and "" applies to
            everything/
        */
        virtual wxString VGetExt() const = 0;

        //! Get the name of this action
        /*!
            Displayed in the action popup menu.
            Example: "View RGD file"
        */
        virtual wxString VGetAction() const = 0;

        //! Handle an action
        /*!
            \param[in] sFile The name of the file or folder being acted upon
            \param[in] oParent The tree item ID of the parent of oFile
                For folders this is the same as oFile.
            \param[in] oFile The tree item Id of the file or folder being acted upon
        */
        virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile) = 0;
    };

    void AddHandler(IHandler *pHandler);
    void AddFolderHandler(IHandler *pHandler);

    enum
    {
        IDC_FilesTree = wxID_HIGHEST + 1,
        IDC_ToolsList,
    };

  protected:
    std::vector<IHandler *> m_vHandlers, m_vFolderHandlers;
    DECLARE_EVENT_TABLE()
};

class CLuaAction : public frmFiles::IHandler
{
  public:
    virtual wxString VGetExt() const;
    virtual wxString VGetAction() const;
    static CLuaFile *DoLoad(IFileStore::IStream *pStream, const char *sFile, CLuaFileCache *pCache = 0);
    static CLuaFile2 *DoLoad2(IFileStore::IStream *pStream, const char *sFile, CLuaFileCache *pCache = 0);
    virtual void VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile);
};
