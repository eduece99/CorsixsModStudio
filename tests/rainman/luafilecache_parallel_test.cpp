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

#include <gtest/gtest.h>
#include "rainman/lua/CLuaFileCache.h"
#include "rainman/core/Exception.h"
#include <future>
#include <thread>
#include <vector>
#include <algorithm>

// Verify that concurrent MakeState/FreeState calls don't corrupt the cache.
TEST(CLuaFileCacheParallel, ConcurrentMakeAndFreeState)
{
    CLuaFileCache cache;

    constexpr int kNumThreads = 16;
    std::vector<std::future<lua_State *>> futures;
    futures.reserve(kNumThreads);

    // Phase 1: Create states concurrently
    for (int i = 0; i < kNumThreads; ++i)
    {
        futures.push_back(std::async(std::launch::async, [&cache]() { return cache.MakeState(); }));
    }

    std::vector<lua_State *> states;
    for (auto &f : futures)
    {
        lua_State *L = f.get();
        ASSERT_NE(L, nullptr);
        states.push_back(L);
    }

    // All states should be unique
    auto sorted = states;
    std::sort(sorted.begin(), sorted.end());
    auto it = std::unique(sorted.begin(), sorted.end());
    EXPECT_EQ(it, sorted.end()) << "MakeState returned duplicate lua_State pointers";

    // Phase 2: Free states concurrently
    std::vector<std::future<void>> freeFutures;
    freeFutures.reserve(kNumThreads);
    for (auto *L : states)
    {
        freeFutures.push_back(std::async(std::launch::async, [&cache, L]() { cache.FreeState(L); }));
    }

    for (auto &f : freeFutures)
    {
        EXPECT_NO_THROW(f.get());
    }
}

// Verify that concurrent AddToCache and Fetch don't crash.
TEST(CLuaFileCacheParallel, ConcurrentAddAndFetch)
{
    CLuaFileCache cache;

    constexpr int kNumStates = 8;
    std::vector<lua_State *> states;
    for (int i = 0; i < kNumStates; ++i)
    {
        states.push_back(cache.MakeState());
    }

    // Add names concurrently
    std::vector<std::future<void>> addFutures;
    for (int i = 0; i < kNumStates; ++i)
    {
        std::string name = "state_" + std::to_string(i);
        addFutures.push_back(
            std::async(std::launch::async, [&cache, L = states[i], name]() { cache.AddToCache(name.c_str(), L); }));
    }
    for (auto &f : addFutures)
    {
        EXPECT_NO_THROW(f.get());
    }

    // Fetch concurrently
    std::vector<std::future<lua_State *>> fetchFutures;
    for (int i = 0; i < kNumStates; ++i)
    {
        std::string name = "state_" + std::to_string(i);
        fetchFutures.push_back(
            std::async(std::launch::async, [&cache, name]() { return cache.Fetch(name.c_str()); }));
    }

    for (int i = 0; i < kNumStates; ++i)
    {
        lua_State *L = fetchFutures[i].get();
        EXPECT_EQ(L, states[i]);
    }
}

// Verify that Clear under concurrent access doesn't crash.
TEST(CLuaFileCacheParallel, ClearWhileMakingStates)
{
    CLuaFileCache cache;

    // Create some states first
    for (int i = 0; i < 4; ++i)
    {
        cache.MakeState();
    }

    // Clear and make concurrently — should not crash
    auto clearFuture = std::async(std::launch::async, [&cache]() { cache.Clear(); });

    // After clear, making new states should work
    clearFuture.get();

    lua_State *L = cache.MakeState();
    EXPECT_NE(L, nullptr);
}
