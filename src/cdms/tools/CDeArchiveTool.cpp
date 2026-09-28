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

#include "common/Common.h"
#include "tools/CDeArchiveTool.h"
#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>
#include <filesystem>

CDeArchiveTool::EditorType CDeArchiveTool::Classify(const wxString &sPath)
{
    const wxFileName file(sPath);
    if (file.GetFullName().IsSameAs(wxT("pipeline.ini"), false))
    {
        return EditorType::Pipeline;
    }
    const wxString sExt = file.GetExt();
    if (sExt.IsSameAs(wxT("burn"), false))
    {
        return EditorType::Burn;
    }
    if (sExt.IsSameAs(wxT("sgaconfig"), false))
    {
        return EditorType::SgaConfig;
    }
    return EditorType::Unsupported;
}

std::vector<wxString> CDeArchiveTool::FindFiles(const wxString &sProjectRoot, EditorType eType)
{
    if (!wxDirExists(sProjectRoot) || eType == EditorType::Unsupported)
    {
        return {};
    }
    if (eType == EditorType::Pipeline)
    {
        const wxString path = wxFileName(sProjectRoot, wxT("pipeline.ini")).GetFullPath();
        return wxFileName::FileExists(path) ? std::vector<wxString>{path} : std::vector<wxString>{};
    }
    wxArrayString vFound;
    wxDir::GetAllFiles(sProjectRoot, &vFound, eType == EditorType::Burn ? wxT("*.burn") : wxT("*.sgaconfig"),
                       wxDIR_FILES | wxDIR_DIRS);
    vFound.Sort();
    std::vector<wxString> vFiles;
    for (const auto &sFile : vFound)
    {
        vFiles.push_back(sFile);
    }
    return vFiles;
}

bool CDeArchiveTool::ReadText(const wxString &sPath, wxString &sContent, wxString &sError)
{
    sContent.clear();
    sError.clear();
    if (Classify(sPath) == EditorType::Unsupported)
    {
        sError = wxT("Only .burn, .sgaconfig and pipeline.ini files can be edited here.");
        return false;
    }
    wxFFile oFile(sPath, wxT("rb"));
    if (!oFile.IsOpened() || !oFile.ReadAll(&sContent, wxConvUTF8))
    {
        sError = wxT("Cannot read UTF-8 text file: ") + sPath;
        return false;
    }
    if (Classify(sPath) == EditorType::Burn)
    {
        sContent.Replace(wxT("\r\r\n"), wxT("\r\n"));
    }
    return true;
}

bool CDeArchiveTool::SaveText(const wxString &sPath, const wxString &sContent, wxString &sError)
{
    sError.clear();
    if (Classify(sPath) == EditorType::Unsupported || !wxFileName::FileExists(sPath))
    {
        sError = wxT("The DE configuration file no longer exists: ") + sPath;
        return false;
    }
    wxTempFile oTemp(sPath);
    const auto sBytes = sContent.utf8_str();
    if (!oTemp.IsOpened() || !oTemp.Write(sBytes.data(), sBytes.length()) || !oTemp.Commit())
    {
        sError = wxT("Could not save file: ") + sPath;
        return false;
    }
    return true;
}

bool CDeArchiveTool::Prepare(const wxString &sExecutable, const wxString &sConfig, const wxString &sSourceFolder,
                             const wxString &sOutput, std::vector<wxString> &vArguments, wxString &sError)
{
    vArguments.clear();
    sError.clear();

    wxFileName oExecutable(sExecutable), oSource = wxFileName::DirName(sSourceFolder), oConfig(sConfig),
                                         oOutput(sOutput);
    if (!oExecutable.IsAbsolute() || !oExecutable.FileExists() ||
        !oExecutable.GetFullName().IsSameAs(wxT("Archive.exe"), false))
    {
        sError = wxT("Select the installed Relic Archive.exe executable.");
        return false;
    }
    if (!oSource.IsAbsolute() || !oSource.DirExists())
    {
        sError = wxT("Select an existing absolute source folder.");
        return false;
    }
    if (!oConfig.IsAbsolute() || !oConfig.FileExists() || Classify(sConfig) != EditorType::SgaConfig)
    {
        sError = wxT("Select an existing .sgaconfig file.");
        return false;
    }
    if (!oOutput.IsAbsolute() || !oOutput.GetExt().IsSameAs(wxT("sga"), false) ||
        !wxFileName::DirName(oOutput.GetPath()).DirExists())
    {
        sError = wxT("Select an absolute .sga output path in an existing folder.");
        return false;
    }
    oExecutable.Normalize();
    oSource.Normalize();
    oConfig.Normalize();
    oOutput.Normalize();
    const auto canonicalPath = [&sError](const wxString &sPath)
    {
        std::error_code error;
        const auto path = std::filesystem::canonical(std::filesystem::path(sPath.ToStdWstring()), error);
        if (error)
        {
            sError = wxT("Cannot resolve archive path: ") + sPath + wxT(" (") + wxString::FromUTF8(error.message()) +
                     wxT(")");
        }
        return error ? wxString{} : wxString(path.wstring());
    };
    const wxString sCanonicalExecutable = canonicalPath(oExecutable.GetFullPath());
    if (!sError.empty())
    {
        return false;
    }
    const wxString sCanonicalSource = canonicalPath(oSource.GetPath());
    if (!sError.empty())
    {
        return false;
    }
    const wxString sCanonicalConfig = canonicalPath(oConfig.GetFullPath());
    if (!sError.empty())
    {
        return false;
    }
    const wxString sCanonicalOutputFolder = canonicalPath(oOutput.GetPath());
    if (!sError.empty())
    {
        return false;
    }
    oExecutable.Assign(sCanonicalExecutable);
    oSource = wxFileName::DirName(sCanonicalSource);
    oConfig.Assign(sCanonicalConfig);
    oOutput.Assign(wxFileName(sCanonicalOutputFolder, oOutput.GetFullName()).GetFullPath());
    if (oOutput.GetFullPath().IsSameAs(oConfig.GetFullPath(), false))
    {
        sError = wxT("The archive output must not replace the configuration file.");
        return false;
    }
    if (oOutput.GetFullPath().Left(oSource.GetPathWithSep().length()).CmpNoCase(oSource.GetPathWithSep()) == 0)
    {
        sError = wxT("Choose an archive output outside the source folder to avoid including it in the archive.");
        return false;
    }

    wxString sContents;
    if (!ReadText(oConfig.GetFullPath(), sContents, sError))
    {
        return false;
    }
    wxString sRelativeRoot;
    for (wxStringTokenizer oLines(sContents, wxT("\r\n"), wxTOKEN_STRTOK); oLines.HasMoreTokens();)
    {
        const wxString sLine = oLines.GetNextToken().Trim(false);
        const wxString sLower = sLine.Lower();
        if (!sLower.StartsWith(wxT("tocstart")) || (sLower.length() > 8 && !wxIsspace(sLower[8])))
        {
            continue;
        }
        const int iKey = sLower.Find(wxT("relativeroot"));
        if (iKey == wxNOT_FOUND)
        {
            sError = wxT("TOCStart is missing a quoted relativeroot.");
            return false;
        }
        const wxString sAttribute = sLine.Mid(iKey + wxString(wxT("relativeroot")).length()).Trim(false);
        if (!sAttribute.StartsWith(wxT("=")))
        {
            sError = wxT("TOCStart is missing a quoted relativeroot.");
            return false;
        }
        const wxString sQuoted = sAttribute.Mid(1).Trim(false);
        const auto iEndQuote = sQuoted.find(wxT('"'), 1);
        if (!sQuoted.StartsWith(wxT("\"")) || iEndQuote == wxString::npos)
        {
            sError = wxT("TOCStart is missing a quoted relativeroot.");
            return false;
        }
        const wxString sValue = sQuoted.Mid(1, iEndQuote - 1);
        if (!sRelativeRoot.empty() && !sRelativeRoot.IsSameAs(sValue, false))
        {
            sError = wxT("Configurations with different TOC relativeroot values require separate source folders.");
            return false;
        }
        sRelativeRoot = sValue;
    }
    if (sRelativeRoot.empty())
    {
        sError = wxT("No TOCStart relativeroot found in the archive configuration.");
        return false;
    }
    sRelativeRoot.Replace(wxT("/"), wxT("\\"));
    wxFileName oRelative = wxFileName::DirName(sRelativeRoot);
    if (oRelative.IsAbsolute() || oRelative.GetDirCount() == 0)
    {
        sError = wxT("TOC relativeroot must name a relative source folder.");
        return false;
    }
    wxFileName oRoot(oSource);
    for (size_t i = oRelative.GetDirCount(); i > 0; --i)
    {
        const wxString sPart = oRelative.GetDirs()[i - 1];
        if (sPart == wxT(".") || sPart == wxT("..") || oRoot.GetDirCount() == 0 ||
            !oRoot.GetDirs().Last().IsSameAs(sPart, false))
        {
            sError = wxT("Selected source folder must end with the config's relativeroot: ") + sRelativeRoot;
            return false;
        }
        oRoot.RemoveLastDir();
    }
    vArguments = {oExecutable.GetFullPath(), wxT("-a"), oOutput.GetFullPath(), wxT("-c"),
                  oConfig.GetFullPath(),     wxT("-r"), oRoot.GetPath()};
    return true;
}

bool CDeArchiveTool::Run(const std::vector<wxString> &vArguments, wxString &sError)
{
    sError.clear();
    if (vArguments.size() != 7 || vArguments[1] != wxT("-a") || vArguments[3] != wxT("-c") ||
        vArguments[5] != wxT("-r"))
    {
        sError = wxT("Invalid Relic Archive.exe command.");
        return false;
    }
    std::vector<std::wstring> vStrings;
    vStrings.reserve(vArguments.size());
    for (const auto &sArgument : vArguments)
    {
        vStrings.push_back(sArgument.ToStdWstring());
    }
    std::vector<const wchar_t *> vArgv;
    vArgv.reserve(vStrings.size() + 1);
    for (const auto &sArgument : vStrings)
    {
        vArgv.push_back(sArgument.c_str());
    }
    vArgv.push_back(nullptr);
    const std::filesystem::path oArchive(vArguments[2].ToStdWstring());
    std::error_code oFileError;
    const bool bPreviouslyExists = std::filesystem::exists(oArchive, oFileError);
    if (oFileError)
    {
        sError = wxT("Could not inspect existing archive: ") + vArguments[2];
        return false;
    }
    const auto oPreviousWriteTime =
        bPreviouslyExists ? std::filesystem::last_write_time(oArchive, oFileError) : std::filesystem::file_time_type{};
    if (oFileError)
    {
        sError = wxT("Could not inspect existing archive: ") + vArguments[2];
        return false;
    }
    const long iExitCode = wxExecute(vArgv.data(), wxEXEC_SYNC);
    if (iExitCode != 0)
    {
        if (iExitCode == -1)
        {
            sError = wxT("Could not start Relic Archive.exe.");
        }
        else
        {
            sError = wxString::Format(wxT("Relic Archive.exe failed (exit code %ld)."), iExitCode);
        }
        return false;
    }
    if (!wxFileName::FileExists(vArguments[2]))
    {
        sError = wxT("Relic Archive.exe reported success but did not create the archive.");
        return false;
    }
    if (bPreviouslyExists && std::filesystem::last_write_time(oArchive, oFileError) == oPreviousWriteTime)
    {
        sError = wxT("Relic Archive.exe reported success but did not update the existing archive.");
        return false;
    }
    if (oFileError)
    {
        sError = wxT("Could not inspect the created archive: ") + vArguments[2];
        return false;
    }
    return true;
}
