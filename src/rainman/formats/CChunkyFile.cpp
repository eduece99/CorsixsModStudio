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

#include "rainman/formats/CChunkyFile.h"
#include "rainman/core/memdebug.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace
{
constexpr char kChunkyHeader[] = "Relic Chunky\x0D\x0A\x1A";
constexpr size_t kHeaderReadSize = 16;
} // namespace

CChunkyFile::CChunkyFile() : m_sHeader{}, m_iVersion(0), m_iUnknown1(0), m_iUnknown2(0), m_iUnknown3(0), m_iUnknown4(0)
{
}

CChunkyFile::~CChunkyFile() = default;

void CChunkyFile::New(long iVersion)
{
    m_vChunks.clear();

    static_assert(sizeof(kChunkyHeader) <= sizeof(m_sHeader));
    std::copy_n(kChunkyHeader, sizeof(kChunkyHeader), m_sHeader.data());

    m_iVersion = iVersion;
    m_iUnknown1 = 1;
    m_iUnknown2 = 36;
    m_iUnknown3 = 28;
    m_iUnknown4 = 1;
}

CChunkyFile::CChunk *CChunkyFile::CChunk::InsertBefore(size_t iBefore, const char *sName, CChunk::eTypes eType)
{
    auto pChunk = std::make_unique<CChunk>();

    pChunk->m_eType = eType;
    std::copy_n(sName, 4, pChunk->m_sName.data());
    pChunk->m_iUnknown1 = (eType == CChunk::T_Folder ? 0 : -1);

    auto *pResult = pChunk.get();
    m_vChildren.insert(m_vChildren.begin() + static_cast<ptrdiff_t>(iBefore), std::move(pChunk));

    return pResult;
}

CChunkyFile::CChunk *CChunkyFile::CChunk::AppendNew(const char *sName, CChunk::eTypes eType)
{
    auto pChunk = std::make_unique<CChunk>();

    pChunk->m_eType = eType;
    std::copy_n(sName, 4, pChunk->m_sName.data());
    pChunk->m_iUnknown1 = (eType == CChunk::T_Folder ? 0 : -1);

    auto *pResult = pChunk.get();
    m_vChildren.push_back(std::move(pChunk));

    return pResult;
}

CChunkyFile::CChunk *CChunkyFile::AppendNew(const char *sName, CChunk::eTypes eType)
{
    auto pChunk = std::make_unique<CChunk>();

    pChunk->m_eType = eType;
    std::copy_n(sName, 4, pChunk->m_sName.data());
    pChunk->m_iUnknown1 = (eType == CChunk::T_Folder ? 0 : -1);

    auto *pResult = pChunk.get();
    m_vChunks.push_back(std::move(pChunk));

    return pResult;
}

void CChunkyFile::Load(IFileStore::IStream *pStream)
{
    RAINMAN_LOG_INFO("CChunkyFile::Load() — parsing chunky file");
    try
    {
        pStream->VRead(kHeaderReadSize, 1, m_sHeader.data());

        if (std::strcmp(m_sHeader.data(), kChunkyHeader) != 0)
        {
            throw CRainmanException(nullptr, __FILE__, __LINE__, "Unrecognised header (%s)", m_sHeader.data());
        }

        uint32_t iVersion = 0;
        uint32_t iUnknown1 = 0;
        uint32_t iUnknown2 = 0;
        uint32_t iUnknown3 = 0;
        uint32_t iUnknown4 = 0;
        pStream->VRead(1, sizeof(iVersion), &iVersion);
        pStream->VRead(1, sizeof(iUnknown1), &iUnknown1);
        if (iVersion >= 3)
        {
            pStream->VRead(1, sizeof(iUnknown2), &iUnknown2);
            pStream->VRead(1, sizeof(iUnknown3), &iUnknown3);
            pStream->VRead(1, sizeof(iUnknown4), &iUnknown4);
        }
        m_iVersion = static_cast<long>(iVersion);
        m_iUnknown1 = static_cast<long>(iUnknown1);
        m_iUnknown2 = static_cast<long>(iUnknown2);
        m_iUnknown3 = static_cast<long>(iUnknown3);
        m_iUnknown4 = static_cast<long>(iUnknown4);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error reading from stream");
    }

    auto pChunk = std::make_unique<CChunk>();

    try
    {
        while (pChunk->_Load(pStream, m_iVersion))
        {
            m_vChunks.push_back(std::move(pChunk));
            pChunk = std::make_unique<CChunk>();
        }
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error loading root chunk (#%lu)",
                                static_cast<unsigned long>(m_vChunks.size()));
    }
}

void CChunkyFile::Save(IFileStore::IOutputStream *pStream)
{
    RAINMAN_LOG_INFO("CChunkyFile::Save() — writing chunky file");
    try
    {
        pStream->VWrite(kHeaderReadSize, 1, m_sHeader.data());
        const uint32_t iVersion = static_cast<uint32_t>(m_iVersion);
        const uint32_t iUnknown1 = static_cast<uint32_t>(m_iUnknown1);
        const uint32_t iUnknown2 = static_cast<uint32_t>(m_iUnknown2);
        const uint32_t iUnknown3 = static_cast<uint32_t>(m_iUnknown3);
        const uint32_t iUnknown4 = static_cast<uint32_t>(m_iUnknown4);
        pStream->VWrite(1, sizeof(iVersion), &iVersion);
        pStream->VWrite(1, sizeof(iUnknown1), &iUnknown1);
        pStream->VWrite(1, sizeof(iUnknown2), &iUnknown2);
        pStream->VWrite(1, sizeof(iUnknown3), &iUnknown3);
        pStream->VWrite(1, sizeof(iUnknown4), &iUnknown4);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error writing to stream");
    }

    for (const auto &pChunk : m_vChunks)
    {
        pChunk->_Save(pStream);
    }
}

bool CChunkyFile::CChunk::_Load(IFileStore::IStream *pStream, long iChunkyVersion)
{
    char sType[5];
    sType[4] = 0;

    try
    {
        pStream->VRead(4, 1, sType);
    }
    catch (const CRainmanException &e)
    {
        return false;
    }
    if (std::strcmp(sType, "DATA") == 0)
        m_eType = T_Data;
    else if (std::strcmp(sType, "FOLD") == 0)
        m_eType = T_Folder;
    else
        throw CRainmanException(nullptr, __FILE__, __LINE__, "Unrecognised chunk type \'%s\'", sType);

    uint32_t iVersion = 0;
    uint32_t iDataLength = 0;
    uint32_t iDescriptorLength = 0;
    uint32_t iUnknown1 = 0;
    uint32_t iUnknown2 = 0;
    try
    {
        pStream->VRead(4, 1, m_sName.data());
        pStream->VRead(1, sizeof(iVersion), &iVersion);
        pStream->VRead(1, sizeof(iDataLength), &iDataLength);
        pStream->VRead(1, sizeof(uint32_t), &iDescriptorLength);
        if (iChunkyVersion >= 3)
        {
            pStream->VRead(1, sizeof(iUnknown1), &iUnknown1);
            pStream->VRead(1, sizeof(iUnknown2), &iUnknown2);
        }
        m_iVersion = static_cast<long>(iVersion);
        m_iDataLength = static_cast<unsigned long>(iDataLength);
        m_iUnknown1 = static_cast<long>(iUnknown1);
        m_iUnknown2 = static_cast<long>(iUnknown2);
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error reading from stream");
    }

    if (iDescriptorLength > 0)
    {
        m_sDescriptor.resize(iDescriptorLength);
        try
        {
            pStream->VRead(iDescriptorLength, 1, m_sDescriptor.data());
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__, "Error reading from stream");
        }
        // Strip trailing null if present (file format includes it)
        if (!m_sDescriptor.empty() && m_sDescriptor.back() == '\0')
            m_sDescriptor.pop_back();
    }
    else
    {
        m_sDescriptor.clear();
    }

    if (m_eType == T_Folder)
    {
        long iDataEnd = 0;
        try
        {
            iDataEnd = pStream->VTell() + static_cast<long>(m_iDataLength);
        }
        catch (const CRainmanException &e)
        {
            throw CRainmanException(e, __FILE__, __LINE__, "Cannot get position from stream");
        }
        unsigned long iChildN = 0;
        while (true)
        {
            try
            {
                if (pStream->VTell() >= iDataEnd)
                    break;
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Cannot get position from stream");
            }

            auto pChunk = std::make_unique<CChunk>();

            try
            {
                if (!pChunk->_Load(pStream, iChunkyVersion))
                    throw CRainmanException(__FILE__, __LINE__, "End of stream reached");
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error reading child #%lu of FOLD%s", iChildN,
                                        m_sName.data());
            }

            m_vChildren.push_back(std::move(pChunk));
            ++iChildN;
        }
    }
    else
    {
        m_vData.resize(m_iDataLength);
        if (m_iDataLength > 0)
        {
            try
            {
                pStream->VRead(m_iDataLength, 1, m_vData.data());
            }
            catch (const CRainmanException &e)
            {
                throw CRainmanException(e, __FILE__, __LINE__, "Error reading from stream");
            }
        }
    }

    return true;
}

unsigned long CChunkyFile::CChunk::_FoldUpdateSize()
{
    unsigned long iDataLength = 0;
    if (m_eType == T_Folder)
    {
        for (const auto &child : m_vChildren)
        {
            iDataLength += child->_FoldUpdateSize();
        }
    }
    else
    {
        iDataLength = static_cast<unsigned long>(m_vData.size());
    }
    m_iDataLength = iDataLength;

    iDataLength += 8 + (5 * sizeof(uint32_t));
    if (!m_sDescriptor.empty())
        iDataLength += static_cast<unsigned long>(m_sDescriptor.size()) + 1;
    return iDataLength;
}

void CChunkyFile::CChunk::_Save(IFileStore::IOutputStream *pStream)
{
    switch (m_eType)
    {
    case T_Data:
        pStream->VWrite(4, 1, "DATA");
        break;
    case T_Folder:
        pStream->VWrite(4, 1, "FOLD");
        break;
    default:
        pStream->VWrite(4, 1, "????");
        break;
    };

    auto iDescriptorLength = static_cast<unsigned long>(m_sDescriptor.size());
    if (iDescriptorLength > 0)
        iDescriptorLength += 1; // include null terminator in wire format

    _FoldUpdateSize();
    const uint32_t iVersion = static_cast<uint32_t>(m_iVersion);
    const uint32_t iDataLength = static_cast<uint32_t>(m_iDataLength);
    const uint32_t iWireDescriptorLength = static_cast<uint32_t>(iDescriptorLength);
    const uint32_t iUnknown1 = static_cast<uint32_t>(m_iUnknown1);
    const uint32_t iUnknown2 = static_cast<uint32_t>(m_iUnknown2);

    try
    {
        pStream->VWrite(4, 1, m_sName.data());
        pStream->VWrite(1, sizeof(iVersion), &iVersion);
        pStream->VWrite(1, sizeof(iDataLength), &iDataLength);
        pStream->VWrite(1, sizeof(iWireDescriptorLength), &iWireDescriptorLength);
        pStream->VWrite(1, sizeof(iUnknown1), &iUnknown1);
        pStream->VWrite(1, sizeof(iUnknown2), &iUnknown2);
        if (iWireDescriptorLength > 0)
            pStream->VWrite(iWireDescriptorLength, 1, m_sDescriptor.c_str());
    }
    catch (const CRainmanException &e)
    {
        throw CRainmanException(e, __FILE__, __LINE__, "Error writing to stream");
    }

    if (m_eType == T_Folder)
    {
        for (const auto &child : m_vChildren)
        {
            child->_Save(pStream);
        }
    }
    else
    {
        if (!m_vData.empty())
            pStream->VWrite(m_iDataLength, 1, m_vData.data());
    }
}

CChunkyFile::CChunk::CChunk()
    : m_eType(T_Folder), m_sName{'N', 'U', 'L', 'L', '\0'}, m_iVersion(0), m_iUnknown1(0), m_iUnknown2(0),
      m_iDataLength(0)
{
}

CChunkyFile::CChunk::~CChunk() = default;

CChunkyFile::CChunk::eTypes CChunkyFile::CChunk::GetType() const { return m_eType; }

long CChunkyFile::CChunk::GetVersion() const { return m_iVersion; }

const char *CChunkyFile::CChunk::GetName() const { return m_sName.data(); }

const char *CChunkyFile::CChunk::GetDescriptor() const { return m_sDescriptor.c_str(); }

CChunkyFile::CChunk *CChunkyFile::GetChildByName(const char *sName, CChunkyFile::CChunk::eTypes eType) const
{
    for (const auto &child : m_vChunks)
    {
        if (child->m_eType == eType && std::strcmp(sName, child->m_sName.data()) == 0)
            return child.get();
    }
    return nullptr;
}

size_t CChunkyFile::CChunk::GetChildCount() const { return m_vChildren.size(); }

CChunkyFile::CChunk *CChunkyFile::CChunk::GetChild(size_t iN) const { return m_vChildren[iN].get(); }

void CChunkyFile::CChunk::RemoveChild(size_t iN)
{
    m_vChildren.erase(m_vChildren.begin() + static_cast<ptrdiff_t>(iN));
}

CChunkyFile::CChunk *CChunkyFile::GetChild(size_t iN) const { return m_vChunks[iN].get(); }

size_t CChunkyFile::GetChildCount() const { return m_vChunks.size(); }

void CChunkyFile::RemoveChild(size_t iN) { m_vChunks.erase(m_vChunks.begin() + static_cast<ptrdiff_t>(iN)); }

CChunkyFile::CChunk *CChunkyFile::CChunk::GetChildByName(const char *sName, CChunkyFile::CChunk::eTypes eType) const
{
    for (const auto &child : m_vChildren)
    {
        if (child->m_eType == eType && std::strcmp(sName, child->m_sName.data()) == 0)
            return child.get();
    }
    return nullptr;
}

CMemoryStore::CStream *CChunkyFile::CChunk::GetData()
{
    return CMemoryStore::OpenStreamExt(m_vData.data(), static_cast<unsigned long>(m_vData.size()), false);
}

char *CChunkyFile::CChunk::GetDataRaw() { return m_vData.data(); }

unsigned long CChunkyFile::CChunk::GetDataLength() const { return static_cast<unsigned long>(m_vData.size()); }

void CChunkyFile::CChunk::SetVersion(long iValue) { m_iVersion = iValue; }

void CChunkyFile::CChunk::SetUnknown1(long iValue) { m_iUnknown1 = iValue; }

void CChunkyFile::CChunk::SetDescriptor(const char *sValue) { m_sDescriptor = sValue; }

void CChunkyFile::CChunk::SetData(CMemoryStore::COutStream *pStream)
{
    auto iLen = pStream->GetDataLength();
    m_vData.assign(pStream->GetData(), pStream->GetData() + iLen);
    m_iDataLength = static_cast<unsigned long>(iLen);
}
