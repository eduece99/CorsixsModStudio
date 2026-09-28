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

#include "CFileTreePresenter.h"

CFileTreePresenter::CFileTreePresenter(IFileTreeView &view) : m_view(view) {}

int CFileTreePresenter::ClassifyFileIcon(const wxString &sFileName)
{
    int iDot = sFileName.Find('.', true);
    if (iDot == wxNOT_FOUND)
    {
        return Icon_Unknown;
    }

    wxString sExt = sFileName.Mid(iDot);

    if (sExt.IsSameAs(wxT(".ai"), false))
    {
        return Icon_AI;
    }
    if (sExt.IsSameAs(wxT(".lua"), false))
    {
        return Icon_Lua;
    }
    if (sExt.IsSameAs(wxT(".burn"), false))
    {
        return Icon_Lua;
    }
    if (sExt.IsSameAs(wxT(".nil"), false))
    {
        return Icon_Nil;
    }
    if (sExt.IsSameAs(wxT(".rgd"), false))
    {
        return Icon_Rgd;
    }
    if (sExt.IsSameAs(wxT(".scar"), false))
    {
        return Icon_Scar;
    }
    if (sExt.IsSameAs(wxT(".tga"), false))
    {
        return Icon_Tga;
    }
    if (sExt.IsSameAs(wxT(".rgt"), false))
    {
        return Icon_Rgt;
    }
    if (sExt.IsSameAs(wxT(".dds"), false))
    {
        return Icon_Dds;
    }
    if (sExt.IsSameAs(wxT(".bfx"), false))
    {
        return Icon_Bfx;
    }
    if (sExt.IsSameAs(wxT(".abp"), false))
    {
        return Icon_Abp;
    }
    if (sExt.IsSameAs(wxT(".rgm"), false))
    {
        return Icon_Rgm;
    }
    if (sExt.IsSameAs(wxT(".nis"), false))
    {
        return Icon_Nis;
    }

    return Icon_Unknown;
}

bool CFileTreePresenter::IsEngineFileMapName(const wxString &sFileMapName,
                                             const std::vector<wxString> &asEngineFileMapNames)
{
    for (const auto &sEngine : asEngineFileMapNames)
    {
        if (sFileMapName == sEngine)
        {
            return true;
        }
    }
    return false;
}

CFileTreePresenter::FileSource
CFileTreePresenter::CategorizeFileSource(const wxString &sFileMapName, const wxString &sThisModFileMapName,
                                         const std::vector<wxString> &asEngineFileMapNames)
{
    if (sFileMapName == sThisModFileMapName)
    {
        return FileSource::ThisMod;
    }
    if (IsEngineFileMapName(sFileMapName, asEngineFileMapNames))
    {
        return FileSource::Engine;
    }
    return FileSource::OtherMod;
}
