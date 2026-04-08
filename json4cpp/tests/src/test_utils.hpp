//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint> // uint8_t
#include <fstream> // ifstream, istreambuf_iterator, ios
#include <vector> // vector

namespace utils
{

inline std::vector<std::uint8_t> read_binary_file(const std::string& filename)
{
    std::ifstream file(filename, std::ios::binary);
    file.unsetf(std::ios::skipws);

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> byte_vector;
    byte_vector.reserve(static_cast<std::size_t>(size));
    byte_vector.insert(byte_vector.begin(), std::istream_iterator<std::uint8_t>(file), std::istream_iterator<std::uint8_t>());
    return byte_vector;
}

} // namespace utils
