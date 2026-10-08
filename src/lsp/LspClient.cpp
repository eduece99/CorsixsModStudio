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

#include "lsp/LspClient.h"
#include <rainman/core/RainmanLog.h>
#include <sstream>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace lsp
{

CLspClient::~CLspClient() { Stop(); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CLspClient::StartAsync(const std::wstring &serverPath, const std::string &workspaceRoot,
                            const std::wstring &configPath, ReadyCallback onReady)
{
    Stop(); // Clean up any previous session

    CDMS_LOG_INFO("LSP: Starting language server (async)...");

    // Build command-line args: always use --stdio, optionally add --configpath
    std::wstring args = L"--stdio";
    if (!configPath.empty())
    {
        args += L" --configpath=\"" + configPath + L"\"";
        std::string configPathUtf8(configPath.begin(), configPath.end());
        CDMS_LOG_INFO("LSP: Using config file: {}", configPathUtf8);
    }

    if (!m_process.Start(serverPath, args, L""))
    {
        CDMS_LOG_ERROR("LSP: Failed to spawn language server process");
        m_bStartFailed.store(true);
        if (onReady)
        {
            // Deliver failure callback via Poll()
            std::lock_guard lock(m_mtx);
            m_pendingCallbacks.push_back([cb = std::move(onReady)]() { cb(false); });
        }
        return;
    }
    CDMS_LOG_INFO("LSP: Process spawned, starting I/O thread");

    m_bStopRequested.store(false);
    m_bReady.store(false);
    m_bStartFailed.store(false);

    // Capture workspace root for init handshake on I/O thread
    m_ioThread = std::thread(
        [this, workspaceRoot, onReady = std::move(onReady)]() mutable
        {
            bool ok = RunInitHandshake(workspaceRoot);

            if (ok)
            {
                m_bReady.store(true);
                CDMS_LOG_INFO("LSP: Language server initialized successfully");
            }
            else
            {
                m_bStartFailed.store(true);
                CDMS_LOG_ERROR("LSP: Init handshake failed");
            }

            // Notify blocking Start() waiters
            m_cvReady.notify_all();

            // Queue the ready callback for UI thread dispatch via Poll()
            if (onReady)
            {
                std::lock_guard lock(m_mtx);
                m_pendingCallbacks.push_back([cb = std::move(onReady), ok]() { cb(ok); });
            }

            if (!ok)
            {
                return;
            }

            // Enter the main I/O loop
            IoThreadMain();
        });
}

bool CLspClient::Start(const std::wstring &serverPath, const std::string &workspaceRoot, const std::wstring &configPath)
{
    StartAsync(serverPath, workspaceRoot, configPath, nullptr);

    // Wait for initialization to complete
    std::unique_lock lock(m_mtx);
    m_cvReady.wait(lock, [this] { return m_bReady.load() || m_bStartFailed.load(); });

    return m_bReady.load();
}

void CLspClient::Stop()
{
    if (!m_ioThread.joinable())
    {
        return;
    }

    CDMS_LOG_INFO("LSP: Shutting down language server");

    // Signal the I/O thread to stop
    m_bStopRequested.store(true);

    // Enqueue shutdown request + exit notification so the I/O thread sends them
    {
        std::lock_guard lock(m_mtx);
        nlohmann::json shutdownRequest = {{"jsonrpc", "2.0"}, {"id", m_nextRequestId++}, {"method", "shutdown"}};
        m_outQueue.push_back(JsonRpc::Encode(shutdownRequest));

        nlohmann::json exitNotif = {{"jsonrpc", "2.0"}, {"method", "exit"}, {"params", nlohmann::json::object()}};
        m_outQueue.push_back(JsonRpc::Encode(exitNotif));
    }

    // Wait for the I/O thread to finish (it drains outbound queue before exiting)
    if (m_ioThread.joinable())
    {
        m_ioThread.join();
    }

    m_process.Kill();
    m_framing.Reset();

    m_bReady.store(false);
    m_bStartFailed.store(false);
    m_bWorkspaceLoaded.store(false);
    m_bReceivedProgressBegin.store(false);
    m_bReceivedProgressBegin.store(false);

    std::lock_guard lock(m_mtx);
    m_outQueue.clear();
    m_pendingCallbacks.clear();
    m_completionCallbacks.clear();
    m_signatureHelpCallbacks.clear();
    m_hoverCallbacks.clear();
    m_diagnosticsCallbacks.clear();
    m_activeProgressTokens.clear();
    m_documentVersions.clear();
}

bool CLspClient::IsReady() const { return m_bReady.load(); }

bool CLspClient::IsWorkspaceLoaded() const
{
    if (m_bWorkspaceLoaded.load())
    {
        return true;
    }
    // If the server never announced any progress, treat workspace as loaded
    return !m_bReceivedProgressBegin.load();
}

bool CLspClient::IsRunning() const { return m_bReady.load() && m_process.IsRunning(); }

// ---------------------------------------------------------------------------
// I/O thread
// ---------------------------------------------------------------------------

bool CLspClient::RunInitHandshake(const std::string &workspaceRoot)
{
    // Build initialize params
    nlohmann::json capabilities = {
        {"textDocument",
         {{"synchronization", {{"didSave", true}, {"dynamicRegistration", false}}},
          {"completion",
           {{"completionItem", {{"snippetSupport", false}, {"documentationFormat", {"plaintext"}}}},
            {"dynamicRegistration", false}}},
          {"signatureHelp", {{"dynamicRegistration", false}}},
          {"hover", {{"dynamicRegistration", false}, {"contentFormat", {"markdown"}}}},
          {"definition", {{"dynamicRegistration", false}}},
          {"publishDiagnostics", {{"relatedInformation", false}}}}}};

    nlohmann::json initParams = {
        {"processId",
#ifdef _WIN32
         static_cast<int>(GetCurrentProcessId())
#else
         static_cast<int>(getpid())
#endif
        },
        {"capabilities", capabilities},
        {"rootUri", workspaceRoot.empty() ? nlohmann::json(nullptr) : nlohmann::json("file:///" + workspaceRoot)}};

    // Advertise support for server-initiated progress reporting
    initParams["capabilities"]["window"] = {{"workDoneProgress", true}};

    nlohmann::json initRequest = {{"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"}, {"params", initParams}};
    std::string encoded = JsonRpc::Encode(initRequest);
    m_process.Write(encoded);

    // Wait for initialize response with timeout (blocking reads on I/O thread)
    constexpr DWORD kMaxWaitMs = 10000;
    constexpr DWORD kReadTimeoutMs = 200;
    DWORD totalWaited = 0;

    while (totalWaited < kMaxWaitMs && !m_bStopRequested.load())
    {
        std::string data = m_process.ReadWithTimeout(kReadTimeoutMs);
        if (!data.empty())
        {
            m_framing.Feed(data);
        }

        while (auto msg = m_framing.TryParse())
        {
            if (msg->contains("id") && (*msg)["id"].get<int>() == 0)
            {
                // Got init response — send initialized notification
                nlohmann::json initializedNotif = {
                    {"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", nlohmann::json::object()}};
                m_process.Write(JsonRpc::Encode(initializedNotif));
                return true;
            }
        }

        totalWaited += kReadTimeoutMs;
    }

    CDMS_LOG_ERROR("LSP: Initialize handshake timed out after {}ms", kMaxWaitMs);
    return false;
}

void CLspClient::IoThreadMain()
{
    constexpr DWORD kReadTimeoutMs = 100;

    while (!m_bStopRequested.load())
    {
        // 1. Drain outbound message queue
        {
            std::vector<std::string> toSend;
            {
                std::lock_guard lock(m_mtx);
                toSend.swap(m_outQueue);
            }
            for (const auto &msg : toSend)
            {
                if (!m_process.Write(msg))
                {
                    CDMS_LOG_WARN("LSP: Failed to write message to server pipe");
                }
            }
        }

        // 2. Read from pipe (blocking with timeout so we can check stop flag)
        std::string data = m_process.ReadWithTimeout(kReadTimeoutMs);
        if (!data.empty())
        {
            m_framing.Feed(data);
        }

        // 3. Parse complete messages and produce callbacks
        std::vector<std::function<void()>> newCallbacks;
        while (auto msg = m_framing.TryParse())
        {
            HandleMessage(*msg);
        }

        // If the process died, stop the loop
        if (!m_process.IsRunning())
        {
            CDMS_LOG_WARN("LSP: Language server process exited unexpectedly");
            m_bReady.store(false);
            break;
        }
    }

    // Final drain: send any remaining outbound messages (shutdown/exit)
    std::vector<std::string> finalMessages;
    {
        std::lock_guard lock(m_mtx);
        finalMessages.swap(m_outQueue);
    }
    for (const auto &msg : finalMessages)
    {
        m_process.Write(msg);
    }

    CDMS_LOG_INFO("LSP: I/O thread exiting");
}

// ---------------------------------------------------------------------------
// Document operations
// ---------------------------------------------------------------------------

void CLspClient::OpenDocument(const std::string &uri, const std::string &languageId, const std::string &text)
{
    CDMS_LOG_INFO("LSP: Opening document: {} (lang={}, {} bytes)", uri, languageId, text.size());

    {
        std::lock_guard lock(m_mtx);
        m_documentVersions[uri] = 1;
    }

    DidOpenTextDocumentParams params;
    params.textDocument.uri = uri;
    params.textDocument.languageId = languageId;
    params.textDocument.version = 1;
    params.textDocument.text = text;

    SendNotification("textDocument/didOpen", params);
}

void CLspClient::ChangeDocument(const std::string &uri, const std::string &text)
{
    int version;
    {
        std::lock_guard lock(m_mtx);
        version = ++m_documentVersions[uri];
    }

    DidChangeTextDocumentParams params;
    params.textDocument.uri = uri;
    params.textDocument.version = version;
    params.contentChanges.push_back({text});

    SendNotification("textDocument/didChange", params);
}

void CLspClient::CloseDocument(const std::string &uri)
{
    CDMS_LOG_DEBUG("LSP: Closing document: {}", uri);
    DidCloseTextDocumentParams params;
    params.textDocument.uri = uri;

    SendNotification("textDocument/didClose", params);

    std::lock_guard lock(m_mtx);
    m_documentVersions.erase(uri);
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

void CLspClient::RequestCompletion(const std::string &uri, int line, int character, CompletionCallback callback)
{
    int id;
    {
        std::lock_guard lock(m_mtx);
        id = m_nextRequestId++;
        m_completionCallbacks[id] = std::move(callback);
    }

    CompletionParams params;
    params.textDocument.uri = uri;
    params.position.line = line;
    params.position.character = character;

    nlohmann::json request = {
        {"jsonrpc", "2.0"}, {"id", id}, {"method", "textDocument/completion"}, {"params", params}};
    Send(request);
}

void CLspClient::RequestSignatureHelp(const std::string &uri, int line, int character, SignatureHelpCallback callback)
{
    int id;
    {
        std::lock_guard lock(m_mtx);
        id = m_nextRequestId++;
        m_signatureHelpCallbacks[id] = std::move(callback);
    }

    SignatureHelpParams params;
    params.textDocument.uri = uri;
    params.position.line = line;
    params.position.character = character;

    nlohmann::json request = {
        {"jsonrpc", "2.0"}, {"id", id}, {"method", "textDocument/signatureHelp"}, {"params", params}};
    Send(request);
}

void CLspClient::RequestHover(const std::string &uri, int line, int character, HoverCallback callback)
{
    int id;
    {
        std::lock_guard lock(m_mtx);
        id = m_nextRequestId++;
        m_hoverCallbacks[id] = std::move(callback);
    }

    HoverParams params;
    params.textDocument.uri = uri;
    params.position.line = line;
    params.position.character = character;

    nlohmann::json request = {{"jsonrpc", "2.0"}, {"id", id}, {"method", "textDocument/hover"}, {"params", params}};
    Send(request);
}

void CLspClient::RequestDefinition(const std::string &uri, int line, int character, DefinitionCallback callback)
{
    int id;
    {
        std::lock_guard lock(m_mtx);
        id = m_nextRequestId++;
        m_definitionCallbacks[id] = std::move(callback);
    }

    DefinitionParams params;
    params.textDocument.uri = uri;
    params.position.line = line;
    params.position.character = character;

    nlohmann::json request = {
        {"jsonrpc", "2.0"}, {"id", id}, {"method", "textDocument/definition"}, {"params", params}};
    Send(request);
}

// ---------------------------------------------------------------------------
// Polling and callbacks
// ---------------------------------------------------------------------------

void CLspClient::RegisterDiagnosticsCallback(const std::string &uri, DiagnosticsCallback callback)
{
    std::lock_guard lock(m_mtx);
    m_diagnosticsCallbacks[uri] = std::move(callback);
}

void CLspClient::UnregisterDiagnosticsCallback(const std::string &uri)
{
    std::lock_guard lock(m_mtx);
    m_diagnosticsCallbacks.erase(uri);
}

void CLspClient::Poll()
{
    // Swap pending callbacks out under lock, then invoke on UI thread
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard lock(m_mtx);
        callbacks.swap(m_pendingCallbacks);
    }

    for (auto &cb : callbacks)
    {
        cb();
    }
}

// ---------------------------------------------------------------------------
// Message handling (called on I/O thread)
// ---------------------------------------------------------------------------

void CLspClient::HandleMessage(const nlohmann::json &message)
{
    if (message.contains("id") && message.contains("result"))
    {
        HandleResponse(message["id"].get<int>(), message["result"]);
    }
    else if (message.contains("id") && message.contains("error"))
    {
        int id = message["id"].get<int>();
        auto errObj = message["error"];
        CDMS_LOG_WARN("LSP: Error response id={}: {} (code={})", id, errObj.value("message", "unknown"),
                      errObj.value("code", -1));
        std::lock_guard lock(m_mtx);
        m_completionCallbacks.erase(id);
        m_signatureHelpCallbacks.erase(id);
        m_hoverCallbacks.erase(id);
        m_definitionCallbacks.erase(id);
    }
    else if (message.contains("id") && message.contains("method"))
    {
        // Server→client request — requires a response
        int id = message["id"].get<int>();
        std::string method = message["method"].get<std::string>();
        nlohmann::json params = message.value("params", nlohmann::json::object());
        HandleServerRequest(id, method, params);
    }
    else if (message.contains("method"))
    {
        std::string method = message["method"].get<std::string>();
        nlohmann::json params = message.value("params", nlohmann::json::object());
        HandleNotification(method, params);
    }
}

void CLspClient::HandleResponse(int id, const nlohmann::json &result)
{
    std::lock_guard lock(m_mtx);

    // Check completion callbacks
    auto compIt = m_completionCallbacks.find(id);
    if (compIt != m_completionCallbacks.end())
    {
        CompletionList list = result.get<CompletionList>();
        auto cb = std::move(compIt->second);
        m_completionCallbacks.erase(compIt);
        m_pendingCallbacks.push_back([cb = std::move(cb), list = std::move(list)]() mutable { cb(std::move(list)); });
        return;
    }

    // Check signature help callbacks
    auto sigIt = m_signatureHelpCallbacks.find(id);
    if (sigIt != m_signatureHelpCallbacks.end())
    {
        auto cb = std::move(sigIt->second);
        m_signatureHelpCallbacks.erase(sigIt);
        if (result.is_null())
        {
            m_pendingCallbacks.push_back([cb = std::move(cb)]() { cb(std::nullopt); });
        }
        else
        {
            auto help = result.get<SignatureHelp>();
            m_pendingCallbacks.push_back([cb = std::move(cb), help = std::move(help)]() mutable
                                         { cb(std::move(help)); });
        }
        return;
    }

    // Check hover callbacks
    auto hoverIt = m_hoverCallbacks.find(id);
    if (hoverIt != m_hoverCallbacks.end())
    {
        auto cb = std::move(hoverIt->second);
        m_hoverCallbacks.erase(hoverIt);
        if (result.is_null())
        {
            m_pendingCallbacks.push_back([cb = std::move(cb)]() { cb(std::nullopt); });
        }
        else
        {
            auto hover = result.get<HoverResult>();
            m_pendingCallbacks.push_back([cb = std::move(cb), hover = std::move(hover)]() mutable
                                         { cb(std::move(hover)); });
        }
        return;
    }

    // Check definition callbacks
    auto defIt = m_definitionCallbacks.find(id);
    if (defIt != m_definitionCallbacks.end())
    {
        auto cb = std::move(defIt->second);
        m_definitionCallbacks.erase(defIt);
        if (result.is_null())
        {
            m_pendingCallbacks.push_back([cb = std::move(cb)]() { cb(std::nullopt); });
        }
        else if (result.is_array())
        {
            // Definition can return Location[] — take the first one
            if (result.empty())
            {
                m_pendingCallbacks.push_back([cb = std::move(cb)]() { cb(std::nullopt); });
            }
            else
            {
                auto loc = result[0].get<Location>();
                m_pendingCallbacks.push_back([cb = std::move(cb), loc = std::move(loc)]() mutable
                                             { cb(std::move(loc)); });
            }
        }
        else
        {
            // Single Location object
            auto loc = result.get<Location>();
            m_pendingCallbacks.push_back([cb = std::move(cb), loc = std::move(loc)]() mutable { cb(std::move(loc)); });
        }
        return;
    }
}

void CLspClient::HandleNotification(const std::string &method, const nlohmann::json &params)
{
    if (method == "textDocument/publishDiagnostics")
    {
        auto diag = params.get<PublishDiagnosticsParams>();
        CDMS_LOG_INFO("LSP: Received {} diagnostics for {}", diag.diagnostics.size(), diag.uri);

        std::lock_guard lock(m_mtx);
        auto it = m_diagnosticsCallbacks.find(diag.uri);
        if (it != m_diagnosticsCallbacks.end())
        {
            auto cb = it->second; // Copy — diagnostics callbacks are long-lived
            auto uri = diag.uri;
            auto diagnostics = std::move(diag.diagnostics);
            m_pendingCallbacks.push_back(
                [cb = std::move(cb), uri = std::move(uri), diagnostics = std::move(diagnostics)]() mutable
                { cb(uri, std::move(diagnostics)); });
        }
        else
        {
            CDMS_LOG_WARN("LSP: No callback registered for diagnostics URI: {}", diag.uri);
        }
    }
    else if (method == "$/progress")
    {
        // Track workspace loading progress tokens
        std::string token;
        if (params.contains("token"))
        {
            if (params["token"].is_string())
            {
                token = params["token"].get<std::string>();
            }
            else
            {
                token = std::to_string(params["token"].get<int>());
            }
        }

        std::string kind;
        if (params.contains("value") && params["value"].contains("kind"))
        {
            kind = params["value"]["kind"].get<std::string>();
        }

        std::string title;
        if (params.contains("value") && params["value"].contains("title"))
        {
            title = params["value"]["title"].get<std::string>();
        }

        CDMS_LOG_INFO("LSP: $/progress token={} kind={} title={}", token, kind, title);

        if (kind == "begin")
        {
            std::lock_guard lock(m_mtx);
            m_activeProgressTokens.insert(token);
            m_bReceivedProgressBegin.store(true);
        }
        else if (kind == "end")
        {
            std::lock_guard lock(m_mtx);
            m_activeProgressTokens.erase(token);
            if (m_activeProgressTokens.empty() && !m_bWorkspaceLoaded.load())
            {
                m_bWorkspaceLoaded.store(true);
                CDMS_LOG_INFO("LSP: All progress tokens complete — workspace loaded");
            }
        }
    }
    else if (method == "window/logMessage")
    {
        int type = params.value("type", 4);
        std::string msg = params.value("message", "");
        if (type <= 1)
        {
            CDMS_LOG_ERROR("LuaLS: {}", msg);
        }
        else if (type == 2)
        {
            CDMS_LOG_WARN("LuaLS: {}", msg);
        }
        else if (type == 3)
        {
            CDMS_LOG_INFO("LuaLS: {}", msg);
        }
        else
        {
            CDMS_LOG_DEBUG("LuaLS: {}", msg);
        }
    }
    else
    {
        CDMS_LOG_TRACE("LSP: Ignoring notification: {}", method);
    }
}

void CLspClient::HandleServerRequest(int id, const std::string &method, const nlohmann::json &params)
{
    if (method == "window/workDoneProgress/create")
    {
        // Acknowledge the progress token — required by LSP spec
        CDMS_LOG_DEBUG("LSP: Acknowledging progress token creation (id={})", id);
        nlohmann::json response = {{"jsonrpc", "2.0"}, {"id", id}, {"result", nlohmann::json::value_t::null}};
        Send(response);
    }
    else if (method == "client/registerCapability")
    {
        // Accept dynamic capability registration
        CDMS_LOG_DEBUG("LSP: Accepting dynamic capability registration (id={})", id);
        nlohmann::json response = {{"jsonrpc", "2.0"}, {"id", id}, {"result", nlohmann::json::value_t::null}};
        Send(response);
    }
    else
    {
        CDMS_LOG_WARN("LSP: Unhandled server request: {} (id={})", method, id);
        nlohmann::json response = {
            {"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", -32601}, {"message", "Method not found: " + method}}}};
        Send(response);
    }
}

// ---------------------------------------------------------------------------
// Wire-level helpers
// ---------------------------------------------------------------------------

void CLspClient::SendRequest(const std::string &method, const nlohmann::json &params)
{
    int id;
    {
        std::lock_guard lock(m_mtx);
        id = m_nextRequestId++;
    }
    nlohmann::json request = {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    Send(request);
}

void CLspClient::SendNotification(const std::string &method, const nlohmann::json &params)
{
    CDMS_LOG_INFO("LSP: Sending notification: {}", method);
    nlohmann::json notification = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
    Send(notification);
}

void CLspClient::Send(const nlohmann::json &message)
{
    std::string encoded = JsonRpc::Encode(message);
    std::lock_guard lock(m_mtx);
    m_outQueue.push_back(std::move(encoded));
}

} // namespace lsp
