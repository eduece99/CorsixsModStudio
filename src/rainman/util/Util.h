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

#pragma once
#include "rainman/core/gnuc_defines.h"

#include "rainman/formats/CRgdHashTable.h"
#include "rainman/io/IDirectoryTraverser.h"
#include "rainman/core/Api.h"

//! Retrieves the Dawn of War installation path
/*!
    Looks for the DoW installation path in the registry,
    if not found looks for the Demo installation path,
    if not found returns default install location

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result

    \author Corsix
*/
RAINMAN_API char *Rainman_GetDoWPath();

//! Retrieves the Dawn of War: Dark Crusade installation path
/*!
    Looks for the DoW:DC installation path in the registry,
    if not found returns default install location

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result

    \author Corsix
*/
RAINMAN_API char *Rainman_GetDCPath();

//! Retrieves the Dawn of War: Soul Storm installation path
/*!
    Looks for the DoW:SS installation path via Steam uninstall registry,
    then THQ registry key (including WOW6432Node for 64-bit),
    if not found returns default install location

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result

    \author Corsix
*/
RAINMAN_API char *Rainman_GetSSPath();

//! Retrieves the Company of Heroes installation path
/*!
    Looks for the CoH installation path in the registry,
    if not found looks for the Beta installation path,
    if not found returns default install location

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result

    \author Corsix
*/
RAINMAN_API char *Rainman_GetCoHPath();

//! Retrieves the Dawn of War Definitive Edition installation path
/*!
    Looks for the DoW DE installation path via Steam uninstall registry,
    if not found returns default Steam install location

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result
*/
RAINMAN_API char *Rainman_GetDEPath();

//! Retrieves the Dawn of War DE user mods folder
/*!
    Returns the AppData mods folder used by DoW:DE for user-created mods:
    %APPDATA%\Relic Entertainment\Dawn of War\mods

    Creates the directory tree if it does not exist. Throws on directory creation
    failure or if the path cannot be represented by the narrow-path API.

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete[] the result
*/
RAINMAN_API char *Rainman_GetDEModsPath();

RAINMAN_API void Rainman_DeleteCharArray(char *pString);

typedef void (*Rainman_ForEachFunction)(IDirectoryTraverser::IIterator *, void *pTag);
RAINMAN_API void Rainman_ForEach(IDirectoryTraverser::IIterator *pDirectory, Rainman_ForEachFunction pFn, void *pTag,
                                 bool bRecursive);

//! Loads all the RGD dictionaries from a specified folder
/*!
    All .txt files in the specified folder are loaded as RGD hash table dictionaries
    \param[in] sPath The folder to iterate over and find dictionaries in. Subdirectories are not searched.
    \param[out] sCustom If passed, the full path of the custom dictionary is strdup()ped here (If no custom dicitonary
   is found, nothing is done with this parameter)
    \param[in] bIgnoreLoadErrors If set to true, any errors which occour when loading a dictionary are ignored

    \return Returns a valid pointer or throws a CRainmanException

    \attention You MUST delete the result. You MUST free() *sCustom if sCustom is specified

    \author Corsix
*/
RAINMAN_API CRgdHashTable *Rainman_LoadDictionaries(const char *sPath, char **sCustom = 0,
                                                    bool bIgnoreLoadErrors = true);
