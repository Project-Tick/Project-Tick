//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdio>   // fopen, fclose, FILE
#include <memory> // unique_ptr
#include <test_data.hpp>
#include <doctest.h>

namespace utils
{

inline bool check_testsuite_downloaded()
{
    using FilePtr = std::unique_ptr<FILE, int(*)(FILE*)>;
    const FilePtr file(std::fopen(TEST_DATA_DIRECTORY "/README.md", "r"), std::fclose);
    return file != nullptr;
}

TEST_CASE("check test suite is downloaded")
{
    REQUIRE_MESSAGE(utils::check_testsuite_downloaded(), "Test data not found in '" TEST_DATA_DIRECTORY "'. Please execute target 'download_test_data' before running this test suite. See <https://github.com/nlohmann/json#execute-unit-tests> for more information.");
}

}  // namespace utils
