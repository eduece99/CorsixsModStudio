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

#include "rainman/util/Util.h"
#include "rainman/core/Internal_Util.h"
#include "rainman/io/CFileSystemStore.h"
#ifndef RAINMAN_GNUC
#include <windows.h>
#include <shlobj.h>
#endif
#include <cstring>
#include "rainman/core/memdebug.h"
#include "rainman/core/Exception.h"

RAINMAN_API char *Rainman_GetDoWPath()
{
    /*
        Checks the Windows registry for the Dawn of War installation path. Looks at (in order):
        HKEY_LOCAL_MACHINE\Software\THQ\Dawn of War\
        HKEY_LOCAL_MACHINE\Software\THQ\Dawn of War DEMO\
        If neither of these are found then the default DoW installation path is returned (incase the DoW install didn't
       do its registry bit correctly) The Dawn of War BETA registry location is not checked becuase the DoW beta is very
       outdated.
    */
    char *sDoWPath = nullptr;
    DWORD dataLen = MAX_PATH + 1,
          t; // dataLen is copied into t for each call to RegQueryValueEx, as that function modifies the value passed
    sDoWPath = CHECK_MEM(new char[dataLen]);

#ifndef RAINMAN_GNUC

    long iReturnVal;
    HKEY hkey;

    // Try to open the DoW registry key
    iReturnVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Dawn of War\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

    // Try to open the DoW Demo registry key
    iReturnVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Dawn of War DEMO\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

#endif

    // Return the default path as the registry failed us :/
    return strcpy(sDoWPath, "C:\\Program Files\\THQ\\Dawn of War");
}

RAINMAN_API char *Rainman_GetDCPath()
{
    /*
        Checks the Windows registry for the Dawn of War: Dark Crusade installation path. Looks at (in order):
        HKEY_LOCAL_MACHINE\Software\THQ\Dawn of War - Dark Crusade\
        If neither of these are found then the default DoW:DC installation path is returned (incase the DoW:DC install
       didn't do its registry bit correctly)
    */
    char *sDoWPath = nullptr;
    DWORD dataLen = MAX_PATH + 1,
          t; // dataLen is copied into t for each call to RegQueryValueEx, as that function modifies the value passed
    sDoWPath = CHECK_MEM(new char[dataLen]);

#ifndef RAINMAN_GNUC

    long iReturnVal;
    HKEY hkey;

    // Try to open the DoW:DC registry key
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Dawn of War - Dark Crusade\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

#endif

    // Return the default path as the registry failed us :/
    return strcpy(sDoWPath, "C:\\Program Files\\THQ\\Dawn of War - Dark Crusade");
}

RAINMAN_API char *Rainman_GetSSPath()
{
    /*
        Checks the Windows registry for the Dawn of War: Soulstorm installation path. Looks at (in order):
        HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 9450\
        HKEY_LOCAL_MACHINE\Software\THQ\Dawn of War - Soulstorm\
        HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\THQ\Dawn of War - Soulstorm\
        If none of these are found then the default DoW:SS installation path is returned (incase the DoW:SS install
       didn't do its registry bit correctly)
    */
    char *sDoWPath = nullptr;
    DWORD dataLen = MAX_PATH + 1,
          t; // dataLen is copied into t for each call to RegQueryValueEx, as that function modifies the value passed
    sDoWPath = CHECK_MEM(new char[dataLen]);

#ifndef RAINMAN_GNUC

    long iReturnVal;
    HKEY hkey;

    // Try the Steam uninstall registry key for Soulstorm
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 9450\\",
                     NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

    // Try to open the DoW:SS registry key
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Dawn of War - Soulstorm\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

    // Try the WOW6432Node path for 64-bit systems (32-bit THQ installer writes here)
    iReturnVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\THQ\\Dawn of War - Soulstorm\\", NULL,
                              KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDoWPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDoWPath;
        }
    }

#endif

    // Return the default path as the registry failed us :/
    return strcpy(sDoWPath, "C:\\Program Files\\THQ\\Dawn of War - Soulstorm");
}

RAINMAN_API void Rainman_ForEach(IDirectoryTraverser::IIterator *pDirectory, Rainman_ForEachFunction pFn, void *pTag,
                                 bool bRecursive)
{
    Util_ForEach(pDirectory, (Util_ForEachFunction)pFn, pTag, bRecursive);
}

RAINMAN_API void Rainman_DeleteCharArray(char *pString) { delete[] pString; }

RAINMAN_API char *Rainman_GetCoHPath()
{
    /*
        Checks the Windows registry for the Company of Heroes installation path. Looks at (in order):
        HKEY_LOCAL_MACHINE\Software\THQ\Company of Heroes\
        HKEY_LOCAL_MACHINE\Software\THQ\Company of Heroes Single Player Demo\
        HKEY_LOCAL_MACHINE\Software\THQ\Company of Heroes BETA\
        If neither of these are found then the default CoH installation path is returned (incase the CoH install didn't
       do its registry bit correctly)
    */
    char *sCoHPath = nullptr;
    DWORD dataLen = 1024,
          t; // dataLen is copied into t for each call to RegQueryValueEx, as that function modifies the value passed
    sCoHPath = CHECK_MEM(new char[dataLen]);

#ifndef RAINMAN_GNUC

    long iReturnVal;
    HKEY hkey;

    // Try to open the CoH registry key
    iReturnVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Company of Heroes\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallDir", nullptr, nullptr, (LPBYTE)sCoHPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sCoHPath;
        }
    }

    // Try to open the CoH Demo registry key if CoH registry key wasn't found
    iReturnVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Company of Heroes Single Player Demo\\", NULL,
                              KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallDir", nullptr, nullptr, (LPBYTE)sCoHPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sCoHPath;
        }
    }

    // Try to open the CoH Beta registry key if CoH registry key wasn't found
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\THQ\\Company of Heroes BETA\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sCoHPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sCoHPath;
        }
    }

#endif

    // Return the default path if the registry failed us :/ (This value is guessed as CoH is not released yet)
    return strcpy(sCoHPath, "C:\\Program Files\\THQ\\Company of Heroes");
}

RAINMAN_API char *Rainman_GetDEPath()
{
    /*
        Checks the Windows registry for the Dawn of War Definitive Edition installation path.
        Looks at (in order):
        HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 3556750\
        If not found, returns default Steam install location.
    */
    char *sDEPath = nullptr;
    DWORD dataLen = MAX_PATH + 1,
          t; // dataLen is copied into t for each call to RegQueryValueEx, as that function modifies the value passed
    sDEPath = CHECK_MEM(new char[dataLen]);

#ifndef RAINMAN_GNUC

    long iReturnVal;
    HKEY hkey;

    // Try the Steam uninstall registry key for DE
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 3556750\\",
                     NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        t = dataLen;
        iReturnVal = RegQueryValueEx(hkey, "InstallLocation", nullptr, nullptr, (LPBYTE)sDEPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            return sDEPath;
        }
    }

    // Try the Steam path and construct DE location from it
    iReturnVal =
        RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam\\", NULL, KEY_QUERY_VALUE, &hkey);
    if (iReturnVal == ERROR_SUCCESS)
    {
        char sSteamPath[MAX_PATH + 1];
        t = sizeof(sSteamPath);
        iReturnVal = RegQueryValueEx(hkey, "InstallPath", nullptr, nullptr, (LPBYTE)sSteamPath, &t);
        RegCloseKey(hkey);
        if (iReturnVal == ERROR_SUCCESS)
        {
            snprintf(sDEPath, dataLen, "%s\\steamapps\\common\\Dawn of War Definitive Edition", sSteamPath);
            return sDEPath;
        }
    }

#endif

    // Return the default path as the registry failed us :/
    return strcpy(sDEPath, "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Dawn of War Definitive Edition");
}

RAINMAN_API char *Rainman_GetDEModsPath()
{
    /*
        Returns %APPDATA%\Relic Entertainment\Dawn of War\mods
        Creates the directory tree if it does not already exist.
    */
    char *sModsPath = CHECK_MEM(new char[MAX_PATH + 1]);

#ifndef RAINMAN_GNUC
    char sAppData[MAX_PATH + 1];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, sAppData) != S_OK)
    {
        delete[] sModsPath;
        QUICK_THROW("Failed to get APPDATA folder path");
    }

    snprintf(sModsPath, MAX_PATH + 1, "%s\\Relic Entertainment\\Dawn of War\\mods", sAppData);

    // Create each level of the directory tree if it doesn't exist
    char sPartial[MAX_PATH + 1];
    snprintf(sPartial, sizeof(sPartial), "%s\\Relic Entertainment", sAppData);
    CreateDirectoryA(sPartial, nullptr);
    snprintf(sPartial, sizeof(sPartial), "%s\\Relic Entertainment\\Dawn of War", sAppData);
    CreateDirectoryA(sPartial, nullptr);
    CreateDirectoryA(sModsPath, nullptr);
#else
    delete[] sModsPath;
    QUICK_THROW("Rainman_GetDEModsPath is only supported on Windows");
#endif

    return sModsPath;
}

RAINMAN_API CRgdHashTable *Rainman_LoadDictionaries(const char *sPath, char **sCustom, bool bIgnoreLoadErrors)
{
    /*
        Uses a CFileSystemStore to iterate over the directory (so that the platform specific code isn't repeated here)
        The file called custom.txt is marked as a custom file, every other file is not
    */

    // Check our input is valid (non-null, etc.)
    if (sPath == nullptr)
    {
        QUICK_THROW("No path");
    }

    CFileSystemStore oFSO;
    CRgdHashTable *pRgdHashTable = CHECK_MEM(new CRgdHashTable());
    IDirectoryTraverser::IIterator *pItr;

    try
    {
        pItr = oFSO.VIterate(sPath);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Cannot iterate over \'%s\'", sPath);
    }

    try
    {
        // Check if directory has anything in it (blank directories can either be null pointers or T_Nothing so accodate
        // for both)
        if (pItr && pItr->VGetType() != IDirectoryTraverser::IIterator::T_Nothing)
        {
            do
            {
                if (pItr->VGetType() == IDirectoryTraverser::IIterator::T_File)
                {
                    bool bCustom = false;
                    if (stricmp(pItr->VGetName(), "custom.txt") == 0)
                    {
                        bCustom = true;
                        // Set the sCustom parameter if it was passed
                        if (sCustom)
                        {
                            *sCustom = strdup(pItr->VGetFullPath());
                        }
                    }

                    // Load the dictionary
                    try
                    {
                        pRgdHashTable->ExtendWithDictionary(pItr->VGetFullPath(), bCustom);
                    }
                    catch (const CRainmanException &e)
                    {
                        if (bIgnoreLoadErrors)
                        {
                        }
                        else
                        {
                            throw CRainmanException(e, __FILE__, __LINE__, "Error loading \'%s\'",
                                                    pItr->VGetFullPath());
                        }
                    }
                }
            } while (pItr->VNextItem() == IDirectoryTraverser::IIterator::E_OK);
        }
    }
    CATCH_THROW("Error") // Error could have been many causes, so only a generic message can be given

    return pRgdHashTable;
}
