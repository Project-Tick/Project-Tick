//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <nlohmann/detail/macro_scope.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
#ifdef JSON_HAS_CPP_17

template<bool... Booleans>
struct cxpr_or_impl : std::integral_constant < bool, (Booleans || ...) > {};

template<bool... Booleans>
struct cxpr_and_impl : std::integral_constant < bool, (Booleans &&...) > {};

#else

template<bool... Booleans>
struct cxpr_or_impl : std::false_type {};

template<bool... Booleans>
struct cxpr_or_impl<true, Booleans...> : std::true_type {};

template<bool... Booleans>
struct cxpr_or_impl<false, Booleans...> : cxpr_or_impl<Booleans...> {};

template<bool... Booleans>
struct cxpr_and_impl : std::true_type {};

template<bool... Booleans>
struct cxpr_and_impl<true, Booleans...> : cxpr_and_impl<Booleans...> {};

template<bool... Booleans>
struct cxpr_and_impl<false, Booleans...> : std::false_type {};

#endif

template<class Boolean>
struct cxpr_not : std::integral_constant < bool, !Boolean::value > {};

template<class... Booleans>
struct cxpr_or : cxpr_or_impl<Booleans::value...> {};

template<bool... Booleans>
struct cxpr_or_c : cxpr_or_impl<Booleans...> {};

template<class... Booleans>
struct cxpr_and : cxpr_and_impl<Booleans::value...> {};

template<bool... Booleans>
struct cxpr_and_c : cxpr_and_impl<Booleans...> {};

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
