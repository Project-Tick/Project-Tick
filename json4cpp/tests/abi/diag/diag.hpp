//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++ (supporting code)
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>

std::size_t json_sizeof_diag_on();
std::size_t json_sizeof_diag_on_explicit();

std::size_t json_sizeof_diag_off();
std::size_t json_sizeof_diag_off_explicit();

void json_at_diag_on();
void json_at_diag_off();
