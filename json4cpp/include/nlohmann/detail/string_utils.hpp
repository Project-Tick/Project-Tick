//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef> // size_t
#include <string> // string, to_string

#include <nlohmann/detail/abi_macros.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

template<typename StringType>
void int_to_string(StringType& target, std::size_t value)
{
    // For ADL
    using std::to_string;
    target = to_string(value);
}

template<typename StringType>
StringType to_string(std::size_t value)
{
    StringType result;
    int_to_string(result, value);
    return result;
}

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
