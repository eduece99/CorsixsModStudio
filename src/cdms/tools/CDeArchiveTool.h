/*
Rainman Library
Copyright (C) 2006 Corsix <corsix@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _C_DE_ARCHIVE_TOOL_H_
#define _C_DE_ARCHIVE_TOOL_H_

#include <wx/string.h>
#include <vector>

class CDeArchiveTool
{
  public:
    enum class EditorType
    {
        Burn,
        SgaConfig,
        Pipeline,
        Unsupported
    };

    static EditorType Classify(const wxString &sPath);

    // Finds physical burn/config files recursively, or pipeline.ini at the project root.
    static std::vector<wxString> FindFiles(const wxString &sProjectRoot, EditorType eType);
    static bool ReadText(const wxString &sPath, wxString &sContent, wxString &sError);
    static bool SaveText(const wxString &sPath, const wxString &sContent, wxString &sError);

    // Source is the physical folder named by TOCStart relativeroot in the config.
    // Archive.exe's -r is the ancestor from which relativeroot resolves to source.
    static bool Prepare(const wxString &sExecutable, const wxString &sConfig, const wxString &sSourceFolder,
                        const wxString &sOutput, std::vector<wxString> &vArguments, wxString &sError);

    // Archive.exe -help (DoW:DE install): -a <archivefile> -c <buildfile> -r <rootpath>.
    // The buildfile is a .sgaconfig; rootpath is the parent of its relativeroot.
    static bool Run(const std::vector<wxString> &vArguments, wxString &sError);
};

#endif
