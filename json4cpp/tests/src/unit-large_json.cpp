//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#include "doctest_compatibility.h"

#include <nlohmann/json.hpp>
using nlohmann::json;

#include <algorithm>

TEST_CASE("tests on very large JSONs")
{
    SECTION("issue #1419 - Segmentation fault (stack overflow) due to unbounded recursion")
    {
        const auto depth = 5000000;

        std::string s(static_cast<std::size_t>(2 * depth), '[');
        std::fill(s.begin() + depth, s.end(), ']');

        json _;
        CHECK_NOTHROW(_ = nlohmann::json::parse(s));
    }
}

