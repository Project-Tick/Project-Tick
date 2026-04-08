//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

/*
This file implements a driver for American Fuzzy Lop (afl-fuzz). It relies on
an implementation of the `LLVMFuzzerTestOneInput` function which processes a
passed byte array.
*/

#include <vector>    // for vector
#include <cstdint>   // for uint8_t
#include <iostream>  // for cin

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main()
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000))
    {
#endif
        // copy stdin to byte vector
        std::vector<uint8_t> vec;
        char c = 0;
        while (std::cin.get(c))
        {
            vec.push_back(static_cast<uint8_t>(c));
        }

        LLVMFuzzerTestOneInput(vec.data(), vec.size());
#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif
}
