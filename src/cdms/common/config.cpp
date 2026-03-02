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

#include "config.h"
#include "strings.h"
#include "strconv.h"
#include <rainman/util/Util.h>
#include <rainman/core/Exception.h>
#include "Common.h"

wxColour ConfGetColour(const wxString &keyname, int def_r, int def_g, int def_b)
{
    wxString sKey;

    sKey = keyname;
    sKey.Append(AppStr(config_colour_rpost));
    TheConfig->Read(sKey, &def_r, def_r);

    sKey = keyname;
    sKey.Append(AppStr(config_colour_gpost));
    TheConfig->Read(sKey, &def_g, def_g);

    sKey = keyname;
    sKey.Append(AppStr(config_colour_bpost));
    TheConfig->Read(sKey, &def_b, def_b);

    return wxColour(def_r, def_g, def_b);
}

wxString ConfGetDoWFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetDoWPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return TheConfig->Read(AppStr(config_dowfolder), wsDefault);
}

wxString ConfGetDCFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetDCPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW:DC path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return TheConfig->Read(AppStr(config_dcfolder), wsDefault);
}

wxString ConfGetSSFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetSSPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW:SS path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return TheConfig->Read(AppStr(config_ssfolder), wsDefault);
}

wxString ConfGetCoHFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetCoHPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get CoH path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return TheConfig->Read(AppStr(config_cohfolder), wsDefault);
}

wxString ConfGetModToolsFolder()
{
    wxString sDoWFolder;
    try
    {
        sDoWFolder = ConfGetDoWFolder();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW folder");
    }
    sDoWFolder.Append(wxT("ModTools\\"));
    return TheConfig->Read(AppStr(config_modtoolsfolder), sDoWFolder);
}

wxString ConfGetDEFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetDEPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW:DE path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return TheConfig->Read(AppStr(config_defolder), wsDefault);
}

wxString ConfGetDEModsFolder()
{
    char *sDefault;
    try
    {
        sDefault = Rainman_GetDEModsPath();
    }
    catch (const CRainmanException &e)
    {
        throw CModStudioException(e, __FILE__, __LINE__, "Unable to get DoW:DE mods path");
    }
    wxString wsDefault = AsciiTowxString(sDefault);
    Rainman_DeleteCharArray(sDefault);
    return wsDefault;
}
