//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#include "doctest_compatibility.h"

#include <nlohmann/json_v3_10_5.hpp>
using nlohmann::json;
using nlohmann::ordered_json;

TEST_CASE("use library v3.10.5 without inline namespace")
{
    json j;
    j[ordered_json::json_pointer("/root")] = json::object();
    // In v3.10.5 mixing json_pointers of different basic_json types
    // results in implicit string conversion
    CHECK(j.dump() == "{\"/root\":{}}");
}
