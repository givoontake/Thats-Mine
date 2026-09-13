#pragma once
#include <cstdint>
#include <memory>

using RoomKey = std::uint64_t;

template <typename T>
using SP = std::shared_ptr<T>;

template <typename T>
using WP = std::weak_ptr<T>;
