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

#include "frmFiles.h"

#include "frmImage.h"
#include "frmRgdEditor.h"
#include "frmScarEditor.h"
#include "frmRgmMaterialEditor.h"
#include "frmMassExtract.h"
#include "frmRgdMacro.h"
#include "frmMessage.h"
#include "frame/Construct.h"
#include "common/strconv.h"
#include "common/strings.h"
#include "common/config.h"
#include "common/Utility.h"
#include "common/ThemeColours.h"
#include "presenters/CFileTreePresenter.h"
#include "res/Icons.h"
#include <wx/artprov.h>
#include <wx/wupdlock.h>
#include <zlib.h>
extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
}

#include <wx/progdlg.h>
#include <wx/clipbrd.h>
#include <memory>
#include <rainman/io/IFileStore.h>
#include <rainman/lua/CLuaFromRgd.h>
#include <rainman/formats/CBfxFile.h>
#include <rainman/formats/CChunkyFile.h>
#include "common/Common.h"
#include <rainman/core/RainmanLog.h>

BEGIN_EVENT_TABLE(frmFiles, wxWindow)
EVT_SIZE(frmFiles::OnSize)
EVT_TREE_ITEM_GETTOOLTIP(IDC_FilesTree, frmFiles::OnNodeTooltip)
EVT_LIST_ITEM_ACTIVATED(IDC_ToolsList, frmFiles::LaunchTool)
EVT_TREE_ITEM_ACTIVATED(IDC_FilesTree, frmFiles::OnNodeActivate)
EVT_TREE_SEL_CHANGED(IDC_FilesTree, frmFiles::OnNodeSelected)
EVT_TREE_ITEM_EXPANDING(IDC_FilesTree, frmFiles::OnTreeExpanding)
EVT_TREE_ITEM_MENU(IDC_FilesTree, frmFiles::OnNodeRightClick)
EVT_MENU(wxID_ANY, frmFiles::OnMenu)
EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, frmFiles::OnPageChange)
END_EVENT_TABLE()

extern "C"
{
    typedef unsigned long int ub4; /* unsigned 4-byte quantities */
    typedef unsigned char ub1;
    ub4 hash(ub1 *k, ub4 length, ub4 initval);
    ub4 hash3(ub1 *k, ub4 length, ub4 initval);
}

CFilesTreeItemData::CFilesTreeItemData(IDirectoryTraverser::IIterator *pItr)
{
    sMod = nullptr;
    sSource = nullptr;
    if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
    {
        try
        {
            sMod = (const char *)pItr->VGetTag(0);
        }
        IGNORE_EXCEPTIONS
        try
        {
            sSource = (const char *)pItr->VGetTag(1);
        }
        IGNORE_EXCEPTIONS
    }
}

CFilesTreeItemData::CFilesTreeItemData(IDirectoryTraverser::IIterator *pItr, IDirectoryTraverser::IIterator *pSubDirItr)
{
    sMod = nullptr;
    sSource = nullptr;
    pToFillWith.reset(pSubDirItr);
    if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
    {
        try
        {
            sMod = (const char *)pItr->VGetTag(0);
        }
        IGNORE_EXCEPTIONS
        try
        {
            sSource = (const char *)pItr->VGetTag(1);
        }
        IGNORE_EXCEPTIONS
    }
}

CFilesTreeItemData::~CFilesTreeItemData() = default;

// Handlers — CLuaAction method implementations (declared in frmFiles.h)

#include "actions/ActionUtil.h"

wxString CLuaAction::VGetExt() const { return wxT("lua"); }

wxString CLuaAction::VGetAction() const { return wxT("View as LUA file"); }

CLuaFile *CLuaAction::DoLoad(IFileStore::IStream *pStream, const char *sFile, CLuaFileCache *pCache)
{
    CLuaFile *pLua = new CLuaFile;
    if (!pLua)
    {
        ErrorBoxAS(err_memory);
        return 0;
    }

    pLua->AssignCache(pCache);
    try
    {
        pLua->Load(pStream, TheConstruct->GetModuleService().GetModule(), sFile);
    }
    catch (const CRainmanException &e)
    {
        ErrorBoxE(CModStudioException(e, __FILE__, __LINE__, "Cannot load Lua file \'%s\'", sFile));
        delete pLua;
        return 0;
    }

    return pLua;
}

CLuaFile2 *CLuaAction::DoLoad2(IFileStore::IStream *pStream, const char *sFile, CLuaFileCache *pCache)
{
    CLuaFile2 *pLua = new CLuaFile2;
    if (!pLua)
    {
        ErrorBoxAS(err_memory);
        return 0;
    }

    pLua->setCache(pCache, false);
    if (sFile[0] == 'G' || sFile[0] == 'g')
    {
        pLua->setRootFolder("generic\\attrib\\");
    }
    else if (sFile[0] == 'A' || sFile[0] == 'a')
    {
        pLua->setRootFolder("attrib\\attrib\\");
    }
    else
    {
        pLua->setRootFolder("data\\attrib\\");
    }

    try
    {
        pLua->loadFile(pStream, TheConstruct->GetModuleService().GetModule(), sFile);
    }
    catch (const CRainmanException &e)
    {
        ErrorBoxE(e);
        delete pLua;
        return 0;
    }

    return pLua;
}

void CLuaAction::VHandle(wxString sFile, wxTreeItemId &oParent, wxTreeItemId &oFile)
{
    UNUSED(oFile);
    auto streamResult = TheConstruct->GetFileService().OpenStream(sFile);
    if (!streamResult)
    {
        ErrorBox("Cannot open file");
        return;
    }
    std::unique_ptr<IFileStore::IStream> pStream(streamResult.value().release());
    auto saFile = wxStringToAscii(sFile);
    CLuaFile2 *pLua = DoLoad2(pStream.get(), saFile.get());
    if (pLua)
    {
        frmRGDEditor *pForm;
        TheConstruct->GetTabs()->AddPage(
            pForm = new frmRGDEditor(oParent, sFile, TheConstruct->GetTabs(), -1),
            wxString().Append(wxT("LUA")).Append(wxT(" [")).Append(OnlyFilename(sFile)).Append(wxT("]")), true);
        if (!pForm->FillFromLua2(pLua))
        {
            ErrorBox("Cannot load file");
            delete pLua;
            return;
        }
    }
}

// Action class includes
#include "actions/CTextViewAction.h"
#include "actions/CFilePathCopyAction.h"
#include "actions/CExtractAction.h"
#include "actions/CMakeCopyAction.h"
#include "actions/CSmfWavAction.h"
#include "actions/CRGODeBurnAction.h"
#include "actions/CRgdToLuaDumpAction.h"
#include "actions/CBfxToLuaDumpAction.h"
#include "actions/CBFXRGTDeBurnAction.h"
#include "actions/CRgtViewAction.h"
#include "actions/CDdsViewAction.h"
#include "actions/CTgaViewAction.h"
#include "actions/CRgtToGenericAction.h"
#include "actions/CDdsToRgtAction.h"
#include "actions/CExtractFolderAction.h"
#include "actions/CRgdMacroAction.h"
#include "actions/CRGDAction.h"
#include "actions/CRGMMaterialAction.h"
#include "actions/CMuaxAction.h"
#include "actions/CBfxAction.h"
#include "actions/CAbpAction.h"
#include "actions/CMuaAction.h"
#include "actions/CSuaAction.h"
#include "actions/CLuaBurnAction.h"
#include "actions/CLuaBurnFolderAction.h"
#include "actions/CLuaBurnFolderIncReqAction.h"
#include "actions/CLuaDumpFolder.h"
#include "actions/CScanHashesAction.h"
#include "actions/CNilAction.h"
#include "actions/CScarAction.h"
#include "actions/CNisAction.h"
#include "actions/CAiAction.h"

// Proper stuff

wxTreeItemId frmFiles::FindFile(wxString sFile, bool bParent)
{
    wxTreeItemId oParent = m_pTree->GetRootItem();

    wxTreeItemIdValue oCookie;
    wxTreeItemId oChild = m_pTree->GetFirstChild(oParent, oCookie);

    wxString sPart = sFile.BeforeFirst('\\');
    bool bMoveNext = true;
    while (oChild.IsOk())
    {
        if (m_pTree->GetItemText(oChild).IsSameAs(sPart, false))
        {
            sFile = sFile.AfterFirst('\\');
            sPart = sFile.BeforeFirst('\\');

            if (!m_pTree->ItemHasChildren(oChild) || sPart.IsEmpty())
            {
                if (sPart.IsEmpty())
                {
                    m_pTree->SelectItem(oChild);
                }
                return bParent ? oParent : oChild;
            }
            else
            {
                oParent = oChild;
                if (!m_pTree->IsExpanded(oParent))
                {
                    m_pTree->Expand(oParent);
                }
                oChild = m_pTree->GetFirstChild(oParent, oCookie);
                bMoveNext = false;
            }
        }
        if (bMoveNext)
        {
            oChild = m_pTree->GetNextChild(oParent, oCookie);
        }
        else
        {
            bMoveNext = true;
        }
    }

    return oParent;
}

wxTreeCtrl *frmFiles::GetTree() { return m_pTree; }

bool frmFiles::_IsEngineFileMapName(const char *sName, CModuleFile *pMod)
{
    size_t iC = pMod->GetEngineCount();
    for (size_t i = 0; i < iC; ++i)
    {
        if (strcmp(pMod->GetEngine(i)->GetFileMapName(), sName) == 0)
        {
            return true;
        }
    }
    return false;
}

void frmFiles::OnPageChange(wxAuiNotebookEvent &event)
{
    int iNew = event.GetSelection();

    if (iNew == 2) // LUA
    {
        m_pLuaTree->OnActivated();
    }
}

bool frmFiles::UpdateDirectoryChildren(const wxTreeItemId &oDirectory, IDirectoryTraverser::IIterator *pChildren)
{
    // If the directory node hasn't been lazily populated yet, populate it now
    PopulateChildren(oDirectory);

    std::unique_ptr<char[]> sTreeItem;
    const char *sDirItem;
    wxTreeItemIdValue oTreeCookie;
    bool bGotOld = false;

    m_pTree->Freeze();
    wxTreeItemId oTreeItem = m_pTree->GetFirstChild(oDirectory, oTreeCookie), oTheOldItem;
    do
    {
        sTreeItem = oTreeItem.IsOk() ? wxStringToAscii(m_pTree->GetItemText(oTreeItem)) : nullptr;
        sDirItem = pChildren->VGetType() == IDirectoryTraverser::IIterator::T_Nothing ? 0 : pChildren->VGetName();

        if (sTreeItem && sDirItem)
        {
            // Items on both
            int i = stricmp(sTreeItem.get(), sDirItem);
            if (i < 0)
            {
                // Tree item before dir (delete item)
                wxTreeItemId oOld = oTreeItem;
                oTheOldItem = oTreeItem;
                bGotOld = true;
                oTreeItem = m_pTree->GetNextChild(oOld, oTreeCookie);
                m_pTree->Delete(oOld);
            }
            else if (i == 0)
            {
                CFilesTreeItemData *pData = (CFilesTreeItemData *)m_pTree->GetItemData(oTreeItem);
                if (pData)
                {
                    if (pChildren->VGetType() == IDirectoryTraverser::IIterator::T_File)
                    {
                        pData->sMod = (const char *)pChildren->VGetTag(0);
                        pData->sSource = (const char *)pChildren->VGetTag(1);
                    }
                }

                if (pChildren->VGetType() == IDirectoryTraverser::IIterator::T_File)
                {
                    if (AsciiTowxString((const char *)pChildren->VGetTag(0)) ==
                        TheConstruct->GetModuleService().GetFileMapName())
                    {
                        // This mod
                        m_pTree->SetItemTextColour(oTreeItem, m_cThisMod);
                    }
                    else if (_IsEngineFileMapName((const char *)pChildren->VGetTag(0),
                                                  TheConstruct->GetModuleService().GetModule()))
                    {
                        // Engine
                        m_pTree->SetItemTextColour(oTreeItem, m_cEngine);
                    }
                    else
                    {
                        // other mod
                        m_pTree->SetItemTextColour(oTreeItem, m_cOtherMod);
                    }
                }
                oTheOldItem = oTreeItem;
                bGotOld = true;
                oTreeItem = m_pTree->GetNextChild(oTreeItem, oTreeCookie);
                pChildren->VNextItem();
            }
            else
            {
                // Tree item after dir (new item)
                bool bRet = _FillPartialFromIDirectoryTraverserIIterator(oDirectory, pChildren, 0, true,
                                                                         bGotOld ? &oTheOldItem : 0);
                if (!bRet)
                {
                    m_pTree->Thaw();
                    m_pTree->Update();
                    return false;
                }
                pChildren->VNextItem();
            }
        }
        else if (sTreeItem)
        {
            // Item in tree only
            wxTreeItemId oOld = oTreeItem;
            oTheOldItem = oTreeItem;
            bGotOld = true;
            oTreeItem = m_pTree->GetNextChild(oOld, oTreeCookie);
            m_pTree->Delete(oOld);
        }
        else if (sDirItem)
        {
            // Item in dir only
            bool bRet = _FillPartialFromIDirectoryTraverserIIterator(oDirectory, pChildren, 0);
            m_pTree->Thaw();
            m_pTree->Update();
            return bRet;
        }

    } while (sTreeItem && sDirItem);

    m_pTree->Thaw();
    m_pTree->Update();
    return true;
}

void frmFiles::AddHandler(IHandler *pHandler) { m_vHandlers.push_back(pHandler); }

void frmFiles::AddFolderHandler(IHandler *pHandler) { m_vFolderHandlers.push_back(pHandler); }

frmFiles::~frmFiles()
{
    for (std::vector<IHandler *>::iterator itr = m_vHandlers.begin(); itr != m_vHandlers.end(); ++itr)
    {
        delete *itr;
    }
    for (std::vector<IHandler *>::iterator itr = m_vFolderHandlers.begin(); itr != m_vFolderHandlers.end(); ++itr)
    {
        delete *itr;
    }
}

void frmFiles::LaunchTool(wxListEvent &event) { TheConstruct->DoTool(event.GetText()); }

void frmFiles::OnNodeTooltip(wxTreeEvent &event)
{
    CFilesTreeItemData *pData = (CFilesTreeItemData *)m_pTree->GetItemData(event.GetItem());
    if (pData)
    {
        if (pData->sMod && pData->sSource)
        {
            wxString sTooltip;
            wxTreeItemId oID = event.GetItem();
            sTooltip = wxT(""); // m_pTree->GetItemText(oID);
            // sTooltip.Append(wxT(" | "));
            sTooltip.Append(AppStr(file_modname)).Append(AsciiTowxString(pData->sMod)).Append(wxT(" | "));
            sTooltip.Append(AppStr(file_sourcename)).Append(AsciiTowxString(pData->sSource));

            // SetToolTip( 0 );
            event.SetToolTip(sTooltip);
            TheConstruct->GetStatusBar()->SetStatusText(sTooltip);
        }
        else
        {
            event.SetToolTip(wxT(""));
            TheConstruct->GetStatusBar()->SetStatusText(wxT(""));
        }
    }
}

void frmFiles::OnNodeActivate(wxTreeEvent &event)
{
    CFilesTreeItemData *pData = (CFilesTreeItemData *)m_pTree->GetItemData(event.GetItem());
    if (pData && pData->sMod && pData->sSource)
    {
        wxString sPath, sExt;
        wxTreeItemId oCurrent, oRoot, oParent;
        oCurrent = event.GetItem();
        oParent = m_pTree->GetItemParent(oCurrent);
        oRoot = m_pTree->GetRootItem();
        while (oCurrent != oRoot)
        {
            sPath.Prepend(m_pTree->GetItemText(oCurrent));
            sPath.Prepend(wxT("\\"));
            oCurrent = m_pTree->GetItemParent(oCurrent);
        }

        sPath.Remove(0, 1);

        // If the preview tab already shows this file, just pin it
        auto &tabMgr = TheConstruct->GetTabManager();
        if (tabMgr.HasPreviewTab() && tabMgr.GetPreviewFilePath() == sPath)
        {
            tabMgr.PinPreviewTab();
            return;
        }

        sExt = sPath.AfterLast('.');
        sExt.LowerCase();
        for (std::vector<IHandler *>::iterator itr = m_vHandlers.begin(); itr != m_vHandlers.end(); ++itr)
        {
            if ((*itr)->VGetExt() == sExt)
            {
                try
                {
                    wxWindowUpdateLocker lock(TheConstruct->GetTabs());
                    (*itr)->VHandle(sPath, oParent, oCurrent);
                }
                catch (const CRainmanException &e)
                {
                    _ErrorBox(e, __FILE__, __LINE__, false);
                }
                return;
            }
        }
        // If there is no specilized handler, show a list of generic ones
        OnNodeRightClick(event);
    }
}

void frmFiles::OnNodeSelected(wxTreeEvent &event)
{
    wxTreeItemId oItem = event.GetItem();
    auto *pData = static_cast<CFilesTreeItemData *>(m_pTree->GetItemData(oItem));
    if (!pData || !pData->sMod || !pData->sSource)
    {
        // Toggle expand/collapse for folder items on click
        if (m_pTree->ItemHasChildren(oItem))
        {
            m_pTree->Toggle(oItem);
        }
        return;
    }

    // Skip preview if file preview is disabled in View menu
    bool bPreviewEnabled = true;
    TheConfig->Read(AppStr(config_preview_enabled), &bPreviewEnabled, true);
    if (!bPreviewEnabled)
    {
        return;
    }

    wxString sPath;
    wxTreeItemId oCurrent = event.GetItem();
    wxTreeItemId oParent = m_pTree->GetItemParent(oCurrent);
    wxTreeItemId oRoot = m_pTree->GetRootItem();
    while (oCurrent != oRoot)
    {
        sPath.Prepend(m_pTree->GetItemText(oCurrent));
        sPath.Prepend(wxT("\\"));
        oCurrent = m_pTree->GetItemParent(oCurrent);
    }
    sPath.Remove(0, 1);

    auto &tabMgr = TheConstruct->GetTabManager();

    // Skip if this file is already the preview tab
    if (tabMgr.HasPreviewTab() && tabMgr.GetPreviewFilePath() == sPath)
    {
        return;
    }

    wxString sExt = sPath.AfterLast('.');
    sExt.LowerCase();
    for (auto *pHandler : m_vHandlers)
    {
        if (pHandler->VGetExt() == sExt)
        {
            try
            {
                wxWindowUpdateLocker lock(tabMgr.GetTabs());
                tabMgr.ClosePreviewTab();
                size_t countBefore = tabMgr.GetTabs()->GetPageCount();
                oCurrent = event.GetItem();
                pHandler->VHandle(sPath, oParent, oCurrent);

                // If the handler added a new page, mark it as the preview tab
                if (tabMgr.GetTabs()->GetPageCount() > countBefore)
                {
                    size_t lastIdx = tabMgr.GetTabs()->GetPageCount() - 1;
                    tabMgr.SetPreviewPage(tabMgr.GetTabs()->GetPage(lastIdx), sPath);
                }
            }
            catch (const CRainmanException &e)
            {
                _ErrorBox(e, __FILE__, __LINE__, false);
            }
            return;
        }
    }
}

void frmFiles::OnNodeRightClick(wxTreeEvent &event)
{
    int flags;
    wxTreeItemId oItemID = m_pTree->HitTest(event.GetPoint(), flags);
    if (oItemID.IsOk())
    {
        m_pTree->SelectItem(oItemID);
    }
    else
    {
        return;
    }

    CFilesTreeItemData *pData = (CFilesTreeItemData *)m_pTree->GetItemData(oItemID);

    wxString sPath, sExt;
    wxTreeItemId oCurrent, oRoot, oParent;
    oCurrent = oItemID;
    m_oPopupTreeItem = oCurrent;
    oParent = m_pTree->GetItemParent(oCurrent);
    oRoot = m_pTree->GetRootItem();
    while (oCurrent != oRoot)
    {
        sPath.Prepend(m_pTree->GetItemText(oCurrent));
        sPath.Prepend(wxT("\\"));
        oCurrent = m_pTree->GetItemParent(oCurrent);
    }

    wxMenu *pPopup = new wxMenu;

    sPath.Remove(0, 1);
    if (pData && pData->sSource)
    {
        sExt = sPath.AfterLast('.');
        sExt.LowerCase();
        int i = 0;
        for (std::vector<IHandler *>::iterator itr = m_vHandlers.begin(); itr != m_vHandlers.end(); ++itr)
        {
            if (((*itr)->VGetExt() == sExt) || ((*itr)->VGetExt() == wxT("")))
            {
                pPopup->Append(wxID_HIGHEST + 1337 + i, (*itr)->VGetAction());
            }
            ++i;
        }

        m_sPopupFileName = sPath;
        m_oPopupTreeParent = oParent;
        m_bPopupIsFolder = false;
    }
    else
    {
        int i = 0;
        for (std::vector<IHandler *>::iterator itr = m_vFolderHandlers.begin(); itr != m_vFolderHandlers.end(); ++itr)
        {
            pPopup->Append(wxID_HIGHEST + 1337 + i, (*itr)->VGetAction());
            ++i;
        }

        m_sPopupFileName = sPath;
        m_oPopupTreeParent = m_oPopupTreeItem;
        m_bPopupIsFolder = true;
    }
    PopupMenu(pPopup);
    delete pPopup;
}

void frmFiles::OnMenu(wxCommandEvent &event)
{
    if (event.GetId() >= (wxID_HIGHEST + 1337))
    {
        try
        {
            wxWindowUpdateLocker lock(TheConstruct->GetTabs());
            if (m_bPopupIsFolder)
            {
                m_vFolderHandlers[event.GetId() - (wxID_HIGHEST + 1337)]->VHandle(m_sPopupFileName, m_oPopupTreeParent,
                                                                                  m_oPopupTreeItem);
            }
            else
            {
                m_vHandlers[event.GetId() - (wxID_HIGHEST + 1337)]->VHandle(m_sPopupFileName, m_oPopupTreeParent,
                                                                            m_oPopupTreeItem);
            }
        }
        catch (const CRainmanException &e)
        {
            ErrorBoxE(e);
        }
    }
}

frmFiles::frmFiles(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : wxWindow(parent, id, pos, size)
{
    wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);
    m_pTabs =
        new wxAuiNotebook(this, -1, wxPoint(0, 0), wxDefaultSize,
                          (wxAUI_NB_DEFAULT_STYLE | wxNO_BORDER) &
                              (~(wxAUI_NB_CLOSE_BUTTON | wxAUI_NB_CLOSE_ON_ACTIVE_TAB | wxAUI_NB_CLOSE_ON_ALL_TABS |
                                 wxAUI_NB_TAB_EXTERNAL_MOVE | wxAUI_NB_TAB_SPLIT | wxAUI_NB_TAB_MOVE)));

    m_pTabs->AddPage(m_pTree = new wxTreeCtrl(m_pTabs, IDC_FilesTree, wxDefaultPosition, wxDefaultSize,
                                              wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT),
                     AppStr(file_filestabname));

    // Toggle expand/collapse when clicking an already-selected folder.
    // SEL_CHANGED doesn't fire for re-clicks on the same item, so we
    // catch LEFT_DOWN and handle that case here.
    m_pTree->Bind(wxEVT_LEFT_DOWN,
                  [this](wxMouseEvent &evt)
                  {
                      int flags = 0;
                      wxTreeItemId oHit = m_pTree->HitTest(evt.GetPosition(), flags);
                      if (oHit.IsOk() && (flags & wxTREE_HITTEST_ONITEMLABEL) && oHit == m_pTree->GetSelection() &&
                          m_pTree->ItemHasChildren(oHit))
                      {
                          m_pTree->Toggle(oHit);
                          return;
                      }
                      evt.Skip();
                  });
    m_pTabs->AddPage(m_pTools = new wxListCtrl(m_pTabs, IDC_ToolsList, wxDefaultPosition, wxDefaultSize,
                                               wxLC_ICON | wxLC_SINGLE_SEL),
                     AppStr(file_toolstabname));

    m_pTabs->AddPage(m_pLuaTree = new frmLuaInheritTree(m_pTabs, wxID_ANY), wxT("Lua"));

    m_pTools->SetAutoLayout(true);
    pTopSizer->Add(m_pTabs, 1, wxEXPAND);

    wxImageList *pToolImages = new wxImageList(48, 48);
    size_t iToolCount = TheConstruct->GetToolCount();
    for (size_t i = 0; i < iToolCount; ++i)
    {
        pToolImages->Add(TheConstruct->GetTool(i)->GetBitmapBundle(wxSize(48, 48)).GetBitmap(wxSize(48, 48)));
    }
    /*
    pToolImages->Add(wxBitmap(wxT("IDB_TOOL_LOC"), wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255, 0, 255)); // ( 0)
    Locilization pToolImages->Add(wxBitmap(wxT("IDB_TOOL_UCS"), wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255, 0, 255)); //
    ( 1) UCS Editor pToolImages->Add(wxBitmap(wxT("IDB_SNAPSHOT"), wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255, 0, 255));
    // ( 2) Attrib Snapshot pToolImages->Add(wxBitmap(wxT("IDB_SGAPACK"), wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255, 0,
    255)); // ( 3) SGA Pack pToolImages->Add(wxBitmap(wxT("IDB_TOOL_EXTALL"), wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255,
    0, 255)); // ( 4) Extract all pToolImages->Add(wxBitmap(wxT("IDB_TOOL_CALCULATOR"), wxBITMAP_TYPE_BMP_RESOURCE),
    wxColour(255, 0, 255)); // ( 5) DPS Calculator pToolImages->Add(wxBitmap(wxT("IDB_REDBUTTON"),
    wxBITMAP_TYPE_BMP_RESOURCE), wxColour(255, 0, 255)); // ( 6) Red button :D
    */

    m_pTools->AssignImageList(pToolImages, wxIMAGE_LIST_NORMAL);
    m_pTools->Arrange(wxLIST_ALIGN_SNAP_TO_GRID);

    for (size_t i = 0; i < iToolCount; ++i)
    {
        m_pTools->InsertItem(i, TheConstruct->GetTool(i)->GetName(), i);
    }

    /*
    m_pTools->InsertItem(0, AppStr(locale), 0);
    m_pTools->InsertItem(1, AppStr(ucs_editor), 1);
    m_pTools->InsertItem(2, AppStr(xml_export), 2);
    m_pTools->InsertItem(3, AppStr(sgapack_title), 3);
    m_pTools->InsertItem(4, AppStr(massext_toolname), 4);
    m_pTools->InsertItem(5, AppStr(dpscalculator_title), 5);
    m_pTools->InsertItem(6, AppStr(redbutton_toolname), 6);
    */

    wxImageList *pFileTypes = new wxImageList(16, 16);
    const wxSize kIconSize(16, 16);
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileAi, kIconSize));           // ( 0) AI
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileLua, kIconSize));          // ( 1) LUA
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileNil, kIconSize));          // ( 2) NIL
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileRgd, kIconSize));          // ( 3) RGD
    pFileTypes->Add(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_OTHER, kIconSize));      // ( 4) Closed
    pFileTypes->Add(wxArtProvider::GetBitmap(wxART_FOLDER_OPEN, wxART_OTHER, kIconSize)); // ( 5) Open
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileScar, kIconSize));         // ( 6) SCAR
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kHardDrive, kIconSize));        // ( 7) ToC
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileTga, kIconSize));          // ( 8) TGA
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileRgt, kIconSize));          // ( 9) RGT
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileDds, kIconSize));          // (10) DDS
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileBfx, kIconSize));          // (11) BFX
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileAbp, kIconSize));          // (12) ABP
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileRgm, kIconSize));          // (13) RGM
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileNil, kIconSize));          // (14) Unknown
    pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileNis, kIconSize));          // (15) NIS

    m_pTree->AssignImageList(pFileTypes);

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);

    // Specific actions
    AddHandler(new CRGMMaterialAction);
    AddHandler(new CNilAction);
    AddHandler(new CLuaAction);
    AddHandler(new CLuaBurnAction);
    AddHandler(new CRGDAction);
    AddHandler(new CRgdToLuaDumpAction);
    AddHandler(new CScarAction);
    AddHandler(new CNisAction);
    AddHandler(new CAiAction);
    AddHandler(new CMuaxAction);
    AddHandler(new CSuaAction);
    AddHandler(new CMuaAction);
    AddHandler(new CAbpAction);
    AddHandler(new CBfxAction);
    AddHandler(new CBfxToLuaDumpAction);
    AddHandler(new CRgtViewAction);
    AddHandler(new CRgtToGenericAction);
    AddHandler(new CDdsViewAction);
    AddHandler(new CDdsToRgtAction);
    AddHandler(new CTgaViewAction);
    AddHandler(new CRGODeBurnAction);
    // Generic actions
    AddHandler(new CMakeCopyAction);
    AddHandler(new CExtractAction);
    AddHandler(new CFilePathCopyAction);
    AddHandler(new CTextViewAction);

    // Folder actions
    AddFolderHandler(new CExtractFolderAction);
    AddFolderHandler(new CLuaBurnFolderAction);
    AddFolderHandler(new CLuaBurnFolderIncReqAction);
    AddFolderHandler(new CRgdMacroAction);
    AddFolderHandler(new CLuaDumpFolder);
    AddFolderHandler(new CBFXRGTDeBurnAction);
    AddFolderHandler(new CScanHashesAction);
}

void frmFiles::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
    m_pTools->DeleteAllItems();
    size_t iToolCount = TheConstruct->GetToolCount();
    for (size_t i = 0; i < iToolCount; ++i)
    {
        m_pTools->InsertItem(i, TheConstruct->GetTool(i)->GetName(), i);
    }
}

bool frmFiles::FillFromIDirectoryTraverser(IDirectoryTraverser *pTraverser)
{
    m_cThisMod = ThemeColours::FileThisMod();
    m_cOtherMod = ThemeColours::FileOtherMod();
    m_cEngine = ThemeColours::FileEngine();

    m_pTree->Freeze();
    m_pTree->DeleteAllItems();
    wxTreeItemId oRoot = m_pTree->AddRoot(wxT(""));

    unsigned long iEntryCount = pTraverser->VGetEntryPointCount();
    for (unsigned long iEntryPoint = 0; iEntryPoint < iEntryCount; ++iEntryPoint)
    {
        const char *sEntryPoint = pTraverser->VGetEntryPoint(iEntryPoint);
        wxTreeItemId oEntry = m_pTree->AppendItem(oRoot, AsciiTowxString(sEntryPoint), 7);

        IDirectoryTraverser::IIterator *pEntry = pTraverser->VIterate(sEntryPoint);
        if (pEntry)
        {
            // Populate the first level of children under each entry point
            _FillOneLevelFromIterator(oEntry, pEntry);
            delete pEntry;
        }
    }
    m_pTree->Thaw();
    return true;
}

void frmFiles::_FillOneLevelFromIterator(wxTreeItemId oParent, IDirectoryTraverser::IIterator *pChildren)
{
    if (!pChildren || pChildren->VGetType() == IDirectoryTraverser::IIterator::T_Nothing)
    {
        return;
    }

    do
    {
        switch (pChildren->VGetType())
        {
        case IDirectoryTraverser::IIterator::T_File:
        {
            wxString sName = AsciiTowxString(pChildren->VGetName());
            int iImg = CFileTreePresenter::ClassifyFileIcon(sName);
            wxTreeItemId oChild = m_pTree->AppendItem(oParent, sName, iImg, iImg, new CFilesTreeItemData(pChildren));
            if (AsciiTowxString((const char *)pChildren->VGetTag(0)) ==
                TheConstruct->GetModuleService().GetFileMapName())
            {
                m_pTree->SetItemTextColour(oChild, m_cThisMod);
            }
            else if (_IsEngineFileMapName((const char *)pChildren->VGetTag(0),
                                          TheConstruct->GetModuleService().GetModule()))
            {
                m_pTree->SetItemTextColour(oChild, m_cEngine);
            }
            else
            {
                m_pTree->SetItemTextColour(oChild, m_cOtherMod);
            }
            break;
        }
        case IDirectoryTraverser::IIterator::T_Directory:
        {
            // Store the sub-iterator for lazy population on expand
            IDirectoryTraverser::IIterator *pSubDir = pChildren->VOpenSubDir();
            bool bHasChildren = pSubDir && pSubDir->VGetType() != IDirectoryTraverser::IIterator::T_Nothing;

            auto *pData = new CFilesTreeItemData(pChildren, bHasChildren ? pSubDir : nullptr);
            wxTreeItemId oChild = m_pTree->AppendItem(oParent, AsciiTowxString(pChildren->VGetName()), 4, 4, pData);
            m_pTree->SetItemImage(oChild, 5, wxTreeItemIcon_Expanded);

            if (bHasChildren)
            {
                m_pTree->SetItemHasChildren(oChild, true);
            }
            else
            {
                delete pSubDir;
            }
            break;
        }
        default:
            break;
        }
    } while (pChildren->VNextItem() == IDirectoryTraverser::IIterator::E_OK);
}

void frmFiles::PopulateChildren(const wxTreeItemId &oParent)
{
    auto *pData = static_cast<CFilesTreeItemData *>(m_pTree->GetItemData(oParent));
    if (!pData || !pData->pToFillWith)
    {
        return;
    }

    m_pTree->Freeze();
    _FillOneLevelFromIterator(oParent, pData->pToFillWith.get());

    // Iterator consumed — release it
    pData->pToFillWith.reset();
    m_pTree->Thaw();
}

void frmFiles::OnTreeExpanding(wxTreeEvent &event) { PopulateChildren(event.GetItem()); }

bool frmFiles::_FillPartialFromIDirectoryTraverserIIterator(wxTreeItemId oParent,
                                                            IDirectoryTraverser::IIterator *pChildren, int iExpandLevel,
                                                            bool bOnlyOne, wxTreeItemId *pAfter)
{
    if (!pChildren)
    {
        return false;
    }

    if (pChildren->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
    {
        IDirectoryTraverser::IIterator::eErrors eNextError;
        do
        {
            switch (pChildren->VGetType())
            {
            case IDirectoryTraverser::IIterator::T_File:
            {
                wxString sName = AsciiTowxString(pChildren->VGetName());
                int iImg = CFileTreePresenter::ClassifyFileIcon(sName);
                wxTreeItemId oChild;
                if (pAfter)
                {
                    oChild =
                        m_pTree->InsertItem(oParent, *pAfter, sName, iImg, iImg, new CFilesTreeItemData(pChildren));
                }
                else
                {
                    if (bOnlyOne)
                    {
                        oChild = m_pTree->InsertItem(oParent, 0, sName, iImg, iImg, new CFilesTreeItemData(pChildren));
                    }
                    else
                    {
                        oChild = m_pTree->AppendItem(oParent, sName, iImg, iImg, new CFilesTreeItemData(pChildren));
                    }
                }
                //! \todo pull file colours out of config file
                if (AsciiTowxString((const char *)pChildren->VGetTag(0)) ==
                    TheConstruct->GetModuleService().GetFileMapName())
                {
                    // This mod
                    m_pTree->SetItemTextColour(oChild, m_cThisMod);
                }
                else if (_IsEngineFileMapName((const char *)pChildren->VGetTag(0),
                                              TheConstruct->GetModuleService().GetModule()))
                {
                    // Engine
                    m_pTree->SetItemTextColour(oChild, m_cEngine);
                }
                else
                {
                    // other mod
                    m_pTree->SetItemTextColour(oChild, m_cOtherMod);
                }
            }
            break;
            case IDirectoryTraverser::IIterator::T_Directory:
            {
                // Store sub-iterator for lazy population on expand
                IDirectoryTraverser::IIterator *pSubDir = pChildren->VOpenSubDir();
                bool bHasChildren = pSubDir && pSubDir->VGetType() != IDirectoryTraverser::IIterator::T_Nothing;

                auto *pItemData = new CFilesTreeItemData(pChildren, bHasChildren ? pSubDir : nullptr);
                wxTreeItemId oChild;
                if (pAfter)
                {
                    oChild =
                        m_pTree->InsertItem(oParent, *pAfter, AsciiTowxString(pChildren->VGetName()), 4, 4, pItemData);
                }
                else
                {
                    oChild = m_pTree->AppendItem(oParent, AsciiTowxString(pChildren->VGetName()), 4, 4, pItemData);
                }
                m_pTree->SetItemImage(oChild, 5, wxTreeItemIcon_Expanded);

                if (bHasChildren)
                {
                    m_pTree->SetItemHasChildren(oChild, true);
                }
                else
                {
                    delete pSubDir;
                }
                break;
            }
            };
            if (bOnlyOne)
            {
                return true;
            }
        } while ((eNextError = pChildren->VNextItem()) == IDirectoryTraverser::IIterator::E_OK);
    }
    return true;
}
