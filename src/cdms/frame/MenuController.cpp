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

#include "MenuController.h"
#include "Construct.h"
#include "common/strings.h"
#include "common/config.h"
#include "ToolRegistry.h"
#include "RelicToolResolver.h"

void MenuController::Build(wxFrame *pFrame, ToolRegistry &registry, RelicToolResolver &relicResolver)
{
    auto *pConstruct = static_cast<ConstructFrame *>(pFrame);

    // File menu
    auto *pMenu_File = new wxMenu;
    pMenu_File->Append(wxID_NEW, AppStr(new_mod_menu), AppStr(new_mod));
    pMenu_File->Append(IDM_LoadModDE, AppStr(open_modde_menu), AppStr(open_modde_help));
    pMenu_File->Append(IDM_BrowseModsDE, AppStr(browse_modde_menu), AppStr(browse_modde_help));
    pMenu_File->Append(IDM_ConfigureDEInstall, AppStr(configure_modde_install_menu),
                       AppStr(configure_modde_install_help));
    pMenu_File->Append(IDM_LoadModSS, AppStr(open_modss_menu), AppStr(open_modss_help));
    pMenu_File->Append(IDM_LoadSga, AppStr(open_sga_menu), AppStr(open_sga_help));
    pMenu_File->AppendSeparator();
    pMenu_File->Append(IDM_LoadModDoWWA, AppStr(open_mod_menu), AppStr(open_mod_help));
    pMenu_File->Append(IDM_LoadModDC, AppStr(open_moddc_menu), AppStr(open_moddc_help));
    pMenu_File->Append(IDM_LoadModCoH, AppStr(open_modcoh_menu), AppStr(open_modcoh_help));
    pMenu_File->AppendSeparator();
    pMenu_File->Append(wxID_CLOSE, AppStr(close_mod_menu), AppStr(close_mod_help));
    pMenu_File->Append(wxID_EXIT, AppStr(exit_menu), AppStr(exit_help));
    pMenu_File->Enable(wxID_CLOSE, false);

    // View menu — Theme
    auto *pMenu_View = new wxMenu;
    pMenu_View->AppendRadioItem(IDM_ThemeSystem, AppStr(view_theme_system), AppStr(view_theme_system_help));
    pMenu_View->AppendRadioItem(IDM_ThemeLight, AppStr(view_theme_light), AppStr(view_theme_light_help));
    pMenu_View->AppendRadioItem(IDM_ThemeDark, AppStr(view_theme_dark), AppStr(view_theme_dark_help));

    // Read saved preference and check the right radio item (0=system, 1=light, 2=dark)
    int iAppearance = 0;
    TheConfig->Read(AppStr(config_appearance), &iAppearance, 0);
    if (iAppearance == 1)
    {
        pMenu_View->Check(IDM_ThemeLight, true);
    }
    else if (iAppearance == 2)
    {
        pMenu_View->Check(IDM_ThemeDark, true);
    }
    else
    {
        pMenu_View->Check(IDM_ThemeSystem, true);
    }

    pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnThemeChange, pConstruct, IDM_ThemeSystem);
    pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnThemeChange, pConstruct, IDM_ThemeLight);
    pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnThemeChange, pConstruct, IDM_ThemeDark);

    // Language server toggle
    pMenu_View->AppendSeparator();
    pMenu_View->AppendCheckItem(IDM_LspEnabled, AppStr(view_lsp_enabled), AppStr(view_lsp_enabled_help));
    bool bLspEnabled = true;
    TheConfig->Read(AppStr(config_lsp_enabled), &bLspEnabled, true);
    pMenu_View->Check(IDM_LspEnabled, bLspEnabled);
    pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnLspToggle, pConstruct, IDM_LspEnabled);

    // File preview toggle
    pMenu_View->AppendCheckItem(IDM_PreviewEnabled, AppStr(view_preview_enabled), AppStr(view_preview_enabled_help));
    bool bPreviewEnabled = true;
    TheConfig->Read(AppStr(config_preview_enabled), &bPreviewEnabled, true);
    pMenu_View->Check(IDM_PreviewEnabled, bPreviewEnabled);
    pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnPreviewToggle, pConstruct, IDM_PreviewEnabled);

    // Mod menu
    auto *pMenu_Mod = new wxMenu;
    pMenu_Mod->Append(wxID_PROPERTIES, AppStr(mod_properties_menu), AppStr(mod_properties_help));
    pMenu_Mod->Append(IDM_DEArchive, AppStr(modde_archive_menu), AppStr(modde_archive_help));
    pMenu_Mod->Enable(IDM_DEArchive, false);
    pMenu_Mod->AppendSeparator();
    for (size_t t = 0; t < registry.GetCount(); ++t)
    {
        int id = IDM_ModToolBase + static_cast<int>(t);
        pMenu_Mod->Append(id, registry.Get(t)->GetName(), registry.Get(t)->GetHelpString());
        pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnToolMenuCommand, pConstruct, id);
    }

    // Relic tools menu — built from resolver's tool list
    auto *pMenu_RelicTools = new wxMenu;
    for (size_t i = 0; i < relicResolver.GetToolCount(); ++i)
    {
        const auto &tool = relicResolver.GetTool(i);
        int id = IDM_RelicToolBase + static_cast<int>(i);
        pMenu_RelicTools->Append(id, tool.sMenuLabel, tool.sHelpString);
        pMenu_RelicTools->Enable(id, tool.bFound);
        pConstruct->Bind(wxEVT_MENU, &ConstructFrame::OnRelicToolCommand, pConstruct, id);
    }

    // Play menu
    auto *pMenu_Play = new wxMenu;
    pMenu_Play->Append(IDM_PlayMod, AppStr(play_mod), AppStr(play_mod_help));
    pMenu_Play->Append(IDM_PlayWarn, AppStr(play_warn), AppStr(play_warn_help));
    pMenu_Play->AppendSeparator();
    pMenu_Play->AppendCheckItem(IDM_PlayDev, AppStr(play_dev), AppStr(play_dev_help));
    pMenu_Play->AppendCheckItem(IDM_PlayNoMovies, AppStr(play_nomov), AppStr(play_nomov_help));
    pMenu_Play->Check(IDM_PlayNoMovies, true);
    pMenu_Play->Enable(IDM_PlayMod, false);

    // Help menu
    auto *pMenu_Help = new wxMenu;
    pMenu_Help->Append(wxID_HELP_CONTENTS, AppStr(help_index_menu), AppStr(help_index_help));
    pMenu_Help->Append(IDM_GitHubRepo, AppStr(github_repo_menu), AppStr(github_repo_help));
    pMenu_Help->AppendSeparator();
    pMenu_Help->Append(IDM_Credits, AppStr(credits_menu), AppStr(credits_help));

    // Assemble menu bar
    auto *pMenuBar = new wxMenuBar;
    pMenuBar->Append(pMenu_File, AppStr(file_menu));
    pMenuBar->Append(pMenu_View, AppStr(view_menu));
    pMenuBar->Append(pMenu_Mod, AppStr(mod_menu));
    pMenuBar->Append(pMenu_RelicTools, AppStr(relic_tools_menu));
    pMenuBar->Append(pMenu_Play, AppStr(play_menu));
    pMenuBar->Append(pMenu_Help, AppStr(help_menu));
    pFrame->SetMenuBar(pMenuBar);
    pMenuBar->EnableTop(2, false);
}

void MenuController::UpdateRelicToolsState(wxFrame *pFrame, const RelicToolResolver &resolver)
{
    wxMenuBar *pMenuBar = pFrame->GetMenuBar();
    if (!pMenuBar)
    {
        return;
    }

    for (size_t i = 0; i < resolver.GetToolCount(); ++i)
    {
        int id = IDM_RelicToolBase + static_cast<int>(i);
        wxMenuItem *pItem = pMenuBar->FindItem(id);
        if (pItem)
        {
            pItem->Enable(resolver.GetTool(i).bFound);
        }
    }
}
