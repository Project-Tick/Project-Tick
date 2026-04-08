//     __ _____ _____ _____   _     _   
//  __|  |   __|     |   | |_| |_ _| |_   JSON for Modern C++
// |  |  |__   |  |  | | | |_   _|_   _|  version 10.0.3
// |_____|_____|_____|_|___| |_|   |_|    https://github.com/Project-Tick/Project-Tick
//
// SPDX-FileCopyrightText: 2013-2026 Niels Lohmann <https://nlohmann.me>
// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-License-Identifier: MIT

#pragma once

#include <initializer_list>
#include <utility>

#include <nlohmann/detail/abi_macros.hpp>
#include <nlohmann/detail/meta/type_traits.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

template<typename BasicJsonType>
class json_ref
{
  public:
    using value_type = BasicJsonType;

    json_ref(value_type&& value)
        : owned_value(std::move(value))
    {}

    json_ref(const value_type& value)
        : value_ref(&value)
    {}

    json_ref(std::initializer_list<json_ref> init)
        : owned_value(init)
    {}

    template <
        class... Args,
        enable_if_t<std::is_constructible<value_type, Args...>::value, int> = 0 >
    json_ref(Args && ... args)
        : owned_value(std::forward<Args>(args)...)
    {}

    // class should be movable only
    json_ref(json_ref&&) noexcept = default;
    json_ref(const json_ref&) = delete;
    json_ref& operator=(const json_ref&) = delete;
    json_ref& operator=(json_ref&&) = delete;
    ~json_ref() = default;

    value_type moved_or_copied() const
    {
        if (value_ref == nullptr)
        {
            return std::move(owned_value);
        }
        return *value_ref;
    }

    value_type const& operator*() const
    {
        return value_ref ? *value_ref : owned_value;
    }

    value_type const* operator->() const
    {
        return &** this;
    }

  private:
    mutable value_type owned_value = nullptr;
    value_type const* value_ref = nullptr;
};

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END
