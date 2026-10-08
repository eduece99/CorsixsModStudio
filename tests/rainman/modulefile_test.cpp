#include <gtest/gtest.h>
#include "rainman/module/CModuleFile.h"
#include "rainman/localization/CUcsFile.h"
#include "rainman/core/Exception.h"
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include "../common/TestProcessId.h"
#include <optional>

class ModuleFileTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() /
            ("mod_test_" + std::to_string(GetTestProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::string writeModuleFile(const std::string& name, const std::string& content) {
        auto path = (tempDir / name).string();
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
        ofs.close();
        return path;
    }
};

TEST_F(ModuleFileTest, DefaultConstructor) {
    CModuleFile mod;
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
}

TEST_F(ModuleFileTest, SetAndGetLocale) {
    CModuleFile mod;
    mod.SetLocale("English");
    EXPECT_STREQ(mod.GetLocale(), "English");

    mod.SetLocale("French");
    EXPECT_STREQ(mod.GetLocale(), "French");
}

TEST_F(ModuleFileTest, LoadNonexistentFileThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.LoadModuleFile("C:\\nonexistent\\path\\fake.module");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, LoadModuleFileNormalizesWindowsSeparatorsOnLinux) {
    const auto nativePath = writeModuleFile("separator.module", "");
    auto modulePath = nativePath;
#ifdef RAINMAN_GNUC
    std::replace(modulePath.begin(), modulePath.end(), '/', '\\');
#endif

    CModuleFile mod;
    mod.LoadModuleFile(modulePath.c_str());

    auto expectedApplicationPath = tempDir.string();
    expectedApplicationPath.push_back(std::filesystem::path::preferred_separator);
    EXPECT_STREQ(mod.GetApplicationPath(), expectedApplicationPath.c_str());
}

TEST_F(ModuleFileTest, LoadEmptyFileClassifiesAsCohEarly) {
    // An empty .module file gets classified based on default heuristics
    // (no data sources and no folders/archives/required = CohEarly wins)
    auto path = writeModuleFile("empty.module", "");

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroesEarly);
}

TEST_F(ModuleFileTest, LoadDawnOfWarModule) {
    std::string content =
        "[global]\r\n"
        "UIName = Test Mod\r\n"
        "Description = A test mod\r\n"
        "DllName = TestDll\r\n"
        "ModFolder = TestFolder\r\n"
        "TextureFE = texturefe\r\n"
        "TextureIcon = textureicon\r\n"
        "ModVersion = 2.1.3\r\n"
        "DataFolder.1 = DataFolder1\r\n"
        "DataFolder.2 = DataFolder2\r\n"
        "ArchiveFile.1 = archive1\r\n"
        "RequiredMod.1 = RequiredBase\r\n"
        "CompatableMod.1 = CompatMod1\r\n";

    auto path = writeModuleFile("test.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_STREQ(mod.GetUiName(), "Test Mod");
    EXPECT_STREQ(mod.GetDescription(), "A test mod");
    EXPECT_STREQ(mod.GetDllName(), "TestDll");
    EXPECT_STREQ(mod.GetModFolder(), "TestFolder");
    EXPECT_STREQ(mod.GetTextureFe(), "texturefe");
    EXPECT_STREQ(mod.GetTextureIcon(), "textureicon");
    EXPECT_EQ(mod.GetVersionMajor(), 2);
    EXPECT_EQ(mod.GetVersionMinor(), 1);
    EXPECT_EQ(mod.GetVersionRevision(), 3);
    EXPECT_EQ(mod.GetFolderCount(), 2u);
    EXPECT_EQ(mod.GetArchiveCount(), 1u);
    EXPECT_EQ(mod.GetRequiredCount(), 1u);
    EXPECT_EQ(mod.GetCompatibleCount(), 1u);
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
}

TEST_F(ModuleFileTest, LoadDawnOfWarFolderDetails) {
    std::string content =
        "[global]\r\n"
        "UIName = FolderTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = MyData\r\n"
        "DataFolder.5 = OtherData\r\n";

    auto path = writeModuleFile("folders.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetFolderCount(), 2u);
    auto* f0 = mod.GetFolder(0);
    EXPECT_STREQ(f0->GetName(), "MyData");
    EXPECT_EQ(f0->GetNumber(), 1);

    auto* f1 = mod.GetFolder(1);
    EXPECT_STREQ(f1->GetName(), "OtherData");
    EXPECT_EQ(f1->GetNumber(), 5);
}

TEST_F(ModuleFileTest, LoadArchiveAppendsSga) {
    std::string content =
        "[global]\r\n"
        "UIName = ArchTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "ArchiveFile.1 = W40k\r\n";

    auto path = writeModuleFile("arch.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetArchiveCount(), 1u);
    // ArchiveFile values get ".sga" appended
    EXPECT_STREQ(mod.GetArchive(0)->GetFileName(), "W40k.sga");
}

TEST_F(ModuleFileTest, LoadRequiredMod) {
    std::string content =
        "[global]\r\n"
        "UIName = ReqTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "RequiredMod.1 = W40k\r\n"
        "RequiredMod.2 = WXP\r\n";

    auto path = writeModuleFile("req.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetRequiredCount(), 2u);
    EXPECT_STREQ(mod.GetRequired(0)->GetFileName(), "W40k");
    EXPECT_STREQ(mod.GetRequired(1)->GetFileName(), "WXP");
}

TEST_F(ModuleFileTest, VersionParsingMajorOnly) {
    std::string content =
        "[global]\r\n"
        "UIName = Test\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "ModVersion = 5\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("ver.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetVersionMajor(), 5);
    EXPECT_EQ(mod.GetVersionMinor(), 0);
    EXPECT_EQ(mod.GetVersionRevision(), 0);
}

TEST_F(ModuleFileTest, VersionParsingMajorMinor) {
    std::string content =
        "[global]\r\n"
        "UIName = Test\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "ModVersion = 3.7\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("ver2.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetVersionMajor(), 3);
    EXPECT_EQ(mod.GetVersionMinor(), 7);
    EXPECT_EQ(mod.GetVersionRevision(), 0);
}

TEST_F(ModuleFileTest, SgaOutputVersionDoW) {
    std::string content =
        "[global]\r\n"
        "UIName = Test\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("dow.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
    EXPECT_EQ(mod.GetSgaOutputVersion(), 2);
}

TEST_F(ModuleFileTest, CommentsAreIgnored) {
    std::string content =
        "[global]\r\n"
        "; This is a comment\r\n"
        "UIName = Commented Mod\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("comment.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_STREQ(mod.GetUiName(), "Commented Mod");
}

TEST_F(ModuleFileTest, FileMapNameFromFilename) {
    std::string content =
        "[global]\r\n"
        "UIName = MapTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("mymod.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    // FileMapName is derived from the filename without extension
    EXPECT_STREQ(mod.GetFileMapName(), "mymod");
}

TEST_F(ModuleFileTest, SettersWorkAfterLoad) {
    std::string content =
        "[global]\r\n"
        "UIName = Original\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("set.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    mod.SetName("NewName");
    EXPECT_STREQ(mod.GetName(), "NewName");

    mod.SetDescription("New desc");
    EXPECT_STREQ(mod.GetDescription(), "New desc");

    mod.SetVersionMajor(9);
    EXPECT_EQ(mod.GetVersionMajor(), 9);
}

// ==========================================================================
// T1 — CoH Module Parsing Tests
// ==========================================================================

TEST_F(ModuleFileTest, LoadCompanyOfHeroesModule) {
    // CoH module: has LocFolder, ScenarioPackFolder, data sources,
    // and Name (not UIName) — heuristic should classify as MT_CompanyOfHeroes
    std::string content =
        "[global]\r\n"
        "Name = CoH Test Mod\r\n"
        "Description = A CoH mod\r\n"
        "DllName = CohDll\r\n"
        "ModFolder = CoHFolder\r\n"
        "LocFolder = Locale\r\n"
        "ScenarioPackFolder = Scenarios\r\n"
        "ModVersion = 1.2\r\n"
        "[data:common:0]\r\n"
        "folder = CoHFolder\\Data\r\n"
        "archive.1 = CoHArchive\r\n"
        "[engine:common:1]\r\n"
        "folder = Engine\\Data\r\n";

    auto path = writeModuleFile("coh.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroes);
    EXPECT_STREQ(mod.GetName(), "CoH Test Mod");
    EXPECT_STREQ(mod.GetDescription(), "A CoH mod");
    EXPECT_STREQ(mod.GetDllName(), "CohDll");
    EXPECT_STREQ(mod.GetModFolder(), "CoHFolder");
    EXPECT_STREQ(mod.GetLocaleFolder(), "Locale");
    EXPECT_EQ(mod.GetVersionMajor(), 1);
    EXPECT_EQ(mod.GetVersionMinor(), 2);
    EXPECT_EQ(mod.GetDataSourceCount(), 2u);
}

TEST_F(ModuleFileTest, LoadCohDataSourceParsing) {
    // Verify the internal structure of parsed data sources
    std::string content =
        "[global]\r\n"
        "Name = DSTest\r\n"
        "ModFolder = DSMod\r\n"
        "LocFolder = Locale\r\n"
        "ScenarioPackFolder = Scenarios\r\n"
        "[data:common:0]\r\n"
        "folder = DataFolder\r\n"
        "archive.1 = Arch1\r\n"
        "archive.2 = Arch2\r\n"
        "[art:normals:5]\r\n"
        "folder = ArtFolder\r\n";

    auto path = writeModuleFile("ds.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroes);
    ASSERT_EQ(mod.GetDataSourceCount(), 2u);

    // First data source: [data:common:0]
    auto* ds0 = mod.GetDataSource(0);
    ASSERT_NE(ds0, nullptr);
    EXPECT_STREQ(ds0->GetToc(), "data");
    EXPECT_STREQ(ds0->GetOption(), "common");
    EXPECT_EQ(ds0->GetNumber(), 0);
    EXPECT_STREQ(ds0->GetFolder(), "DataFolder");
    ASSERT_EQ(ds0->GetArchiveCount(), 2u);
    EXPECT_STREQ(ds0->GetArchive(0)->GetFileName(), "Arch1.sga");
    EXPECT_EQ(ds0->GetArchive(0)->GetNumber(), 1);
    EXPECT_STREQ(ds0->GetArchive(1)->GetFileName(), "Arch2.sga");
    EXPECT_EQ(ds0->GetArchive(1)->GetNumber(), 2);

    // Second data source: [art:normals:5]
    auto* ds1 = mod.GetDataSource(1);
    ASSERT_NE(ds1, nullptr);
    EXPECT_STREQ(ds1->GetToc(), "art");
    EXPECT_STREQ(ds1->GetOption(), "normals");
    EXPECT_EQ(ds1->GetNumber(), 5);
    EXPECT_STREQ(ds1->GetFolder(), "ArtFolder");
    EXPECT_EQ(ds1->GetArchiveCount(), 0u);
}

TEST_F(ModuleFileTest, LoadCohEarlyModule) {
    // CohEarly: has PatcherUrl, no data sources, no DoW-specific fields
    std::string content =
        "[global]\r\n"
        "Name = Early Beta\r\n"
        "Description = Pre-release\r\n"
        "DllName = EarlyDll\r\n"
        "ModFolder = EarlyFolder\r\n"
        "PatcherUrl = http://example.com/patch\r\n"
        "ModVersion = 0.9.1\r\n";

    auto path = writeModuleFile("early.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroesEarly);
    EXPECT_STREQ(mod.GetName(), "Early Beta");
    EXPECT_STREQ(mod.GetDescription(), "Pre-release");
    EXPECT_EQ(mod.GetVersionMajor(), 0);
    EXPECT_EQ(mod.GetVersionMinor(), 9);
    EXPECT_EQ(mod.GetVersionRevision(), 1);
}

TEST_F(ModuleFileTest, LoadCohDataSourceWithoutFolder) {
    // A data source section with archives but no folder directive
    std::string content =
        "[global]\r\n"
        "Name = NoFolder\r\n"
        "ModFolder = NFMod\r\n"
        "LocFolder = Locale\r\n"
        "ScenarioPackFolder = Scenarios\r\n"
        "[data:common:0]\r\n"
        "archive.1 = OnlyArchive\r\n";

    auto path = writeModuleFile("nofolder.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    ASSERT_EQ(mod.GetDataSourceCount(), 1u);
    auto* ds = mod.GetDataSource(0);
    // Folder was never set, should be empty
    EXPECT_STREQ(ds->GetFolder(), "");
    EXPECT_EQ(ds->GetArchiveCount(), 1u);
}

// ==========================================================================
// T3 — Create() Factory Tests (declared but unimplemented — verify linker)
// Note: Create(), NewFolder, DeleteFolder, NewArchive, DeleteArchive,
// NewRequired, DeleteRequired, NewCompatible, DeleteCompatible,
// NewDataSource, DeleteDataSource are declared in CModuleFile.h but
// have no implementation in CModuleFile.cpp. They are dead declarations
// with no callers. Testing is skipped — see T5 note below.
// ==========================================================================

// ==========================================================================
// T4 — UCS Resolution Tests
// ==========================================================================

TEST_F(ModuleFileTest, NewUcsAddsHandler) {
    CModuleFile mod;
    EXPECT_EQ(mod.GetUcsCount(), 0u);

    mod.NewUCS("test.ucs");
    EXPECT_EQ(mod.GetUcsCount(), 1u);

    auto* handler = mod.GetUcs(0);
    ASSERT_NE(handler, nullptr);
    EXPECT_STREQ(handler->GetFileName(), "test.ucs");
    // Handle should be null since no CUcsFile was provided
    EXPECT_EQ(handler->GetUcsHandle(), nullptr);
}

TEST_F(ModuleFileTest, NewUcsWithHandle) {
    CModuleFile mod;

    // Create a UCS file with a string entry
    auto pUcs = std::make_shared<CUcsFile>();
    pUcs->New();
    const wchar_t* testStr = L"Hello World";
    pUcs->SetString(42, testStr);

    mod.NewUCS("strings.ucs", pUcs);
    EXPECT_EQ(mod.GetUcsCount(), 1u);

    auto* handler = mod.GetUcs(0);
    ASSERT_NE(handler, nullptr);
    ASSERT_NE(handler->GetUcsHandle(), nullptr);
}

TEST_F(ModuleFileTest, ResolveUcsById) {
    CModuleFile mod;

    auto pUcs = std::make_shared<CUcsFile>();
    pUcs->New();
    const wchar_t* expected = L"Test String";
    pUcs->SetString(100, expected);

    mod.NewUCS("test.ucs", pUcs);

    const wchar_t* result = mod.ResolveUCS((unsigned long)100);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, expected);
}

TEST_F(ModuleFileTest, ResolveUcsReturnsNullForMissingId) {
    CModuleFile mod;

    auto pUcs = std::make_shared<CUcsFile>();
    pUcs->New();
    pUcs->SetString(1, L"exists");

    mod.NewUCS("test.ucs", pUcs);

    const wchar_t* result = mod.ResolveUCS((unsigned long)999);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ModuleFileTest, ResolveUcsByCharDollarString) {
    CModuleFile mod;

    auto pUcs = std::make_shared<CUcsFile>();
    pUcs->New();
    pUcs->SetString(200, L"Dollar Result");

    mod.NewUCS("test.ucs", pUcs);

    const wchar_t* result = mod.ResolveUCS("$200");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"Dollar Result");
}

TEST_F(ModuleFileTest, ResolveUcsByWcharDollarString) {
    CModuleFile mod;

    auto pUcs = std::make_shared<CUcsFile>();
    pUcs->New();
    pUcs->SetString(300, L"Wide Dollar");

    mod.NewUCS("test.ucs", pUcs);

    const wchar_t* result = mod.ResolveUCS(L"$300");
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, L"Wide Dollar");
}

TEST_F(ModuleFileTest, ResolveUcsNonDollarStringThrows) {
    CModuleFile mod;
    mod.NewUCS("test.ucs");

    std::optional<CRainmanException> caught;
    try {
        mod.ResolveUCS("notdollar");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, ResolveUcsWcharNonDollarStringThrows) {
    CModuleFile mod;
    mod.NewUCS("test.ucs");

    std::optional<CRainmanException> caught;
    try {
        mod.ResolveUCS(L"notdollar");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, ResolveUcsMultipleFiles) {
    CModuleFile mod;

    // First UCS doesn't have the ID
    auto pUcs1 = std::make_shared<CUcsFile>();
    pUcs1->New();
    pUcs1->SetString(1, L"First File");

    // Second UCS has the ID
    auto pUcs2 = std::make_shared<CUcsFile>();
    pUcs2->New();
    pUcs2->SetString(2, L"Second File");

    mod.NewUCS("first.ucs", pUcs1);
    mod.NewUCS("second.ucs", pUcs2);

    // Should find ID 1 in first file
    const wchar_t* r1 = mod.ResolveUCS((unsigned long)1);
    ASSERT_NE(r1, nullptr);
    EXPECT_STREQ(r1, L"First File");

    // Should find ID 2 in second file
    const wchar_t* r2 = mod.ResolveUCS((unsigned long)2);
    ASSERT_NE(r2, nullptr);
    EXPECT_STREQ(r2, L"Second File");
}

// ==========================================================================
// T5 — Collection Mutation Tests (only implemented methods)
// Note: NewFolder, DeleteFolder, NewArchive, DeleteArchive, NewRequired,
// DeleteRequired, NewCompatible, DeleteCompatible, NewDataSource,
// DeleteDataSource are declared but NOT implemented (dead code).
// Only NewUCS, DeleteUcs, GetUcs are implemented.
// ==========================================================================

TEST_F(ModuleFileTest, DeleteUcsRemovesHandler) {
    CModuleFile mod;

    mod.NewUCS("a.ucs");
    mod.NewUCS("b.ucs");
    mod.NewUCS("c.ucs");
    ASSERT_EQ(mod.GetUcsCount(), 3u);

    mod.DeleteUcs(1); // Remove "b.ucs"
    EXPECT_EQ(mod.GetUcsCount(), 2u);
    EXPECT_STREQ(mod.GetUcs(0)->GetFileName(), "a.ucs");
    EXPECT_STREQ(mod.GetUcs(1)->GetFileName(), "c.ucs");
}

TEST_F(ModuleFileTest, DeleteUcsOutOfRangeThrows) {
    CModuleFile mod;
    mod.NewUCS("only.ucs");

    std::optional<CRainmanException> caught;
    try {
        mod.DeleteUcs(5);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, GetUcsOutOfRangeThrows) {
    CModuleFile mod;

    std::optional<CRainmanException> caught;
    try {
        mod.GetUcs(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, GetEngineOutOfRangeThrows) {
    CModuleFile mod;

    std::optional<CRainmanException> caught;
    try {
        mod.GetEngine(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

// ==========================================================================
// T6 — GetArchiveFullPath (exercises _DawnOfWarRemoveDynamics indirectly)
// ==========================================================================

TEST_F(ModuleFileTest, GetArchiveFullPathDawnOfWar) {
    std::string content =
        "[global]\r\n"
        "UIName = PathTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "ModFolder = MyMod\r\n"
        "ArchiveFile.1 = W40k\r\n";

    auto path = writeModuleFile("pathtest.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    ASSERT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
    ASSERT_EQ(mod.GetArchiveCount(), 1u);

    // First call with nullptr to get the length
    size_t len = mod.GetArchiveFullPath(0, nullptr);
    EXPECT_GT(len, 0u);

    // Second call to get the actual path
    std::vector<char> buf(len + 1);
    mod.GetArchiveFullPath(0, buf.data());

    std::string fullPath(buf.data());
    // DoW format: {applicationPath}{modFolder}\{archiveName}
    // Archive name has .sga appended during parsing
    EXPECT_NE(fullPath.find("MyMod"), std::string::npos);
    EXPECT_NE(fullPath.find("W40k.sga"), std::string::npos);
}

TEST_F(ModuleFileTest, GetArchiveFullPathWithLocaleExpansion) {
    // Archive name contains %LOCALE% which _DawnOfWarRemoveDynamics expands
    std::string content =
        "[global]\r\n"
        "UIName = LocaleTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "ModFolder = TestMod\r\n"
        "ArchiveFile.1 = %LOCALE%\\W40kLocale\r\n";

    auto path = writeModuleFile("localetest.module", content);

    CModuleFile mod;
    mod.SetLocale("English");
    mod.LoadModuleFile(path.c_str());

    ASSERT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
    ASSERT_EQ(mod.GetArchiveCount(), 1u);

    std::vector<char> buf(512);
    mod.GetArchiveFullPath(0, buf.data());

    std::string fullPath(buf.data());
    // %LOCALE% should expand to Locale\English
#ifdef RAINMAN_GNUC
    EXPECT_NE(fullPath.find("Locale/English"), std::string::npos);
#else
    EXPECT_NE(fullPath.find("Locale\\English"), std::string::npos);
#endif
    EXPECT_NE(fullPath.find("W40kLocale.sga"), std::string::npos);
    // %LOCALE% should NOT appear in the output
    EXPECT_EQ(fullPath.find("%LOCALE%"), std::string::npos);
}

TEST_F(ModuleFileTest, GetArchiveFullPathCohEarly) {
    // CohEarly format: {appPath}{modFolder}\Archives\{archiveName}
    std::string content =
        "[global]\r\n"
        "Name = CohEarlyPath\r\n"
        "PatcherUrl = http://example.com\r\n"
        "ModFolder = EarlyMod\r\n"
        "ArchiveFile.1 = TestArch\r\n";

    auto path = writeModuleFile("earlypath.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    ASSERT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroesEarly);
    ASSERT_EQ(mod.GetArchiveCount(), 1u);

    std::vector<char> buf(512);
    mod.GetArchiveFullPath(0, buf.data());

    std::string fullPath(buf.data());
    EXPECT_NE(fullPath.find("EarlyMod"), std::string::npos);
    EXPECT_NE(fullPath.find("Archives"), std::string::npos);
    EXPECT_NE(fullPath.find("TestArch.sga"), std::string::npos);
}

TEST_F(ModuleFileTest, GetArchiveFullPathOutOfRangeThrows) {
    std::string content =
        "[global]\r\n"
        "UIName = Test\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("noarch.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    std::optional<CRainmanException> caught;
    try {
        mod.GetArchiveFullPath(0, nullptr);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

// ==========================================================================
// T7 — Heuristic Edge Cases
// ==========================================================================

TEST_F(ModuleFileTest, HeuristicTiedScoreThrows) {
    // PatcherUrl (+3 CohEarly) + DataFolder.1 (+3 DoW) + no data sources (+5 DoW, +5 CohEarly)
    // + folders exist (no CoH/CohEarly bonus)
    // DoW: 3+5=8, CohEarly: 3+5=8, CoH: 0 → tie → exception
    std::string content =
        "[global]\r\n"
        "PatcherUrl = http://example.com\r\n"
        "DataFolder.1 = SomeData\r\n";

    auto path = writeModuleFile("tied.module", content);

    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.LoadModuleFile(path.c_str());
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
    EXPECT_NE(std::strstr(caught->getMessage(), "Unable to determine"), nullptr);
}

TEST_F(ModuleFileTest, HeuristicMixedDirectivesDowWins) {
    // UIName (+3 DoW), Name (+1 CoH, +1 CohEarly), Description (+1 each),
    // TextureFE (+3 DoW), TextureIcon (+3 DoW),
    // DataFolder.1 (+3 DoW), ArchiveFile.1 (+3 DoW)
    // No DataSources: +5 DoW, +5 CohEarly
    // Folders/Archives exist: no CoH/CohEarly bonus
    // DoW: 3+1+3+3+3+3+5=21, CoH: 1+1=2, CohEarly: 1+1+5=7
    std::string content =
        "[global]\r\n"
        "UIName = Mixed\r\n"
        "Name = AlsoMixed\r\n"
        "Description = Both\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n"
        "ArchiveFile.1 = Arch\r\n";

    auto path = writeModuleFile("mixed.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    // DoW should win with mixed directives when it has strong signals
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_DawnOfWar);
}

TEST_F(ModuleFileTest, HeuristicAllZeroClassifiesAsEmpty) {
    // A file with only an unknown section header and no key=value pairs.
    // Empty-collection bonuses always produce non-zero scores, so the
    // heuristic picks CohEarly (gets both no-datasources and no-folders bonuses).
    std::string content = "[somesection]\r\n";

    auto path = writeModuleFile("allzero.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());
    // With no content, CohEarly wins: no-datasources(+5) + no-folders-archives-requireds(+5) = 10
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroesEarly);
}

// ==========================================================================
// T8 — IFileStore / IDirectoryTraverser Virtual Tests
// ==========================================================================

TEST_F(ModuleFileTest, VGetEntryPointCountWithNoData) {
    CModuleFile mod;
    // CFileMap is lazy-initialized; calling VGetEntryPointCount creates it
    EXPECT_EQ(mod.VGetEntryPointCount(), 0u);
}

TEST_F(ModuleFileTest, VDirectoryExistsWithNoData) {
    CModuleFile mod;
    EXPECT_FALSE(mod.VDirectoryExists("nonexistent"));
}

TEST_F(ModuleFileTest, VDirectoryExistsEmptyPath) {
    CModuleFile mod;
    EXPECT_FALSE(mod.VDirectoryExists(""));
}

TEST_F(ModuleFileTest, VIterateEmptyThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.VIterate("");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    // Expect an error since there's no data loaded
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, VOpenStreamNonexistentThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.VOpenStream("nonexistent\\file.txt");
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

// ==========================================================================
// T9 — Module Settings Tests
// ==========================================================================

TEST_F(ModuleFileTest, SgaOutputVersionCoh) {
    std::string content =
        "[global]\r\n"
        "Name = CoH\r\n"
        "ModFolder = CohMod\r\n"
        "LocFolder = Locale\r\n"
        "ScenarioPackFolder = Scenarios\r\n"
        "[data:common:0]\r\n"
        "folder = Data\r\n";

    auto path = writeModuleFile("cohver.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroes);
    EXPECT_EQ(mod.GetSgaOutputVersion(), 4);
}

TEST_F(ModuleFileTest, SgaOutputVersionCohEarly) {
    std::string content =
        "[global]\r\n"
        "Name = EarlyVer\r\n"
        "PatcherUrl = http://example.com\r\n";

    auto path = writeModuleFile("earlyver.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());
    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroesEarly);
    EXPECT_EQ(mod.GetSgaOutputVersion(), 4);
}

TEST_F(ModuleFileTest, GetApplicationPathAfterLoad) {
    std::string content =
        "[global]\r\n"
        "UIName = PathMod\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n";

    auto path = writeModuleFile("apppath.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    const char* appPath = mod.GetApplicationPath();
    ASSERT_NE(appPath, nullptr);
    // ApplicationPath should be the directory containing the module file
    std::string appPathStr(appPath);
#ifdef RAINMAN_GNUC
    std::string expectedDir = tempDir.string() + "/";
#else
    std::string expectedDir = tempDir.string() + "\\";
#endif
    EXPECT_EQ(appPathStr, expectedDir);
}

TEST_F(ModuleFileTest, SetAndGetMapPackRootFolder) {
    CModuleFile mod;

    // Initially empty
    EXPECT_STREQ(mod.GetMapPackRootFolder(), L"");

    mod.SetMapPackRootFolder(L"C:\\Maps\\Scenarios");
    EXPECT_STREQ(mod.GetMapPackRootFolder(), L"C:\\Maps\\Scenarios");

    // Setting again should free old value
    mod.SetMapPackRootFolder(L"D:\\OtherMaps");
    EXPECT_STREQ(mod.GetMapPackRootFolder(), L"D:\\OtherMaps");
}

TEST_F(ModuleFileTest, GetEngineCountInitiallyZero) {
    CModuleFile mod;
    EXPECT_EQ(mod.GetEngineCount(), 0u);
}

TEST_F(ModuleFileTest, GetApplicationPathBeforeLoad) {
    CModuleFile mod;
    // Before loading, application path should be empty
    EXPECT_STREQ(mod.GetApplicationPath(), "");
}

TEST_F(ModuleFileTest, IsFauxModuleDefaultFalse) {
    CModuleFile mod;
    EXPECT_FALSE(mod.IsFauxModule());
}

TEST_F(ModuleFileTest, LoadModuleFileResetsPreviousState) {
    // Load one module, then load another — state should be fully reset
    std::string content1 =
        "[global]\r\n"
        "UIName = First Mod\r\n"
        "TextureFE = t1\r\n"
        "TextureIcon = t1\r\n"
        "DataFolder.1 = Data1\r\n"
        "DataFolder.2 = Data2\r\n"
        "ArchiveFile.1 = Arch1\r\n";

    std::string content2 =
        "[global]\r\n"
        "UIName = Second Mod\r\n"
        "TextureFE = t2\r\n"
        "TextureIcon = t2\r\n"
        "DataFolder.1 = DataX\r\n";

    auto path1 = writeModuleFile("first.module", content1);
    auto path2 = writeModuleFile("second.module", content2);

    CModuleFile mod;
    mod.LoadModuleFile(path1.c_str());
    EXPECT_STREQ(mod.GetUiName(), "First Mod");
    EXPECT_EQ(mod.GetFolderCount(), 2u);
    EXPECT_EQ(mod.GetArchiveCount(), 1u);

    // Load second module — previous state should be cleaned
    mod.LoadModuleFile(path2.c_str());
    EXPECT_STREQ(mod.GetUiName(), "Second Mod");
    EXPECT_EQ(mod.GetFolderCount(), 1u);
    EXPECT_EQ(mod.GetArchiveCount(), 0u);
}

TEST_F(ModuleFileTest, MultipleDataSourceSections) {
    // Verify multiple CoH data source sections are parsed independently
    std::string content =
        "[global]\r\n"
        "Name = MultiDS\r\n"
        "ModFolder = MultiMod\r\n"
        "LocFolder = Locale\r\n"
        "ScenarioPackFolder = Scenarios\r\n"
        "[data:common:0]\r\n"
        "folder = CommonData\r\n"
        "archive.1 = CommonArch\r\n"
        "[art:normals:1]\r\n"
        "folder = ArtData\r\n"
        "[sound:high:2]\r\n"
        "folder = SoundData\r\n"
        "archive.1 = SoundArch1\r\n"
        "archive.2 = SoundArch2\r\n";

    auto path = writeModuleFile("multids.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    EXPECT_EQ(mod.GetModuleType(), CModuleFile::MT_CompanyOfHeroes);
    ASSERT_EQ(mod.GetDataSourceCount(), 3u);

    EXPECT_STREQ(mod.GetDataSource(0)->GetToc(), "data");
    EXPECT_EQ(mod.GetDataSource(0)->GetArchiveCount(), 1u);

    EXPECT_STREQ(mod.GetDataSource(1)->GetToc(), "art");
    EXPECT_EQ(mod.GetDataSource(1)->GetArchiveCount(), 0u);

    EXPECT_STREQ(mod.GetDataSource(2)->GetToc(), "sound");
    EXPECT_EQ(mod.GetDataSource(2)->GetArchiveCount(), 2u);
}

TEST_F(ModuleFileTest, GetDataSourceOutOfRangeThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.GetDataSource(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, CompatibleModParsing) {
    std::string content =
        "[global]\r\n"
        "UIName = CompatTest\r\n"
        "TextureFE = t\r\n"
        "TextureIcon = t\r\n"
        "DataFolder.1 = Data\r\n"
        "CompatableMod.1 = Compat1\r\n"
        "CompatableMod.3 = Compat3\r\n"
        "CompatableMod.2 = Compat2\r\n";

    auto path = writeModuleFile("compat.module", content);

    CModuleFile mod;
    mod.LoadModuleFile(path.c_str());

    ASSERT_EQ(mod.GetCompatibleCount(), 3u);
    EXPECT_STREQ(mod.GetCompatible(0)->GetFileName(), "Compat1");
    EXPECT_STREQ(mod.GetCompatible(1)->GetFileName(), "Compat3");
    EXPECT_STREQ(mod.GetCompatible(2)->GetFileName(), "Compat2");
}

TEST_F(ModuleFileTest, GetCompatibleOutOfRangeThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.GetCompatible(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, GetFolderOutOfRangeThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.GetFolder(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, GetArchiveOutOfRangeThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.GetArchive(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}

TEST_F(ModuleFileTest, GetRequiredOutOfRangeThrows) {
    CModuleFile mod;
    std::optional<CRainmanException> caught;
    try {
        mod.GetRequired(0);
    } catch (const CRainmanException &ex) {
        caught = ex;
    }
    ASSERT_TRUE(caught.has_value());
}
