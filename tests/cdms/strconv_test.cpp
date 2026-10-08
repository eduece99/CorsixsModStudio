#include <gtest/gtest.h>
#include <wx/init.h>
#include "common/strconv.h"
#include <cstring>
#include <cstdlib>

class StrconvTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() { wxInitialize(); }
    static void TearDownTestSuite() { wxUninitialize(); }
};

TEST_F(StrconvTest, AsciiToUnicodeNormalString) {
    wchar_t* result = AsciiToUnicode("hello");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"hello");
    delete[] result;
}

TEST_F(StrconvTest, AsciiToUnicodeEmptyString) {
    wchar_t* result = AsciiToUnicode("");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"");
    delete[] result;
}

TEST_F(StrconvTest, UnicodeToAsciiNormalString) {
    char* result = UnicodeToAscii(L"hello");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello");
    delete[] result;
}

TEST_F(StrconvTest, UnicodeToAsciiOutOfRangeBecomesQuestionMark) {
    // Characters > 0xFF should become '?'
    wchar_t input[] = { L'A', 0x0100, L'B', 0x00 };
    char* result = UnicodeToAscii(input);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "A?B");
    delete[] result;
}

TEST_F(StrconvTest, UnicodeToAsciiEmptyString) {
    char* result = UnicodeToAscii(L"");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "");
    delete[] result;
}

TEST_F(StrconvTest, AsciiToUnicodeDelConvertsAndDeletesSource) {
    char* source = new char[6];
    std::memcpy(source, "world", 6);
    wchar_t* result = AsciiToUnicodeDel(source);
    // source is now deleted by the function
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"world");
    delete[] result;
}

TEST_F(StrconvTest, AsciiToUnicodeFreeConvertsAndFreesSource) {
    char* source = static_cast<char*>(std::malloc(4));
    std::memcpy(source, "abc", 4);
    wchar_t* result = AsciiToUnicodeFree(source);
    // source is now freed by the function
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"abc");
    delete[] result;
}

TEST_F(StrconvTest, AsciiTowxStringNormalConversion) {
    wxString result = AsciiTowxString("test");
    EXPECT_EQ(result, wxT("test"));
}

TEST_F(StrconvTest, AsciiTowxStringNullReturnsEmpty) {
    wxString result = AsciiTowxString(nullptr);
    EXPECT_TRUE(result.IsEmpty());
}

TEST_F(StrconvTest, wxStringToAsciiRoundTrip) {
    wxString input(wxT("round trip"));
    auto result = wxStringToAscii(input);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result.get(), "round trip");
}

TEST_F(StrconvTest, AsciiToUnicodeHighAsciiRange) {
    // Characters 0x80-0xFF should be preserved as wchar_t values
    char input[] = { (char)0x80, (char)0xC0, (char)0xFF, 0x00 };
    wchar_t* result = AsciiToUnicode(input);
    ASSERT_NE(result, nullptr);
    // char is signed on MSVC, so (char)0x80 sign-extends to (wchar_t)(signed char)0x80
    // The function does pUnicode[i] = (wchar_t)sAscii[i], which sign-extends
    EXPECT_EQ(result[2], (wchar_t)(char)0xFF);
    EXPECT_EQ(result[3], L'\0');
    delete[] result;
}

TEST_F(StrconvTest, UnicodeToAsciiBoundaryFF) {
    // 0xFF is within the ASCII-extended range and should pass through
    wchar_t input[] = { 0x00FF, 0x0000 };
    char* result = UnicodeToAscii(input);
    ASSERT_NE(result, nullptr);
    // 0xFF has no bits set outside 0xFF, so it should NOT become '?'
    EXPECT_EQ((unsigned char)result[0], 0xFF);
    delete[] result;
}

TEST_F(StrconvTest, UnicodeToAsciiAllHighChars) {
    wchar_t input[] = { 0x0100, 0x4E2D, 0xFFFF, 0x0000 };
    char* result = UnicodeToAscii(input);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "???");
    delete[] result;
}

TEST_F(StrconvTest, AsciiTowxStringEmptyString) {
    wxString result = AsciiTowxString("");
    EXPECT_TRUE(result.IsEmpty());
}

TEST_F(StrconvTest, wxStringToAsciiEmptyString) {
    wxString input(wxT(""));
    auto result = wxStringToAscii(input);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result.get(), "");
}

TEST_F(StrconvTest, wxStringToAsciiUnicodeContent) {
    // wxString with a character > 0xFF should become '?'
    wxString input;
    input += (wchar_t)0x4E2D; // Chinese character
    input += wxT("A");
    auto result = wxStringToAscii(input);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result.get()[0], '?');
    EXPECT_EQ(result.get()[1], 'A');
}
