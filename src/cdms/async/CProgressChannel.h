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

#pragma once

#include <functional>
#include <string>

//! Thread-safe progress reporting channel.
/*!
    Wraps a callback that receives progress messages. The Report() method
    can be called from any thread — the callback itself determines how
    the message is delivered (e.g., directly, or via CallAfter for GUI updates).

    Provides a static RainmanCallback() that is type-compatible with
    Rainman's pfnStatusCallback, allowing seamless bridging.
*/
class CProgressChannel
{
  public:
    using ProgressCallback = std::function<void(const std::string &)>;

    explicit CProgressChannel(ProgressCallback fnOnProgress);

    //! Report a progress message. Safe to call from any thread.
    void Report(const std::string &sMessage);

    //! Rainman-compatible static callback (matches pfnStatusCallback signature).
    /*!
        Usage: pass RainmanCallback as the callback function and a
        CProgressChannel* as the tag parameter.
    */
#ifdef _WIN32
    static void __cdecl RainmanCallback(const char *sMsg, void *pTag);
#else
    static void RainmanCallback(const char *sMsg, void *pTag);
#endif

  private:
    ProgressCallback m_fnOnProgress;
};
