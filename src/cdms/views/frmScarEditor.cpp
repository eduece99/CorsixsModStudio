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

#include "frmScarEditor.h"
#include "CHoverPopup.h"
#include "FindBar.h"
#include "ScarReferenceLoader.h"
#include "ScarSyntaxConfig.h"
#include "frame/Construct.h"
#include "common/strconv.h"
#include "common/strings.h"
#include "common/Utility.h"
#include "common/ThemeColours.h"
#include "presenters/CScarEditorPresenter.h"
#include "res/Icons.h"
extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <rainman/lua/Lua51.h>
}
#include <wx/artprov.h>
#include <wx/toolbar.h>
#include <wx/tbarbase.h>
#include <wx/popupwin.h>
#include <wx/display.h>
#include <wx/menu.h>
#include <wx/filename.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "common/Common.h"
#include <cstdint>
#include <rainman/core/RainmanLog.h>
#include <lsp/LspClient.h>
#include <lsp/Protocol.h>

BEGIN_EVENT_TABLE(frmScarEditor, wxWindow)
EVT_SIZE(frmScarEditor::OnSize)
EVT_TOOL(IDC_ToolCheck, frmScarEditor::OnCheckLua)
EVT_BUTTON(IDC_Compile, frmScarEditor::OnCheckLua)
EVT_TOOL(IDC_ToolSave, frmScarEditor::OnSave)
EVT_BUTTON(wxID_SAVE, frmScarEditor::OnSave)
EVT_STC_CHARADDED(IDC_Text, frmScarEditor::OnCharAdded)
EVT_STC_MODIFIED(IDC_Text, frmScarEditor::OnModified)
EVT_STC_STYLENEEDED(IDC_Text, frmScarEditor::OnStyleNeeded)
EVT_STC_USERLISTSELECTION(IDC_Text, frmScarEditor::OnAutoCompChoose)
EVT_STC_SAVEPOINTLEFT(IDC_Text, frmScarEditor::OnSavePointLeave)
EVT_STC_SAVEPOINTREACHED(IDC_Text, frmScarEditor::OnSavePointReach)
EVT_CLOSE(frmScarEditor::OnCloseWindow)
EVT_CHOICE(IDC_FunctionDrop, frmScarEditor::OnFuncListChoose)
EVT_TIMER(wxID_ANY, frmScarEditor::OnLspTimer)
EVT_STC_DWELLSTART(IDC_Text, frmScarEditor::OnDwellStart)
EVT_STC_DWELLEND(IDC_Text, frmScarEditor::OnDwellEnd)
EVT_STC_UPDATEUI(IDC_Text, frmScarEditor::OnUpdateUI)
EVT_MENU(IDC_GoToDefinition, frmScarEditor::OnContextGoToDef)
END_EVENT_TABLE()

void frmScarEditor::_RestorePreviousCalltip()
{
    if (m_stkCalltips.size())
    {
        if (m_pSTC->CallTipActive())
        {
            m_pSTC->CallTipCancel();
        }
        m_oThisCalltip = m_stkCalltips.top();
        m_pSTC->CallTipShow(m_oThisCalltip.iPos, m_oThisCalltip.sTip);
        m_stkCalltips.pop();
    }
}

void frmScarEditor::OnSave(wxCommandEvent &event)
{
    UNUSED(event);
    DoSave();
}

void frmScarEditor::DoSave()
{
    if (m_bReadOnly)
    {
        return;
    }
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

    wxString sContent = m_pSTC->GetText();
    auto saContent = wxStringToAscii(sContent);
    if (!saContent)
    {
        ErrorBoxAS(err_memory);
        return;
    }

    try
    {
        streamResult.value()->VWrite((unsigned long)sContent.Length(), 1, saContent.get());
    }
    catch (const CRainmanException &e)
    {
        RestoreBackupFile(TheConstruct->GetModuleService().GetModule(), m_sFilename);
        ErrorBoxE(e);
        return;
    }
    m_bNeedsSaving = false;
    m_pSTC->SetSavePoint();
    TheConstruct->GetTabManager().UpdateDirtyState(this, false);
    ThemeColours::ShowMessageBox(AppStr(scar_savegood), AppStr(scar_save), wxICON_INFORMATION, this);
}

void frmScarEditor::OnSavePointLeave(wxStyledTextEvent &event)
{
    UNUSED(event);
    m_bNeedsSaving = true;
    TheConstruct->GetTabManager().UpdateDirtyState(this, true);
}

void frmScarEditor::OnSavePointReach(wxStyledTextEvent &event)
{
    UNUSED(event);
    m_bNeedsSaving = false;
    TheConstruct->GetTabManager().UpdateDirtyState(this, false);
}

void frmScarEditor::OnCloseWindow(wxCloseEvent &event)
{
    if (m_bNeedsSaving)
    {
        int ans = ThemeColours::ShowMessageBox(wxT("Text has been modified. Save file?"), m_sFilename,
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

void frmScarEditor::OnAutoCompChoose(wxStyledTextEvent &event)
{
    UNUSED(event);
    _RestorePreviousCalltip();
}

void frmScarEditor::ShowAutoComplete()
{
    if (m_pSTC->AutoCompActive())
    {
        return;
    }

    int iPos = m_pSTC->GetCurrentPos();
    int iWordPos = m_pSTC->WordStartPosition(iPos, true);
    int iWordLen = iPos - iWordPos;

    if (m_bLspOpen)
    {
        // LSP is active — use language server completions exclusively
        auto *pFrame = GetConstruct();
        auto *pClient = pFrame ? pFrame->GetLspClient() : nullptr;
        if (pClient)
        {
            int line = m_pSTC->LineFromPosition(iPos);
            int col = iPos - m_pSTC->PositionFromLine(line);
            // Remap cursor position to translated coordinates for LSP
            lsp::Position lspPos{line, col};
            if (m_lspTranslation)
            {
                lspPos = lsp::CScarAnnotationTranslator::MapToTranslated(*m_lspTranslation, lspPos);
            }
            pClient->RequestCompletion(m_sLspUri, lspPos.line, lspPos.character,
                                       [this, iWordLen](lsp::CompletionList completions)
                                       {
                                           if (completions.items.empty())
                                           {
                                               return;
                                           }

                                           // Map LSP CompletionItemKind to our icon types
                                           auto mapKind = [](lsp::CompletionItemKind kind) -> int
                                           {
                                               switch (kind)
                                               {
                                               case lsp::CompletionItemKind::Function:
                                               case lsp::CompletionItemKind::Method:
                                                   return ACT_Function;
                                               case lsp::CompletionItemKind::Keyword:
                                                   return ACT_Keyword;
                                               case lsp::CompletionItemKind::Constant:
                                               case lsp::CompletionItemKind::EnumMember:
                                                   return ACT_Constant;
                                               case lsp::CompletionItemKind::Variable:
                                               case lsp::CompletionItemKind::Field:
                                               case lsp::CompletionItemKind::Property:
                                                   return ACT_LocalFunc;
                                               default:
                                                   return ACT_Keyword;
                                               }
                                           };

                                           wxString sItems;
                                           for (size_t i = 0; i < completions.items.size(); ++i)
                                           {
                                               if (i > 0)
                                               {
                                                   sItems.Append(' ');
                                               }
                                               sItems.Append(wxString::FromUTF8(completions.items[i].label));
                                               sItems.Append(
                                                   wxString::Format(wxT("?%d"), mapKind(completions.items[i].kind)));
                                           }

                                           _PushThisCalltip();
                                           m_pSTC->AutoCompSetOrder(wxSTC_ORDER_CUSTOM);
                                           m_pSTC->AutoCompShow(iWordLen, sItems);
                                       });
        }
        return;
    }

    // LSP is not active — use legacy SCAR/Lua autocomplete
    wxString sPrefix;
    if (iWordLen > 0)
    {
        sPrefix = m_pSTC->GetTextRange(iWordPos, iPos).Lower();
    }

    // Collect candidates as (name, type) pairs
    struct AutoCompItem
    {
        wxString sName;
        int iType;
    };
    std::vector<AutoCompItem> vCandidates;

    // SCAR API functions and constants
    for (const auto &fn : m_vScarFunctions)
    {
        wxString sName = wxString::FromUTF8(fn.sName);
        if (iWordLen == 0 || sName.Lower().StartsWith(sPrefix))
        {
            vCandidates.push_back({sName, fn.iType == 0 ? ACT_Function : ACT_Constant});
        }
    }

    // Lua keywords
    static const wxChar *const kLuaKeywords[] = {
        wxT("and"),   wxT("break"), wxT("do"),       wxT("else"),   wxT("elseif"), wxT("end"),
        wxT("false"), wxT("for"),   wxT("function"), wxT("if"),     wxT("in"),     wxT("local"),
        wxT("nil"),   wxT("not"),   wxT("or"),       wxT("repeat"), wxT("return"), wxT("then"),
        wxT("true"),  wxT("until"), wxT("while"),    nullptr};
    for (const wxChar *const *pp = kLuaKeywords; *pp; ++pp)
    {
        wxString sKw(*pp);
        if (iWordLen == 0 || sKw.Lower().StartsWith(sPrefix))
        {
            vCandidates.push_back({sKw, ACT_Keyword});
        }
    }

    // Local function definitions from current source
    auto localFuncs = CScarEditorPresenter::ParseFunctionDefinitions(m_pSTC->GetText());
    for (const auto &def : localFuncs)
    {
        if (iWordLen == 0 || def.sName.Lower().StartsWith(sPrefix))
        {
            vCandidates.push_back({def.sName, ACT_LocalFunc});
        }
    }

    if (vCandidates.empty())
    {
        return;
    }

    // Sort by name case-insensitively and deduplicate
    std::sort(vCandidates.begin(), vCandidates.end(),
              [](const AutoCompItem &a, const AutoCompItem &b) { return a.sName.CmpNoCase(b.sName) < 0; });
    vCandidates.erase(std::unique(vCandidates.begin(), vCandidates.end(),
                                  [](const AutoCompItem &a, const AutoCompItem &b)
                                  { return a.sName.CmpNoCase(b.sName) == 0; }),
                      vCandidates.end());

    // Build space-separated list with type suffixes (e.g. "SGroup_Count?1")
    wxString sItems;
    for (size_t i = 0; i < vCandidates.size(); ++i)
    {
        if (i > 0)
        {
            sItems.Append(' ');
        }
        sItems.Append(vCandidates[i].sName);
        sItems.Append(wxString::Format(wxT("?%d"), vCandidates[i].iType));
    }

    _PushThisCalltip();
    m_pSTC->AutoCompSetOrder(wxSTC_ORDER_PRESORTED);
    m_pSTC->AutoCompShow(iWordLen, sItems);
}

void frmScarEditor::OnKeyDown(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_SPACE && event.ControlDown() && !event.AltDown() && !event.ShiftDown())
    {
        ShowAutoComplete();
        return;
    }
    event.Skip();
}

void frmScarEditor::_PushThisCalltip()
{
    if (m_pSTC->CallTipActive())
    {
        m_stkCalltips.push(m_oThisCalltip);
    }
    else
    {
        while (m_stkCalltips.size())
        {
            m_stkCalltips.pop();
        }
    }
}

void frmScarEditor::OnCharAdded(wxStyledTextEvent &event)
{
    int iCurrentLine = m_pSTC->GetCurrentLine();
    if ((char)event.GetKey() == '\n')
    {
        int iLineIndentation = 0;
        if (iCurrentLine > 0)
        {
            iLineIndentation = m_pSTC->GetLineIndentation(iCurrentLine - 1);
        }
        if (iLineIndentation == 0)
        {
            return;
        }
        m_pSTC->SetLineIndentation(iCurrentLine, iLineIndentation);
        m_pSTC->LineEnd();
    }
    else if ((char)event.GetKey() == '(' && !m_bLspOpen)
    {
        // Legacy SCAR reference calltip — skipped when LSP handles signature help natively
        _PushThisCalltip();
        int iPos = m_pSTC->GetCurrentPos() - 1;
        int iWordPos = m_pSTC->WordStartPosition(iPos, false);
        m_oThisCalltip.iPos = iWordPos;
        wxString sWord = m_pSTC->GetTextRange(iWordPos, iPos);
        std::string sWordUtf8(sWord.ToUTF8());
        for (const auto &fn : m_vScarFunctions)
        {
            if (fn.iType == 0 && sWordUtf8 == fn.sName)
            {
                m_oThisCalltip.sTip = wxT("");
                m_oThisCalltip.sTip.Append(wxString::FromUTF8(fn.sReturn));
                m_oThisCalltip.sTip.Append(wxT(" "));
                m_oThisCalltip.sTip.Append(wxString::FromUTF8(fn.sName));
                m_oThisCalltip.sTip.Append(wxT("("));
                bool bNeedComma = false;
                for (const auto &arg : fn.sArguments)
                {
                    if (bNeedComma)
                    {
                        m_oThisCalltip.sTip.Append(wxT(",\n    "));
                    }
                    m_oThisCalltip.sTip.Append(wxString::FromUTF8(arg));
                    bNeedComma = true;
                }
                m_oThisCalltip.sTip.Append(wxT(")\n"));
                m_oThisCalltip.sTip.Append(wxString::FromUTF8(fn.sDesc));
                if (m_pSTC->CallTipActive())
                {
                    m_pSTC->CallTipCancel();
                }
                m_pSTC->CallTipSetBackground(ThemeColours::CallTipBg());
                m_pSTC->CallTipSetForeground(ThemeColours::CallTipFg());
                m_pSTC->CallTipShow(iWordPos, m_oThisCalltip.sTip);
                return;
            }
        }
        if (m_pSTC->CallTipActive())
        {
            m_pSTC->CallTipCancel();
        }
        m_pSTC->CallTipSetBackground(ThemeColours::CallTipBg());
        m_pSTC->CallTipSetForeground(ThemeColours::CallTipFg());
        m_oThisCalltip.sTip = wxT("No help available");
        m_pSTC->CallTipShow(iWordPos, m_oThisCalltip.sTip);
    }
    else if ((char)event.GetKey() == '_')
    {
        // Legacy autocomplete trigger — skip when LSP handles completions
        if (!m_bLspOpen)
        {
            ShowAutoComplete();
        }
    }
    else if ((char)event.GetKey() == ')')
    {
        m_pSTC->CallTipCancel();
        _RestorePreviousCalltip();
    }
}

void frmScarEditor::Load(IFileStore::IStream *pFile)
{
    long iLength;
    try
    {
        pFile->VSeek(0, IFileStore::IStream::SL_End);
        iLength = pFile->VTell();
        pFile->VSeek(0, IFileStore::IStream::SL_Root);
    }
    catch (const CRainmanException &e)
    {
        ErrorBoxE(e);
        return;
    }

    auto sBuffer = std::make_unique<unsigned char[]>(iLength);
    try
    {
        pFile->VRead(iLength, 1, sBuffer.get());
    }
    catch (const CRainmanException &e)
    {
        ErrorBoxE(e);
        return;
    }

    // Convert to wxString (fast linear scan)
    wxString sContent(static_cast<size_t>(iLength), wxT('\0'));
    for (long i = 0; i < iLength; ++i)
    {
        sContent[i] = static_cast<wchar_t>(sBuffer[i]);
    }

    if (m_sFilename.Lower().EndsWith(wxT(".burn")))
    {
        sContent.Replace(wxT("\r\r\n"), wxT("\r\n"));
    }

    // Defer text insertion so the tab appears immediately
    CallAfter(
        [this, sContent = std::move(sContent)]()
        {
            m_pSTC->Freeze();
            m_pSTC->AddText(sContent);
            m_pSTC->Thaw();
            m_bNeedsSaving = false;
            m_pSTC->EmptyUndoBuffer();
            m_pSTC->SetSavePoint();

            m_pFunctionDropdown->Clear();
            FillFunctionDrop(wxString());

            if (!m_bLspOpen)
            {
                LspOpenDocument();
            }
            else
            {
                LspSyncDocument();
            }
        });
}

void frmScarEditor::OnStyleNeeded(wxStyledTextEvent &event)
{
    int iEndStyle = m_pSTC->GetEndStyled();
    m_pSTC->Colourise(iEndStyle, event.GetPosition());
}

void frmScarEditor::OnModified(wxStyledTextEvent &event)
{
    event.Skip();
    int mod = event.GetModificationType();
    if (mod & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT))
    {
        m_bLspNeedsSync = true;
    }
}

void frmScarEditor::OnUpdateUI(wxStyledTextEvent &event)
{
    event.Skip();

    // Expand horizontal scroll width to fit the widest visible line + buffer,
    // so users can always scroll a bit past the end of long lines.
    static constexpr int kScrollBuffer = 100;
    int iFirstLine = m_pSTC->GetFirstVisibleLine();
    int iLinesOnScreen = m_pSTC->LinesOnScreen();
    int iXOffset = m_pSTC->GetXOffset();
    int iMaxWidth = 0;
    for (int i = 0; i < iLinesOnScreen; ++i)
    {
        int iDocLine = m_pSTC->DocLineFromVisible(iFirstLine + i);
        if (iDocLine >= m_pSTC->GetLineCount())
        {
            break;
        }
        int iEndPos = m_pSTC->GetLineEndPosition(iDocLine);
        int iX = m_pSTC->PointFromPosition(iEndPos).x + iXOffset;
        if (iX > iMaxWidth)
        {
            iMaxWidth = iX;
        }
    }
    int iDesired = iMaxWidth + kScrollBuffer;
    if (iDesired > m_pSTC->GetScrollWidth())
    {
        m_pSTC->SetScrollWidth(iDesired);
    }
}

const char *lua_tolstring(lua_State *L, int index, size_t *len)
{
    if (len)
    {
        *len = lua_strlen(L, index);
    }
    return lua_tostring(L, index);
}

template <class T> static inline void loadLuaFn(T &oP, wxString sN)
{
    (void)oP;
    (void)sN;
}

void frmScarEditor::OnCheckLua(wxCommandEvent &event)
{
    UNUSED(event);
    lua_State *(*pfn_lua_open)(void) = lua_open;
    int (*pfn_luaL_loadbuffer)(lua_State *, const char *, size_t, const char *) = luaL_loadbuffer;
    const char *(*pfn_lua_tolstring)(lua_State *, int, size_t *) = lua_tolstring;
    void (*pfn_lua_close)(lua_State *) = lua_close;

    if (GetConstruct()->GetModuleService().GetModuleType() != CModuleFile::MT_DawnOfWar)
    {
        pfn_lua_open = (lua_State * (*)(void)) lua51L_newstate;
        pfn_luaL_loadbuffer = lua51L_loadbuffer;
        pfn_lua_tolstring = lua51_tolstring;
        pfn_lua_close = lua51_close;
    }

    auto sLua = wxStringToAscii(m_pSTC->GetText());

    lua_State *L;
    L = pfn_lua_open();
    int iLuaError = pfn_luaL_loadbuffer(L, sLua.get(), strlen(sLua.get()), "");
    if (iLuaError)
    {
        const char *sRawErr = pfn_lua_tolstring(L, -1, 0);
        CScarEditorPresenter::LuaError oError;
        if (CScarEditorPresenter::ParseLuaError(sRawErr, oError))
        {
            wxString sError;
            sError.Printf(AppStr(scar_bad), oError.iLine, oError.sMessage.c_str());
            ThemeColours::ShowMessageBox(sError, AppStr(scar_checklua), wxICON_ERROR, this);
        }
        else
        {
            ThemeColours::ShowMessageBox(wxString::FromUTF8(sRawErr), AppStr(scar_checklua), wxICON_ERROR, this);
        }
    }
    else
    {
        ThemeColours::ShowMessageBox(AppStr(scar_luagood), AppStr(scar_checklua), wxICON_INFORMATION, this);
    }

    pfn_lua_close(L);

    m_pFunctionDropdown->Clear();
    FillFunctionDrop(wxString());
}

void frmScarEditor::OnFuncListChoose(wxCommandEvent &event)
{
    if (event.GetSelection())
    {
        m_pFunctionDropdown->Clear();
        int iPosNew = FillFunctionDrop(event.GetString());
        if (iPosNew != -1)
        {
            m_pSTC->GotoPos(iPosNew);
            m_pSTC->Home();
            m_pSTC->LineEndExtend();
            m_pSTC->SetFocus();
        }
    }
}

int frmScarEditor::FillFunctionDrop(const wxString &sNameTarget)
{
    int iRet = -1;
    m_pFunctionDropdown->Append(AppStr(scar_funcdrop), (void *)0);
    m_pFunctionDropdown->SetSelection(0);

    auto aFunctions = CScarEditorPresenter::ParseFunctionDefinitions(m_pSTC->GetText());

    for (const auto &def : aFunctions)
    {
        if (iRet == -1 && sNameTarget.IsSameAs(def.sName, false))
        {
            iRet = def.iPosition;
        }
        m_pFunctionDropdown->Append(def.sName, reinterpret_cast<void *>(static_cast<intptr_t>(def.iPosition)));
    }
    return iRet;
}

frmScarEditor::frmScarEditor(const wxTreeItemId &oFileParent, wxString sFilename, wxWindow *parent, wxWindowID id,
                             const wxPoint &pos, const wxSize &size, const wchar_t *pLangRef)
    : m_oFileParent(oFileParent), m_sFilename(sFilename), wxWindow(parent, id, pos, size), m_bNeedsSaving(false)
{
    // Load SCAR API reference data
    if (pLangRef)
    {
        const wchar_t *pRefPath = (GetConstruct()->GetModuleService().GetModuleType() == CModuleFile::MT_DawnOfWar)
                                      ? AppStr(app_scarreffile)
                                      : AppStr(app_cohscarreffile);
        m_vScarFunctions = LoadScarReference(pRefPath);
    }

    // Gui
    wxBoxSizer *pTopSizer = new wxBoxSizer(wxVERTICAL);

    wxToolBar *pToolbar;
    pTopSizer->Add(pToolbar = new wxToolBar(this, -1, wxDefaultPosition, wxDefaultSize,
                                            wxTB_NODIVIDER | wxTB_HORIZONTAL | wxNO_BORDER),
                   0, wxEXPAND | wxALL, 3);

    wxArrayString lstFuncDrop;
    pTopSizer->Add(m_pFunctionDropdown =
                       new wxChoice(this, IDC_FunctionDrop, wxDefaultPosition, wxDefaultSize, lstFuncDrop),
                   0, wxEXPAND | wxALL, 3);
    auto oSaveBundle = wxArtProvider::GetBitmapBundle(wxART_FILE_SAVE, wxART_TOOLBAR, wxSize(32, 32));
    auto oCheckBundle = wxBitmapBundle::FromSVG(cdms::icons::kCheck, wxSize(32, 32));
    pToolbar->SetToolBitmapSize(wxSize(32, 32));
    pToolbar->AddTool(IDC_ToolSave, AppStr(scar_save), oSaveBundle, AppStr(scar_save));
    pToolbar->AddTool(IDC_ToolCheck, AppStr(scar_checklua), oCheckBundle, AppStr(scar_checklua));
    pToolbar->Realize();

    m_pSTC = new wxStyledTextCtrl(this, IDC_Text);
    m_pFindBar = new FindBar(this, m_pSTC);
    pTopSizer->Add(m_pFindBar, 0, wxEXPAND);

    pTopSizer->Add(m_pSTC, 1, wxEXPAND | wxALL, 0);
    FillFunctionDrop(wxString());

    // Apply syntax highlighting, autocomplete icons, and indicator styles
    ApplyScarSyntaxConfig(m_pSTC);

    // Bind Ctrl+Space to show autocomplete
    m_pSTC->Bind(wxEVT_KEY_DOWN, &frmScarEditor::OnKeyDown, this);

    // Bind Ctrl+Click for Go to Definition and right-click context menu
    m_pSTC->Bind(wxEVT_LEFT_DOWN, &frmScarEditor::OnMouseDown, this);
    m_pSTC->Bind(wxEVT_CONTEXT_MENU, &frmScarEditor::OnContextMenu, this);

    // Register SCAR API keywords for syntax highlighting (keyword sets 2+)
    // ApplyScarSyntaxConfig sets keyword sets 0-1 (builtins, Lua keywords).
    // Count how many keyword sets were already used.
    int iWordSet = 0;
    for (int i = 0; g_LuaWords[i].type != -1; ++i)
    {
        if (g_LuaWords[i].words)
        {
            ++iWordSet;
        }
    }

    wxString sScarFns, sScarConstants;
    for (const auto &fn : m_vScarFunctions)
    {
        if (fn.iType == 0)
        {
            sScarFns.Append(wxString::FromUTF8(fn.sName));
            sScarFns.Append(' ');
        }
        else
        {
            sScarConstants.Append(wxString::FromUTF8(fn.sName));
            sScarConstants.Append(' ');
        }
    }
    m_pSTC->SetKeyWords(iWordSet, sScarFns);
    m_pSTC->SetKeyWords(++iWordSet, sScarConstants);

    // misc. STC stuff
    m_pSTC->SetMarginWidth(0, m_pSTC->TextWidth(wxSTC_STYLE_LINENUMBER, _T("_999999")));
    m_pSTC->SetScrollWidth(1);

    // Buttons
    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxWindow *pBgTemp;

    pButtonSizer->Add(pBgTemp = new wxButton(this, IDC_Compile, AppStr(scar_checklua)), 0, wxEXPAND | wxALL, 3);
    pButtonSizer->Add(new wxButton(this, wxID_SAVE, AppStr(scar_save)), 0, wxEXPAND | wxALL, 3);

    pTopSizer->Add(pButtonSizer, 0, wxALIGN_RIGHT);

    SetBackgroundColour(pBgTemp->GetBackgroundColour());

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);

    // Start LSP polling timer (document will be opened after Load() populates text)
    m_lspTimer.SetOwner(this);
    m_lspTimer.Start(100); // Poll every 100ms
}

frmScarEditor::~frmScarEditor()
{
    m_lspTimer.Stop();
    LspCloseDocument();
}

// ---------------------------------------------------------------------------
// LSP integration
// ---------------------------------------------------------------------------

void frmScarEditor::LspOpenDocument()
{
    auto *pFrame = GetConstruct();
    if (!pFrame)
    {
        return;
    }
    auto *pClient = pFrame->GetLspClient();
    if (!pClient)
    {
        CDMS_LOG_DEBUG("LSP: No language server available for editor");
        return;
    }

    // If the server hasn't finished initializing, defer the open to the next timer tick
    if (!pClient->IsReady())
    {
        m_bLspNeedsOpen = true;
        CDMS_LOG_DEBUG("LSP: Server not ready yet, deferring document open");
        return;
    }

    m_bLspNeedsOpen = false;

    // Build a file:// URI from the filename
    m_sLspUri = "file:///" + std::string(m_sFilename.ToUTF8());
    // Normalize backslashes to forward slashes for URI
    for (char &c : m_sLspUri)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    m_iLspVersion = 1;
    std::string text(m_pSTC->GetText().ToUTF8());

    // Translate --? SCAR annotations to ---@ LuaLS annotations before sending
    m_lspTranslation = lsp::CScarAnnotationTranslator::Translate(text);
    // Register callback before opening so diagnostics are never missed
    pClient->RegisterDiagnosticsCallback(m_sLspUri, [this](const std::string &uri, std::vector<lsp::Diagnostic> diags)
                                         { ApplyDiagnostics(uri, std::move(diags)); });

    pClient->OpenDocument(m_sLspUri, "lua", m_lspTranslation->text);
    m_bLspOpen = true;
    CDMS_LOG_INFO("LSP: Opened document {} ({} bytes, version {})", m_sLspUri, m_lspTranslation->text.size(),
                  m_iLspVersion);
}

void frmScarEditor::LspSyncDocument()
{
    if (!m_bLspOpen)
    {
        return;
    }

    auto *pFrame = GetConstruct();
    if (!pFrame)
    {
        return;
    }
    auto *pClient = pFrame->GetLspClient();
    if (!pClient)
    {
        return;
    }

    ++m_iLspVersion;
    std::string text(m_pSTC->GetText().ToUTF8());

    // Translate --? SCAR annotations to ---@ LuaLS annotations before sending
    m_lspTranslation = lsp::CScarAnnotationTranslator::Translate(text);
    pClient->ChangeDocument(m_sLspUri, m_lspTranslation->text);
}

void frmScarEditor::LspCloseDocument()
{
    if (!m_bLspOpen)
    {
        return;
    }

    m_bLspOpen = false;

    // During app shutdown the ConstructFrame may already be partially
    // destroyed when child windows are torn down by the base-class
    // destructor.  Calling GetConstruct() at that point is UB.
    // IsBeingDeleted() on the top-level window detects this case.
    auto *pTopWin = wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
    if (!pTopWin || pTopWin->IsBeingDeleted())
    {
        return;
    }

    auto *pFrame = GetConstruct();
    if (!pFrame)
    {
        return;
    }
    auto *pClient = pFrame->GetLspClient();
    if (pClient)
    {
        pClient->UnregisterDiagnosticsCallback(m_sLspUri);
        pClient->CloseDocument(m_sLspUri);
    }
}

void frmScarEditor::OnLspTimer(wxTimerEvent &event)
{
    UNUSED(event);
    auto *pFrame = GetConstruct();
    if (!pFrame)
    {
        return;
    }
    auto *pClient = pFrame->GetLspClient();
    if (!pClient)
    {
        // Client was shut down — reset editor state so the document will be
        // re-opened when a new client becomes available.
        if (m_bLspOpen)
        {
            m_bLspOpen = false;
            m_bLspNeedsOpen = true;
        }
        return;
    }

    // Retry deferred document open if the server is now ready.
    // Use else-if so open and sync are never in the same tick — gives
    // the server time to process didOpen before receiving didChange.
    if (m_bLspNeedsOpen && !m_bLspOpen)
    {
        LspOpenDocument();
    }
    else if (m_bLspNeedsSync && m_bLspOpen)
    {
        CDMS_LOG_INFO("LSP: Timer tick syncing document (version {})", m_iLspVersion + 1);
        m_bLspNeedsSync = false;
        LspSyncDocument();
    }

    pClient->Poll();
}

void frmScarEditor::ApplyDiagnostics(const std::string &uri, std::vector<lsp::Diagnostic> diagnostics)
{
    if (uri != m_sLspUri)
    {
        return;
    }

    CDMS_LOG_INFO("LSP: Applying {} diagnostics for {}", diagnostics.size(), uri);

    // Store diagnostics for hover tooltip lookup
    m_vDiagnostics = diagnostics;

    // Remap diagnostic positions from translated (LSP) coordinates to original (editor) coordinates
    if (m_lspTranslation)
    {
        for (auto &diag : m_vDiagnostics)
        {
            diag.range.start = lsp::CScarAnnotationTranslator::MapToOriginal(*m_lspTranslation, diag.range.start);
            diag.range.end = lsp::CScarAnnotationTranslator::MapToOriginal(*m_lspTranslation, diag.range.end);
        }
    }

    // Clear existing diagnostic indicators
    m_pSTC->SetIndicatorCurrent(INDICATOR_LSP_ERROR);
    m_pSTC->IndicatorClearRange(0, m_pSTC->GetLength());
    m_pSTC->SetIndicatorCurrent(INDICATOR_LSP_WARNING);
    m_pSTC->IndicatorClearRange(0, m_pSTC->GetLength());
    m_pSTC->SetIndicatorCurrent(INDICATOR_LSP_INFO);
    m_pSTC->IndicatorClearRange(0, m_pSTC->GetLength());

    for (const auto &diag : m_vDiagnostics)
    {
        int startPos = m_pSTC->PositionFromLine(diag.range.start.line) + diag.range.start.character;
        int endPos = m_pSTC->PositionFromLine(diag.range.end.line) + diag.range.end.character;
        int length = endPos - startPos;
        if (length <= 0)
        {
            length = 1;
        }

        int indicator = INDICATOR_LSP_INFO;
        switch (diag.severity)
        {
        case lsp::DiagnosticSeverity::Error:
            indicator = INDICATOR_LSP_ERROR;
            break;
        case lsp::DiagnosticSeverity::Warning:
            indicator = INDICATOR_LSP_WARNING;
            break;
        default:
            indicator = INDICATOR_LSP_INFO;
            break;
        }

        m_pSTC->SetIndicatorCurrent(indicator);
        m_pSTC->IndicatorFillRange(startPos, length);
    }

    // Force repaint so indicators are visible immediately — wxStyledTextCtrl
    // may not invalidate the affected region automatically when indicators
    // are set outside of a user-initiated edit.
    if (!m_vDiagnostics.empty())
    {
        m_pSTC->Refresh(false);
    }
}

void frmScarEditor::OnDwellStart(wxStyledTextEvent &event)
{
    int pos = event.GetPosition();
    if (pos < 0)
    {
        return;
    }

    // Don't overwrite an active calltip from function signatures
    if (m_pSTC->CallTipActive())
    {
        return;
    }

    // Collect all diagnostics that cover the hovered position
    std::vector<DiagnosticEntry> matchedDiags;
    for (const auto &diag : m_vDiagnostics)
    {
        int startPos = m_pSTC->PositionFromLine(diag.range.start.line) + diag.range.start.character;
        int endPos = m_pSTC->PositionFromLine(diag.range.end.line) + diag.range.end.character;
        if (endPos <= startPos)
        {
            endPos = startPos + 1;
        }

        if (pos >= startPos && pos < endPos)
        {
            std::string prefix;
            switch (diag.severity)
            {
            case lsp::DiagnosticSeverity::Error:
                prefix = "Error: ";
                break;
            case lsp::DiagnosticSeverity::Warning:
                prefix = "Warning: ";
                break;
            case lsp::DiagnosticSeverity::Information:
                prefix = "Info: ";
                break;
            case lsp::DiagnosticSeverity::Hint:
                prefix = "Hint: ";
                break;
            }
            std::string text = prefix + diag.message;
            if (!diag.source.empty())
            {
                text += " [" + diag.source + "]";
            }
            matchedDiags.push_back({diag.severity, std::move(text)});
        }
    }

    // Request hover info from LSP (diagnostics will be merged into the popup)
    auto *pFrame = GetConstruct();
    auto *pClient = pFrame ? pFrame->GetLspClient() : nullptr;
    if (pClient && m_bLspOpen)
    {
        int line = m_pSTC->LineFromPosition(pos);
        int col = pos - m_pSTC->PositionFromLine(line);
        // Remap cursor position to translated coordinates for LSP
        lsp::Position lspPos{line, col};
        if (m_lspTranslation)
        {
            lspPos = lsp::CScarAnnotationTranslator::MapToTranslated(*m_lspTranslation, lspPos);
        }
        int hoverPos = pos;

        pClient->RequestHover(m_sLspUri, lspPos.line, lspPos.character,
                              [this, hoverPos, diags = std::move(matchedDiags)](std::optional<lsp::HoverResult> hover)
                              {
                                  HoverParts parts;
                                  if (hover && !hover->contents.empty())
                                  {
                                      parts = ParseHoverMarkdown(hover->contents);
                                  }
                                  parts.diagnostics = std::move(diags);

                                  if (parts.code.empty() && parts.description.empty() && parts.diagnostics.empty())
                                  {
                                      return;
                                  }
                                  // Don't show if a calltip is already active
                                  if (m_pSTC->CallTipActive())
                                  {
                                      return;
                                  }
                                  DismissHoverPopup();

                                  wxPoint ptEditor = m_pSTC->PointFromPosition(hoverPos);
                                  int lineHeight = m_pSTC->TextHeight(m_pSTC->LineFromPosition(hoverPos));
                                  wxPoint ptScreen = m_pSTC->ClientToScreen(ptEditor);
                                  ptScreen.y += lineHeight + 4;

                                  m_pHoverPopup = new CHoverPopup(this, parts, ptScreen, lineHeight);
                                  m_pHoverPopup->Popup();
                              });
    }
    else if (!matchedDiags.empty())
    {
        // LSP unavailable — show diagnostics-only popup
        DismissHoverPopup();
        HoverParts parts;
        parts.diagnostics = std::move(matchedDiags);

        wxPoint ptEditor = m_pSTC->PointFromPosition(pos);
        int lineHeight = m_pSTC->TextHeight(m_pSTC->LineFromPosition(pos));
        wxPoint ptScreen = m_pSTC->ClientToScreen(ptEditor);
        ptScreen.y += lineHeight + 4;

        m_pHoverPopup = new CHoverPopup(this, parts, ptScreen, lineHeight);
        m_pHoverPopup->Popup();
    }
}

void frmScarEditor::OnDwellEnd(wxStyledTextEvent &event)
{
    UNUSED(event);
    // Only cancel CallTip if it's not a function signature calltip
    if (m_pSTC->CallTipActive() && m_oThisCalltip.sTip.IsEmpty())
    {
        m_pSTC->CallTipCancel();
    }
    DismissHoverPopup();
}

void frmScarEditor::DismissHoverPopup()
{
    if (m_pHoverPopup)
    {
        m_pHoverPopup->Dismiss();
        m_pHoverPopup = nullptr;
    }
}

void frmScarEditor::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

void frmScarEditor::ShowFindBar() { m_pFindBar->Activate(); }

void frmScarEditor::HideFindBar() { m_pFindBar->Deactivate(); }

// ---------------------------------------------------------------------------
// Go to Definition
// ---------------------------------------------------------------------------

void frmScarEditor::OnMouseDown(wxMouseEvent &event)
{
    if (event.ControlDown() && !event.AltDown() && !event.ShiftDown())
    {
        // Ctrl+Click: position the caret first, then trigger Go to Definition
        int pos = m_pSTC->PositionFromPoint(event.GetPosition());
        if (pos != wxSTC_INVALID_POSITION)
        {
            m_pSTC->SetCurrentPos(pos);
            m_pSTC->SetAnchor(pos);
            GoToDefinition();
            return;
        }
    }
    event.Skip();
}

void frmScarEditor::OnContextMenu(wxContextMenuEvent &event)
{
    UNUSED(event);
    DismissHoverPopup();
    wxMenu menu;
    menu.Append(IDC_GoToDefinition, wxT("Go to Definition\tCtrl+Click"));
    menu.AppendSeparator();
    menu.Append(wxID_CUT, wxT("Cut"));
    menu.Append(wxID_COPY, wxT("Copy"));
    menu.Append(wxID_PASTE, wxT("Paste"));
    menu.Append(wxID_SELECTALL, wxT("Select All"));
    PopupMenu(&menu);
}

void frmScarEditor::OnContextGoToDef(wxCommandEvent &event)
{
    UNUSED(event);
    GoToDefinition();
}

void frmScarEditor::GoToDefinition()
{
    auto *pFrame = GetConstruct();
    auto *pClient = pFrame ? pFrame->GetLspClient() : nullptr;
    if (!pClient || !m_bLspOpen)
    {
        return;
    }

    int pos = m_pSTC->GetCurrentPos();
    int line = m_pSTC->LineFromPosition(pos);
    int col = pos - m_pSTC->PositionFromLine(line);

    // Remap cursor position to translated coordinates for LSP
    lsp::Position lspPos{line, col};
    if (m_lspTranslation)
    {
        lspPos = lsp::CScarAnnotationTranslator::MapToTranslated(*m_lspTranslation, lspPos);
    }

    // Decode percent-encoded characters in a file URI (e.g. %3A → ':')
    auto decodeUri = [](const std::string &uri) -> std::string
    {
        std::string result;
        result.reserve(uri.size());
        for (size_t i = 0; i < uri.size(); ++i)
        {
            if (uri[i] == '%' && i + 2 < uri.size())
            {
                unsigned int ch = 0;
                if (sscanf(uri.c_str() + i + 1, "%2x", &ch) == 1)
                {
                    result += static_cast<char>(ch);
                    i += 2;
                    continue;
                }
            }
            result += uri[i];
        }
        return result;
    };

    pClient->RequestDefinition(m_sLspUri, lspPos.line, lspPos.character,
                               [this, decodeUri](std::optional<lsp::Location> location)
                               {
                                   if (!location)
                                   {
                                       // No definition found — show a brief status message
                                       TheConstruct->SetStatusText(wxT("No definition found"));
                                       return;
                                   }

                                   // Convert URI to local file path
                                   std::string targetUri = location->uri;
                                   wxString sTargetPath;
                                   if (targetUri.find("file:///") == 0)
                                   {
                                       sTargetPath = wxString::FromUTF8(decodeUri(targetUri.substr(8)));
                                       sTargetPath.Replace(wxT("/"), wxT("\\"));
                                   }
                                   else
                                   {
                                       sTargetPath = wxString::FromUTF8(decodeUri(targetUri));
                                   }

                                   int targetLine = location->range.start.line;

                                   // Remap from translated to original coordinates
                                   if (targetUri == m_sLspUri && m_lspTranslation)
                                   {
                                       lsp::Position origPos = lsp::CScarAnnotationTranslator::MapToOriginal(
                                           *m_lspTranslation, location->range.start);
                                       targetLine = origPos.line;
                                   }

                                   // Check if it's the same file
                                   if (targetUri == m_sLspUri)
                                   {
                                       int targetPos = m_pSTC->PositionFromLine(targetLine);
                                       m_pSTC->GotoPos(targetPos);
                                       m_pSTC->Home();
                                       m_pSTC->LineEndExtend();
                                       m_pSTC->SetFocus();
                                       return;
                                   }

                                   // Open the target file in a new read-only tab
                                   OpenDefinitionFile(sTargetPath, targetLine);
                               });
}

void frmScarEditor::LoadFromDiskReadOnly(const wxString &sPath, int iGotoLine)
{
    std::ifstream file(std::filesystem::path(sPath.ToStdWstring()), std::ios::binary);
    if (!file.is_open())
    {
        ThemeColours::ShowMessageBox(wxT("Cannot open file: ") + sPath, wxT("Go to Definition"), wxICON_ERROR, this);
        return;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    m_pSTC->AddText(wxString::FromUTF8(content));
    m_pSTC->EmptyUndoBuffer();
    m_pSTC->SetSavePoint();
    m_pSTC->SetReadOnly(true);
    m_bReadOnly = true;
    m_bNeedsSaving = false;

    m_pFunctionDropdown->Clear();
    FillFunctionDrop(wxString());

    if (iGotoLine >= 0)
    {
        int targetPos = m_pSTC->PositionFromLine(iGotoLine);
        m_pSTC->GotoPos(targetPos);
        m_pSTC->Home();
        m_pSTC->LineEndExtend();
        m_pSTC->EnsureVisibleEnforcePolicy(iGotoLine);
        m_pSTC->SetFocus();
    }
}

/*static*/ void frmScarEditor::OpenDefinitionFile(const wxString &sPath, int iLine)
{
    if (!wxFileExists(sPath))
    {
        TheConstruct->SetStatusText(wxT("Definition file not found: ") + sPath);
        return;
    }

    wxString sTabLabel = wxString(wxT("Def [")).Append(wxFileName(sPath).GetFullName()).Append(wxT("] [Read-Only]"));

    auto *pForm = new frmScarEditor(wxTreeItemId(), sPath, TheConstruct->GetTabs(), -1, wxDefaultPosition,
                                    wxDefaultSize, nullptr);
    TheConstruct->GetTabs()->AddPage(pForm, sTabLabel, true);
    pForm->LoadFromDiskReadOnly(sPath, iLine);
}
