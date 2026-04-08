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
#include <exception>
#include <iostream>

struct Foo
{
    int a;
    int b;
};

namespace nlohmann
{
template <>
struct adl_serializer<Foo>
{
    static void to_json(json& j, Foo const& f)
    {
        switch (f.b)
        {
            case 0:
                j["a"] = f.a;
                break;
            case 1:
                j[0] = f.a;
                break;
            default:
                j = "test";
        }
        if (f.a == 1)
        {
            throw std::runtime_error("b is invalid");
        }
    }
};
} // namespace nlohmann

TEST_CASE("check_for_mem_leak_on_adl_to_json-1")
{
    try
    {
        const nlohmann::json j = Foo {1, 0};
        std::cout << j.dump() << "\n";
    }
    catch (...) // NOLINT(bugprone-empty-catch)
    {
        // just ignore the exception in this POC
    }
}

TEST_CASE("check_for_mem_leak_on_adl_to_json-2")
{
    try
    {
        const nlohmann::json j = Foo {1, 1};
        std::cout << j.dump() << "\n";
    }
    catch (...) // NOLINT(bugprone-empty-catch)
    {
        // just ignore the exception in this POC
    }
}

TEST_CASE("check_for_mem_leak_on_adl_to_json-2")
{
    try
    {
        const nlohmann::json j = Foo {1, 2};
        std::cout << j.dump() << "\n";
    }
    catch (...) // NOLINT(bugprone-empty-catch)
    {
        // just ignore the exception in this POC
    }
}


