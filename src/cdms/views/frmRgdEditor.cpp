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

#include "frmRgdEditor.h"
#include "frmScarEditor.h"
#include "actions/ActionUtil.h"
#include "common/TabDialog.h"
#include "tools/Tools.h"
#include "frmUCSEditor.h"
#include "frmFileSelector.h"
#include "common/strconv.h"
#include "frame/Construct.h"
#include "common/config.h"
#include "common/strings.h"
#include "common/Utility.h"
#include "presenters/CRgdEditorPresenter.h"
#include <wx/clipbrd.h>
#include <wx/artprov.h>
#include <wx/propgrid/manager.h>
#include <wx/toolbar.h>
#include <wx/tbarbase.h>
#include "common/Common.h"
#include "common/ThemeColours.h"
#include <cstdint>
#include <memory>
#include <rainman/io/IFileStore.h>
#include <rainman/core/RainmanLog.h>
#include <rainman/core/WideFile.h>

extern "C"
{
    typedef unsigned long int ub4; /* unsigned 4-byte quantities */
    typedef unsigned char ub1;
    ub4 hash(ub1 *k, ub4 length, ub4 initval);
}

std::map<unsigned long, frmRGDEditor::_NodeHelp> *frmRGDEditor::m_pNodeHelp = 0;

static wxPGProperty *GetNextPGSibling(wxPGProperty *prop)
{
    if (!prop)
    {
        return nullptr;
    }
    wxPGProperty *parent = prop->GetParent();
    if (!parent)
    {
        return nullptr;
    }
    unsigned int idx = prop->GetIndexInParent() + 1;
    return (idx < parent->GetChildCount()) ? parent->Item(idx) : nullptr;
}

BEGIN_EVENT_TABLE(frmRGDEditor, wxWindow)
EVT_SIZE(frmRGDEditor::OnSize)
EVT_PG_CHANGED(IDC_PropertyGrid, frmRGDEditor::OnPropertyChange)
EVT_TREE_SEL_CHANGING(IDC_TablesTree, frmRGDEditor::OnTreeSelect)
EVT_TREE_ITEM_MENU(IDC_TablesTree, frmRGDEditor::OnTreeRightClick)
EVT_TREE_ITEM_EXPANDING(IDC_TablesTree, frmRGDEditor::OnTreeExpanding)
EVT_MENU(IDC_AddChild, frmRGDEditor::OnAddChild)
EVT_MENU(IDC_Delete, frmRGDEditor::OnDelete)
EVT_MENU(IDC_Copy, frmRGDEditor::OnCopy)
EVT_MENU(IDC_Paste, frmRGDEditor::OnPaste)
EVT_MENU(IDC_PasteInto, frmRGDEditor::OnPasteInto)
EVT_BUTTON(wxID_SAVE, frmRGDEditor::OnSave)
EVT_TOOL(IDC_ToolSave, frmRGDEditor::OnSave)
EVT_TOOL(IDC_ToolViewCode, frmRGDEditor::OnViewCode)
EVT_BUTTON(IDC_ToolViewCode, frmRGDEditor::OnViewCode)
EVT_CLOSE(frmRGDEditor::OnCloseWindow)
END_EVENT_TABLE()

class CRGDTreeItemData : public wxTreeItemData
{
  public:
    CRGDTreeItemData(IMetaNode *pNode, IMetaNode::IMetaTable *pTable, bool DelNode, bool DelTable,
                     bool DelayLoad = false)
        : pNode(pNode), pTable(pTable), DelNode(DelNode), DelTable(DelTable), DelayedLoad(DelayLoad)
    {
    }
    ~CRGDTreeItemData()
    {
        if (DelNode)
        {
            delete pNode;
        }
        if (DelTable)
        {
            delete pTable;
        }
    }
    IMetaNode *pNode;
    IMetaNode::IMetaTable *pTable;
    bool DelayedLoad;

  public:
    bool DelNode;
    bool DelTable;
};

class RGDwxPropertyGridManager : public wxPropertyGridManager
{
  public:
    RGDwxPropertyGridManager(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint &pos = wxDefaultPosition,
                             const wxSize &size = wxDefaultSize, long style = wxPGMAN_DEFAULT_STYLE,
                             const wxString &name = wxASCII_STR(wxPropertyGridManagerNameStr))
        : wxPropertyGridManager(parent, id, pos, size, style, name)
    {
    }

    void OnSize(wxSizeEvent &event)
    {
        event.Skip();
        if (m_pTxtHelpCaption)
        {
            m_pTxtHelpCaption->SetBackgroundColour(GetBackgroundColour());
            m_pTxtHelpCaption->SetWindowStyleFlag(m_pTxtHelpCaption->GetWindowStyleFlag() | wxST_NO_AUTORESIZE);
        }
        if (m_pTxtHelpContent)
        {
            m_pTxtHelpContent->SetBackgroundColour(GetBackgroundColour());
        }
    }

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(RGDwxPropertyGridManager, wxPropertyGridManager)
EVT_SIZE(RGDwxPropertyGridManager::OnSize)
END_EVENT_TABLE()

class MywxDataObjectSimple : public wxDataObjectSimple
{
  protected:
    size_t iLen;
    char *pData;

  public:
    MywxDataObjectSimple(const wxDataFormat &format = wxFormatInvalid) : wxDataObjectSimple(format), iLen(0), pData(0)
    {
    }

    ~MywxDataObjectSimple()
    {
        if (pData)
        {
            delete[] pData;
        }
    }

    virtual size_t GetDataSize() const { return iLen; }

    virtual bool GetDataHere(void *buf) const
    {
        if (iLen)
        {
            memcpy(buf, pData, iLen);
        }
        return true;
    }

    virtual bool SetData(size_t len, const void *buf)
    {
        if (pData)
        {
            delete[] pData;
        }
        memcpy(pData = new char[iLen = len], buf, len);
        return true;
    }
};

wxIMPLEMENT_DYNAMIC_CLASS(myReferencePropertyClass, wxStringProperty);
bool myReferencePropertyClass::OnButtonClick(wxPropertyGrid *propgrid, wxString &value)
{

    frmFileSelector *pSelector =
        new frmFileSelector(TheConstruct->GetModuleService().GetModuleType() == CModuleFile::eModuleType::MT_DawnOfWar
                                ? wxT("data\\attrib\\")
                                : wxT("attrib\\attrib\\"),
                            value, true);

    int res = pSelector->ShowModal();
    if (res == wxID_OK)
    {
        value = pSelector->GetFile();
        delete pSelector;
        return true;
    }

    delete pSelector;
    return false;
}

wxIMPLEMENT_DYNAMIC_CLASS(myUcsRefPropertyClass, wxStringProperty);
bool myUcsRefPropertyClass::OnButtonClick(wxPropertyGrid *propgrid, wxString &value)
{

    // the current value may be invalid, as validator is not called before button press
    size_t iN = 0, iL = value.Len();
    if (iL > 0)
    {
        if (value[0] == '$')
        {
            iN = 1;
        }
        for (; iN < iL; ++iN)
        {
            if (value[iN] < '0' || value[iN] > '9')
            {
                _ErrorBox(wxT("Not a valid UCS reference (numbers only)"), __FILE__, __LINE__);
                return false;
            }
        }
    }

    frmTabDialog *pNewTabs = new frmTabDialog(wxT("UCS Editor for RGD"));

    // launch UCS selector to find UCS
    unsigned long iVal, iVal2 = ULONG_MAX;
    if (value[0] == '$')
    {
        iVal = wcstoul(value.c_str() + 1, 0, 10);
    }
    else
    {
        iVal = wcstoul(value.c_str(), 0, 10);
    }

    frmUCSSelector *pSelector = new frmUCSSelector(wxT("Select UCS to get value from"));

    if (!pSelector->SelectFromReference(iVal))
    {
        bool bIsBlanko = true;
        size_t iL = value.Len();
        for (size_t i = 0; bIsBlanko && (i < iL); ++i)
        {
            if (value[i] != '0' && value[i] != ' ')
            {
                if (i == 0 && value[0] == '$')
                    ;
                else
                {
                    bIsBlanko = false;
                }
            }
        }
        if (!bIsBlanko)
        {
            iVal2 = iVal;
        }
        CUcsTool::HandleSelectorResponse(pSelector, pNewTabs->GetTabs(), &iVal2, true);
        if (!bIsBlanko)
        {
            iVal2 = ULONG_MAX;
        }
    }
    else
    {
        frmUCSEditor *pForm;
        pNewTabs->GetTabs()->AddPage(
            pForm = new frmUCSEditor(pNewTabs->GetTabs(), -1, pSelector->IsAnswerUcsReadOnly(), wxDefaultPosition,
                                     wxWindow::FromDIP(wxSize(620, 750), pNewTabs->GetTabs()), &iVal2),
            wxString()
                .Append(AppStr(ucsedit_tabname))
                .Append(wxT(" ["))
                .Append(pSelector->GetAnswer())
                .Append(wxT("]")),
            true);
        pNewTabs->Show(true);
        pForm->FillFromCUcsFile(pSelector->GetAnswerUcs(), iVal);
        pForm->SetTabStripForLoad(pNewTabs->GetTabs());
        delete pSelector;
    }

    pNewTabs->ShowModal();
    pNewTabs->Destroy();

    if (iVal2 != ULONG_MAX)
    {
        wchar_t sNumberBuffer[34];
        int iN = 0;
        if (value[0] == '$')
        {
            iN = 1, sNumberBuffer[0] = '$';
        }
        _ultow(iVal2, sNumberBuffer + iN, 10);
        value = sNumberBuffer;
        return true;
    }

    return false;
}

wxPGProperty *SetUcsValidator(wxPGProperty *p)
{
    wxTextValidator validator(wxFILTER_INCLUDE_CHAR_LIST);
    wxArrayString oValid;
    oValid.Add(wxT("0"));
    oValid.Add(wxT("1"));
    oValid.Add(wxT("2"));
    oValid.Add(wxT("3"));
    oValid.Add(wxT("4"));
    oValid.Add(wxT("5"));
    oValid.Add(wxT("6"));
    oValid.Add(wxT("7"));
    oValid.Add(wxT("8"));
    oValid.Add(wxT("9"));
    oValid.Add(wxT("$"));
    validator.SetIncludes(oValid);
    p->SetValidator(validator);
    return p;
}

wxValidator *myUcsRefPropertyClass::DoGetValidator() const
{
    wxTextValidator *validator = new wxTextValidator(wxFILTER_INCLUDE_CHAR_LIST);
    wxArrayString oValid;
    oValid.Add(wxT("0"));
    oValid.Add(wxT("1"));
    oValid.Add(wxT("2"));
    oValid.Add(wxT("3"));
    oValid.Add(wxT("4"));
    oValid.Add(wxT("5"));
    oValid.Add(wxT("6"));
    oValid.Add(wxT("7"));
    oValid.Add(wxT("8"));
    oValid.Add(wxT("9"));
    oValid.Add(wxT("$"));
    validator->SetIncludes(oValid);
    return validator;
}

char *frmRGDEditor::_ReadNiceString(FILE *f)
{
    uint32_t iLen;
    fread(&iLen, sizeof(uint32_t), 1, f);
    char *s = new char[iLen + 1];
    s[iLen] = 0;
    fread(s, iLen, 1, f);
    return s;
}

void frmRGDEditor::_MakeSureNodeHelpLoaded()
{
    if (!m_pNodeHelp)
    {
        m_pNodeHelp = new std::map<unsigned long, _NodeHelp>;
        FILE *f = RainmanFOpen(AppStr(app_luareffile), L"rb");
        if (!f)
        {
            CDMS_LOG_WARN("Could not open Lua reference file: {}", AppStr(app_luareffile).mb_str().data());
            return;
        }
        uint32_t iHelpCount;
        fread(&iHelpCount, sizeof(uint32_t), 1, f);
        for (uint32_t i = 0; i < iHelpCount; ++i)
        {
            unsigned long iHash;
            _NodeHelp oHelp;
            fread(&oHelp.iLuaType, sizeof(short int), 1, f);
            fread(&iHash, sizeof(unsigned long), 1, f);
            oHelp.sShortDesc = _ReadNiceString(f);
            oHelp.sLongDesc = _ReadNiceString(f);
            if (oHelp.iLuaType == 100)
            {
                oHelp.lstRefs = new std::list<char *>;
                uint32_t iRefCount;
                fread(&iRefCount, sizeof(uint32_t), 1, f);
                for (uint32_t j = 0; j < iRefCount; ++j)
                {
                    oHelp.lstRefs->push_back(_ReadNiceString(f));
                }
            }
            else
            {
                oHelp.sSample = _ReadNiceString(f);
            }

            m_pNodeHelp->operator[](iHash) = oHelp;
        }

        fclose(f);
    }
}

void frmRGDEditor::FreeNodeHelp()
{
    if (m_pNodeHelp)
    {
        for (std::map<unsigned long, _NodeHelp>::iterator itr = m_pNodeHelp->begin(); itr != m_pNodeHelp->end(); ++itr)
        {
            delete[] itr->second.sShortDesc;
            delete[] itr->second.sLongDesc;
            if (itr->second.iLuaType == 100)
            {
                for (std::list<char *>::iterator itr2 = itr->second.lstRefs->begin();
                     itr2 != itr->second.lstRefs->end(); ++itr2)
                {
                    delete[] *itr2;
                }
                delete itr->second.lstRefs;
            }
            else
            {
                delete[] itr->second.sSample;
            }
        }
        delete m_pNodeHelp;
        m_pNodeHelp = 0;
    }
}

void frmRGDEditor::DoSave()
{
    BackupFile(TheConstruct->GetModuleService().GetModule(), m_sFilename);

    wxString sDir = m_sFilename.BeforeLast('\\');
    auto dirResult = TheConstruct->GetFileService().Iterate(sDir);
    if (!dirResult)
    {
        ErrorBoxS(dirResult.error());
        return;
    }

    auto streamResult = TheConstruct->GetFileService().OpenOutputStream(m_sFilename, true);
    if (!streamResult)
    {
        ErrorBoxS(streamResult.error());
        return;
    }
    TheConstruct->GetFilesList()->UpdateDirectoryChildren(m_oFileParent, dirResult.value().get());

    auto saNewFile = wxStringToAscii(m_sFilename);

    if (m_iObjectType == 1)
    {
        try
        {
            m_pNodeObject->Save(streamResult.value().get());
        }
        catch (const CRainmanException &e)
        {
            RestoreBackupFile(TheConstruct->GetModuleService().GetModule(), m_sFilename);
            ErrorBoxE(e);
            return;
        }
        m_bDataNeedsSaving = false;
        TheConstruct->GetTabManager().UpdateDirtyState(this, false);
        ThemeColours::ShowMessageBox(AppStr(rgd_savegood), AppStr(rgd_save), wxICON_INFORMATION, this);
    }
    else if (m_iObjectType == 3)
    {
        auto sNormPath = CRgdEditorPresenter::NormaliseLua2SavePath(saNewFile.get());

        try
        {
            m_pLua2Object->saveFile(streamResult.value().get(), sNormPath.c_str());
        }
        catch (const CRainmanException &e)
        {
            RestoreBackupFile(TheConstruct->GetModuleService().GetModule(), m_sFilename);
            ErrorBoxE(e);
            return;
        }
        m_bDataNeedsSaving = false;
        TheConstruct->GetTabManager().UpdateDirtyState(this, false);
        ThemeColours::ShowMessageBox(AppStr(rgd_savegood), AppStr(rgd_save), wxICON_INFORMATION, this);
    }
    else
    {
    }
}

void frmRGDEditor::OnSave(wxCommandEvent &event)
{
    UNUSED(event);
    DoSave();
}

void frmRGDEditor::OnViewCode(wxCommandEvent &event)
{
    UNUSED(event);

    auto streamResult = TheConstruct->GetFileService().OpenStream(m_sFilename);
    if (!streamResult)
    {
        ErrorBox("Cannot open file");
        return;
    }

    auto *pForm = new frmScarEditor(m_oFileParent, m_sFilename, TheConstruct->GetTabs(), -1);
    TheConstruct->GetTabManager().AddPage(
        pForm, wxString().Append(wxT("Code [")).Append(OnlyFilename(m_sFilename)).Append(wxT("]")), true);
    pForm->Load(streamResult.value().get());
}

frmRGDEditor::frmRGDEditor(const wxTreeItemId &oFileParent, wxString sFilename, wxWindow *parent, wxWindowID id,
                           const wxPoint &pos, const wxSize &size)
    : m_oFileParent(oFileParent), m_sFilename(sFilename), wxWindow(parent, id, pos, size)
{
    _MakeSureNodeHelpLoaded();
    m_iObjectType = 0;
    m_pNodeObject = 0;
    m_pLua2Object = 0;
    m_bDeleteWhenDone = false;
    m_bDataNeedsSaving = false;

    wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);

    wxToolBar *pToolbar;
    pTopSizer->Add(pToolbar = new wxToolBar(this, -1, wxDefaultPosition, wxDefaultSize,
                                            wxTB_NODIVIDER | wxTB_HORIZONTAL | wxNO_BORDER),
                   0, wxEXPAND | wxALL, 3);
    pTopSizer->Add(m_pSplitter = new wxSplitterWindow(this, -1), 1, wxEXPAND | wxALL, 0);

    auto oSaveBundle = wxArtProvider::GetBitmapBundle(wxART_FILE_SAVE, wxART_TOOLBAR, wxSize(32, 32));
    pToolbar->SetToolBitmapSize(wxSize(32, 32));
    pToolbar->AddTool(IDC_ToolSave, AppStr(rgd_save), oSaveBundle, AppStr(rgd_save));
    pToolbar->AddSeparator();

    // "View Code" tool — SVG icon that scales cleanly at any DPI
    static const char sSvgCode[] =
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">)"
        R"(<polyline points="11,6 4,16 11,26" fill="none" stroke="#d5873e" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>)"
        R"(<polyline points="21,6 28,16 21,26" fill="none" stroke="#d5873e" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>)"
        R"(<line x1="18" y1="4" x2="14" y2="28" stroke="#d5873e" stroke-width="2" stroke-linecap="round"/>)"
        R"(</svg>)";
    auto oCodeBundle = wxBitmapBundle::FromSVG(sSvgCode, wxSize(32, 32));
    pToolbar->AddTool(IDC_ToolViewCode, wxT("View Code"), oCodeBundle, wxT("Open in code editor"));
    pToolbar->Realize();

    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxWindow *pBgTemp;
    pButtonSizer->Add(new wxButton(this, IDC_ToolViewCode, wxT("</> View Code")), 0, wxEXPAND | wxALL, 3);
    pButtonSizer->Add(pBgTemp = new wxButton(this, wxID_SAVE, AppStr(rgd_save)), 0, wxEXPAND | wxALL, 3);
    pTopSizer->Add(pButtonSizer, 0, wxALIGN_RIGHT);

    m_pTables = new wxTreeCtrl(m_pSplitter, IDC_TablesTree, wxDefaultPosition, wxDefaultSize,
                               wxTR_EDIT_LABELS | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
    m_pPropManager = new RGDwxPropertyGridManager(m_pSplitter, IDC_PropertyGrid, wxDefaultPosition, wxDefaultSize,
                                                  wxPG_DEFAULT_STYLE | wxPG_DESCRIPTION);
    m_pPropertyGrid = m_pPropManager->GetGrid();
    ThemeColours::ApplyPropertyGridTheme(m_pPropertyGrid);

    m_pSplitter->SetSashGravity(0.5);
    m_pSplitter->SetMinimumPaneSize(48);
    m_pSplitter->SplitVertically(m_pTables, m_pPropManager);

    SetBackgroundColour(pBgTemp->GetBackgroundColour());
    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);

    CallAfter([this]() { m_pSplitter->SetSashPosition(0); });
}

frmRGDEditor::~frmRGDEditor()
{
    if (m_iObjectType == 3)
    {
        delete m_pTableObject;
    }
    if (m_bDeleteWhenDone)
    {
        if (m_iObjectType == 1)
        {
            delete m_pNodeObject;
        }
        else if (m_iObjectType == 2)
        {
            delete m_pTableObject;
        }
        else if (m_iObjectType == 3)
        {
            m_pTables->DeleteAllItems();
            delete m_pLua2Object;
        }
    }
}

frmRGDEditor::_NodeHelp::_NodeHelp()
{
    iLuaType = 254;
    sShortDesc = 0;
    sLongDesc = 0;
    sSample = 0;
    lstRefs = 0;
}

unsigned long frmRGDEditor::NodeNameToMultiHash(const wchar_t *sName)
{
    return CRgdEditorPresenter::ComputeMultiHash(sName);
}

wxPGProperty *frmRGDEditor::GetMetaNodeEditor(IMetaNode *pNode, wxString sName, wxString sNameId)
{
    try
    {
        if (sName == wxT(""))
        {
            sName = GetMetaNodeName(pNode);
        }
        switch (pNode->VGetType())
        {
        case IMetaNode::DT_Bool:
            return new wxBoolProperty(sName, sNameId, pNode->VGetValueBool());

        case IMetaNode::DT_Integer:
        {
            wchar_t sNumberBuffer[34];
            sNumberBuffer[0] = 0;
            _ultow(pNode->VGetValueInteger(), sNumberBuffer, 10);
            return SetUcsValidator(new myUcsRefPropertyClass(sName, sNameId, sNumberBuffer));
        }

        case IMetaNode::DT_Float:
            return new wxFloatProperty(sName, sNameId, pNode->VGetValueFloat());

        case IMetaNode::DT_String:
            return new wxStringProperty(sName, sNameId, AsciiTowxString(pNode->VGetValueString()));

        case IMetaNode::DT_WString:
            return SetUcsValidator(new myUcsRefPropertyClass(sName, sNameId, pNode->VGetValueWString()));

        case IMetaNode::DT_Table:
        {
            wxArrayString aPossibles;
            if (m_pNodeHelp)
            {
                _NodeHelp oHelp = m_pNodeHelp->operator[](NodeNameToMultiHash(GetMetaNodeName(pNode)));
                if (oHelp.iLuaType == 100)
                {
                    for (std::list<char *>::iterator itr = oHelp.lstRefs->begin(); itr != oHelp.lstRefs->end(); ++itr)
                    {
                        aPossibles.Add(AsciiTowxString(*itr));
                    }
                }
            }
            IMetaNode::IMetaTable *pTable = pNode->VGetValueMetatable();
            wxPGProperty *pTmp;
            switch (pTable->VGetReferenceType())
            {
            case IMetaNode::DT_String:
                // pTmp = wxEditEnumProperty(sName, sNameId, aPossibles,
                // AsciiTowxString(pTable->VGetReferenceString()));
                pTmp = new myReferencePropertyClass(sName, sNameId, AsciiTowxString(pTable->VGetReferenceString()));
                break;

            case IMetaNode::DT_WString:
                // pTmp = wxEditEnumProperty(sName, sNameId, aPossibles, pTable->VGetReferenceWString());
                pTmp = new myReferencePropertyClass(sName, sNameId, pTable->VGetReferenceWString());
                break;

            default:
                // pTmp = wxEditEnumProperty(sName, sNameId, aPossibles, wxT(""));
                pTmp = new myReferencePropertyClass(sName, sNameId, wxT(""));
                break;
            }
            delete pTable;
            return pTmp;
        }
        }
    }
    catch (const CRainmanException &e)
    {
        return 0;
    }
    wxPGProperty *pTmp = new wxStringProperty(sName, sNameId, wxT(""));
    pTmp->Enable(false);
    return pTmp;
}

wxString frmRGDEditor::GetMetaNodeHelp(IMetaNode *pNode)
{
    wxString S;
    bool bNeedGap = false;

    // UCS
    try
    {
        if (pNode->VGetType() == IMetaNode::DT_WString)
        {
            if (CUcsFile::IsDollarString(pNode->VGetValueWString()))
            {
                const wchar_t *sDeref =
                    TheConstruct->GetModuleService().GetModule()->ResolveUCS(pNode->VGetValueWString());
                S.Append(pNode->VGetValueWString()).Append(wxT(" = ")).Append(sDeref ? sDeref : wxT("?"));
                bNeedGap = true;
            }
        }
        else if (pNode->VGetType() == IMetaNode::DT_Integer)
        {
            const wchar_t *sDeref = TheConstruct->GetModuleService().ResolveUCS(pNode->VGetValueInteger());
            wchar_t sNumberBuffer[34];
            sNumberBuffer[0] = '$';
            _ultow(pNode->VGetValueInteger(), sNumberBuffer + 1, 10);
            S.Append(sNumberBuffer).Append(wxT(" = ")).Append(sDeref ? sDeref : wxT("?"));
            bNeedGap = true;
        }
        else if (pNode->VGetType() == IMetaNode::DT_String)
        {
            if (CUcsFile::IsDollarString(pNode->VGetValueString()))
            {
                const wchar_t *sDeref =
                    TheConstruct->GetModuleService().GetModule()->ResolveUCS(pNode->VGetValueString());
                S.Append(AsciiTowxString(pNode->VGetValueString()))
                    .Append(wxT(" = "))
                    .Append(sDeref ? sDeref : wxT("?"));
                bNeedGap = true;
            }
        }
    }
    IGNORE_EXCEPTIONS

    // Short Desc
    if (m_pNodeHelp == 0)
    {
        return S;
    }

    _NodeHelp oHelp = m_pNodeHelp->operator[](NodeNameToMultiHash(GetMetaNodeName(pNode)));
    if (bNeedGap)
    {
        S.Append(wxT("\n"));
    }
    if (oHelp.sShortDesc && *oHelp.sShortDesc)
    {
        S.Append(AsciiTowxString(oHelp.sShortDesc));
        bNeedGap = true;
    }

    // Long Desc
    if (bNeedGap)
    {
        S.Append(wxT("\n"));
    }
    if (oHelp.sLongDesc && *oHelp.sLongDesc)
    {
        S.Append(AsciiTowxString(oHelp.sLongDesc));
        bNeedGap = true;
    }

    // End
    return S;
}

void frmRGDEditor::_DoFillRightSide(IMetaNode *pNode, IMetaNode::IMetaTable *pTable)
{
    m_pPropertyGrid->Append(new wxPropertyCategory(wxT("Properties")));
    // m_pLua2Object
    wxPGProperty *oName =
        m_pPropertyGrid->Append(new wxStringProperty(wxT("Name"), wxT("Name"), GetMetaNodeName(pNode)));
    m_pPropertyGrid->SetPropertyHelpString(oName, wxT("The name of this item"));
    if (m_pLua2Object)
    {
        oName->Enable(false);
    }

    wxArrayString asDataTypes;
    wxArrayInt aiDataTypes;

    asDataTypes.Add(wxT("Number"));
    aiDataTypes.Add(0);
    asDataTypes.Add(wxT("CoH UCS Ref"));
    aiDataTypes.Add(1);
    asDataTypes.Add(wxT("True / False"));
    aiDataTypes.Add(2);
    asDataTypes.Add(wxT("Text"));
    aiDataTypes.Add(3);
    asDataTypes.Add(wxT("DoW UCS Ref"));
    aiDataTypes.Add(4);
    asDataTypes.Add(wxT("Table"));
    aiDataTypes.Add(100);
    asDataTypes.Add(wxT("Unknown"));
    aiDataTypes.Add(254);

    wxPGProperty *oVal;
    m_pPropertyGrid->SetPropertyHelpString(
        m_pPropertyGrid->Append(
            new wxEnumProperty(wxT("Data Type"), wxT("DataType"), asDataTypes, aiDataTypes, (int)pNode->VGetType())),
        wxT("What kind of value this is"));
    if (pTable)
    {
        m_pPropertyGrid->SetPropertyHelpString(
            oVal = m_pPropertyGrid->Append(GetMetaNodeEditor(pNode, wxT("Reference"), wxT("Value"))),
            GetMetaNodeHelp(pNode));
        m_pPropertyGrid->Append(new wxPropertyCategory(wxT("Table Children"), wxT("Children")));
        try
        {
            unsigned long iChildren = pTable->VGetChildCount();
            if (iChildren > 0)
            {
                for (unsigned long i = 0; i < iChildren; ++i)
                {
                    IMetaNode *pNode = pTable->VGetChild(i);
                    wxPGProperty *oEntry = m_pPropertyGrid->Append(GetMetaNodeEditor(pNode, L""));
                    m_pPropertyGrid->SetPropertyHelpString(oEntry, GetMetaNodeHelp(pNode));
                    m_pPropertyGrid->SetPropertyClientData(oEntry, (void *)i);
                    delete pNode;
                }
            }
        }
        catch (const CRainmanException &e)
        {
            ErrorBoxE(e);
        }
    }
    else
    {
        m_pPropertyGrid->SetPropertyHelpString(
            oVal = m_pPropertyGrid->Append(GetMetaNodeEditor(pNode, wxT("Value"), wxT("Value"))),
            GetMetaNodeHelp(pNode));
    }

    m_pPropertyGrid->Thaw();
    m_pPropertyGrid->SetSplitterPosition(m_pPropertyGrid->GetClientSize().GetWidth() / 2);
    // m_pPropertyGrid->SelectProperty(oVal, false);
    m_pPropertyGrid->Refresh();
}

void frmRGDEditor::OnTreeSelect(wxTreeEvent &event)
{
    wxWindow *pEditor = m_pPropertyGrid->GetEditorControl();
    if (pEditor)
    {
        pEditor->Validate();
    }

    m_pPropertyGrid->Clear();
    m_pPropertyGrid->Freeze();

    if (!event.GetItem().IsOk())
    {
        m_pPropertyGrid->Thaw();
        m_pPropertyGrid->Refresh();
        return;
    }
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(event.GetItem());
    if (pData && pData->DelayedLoad && pData->pTable)
    {
        _FillFromMetaTable(event.GetItem(), pData->pTable);
        pData->DelayedLoad = false;
    }

    _DoFillRightSide(pData->pNode, pData->pTable);
}

wxString frmRGDEditor::GetMetaNodeName(IMetaNode *pNode)
{
    if (pNode->VGetName())
    {
        return CRgdEditorPresenter::FormatNodeName(pNode->VGetName(), 0);
    }

    unsigned long iHash = 0;
    try
    {
        iHash = pNode->VGetNameHash();
    }
    catch (const CRainmanException &e)
    {
        return wxT("[unknown name]");
    }
    return CRgdEditorPresenter::FormatNodeName(nullptr, iHash);
}

wxString frmRGDEditor::GetMetaTableValue(IMetaNode::IMetaTable *pTable, IMetaNode *pNode)
{
    wxString sRef;
    bool bHasRef = true;
    try
    {
        switch (pTable->VGetReferenceType())
        {
        case IMetaNode::DT_NoData:
            bHasRef = false;
            break;

        case IMetaNode::DT_WString:
            sRef = pTable->VGetReferenceWString();
            break;

        case IMetaNode::DT_String:
            sRef = AsciiTowxString(pTable->VGetReferenceString());
        }
    }
    catch (const CRainmanException &e)
    {
        return wxT("-- unknown name");
    }
    bool bIsGameData = pNode && GetMetaNodeName(pNode) == wxT("GameData");
    return CRgdEditorPresenter::FormatTableReference(sRef, bIsGameData, bHasRef);
}

bool frmRGDEditor::_SyncTreeView(IMetaNode *pNode, wxTreeItemId &oNode)
{
    bool bRet = true;

    // Sync node name
    wxString sDesiredName = GetMetaNodeName(pNode);
    if (sDesiredName != m_pTables->GetItemText(oNode))
    {
        m_pTables->SetItemText(oNode, sDesiredName);
    }

    // Sync item data
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(oNode);
    if (pData->pNode != pNode)
    {
        if (pData->DelNode)
        {
            delete pData->pNode;
        }
        pData->pNode = pNode;
        pData->DelNode = true;
        bRet = false;
    }
    if (pData->pTable && pData->DelTable)
    {
        delete pData->pTable;
    }
    if (pNode->VGetType() == IMetaNode::DT_Table)
    {
        pData->pTable = pNode->VGetValueMetatable();
        pData->DelTable = true;
    }
    else
    {
        pData->pTable = 0;
        pData->DelTable = false;
    }

    // Sync children
    if (pData->pTable)
    {
        if (pData->DelayedLoad)
        {
            if (pData->pTable->VGetChildCount() == 0)
            {
                m_pTables->SetItemHasChildren(oNode, false);
                pData->DelayedLoad = false;
            }
        }
        else
        {
            if (pData->pTable->VGetChildCount() == 0)
            {
                m_pTables->DeleteChildren(oNode);
                m_pTables->SetItemHasChildren(oNode, false);
            }
            else
            {
                wxTreeItemIdValue oCookie;
                wxTreeItemId oNodeChild = m_pTables->GetFirstChild(oNode, oCookie);
                size_t iChildCount = pData->pTable->VGetChildCount();
                size_t iChild = 0;
                while (iChild < iChildCount || oNodeChild.IsOk())
                {
                    if (iChild == iChildCount)
                    {
                        // Entries in tree not in node -> delete tree entry
                        wxTreeItemId oNextC = m_pTables->GetNextChild(oNode, oCookie);
                        m_pTables->Delete(oNodeChild);
                        oNodeChild = oNextC;
                    }
                    else if (!oNodeChild.IsOk())
                    {
                        // Entries in node not in tree -> append tree entry
                        IMetaNode *pNode = pData->pTable->VGetChild((unsigned long)iChild);
                        wxString sName = GetMetaNodeName(pNode);
                        if (pNode->VGetType() == IMetaNode::DT_Table)
                        {
                            IMetaNode::IMetaTable *pChild = pNode->VGetValueMetatable();
                            if (pChild->VGetChildCount())
                            {
                                wxTreeItemId oChild = m_pTables->AppendItem(
                                    oNode, sName, -1, -1, new CRGDTreeItemData(pNode, pChild, true, true, true));
                                m_pTables->SetItemHasChildren(oChild, true);
                            }
                            else
                            {
                                wxTreeItemId oChild = m_pTables->AppendItem(
                                    oNode, sName, -1, -1, new CRGDTreeItemData(pNode, pChild, true, true, false));
                            }
                        }
                        else
                        {
                            m_pTables->AppendItem(oNode, sName, -1, -1, new CRGDTreeItemData(pNode, 0, true, false));
                        }

                        ++iChild;
                    }
                    else
                    {
                        IMetaNode *pNodeChild = pData->pTable->VGetChild((unsigned long)iChild);

                        wxString sName_pNode = GetMetaNodeName(pNodeChild);
                        wxString sName_oNode = m_pTables->GetItemText(oNodeChild);
                        int iCmp = sName_oNode.Cmp(sName_pNode);
                        if (iCmp < 0)
                        {
                            // sName_oNode < sName_pNode -> item in oNode not in tree -> insert tree
                            if (pNodeChild->VGetType() == IMetaNode::DT_Table)
                            {
                                IMetaNode::IMetaTable *pChild = pNodeChild->VGetValueMetatable();
                                if (pChild->VGetChildCount())
                                {
                                    wxTreeItemId oChild = m_pTables->InsertItem(
                                        oNode, iChild, sName_pNode, -1, -1,
                                        new CRGDTreeItemData(pNodeChild, pChild, true, true, true));
                                    m_pTables->SetItemHasChildren(oChild, true);
                                }
                                else
                                {
                                    wxTreeItemId oChild = m_pTables->InsertItem(
                                        oNode, iChild, sName_pNode, -1, -1,
                                        new CRGDTreeItemData(pNodeChild, pChild, true, true, false));
                                }
                            }
                            else
                            {
                                m_pTables->InsertItem(oNode, iChild, sName_pNode, -1, -1,
                                                      new CRGDTreeItemData(pNodeChild, 0, true, false));
                            }

                            ++iChild;
                        }
                        else if (iCmp > 0)
                        {
                            delete pNodeChild;
                            // sName_oNode > sName_pNode -> item in tree not in oNode -> delete from tree
                            wxTreeItemId oNextC = m_pTables->GetNextChild(oNode, oCookie);
                            m_pTables->Delete(oNodeChild);
                            oNodeChild = oNextC;
                        }
                        else
                        {
                            if (_SyncTreeView(pNodeChild, oNodeChild))
                            {
                                delete pNodeChild;
                            }
                            oNodeChild = m_pTables->GetNextChild(oNode, oCookie);
                            ++iChild;
                        }
                    }
                }
            }
        }
    }
    else
    {
        m_pTables->DeleteChildren(oNode);
        m_pTables->SetItemHasChildren(oNode, false);
    }
    return bRet;
}

bool frmRGDEditor::FillFromMetaNode(CRgdFile *pNode, bool bDeleteWhenDone)
{
    m_iObjectType = 1;
    m_pNodeObject = pNode;
    m_bDeleteWhenDone = bDeleteWhenDone;

    m_pTables->SetWindowStyle(m_pTables->GetWindowStyle() & (~wxTR_HIDE_ROOT));

    wxString sName = GetMetaNodeName(pNode);

    if (pNode->VGetType() == IMetaNode::DT_Table)
    {
        IMetaNode::IMetaTable *pTable = pNode->VGetValueMetatable();
        wxTreeItemId oParent = m_pTables->AddRoot(sName, -1, -1, new CRGDTreeItemData(pNode, pTable, false, true));
        _FillFromMetaTable(oParent, pTable);
        m_pTables->Expand(oParent);
    }
    else
    {
        m_pTables->AddRoot(sName, -1, -1, new CRGDTreeItemData(pNode, 0, false, false));
    }

    return true;
}

bool frmRGDEditor::FillFromLua2(CLuaFile2 *pLua, bool bDeleteWhenDone)
{
    m_iObjectType = 3;
    m_pTableObject = pLua->asMetaTable();
    m_bDeleteWhenDone = bDeleteWhenDone;
    m_pLua2Object = pLua;

    m_pTables->SetWindowStyle(m_pTables->GetWindowStyle() | wxTR_HIDE_ROOT);
    wxTreeItemId oRoot = m_pTables->AddRoot(wxT(""), -1, -1, new CRGDTreeItemData(0, m_pTableObject, false, false));
    _FillFromMetaTable(oRoot, m_pTableObject, true);
    return true;
}

bool frmRGDEditor::FillFromMetaTable(IMetaNode::IMetaTable *pTable, bool bDeleteWhenDone)
{
    m_iObjectType = 2;
    m_pTableObject = pTable;
    m_bDeleteWhenDone = bDeleteWhenDone;

    m_pTables->SetWindowStyle(m_pTables->GetWindowStyle() | wxTR_HIDE_ROOT);
    wxTreeItemId oRoot = m_pTables->AddRoot(wxT(""), -1, -1, new CRGDTreeItemData(0, pTable, false, false));
    _FillFromMetaTable(oRoot, pTable, true);
    return true;
}

void frmRGDEditor::OnTreeExpanding(wxTreeEvent &event)
{
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(event.GetItem());
    if (pData && pData->DelayedLoad && pData->pTable)
    {
        _FillFromMetaTable(event.GetItem(), pData->pTable);
        pData->DelayedLoad = false;
    }
}

void frmRGDEditor::_FillFromMetaTable(const wxTreeItemId &oParent, IMetaNode::IMetaTable *pTable, bool bSkipLuaGlobals)
{
    unsigned long iChildren = pTable->VGetChildCount();
    for (unsigned long i = 0; i < iChildren; ++i)
    {
        IMetaNode *pNode = pTable->VGetChild(i);
        wxString sName = GetMetaNodeName(pNode);
        if (bSkipLuaGlobals)
        {
            if (sName.IsSameAs(wxT("__pow")) || sName.IsSameAs(wxT("_LOADED")) || sName.IsSameAs(wxT("_VERSION")) ||
                sName.IsSameAs(wxT("assert")) || sName.IsSameAs(wxT("collectgarbage")) ||
                sName.IsSameAs(wxT("error")) || sName.IsSameAs(wxT("gcinfo")) || sName.IsSameAs(wxT("Inherit")) ||
                sName.IsSameAs(wxT("InheritMeta")) || sName.IsSameAs(wxT("ipairs")) ||
                sName.IsSameAs(wxT("loadstring")) || sName.IsSameAs(wxT("math")) || sName.IsSameAs(wxT("newproxy")) ||
                sName.IsSameAs(wxT("next")) || sName.IsSameAs(wxT("pairs")) || sName.IsSameAs(wxT("pcall")) ||
                sName.IsSameAs(wxT("rawequal")) || sName.IsSameAs(wxT("Reference")) || sName.IsSameAs(wxT("string")) ||
                sName.IsSameAs(wxT("tonumber")) || sName.IsSameAs(wxT("tostring")) || sName.IsSameAs(wxT("type")) ||
                sName.IsSameAs(wxT("unpack")) || sName.IsSameAs(wxT("xpcall")))
            {
                delete pNode;
                continue;
            }
        }
        if (pNode->VGetType() == IMetaNode::DT_Table)
        {
            IMetaNode::IMetaTable *pChild = pNode->VGetValueMetatable();
            if (pChild->VGetChildCount())
            {
                wxTreeItemId oChild = m_pTables->AppendItem(oParent, sName, -1, -1,
                                                            new CRGDTreeItemData(pNode, pChild, true, true, true));
                m_pTables->SetItemHasChildren(oChild, true);
                if (bSkipLuaGlobals)
                {
                    m_pTables->Expand(oChild);
                }
            }
            else
            {
                wxTreeItemId oChild = m_pTables->AppendItem(oParent, sName, -1, -1,
                                                            new CRGDTreeItemData(pNode, pChild, true, true, false));
            }
            //
        }
        else
        {
            m_pTables->AppendItem(oParent, sName, -1, -1, new CRGDTreeItemData(pNode, 0, true, false));
        }
    }
}

void frmRGDEditor::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

void frmRGDEditor::OnPropertyChange(wxPropertyGridEvent &event)
{
    if (!m_bDataNeedsSaving)
    {
        m_bDataNeedsSaving = true;
        TheConstruct->GetTabManager().UpdateDirtyState(this, true);
    }
    if (event.GetPropertyName().IsSameAs(wxT("Name")))
    {
        // Current selection name
        CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(m_pTables->GetSelection());
        if (pData->pNode->VGetName())
        {
            auto sName = wxStringToAscii(event.GetPropertyValue().GetString());
            try
            {
                pData->pNode->VSetName(sName.get());
                // m_pPropertyGrid->Clear();

                m_pTables->SetItemText(m_pTables->GetSelection(), event.GetPropertyValue().GetString());

                // wxTreeEvent oE;
                // oE.SetItem(m_pTables->GetSelection());
                // OnTreeSelect(oE);
            }
            catch (const CRainmanException &e)
            {
                m_pTables->SetItemText(m_pTables->GetSelection(), event.GetPropertyValue().GetString());
                m_pPropertyGrid->SetPropertyHelpString(wxT("Name"), GetMetaNodeHelp(pData->pNode));
                ErrorBoxE(e);
            }
        }
        else
        {
            ThemeColours::ShowMessageBox(AppStr(rgd_hashname), AppStr(rgd_errortitle), wxICON_ERROR, this);
            event.GetProperty()->SetValueFromString(GetMetaNodeName(pData->pNode));
        }
    }
    else if (event.GetPropertyName().IsSameAs(wxT("DataType")))
    {
        // Current selection type
        CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(m_pTables->GetSelection());
        int iOld = (int)pData->pNode->VGetType(), iNew = event.GetPropertyValue().GetLong();
        if (iOld != iNew)
        {
            try
            {
                pData->pNode->VSetType((IMetaNode::eDataTypes)iNew);
                m_pPropertyGrid->Freeze();
                m_pPropertyGrid->ReplaceProperty(
                    wxT("Value"), (iNew == 100 ? GetMetaNodeEditor(pData->pNode, wxT("Reference"), wxT("Value"))
                                               : GetMetaNodeEditor(pData->pNode, wxT("Value"), wxT("Value"))));
                if (iNew == 100)
                {
                    m_pPropertyGrid->Append(new wxPropertyCategory(wxT("Table Children"), wxT("Children")));
                    pData->pTable = pData->pNode->VGetValueMetatable();
                    pData->DelTable = true;
                }
                else if (iOld == 100)
                {
                    wxPGProperty *oChildren = m_pPropertyGrid->GetPropertyByName(wxT("Children"));
                    m_pPropertyGrid->DeleteProperty(oChildren);
                    m_pTables->DeleteChildren(m_pTables->GetSelection());
                    if (pData->DelTable)
                    {
                        delete pData->pTable;
                    }
                    pData->pTable = 0;
                    pData->DelTable = false;
                }
                m_pPropertyGrid->Thaw();
                m_pPropertyGrid->Refresh();
                m_pTables->Refresh();
            }
            catch (const CRainmanException &e)
            {
                ErrorBoxE(e);
                event.GetProperty()->SetValue(wxVariant(iOld));
                m_pPropertyGrid->Refresh();
            }
        }
    }
    else if (event.GetPropertyName().IsSameAs(wxT("Value")))
    {
        // Current selection value
        _DoValueChange(m_pTables->GetSelection(), event, event.GetProperty(), false);
    }
    else
    {
        // Current selection child value
        unsigned long iChild = (unsigned long)event.GetProperty()->GetClientData();
        const wxTreeItemId oParent = m_pTables->GetSelection();
        wxTreeItemIdValue oTreeCookie;
        wxTreeItemId oTreeItem = m_pTables->GetFirstChild(oParent, oTreeCookie);
        while (iChild && oTreeItem.IsOk())
        {
            oTreeItem = m_pTables->GetNextChild(oParent, oTreeCookie);
            --iChild;
        }
        if (oTreeItem.IsOk())
        {
            _DoValueChange(oTreeItem, event, event.GetProperty(), true);
        }
    }
}

void frmRGDEditor::_DoValueChange(wxTreeItemId oTreeItem, wxPropertyGridEvent &event, wxPGProperty *oPropItem,
                                  bool bIsChild)
{
    UNUSED(bIsChild);
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(oTreeItem);
    switch (pData->pNode->VGetType())
    {
    case IMetaNode::DT_Bool:
        try
        {
            pData->pNode->VSetValueBool(event.GetPropertyValue().GetBool() ? true : false);
        }
        catch (const CRainmanException &e)
        {
            event.GetProperty()->SetValueFromInt(pData->pNode->VGetValueBool() ? 1 : 0);
            ErrorBoxE(e);
        }
        break;
    case IMetaNode::DT_Integer:
    {
        try
        {
            pData->pNode->VSetValueInteger(wcstoul(event.GetPropertyValue().GetString().c_str(), 0, 10));
            m_pPropertyGrid->SetPropertyHelpString(oPropItem, GetMetaNodeHelp(pData->pNode));
        }
        catch (const CRainmanException &e)
        {
            wchar_t sNumberBuffer[34];
            sNumberBuffer[0] = 0;
            _ultow(pData->pNode->VGetValueInteger(), sNumberBuffer, 10);

            event.GetProperty()->SetValue(wxVariant(wxString(sNumberBuffer)));
            ErrorBoxE(e);
        }
        break;
    }
    case IMetaNode::DT_Float:
        try
        {
            pData->pNode->VSetValueFloat((float)event.GetPropertyValue().GetDouble());
        }
        catch (const CRainmanException &e)
        {
            event.GetProperty()->SetValue(wxVariant((double)pData->pNode->VGetValueFloat()));
            ErrorBoxE(e);
        }
        break;
    case IMetaNode::DT_String:
    {
        auto saValue = wxStringToAscii(event.GetPropertyValue().GetString());
        try
        {
            pData->pNode->VSetValueString(saValue.get());
        }
        catch (const CRainmanException &e)
        {
            event.GetProperty()->SetValueFromString(AsciiTowxString(pData->pNode->VGetValueString()));
            ErrorBoxE(e);
        }
        break;
    }
    case IMetaNode::DT_WString:
        try
        {
            pData->pNode->VSetValueWString(event.GetPropertyValue().GetString());
            m_pPropertyGrid->SetPropertyHelpString(oPropItem, GetMetaNodeHelp(pData->pNode));
        }
        catch (const CRainmanException &e)
        {
            event.GetProperty()->SetValueFromString(pData->pNode->VGetValueWString());
            ErrorBoxE(e);
        }
        break;
    case IMetaNode::DT_Table:
        // Needs fixing maybe? (LUAs)
        switch (pData->pTable->VGetReferenceType())
        {
        case IMetaNode::DT_NoData:
            pData->pTable->VSetReferenceType(IMetaNode::DT_String);
        case IMetaNode::DT_String:
        {
            auto saValue = wxStringToAscii(event.GetPropertyValue().GetString());
            try
            {
                pData->pTable->VSetReferenceString(saValue.get());
            }
            catch (const CRainmanException &e)
            {
                event.GetProperty()->SetValueFromString(AsciiTowxString(pData->pTable->VGetReferenceString()));
                ErrorBoxE(e);
            }
            break;
        }
        case IMetaNode::DT_WString:
            try
            {
                pData->pTable->VSetReferenceWString(event.GetPropertyValue().GetString());
            }
            catch (const CRainmanException &e)
            {
                event.GetProperty()->SetValueFromString(pData->pTable->VGetReferenceWString());
                ErrorBoxE(e);
            }
            break;
        };
        break;
    };
}

void frmRGDEditor::OnTreeRightClick(wxTreeEvent &event)
{
    int flags;
    wxTreeItemId oItemID = m_pTables->HitTest(event.GetPoint(), flags);
    if (oItemID.IsOk())
    {
        m_pTables->SelectItem(oItemID);
    }
    else
    {
        return;
    }

    wxMenu *pPopup = new wxMenu;
    pPopup->Append(IDC_AddChild, wxT("Add Child"), wxT("Add a new entry to this table"));

    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(oItemID);
    if (pData && pData->DelayedLoad && pData->pTable)
    {
        _FillFromMetaTable(event.GetItem(), pData->pTable);
        pData->DelayedLoad = false;
    }
    if (pData->pNode->VGetType() != IMetaNode::DT_Table)
    {
        pPopup->Enable(IDC_AddChild, false);
    }
    pPopup->Append(IDC_Delete, wxT("Delete"), wxT("Delete this entry"));

    pPopup->Append(IDC_Copy, wxT("Copy"), wxT("Copy this entry"));
    pPopup->Append(IDC_Paste, wxT("Paste"), wxT("Paste over this entry"));
    pPopup->Append(IDC_PasteInto, wxT("Paste Into"), wxT("Paste as new child in this table"));

    pPopup->Enable(IDC_Paste, false);
    pPopup->Enable(IDC_PasteInto, false);

    if (wxTheClipboard->IsSupported(wxDataFormat(wxT("application/x-rainman-rgd"))))
    {
        pPopup->Enable(IDC_Paste, true);
        if (pData->pNode->VGetType() == IMetaNode::DT_Table)
        {
            pPopup->Enable(IDC_PasteInto, true);
        }
    }

    PopupMenu(pPopup);
    delete pPopup;
}

void frmRGDEditor::OnCopy(wxCommandEvent &event)
{
    UNUSED(event);
    wxTreeItemId oThis;
    CRGDTreeItemData *pChild = (CRGDTreeItemData *)m_pTables->GetItemData(m_pTables->GetSelection());
    if (!pChild->pNode)
    {
        ThemeColours::ShowMessageBox(AppStr(rgd_cannotcopy), AppStr(rgd_errortitle), wxICON_ERROR, this);
        return;
    }

    CMemoryStore::COutStream *pDataStr = pChild->pNode->VGetNodeAsRainmanRgd();

    MywxDataObjectSimple *pData = new MywxDataObjectSimple(wxDataFormat(wxT("application/x-rainman-rgd")));
    pData->SetData(pDataStr->GetDataLength(), pDataStr->GetData());
    delete pDataStr;

    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(pData);
        wxTheClipboard->Close();
    }
    else
    {
        ThemeColours::ShowMessageBox(AppStr(rgd_cannotopenclip), AppStr(rgd_errortitle), wxICON_ERROR, this);
    }
}

void frmRGDEditor::OnPaste(wxCommandEvent &event)
{
    UNUSED(event);
    wxTreeItemId oNode = m_pTables->GetSelection();
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(oNode);

    if (wxTheClipboard->Open())
    {
        if (wxTheClipboard->IsSupported(wxDataFormat(wxT("application/x-rainman-rgd"))))
        {
            m_bDataNeedsSaving = true;
            TheConstruct->GetTabManager().UpdateDirtyState(this, true);

            MywxDataObjectSimple oData(wxDataFormat(wxT("application/x-rainman-rgd")));
            wxTheClipboard->GetData(oData);
            auto pObjData = std::make_unique<char[]>(oData.GetDataSize());
            oData.GetDataHere(pObjData.get());
            std::unique_ptr<IFileStore::IStream> pStream(
                CMemoryStore::OpenStreamExt(pObjData.release(), (unsigned long)oData.GetDataSize(), true));

            pData->pNode->SGetNodeFromRainmanRgd(pStream.get(), false);

            _SyncTreeView(pData->pNode, oNode);

            m_pPropertyGrid->Freeze();
            m_pPropertyGrid->Clear();
            _DoFillRightSide(pData->pNode, pData->pTable);
        }
        wxTheClipboard->Close();
    }
}

void frmRGDEditor::OnPasteInto(wxCommandEvent &event)
{
    UNUSED(event);
    wxTreeItemId oNode = m_pTables->GetSelection();
    CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(oNode);

    if (wxTheClipboard->Open())
    {
        if (wxTheClipboard->IsSupported(wxDataFormat(wxT("application/x-rainman-rgd"))))
        {
            MywxDataObjectSimple oData(wxDataFormat(wxT("application/x-rainman-rgd")));
            wxTheClipboard->GetData(oData);
            auto pObjData = std::make_unique<char[]>(oData.GetDataSize());
            oData.GetDataHere(pObjData.get());
            std::unique_ptr<IFileStore::IStream> pStream(
                CMemoryStore::OpenStreamExt(pObjData.release(), (unsigned long)oData.GetDataSize(), true));

            unsigned long iNodeHash, iNodeNameLen;
            char *sNodeName = 0;

            pStream->VRead(1, sizeof(long), &iNodeHash);
            pStream->VRead(1, sizeof(long), &iNodeNameLen);
            if (iNodeNameLen)
            {
                auto pNodeName = std::make_unique<char[]>(iNodeNameLen + 1);
                pStream->VRead(iNodeNameLen, 1, pNodeName.get());
                pNodeName[iNodeNameLen] = 0;
                sNodeName = pNodeName.release();
            }
            pStream->VSeek(0, IFileStore::IStream::SL_Root);

            unsigned long iChildCount = pData->pTable->VGetChildCount();
            IMetaNode *pExistingChild = 0;
            for (unsigned long i = 0; i < iChildCount; ++i)
            {
                IMetaNode *pChildNode = pData->pTable->VGetChild(i);

                if (pChildNode->VGetName() && sNodeName)
                {
                    if (strcmp(pChildNode->VGetName(), sNodeName) == 0)
                    {
                        pExistingChild = pChildNode;
                        break;
                    }
                }
                else
                {
                    if (pChildNode->VGetNameHash() == iNodeHash)
                    {
                        pExistingChild = pChildNode;
                        break;
                    }
                }

                delete pChildNode;
            }
            if (sNodeName)
            {
                delete[] sNodeName;
            }
            if (!pExistingChild)
            {
                pExistingChild = pData->pTable->VAddChild("__$ModStudioTemp$__");
            }
            pExistingChild->SGetNodeFromRainmanRgd(pStream.get(), true);
            delete pExistingChild;

            _SyncTreeView(pData->pNode, oNode);

            m_pPropertyGrid->Freeze();
            m_pPropertyGrid->Clear();
            _DoFillRightSide(pData->pNode, pData->pTable);
        }
        wxTheClipboard->Close();
    }
}

void frmRGDEditor::OnAddChild(wxCommandEvent &event)
{
    UNUSED(event);
    wxString sName = wxGetTextFromUser(wxT("Name of new child:"), wxT("Add Child"), wxT("new"), (wxWindow *)this);
    if (sName != wxT(""))
    {
        auto saName = wxStringToAscii(sName);
        IMetaNode *pNode, *pNode2;
        CRGDTreeItemData *pData = (CRGDTreeItemData *)m_pTables->GetItemData(m_pTables->GetSelection());
        if (pData->pTable)
        {
            try
            {
                pNode = pData->pTable->VAddChild(saName.get());
            }
            catch (const CRainmanException &e)
            {
                ErrorBoxE(e);
                return;
            }

            unsigned long iID;
            wxPGProperty *oPropItem = m_pPropertyGrid->GetPropertyByName(wxT("Children"))->Item(0);
            for (iID = 0; iID < pData->pTable->VGetChildCount(); ++iID)
            {
                pNode2 = pData->pTable->VGetChild(iID);
                bool bNames = (pNode->VGetName() && pNode2->VGetName());
                if ((bNames && (stricmp(pNode->VGetName(), pNode2->VGetName()) == 0)) ||
                    (!bNames && pNode->VGetNameHash() == pNode2->VGetNameHash()))
                {
                    wxPGProperty *oEntry;
                    if (iID == (pData->pTable->VGetChildCount() - 1))
                    {
                        m_pTables->AppendItem(m_pTables->GetSelection(), AsciiTowxString(saName.get()), -1, -1,
                                              new CRGDTreeItemData(pNode, 0, true, false));
                        oEntry = m_pPropertyGrid->Append(GetMetaNodeEditor(pNode, L""));
                        m_pPropertyGrid->Refresh();
                        m_pTables->Refresh();
                    }
                    else
                    {
                        m_pTables->InsertItem(m_pTables->GetSelection(), iID, AsciiTowxString(saName.get()), -1, -1,
                                              new CRGDTreeItemData(pNode, 0, true, false));
                        oEntry = m_pPropertyGrid->Insert(oPropItem, GetMetaNodeEditor(pNode, L""));
                    }
                    m_pPropertyGrid->SetPropertyHelpString(oEntry, GetMetaNodeHelp(pNode));
                    m_pPropertyGrid->SetPropertyClientData(oEntry, (void *)iID);
                    oEntry = GetNextPGSibling(oEntry);
                    while (oEntry != nullptr)
                    {
                        m_pPropertyGrid->SetPropertyClientData(
                            oEntry, (void *)(((unsigned long)m_pPropertyGrid->GetPropertyClientData(oEntry)) + 1));
                        oEntry = GetNextPGSibling(oEntry);
                    }
                    pNode = 0;
                    delete pNode2;
                    break;
                }
                delete pNode2;
                oPropItem = GetNextPGSibling(oPropItem);
            }
            delete pNode;
        }
        else
        {
            ThemeColours::ShowMessageBox(AppStr(rgd_cantaddchild), AppStr(rgd_errortitle), wxICON_ERROR, this);
        }
    }
}

void frmRGDEditor::OnDelete(wxCommandEvent &event)
{
    UNUSED(event);
    wxTreeItemId oParent, oThis;
    oThis = m_pTables->GetSelection();
    oParent = m_pTables->GetItemParent(oThis);
    if (!oParent.IsOk())
    {
        ThemeColours::ShowMessageBox(AppStr(rgd_cantdelete), AppStr(rgd_errortitle), wxICON_ERROR, this);
        return;
    }
    CRGDTreeItemData *pParent = (CRGDTreeItemData *)m_pTables->GetItemData(oParent);
    CRGDTreeItemData *pChild = (CRGDTreeItemData *)m_pTables->GetItemData(oThis);
    if (!pParent || !pParent->pTable)
    {
        ThemeColours::ShowMessageBox(AppStr(rgd_cantdelete), AppStr(rgd_errortitle), wxICON_ERROR, this);
        return;
    }
    for (unsigned long iID = 0; iID < pParent->pTable->VGetChildCount(); ++iID)
    {
        IMetaNode *pNode = pParent->pTable->VGetChild(iID);
        bool bNames = (pNode->VGetName() && pChild->pNode->VGetName());
        if ((bNames && (stricmp(pNode->VGetName(), pChild->pNode->VGetName()) == 0)) ||
            (!bNames && pNode->VGetNameHash() == pChild->pNode->VGetNameHash()))
        {
            try
            {
                pParent->pTable->VDeleteChild(iID);
                m_pTables->Delete(oThis);
                m_pPropManager->ClearPage(m_pPropManager->GetSelectedPage());
            }
            catch (const CRainmanException &e)
            {
                ErrorBoxE(e);
            }
            delete pNode;
            return;
        }
        delete pNode;
    }
    ThemeColours::ShowMessageBox(AppStr(rgd_cantdelete), AppStr(rgd_errortitle), wxICON_ERROR, this);
}

void frmRGDEditor::OnCloseWindow(wxCloseEvent &event)
{
    if (m_bDataNeedsSaving)
    {
        int ans = ThemeColours::ShowMessageBox(wxT("File has been modified. Save file?"), m_sFilename,
                                               wxYES_NO | wxCANCEL | wxICON_WARNING, this);

        switch (ans)
        {
        case wxYES:
            DoSave();
            break;

        case wxCANCEL:
        default:
            if (event.CanVeto())
            {
                event.Veto();
            }
        case wxNO:
            break;
        }
    }
}
