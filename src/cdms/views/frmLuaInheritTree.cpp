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
#include "frmLuaInheritTree.h"
#include "tools/Tools.h"
#include "frmRgdEditor.h"
#include "common/Common.h"
#include "common/ThemeColours.h"
#include "res/Icons.h"
#include <rainman/core/RainmanLog.h>
#include <rainman/module/CFileMap.h>
#include <filesystem>

BEGIN_EVENT_TABLE(frmLuaInheritTree, wxWindow)
EVT_SIZE(frmLuaInheritTree::OnSize)
EVT_TREE_ITEM_GETTOOLTIP(IDC_LuaTree, frmLuaInheritTree::OnTreeTooltip)
EVT_TREE_ITEM_ACTIVATED(IDC_LuaTree, frmLuaInheritTree::OnTreeActivate)
EVT_TREE_ITEM_MENU(IDC_LuaTree, frmLuaInheritTree::OnTreeRightClick)
EVT_TREE_ITEM_EXPANDING(IDC_LuaTree, frmLuaInheritTree::OnTreeExpanding)
END_EVENT_TABLE()

class CLuaTreeItemData : public wxTreeItemData
{
  public:
    CInheritTable::CNode *pNode;
    bool bNeedsFilling;

    CLuaTreeItemData(CInheritTable::CNode *pp, bool b = false)
    {
        pNode = pp;
        bNeedsFilling = b;
    }
    ~CLuaTreeItemData() {}
};

void frmLuaInheritTree::OnActivated()
{
    if (bFirstActivate)
    {
        if (m_pInheritTable)
        {
            CDMS_LOG_INFO("Lua inherit tree: building tree from 'Generic\\Attrib\\'");
            m_pPresenter->BuildTree(m_pInheritTable, wxT("Generic\\Attrib\\"));
        }
        else
        {
            CDMS_LOG_DEBUG("Lua inherit tree: no inherit table available — skipping build");
        }

        bFirstActivate = false;
    }
}

void frmLuaInheritTree::ShowLoading(const wxString &sMessage)
{
    if (GetParent())
    {
        GetParent()->SetLabel(sMessage);
    }
}

void frmLuaInheritTree::HideLoading() {}

void frmLuaInheritTree::ShowError(const wxString &sMessage)
{
    CDMS_LOG_ERROR("Lua inherit tree: {}", sMessage.mb_str().data());
    ThemeColours::ShowMessageBox(sMessage, wxT("Error"), wxOK | wxICON_ERROR, this);
}

void frmLuaInheritTree::OnTreeDataReady()
{
    CDMS_LOG_INFO("Lua inherit tree: tree data ready, populating UI");
    m_pTree->Freeze();
    _AddChildren(m_pTree->GetRootItem(), m_pInheritTable->getRoot());
    m_pTree->Thaw();
    m_pTree->Expand(m_pTree->GetRootItem());
}

void frmLuaInheritTree::DisableControls()
{
    if (m_pTree)
    {
        m_pTree->Disable();
    }
}

void frmLuaInheritTree::EnableControls()
{
    if (m_pTree)
    {
        m_pTree->Enable();
    }
}

void frmLuaInheritTree::_AddChildren(const wxTreeItemId &oParent, CInheritTable::CNode *pParent)
{
    size_t iL = pParent->getChildCount();
    for (size_t i = 0; i < iL; ++i)
    {
        CInheritTable::CNode *pChild = pParent->getChild(i);
        int iImg = pChild->getIsNil() ? 1 : 0;
        wxTreeItemId oChild = m_pTree->AppendItem(oParent, AsciiTowxString(pChild->getMiniName()), iImg, iImg,
                                                  new CLuaTreeItemData(pChild, true));
        // m_pTree->SetItemImage(oChild, iImg, wxTreeItemIcon_Expanded);
        // m_pTree->SetItemImage(oChild, iImg, wxTreeItemIcon_SelectedExpanded);
        //_AddChildren(oChild, pChild);
        m_pTree->SetItemHasChildren(oChild, pChild->getChildCount() > 0);
    }
    if (iL > 0)
    {
        m_pTree->SortChildren(oParent);
    }
}

frmLuaInheritTree::frmLuaInheritTree(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : wxWindow(parent, id, pos, size)
{
    bFirstActivate = true;
    m_pInheritTable = 0;
    m_pPresenter = std::make_unique<CLuaInheritTreePresenter>(*this, this);

    if (CMakeLuaInheritTree::_DoesExist("Generic\\attrib\\"))
    {
        CDMS_LOG_INFO("Lua inherit tree: 'Generic\\attrib\\' folder found, creating tree widget");
        const auto sourcePaths =
            TheConstruct->GetModuleService().GetFileMap()->GetSourceFolderPaths("Generic\\attrib\\");
        if (sourcePaths.empty())
        {
            CDMS_LOG_DEBUG("Lua inherit tree: 'Generic\\attrib\\' has no filesystem-backed source directory");
        }
        for (const auto &sourcePath : sourcePaths)
        {
            std::error_code ec;
            const auto absolutePath = std::filesystem::absolute(sourcePath, ec);
            if (ec)
            {
                CDMS_LOG_WARN("Lua inherit tree: unable to resolve source path '{}' to an absolute path: {}",
                              sourcePath, ec.message());
                continue;
            }
            CDMS_LOG_DEBUG("Lua inherit tree: absolute path for 'Generic\\attrib\\' is '{}'",
                           absolutePath.lexically_normal().string());
        }
        wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);

        m_pInheritTable = new CInheritTable;
        m_pTree = new wxTreeCtrl(this, IDC_LuaTree, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS);

        wxImageList *pFileTypes = new wxImageList(16, 16);
        const wxSize kIconSize(16, 16);
        pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileLua, kIconSize));  // ( 0) LUA
        pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kFileNil, kIconSize));  // ( 1) NIL
        pFileTypes->Add(cdms::icons::SvgToBitmap(cdms::icons::kListTree, kIconSize)); // ( 2) ToC
        m_pTree->AssignImageList(pFileTypes);

        wxTreeItemId oRoot = m_pTree->AddRoot(wxT("Generic"), 2, 2);
        m_pTree->SetItemImage(oRoot, 2, wxTreeItemIcon_Expanded);
        m_pTree->SetItemImage(oRoot, 2, wxTreeItemIcon_SelectedExpanded);

        pTopSizer->Add(m_pTree, 1, wxEXPAND | wxALL, 0);

        SetSizer(pTopSizer);
        pTopSizer->SetSizeHints(this);
    }
    else
    {
        CDMS_LOG_INFO("Lua inherit tree: 'Generic\\attrib\\' folder not found in module — tab will be empty");

        auto *pSizer = new wxBoxSizer(wxVERTICAL);
        pSizer->AddStretchSpacer();

        auto *pLabel = new wxStaticText(this, wxID_ANY,
                                        wxT("No Lua inheritance data available.\n\n"
                                            "This module does not contain a 'Generic\\attrib' folder\n"
                                            "with Lua attribute files.\n\n"
                                            "Lua attributes are used by Dawn of War and Winter Assault.\n"
                                            "Later titles (Dark Crusade, Soulstorm, Company of Heroes)\n"
                                            "use compiled RGD attributes instead."),
                                        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
        pLabel->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
        pSizer->Add(pLabel, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 20);

        pSizer->AddStretchSpacer();
        SetSizer(pSizer);
        pSizer->SetSizeHints(this);
    }
}
frmLuaInheritTree::~frmLuaInheritTree()
{
    if (m_pInheritTable)
    {
        delete m_pInheritTable;
    }
}
void frmLuaInheritTree::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

static wxString OnlyFilename(wxString sName) { return sName.AfterLast('\\'); }

void frmLuaInheritTree::OnTreeActivate(wxTreeEvent &event)
{
    CLuaTreeItemData *pData = (CLuaTreeItemData *)m_pTree->GetItemData(event.GetItem());
    if (pData)
    {
        /*
        wxString sTooltip;
        sTooltip = AsciiTowxString(pData->pNode->getFullName());
        TheConstruct->GetStatusBar()->SetStatusText(sTooltip);
        */
        wxString sFile(wxT("Generic\\attrib\\"));
        sFile += AsciiTowxString(pData->pNode->getFullName());
        auto streamResult = TheConstruct->GetFileService().OpenStream(sFile);
        if (!streamResult)
        {
            ErrorBoxS(streamResult.error());
            return;
        }
        auto saFile = wxStringToAscii(sFile);
        CLuaFile2 *pLua = CLuaAction::DoLoad2(streamResult.value().get(), saFile.get());
        if (pLua)
        {
            frmRGDEditor *pForm;
            TheConstruct->GetTabs()->AddPage(
                pForm = new frmRGDEditor(TheConstruct->GetFilesList()->FindFile(sFile, true), sFile,
                                         TheConstruct->GetTabs(), -1),
                wxString().Append(wxT("LUA")).Append(wxT(" [")).Append(OnlyFilename(sFile)).Append(wxT("]")), true);
            if (!pForm->FillFromLua2(pLua))
            {
                ErrorBox("Cannot load file");
                delete pLua;
                return;
            }
        }
    }
}
void frmLuaInheritTree::OnTreeTooltip(wxTreeEvent &event)
{
    CLuaTreeItemData *pData = (CLuaTreeItemData *)m_pTree->GetItemData(event.GetItem());
    if (pData)
    {
        wxString sTooltip;
        sTooltip = AsciiTowxString(pData->pNode->getFullName());
        TheConstruct->GetStatusBar()->SetStatusText(sTooltip);
    }
}
void frmLuaInheritTree::OnTreeExpanding(wxTreeEvent &event)
{
    CLuaTreeItemData *pData = (CLuaTreeItemData *)m_pTree->GetItemData(event.GetItem());
    if (pData && pData->bNeedsFilling)
    {
        _AddChildren(event.GetItem(), pData->pNode);
        pData->bNeedsFilling = false;
    }
}
void frmLuaInheritTree::OnTreeRightClick(wxTreeEvent &event)
{
    UNUSED(event);
    // TODO
}
