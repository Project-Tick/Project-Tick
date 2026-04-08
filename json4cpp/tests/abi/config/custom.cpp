//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#include "doctest_compatibility.h"

#include "config.hpp"

// define custom namespace
#define NLOHMANN_JSON_NAMESPACE nlohmann // this line may be omitted
#define NLOHMANN_JSON_NAMESPACE_BEGIN namespace nlohmann {
#define NLOHMANN_JSON_NAMESPACE_END }
#include <nlohmann/json_fwd.hpp>

TEST_CASE("custom namespace")
{
    // GCC 4.8 fails with regex_error
#if !DOCTEST_GCC || DOCTEST_GCC >= DOCTEST_COMPILER(4, 9, 0)
    SECTION("namespace matches expectation")
    {
        std::string expected = "nlohmann::basic_json";

        // fallback for Clang
        const std::string ns{STRINGIZE(NLOHMANN_JSON_NAMESPACE) "::basic_json"};

        CHECK(namespace_name<nlohmann::json>(ns) == expected);
    }
#endif
}
