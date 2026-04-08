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

TEST_CASE("version information")
{
    SECTION("meta()")
    {
        json j = json::meta();

        CHECK(j["name"] == "JSON for Modern C++");
        CHECK(j["copyright"] == "(C) 2013-2026 Niels Lohmann");
        CHECK(j["url"] == "https://github.com/nlohmann/json");
        CHECK(j["version"] == json(
        {
            {"string", "3.12.0"},
            {"major", 3},
            {"minor", 12},
            {"patch", 0}
        }));

        CHECK(j.find("platform") != j.end());
        CHECK(j.at("compiler").find("family") != j.at("compiler").end());
        CHECK(j.at("compiler").find("version") != j.at("compiler").end());
        CHECK(j.at("compiler").find("c++") != j.at("compiler").end());
    }
}
