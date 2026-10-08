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

#include "rainman/core/RainmanLog.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <filesystem>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

bool RainmanLog::s_bInitialised = false;

void RainmanLog::init(const char *sLogDir)
{
    if (s_bInitialised)
        return;

    // Ensure the log directory exists
    std::filesystem::create_directories(sLogDir);

    // Shared sinks: coloured console + 5 MB rotating file (3 backups)
    auto pConsoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    pConsoleSink->set_level(spdlog::level::debug);

    // Include PID in log filename so parallel test processes don't collide
#ifdef _WIN32
    const auto iProcessId = _getpid();
#else
    const auto iProcessId = getpid();
#endif
    std::string sLogPath = std::string(sLogDir) + "/corsixmodstudio_" + std::to_string(iProcessId) + ".log";
    constexpr std::size_t iMaxSize = static_cast<std::size_t>(5) * 1024 * 1024; // 5 MB
    constexpr std::size_t iMaxFiles = 3;

    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> pFileSink;
    try
    {
        pFileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(sLogPath, iMaxSize, iMaxFiles, true);
        pFileSink->set_level(spdlog::level::trace);
    }
    catch (const spdlog::spdlog_ex &)
    {
        // If the file sink fails (e.g. file locked), fall back to console only
        pFileSink = nullptr;
    }

    std::vector<spdlog::sink_ptr> vSinks{pConsoleSink};
    if (pFileSink)
        vSinks.push_back(pFileSink);

    // "rainman" logger — library layer
    auto pRainman = std::make_shared<spdlog::logger>("rainman", vSinks.begin(), vSinks.end());
    pRainman->set_level(spdlog::level::debug);
    pRainman->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
    spdlog::register_logger(pRainman);

    // "cdms" logger — GUI layer
    auto pCdms = std::make_shared<spdlog::logger>("cdms", vSinks.begin(), vSinks.end());
    pCdms->set_level(spdlog::level::debug);
    pCdms->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
    spdlog::register_logger(pCdms);

    // Flush on warnings and above so errors are never lost
    spdlog::flush_on(spdlog::level::warn);

    s_bInitialised = true;
}

std::shared_ptr<spdlog::logger> RainmanLog::get()
{
    auto pLogger = spdlog::get("rainman");
    if (!pLogger)
    {
        // Fallback: auto-init with defaults if caller forgot to call init()
        init();
        pLogger = spdlog::get("rainman");
    }
    return pLogger;
}

std::shared_ptr<spdlog::logger> RainmanLog::getCdms()
{
    auto pLogger = spdlog::get("cdms");
    if (!pLogger)
    {
        init();
        pLogger = spdlog::get("cdms");
    }
    return pLogger;
}
