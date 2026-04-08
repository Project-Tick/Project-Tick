//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <nlohmann/detail/abi_macros.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

template<typename ...Ts> struct make_void
{
    using type = void;
};
template<typename ...Ts> using void_t = typename make_void<Ts...>::type;

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
