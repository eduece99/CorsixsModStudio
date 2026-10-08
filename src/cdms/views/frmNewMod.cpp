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

#include "common/Common.h"
#include "frmNewMod.h"
#include "common/strings.h"
#include "common/strconv.h"
#include "common/config.h"
#include "services/CDEModGenerator.h"
#include "frame/Construct.h"
#include "CtrlStatusText.h"
#include <errno.h>
#include <filesystem>
#include <stdexcept>
#include <wx/textdlg.h>
#include "common/ThemeColours.h"
#include "rainman/core/RainmanLog.h"

BEGIN_EVENT_TABLE(frmNewMod, wxDialog)
EVT_BUTTON(IDC_New, frmNewMod::OnNewClick)
EVT_BUTTON(IDC_Cancel, frmNewMod::OnCancelClick)
EVT_BUTTON(IDC_Browse, frmNewMod::OnBrowseClick)
EVT_CHOICE(IDC_Game, frmNewMod::OnGameChange)
END_EVENT_TABLE()

wxString frmNewMod::_UpdatePath(wxString sName)
{
    wxString sVal = wxT("");
    size_t iL = sName.Len();
    for (size_t i = 0; i < iL; ++i)
    {
        if ((sName[i] >= 'a' && sName[i] <= 'z') || (sName[i] >= 'A' && sName[i] <= 'Z'))
        {
            sVal.Append(sName[i]);
        }
        else if (sName[i] == ' ')
        {
            sVal.Append('_');
        }
    }
    return sVal;
}

const int g_kDefinitiveEdition = 0;
const int g_kCompanyOfHeroes = 1;
const int g_kDawnOfWar = 2;
const int g_kWinterAssault = 3;
const int g_kDarkCrusade = 4;
const int g_kSoulstorm = 5;

frmNewMod::frmNewMod()
    : wxDialog(wxTheApp->GetTopWindow(), -1, AppStr(new_mod), wxPoint(0, 0), wxDefaultSize,
               wxFRAME_FLOAT_ON_PARENT | wxFRAME_TOOL_WINDOW | wxCAPTION)
{
    m_pCreation = nullptr;
    try
    {
        m_sDoWPath = ConfGetDoWFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW folder");
    }
    try
    {
        m_sCoHPath = ConfGetCoHFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get CoH folder");
    }

    try
    {
        m_sDCPath = ConfGetDCFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DC folder");
    }

    try
    {
        m_sSSPath = ConfGetSSFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get SS folder");
    }

    try
    {
        m_sDEGamePath = ConfGetDEFolder();
        m_sDEPath = ConfGetDEModsFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DE folder");
    }

    CentreOnParent();
    wxFlexGridSizer *pTopSizer = new wxFlexGridSizer(2);
    pTopSizer->SetFlexibleDirection(wxHORIZONTAL);
    pTopSizer->AddGrowableCol(1, 1);

    wxArrayString aBases;
    aBases.Add(wxT("Dawn of War: Definitive Edition"));
    aBases.Add(wxT("Company of Heroes / Opposing Fronts"));
    aBases.Add(wxT("Dawn of War"));
    aBases.Add(wxT("Dawn of War: Winter Assault"));
    aBases.Add(wxT("Dawn of War: Dark Crusade"));
    aBases.Add(wxT("Dawn of War: Soulstorm"));

    wxWindow *pBgTemp;

    pTopSizer->Add(SBT(pBgTemp = new wxStaticText(this, -1, AppStr(newmod_name)), AppStr(newmod_namehelp)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(
        SBT(m_pName = new wxTextCtrl(this, IDC_Name, wxT("MyMod"), wxDefaultPosition,
                                     wxSize(FromDIP(300), wxDefaultCoord)),
            AppStr(newmod_namehelp)),
        1, wxALL | wxEXPAND, 3);
    m_pName->SetToolTip(wxT("DE identifier: 1-64 ASCII letters, digits or underscores; no spaces."));

    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(newmod_base)), AppStr(newmod_basehelp)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(
        SBT(m_pList = new wxChoice(this, IDC_Game, wxDefaultPosition, wxDefaultSize, aBases), AppStr(newmod_basehelp)),
        1, wxALL | wxEXPAND, 3);

    m_pDisplayLabel = new wxStaticText(this, -1, wxT("Display name"));
    pTopSizer->Add(m_pDisplayLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    m_pDisplayName = new wxTextCtrl(this, -1, wxT("My Mod"));
    pTopSizer->Add(m_pDisplayName, 1, wxEXPAND | wxALL, 3);

    m_pDescriptionLabel = new wxStaticText(this, -1, wxT("Description"));
    pTopSizer->Add(m_pDescriptionLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    m_pDescription = new wxTextCtrl(this, -1, wxT(""));
    pTopSizer->Add(m_pDescription, 1, wxEXPAND | wxALL, 3);

    wxArrayString parents;
    parents.Add(wxT("DoWDE"));
    parents.Add(wxT("DXP3"));
    parents.Add(wxT("DXP2"));
    parents.Add(wxT("WXP"));
    parents.Add(wxT("W40k"));
    m_pParentLabel = new wxStaticText(this, -1, wxT("Parent MOD"));
    pTopSizer->Add(m_pParentLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    m_pParent = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, parents);
    pTopSizer->Add(m_pParent, 1, wxEXPAND | wxALL, 3);
    const auto gameDir = std::filesystem::path(m_sDEGamePath.ToStdWstring());
    m_pParent->SetSelection(std::filesystem::is_regular_file(gameDir / "DoWDE.module") ? 0 : 1);

    pTopSizer->Add(SBT(new wxStaticText(this, -1, AppStr(newmod_destination)), AppStr(newmod_destinationhelp)), 0,
                   wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE | wxALL, 3);

    wxBoxSizer *pDestSizer = new wxBoxSizer(wxHORIZONTAL);

    pDestSizer->Add(
        SBT(m_pCreation = new wxStaticText(this, -1, m_sDEPath, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE),
            AppStr(newmod_destinationhelp)),
        1, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, 3);
    pDestSizer->Add(
        SBT(m_pBrowse = new wxButton(this, IDC_Browse, AppStr(sgapack_browse)), AppStr(sgapack_dirselect_label_help)),
        0, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT | wxFIXED_MINSIZE | wxALL, 3);
    pTopSizer->Add(pDestSizer, 1, wxEXPAND);

    m_pList->SetSelection(g_kDefinitiveEdition);
    m_pBrowse->Disable();

    wxBoxSizer *pButtonSizer = new wxBoxSizer(wxHORIZONTAL);

    pTopSizer->AddSpacer(0);
    pButtonSizer->Add(new wxButton(this, IDC_Cancel, AppStr(newmod_cancel)), 0, wxEXPAND | wxALL, 3);
    pButtonSizer->Add(new wxButton(this, IDC_New, AppStr(newmod_create)), 0, wxEXPAND | wxALL, 3);

    pTopSizer->Add(pButtonSizer, 0, wxALIGN_LEFT);

    SetSizer(pTopSizer);
    pTopSizer->SetSizeHints(this);
    SetBackgroundColour(pBgTemp->GetBackgroundColour());
}

wxString frmNewMod::GetPath() { return m_sDoWPath; }

bool frmNewMod::IsDEMod() const { return m_pList->GetSelection() == g_kDefinitiveEdition; }

void frmNewMod::OnGameChange(wxCommandEvent &event)
{
    const bool de = m_pList->GetSelection() == g_kDefinitiveEdition;
    m_pDisplayLabel->Show(de);
    m_pDisplayName->Show(de);
    m_pDescriptionLabel->Show(de);
    m_pDescription->Show(de);
    m_pParentLabel->Show(de);
    m_pParent->Show(de);
    m_pBrowse->Enable(!de);
    m_pName->SetToolTip(de ? wxString(wxT("DE identifier: 1-64 ASCII letters, digits or underscores; no spaces."))
                           : AppStr(newmod_namehelp));
    if (de && m_pName->GetValue() == wxT("My Mod"))
    {
        m_pName->SetValue(wxT("MyMod"));
    }
    else if (!de && m_pName->GetValue() == wxT("MyMod"))
    {
        m_pName->SetValue(wxT("My Mod"));
    }
    if (de)
    {
        m_pCreation->SetLabel(m_sDEPath);
    }
    else if (event.GetSelection() == g_kCompanyOfHeroes)
    {
        m_pCreation->SetLabel(m_sCoHPath);
    }
    else if (event.GetSelection() == g_kDarkCrusade)
    {
        m_pCreation->SetLabel(m_sDCPath);
    }
    else if (event.GetSelection() == g_kSoulstorm)
    {
        m_pCreation->SetLabel(m_sSSPath);
    }
    else if (event.GetSelection() == g_kDawnOfWar || event.GetSelection() == g_kWinterAssault)
    {
        m_pCreation->SetLabel(m_sDoWPath);
    }
    GetSizer()->Layout();
    GetSizer()->Fit(this);
}

void frmNewMod::OnBrowseClick(wxCommandEvent &event)
{
    UNUSED(event);
    if (m_pList->GetSelection() == g_kDefinitiveEdition)
    {
        return;
    }
    wxString sVal =
        wxDirSelector(AppStr(sgapack_dirselect), m_pCreation->GetLabel(), 0, wxDefaultPosition, TheConstruct);

    if (sVal.empty())
    {
        return;
    }
    m_pCreation->SetLabel(sVal);

    if (m_pList->GetSelection() == g_kCompanyOfHeroes)
    {
        m_sCoHPath = sVal;
        TheConfig->Write(AppStr(config_cohfolder), sVal);
    }
    else if (m_pList->GetSelection() == g_kDarkCrusade)
    {
        m_sDCPath = sVal;
        TheConfig->Write(AppStr(config_dcfolder), sVal);
    }
    else if (m_pList->GetSelection() == g_kSoulstorm)
    {
        m_sSSPath = sVal;
        TheConfig->Write(AppStr(config_ssfolder), sVal);
    }
    else if (m_pList->GetSelection() == g_kDawnOfWar || m_pList->GetSelection() == g_kWinterAssault)
    {
        m_sDoWPath = sVal;
        TheConfig->Write(AppStr(config_dowfolder), sVal);
    }
}

void frmNewMod::_MakeCOH_Language(const char *sToc, const char *sName1, const char *sName2, const char *sDirectoryName,
                                  FILE *fModule)
{
    fprintf(fModule, "[data:%s:01]\xD\n", sToc);
    fprintf(fModule, "folder = %s\\Locale\\%s\\Data\xD\n", sDirectoryName, sName1);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sLocale-%s\xD\n", sDirectoryName, sDirectoryName, sName1);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:%s:02]\xD\n", sToc);
    fprintf(fModule, "folder = %s\\DataSound%s\xD\n", sDirectoryName, sName2);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sSoundSpeech%s\xD\n", sDirectoryName, sDirectoryName, sName2);
    fprintf(fModule, "archive.02 = %s\\Archives\\%sAlliesSpeech%s\xD\n", sDirectoryName, sDirectoryName, sName2);
    fprintf(fModule, "archive.03 = %s\\Archives\\%sAxisSpeech%s\xD\n", sDirectoryName, sDirectoryName, sName2);
    fprintf(fModule, "archive.04 = %s\\Archives\\%sSoundNIS%s\xD\n", sDirectoryName, sDirectoryName, sName2);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:%s:03]\xD\n", sToc);
    fprintf(fModule, "folder = WW2\\Locale\\%s\\Data\xD\n", sName1);
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2Locale-%s\xD\n", sName1);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:%s:04]\xD\n", sToc);
    fprintf(fModule, "folder = WW2\\DataSound%s\xD\n", sName2);
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2SoundSpeech%s\xD\n", sName2);
    fprintf(fModule, "archive.02 = WW2\\Archives\\WW2AlliesSpeech%s\xD\n", sName2);
    fprintf(fModule, "archive.03 = WW2\\Archives\\WW2AxisSpeech%s\xD\n", sName2);
    fprintf(fModule, "archive.04 = WW2\\Archives\\WW2SoundNIS%s\xD\n", sName2);
    fprintf(fModule, "archive.05 = WW2\\Archives\\OFCoreSpeech%s\xD\n", sName2);
    fprintf(fModule, "archive.06 = WW2\\Archives\\OFFullSpeech%s\xD\n", sName2);

    fprintf(fModule, "\xD\n");
}

void frmNewMod::_MakeCOH(char *sNiceName, char *sDirectoryFullPath, char *sDirectoryName, FILE *fModule)
{
    auto sName = wxStringToAscii(m_pName->GetValue());

    // Setup directories
    char *saDirExt = new char[strlen(sDirectoryFullPath) + 64];
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\Data");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\Locale");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\Movies");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataAttrib");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundLow");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundEnglish");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundGerman");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundFrench");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundSpanish");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundRussian");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataSoundHigh");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataArtLow");
    _mkdir(saDirExt);
    strcpy(saDirExt, sDirectoryFullPath);
    strcat(saDirExt, "\\DataArtHigh");
    _mkdir(saDirExt);
    delete[] saDirExt;

    // Write module file
    fprintf(fModule, "[global]\xD\n");
    fprintf(fModule, "Name = %s\xD\n", sNiceName);
    fprintf(fModule, "UIName = %s\xD\n", sName.get());
    fprintf(fModule, "Description = The %s mod\xD\n", sName.get());
    fprintf(fModule, "DllName = WW2Mod\xD\n");
    fprintf(fModule, "ModVersion = 1.0\xD\n");
    fprintf(fModule, "ModFolder = %s\\Data\xD\n", sDirectoryName);
    fprintf(fModule, "LocFolder = %s\\Locale\xD\n", sDirectoryName);
    fprintf(fModule, "ScenarioPackFolder = %s\\Scenarios\xD\n", sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[attrib:common]\xD\n");
    fprintf(fModule, "folder = %s\\DataAttrib\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\AttribArchive\xD\n", sDirectoryName);
    fprintf(fModule, "archive.02 = WW2\\Archives\\AttribArchive\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[movies:common:01]\xD\n");
    fprintf(fModule, "folder = %s\\Movies\xD\n", sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[movies:common:02]\xD\n");
    fprintf(fModule, "folder = WW2\\Movies\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:common:01]\xD\n");
    fprintf(fModule, "folder = %s\\Data\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sData\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "archive.02 = %s\\Archives\\%sArt\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "archive.03 = %s\\Archives\\%sSound\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "archive.04 = %s\\Archives\\%sArtAmbient\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:common:02]\xD\n");
    fprintf(fModule, "folder = WW2\\Data\xD\n");
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2Data\xD\n");
    fprintf(fModule, "archive.02 = WW2\\Archives\\WW2Art\xD\n");
    fprintf(fModule, "archive.03 = WW2\\Archives\\WW2Sound\xD\n");
    fprintf(fModule, "archive.04 = WW2\\Archives\\WW2ArtAmbient\xD\n");
    fprintf(fModule, "archive.05 = WW2\\Archives\\OFSPData\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:sound_low:01]\xD\n");
    fprintf(fModule, "folder = %s\\DataSoundLow\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sSoundLow\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:sound_low:02]\xD\n");
    fprintf(fModule, "folder = WW2\\DataSoundLow\xD\n");
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2SoundLow\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:sound_high:01]\xD\n");
    fprintf(fModule, "folder = %s\\DataSoundHigh\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sSoundHigh\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:sound_high:02]\xD\n");
    fprintf(fModule, "folder = WW2\\DataSoundHigh\xD\n");
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2SoundHigh\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:art_low:01]\xD\n");
    fprintf(fModule, "folder = %s\\DataArtLow\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sArtLow\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:art_low:02]\xD\n");
    fprintf(fModule, "folder = WW2\\DataArtLow\xD\n");
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2ArtLow\xD\n");
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:art_high:01]\xD\n");
    fprintf(fModule, "folder = %s\\DataArtHigh\xD\n", sDirectoryName);
    fprintf(fModule, "archive.01 = %s\\Archives\\%sArtHigh\xD\n", sDirectoryName, sDirectoryName);
    fprintf(fModule, "\xD\n");

    fprintf(fModule, "[data:art_high:02]\xD\n");
    fprintf(fModule, "folder = WW2\\DataArtHigh\xD\n");
    fprintf(fModule, "archive.01 = WW2\\Archives\\WW2ArtHigh\xD\n");
    fprintf(fModule, "\xD\n");

    _MakeCOH_Language("english", "English", "English", sDirectoryName, fModule);
    _MakeCOH_Language("german", "German", "German", sDirectoryName, fModule);
    _MakeCOH_Language("french", "French", "French", sDirectoryName, fModule);
    _MakeCOH_Language("spanish", "Spanish", "Spanish", sDirectoryName, fModule);
    _MakeCOH_Language("russian", "Russian", "Russian", sDirectoryName, fModule);
    _MakeCOH_Language("italian", "Italian", "English", sDirectoryName, fModule);
    _MakeCOH_Language("czech", "Czech", "English", sDirectoryName, fModule);
    _MakeCOH_Language("polish", "Polish", "English", sDirectoryName, fModule);
    _MakeCOH_Language("chinesetrad", "ChineseTrad", "English", sDirectoryName, fModule);
    _MakeCOH_Language("japanese", "Japanese", "English", sDirectoryName, fModule);
    _MakeCOH_Language("korean", "Korean", "English", sDirectoryName, fModule);
    _MakeCOH_Language("chineseenglish", "ChineseEnglish", "English", sDirectoryName, fModule);
    _MakeCOH_Language("chinesesimp", "ChineseSimp", "English", sDirectoryName, fModule);
}

void frmNewMod::OnNewClick(wxCommandEvent &event)
{
    UNUSED(event);
    CDMS_LOG_INFO("Creating new mod");
    if (m_pList->GetSelection() == g_kDefinitiveEdition)
    {
        try
        {
            CDEModGenerator::Options options{
                m_pName->GetValue().ToStdString(wxConvUTF8),
                m_pDisplayName->GetValue().ToStdString(wxConvUTF8),
                m_pDescription->GetValue().ToStdString(wxConvUTF8),
                m_pParent->GetStringSelection().ToStdString(wxConvUTF8),
            };
            const auto modsRoot = std::filesystem::path(m_sDEPath.ToStdWstring());
            const auto gamePath = std::filesystem::path(m_sDEGamePath.ToStdWstring());
            if (!std::filesystem::is_regular_file(gamePath / (options.parent + ".module")))
            {
                throw std::runtime_error("Selected parent module is missing from the configured DE game folder: " +
                                         options.parent + ".module");
            }
            const auto module = CDEModGenerator::Create(modsRoot, options);
            m_sDoWPath = wxString(module.wstring());
            EndModal(wxID_NEW);
        }
        catch (const std::exception &e)
        {
            ThemeColours::ShowMessageBox(wxString::FromUTF8(e.what()), AppStr(new_mod), wxICON_ERROR, this);
        }
        return;
    }

    wxString sModNiceName = _UpdatePath(m_pName->GetValue());
    auto saNice = wxStringToAscii(sModNiceName);

    // Secure a directory
    auto saDoW = wxStringToAscii(m_pCreation->GetLabel());
    char *saDir = new char[strlen(saDoW.get()) + sModNiceName.Len() + 32];
    int iRes;
    unsigned int iVal = 0;
    do
    {
        char buffer[20];
        strcpy(saDir, saDoW.get());
        size_t iL = strlen(saDir) - 1;
        if ((saDir[iL] != '\\') && (saDir[iL] != '/'))
        {
            strcat(saDir, "\\");
        }
        strcat(saDir, saNice.get());
        if (iVal != 0)
        {
            _ultoa(iVal, buffer, 10);
            strcat(saDir, buffer);
        }
        ++iVal;
    } while (((iRes = _mkdir(saDir)) == -1) && (errno == EEXIST));
    if (iRes == -1 && errno != EEXIST)
    {
        delete[] saDir;
        ThemeColours::ShowMessageBox(AppStr(newmod_error), AppStr(new_mod), wxICON_ERROR, this);
        EndModal(wxID_CLOSE);
        return;
    }
    if (m_pList->GetSelection() == g_kDawnOfWar || m_pList->GetSelection() == g_kWinterAssault ||
        m_pList->GetSelection() == g_kDarkCrusade || m_pList->GetSelection() == g_kSoulstorm)
    {
        char *saDirExt = new char[strlen(saDir) + 30];
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Shared_Textures");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Shared_Textures\\Full");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Sound");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Sound\\Low");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Sound\\Med");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Sound\\Full");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Music");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Whm");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Whm\\High");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Whm\\Low");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Data_Whm\\Medium");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Chinese");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Czech");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\English");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\English_Chinese");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\French");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\German");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Italian");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Japanese");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Korean");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Korean adult");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Polish");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Russian");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Slovak");
        _mkdir(saDirExt);
        strcpy(saDirExt, saDir);
        strcat(saDirExt, "\\Locale\\Spanish");
        _mkdir(saDirExt);
        delete[] saDirExt;
    }
    char *sTmp222 = saDir + strlen(saDoW.get());
    char *sTmpDir = strdup((*sTmp222 == '/' || *sTmp222 == '\\') ? sTmp222 + 1 : sTmp222);
    char *sDirBackup = strdup(saDir);
    // Secure a module file
    FILE *f = 0;
    iVal = 0;
    do
    {
        if (f)
        {
            fclose(f);
        }
        char buffer[20];
        strcpy(saDir, saDoW.get());
        size_t iL = strlen(saDir) - 1;
        if ((saDir[iL] != '\\') && (saDir[iL] != '/'))
        {
            strcat(saDir, "\\");
        }
        strcat(saDir, saNice.get());
        if (iVal != 0)
        {
            _ultoa(iVal, buffer, 10);
            strcat(saDir, buffer);
        }
        strcat(saDir, ".module");
        ++iVal;
    } while (f = fopen(saDir, "r"));
    f = fopen(saDir, "wb");
    m_sDoWPath = AsciiTowxString(saDir);

    if (m_pList->GetSelection() == g_kCompanyOfHeroes)
    {
        _MakeCOH(saNice.get(), sDirBackup, sTmpDir, f);
    }
    else
    {
        saNice = wxStringToAscii(m_pName->GetValue());

        fprintf(f, "[global]\xD\n");
        fprintf(f, "UIName = %s\xD\n", saNice.get());
        fprintf(f, "Description = \xD\n");
        switch (m_pList->GetSelection())
        {
        case g_kDawnOfWar:
            fprintf(f, "DllName = W40kMod\xD\n");
            break;
        case g_kDarkCrusade:
        case g_kSoulstorm:
            fprintf(f, "Playable = 1\xD\n");
        case g_kWinterAssault:
            fprintf(f, "DllName = WXPMod\xD\n");
            break;
        default:
            break;
        };
        fprintf(f, "ModFolder = %s\xD\nModVersion = 1.0\xD\nTextureFE = \xD\nTextureIcon = \xD\n\xD\n", sTmpDir);
        free(sTmpDir);
        fprintf(f, "DataFolder.1 = %%LOCALE%%\\Data\xD\nDataFolder.2 = Data\xD\nDataFolder.3 = "
                   "Data_Shared_Textures\\%%TEXTURE-LEVEL%%\xD\n");
        fprintf(f, "DataFolder.4 = Data_Sound\\%%SOUND-LEVEL%%\xD\nDataFolder.5 = Data_Music\xD\nDataFolder.6 = "
                   "Data_Whm\\%%MODEL-LEVEL%%\xD\n\xD\n");

        switch (m_pList->GetSelection())
        {
        case g_kDawnOfWar:
            fprintf(f, "RequiredMod.1 = W40k\xD\n");
            break;
        case g_kWinterAssault:
            fprintf(f, "RequiredMod.1 = WXP\xD\n");
            fprintf(f, "RequiredMod.2 = W40k\xD\n");
            break;
        case g_kDarkCrusade:
        case g_kSoulstorm:
            fprintf(f, "RequiredMod.1 = DXP2\xD\n");
            fprintf(f, "RequiredMod.2 = W40k\xD\n");
            break;
        default:
            break;
        };
    }

    free(sDirBackup);
    delete[] saDir;

    fclose(f);
    // End
    EndModal(wxID_NEW);
}

void frmNewMod::OnCancelClick(wxCommandEvent &event)
{
    UNUSED(event);
    EndModal(wxID_CLOSE);
}
