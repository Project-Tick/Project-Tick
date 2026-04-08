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

#define NLOHMANN_JSON_NAMESPACE_NO_VERSION 1
#include <nlohmann/json_fwd.hpp>

TEST_CASE("default namespace without version component")
{
    // GCC 4.8 fails with regex_error
#if !DOCTEST_GCC || DOCTEST_GCC >= DOCTEST_COMPILER(4, 9, 0)
    SECTION("namespace matches expectation")
    {
        std::string expected = "nlohmann::json_abi";

#if JSON_DIAGNOSTICS
        expected += "_diag";
#endif

#if JSON_DIAGNOSTIC_POSITIONS
        expected += "_dp";
#endif

#if JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON
        expected += "_ldvcmp";
#endif

        expected += "::basic_json";

        // fallback for Clang
        const std::string ns{STRINGIZE(NLOHMANN_JSON_NAMESPACE) "::basic_json"};

        CHECK(namespace_name<nlohmann::json>(ns) == expected);
    }
#endif
}
