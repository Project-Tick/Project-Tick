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
using nlohmann::json;

// ICPC errors out on multibyte character sequences in source files
#ifndef __INTEL_COMPILER
namespace
{
bool wstring_is_utf16();
bool wstring_is_utf16()
{
    return (std::wstring(L"💩") == std::wstring(L"\U0001F4A9"));
}

bool u16string_is_utf16();
bool u16string_is_utf16()
{
    return (std::u16string(u"💩") == std::u16string(u"\U0001F4A9"));
}

bool u32string_is_utf32();
bool u32string_is_utf32()
{
    return (std::u32string(U"💩") == std::u32string(U"\U0001F4A9"));
}
} // namespace

TEST_CASE("wide strings")
{
    SECTION("std::wstring")
    {
        if (wstring_is_utf16())
        {
            std::wstring const w = L"[12.2,\"Ⴥaäö💤🧢\"]";
            json const j = json::parse(w);
            CHECK(j.dump() == "[12.2,\"Ⴥaäö💤🧢\"]");
        }
    }

    SECTION("invalid std::wstring")
    {
        if (wstring_is_utf16())
        {
            std::wstring const w = L"\"\xDBFF";
            json _;
            CHECK_THROWS_AS(_ = json::parse(w), json::parse_error&);
        }
    }

    SECTION("std::u16string")
    {
        if (u16string_is_utf16())
        {
            std::u16string const w = u"[12.2,\"Ⴥaäö💤🧢\"]";
            json const j = json::parse(w);
            CHECK(j.dump() == "[12.2,\"Ⴥaäö💤🧢\"]");
        }
    }

    SECTION("invalid std::u16string")
    {
        if (wstring_is_utf16())
        {
            std::u16string const w = u"\"\xDBFF";
            json _;
            CHECK_THROWS_AS(_ = json::parse(w), json::parse_error&);
        }
    }

    SECTION("std::u32string")
    {
        if (u32string_is_utf32())
        {
            std::u32string const w = U"[12.2,\"Ⴥaäö💤🧢\"]";
            json const j = json::parse(w);
            CHECK(j.dump() == "[12.2,\"Ⴥaäö💤🧢\"]");
        }
    }

    SECTION("invalid std::u32string")
    {
        if (u32string_is_utf32())
        {
            std::u32string const w = U"\"\x110000";
            json _;
            CHECK_THROWS_AS(_ = json::parse(w), json::parse_error&);
        }
    }
}
#endif
