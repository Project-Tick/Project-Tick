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

TEST_CASE("use current library with inline namespace")
{
    SECTION("implicitly")
    {
        using nlohmann::json;
        using nlohmann::ordered_json;

        json j;
        // In v3.10.5 mixing json_pointers of different basic_json types
        // results in implicit string conversion
        j[ordered_json::json_pointer("/root")] = json::object();
        CHECK(j.dump() == "{\"root\":{}}");
    }

    SECTION("explicitly")
    {
        using NLOHMANN_JSON_NAMESPACE::json;
        using NLOHMANN_JSON_NAMESPACE::ordered_json;

        json j;
        j[ordered_json::json_pointer("/root")] = json::object();
        CHECK(j.dump() == "{\"root\":{}}");
    }
}
