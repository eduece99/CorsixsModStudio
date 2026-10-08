/*
LSP Client Library
Copyright (C) 2026 CorsixModStudio Contributors

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

#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/types.h>
using DWORD = std::uint32_t;
#endif

namespace lsp
{

/// Manages a child process with stdin/stdout pipe redirection.
/// Used to communicate with the language server via JSON-RPC over stdio.
class CProcess
{
  public:
    CProcess() = default;
    ~CProcess();

    CProcess(const CProcess &) = delete;
    CProcess &operator=(const CProcess &) = delete;
    CProcess(CProcess &&) = delete;
    CProcess &operator=(CProcess &&) = delete;

    /// Spawn a child process with the given command line and working directory.
    /// stdin and stdout are redirected through pipes for JSON-RPC communication.
    /// @returns true on success, false on failure.
    bool Start(const std::wstring &exePath, const std::wstring &args, const std::wstring &workingDir);

    /// Write data to the child's stdin.
    /// @returns true if all bytes were written successfully.
    bool Write(const std::string &data);
    bool Write(const char *data, size_t length);

    /// Non-blocking read from the child's stdout.
    /// Reads whatever data is currently available (up to bufferSize bytes).
    /// @returns the data read, or empty string if nothing is available.
    std::string Read(size_t bufferSize = 65536);

    /// Blocking read from the child's stdout with a timeout.
    /// Polls for data availability (via PeekNamedPipe) up to @p timeoutMs
    /// milliseconds, then reads whatever is available (up to bufferSize bytes).
    /// Intended for use on a background I/O thread.
    /// @returns the data read, or empty string on timeout or error.
    std::string ReadWithTimeout(DWORD timeoutMs, size_t bufferSize = 65536);

    /// Check if the child process is still running.
    [[nodiscard]] bool IsRunning() const;

    /// Terminate the child process.
    void Kill();

  private:
    void Cleanup();

#ifdef _WIN32
    HANDLE m_hProcess = INVALID_HANDLE_VALUE;
    HANDLE m_hStdinWrite = INVALID_HANDLE_VALUE;
    HANDLE m_hStdoutRead = INVALID_HANDLE_VALUE;
#else
    mutable pid_t m_iProcess = -1;
    int m_iStdinWrite = -1;
    int m_iStdoutRead = -1;
#endif
};

} // namespace lsp
