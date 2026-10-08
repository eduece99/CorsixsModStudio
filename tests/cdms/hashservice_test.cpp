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

#include <gtest/gtest.h>
#include "services/HashService.h"
#include <rainman/formats/CRgdHashTable.h>
#include <filesystem>
#include <fstream>
#include "../common/TestProcessId.h"

class HashServiceTest : public ::testing::Test
{
  protected:
	HashService m_svc;
	std::filesystem::path m_tempDir;

	void SetUp() override
	{
		m_tempDir = std::filesystem::temp_directory_path() /
		            ("hashsvc_test_" + std::to_string(GetTestProcessId()) + "_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
		std::filesystem::create_directories(m_tempDir);

		// Create a minimal dictionary file
		auto dictPath = m_tempDir / "test_dict.txt";
		std::ofstream ofs(dictPath);
		ofs << "weapon_damage\n";
		ofs << "health_max\n";
		ofs.close();
	}

	void TearDown() override { std::filesystem::remove_all(m_tempDir); }
};

TEST_F(HashServiceTest, DefaultIsNotInitialized)
{
	EXPECT_FALSE(m_svc.IsInitialized());
	EXPECT_EQ(m_svc.GetHashTable(), nullptr);
}

TEST_F(HashServiceTest, InitializeWithValidPath)
{
	auto result = m_svc.Initialize(wxString::FromUTF8(m_tempDir.string()));
	ASSERT_TRUE(result.ok()) << result.error().utf8_str().data();
	EXPECT_TRUE(m_svc.IsInitialized());
	EXPECT_NE(m_svc.GetHashTable(), nullptr);
}

TEST_F(HashServiceTest, ValueToHashReturnsConsistentValue)
{
	auto result = m_svc.Initialize(wxString::FromUTF8(m_tempDir.string()));
	ASSERT_TRUE(result.ok());

	unsigned long h1 = m_svc.ValueToHash("weapon_damage");
	unsigned long h2 = m_svc.ValueToHash("weapon_damage");
	EXPECT_EQ(h1, h2);
	EXPECT_NE(h1, 0UL);
}

TEST_F(HashServiceTest, FillUnknownListStartsEmpty)
{
	auto result = m_svc.Initialize(wxString::FromUTF8(m_tempDir.string()));
	ASSERT_TRUE(result.ok());

	std::vector<unsigned long> unknowns;
	m_svc.FillUnknownList(unknowns);
	// After fresh init with a dictionary, there should be no unknowns
	EXPECT_TRUE(unknowns.empty());
}

TEST_F(HashServiceTest, ShutdownCleansUp)
{
	auto result = m_svc.Initialize(wxString::FromUTF8(m_tempDir.string()));
	ASSERT_TRUE(result.ok());

	m_svc.Shutdown();
	EXPECT_FALSE(m_svc.IsInitialized());
	EXPECT_EQ(m_svc.GetHashTable(), nullptr);
}
