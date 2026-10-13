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

#include "rainman/formats/CRgmFile.h"
#include "rainman/core/Internal_Util.h"
#include "rainman/core/memdebug.h"
#include <cstdint>

CRgmFile::CRgmFile() = default;

CRgmFile::~CRgmFile() { _Free(); }

void CRgmFile::Load(IFileStore::IStream *pStream)
{
    RAINMAN_LOG_INFO("CRgmFile::Load() — parsing RGM model file");
    _Free();
    m_pChunky = std::make_unique<CChunkyFile>();

    try
    {
        m_pChunky->Load(pStream);
    }
    CATCH_THROW("Cannot parse chunks from stream")

    size_t iC = m_pChunky->GetChildCount();
    for (size_t i = 0; i < iC; ++i)
    {
        try
        {
            _ParseChunk(m_pChunky->GetChild(i));
        }
        CATCH_THROW("Error searching for material chunks")
    }
}

bool CRgmFile::_TSETPrune(CChunkyFile::CChunk *pChunk, std::vector<std::pair<bool, const char *>> &vUsedTextures,
                          size_t &iTSETCount)
{
    if (pChunk->GetType() == CChunkyFile::CChunk::T_Folder)
    {
        if (strcmp(pChunk->GetName(), "TSET") == 0)
        {
            ++iTSETCount;
            if (pChunk->GetChildCount() == 1) // more than 1 = embedded texture data
            {
                const char *sTextureName = pChunk->GetDescriptor();

                for (auto itr = vUsedTextures.begin(); itr != vUsedTextures.end(); ++itr)
                {
                    if (stricmp(sTextureName, itr->second) == 0)
                    {
                        itr->first = true;
                        return false;
                    }
                }
                --iTSETCount;
                return true;
            }
        }
        else
        {
            size_t iC = pChunk->GetChildCount();
            for (size_t i = 0; i < iC; ++i)
            {
                try
                {
                    if (_TSETPrune(pChunk->GetChild(i), vUsedTextures, iTSETCount))
                    {
                        pChunk->RemoveChild(i);
                        --i;
                        --iC;
                    }
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error pruning child for TSET chunks (FOLD%s)",
                                            pChunk->GetName());
                }
            }
        }
    }
    return false;
}

void CRgmFile::Save(IFileStore::IOutputStream *pStream)
{
    RAINMAN_LOG_INFO("CRgmFile::Save() — writing RGM model file");
    for (auto itr = m_vMaterials.begin(); itr != m_vMaterials.end(); ++itr)
    {
        (**itr)._WriteChunk();
    }

    std::vector<std::pair<bool, const char *>> vUsedTextures;
    for (auto itr = m_vMaterials.begin(); itr != m_vMaterials.end(); ++itr)
    {
        for (auto itr2 = (**itr).m_vVariables.begin(); itr2 != (**itr).m_vVariables.end(); ++itr2)
        {
            if ((**itr2).GetType() == CMaterial::CVariable::VT_Text)
            {
                /* To count as a texture name, the value must:
                    - Contain only the characters a-z, A-Z, 0-9, underscore, space & forward slash
                    - Not begin or end with a forward slash
                    - Not immediatly follow one forward slash with another
                    - Be at least 1 character long
                */
                const char *sVal = (**itr2).GetValueText();
                const char *sValCopy = sVal;
                if (*sVal == '\\' || *sVal == 0)
                    continue;
                --sVal;
                do
                {
                    ++sVal;
                    if ((*sVal >= 'a' && *sVal <= 'z') || (*sVal >= 'A' && *sVal <= 'Z') ||
                        (*sVal >= '0' && *sVal <= '9') || *sVal == '_' || *sVal == ' ')
                        ; // NOLINT(bugprone-branch-clone) valid char, continue loop
                    else if (*sVal == '\\' && sVal[-1] != '\\')
                        ;
                    else if (*sVal == 0 && sVal[-1] != '\\')
                    {
                        for (auto itr3 = vUsedTextures.begin(); itr3 != vUsedTextures.end(); ++itr3)
                        {
                            if (stricmp(itr3->second, sValCopy) == 0)
                                goto already_got_tex;
                        }
                        vUsedTextures.emplace_back(false, sValCopy);
                    already_got_tex:;
                    }
                    else
                        break;
                } while (*sVal);
            }
        }
    }

    CChunkyFile::CChunk *pModl = nullptr;
    size_t iC = m_pChunky->GetChildCount();
    size_t iTSETCount = 0;
    for (size_t i = 0; i < iC; ++i)
    {
        try
        {
            CChunkyFile::CChunk *p = m_pChunky->GetChild(i);
            if (_TSETPrune(p, vUsedTextures, iTSETCount))
            {
                m_pChunky->RemoveChild(i);
                --i;
                --iC;
            }
            else
            {
                if (p->GetType() == CChunkyFile::CChunk::T_Folder && strcmp(p->GetName(), "MODL") == 0)
                    pModl = p;
            }
        }
        CATCH_THROW("Error pruning FOLDTSET chunks")
    }

    if (pModl)
    {
        CChunkyFile::CChunk *pINFO = nullptr;
        size_t iTSETInsertBefore = 0;
        iC = pModl->GetChildCount();
        bool bPrevWasTSET = false;
        for (size_t i = 0; i < iC; ++i)
        {
            CChunkyFile::CChunk *p = pModl->GetChild(i);
            if (p->GetType() == CChunkyFile::CChunk::T_Data && strcmp(p->GetName(), "INFO") == 0)
                pINFO = p;
            if (p->GetType() == CChunkyFile::CChunk::T_Folder && strcmp(p->GetName(), "TSET") == 0)
                bPrevWasTSET = true;
            else if (bPrevWasTSET)
            {
                iTSETInsertBefore = i;
                break;
            }
        }

        for (auto itr = vUsedTextures.begin(); itr != vUsedTextures.end(); ++itr)
        {
            if (!itr->first)
            {
                CChunkyFile::CChunk *pTSET =
                    pModl->InsertBefore(iTSETInsertBefore, "TSET", CChunkyFile::CChunk::T_Folder);
                pTSET->SetVersion(1);
                pTSET->SetDescriptor(itr->second);
                CChunkyFile::CChunk *pDATA = pTSET->AppendNew("DATA", CChunkyFile::CChunk::T_Data);
                pDATA->SetVersion(3);
                pDATA->SetUnknown1(1);
                CMemoryStore::COutStream *pData = CMemoryStore::OpenOutputStreamExt();
                unsigned long iL = 8 + (unsigned long)strlen(itr->second);
                pData->VWrite(1, sizeof(uint32_t), &iL);
                pData->VWrite(8, 1, "GENERIC:");
                pData->VWrite(iL - 8, 1, itr->second);
                iL = 0;
                pData->VWrite(1, sizeof(uint32_t), &iL);
                pDATA->SetData(pData);
                delete pData;
            }
        }

        if (pINFO)
        {
            auto *pVal = (unsigned short *)pINFO->GetDataRaw();
            ++pVal;
            *pVal = (unsigned short)iTSETCount;
        }
    }

    try
    {
        m_pChunky->Save(pStream);
    }
    CATCH_THROW("Cannot save chunks to stream")
}

void CRgmFile::CMaterial::_WriteChunk()
{
    m_pOurChunk->SetDescriptor(m_sName.c_str());

    size_t iC = m_pOurChunk->GetChildCount();
    for (size_t i = 0; i < iC; ++i)
    {
        CChunkyFile::CChunk *pChild = m_pOurChunk->GetChild(i);
        if (strcmp(pChild->GetName(), "INFO") == 0)
        {
            try
            {
                CMemoryStore::COutStream *pData = CMemoryStore::OpenOutputStreamExt();
                auto iL = (unsigned long)m_sDxName.size();
                pData->VWrite(1, sizeof(uint32_t), &iL);
                pData->VWrite(iL, 1, m_sDxName.c_str());
                pChild->SetData(pData);
                delete pData;
            }
            catch (const CRainmanException &e)
            {
                _Free();
                throw CRainmanException(e, __FILE__, __LINE__, "Error writing DATAINFO chunk");
            }
        }
    }

    for (auto itr = m_vVariables.begin(); itr != m_vVariables.end(); ++itr)
    {
        (**itr)._WriteChunk();
    }
}

void CRgmFile::_ParseChunk(CChunkyFile::CChunk *pChunk)
{
    if (pChunk->GetType() == CChunkyFile::CChunk::T_Folder)
    {
        if (strcmp(pChunk->GetName(), "MTRL") == 0)
        {
            std::unique_ptr<CMaterial> pNewMat;
            try
            {
                pNewMat.reset(new CMaterial(pChunk));
            }
            CATCH_THROW("Cannot parse material")

            m_vMaterials.push_back(std::move(pNewMat));
        }
        else
        {
            size_t iC = pChunk->GetChildCount();
            for (size_t i = 0; i < iC; ++i)
            {
                try
                {
                    _ParseChunk(pChunk->GetChild(i));
                }
                catch (const CRainmanException &e)
                {
                    throw CRainmanException(e, __FILE__, __LINE__, "Error searching child for material chunks (FOLD%s)",
                                            pChunk->GetName());
                }
            }
        }
    }
}

CRgmFile::CMaterial::CMaterial(CChunkyFile::CChunk *pChunk)
{
    // Basic chunk check
    if (pChunk->GetType() != CChunkyFile::CChunk::T_Folder || strcmp(pChunk->GetName(), "MTRL") != 0)
        throw CRainmanException(__FILE__, __LINE__, "Not a material chunk");
    if (pChunk->GetVersion() != 1)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Material is version %lu, expected version 1",
                                pChunk->GetVersion());

    // Initialise members
    m_pOurChunk = pChunk;

    // Parse
    m_sName = pChunk->GetDescriptor();
    size_t iC = pChunk->GetChildCount();
    for (size_t i = 0; i < iC; ++i)
    {
        CChunkyFile::CChunk *pChild = pChunk->GetChild(i);
        if (strcmp(pChild->GetName(), "INFO") == 0)
        {
            try
            {
                _ParseInfo(pChild);
            }
            catch (const CRainmanException &e)
            {
                _Free();
                throw CRainmanException(e, __FILE__, __LINE__, "Error parsing material info");
            }
        }
        else if (memcmp(pChild->GetName(), "\0VAR", 5) == 0)
        {
            std::unique_ptr<CVariable> pVar;
            try
            {
                pVar.reset(new CVariable(pChild));
            }
            catch (const CRainmanException &e)
            {
                _Free();
                throw CRainmanException(e, __FILE__, __LINE__, "Error parsing variable");
            }

            m_vVariables.push_back(std::move(pVar));
        }
    }
}

CRgmFile::CMaterial::~CMaterial() { _Free(); }

void CRgmFile::CMaterial::_Free()
{
    m_sName.clear();
    m_sDxName.clear();
    m_vVariables.clear();
}

void CRgmFile::CMaterial::_ParseInfo(CChunkyFile::CChunk *pChunk)
{
    // Basic chunk check
    if (pChunk->GetType() != CChunkyFile::CChunk::T_Data || strcmp(pChunk->GetName(), "INFO") != 0)
        throw CRainmanException(__FILE__, __LINE__, "Not a material chunk");
    if (pChunk->GetVersion() != 1)
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Material info is version %lu, expected version 1",
                                pChunk->GetVersion());
    if (strcmp(pChunk->GetDescriptor(), "Material Info") != 0)
        throw CRainmanException(nullptr, __FILE__, __LINE__,
                                "Material info descriptor is \"%s\", expected \"Material Info\"",
                                pChunk->GetDescriptor());

    // Read
    CMemoryStore::CStream *pData = pChunk->GetData();

    uint32_t iStrLen = 0;
    pData->VRead(1, sizeof(uint32_t), &iStrLen);

    m_sDxName.resize(iStrLen);

    pData->VRead(iStrLen, 1, m_sDxName.data());

    delete pData;
}

const char *CRgmFile::CMaterial::CVariable::GetName() const { return m_sName.c_str(); }

const char *CRgmFile::CMaterial::GetDxName() const { return m_sDxName.c_str(); }

void CRgmFile::CMaterial::SetName(const char *sValue) { m_sName = sValue; }

void CRgmFile::CMaterial::SetDxName(const char *sValue) { m_sDxName = sValue; }

CRgmFile::CMaterial::CVariable::eValTypes CRgmFile::CMaterial::CVariable::GetType() const { return m_eValType; }

const char *CRgmFile::CMaterial::CVariable::GetValueText() const
{
    if (m_eValType != VT_Text)
        throw CRainmanException(__FILE__, __LINE__, "Value type is not text");
    return m_sValue;
}

float CRgmFile::CMaterial::CVariable::GetValueNumber() const
{
    if (m_eValType != VT_Number)
        throw CRainmanException(__FILE__, __LINE__, "Value type is not number");
    return m_fValue;
}

void CRgmFile::CMaterial::CVariable::SetName(const char *sName) { m_sName = sName; }

void CRgmFile::CMaterial::CVariable::SetValueText(const char *sValue)
{
    if (m_eValType == VT_Text)
        delete[] m_sValue;
    m_sValue = Util_mystrdup(sValue);
    m_eValType = VT_Text;
}

void CRgmFile::CMaterial::CVariable::SetValueNumber(float fValue)
{
    if (m_eValType == VT_Number)
        delete[] m_sValue;
    m_fValue = fValue;
    m_eValType = VT_Number;
}

void CRgmFile::CMaterial::CVariable::_WriteChunk()
{
    CMemoryStore::COutStream *pData = CMemoryStore::OpenOutputStreamExt();

    unsigned long iL;
    iL = (unsigned long)m_sName.size();
    pData->VWrite(1, sizeof(uint32_t), &iL);
    pData->VWrite(iL, 1, m_sName.c_str());

    switch (m_eValType)
    {
    case VT_Number:
        iL = 1;
        pData->VWrite(1, sizeof(uint32_t), &iL);
        iL = (unsigned long)sizeof(float);
        pData->VWrite(1, sizeof(uint32_t), &iL);
        pData->VWrite(1, sizeof(float), &m_fValue);
        break;

    case VT_Text:
        iL = 9;
        pData->VWrite(1, sizeof(uint32_t), &iL);
        iL = 1 + (unsigned long)strlen(m_sValue);
        pData->VWrite(1, sizeof(uint32_t), &iL);
        pData->VWrite(iL, 1, m_sValue);
        break;

    default:
        delete pData;
        pData = nullptr;
        break;
    };

    if (pData != nullptr)
    {
        m_pOurChunk->SetData(pData);
        delete pData;
    }
}

CRgmFile::CMaterial::CVariable::CVariable(CChunkyFile::CChunk *pChunk)
{
    m_pOurChunk = pChunk;

    // Basic chunk check
    if (pChunk->GetType() != CChunkyFile::CChunk::T_Data || memcmp(pChunk->GetName(), "\0VAR", 5) != 0)
        throw CRainmanException(__FILE__, __LINE__, "Not a material variable chunk");
    if (pChunk->GetVersion() != 1)
        throw CRainmanException(nullptr, __FILE__, __LINE__,
                                "Material variable info is version %lu, expected version 1", pChunk->GetVersion());
    if (strcmp(pChunk->GetDescriptor(), "Material Variable") != 0)
        throw CRainmanException(nullptr, __FILE__, __LINE__,
                                "Material variable descriptor is \"%s\", expected \"Material Variable\"",
                                pChunk->GetDescriptor());

    // Read
    CMemoryStore::CStream *pData = pChunk->GetData();

    uint32_t iStrLen = 0;
    uint32_t iDataType = 0;
    uint32_t iDataLen = 0;
    pData->VRead(1, sizeof(uint32_t), &iStrLen);

    m_sName.resize(iStrLen);

    pData->VRead(iStrLen, 1, m_sName.data());

    pData->VRead(1, sizeof(uint32_t), &iDataType);
    pData->VRead(1, sizeof(uint32_t), &iDataLen);

    if (iDataType == 9)
    {
        m_eValType = VT_Text;
        m_sValue = new char[iDataLen];
        pData->VRead(iDataLen, 1, m_sValue);
    }
    else if (iDataType == 1)
    {
        if (iDataLen == sizeof(float))
        {
            m_eValType = VT_Number;
            pData->VRead(1, sizeof(float), &m_fValue);
        }
        else
        {
            delete pData;
            _Free();
            throw CRainmanException(nullptr, __FILE__, __LINE__,
                                    "Data length %lu has not been seen before for numeric data. CONTACT CORSIX",
                                    iDataLen);
        }
    }
    else if (iDataType == 5)
    {
        m_eValType = VT_NumberArray;
        //! todo
    }
    else
    {
        delete pData;
        _Free();
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Data type %lu has not been seen before. CONTACT CORSIX",
                                iDataType);
    }

    delete pData;
}

CRgmFile::CMaterial::CVariable::CVariable()
{
    m_pOurChunk = nullptr;
    m_eValType = VT_Number;
    m_fValue = 0.0;
    m_sName = "New Variable";
}

CRgmFile::CMaterial::CVariable::~CVariable() { _Free(); }

void CRgmFile::CMaterial::CVariable::_Free()
{
    if (m_eValType == VT_Text)
        delete[] m_sValue;
}

void CRgmFile::_Free()
{
    m_vMaterials.clear();
    m_pChunky.reset();
}

size_t CRgmFile::GetMaterialCount() const { return m_vMaterials.size(); }

CRgmFile::CMaterial *CRgmFile::GetMaterial(size_t i) { return m_vMaterials[i].get(); }

const char *CRgmFile::CMaterial::GetName() const { return m_sName.c_str(); }

size_t CRgmFile::CMaterial::GetVariableCount() const { return m_vVariables.size(); }

CRgmFile::CMaterial::CVariable *CRgmFile::CMaterial::GetVariable(size_t i) { return m_vVariables[i].get(); }
