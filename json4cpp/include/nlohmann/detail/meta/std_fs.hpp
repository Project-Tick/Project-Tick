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

#if JSON_HAS_EXPERIMENTAL_FILESYSTEM
#include <experimental/filesystem>
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
namespace std_fs = std::experimental::filesystem;
}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
#elif JSON_HAS_FILESYSTEM
#include <filesystem> // NOLINT(build/c++17)
NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{
namespace std_fs = std::filesystem;
}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
#endif
