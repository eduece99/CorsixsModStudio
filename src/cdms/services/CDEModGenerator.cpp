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

#include "common/Common.h"
#include "services/CDEModGenerator.h"
#include "DefaultBurnTemplate.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace
{
constexpr std::array<std::string_view, 5> kParents = {"DoWDE", "DXP3", "DXP2", "WXP", "W40k"};

bool IsIdentifierCharacter(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

void Validate(const CDEModGenerator::Options &options)
{
    const auto &id = options.identifier;
    if (id.empty() || id.size() > 64 || !std::all_of(id.begin(), id.end(), IsIdentifierCharacter))
    {
        throw std::runtime_error("Mod identifier must be 1-64 ASCII letters, digits or underscores (no spaces).");
    }

    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    constexpr std::array<std::string_view, 4> kDeviceNames = {"CON", "PRN", "AUX", "NUL"};
    if (std::find(kDeviceNames.begin(), kDeviceNames.end(), upper) != kDeviceNames.end() ||
        (upper.size() == 4 && (upper.substr(0, 3) == "COM" || upper.substr(0, 3) == "LPT") && upper[3] >= '1' &&
         upper[3] <= '9'))
    {
        throw std::runtime_error("Mod identifier is a reserved Windows file name.");
    }
    if (upper == "ENGINE" || std::any_of(kParents.begin(), kParents.end(),
                                         [&](std::string_view parent)
                                         {
                                             std::string p(parent);
                                             std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c)
                                                            { return static_cast<char>(std::toupper(c)); });
                                             return upper == p;
                                         }))
    {
        throw std::runtime_error("Mod identifier conflicts with a base game project.");
    }
    const auto validText = [](const std::string &text, std::size_t max, bool required)
    {
        return (!required || !text.empty()) && text.size() <= max &&
               std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= 0x20 && c <= 0x7e; });
    };
    if (!validText(options.displayName, 64, true))
    {
        throw std::runtime_error("Display name must be 1-64 printable ASCII characters.");
    }
    if (!validText(options.description, 256, false))
    {
        throw std::runtime_error("Description must be at most 256 printable ASCII characters.");
    }
    if (!CDEModGenerator::IsValidParent(options.parent))
    {
        throw std::runtime_error("Unknown Definitive Edition parent mod.");
    }
}

void WriteFile(const std::filesystem::path &path, const std::string &contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::out);
    if (!file)
    {
        throw std::runtime_error("Cannot create " + path.string());
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    if (!file)
    {
        throw std::runtime_error("Cannot write " + path.string());
    }
}

std::string MakeModule(const CDEModGenerator::Options &options)
{
    std::ostringstream out;
    out << "[global]\r\n"
        << "UIName = " << options.displayName << "\r\n"
        << "Description = " << options.description << "\r\n"
        << "DllName = DXP3Mod\r\n"
        << "Playable = 1\r\n"
        << "ModFolder = Mod\r\n"
        << "ModVersion = 1.0\r\n"
        << "TextureFE =\r\n"
        << "TextureIcon =\r\n"
        << "RuntimeMode = 3\r\n\r\n"
        << "DataFolder.1 = Data\r\n"
        << "ArchiveFile.1 = Data\r\n";

    const auto first = std::find(kParents.begin(), kParents.end(), options.parent);
    for (auto it = first; it != kParents.end(); ++it)
    {
        out << "RequiredMod." << std::distance(first, it) + 1 << " = " << *it << "\r\n";
    }
    return out.str();
}

std::string MakePipeline(const CDEModGenerator::Options &options)
{
    std::ostringstream out;
    out << "[global]\r\n"
        << "WkDir = %app%\r\n"
        << "EngineLocale = %app%\\Engine\\Locale\\English\r\n"
        << "ToolsData = %app%\\Tools\\ToolsData\r\n\r\n"
        << "[attrib]\r\nlua = \\attrib\\\r\nrgd = \\attrib\\\r\n\r\n"
        << "[burner]\r\nburnerPath = %app%\r\n\r\n"
        << "[p4]\r\nRevControlPort =\r\n\r\n"
        << "[project:" << options.identifier << "]\r\n"
        << "Description = " << options.description << "\r\n"
        << "DataSource = DataSrc\r\n"
        << "DataIntermediate = DataIntermediate\r\n"
        << "DataGeneric = DataGeneric\r\n"
        << "DataBurn = Mod\r\n"
        << "DataFinal = Mod\\Data\r\n"
        << "LocaleFolder = Mod\\Locale\\English\r\n"
        << "DataExtra =\r\n"
        << "Parent = " << options.parent << "\r\n"
        << "DataSourceShared =\r\n"
        << "DataPreview =\r\n"
        << "AttrLoc =\r\n";

    constexpr std::array<std::string_view, 6> projects = {"DoWDE", "DXP3", "DXP2", "WXP", "W40k", "Engine"};
    for (std::size_t i = 0; i < projects.size(); ++i)
    {
        const auto name = projects[i];
        out << "\r\n[project:" << name << "]\r\n"
            << "description = " << name << "\r\n"
            << "datasource =\r\n"
            << "dataintermediate =\r\n"
            << "datageneric = %app%\\" << name << "\\DataGeneric\r\n"
            << "databurn = %app%\\" << name << "\r\n"
            << "datafinal = %app%\\" << name << "\\Data\r\n"
            << "localefolder = %app%\\" << name << "\\Locale\\English\r\n"
            << "dataextra =\r\n"
            << "parent = " << (i + 1 < projects.size() ? projects[i + 1] : std::string_view{}) << "\r\n"
            << "datasourceshared =\r\n"
            << "datapreview =\r\n"
            << "attrloc =\r\n";
    }
    return out.str();
}

constexpr std::string_view kSgaConfig =
    "Archive\r\n"
    "TOCStart alias=\"Data\" relativeroot=\"Data\"\r\n"
    "FileSettingsStart defcompression=\"1\"\r\n"
    "Override wildcard=\".*$\" minsize=\"-1\" maxsize=\"100\" ct=\"0\"\r\n"
    "Override wildcard=\".*$\" minsize=\"100\" maxsize=\"4096\" ct=\"2\"\r\n"
    "Override wildcard=\".*(ttf)|(rgt)$\" minsize=\"-1\" maxsize=\"-1\" ct=\"0\"\r\n"
    "Override wildcard=\".*(lua)$\" minsize=\"-1\" maxsize=\"-1\" ct=\"2\"\r\n"
    "FileSettingsEnd\r\n"
    "TOCEnd\r\n";
} // namespace

bool CDEModGenerator::IsValidParent(std::string_view parent) noexcept
{
    return std::find(kParents.begin(), kParents.end(), parent) != kParents.end();
}

std::filesystem::path CDEModGenerator::Create(const std::filesystem::path &modsRoot, const Options &options)
{
    Validate(options);
    if (modsRoot.empty())
    {
        throw std::runtime_error("Definitive Edition mods directory is not configured.");
    }
    static_assert(!kDefaultBurnTemplate.empty());
    if (!std::filesystem::exists(modsRoot))
    {
        std::filesystem::create_directories(modsRoot);
    }
    if (!std::filesystem::is_directory(modsRoot))
    {
        throw std::runtime_error("Definitive Edition mods path is not a directory: " + modsRoot.string());
    }

    const auto modDir = modsRoot / options.identifier;
    if (std::filesystem::exists(modsRoot / (options.identifier + ".module")))
    {
        throw std::runtime_error("A module with this identifier already exists in " + modsRoot.string());
    }
    if (!std::filesystem::create_directory(modDir))
    {
        throw std::runtime_error("A mod with this identifier already exists: " + modDir.string());
    }

    try
    {
        std::filesystem::create_directories(modDir / "DataSrc");
        std::filesystem::create_directories(modDir / "DataIntermediate");
        std::filesystem::create_directories(modDir / "DataGeneric");
        std::filesystem::create_directories(modDir / "DataGeneric" / "Sound");
        std::filesystem::create_directories(modDir / "Mod" / "Data");
        std::filesystem::create_directories(modDir / "Mod" / "Locale" / "English");

        const auto module = modDir / (options.identifier + ".module");
        WriteFile(module, MakeModule(options));
        WriteFile(modDir / "pipeline.ini", MakePipeline(options));
        WriteFile(modDir / "DataGeneric" / "_default.burn", std::string(kDefaultBurnTemplate));
        WriteFile(modDir / "DataGeneric" / "Sound" / "_default.rat", {});
        WriteFile(modDir / "Mod" / "Data.sgaconfig", std::string(kSgaConfig));
        return module;
    }
    catch (...)
    {
        std::error_code error;
        std::filesystem::remove_all(modDir, error);
        if (error)
        {
            throw std::runtime_error("Mod creation failed and cleanup failed for " + modDir.string() + ": " +
                                     error.message());
        }
        throw;
    }
}
