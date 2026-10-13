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

#include "rainman/lua/CLuaFromRgd.h"
#include "rainman/lua/CLuaFile.h"
#include "rainman/formats/CRgdFile.h"
#include <memory>
#include "rainman/module/CModuleFile.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include "rainman/core/memdebug.h"
#include "rainman/core/Exception.h"
#include "rainman/core/Internal_Util.h"
#include <vector>

extern "C"
{
    using ub4 = unsigned long int; /* unsigned 4-byte quantities */
    using ub1 = unsigned char;
    ub4 hash(ub1 *k, ub4 length, ub4 initval);
    ub4 hash3(ub1 *k, ub4 length, ub4 initval);
}

char *UnicodeToAscii(const wchar_t *pUnicode, int i = 0)
{
    if (pUnicode == nullptr)
    {
        char *sAscii = new char[1];
        if (!sAscii)
            return nullptr;
        sAscii[0] = 0;
        return sAscii;
    }

    size_t iLen = wcslen(pUnicode) + 1;
    char *sAscii = new char[iLen];
    if (!sAscii)
        return nullptr;
    for (size_t i = 0; i < iLen; ++i)
    {
        sAscii[i] = (char)pUnicode[i];
    }
    return sAscii;
}

/* Quick dumping */

void Recursive_Quick_Dump(IMetaNode *pNode, IFileStore::IOutputStream *pOut, const char *sPrefix, bool bG = true)
{
    char *sNewPrefix;
    sNewPrefix = new char[strlen(sPrefix) + 5 + (pNode->VGetName() ? strlen(pNode->VGetName()) : 10)];

    if (!sNewPrefix)
        throw CRainmanException(__FILE__, __LINE__, "Memory allocation error");

    strcpy(sNewPrefix, sPrefix);
    if (!bG)
        strcat(sNewPrefix, "[\"");
    try
    {
        if (pNode->VGetName())
        {
            strcat(sNewPrefix, pNode->VGetName());
        }
        else
        {
            size_t i = strlen(sNewPrefix);
            sNewPrefix[i] = '0';
            sNewPrefix[++i] = 'x';
            unsigned long iHash = pNode->VGetNameHash();
            for (int iNibble = 7; iNibble >= 0; --iNibble)
            {
                sNewPrefix[++i] = "0123456789ABCDEF"[(iHash >> (iNibble << 2)) & 15];
            }
            sNewPrefix[++i] = 0;
        }
        if (!bG)
            strcat(sNewPrefix, "\"]");
    }
    catch (const CRainmanException &e)
    {
        delete[] sNewPrefix;
        throw CRainmanException(e, __FILE__, __LINE__, "pNode problem");
    }

    try
    {
        pOut->VWrite(strlen(sNewPrefix), 1, sNewPrefix);
        pOut->VWrite(3, 1, " = ");
    }
    catch (const CRainmanException &e)
    {
        delete[] sNewPrefix;
        throw CRainmanException(e, __FILE__, __LINE__, "pOut problem");
    }

    try
    {
        switch (pNode->VGetType())
        {
            //! Floating point numeric data
        case IMetaNode::DT_Float:
        {
            char sFpBuffer[35];
            try
            {
                sprintf((char *)sFpBuffer, "%.14g", pNode->VGetValueFloat());
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error getting value");
            }
            try
            {
                pOut->VWrite(strlen(sFpBuffer), 1, sFpBuffer);
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error writing value");
            }
            break;
        }
        case IMetaNode::DT_Integer:
        {
            char sFpBuffer[35];
            try
            {
                sprintf((char *)sFpBuffer, "%lu", pNode->VGetValueInteger());
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error getting value");
            }
            try
            {
                pOut->VWrite(strlen(sFpBuffer), 1, sFpBuffer);
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error writing value");
            }
            break;
        }

            //! Boolean data
        case IMetaNode::DT_Bool:
            try
            {
                pOut->VWrite(5, 1, pNode->VGetValueBool() ? "true " : "false");
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error");
            }
            break;

            //! ASCII text data
        case IMetaNode::DT_String:
            try
            {
                pOut->VWrite(2, 1, "[[");
                pOut->VWrite(strlen(pNode->VGetValueString()), 1, pNode->VGetValueString());
                pOut->VWrite(2, 1, "]]");
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error with DT_String");
            }
            break;

            //! Unicode text data
        case IMetaNode::DT_WString:
        {
            try
            {
                char *sAsc = UnicodeToAscii(pNode->VGetValueWString());
                pOut->VWrite(2, 1, "[[");
                pOut->VWrite(strlen(sAsc), 1, sAsc);
                pOut->VWrite(2, 1, "]]");
                delete[] sAsc;
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error with DT_WString");
            }
            break;
        }

            //! Table data
        case IMetaNode::DT_Table:
        {
            IMetaNode::IMetaTable *pTable;
            try
            {
                pTable = pNode->VGetValueMetatable();
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Cannot get metatable");
            }
            switch (pTable->VGetReferenceType())
            {
            case IMetaNode::DT_String:
                try
                {
                    pOut->VWrite(12, 1, bG ? " Inherit( [[" : "Reference([[");
                    pOut->VWrite(strlen(pTable->VGetReferenceString()), 1, pTable->VGetReferenceString());
                    pOut->VWrite(3, 1, "]])");
                }
                catch (const CRainmanException &e)
                {
                    delete pTable;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error with DT_String");
                }
                break;
            case IMetaNode::DT_WString:
            {
                try
                {
                    char *sAsc = UnicodeToAscii(pTable->VGetReferenceWString());
                    pOut->VWrite(12, 1, bG ? " Inherit( [[" : "Reference([[");
                    pOut->VWrite(strlen(sAsc), 1, sAsc);
                    pOut->VWrite(3, 1, "]])");
                    delete[] sAsc;
                }
                catch (const CRainmanException &e)
                {
                    delete pTable;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error with DT_WString");
                }
                break;
            }
            default:
                try
                {
                    pOut->VWrite(15, 1, bG ? "Inherit([[]])  " : "Reference([[]])");
                }
                catch (const CRainmanException &e)
                {
                    delete pTable;
                    throw CRainmanException(e, __FILE__, __LINE__, "Output error");
                }
                break;
            }

            unsigned long iChildC;
            try
            {
                iChildC = pTable->VGetChildCount();
            }
            catch (const CRainmanException &e)
            {
                delete pTable;
                throw CRainmanException(e, __FILE__, __LINE__, "Table error");
            }
            try
            {
                pOut->VWrite(1, 1, "\n");
            }
            catch (const CRainmanException &e)
            {
                delete pTable;
                throw CRainmanException(e, __FILE__, __LINE__, "Output error");
            }
            for (unsigned long i = 0; i < iChildC; ++i)
            {
                IMetaNode *pNodeC;
                try
                {
                    pNodeC = pTable->VGetChild(i);
                }
                catch (const CRainmanException &e)
                {
                    delete pTable;
                    throw CRainmanException(e, __FILE__, __LINE__, "Cannot get child");
                }
                try
                {
                    Recursive_Quick_Dump(pNodeC, pOut, sNewPrefix, false);
                }
                catch (const CRainmanException &e)
                {
                    delete pNodeC;
                    delete pTable;
                    throw CRainmanException(e, __FILE__, __LINE__, "Cannot dump child");
                }
                delete pNodeC;
            }
            delete pTable;
            break;
        }

            //! No data (not read or written to RGD)
        case IMetaNode::DT_NoData:
        default:
            pOut->VWrite(24, 1, "nil -- unknown data type");
            break;
        }
    }
    catch (const CRainmanException &e)
    {
        delete[] sNewPrefix;
        throw CRainmanException(e, __FILE__, __LINE__, "Error");
    }

    try
    {
        if (pNode->VGetType() != IMetaNode::DT_Table)
            pOut->VWrite(1, 1, "\n");
    }
    catch (const CRainmanException &e)
    {
        delete[] sNewPrefix;
        throw CRainmanException(e, __FILE__, __LINE__, "Error");
    }
    delete[] sNewPrefix;
}

RAINMAN_API void MakeLuaFromRgdQuickly(CRgdFile *pRgd, IFileStore::IOutputStream *pOut)
{
    try
    {
        Recursive_Quick_Dump(pRgd, pOut, "");
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Failed to dump file");
    }
}

/* 2nd generation lua dumping */

IMetaNode::IMetaTable *MakeLuaFromRgdAndNil_LoadTable(IMetaNode::IMetaTable *pSrc, IFileStore *pStore,
                                                      CRgdHashTable *pHashTable, CRgdFile **pRgdOut, CLuaFile **pLuaOut,
                                                      bool bAllowLua = true)
{
    char *sFilename = nullptr;
    IMetaNode::IMetaTable *pRet = nullptr;
    if (pSrc && pStore)
    {
        switch (pSrc->VGetReferenceType())
        {
        case IMetaNode::DT_String:
            sFilename = Util_mystrdup(pSrc->VGetReferenceString());
            break;

        case IMetaNode::DT_WString:
            sFilename = UnicodeToAscii(pSrc->VGetReferenceWString());
            break;
        };

        if (sFilename)
        {
            std::unique_ptr<IFileStore::IStream> pInStream;
            bool bIsLuaStream = true;

            char *sDotChr = strrchr(sFilename, '.');
            if (sDotChr)
                *sDotChr = 0;
            char *sFullpath = new char[strlen(sFilename) + 32];

            if (bAllowLua)
            {
                sprintf(sFullpath, "attrib\\attrib\\%s.lua", sFilename);
                try
                {
                    pInStream = std::unique_ptr<IFileStore::IStream>(pStore->VOpenStream(sFullpath));
                    goto got_input_stream;
                }
                IGNORE_EXCEPTIONS
            }

            sprintf(sFullpath, "attrib\\attrib\\%s.rgd", sFilename);
            try
            {
                pInStream = std::unique_ptr<IFileStore::IStream>(pStore->VOpenStream(sFullpath));
                bIsLuaStream = false;
                goto got_input_stream;
            }
            IGNORE_EXCEPTIONS

            if (bAllowLua)
            {
#ifndef RAINMAN_GNUC
                __nop(); // Optimization barrier (was inline asm hack)
#endif
                sprintf(sFullpath, "data\\attrib\\%s.lua", sFilename);
                try
                {
                    pInStream = std::unique_ptr<IFileStore::IStream>(pStore->VOpenStream(sFullpath));
                    goto got_input_stream;
                }
                IGNORE_EXCEPTIONS
            }

            sprintf(sFullpath, "data\\attrib\\%s.rgd", sFilename);
            try
            {
                pInStream = std::unique_ptr<IFileStore::IStream>(pStore->VOpenStream(sFullpath));
                bIsLuaStream = false;
                goto got_input_stream;
            }
            IGNORE_EXCEPTIONS

        got_input_stream:
            if (pInStream)
            {
                if (bIsLuaStream)
                {
                    auto *pLua = new CLuaFile;
                    try
                    {
                        pLua->Load(pInStream.get(), pStore, sFullpath);
                    }
                    catch (const CRainmanException &e)
                    {
                        delete pLua;
                        delete[] sFullpath;
                        // As a last resort, try to load the RGD instead
                        try
                        {
                            return MakeLuaFromRgdAndNil_LoadTable(pSrc, pStore, pHashTable, pRgdOut, pLuaOut, false);
                        }
                        IGNORE_EXCEPTIONS
                        throw e;
                    }
                    unsigned long iN = pLua->VGetChildCount();
                    for (unsigned long i = 0; i < iN; ++i)
                    {
                        IMetaNode *pChildNode = pLua->VGetChild(i);
                        if (stricmp(pChildNode->VGetName(), "GameData") == 0 &&
                            pChildNode->VGetType() == IMetaNode::DT_Table)
                        {
                            pRet = pChildNode->VGetValueMetatable();
                            *pLuaOut = pLua;
                            delete pChildNode;
                            break;
                        }
                        delete pChildNode;
                    }

                    if (!pRet)
                    {
                        delete pLua;
                    }
                }
                else
                {
                    auto *pRgd = new CRgdFile;
                    try
                    {
                        pRgd->SetHashTable(pHashTable);
                        pRgd->Load(pInStream.get(), sFullpath);
                    }
                    catch (const CRainmanException &e)
                    {
                        delete pRgd;
                        delete[] sFullpath;
                        throw e;
                    }
                    pRet = pRgd->VGetValueMetatable();
                    *pRgdOut = pRgd;
                }
            }

            delete[] sFullpath;
        }
    }

    return pRet;
}

struct MakeLuaFromRgdAndNil_TableEntry
{
    MakeLuaFromRgdAndNil_TableEntry()
    {
        iHash = 0;
        sName = nullptr;
        pRgdNode = nullptr;
        pInhNode = nullptr;
    }

    ~MakeLuaFromRgdAndNil_TableEntry()
    {
        if (sName)
            free(sName);
        if (pRgdNode)
            delete pRgdNode;
        if (pInhNode)
            delete pInhNode;
    }

    unsigned long iHash;
    char *sName;
    IMetaNode *pRgdNode;
    IMetaNode *pInhNode;
};

struct MakeLuaFromRgdAndNil_Nil_Node;

struct MakeLuaFromRgdAndNil_Nil_Table
{
    IMetaNode::eDataTypes ref_type;
    union
    {
        char *ref_s;
        wchar_t *ref_ws;
    };

    std::vector<MakeLuaFromRgdAndNil_Nil_Node *> children;
};

inline float f_abs(float f) { return f < 0 ? -f : f; }

struct MakeLuaFromRgdAndNil_Nil_Value
{
    MakeLuaFromRgdAndNil_Nil_Value()
    {
        iCount = 0;
        type = IMetaNode::DT_NoData;
    }

    MakeLuaFromRgdAndNil_Nil_Value(IMetaNode *pNode, IMetaNode::IMetaTable *pTable)
    {
        iCount = 0;
        type = pNode->VGetType();
        switch (type)
        {
        case IMetaNode::DT_Float:
            f = pNode->VGetValueFloat();

        case IMetaNode::DT_Integer:
            i = pNode->VGetValueInteger();

        case IMetaNode::DT_String:
            s = strdup(pNode->VGetValueString());

        case IMetaNode::DT_Bool:
            b = pNode->VGetValueBool();

        case IMetaNode::DT_WString:
            ws = wcsdup(pNode->VGetValueWString());

        case IMetaNode::DT_Table:
        {
            t = new MakeLuaFromRgdAndNil_Nil_Table;
            t->ref_type =
                pTable->VGetReferenceType(); // NOLINT(clang-analyzer-core.CallAndMessage) — pTable validated by caller
            switch (t->ref_type)
            {
            case IMetaNode::DT_String:
                t->ref_s = strdup(pTable->VGetReferenceString());

            case IMetaNode::DT_WString:
                t->ref_ws = wcsdup(pTable->VGetReferenceWString());
            };
        }
        };
    }

    bool operator==(MakeLuaFromRgdAndNil_Nil_Value &oOther)
    {
        if (oOther.type != type)
            return false;
        switch (type)
        {
        case IMetaNode::DT_Float:
            return (f_abs((float)(oOther.f - f)) < 0.00001f);

        case IMetaNode::DT_Integer:
            return oOther.i == i;

        case IMetaNode::DT_String:
            return (strcmp(oOther.s, s) == 0);

        case IMetaNode::DT_Bool:
            return oOther.b == b;

        case IMetaNode::DT_WString:
            return (wcscmp(oOther.ws, ws) == 0);

        case IMetaNode::DT_Table:
        {
            if (oOther.t->ref_type != t->ref_type)
                return false;
            switch (t->ref_type)
            {
            case IMetaNode::DT_String:
                return (strcmp(oOther.t->ref_s, t->ref_s) == 0);

            case IMetaNode::DT_WString:
                return (wcscmp(oOther.t->ref_ws, t->ref_ws) == 0);

            default:
                return true;
            };
        }

        default:
            return false;
        };
    }

    bool IsEqual(IMetaNode *pNode, IMetaNode::IMetaTable **ppTable)
    {
        if (pNode->VGetType() != type)
            return false;
        switch (type)
        {
        case IMetaNode::DT_Float:
            return (f_abs(pNode->VGetValueFloat() - f) < 0.00001f);

        case IMetaNode::DT_Integer:
            return pNode->VGetValueInteger() == i;

        case IMetaNode::DT_String:
            return (strcmp(pNode->VGetValueString(), s) == 0);

        case IMetaNode::DT_Bool:
            return pNode->VGetValueBool() == b;

        case IMetaNode::DT_WString:
            return (wcscmp(pNode->VGetValueWString(), ws) == 0);

        case IMetaNode::DT_Table:
        {
            IMetaNode::IMetaTable *pTable;
            if (*ppTable)
            {
                pTable = *ppTable;
            }
            else
            {
                pTable = pNode->VGetValueMetatable();
                *ppTable = pTable;
            }
            if (pTable->VGetReferenceType() != t->ref_type)
                return false;
            switch (t->ref_type)
            {
            case IMetaNode::DT_String:
                return (strcmp(pTable->VGetReferenceString(), t->ref_s) == 0);

            case IMetaNode::DT_WString:
                return (wcscmp(pTable->VGetReferenceWString(), t->ref_ws) == 0);

            default:
                return true;
            };
        }

        default:
            return false;
        };
    }

    size_t iCount;

    MakeLuaFromRgdAndNil_Nil_Value &operator++()
    {
        ++iCount;
        return *this;
    }

    IMetaNode::eDataTypes type;
    union
    {
        float f;
        unsigned long i;
        char *s;
        bool b;
        wchar_t *ws;
        MakeLuaFromRgdAndNil_Nil_Table *t;
    };
};

struct MakeLuaFromRgdAndNil_Nil_Node
{
    char *sName;
    unsigned long iNameHash;
    std::vector<MakeLuaFromRgdAndNil_Nil_Value *> vValues;
    size_t iFilesWith;

    MakeLuaFromRgdAndNil_Nil_Node()
    {
        iFilesWith = 0;
        sName = nullptr;
        iNameHash = 0;
    }
};

void MakeLuaFromRgdAndNil_MakeNil_OnFile_Recurse(IMetaNode *pNode, MakeLuaFromRgdAndNil_Nil_Node *pOurNode)
{
    IMetaNode::IMetaTable *pTable = nullptr;
    MakeLuaFromRgdAndNil_Nil_Value *pVal = nullptr;
    for (auto itr = pOurNode->vValues.begin(); itr != pOurNode->vValues.end(); ++itr)
    {
        if ((**itr).IsEqual(pNode, &pTable))
        {
            pVal = *itr;
            break;
        }
    }
    if (pNode->VGetType() == IMetaNode::DT_Table && pTable == nullptr)
        pTable = pNode->VGetValueMetatable();
    if (!pVal)
    {
        pVal = new MakeLuaFromRgdAndNil_Nil_Value(pNode, pTable);
        pOurNode->vValues.push_back(pVal);
    }
    ++(pVal->iCount);
    if (pTable)
    {
    }
}

void MakeLuaFromRgdAndNil_MakeNil_OnFile(IDirectoryTraverser::IIterator *pFile, void *pTag)
{
    const char *sFilename = pFile->VGetName();
    const char *sDotChar = strrchr(sFilename, '.');
    if (sDotChar && (stricmp(sDotChar, ".rgd") == 0))
    {
        std::unique_ptr<IFileStore::IStream> pStream(pFile->VOpenFile());
        CRgdFile oRgd;
        oRgd.Load(pStream.get(), pFile->VGetFullPath());
        MakeLuaFromRgdAndNil_MakeNil_OnFile_Recurse((IMetaNode *)&oRgd, (MakeLuaFromRgdAndNil_Nil_Node *)pTag);
    }
}

void MakeLuaFromRgdAndNil_MakeNil(IFileStore *pStore, IDirectoryTraverser *pTraverse, const char *sFolder)
{
    MakeLuaFromRgdAndNil_Nil_Node oGameData;
    oGameData.sName = strdup("GameData");
    oGameData.iNameHash = (unsigned long)hash((ub1 *)"GameData", 8, 0);

    IDirectoryTraverser::IIterator *pItr = pTraverse->VIterate(sFolder);
    Util_ForEach(pItr, MakeLuaFromRgdAndNil_MakeNil_OnFile, (void *)&oGameData, false);
    delete pItr;
}

//! Prints a node from an RGD file to a LUA stream
/*!
    \param[in] pUcsResolver The module class to use to resolve UCS values
    \param[in] pRgdIn The RGD node (Can be NULL, but only if pNilIn is non-NULL)
    \param[in] pNilIn The matching NIL/LUA(/RGD) node (Can be NULL, but only if pRgdIn is non-NULL)
    \param[in] pStore A filestore to read the Reference()d LUAs/RGDs from (Shouldn't be NULL)
    \param[in] pHashTable The hashtable to use for RGDs read in during Refernce() calls
    \param[in] pLuaOut The output stream for the created LUA
    \param[in] sPrefix The LUA output name of this node, eg "GameData" or "GameData["sqaud_ext"]["value"]"
    \param[in] bG True if the current node is a LUA global, false otherwise
    \return Returns no value, but throws a CRainmanException on error
*/
void MakeLuaFromRgdAndNil_Node(CModuleFile *pUcsResolver, IMetaNode *pRgdIn, IMetaNode *pNilIn,
                               IFileStore::IOutputStream *pLuaOut, IFileStore *pStore, CRgdHashTable *pHashTable,
                               const char *sPrefix, bool bG = true)
{
    bool bEchoVal = false;     // Will we echo the value of this node?
    bool bLookChildren = true; // Will we look at the children of this node? (Tables only)
    IMetaNode::IMetaTable *pTableRgd = nullptr,
                          *pTableNil = nullptr; // Pointers to store the metatables of pRgdIn / pNilIn

    // Get the metatables of pRgdIn & pNilIn
    try
    {
        if (pRgdIn && pRgdIn->VGetType() == IMetaNode::DT_Table)
            pTableRgd = pRgdIn->VGetValueMetatable();
        if (pNilIn && pNilIn->VGetType() == IMetaNode::DT_Table)
            pTableNil = pNilIn->VGetValueMetatable();
    }
    catch (const CRainmanException &e)
    {
        if (pTableRgd)
            delete pTableRgd;
        if (pTableNil)
            delete pTableNil;
        throw CRainmanException(e, __FILE__, __LINE__, "Unable to fetch metatable");
    }

    // Determine if we need to print this node
    if (pNilIn == nullptr || pRgdIn == nullptr)
    {
        if (pRgdIn == nullptr)
        {
            // If there is a LUA/NIL node, and no matching RGD node, nullify the output and don't do any more echoing
            // and dont look at and children either.
            pLuaOut->VWrite(strlen(sPrefix), 1, sPrefix);
            pLuaOut->VWrite(strlen(" = nil\r\n"), 1, " = nil\r\n");
            bLookChildren = false;
        }
        else if (pNilIn == nullptr)
        {
            // If there is an RGD node, but no matching LUA/NIL node, we need to echo the value
            bEchoVal = true;
        }
        else
        {
            // If there is no RGD or LUA/NIL node, we have a problem.
            throw CRainmanException(__FILE__, __LINE__, "RGD node and LUA/NIL node are both NULL");
        }
    }
    else
    {
        if (pRgdIn->VGetType() != pNilIn->VGetType())
        {
            // If the data types are different, then the value must be different, so we always echo it
            bEchoVal = true;
        }
        else
        {
            switch (pRgdIn->VGetType())
            {
            case IMetaNode::DT_Float:
            {
                // Find the different between the two floating point values
                // (== and != on FP values are not reliable)
                try
                {
                    float fD = pRgdIn->VGetValueFloat() - pNilIn->VGetValueFloat();
                    if (fD < 0)
                        fD = -fD;
                    if (fD > 0.00001f)
                    {
                        bEchoVal = true;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching FP values");
                }
                break;
            }
            case IMetaNode::DT_Integer:
                try
                {
                    if (pRgdIn->VGetValueInteger() != pNilIn->VGetValueInteger())
                    {
                        bEchoVal = true;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching integer values");
                }
                break;
            case IMetaNode::DT_Bool:
                try
                {
                    if (pRgdIn->VGetValueBool() != pNilIn->VGetValueBool())
                    {
                        bEchoVal = true;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching boolean values");
                }
                break;
            case IMetaNode::DT_String:
                try
                {
                    if (strcmp(pRgdIn->VGetValueString(), pNilIn->VGetValueString()) != 0)
                    {
                        bEchoVal = true;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching string values");
                }
                break;
            case IMetaNode::DT_WString:
                try
                {
                    if (wcscmp(pRgdIn->VGetValueWString(), pNilIn->VGetValueWString()) != 0)
                    {
                        bEchoVal = true;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching wstring values");
                }
                break;
            case IMetaNode::DT_Table:
            {
                try
                {
                    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) — pTableRgd/pTableNil validated before switch
                    if (pTableRgd->VGetReferenceType() != pTableNil->VGetReferenceType())
                    {
                        bEchoVal = true;
                    }
                    else
                    {
                        switch (pTableRgd->VGetReferenceType())
                        {
                        case IMetaNode::DT_String:
                            if (strcmp(pTableRgd->VGetReferenceString(), pTableNil->VGetReferenceString()) != 0)
                            {
                                bEchoVal = true;
                            }
                            break;
                        case IMetaNode::DT_WString:
                            if (wcscmp(pTableRgd->VGetReferenceWString(), pTableNil->VGetReferenceWString()) != 0)
                            {
                                bEchoVal = true;
                            }
                            break;
                        };
                    }
                }
                catch (const CRainmanException &e)
                {
                    if (pTableRgd)
                        delete pTableRgd;
                    if (pTableNil)
                        delete pTableNil;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error fetching table values");
                }
            }
            break;
            default:
                // Unknown data types should always be echoed, as we dont know how to handle them.
                bEchoVal = true;
                break;
            };
        }
    }

    // Print the value
    if (bEchoVal)
    {
        pLuaOut->VWrite(strlen(sPrefix), 1, sPrefix);
        pLuaOut->VWrite(3, 1, " = ");

        switch (pRgdIn->VGetType())
        {
        case IMetaNode::DT_Float:
        {
            char sFpBuffer[35];
            try
            {
                sprintf((char *)sFpBuffer, "%.10g", pRgdIn->VGetValueFloat());
                pLuaOut->VWrite(strlen(sFpBuffer), 1, sFpBuffer);
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing FP value");
            }
            break;
        }
        case IMetaNode::DT_Integer:
        {
            char sFpBuffer[35];
            try
            {
                sprintf((char *)sFpBuffer, "\"$%lu\"", pRgdIn->VGetValueInteger());
                pLuaOut->VWrite(strlen(sFpBuffer), 1, sFpBuffer);
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing integer value");
            }
            char *sAsc = nullptr;
            try
            {
                const wchar_t *sVal = pUcsResolver->ResolveUCS(pRgdIn->VGetValueInteger());
                if (sVal)
                {
                    sAsc = UnicodeToAscii(sVal);
                    pLuaOut->VWrite(4, 1, " -- ");
                    pLuaOut->VWrite(strlen(sAsc), 1, sAsc);
                }
            }
            IGNORE_EXCEPTIONS
            if (sAsc)
                delete[] sAsc;
            break;
        }
        case IMetaNode::DT_Bool:
            try
            {
                pLuaOut->VWrite(5, 1, pRgdIn->VGetValueBool() ? "true " : "false");
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing boolean value");
            }
            break;

        case IMetaNode::DT_String:
            // Strings need to be encapsulated with [[ and ]], to prevent escape characters from getting in the way
            try
            {
                pLuaOut->VWrite(2, 1, "[[");
                pLuaOut->VWrite(strlen(pRgdIn->VGetValueString()), 1, pRgdIn->VGetValueString());
                pLuaOut->VWrite(2, 1, "]]");
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing string value");
            }
            break;

        case IMetaNode::DT_WString:
        {
            // Unicode strings need to converted back to ASCII, as LUA files are ASCII not unicode
            // Strings need to be encapsulated with [[ and ]], to prevent escape characters from getting in the way
            char *sAsc;
            try
            {
                sAsc = UnicodeToAscii(pRgdIn->VGetValueWString());
                pLuaOut->VWrite(2, 1, "[[");
                pLuaOut->VWrite(strlen(sAsc), 1, sAsc);
                pLuaOut->VWrite(2, 1, "]]");
                delete[] sAsc;
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing w-string value");
            }
            sAsc = nullptr;
            try
            {
                const wchar_t *sVal = pUcsResolver->ResolveUCS(pRgdIn->VGetValueWString());
                if (sVal)
                {
                    sAsc = UnicodeToAscii(sVal);
                    pLuaOut->VWrite(4, 1, " -- ");
                    pLuaOut->VWrite(strlen(sAsc), 1, sAsc);
                }
            }
            IGNORE_EXCEPTIONS
            if (sAsc)
                delete[] sAsc;
            break;
        }

        case IMetaNode::DT_Table:
        {
            // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) — pTableRgd validated before switch
            switch (pTableRgd->VGetReferenceType())
            {
            case IMetaNode::DT_String:
                try
                {
                    pLuaOut->VWrite(bG ? 10 : 12, 1, bG ? "Inherit([[" : "Reference([[");
                    pLuaOut->VWrite(strlen(pTableRgd->VGetReferenceString()), 1, pTableRgd->VGetReferenceString());
                    pLuaOut->VWrite(3, 1, "]])");
                }
                catch (const CRainmanException &e)
                {
                    if (pTableRgd)
                        delete pTableRgd;
                    if (pTableNil)
                        delete pTableNil;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error printing string reference value");
                }
                break;

            case IMetaNode::DT_WString:
            {
                try
                {
                    char *sAsc = UnicodeToAscii(pTableRgd->VGetReferenceWString());
                    pLuaOut->VWrite(bG ? 10 : 12, 1, bG ? "Inherit([[" : "Reference([[");
                    pLuaOut->VWrite(strlen(sAsc), 1, sAsc);
                    pLuaOut->VWrite(3, 1, "]])");
                    delete[] sAsc;
                }
                catch (const CRainmanException &e)
                {
                    if (pTableRgd)
                        delete pTableRgd;
                    if (pTableNil)
                        delete pTableNil;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error printing w-string reference value");
                }
                break;
            }

            default:
                try
                {
                    pLuaOut->VWrite(bG ? 13 : 15, 1, bG ? "Inherit([[]])" : "Reference([[]])");
                }
                catch (const CRainmanException &e)
                {
                    if (pTableRgd)
                        delete pTableRgd;
                    if (pTableNil)
                        delete pTableNil;
                    throw CRainmanException(e, __FILE__, __LINE__, "Error printing empty reference value");
                }
                break;
            };
            break;
        }

        default:
            // output unknown data types, but warn the end user
            try
            {
                pLuaOut->VWrite(50, 1, "nil -- *** WARNING: unknown data type :WARNING ***");
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error printing empty value");
            }
            break;
        };
        pLuaOut->VWrite(strlen("\r\n"), 1, "\r\n");
    }

    // Look at table children

    if (bLookChildren && pRgdIn && pRgdIn->VGetType() == IMetaNode::DT_Table)
    {
        IMetaNode::IMetaTable *pInhTable = nullptr; // The table being inherited / referenced by the current node
        CRgdFile *pDelRgd = nullptr; // stores an RGD file that may have to be "delete"d sometime in the future
        CLuaFile *pDelLua = nullptr; // stores a LUA file that may have to be "delete"d sometime in the future
        if (bEchoVal)
        {
            // If a table node was echoed, that means the reference() was different to the NIL passed to
            // this function. Thus load that one.
            try
            {
                pInhTable = MakeLuaFromRgdAndNil_LoadTable(pTableRgd, pStore, pHashTable, &pDelRgd, &pDelLua);
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                throw CRainmanException(e, __FILE__, __LINE__, "Error loading Reference()d file");
            }
        }
        else
        {
            pInhTable = pTableNil;
        }
        std::vector<MakeLuaFromRgdAndNil_TableEntry *>
            m_vTableEntries; // the nodes present in the resulting child table
        // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage) — pTableRgd validated before this block
        unsigned long iNRgd = pTableRgd->VGetChildCount(),
                      iNInh =
                          pInhTable ? pInhTable->VGetChildCount() : 0; // the number of children in both source tables
        for (unsigned long i = 0; i < iNRgd; ++i)
        {
            // Add all of the RGD's table's children to the node list
            auto *pEntry = new MakeLuaFromRgdAndNil_TableEntry;
            try
            {
                pEntry->pRgdNode = pTableRgd->VGetChild(i);
                pEntry->iHash = (unsigned long)pEntry->pRgdNode->VGetNameHash();
                if (pEntry->pRgdNode->VGetName())
                    pEntry->sName = strdup(pEntry->pRgdNode->VGetName());
            }
            catch (const CRainmanException &e)
            {
                delete pEntry;
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                if (pDelRgd)
                    delete pDelRgd;
                if (pDelLua)
                    delete pDelLua;
                throw CRainmanException(e, __FILE__, __LINE__, "Error importing RGD child %lu", i);
            }
            m_vTableEntries.push_back(pEntry);
        }
        for (unsigned long i = 0; i < iNInh; ++i)
        {
            // Add all of the RGD's table's children to the node list if they are not already in it
            IMetaNode *pNode = nullptr;
            try
            {
                pNode = pInhTable->VGetChild(i);
            }
            catch (const CRainmanException &e)
            {
                if (pTableRgd)
                    delete pTableRgd;
                if (pTableNil)
                    delete pTableNil;
                if (pDelRgd)
                    delete pDelRgd;
                if (pDelLua)
                    delete pDelLua;
                throw CRainmanException(e, __FILE__, __LINE__, "Error fetching LUA/NIL child %lu", i);
            }

            const char *sName = nullptr;
            unsigned long iHash = 0;
            if (pNode->VGetName())
            {
                sName = pNode->VGetName();
                iHash = CRgdHashTable::ValueToHashStatic(sName);
            }
            else
            {
                iHash = (unsigned long)pNode->VGetNameHash();
            }

            bool bDone = false;
            // Look for an entry in the list with the same name/hash
            for (auto itr = m_vTableEntries.begin(); itr != m_vTableEntries.end(); ++itr)
            {
                if ((**itr).iHash == iHash)
                {
                    if (!(**itr).sName && sName)
                        (**itr).sName = strdup(sName);
                    (**itr).pInhNode = pNode;
                    bDone = true;
                    break;
                }
            }
            if (!bDone)
            {
                // Add a new entry if one doesn't already exist
                auto *pEntry = new MakeLuaFromRgdAndNil_TableEntry;
                pEntry->pInhNode = pNode;
                pEntry->iHash = iHash;
                pEntry->sName = strdup(sName);
                m_vTableEntries.push_back(pEntry);
            }
        }
        // Print all of the entries in the list
        for (auto itr = m_vTableEntries.begin(); itr != m_vTableEntries.end(); ++itr)
        {
            char *sNewPrefix = new char[strlen(sPrefix) + 5 + ((**itr).sName ? strlen((**itr).sName) : 10)];
            strcpy(sNewPrefix, sPrefix);
            strcat(sNewPrefix, "[\"");
            if ((**itr).sName)
            {
                strcat(sNewPrefix, (**itr).sName);
            }
            else
            {
                size_t i = strlen(sNewPrefix);
                sNewPrefix[i] = '0';
                sNewPrefix[++i] = 'x';
                unsigned long iHash = (**itr).iHash;
                for (int iNibble = 7; iNibble >= 0; --iNibble)
                {
                    sNewPrefix[++i] = "0123456789ABCDEF"[(iHash >> (iNibble << 2)) & 15];
                }
                sNewPrefix[++i] = 0;
            }
            strcat(sNewPrefix, "\"]");
            MakeLuaFromRgdAndNil_Node(pUcsResolver, (**itr).pRgdNode, (**itr).pInhNode, pLuaOut, pStore, pHashTable,
                                      sNewPrefix, false);
            delete[] sNewPrefix;
            delete *itr;
        }
        if (pDelRgd)
        {
            delete pInhTable;
            delete pDelRgd;
        }
        if (pDelLua)
        {
            // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete) — pDelRgd and pDelLua are mutually exclusive
            delete pInhTable;
            delete pDelLua;
        }
    }

    if (pTableRgd)
        delete pTableRgd;
    if (pTableNil)
        delete pTableNil;
}

RAINMAN_API void MakeLuaFromRgdAndNil(CRgdFile *pRgdIn, CLuaFile *pNilIn, IFileStore *pStore,
                                      IFileStore::IOutputStream *pLuaOut, CModuleFile *pUcsResolver)
{
    if (pNilIn)
    {
        // If we have a NIL file, find the GameData part of it
        unsigned long iN = pNilIn->VGetChildCount();
        for (unsigned long i = 0; i < iN; ++i)
        {
            IMetaNode *pChild;
            pChild = pNilIn->VGetChild(i);
            if (strcmp(pChild->VGetName(), pRgdIn->VGetName()) == 0)
            {
                MakeLuaFromRgdAndNil_Node(pUcsResolver, pRgdIn, pChild, pLuaOut, pStore, pRgdIn->GetHashTable(),
                                          "GameData", true);
                delete pChild;
                return;
            }
            delete pChild;
        }
    }
    // If we have no NIL, or there is no GameData in it, run without a NIL
    MakeLuaFromRgdAndNil_Node(pUcsResolver, pRgdIn, nullptr, pLuaOut, pStore, pRgdIn->GetHashTable(), "GameData", true);
}
