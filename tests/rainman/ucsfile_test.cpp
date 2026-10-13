#include <gtest/gtest.h>
#include "rainman/localization/CUcsFile.h"
#include "rainman/localization/CUcsMap.h"
#include "rainman/io/CMemoryStore.h"
#include "rainman/core/Exception.h"
#include <cstring>
#include <string>
#include <optional>

#define EXPECT_WSTREQ(expected, actual) \
    EXPECT_EQ(std::wstring(expected), std::wstring(actual))

class UcsFileTest : public ::testing::Test {
protected:
    CUcsFile ucs;
};

TEST_F(UcsFileTest, NewCreatesEmptyFile) {
    ucs.New();
    const auto &map = ucs.GetRawMap();
    EXPECT_TRUE(map.Empty());
}

TEST_F(UcsFileTest, SetStringAndResolve) {
    ucs.New();
    ucs.SetString(100, L"Hello World");

    const wchar_t* result = ucs.ResolveStringID(100);
    ASSERT_NE(result, nullptr);
    EXPECT_WSTREQ(L"Hello World", result);
}

TEST_F(UcsFileTest, ResolveNonexistentReturnsNull) {
    ucs.New();
    // Note: operator[] now uses find() and does not insert for missing keys
    const wchar_t* result = ucs.ResolveStringID(999);
    EXPECT_EQ(result, nullptr);
}

TEST_F(UcsFileTest, SetMultipleStrings) {
    ucs.New();
    ucs.SetString(1, L"First");
    ucs.SetString(2, L"Second");
    ucs.SetString(3, L"Third");

    const auto &map = ucs.GetRawMap();
    EXPECT_GE(map.Size(), 3u);

    EXPECT_WSTREQ(L"First", ucs.ResolveStringID(1));
    EXPECT_WSTREQ(L"Second", ucs.ResolveStringID(2));
    EXPECT_WSTREQ(L"Third", ucs.ResolveStringID(3));
}

TEST_F(UcsFileTest, OverwriteString) {
    ucs.New();
    ucs.SetString(1, L"Original");
    ucs.SetString(1, L"Updated");

    EXPECT_WSTREQ(L"Updated", ucs.ResolveStringID(1));
}

TEST_F(UcsFileTest, SetStringNullRemoves) {
    ucs.New();
    ucs.SetString(1, L"Hello");
    ucs.SetString(1, nullptr);

    // After setting null, entry should be erased
    const auto &map = ucs.GetRawMap();
    EXPECT_EQ(map.Find(1), map.end());
}

TEST_F(UcsFileTest, ReplaceStringTransfersOwnership) {
    ucs.New();

    auto owned = std::make_shared<wchar_t[]>(6);
    wcscpy(owned.get(), L"Hello");
    ucs.ReplaceString(1, std::move(owned));

    EXPECT_WSTREQ(L"Hello", ucs.ResolveStringID(1));
}

TEST_F(UcsFileTest, LoadUcsFromMemory) {
    ucs.New();

    // Build a minimal UTF-16LE UCS file in memory: BOM + "1\tHello\r\n"
    CMemoryStore store;
    store.VInit();
    auto* out = CMemoryStore::OpenOutputStreamExt();

    const uint16_t data[] = {0xFEFF, '1', '\t', 'H', 'e', 'l', 'l', 'o', '\r', '\n'};
    out->VWrite(sizeof(data) / sizeof(data[0]), sizeof(uint16_t), data);

    auto* inStream = CMemoryStore::OpenStreamExt(
        const_cast<char*>(out->GetData()), out->GetDataLength(), false);

    CUcsFile ucs2;
    ucs2.Load(inStream);

    const wchar_t* result = ucs2.ResolveStringID(1);
    ASSERT_NE(result, nullptr);
    EXPECT_WSTREQ(L"Hello", result);

    delete inStream;
    delete out;
}

TEST_F(UcsFileTest, LoadSkipsInvalidUnicodeEntries)
{
    CMemoryStore store;
    store.VInit();
    auto *out = CMemoryStore::OpenOutputStreamExt();

    const uint16_t data[] = {0xFEFF, '1', '\t', 'O', 'K', '\n',
                             '2',    '\t', 0xFFFF, '\n',
                             '3',    '\t', 0xD800, '\n',
                             '4',    '\t', 'Y', 'E', 'S', '\n',
                             '5',    '\t', 0xD83D, 0xDE00, '\n'};
    out->VWrite(sizeof(data) / sizeof(data[0]), sizeof(uint16_t), data);

    auto *inStream = CMemoryStore::OpenStreamExt(
        const_cast<char *>(out->GetData()), out->GetDataLength(), false);
    CUcsFile loaded;
    loaded.Load(inStream);

    EXPECT_STREQ(loaded.ResolveStringID(1), L"OK");
    EXPECT_EQ(loaded.ResolveStringID(2), nullptr);
    EXPECT_EQ(loaded.ResolveStringID(3), nullptr);
    EXPECT_STREQ(loaded.ResolveStringID(4), L"YES");
    const wchar_t *pSupplementaryCharacter = loaded.ResolveStringID(5);
    ASSERT_NE(pSupplementaryCharacter, nullptr);
    if constexpr (sizeof(wchar_t) > 2)
    {
        EXPECT_STREQ(pSupplementaryCharacter, L"\U0001F600");
    }
    else
    {
        const wchar_t expected[] = {static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00), 0};
        EXPECT_STREQ(pSupplementaryCharacter, expected);
    }

    delete inStream;
    delete out;
}

TEST_F(UcsFileTest, LoadInvalidHeaderThrows) {
    CMemoryStore store;
    store.VInit();
    auto* out = CMemoryStore::OpenOutputStreamExt();

    // Write an invalid BOM
    unsigned short badBom = 0x1234;
    out->VWrite(1, sizeof(unsigned short), &badBom);

    auto* inStream = CMemoryStore::OpenStreamExt(
        const_cast<char*>(out->GetData()), out->GetDataLength(), false);

    CUcsFile ucs2;
    std::optional<CRainmanException> caught;
    try {
        ucs2.Load(inStream);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
    delete inStream;
    delete out;
}

TEST_F(UcsFileTest, NewClearsExistingData) {
    ucs.New();
    ucs.SetString(1, L"First");
    ucs.SetString(2, L"Second");

    ucs.New(); // Should clear
    const auto &map = ucs.GetRawMap();
    EXPECT_TRUE(map.Empty());
}

TEST_F(UcsFileTest, IsDollarStringValid) {
    EXPECT_TRUE(CUcsFile::IsDollarString("$12345"));
    EXPECT_TRUE(CUcsFile::IsDollarString("$0"));
    EXPECT_TRUE(CUcsFile::IsDollarString("$999999"));
}

TEST_F(UcsFileTest, IsDollarStringInvalid) {
    EXPECT_FALSE(CUcsFile::IsDollarString("12345"));
    EXPECT_FALSE(CUcsFile::IsDollarString("$"));
    EXPECT_FALSE(CUcsFile::IsDollarString("$abc"));
    EXPECT_FALSE(CUcsFile::IsDollarString("$12a"));
    EXPECT_FALSE(CUcsFile::IsDollarString((const char*)nullptr));
    EXPECT_FALSE(CUcsFile::IsDollarString(""));
}

TEST_F(UcsFileTest, IsDollarStringWideValid) {
    EXPECT_TRUE(CUcsFile::IsDollarString(L"$12345"));
    EXPECT_TRUE(CUcsFile::IsDollarString(L"$0"));
}

TEST_F(UcsFileTest, IsDollarStringWideInvalid) {
    EXPECT_FALSE(CUcsFile::IsDollarString(L"12345"));
    EXPECT_FALSE(CUcsFile::IsDollarString(L"$"));
    EXPECT_FALSE(CUcsFile::IsDollarString(L"$abc"));
    EXPECT_FALSE(CUcsFile::IsDollarString((const wchar_t*)nullptr));
}

TEST_F(UcsFileTest, GetRawMapIsConsistent) {
    ucs.New();
    ucs.SetString(10, L"Ten");
    ucs.SetString(20, L"Twenty");

    const auto &map = ucs.GetRawMap();

    auto it10 = map.Find(10);
    ASSERT_NE(it10, map.end());
    EXPECT_WSTREQ(L"Ten", it10->second.get());

    auto it20 = map.Find(20);
    ASSERT_NE(it20, map.end());
    EXPECT_WSTREQ(L"Twenty", it20->second.get());
}
