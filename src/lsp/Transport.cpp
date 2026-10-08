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

#include "lsp/Transport.h"
#include <rainman/core/RainmanLog.h>
#ifdef _WIN32
#include <algorithm>
#else
#include <cerrno>
#include <chrono>
#include <codecvt>
#include <fcntl.h>
#include <locale>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#endif

namespace lsp
{

#ifdef _WIN32
CProcess::~CProcess() { Cleanup(); }

bool CProcess::Start(const std::wstring &exePath, const std::wstring &args, const std::wstring &workingDir)
{
    Cleanup();

    // Create pipes for stdin/stdout redirection
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdinRead = INVALID_HANDLE_VALUE;
    HANDLE hStdoutWrite = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&hStdinRead, &m_hStdinWrite, &sa, 0))
    {
        return false;
    }

    if (!CreatePipe(&m_hStdoutRead, &hStdoutWrite, &sa, 0))
    {
        CloseHandle(hStdinRead);
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = INVALID_HANDLE_VALUE;
        return false;
    }

    // Ensure our ends of the pipes are not inherited by the child
    SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    // Build command line: "exePath" args
    std::wstring cmdLine = L"\"" + exePath + L"\"";
    if (!args.empty())
    {
        cmdLine += L" " + args;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(nullptr,
                             cmdLine.data(), // mutable command line required by CreateProcessW
                             nullptr,        // process security
                             nullptr,        // thread security
                             TRUE,           // inherit handles
                             CREATE_NO_WINDOW,
                             nullptr, // environment
                             workingDir.empty() ? nullptr : workingDir.c_str(), &si, &pi);

    // Close the child's ends of the pipes (we don't use them)
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);

    if (!ok)
    {
        DWORD err = GetLastError();
        CDMS_LOG_ERROR("LSP: CreateProcess failed (error={})", err);
        CloseHandle(m_hStdinWrite);
        CloseHandle(m_hStdoutRead);
        m_hStdinWrite = INVALID_HANDLE_VALUE;
        m_hStdoutRead = INVALID_HANDLE_VALUE;
        return false;
    }

    CDMS_LOG_INFO("LSP: Process created (PID={})", pi.dwProcessId);
    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    return true;
}

bool CProcess::Write(const std::string &data) { return Write(data.data(), data.size()); }

bool CProcess::Write(const char *data, size_t length)
{
    if (m_hStdinWrite == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    size_t totalWritten = 0;
    while (totalWritten < length)
    {
        DWORD written = 0;
        if (!WriteFile(m_hStdinWrite, data + totalWritten, static_cast<DWORD>(length - totalWritten), &written,
                       nullptr))
        {
            return false;
        }
        totalWritten += written;
    }
    return true;
}

std::string CProcess::Read(size_t bufferSize)
{
    if (m_hStdoutRead == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    // Check if data is available without blocking
    DWORD available = 0;
    if (!PeekNamedPipe(m_hStdoutRead, nullptr, 0, nullptr, &available, nullptr) || available == 0)
    {
        return {};
    }

    size_t toRead = (std::min)(static_cast<size_t>(available), bufferSize);
    std::string result(toRead, '\0');

    DWORD bytesRead = 0;
    if (!ReadFile(m_hStdoutRead, result.data(), static_cast<DWORD>(toRead), &bytesRead, nullptr))
    {
        return {};
    }

    result.resize(bytesRead);
    return result;
}

std::string CProcess::ReadWithTimeout(DWORD timeoutMs, size_t bufferSize)
{
    if (m_hStdoutRead == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    // Anonymous pipes are not waitable objects, so we poll with PeekNamedPipe.
    // This runs on a background I/O thread, so the polling doesn't block the UI.
    constexpr DWORD kPollIntervalMs = 10;
    DWORD elapsed = 0;

    while (elapsed < timeoutMs)
    {
        DWORD available = 0;
        if (PeekNamedPipe(m_hStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0)
        {
            return Read(bufferSize);
        }
        Sleep(kPollIntervalMs);
        elapsed += kPollIntervalMs;
    }

    return {};
}

bool CProcess::IsRunning() const
{
    if (m_hProcess == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(m_hProcess, &exitCode))
    {
        return false;
    }

    return exitCode == STILL_ACTIVE;
}

void CProcess::Kill()
{
    if (m_hProcess != INVALID_HANDLE_VALUE && IsRunning())
    {
        TerminateProcess(m_hProcess, 1);
        WaitForSingleObject(m_hProcess, 3000);
    }
    Cleanup();
}

void CProcess::Cleanup()
{
    if (m_hStdinWrite != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = INVALID_HANDLE_VALUE;
    }
    if (m_hStdoutRead != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hStdoutRead);
        m_hStdoutRead = INVALID_HANDLE_VALUE;
    }
    if (m_hProcess != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hProcess);
        m_hProcess = INVALID_HANDLE_VALUE;
    }
}
#else
namespace
{
std::string WideToUtf8(const std::wstring &text)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(text);
}

std::vector<std::string> SplitArguments(const std::wstring &args)
{
    std::vector<std::string> result;
    std::wstring argument;
    bool inQuotes = false;

    for (wchar_t character : args)
    {
        if (character == L'"')
        {
            inQuotes = !inQuotes;
        }
        else if ((character == L' ' || character == L'\t') && !inQuotes)
        {
            if (!argument.empty())
            {
                result.push_back(WideToUtf8(argument));
                argument.clear();
            }
        }
        else
        {
            argument.push_back(character);
        }
    }

    if (!argument.empty())
    {
        result.push_back(WideToUtf8(argument));
    }
    return result;
}

bool SetNonBlocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags != -1 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}
} // namespace

CProcess::~CProcess() { Kill(); }

bool CProcess::Start(const std::wstring &exePath, const std::wstring &args, const std::wstring &workingDir)
{
    Kill();

    int stdinPipe[2];
    int stdoutPipe[2];
    if (pipe(stdinPipe) != 0)
    {
        return false;
    }
    if (pipe(stdoutPipe) != 0)
    {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        return false;
    }

    const std::string executable = WideToUtf8(exePath);
    const std::string directory = workingDir.empty() ? std::string{} : WideToUtf8(workingDir);
    std::vector<std::string> arguments = SplitArguments(args);
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char *>(executable.c_str()));
    for (std::string &argument : arguments)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const pid_t process = fork();
    if (process == -1)
    {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return false;
    }
    if (process == 0)
    {
        if ((!directory.empty() && chdir(directory.c_str()) != 0) ||
            dup2(stdinPipe[0], STDIN_FILENO) == -1 || dup2(stdoutPipe[1], STDOUT_FILENO) == -1 ||
            dup2(stdoutPipe[1], STDERR_FILENO) == -1)
        {
            _exit(127);
        }
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        execvp(executable.c_str(), argv.data());
        _exit(127);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    if (!SetNonBlocking(stdoutPipe[0]))
    {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        kill(process, SIGKILL);
        waitpid(process, nullptr, 0);
        return false;
    }

    m_iProcess = process;
    m_iStdinWrite = stdinPipe[1];
    m_iStdoutRead = stdoutPipe[0];
    return true;
}

bool CProcess::Write(const std::string &data) { return Write(data.data(), data.size()); }

bool CProcess::Write(const char *data, size_t length)
{
    if (m_iStdinWrite == -1)
    {
        return false;
    }

    sigset_t blockedSignals;
    sigset_t previousSignals;
    sigemptyset(&blockedSignals);
    sigaddset(&blockedSignals, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blockedSignals, &previousSignals) != 0)
    {
        return false;
    }

    sigset_t pendingSignals;
    sigpending(&pendingSignals);
    const bool hadPendingSigpipe = sigismember(&pendingSignals, SIGPIPE) == 1;

    size_t totalWritten = 0;
    bool success = true;
    while (totalWritten < length)
    {
        const ssize_t written = write(m_iStdinWrite, data + totalWritten, length - totalWritten);
        if (written > 0)
        {
            totalWritten += static_cast<size_t>(written);
        }
        else if (written == -1 && errno == EINTR)
        {
            continue;
        }
        else
        {
            success = false;
            break;
        }
    }

    if (!success && errno == EPIPE && !hadPendingSigpipe)
    {
        timespec timeout{};
        sigtimedwait(&blockedSignals, nullptr, &timeout);
    }
    pthread_sigmask(SIG_SETMASK, &previousSignals, nullptr);
    return success;
}

std::string CProcess::Read(size_t bufferSize)
{
    if (m_iStdoutRead == -1 || bufferSize == 0)
    {
        return {};
    }

    std::string result(bufferSize, '\0');
    const ssize_t bytesRead = read(m_iStdoutRead, result.data(), result.size());
    if (bytesRead <= 0)
    {
        return {};
    }
    result.resize(static_cast<size_t>(bytesRead));
    return result;
}

std::string CProcess::ReadWithTimeout(DWORD timeoutMs, size_t bufferSize)
{
    if (m_iStdoutRead == -1)
    {
        return {};
    }

    pollfd descriptor{m_iStdoutRead, POLLIN, 0};
    int result;
    do
    {
        result = poll(&descriptor, 1, static_cast<int>(timeoutMs));
    } while (result == -1 && errno == EINTR);

    return result > 0 && (descriptor.revents & (POLLIN | POLLHUP)) ? Read(bufferSize) : std::string{};
}

bool CProcess::IsRunning() const
{
    if (m_iProcess == -1)
    {
        return false;
    }

    int status = 0;
    const pid_t result = waitpid(m_iProcess, &status, WNOHANG);
    if (result == 0 || (result == -1 && errno == EINTR))
    {
        return true;
    }
    m_iProcess = -1;
    return false;
}

void CProcess::Kill()
{
    if (m_iProcess != -1 && IsRunning())
    {
        kill(m_iProcess, SIGTERM);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (IsRunning() && std::chrono::steady_clock::now() < deadline)
        {
            usleep(10000);
        }
        if (IsRunning())
        {
            kill(m_iProcess, SIGKILL);
            while (waitpid(m_iProcess, nullptr, 0) == -1 && errno == EINTR)
            {
            }
            m_iProcess = -1;
        }
    }
    Cleanup();
}

void CProcess::Cleanup()
{
    if (m_iStdinWrite != -1)
    {
        close(m_iStdinWrite);
        m_iStdinWrite = -1;
    }
    if (m_iStdoutRead != -1)
    {
        close(m_iStdoutRead);
        m_iStdoutRead = -1;
    }
}
#endif

} // namespace lsp
