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

#include "frmModule.h"
#include "frame/Construct.h"
#include "common/strconv.h"
#include "CtrlStatusText.h"
#include "common/strings.h"
#include "res/Icons.h"
#include <wx/artprov.h>
#include <wx/notebook.h>
#include "services/FileService.h"
#include "presenters/CModuleSettingsPresenter.h"
#include "common/Common.h"
#include <wx/filename.h>

BEGIN_EVENT_TABLE(frmModule, wxWindow)
EVT_SIZE(frmModule::OnSize)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(frmModule::pgMain, wxWindow)
EVT_SIZE(frmModule::pgMain::OnSize)
EVT_TEXT(IDC_Description, frmModule::pgMain::OnDescriptionUpdate)
EVT_TEXT(IDC_UIName, frmModule::pgMain::OnUINameUpdate)
EVT_TEXT(IDC_TextureFE, frmModule::pgMain::OnTextureFEUpdate)
EVT_TEXT(IDC_TextureIcon, frmModule::pgMain::OnTextureIconUpdate)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(frmModule::pgDataFolders, wxWindow)
EVT_SIZE(frmModule::pgDataFolders::OnSize)
END_EVENT_TABLE()

frmModule::frmModule(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : wxWindow(parent, id, pos, size)
{
    SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    auto *pTopSizer = new wxBoxSizer(wxVERTICAL);
    wxNotebook *pTabs;

    pTopSizer->Add(pTabs = new wxNotebook(this, -1, wxPoint(0, 0)), 1, wxEXPAND | wxALL, 3);
    pTabs->AddPage(new pgMain(pTabs, IDP_General), wxT("General"));
    pTabs->AddPage(new pgDataFolders(pTabs, IDP_DataFolders), wxT("Data Folders"));
    pTabs->AddPage(new pgDataArchives(pTabs, IDP_DataArchives), wxT("Data Archives"));
    pTabs->AddPage(new pgRequiredMods(pTabs, IDP_RequiredMods), wxT("Required Mods"));
    pTabs->AddPage(new pgCompatibleMods(pTabs, IDP_CompatibleMods), wxT("Compatible Mods"));

    SetBackgroundColour(pTabs->GetBackgroundColour());
    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
}

void frmModule::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

frmModule::pgMain::pgMain(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : wxWindow(parent, id, pos, size)
{
    m_bDoneInit = false;
    SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    auto *pTopSizer = new wxFlexGridSizer(2);
    pTopSizer->SetFlexibleDirection(wxHORIZONTAL);
    pTopSizer->AddGrowableCol(1, 1);
    wxStaticText *pBgTemp;

    wxArrayString aEmptyList;

    auto &modSvc = TheConstruct->GetModuleService();
    wxControlWithItems *pModFolders;

    pTopSizer->Add(SBT(pBgTemp = new wxStaticText(this, -1, AppStr(mod_description)), AppStr(mod_description_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(new wxTextCtrl(this, IDC_Description, modSvc.GetDescription()), AppStr(mod_description_help)), 0,
                   wxALL | wxEXPAND, 3);
    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_dll)), AppStr(mod_dll_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(
        SBT(pModFolders = new wxChoice(this, IDC_DllName, wxDefaultPosition, wxDefaultSize, aEmptyList, wxCB_SORT),
            AppStr(mod_dll_help)),
        0, wxALL | wxEXPAND, 3);
    InitDllList(pModFolders);
    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_folder)), AppStr(mod_folder_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(pModFolders = new wxComboBox(this, IDC_ModFolder, wxT(""), wxDefaultPosition, wxDefaultSize,
                                                    aEmptyList, wxCB_SORT),
                       AppStr(mod_folder_help)),
                   0, wxALL | wxEXPAND, 3);
    InitModFolderList(pModFolders);
    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_texturefe)), AppStr(mod_texturefe_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(new wxTextCtrl(this, IDC_TextureFE, modSvc.GetTextureFe()), AppStr(mod_texturefe_help)), 0,
                   wxALL | wxEXPAND, 3);
    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_textureicon)), AppStr(mod_textureicon_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(new wxTextCtrl(this, IDC_TextureIcon, modSvc.GetTextureIcon()), AppStr(mod_textureicon_help)), 0,
                   wxALL | wxEXPAND, 3);
    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_uiname)), AppStr(mod_uiname_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(SBT(new wxTextCtrl(this, IDC_UIName, modSvc.GetUiName()), AppStr(mod_uiname_help)), 0,
                   wxALL | wxEXPAND, 3);

    auto *pVersionSizer = new wxBoxSizer(wxHORIZONTAL);
    wchar_t sNumBuffer[34];
    _ltow(modSvc.GetVersionMajor(), sNumBuffer, 10);
    pVersionSizer->Add(SBT(new wxTextCtrl(this, IDC_VersionMajor, sNumBuffer), AppStr(mod_version_help)), 1,
                       wxALL | wxEXPAND, 3);
    pVersionSizer->Add(SBT(new wxStaticText(this, -1, AppStr(decimal_point)), AppStr(mod_version_help)), 0,
                       wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    _ltow(modSvc.GetVersionMinor(), sNumBuffer, 10);
    pVersionSizer->Add(SBT(new wxTextCtrl(this, IDC_VersionMinor, sNumBuffer), AppStr(mod_version_help)), 1,
                       wxALL | wxEXPAND, 3);
    pVersionSizer->Add(SBT(new wxStaticText(this, -1, AppStr(decimal_point)), AppStr(mod_version_help)), 0,
                       wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    _ltow(modSvc.GetVersionRevision(), sNumBuffer, 10);
    pVersionSizer->Add(SBT(new wxTextCtrl(this, IDC_VersionRevision, sNumBuffer), AppStr(mod_version_help)), 1,
                       wxALL | wxEXPAND, 3);
    pVersionSizer->Add(
        new wxButton(this, IDC_VersionHelp, AppStr(question_mark), wxDefaultPosition,
                     wxSize(FromDIP(24), wxDefaultCoord)), 0,
        wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);

    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(mod_version)), AppStr(mod_version_help)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(pVersionSizer, 0, wxALL | wxEXPAND, 0);

    m_bDoneInit = true;
    SetBackgroundColour(pBgTemp->GetBackgroundColour());
    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
}

void frmModule::pgMain::OnDescriptionUpdate(wxCommandEvent &event)
{
    if (m_bDoneInit)
    {
        TheConstruct->GetModuleService().SetDescription(event.GetString());
    }
}

void frmModule::pgMain::OnUINameUpdate(wxCommandEvent &event)
{
    if (m_bDoneInit)
    {
        TheConstruct->GetModuleService().SetUiName(event.GetString());
    }
}

void frmModule::pgMain::OnTextureFEUpdate(wxCommandEvent &event)
{
    if (m_bDoneInit)
    {
        TheConstruct->GetModuleService().SetTextureFe(event.GetString());
    }
}

void frmModule::pgMain::OnTextureIconUpdate(wxCommandEvent &event)
{
    if (m_bDoneInit)
    {
        TheConstruct->GetModuleService().SetTextureIcon(event.GetString());
    }
}

void frmModule::pgMain::InitModFolderList(wxControlWithItems *pList)
{
    wxString sDoWFolder = wxFileName(TheConstruct->GetModuleFile()).GetPath();
    auto iterResult = FileService::IterateFileSystem(sDoWFolder);
    if (!iterResult)
    {
        return;
    }
    IDirectoryTraverser::IIterator *pItr = iterResult.value().release();
    IDirectoryTraverser::IIterator::eTypes eType;
    eType = pItr->VGetType();

    wxArrayString aAllDirs;
    while (eType == IDirectoryTraverser::IIterator::T_Directory || eType == IDirectoryTraverser::IIterator::T_File)
    {
        if (eType == IDirectoryTraverser::IIterator::T_Directory)
        {
            aAllDirs.Add(AsciiTowxString(pItr->VGetName()));
        }
        if (pItr->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
        {
            break;
        }
        eType = pItr->VGetType();
    }
    delete pItr;

    pList->Append(CModuleSettingsPresenter::FilterModFolders(aAllDirs));
    pList->SetStringSelection(TheConstruct->GetModuleService().GetModFolder());
}

void frmModule::pgMain::InitDllList(wxControlWithItems *pList)
{
    wxString sTheDll = TheConstruct->GetModuleService().GetDllName();
    wxString sDoWFolder = wxFileName(TheConstruct->GetModuleFile()).GetPath();
    auto iterResult = FileService::IterateFileSystem(sDoWFolder);
    if (!iterResult)
    {
        return;
    }
    IDirectoryTraverser::IIterator *pItr = iterResult.value().release();
    IDirectoryTraverser::IIterator::eTypes eType;
    eType = pItr->VGetType();

    wxArrayString aAllDlls;
    while (eType == IDirectoryTraverser::IIterator::T_Directory || eType == IDirectoryTraverser::IIterator::T_File)
    {
        if (eType == IDirectoryTraverser::IIterator::T_File)
        {
            wxString sThisFile = AsciiTowxString(pItr->VGetName());
            if (sThisFile.AfterLast('.') == wxT("dll"))
            {
                aAllDlls.Add(sThisFile.BeforeLast('.'));
            }
        }
        if (pItr->VNextItem() != IDirectoryTraverser::IIterator::E_OK)
        {
            break;
        }
        eType = pItr->VGetType();
    }
    delete pItr;

    pList->Append(CModuleSettingsPresenter::FilterDllFiles(aAllDlls, sTheDll));
    pList->SetStringSelection(sTheDll);
}

void frmModule::pgMain::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

frmModule::pgDataFolders::pgDataFolders(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size,
                                        const wxString &sTitle, const wxString &sItemName, bool bUpdateMessage,
                                        void (*pInitList)(wxArrayString &))
    : wxWindow(parent, id, pos, size)
{
    SetBackgroundStyle(wxBG_STYLE_SYSTEM);
    auto *pTopSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText *pBgTemp;

    wxArrayString aActiveValues;
    pInitList(aActiveValues);

    pTopSizer->Add(pBgTemp = new wxStaticText(this, -1, sTitle), 0, wxALIGN_LEFT | wxALL, 3);
    pTopSizer->Add(new wxListBox(this, -1, wxDefaultPosition, wxDefaultSize, aActiveValues), 1, wxALL | wxEXPAND, 3);

    auto *pButtons = new wxBoxSizer(wxHORIZONTAL);
    const wxSize kBtnIconSize(16, 16);
    pButtons->Add(SBT(new wxBitmapButton(this, -1, wxArtProvider::GetBitmap(wxART_PLUS, wxART_BUTTON, kBtnIconSize)),
                      AppStrS(mod_add_a_new, sItemName)),
                  0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    pButtons->Add(SBT(new wxBitmapButton(this, -1, wxArtProvider::GetBitmap(wxART_MINUS, wxART_BUTTON, kBtnIconSize)),
                      AppStrS(mod_remove_an_existing, sItemName)),
                  0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    pButtons->Add(SBT(new wxBitmapButton(this, -1, cdms::icons::SvgToBitmap(cdms::icons::kChevronUp, kBtnIconSize)),
                      AppStrS(mod_move_up, sItemName)),
                  0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    pButtons->Add(SBT(new wxBitmapButton(this, -1, cdms::icons::SvgToBitmap(cdms::icons::kChevronDown, kBtnIconSize)),
                      AppStrS(mod_move_down, sItemName)),
                  0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    if (bUpdateMessage)
    {
        pButtons->Add(new wxStaticText(this, -1, AppStrS(mod_changes_to, sItemName), wxDefaultPosition, wxDefaultSize,
                                       wxALIGN_CENTRE),
                      0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    }

    pTopSizer->Add(pButtons, 0, wxALL | wxALIGN_LEFT, 0);

    SetBackgroundColour(pBgTemp->GetBackgroundColour());
    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
}

void frmModule::pgDataFolders::OnSize(wxSizeEvent &event)
{
    UNUSED(event);
    Layout();
}

void frmModule::pgDataFolders::FillInitialValues(wxArrayString &aInitialValues)
{
    aInitialValues = CModuleSettingsPresenter::GetFolderEntries(TheConstruct->GetModule());
}

frmModule::pgDataArchives::pgDataArchives(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : pgDataFolders(parent, id, pos, size, AppStr(mod_dataarchives_caption), AppStr(mod_dataarchive), true,
                    FillInitialValues)
{
}

void frmModule::pgDataArchives::FillInitialValues(wxArrayString &aInitialValues)
{
    aInitialValues = CModuleSettingsPresenter::GetArchiveEntries(TheConstruct->GetModule());
}

frmModule::pgRequiredMods::pgRequiredMods(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : pgDataFolders(parent, id, pos, size, AppStr(mod_requiredmods_caption), AppStr(mod_requiredmod), true,
                    FillInitialValues)
{
}

void frmModule::pgRequiredMods::FillInitialValues(wxArrayString &aInitialValues)
{
    aInitialValues = CModuleSettingsPresenter::GetRequiredEntries(TheConstruct->GetModule());
}

frmModule::pgCompatibleMods::pgCompatibleMods(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
    : pgDataFolders(parent, id, pos, size, AppStr(mod_compatiblemods_caption), AppStr(mod_compatiblemod), false,
                    FillInitialValues)
{
}

void frmModule::pgCompatibleMods::FillInitialValues(wxArrayString &aInitialValues)
{
    aInitialValues = CModuleSettingsPresenter::GetCompatibleEntries(TheConstruct->GetModule());
}
