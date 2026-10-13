#include <gtest/gtest.h>
#include "rainman/formats/CRgmFile.h"
#include "rainman/io/CMemoryStore.h"
#include <memory>
#include "rainman/core/Exception.h"
#include <cstring>
#include <cstdint>
#include <vector>
#include <optional>

// Helper to build binary data for a material INFO chunk (DxName)
static CMemoryStore::COutStream *BuildMaterialInfoData(const char *sDxName)
{
	auto *pData = CMemoryStore::OpenOutputStreamExt();
	const uint32_t iLen = static_cast<uint32_t>(strlen(sDxName));
	pData->VWrite(1, sizeof(iLen), &iLen);
	pData->VWrite(iLen, 1, sDxName);
	return pData;
}

// Helper to build a CChunkyFile with material structure (no variables)
// Returns a CMemoryStore output stream containing the serialized chunky data.
static CMemoryStore::COutStream *BuildRgmChunky(
    const char *sMaterialName, const char *sDxName,
    const char *sMaterialName2 = nullptr, const char *sDxName2 = nullptr)
{
	CChunkyFile chunky;
	chunky.New(3);

	auto *pModl = chunky.AppendNew("MODL", CChunkyFile::CChunk::T_Folder);
	pModl->SetVersion(1);

	auto *pMtrl = pModl->AppendNew("MTRL", CChunkyFile::CChunk::T_Folder);
	pMtrl->SetVersion(1);
	pMtrl->SetDescriptor(sMaterialName);

	auto *pInfo = pMtrl->AppendNew("INFO", CChunkyFile::CChunk::T_Data);
	pInfo->SetVersion(1);
	pInfo->SetDescriptor("Material Info");
	auto *pInfoData = BuildMaterialInfoData(sDxName);
	pInfo->SetData(pInfoData);
	delete pInfoData;

	if (sMaterialName2)
	{
		auto *pMtrl2 = pModl->AppendNew("MTRL", CChunkyFile::CChunk::T_Folder);
		pMtrl2->SetVersion(1);
		pMtrl2->SetDescriptor(sMaterialName2);

		auto *pInfo2 = pMtrl2->AppendNew("INFO", CChunkyFile::CChunk::T_Data);
		pInfo2->SetVersion(1);
		pInfo2->SetDescriptor("Material Info");
		auto *pInfoData2 = BuildMaterialInfoData(sDxName2);
		pInfo2->SetData(pInfoData2);
		delete pInfoData2;
	}

	auto *pOut = CMemoryStore::OpenOutputStreamExt();
	chunky.Save(pOut);
	return pOut;
}

class RgmFileTest : public ::testing::Test {
protected:
	CRgmFile rgm;
};

TEST(RgmFileEmpty, DefaultConstructor)
{
	CRgmFile rgm;
	EXPECT_EQ(rgm.GetMaterialCount(), 0u);
}

TEST_F(RgmFileTest, LoadSingleMaterial)
{
	auto *pChunkyData = BuildRgmChunky("TestMaterial", "StandardShader");
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pChunkyData->GetData()), pChunkyData->GetDataLength(), false));

	rgm.Load(inStream.get());
	delete pChunkyData;

	ASSERT_EQ(rgm.GetMaterialCount(), 1u);
	auto *pMat = rgm.GetMaterial(0);
	EXPECT_STREQ(pMat->GetName(), "TestMaterial");
	EXPECT_STREQ(pMat->GetDxName(), "StandardShader");
	EXPECT_EQ(pMat->GetVariableCount(), 0u);
}

TEST_F(RgmFileTest, LoadMultipleMaterials)
{
	auto *pChunkyData = BuildRgmChunky("Material_A", "ShaderA", "Material_B", "ShaderB");
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pChunkyData->GetData()), pChunkyData->GetDataLength(), false));

	rgm.Load(inStream.get());
	delete pChunkyData;

	ASSERT_EQ(rgm.GetMaterialCount(), 2u);
	EXPECT_STREQ(rgm.GetMaterial(0)->GetName(), "Material_A");
	EXPECT_STREQ(rgm.GetMaterial(0)->GetDxName(), "ShaderA");
	EXPECT_STREQ(rgm.GetMaterial(1)->GetName(), "Material_B");
	EXPECT_STREQ(rgm.GetMaterial(1)->GetDxName(), "ShaderB");
}

TEST_F(RgmFileTest, SetMaterialName)
{
	auto *pChunkyData = BuildRgmChunky("OrigName", "OrigDx");
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pChunkyData->GetData()), pChunkyData->GetDataLength(), false));
	rgm.Load(inStream.get());
	delete pChunkyData;

	ASSERT_EQ(rgm.GetMaterialCount(), 1u);
	auto *pMat = rgm.GetMaterial(0);

	pMat->SetName("NewName");
	EXPECT_STREQ(pMat->GetName(), "NewName");

	pMat->SetDxName("NewDxName");
	EXPECT_STREQ(pMat->GetDxName(), "NewDxName");
}

TEST_F(RgmFileTest, SaveAndReloadRoundTrip)
{
	// Load initial data
	auto *pChunkyData = BuildRgmChunky("RoundTrip", "DxShader");
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pChunkyData->GetData()), pChunkyData->GetDataLength(), false));
	rgm.Load(inStream.get());
	delete pChunkyData;

	// Modify a property to prove save captures changes
	rgm.GetMaterial(0)->SetDxName("ModifiedDx");

	// Save
	auto *pSaveStream = CMemoryStore::OpenOutputStreamExt();
	rgm.Save(pSaveStream);
	EXPECT_GT(pSaveStream->GetDataLength(), 0u);

	// Reload into a new CRgmFile
	CRgmFile rgm2;
	std::unique_ptr<IFileStore::IStream> reloadStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pSaveStream->GetData()), pSaveStream->GetDataLength(), false));
	rgm2.Load(reloadStream.get());

	ASSERT_EQ(rgm2.GetMaterialCount(), 1u);
	EXPECT_STREQ(rgm2.GetMaterial(0)->GetName(), "RoundTrip");
	EXPECT_STREQ(rgm2.GetMaterial(0)->GetDxName(), "ModifiedDx");
	delete pSaveStream;
}

TEST_F(RgmFileTest, MultipleMaterialsSaveAndReload)
{
	auto *pChunkyData = BuildRgmChunky("MatAlpha", "DxAlpha", "MatBeta", "DxBeta");
	std::unique_ptr<IFileStore::IStream> inStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pChunkyData->GetData()), pChunkyData->GetDataLength(), false));
	rgm.Load(inStream.get());
	delete pChunkyData;

	// Save
	auto *pSaveStream = CMemoryStore::OpenOutputStreamExt();
	rgm.Save(pSaveStream);

	// Reload
	CRgmFile rgm2;
	std::unique_ptr<IFileStore::IStream> reloadStream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(pSaveStream->GetData()), pSaveStream->GetDataLength(), false));
	rgm2.Load(reloadStream.get());

	ASSERT_EQ(rgm2.GetMaterialCount(), 2u);
	EXPECT_STREQ(rgm2.GetMaterial(0)->GetName(), "MatAlpha");
	EXPECT_STREQ(rgm2.GetMaterial(0)->GetDxName(), "DxAlpha");
	EXPECT_STREQ(rgm2.GetMaterial(1)->GetName(), "MatBeta");
	EXPECT_STREQ(rgm2.GetMaterial(1)->GetDxName(), "DxBeta");
	delete pSaveStream;
}

TEST_F(RgmFileTest, LoadInvalidDataThrows)
{
	const char garbage[] = "This is not a chunky file";
	std::unique_ptr<IFileStore::IStream> stream(CMemoryStore::OpenStreamExt(
	    const_cast<char *>(garbage), sizeof(garbage) - 1, false));

	std::optional<CRainmanException> caught;
	try
	{
		rgm.Load(stream.get());
	}
	catch (const CRainmanException &ex)
	{
		caught = ex;
	}
	ASSERT_TRUE(caught.has_value());
}
