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

#include "Utility.h"
// #include "strings.h"
#include "Common.h"
#include "ThemeColours.h"

#define SKIP_BACKUP return;

CModStudioException::CModStudioException(const char *sFile, unsigned long iLine, const char *sMessage)
    : CRainmanException(sFile, iLine, sMessage)
{
}
CModStudioException::CModStudioException(CRainmanException *pPrecursor, const char *sFile, unsigned long iLine,
                                         const char *sFormat, ...)
{
    m_sFile = sFile;
    m_iLine = iLine;
    m_pPrecursor.reset(pPrecursor);

    size_t iL = 128;
    va_list marker;
    while (true)
    {
        m_sMessage.resize(iL);
        va_start(marker, sFormat);
        int n = _vsnprintf(m_sMessage.data(), iL, sFormat, marker);
        va_end(marker);
        if (n >= 0 && static_cast<size_t>(n) < iL)
        {
            m_sMessage.resize(static_cast<size_t>(n));
            return;
        }
        iL <<= 1;
    }
}
CModStudioException::CModStudioException(const CRainmanException &precursor, const char *sFile, unsigned long iLine,
                                         const char *sFormat, ...)
{
    m_sFile = sFile;
    m_iLine = iLine;
    m_pPrecursor = std::make_unique<CRainmanException>(precursor);

    size_t iL = 128;
    va_list marker;
    while (true)
    {
        m_sMessage.resize(iL);
        va_start(marker, sFormat);
        int n = _vsnprintf(m_sMessage.data(), iL, sFormat, marker);
        va_end(marker);
        if (n >= 0 && static_cast<size_t>(n) < iL)
        {
            m_sMessage.resize(static_cast<size_t>(n));
            return;
        }
        iL <<= 1;
    }
}

bool _ErrorBox(const wxString &sError, const char *sFile, long iLine, bool bUnhandled, bool bAllowCancel)
{
    wchar_t *pFile = AsciiToUnicode(sFile);
    wchar_t pLine[33];
    _ltow(iLine, &pLine[0], 10);
    wxString sErr = wxString(wxT("Error: "))
                        .Append(sError)
                        .Append(wxT("\nSource: "))
                        .Append(pFile)
                        .Append(wxT(" line "))
                        .Append(pLine)
                        .Append(wxT("\nPlease contact Corsix@gmail.com for help, with details of what you did and the "
                                    "source file and line above"));
    int iAnswer = wxOK;
    if (bUnhandled)
    {
        wxMessageBox(sErr, _T("Error"), wxICON_ERROR | wxOK, wxTheApp->GetTopWindow());
    }
    else
    {
        iAnswer = ThemeColours::ShowMessageBox(sErr, wxT("Error"), wxICON_ERROR | wxOK | (bAllowCancel ? wxCANCEL : 0),
                                               wxTheApp->GetTopWindow());
    }
    delete[] pFile;
    return iAnswer == wxOK;
}

bool _ErrorBox(const CRainmanException &e, const char *sFile, long iLine, bool bUnhandled, bool bAllowCancel)
{
    wchar_t *pFile = AsciiToUnicode(sFile);
    wchar_t pLine[33];
    _ltow(iLine, &pLine[0], 10);
    wxString sError;
    if (bUnhandled)
    {
        sError = wxT("An unhandled exception was caught and the application must terminate. Please report this to a "
                     "programmer:");
    }
    else
    {
        sError = wxT("Error raised at ");
        sError.Append(pFile).Append(wxT(" line ")).Append(pLine).Append(wxT(":"));
    }
    delete[] pFile;
    const CRainmanException *pError = &e;
    while (pError)
    {
        pFile = AsciiToUnicode(pError->getFile());
        _ltow(pError->getLine(), &pLine[0], 10);
        sError.Append(wxT("\n")).Append(pFile).Append(wxT(" line ")).Append(pLine).Append(wxT(": "));
        delete[] pFile;
        pFile = AsciiToUnicode(pError->getMessage());
        sError.Append(pFile);
        delete[] pFile;
        pError = pError->getPrecursor();
    }
    int iAnswer = wxOK;
    if (bUnhandled)
    {
        wxMessageBox(sError, _T("Error"), wxICON_ERROR | wxOK, wxTheApp->GetTopWindow());
    }
    else
    {
        iAnswer = ThemeColours::ShowMessageBox(
            sError, wxT("Error"), wxICON_ERROR | wxOK | (bAllowCancel ? wxCANCEL : 0), wxTheApp->GetTopWindow());
    }
    return iAnswer == wxOK;
}

void BackupFile(const wxString &sFile)
{
    UNUSED(sFile);
    SKIP_BACKUP
    /*
    wxString sBakFile = sFile;
    sBakFile.Append(wxT(".bak"));
    if(!wxCopyFile(sFile, sBakFile))
    {
        ThemeColours::ShowMessageBox(AppStr(app_backuperror), AppStr(app_backuperror_title) , wxICON_ERROR,
    wxTheApp->GetTopWindow());
    }
    */
}

/*
Update this function to work with exceptions, kk?
static bool fsCopyFile(IFileStore* pStore, wxString& sSrc, wxString& sDest)
{
    bool bRet = false;
    if(pStore)
    {
        auto saSrc = wxStringToAscii(sSrc);
        auto saDest = wxStringToAscii(sDest);
        if(saSrc && saDest)
        {
            IFileStore::IStream* pIn = pStore->VOpenStream(saSrc.get());
            if(pIn)
            {
                IFileStore::IOutputStream* pOut = pStore->VOpenOutputStream(saDest.get(), true);
                if(pOut)
                {
                    if(pIn->VSeek(0, IFileStore::IStream::SL_End))
                    {
                        long iLength = pIn->VTell();
                        char* sBuffer = new char[iLength];
                        if(sBuffer && pIn->VSeek(0, IFileStore::IStream::SL_Root))
                        {
                            if(pIn->VRead(iLength, 1, sBuffer) && pOut->VWrite(iLength, 1, sBuffer))
                            {
                                bRet = true;
                            }
                        }
                        delete[] sBuffer;
                    }
                    delete pOut;
                }
                delete pIn;
            }
        }

    }
    return bRet;
}
*/

void BackupFile(IFileStore *pStore, const wxString &sFile)
{
    UNUSED(pStore);
    UNUSED(sFile);
    SKIP_BACKUP
    /*
    wxString sBakFile = sFile;
    sBakFile.Append(wxT(".bak"));
    if(!fsCopyFile(pStore, sFile, sBakFile))
    {
        ThemeColours::ShowMessageBox(AppStr(app_backuperror), AppStr(app_backuperror_title) , wxICON_ERROR,
    wxTheApp->GetTopWindow());
    }
    */
}

void RestoreBackupFile(const wxString &sFile)
{
    UNUSED(sFile);
    SKIP_BACKUP
    /*
    wxString sBakFile = sFile;
    sBakFile.Append(wxT(".bak"));
    if(!wxCopyFile(sBakFile, sFile))
    {
        wxString sMsg = AppStr(app_backupcannotrestore);
        sMsg.Append(sBakFile);
        ThemeColours::ShowMessageBox(sMsg, AppStr(app_backuperror_title) , wxICON_ERROR, wxTheApp->GetTopWindow());
    }
    */
}

void RestoreBackupFile(IFileStore *pStore, const wxString &sFile)
{
    UNUSED(pStore);
    UNUSED(sFile);
    SKIP_BACKUP
    /*
    wxString sBakFile = sFile;
    sBakFile.Append(wxT(".bak"));
    if(!fsCopyFile(pStore, sBakFile, sFile))
    {
        wxString sMsg = AppStr(app_backupcannotrestore);
        sMsg.Append(sBakFile);
        ThemeColours::ShowMessageBox(sMsg, AppStr(app_backuperror_title) , wxICON_ERROR, wxTheApp->GetTopWindow());
    }
    */
}

wxFont MakeCodeFont(int iPointSize, bool bBold, bool bItalic)
{
    wxFontWeight eWeight = bBold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL;
    wxFontStyle eStyle = bItalic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;

    // Try Cascadia Mono first (ships with Windows 10 2004+ / Windows Terminal)
    wxFont font(iPointSize, wxFONTFAMILY_MODERN, eStyle, eWeight, false, wxT("Cascadia Mono"));
    if (font.IsOk() && font.GetFaceName().CmpNoCase(wxT("Cascadia Mono")) == 0)
    {
        return font;
    }

    // Fall back to Consolas (ships since Windows Vista)
    font = wxFont(iPointSize, wxFONTFAMILY_MODERN, eStyle, eWeight, false, wxT("Consolas"));
    if (font.IsOk() && font.GetFaceName().CmpNoCase(wxT("Consolas")) == 0)
    {
        return font;
    }

    // Last resort: system default monospace
    return wxFont(iPointSize, wxFONTFAMILY_MODERN, eStyle, eWeight);
}
