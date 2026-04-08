//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <type_traits> // conditional, is_same

#include <nlohmann/detail/abi_macros.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

/*!
@brief Default base class of the @ref basic_json class.

So that the correct implementations of the copy / move ctors / assign operators
of @ref basic_json do not require complex case distinctions
(no base class / custom base class used as customization point),
@ref basic_json always has a base class.
By default, this class is used because it is empty and thus has no effect
on the behavior of @ref basic_json.
*/
struct json_default_base {};

template<class T>
using json_base_class = typename std::conditional <
                        std::is_same<T, void>::value,
                        json_default_base,
                        T
                        >::type;

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
