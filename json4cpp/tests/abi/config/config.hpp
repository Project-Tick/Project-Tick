//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include "doctest.h"

#include <iostream>
#include <regex>
#include <string>

#define STRINGIZE_EX(x) #x
#define STRINGIZE(x) STRINGIZE_EX(x)

template<typename T>
std::string namespace_name(std::string ns, T* /*unused*/ = nullptr) // NOLINT(performance-unnecessary-value-param)
{
#if DOCTEST_MSVC && !DOCTEST_CLANG
    ns = __FUNCSIG__;
#elif !DOCTEST_CLANG
    ns = __PRETTY_FUNCTION__;
#endif
    std::smatch m;

    // extract the true namespace name from the function signature
    CAPTURE(ns);
    CHECK(std::regex_search(ns, m, std::regex("nlohmann(::[a-zA-Z0-9_]+)*::basic_json")));

    return m.str();
}
