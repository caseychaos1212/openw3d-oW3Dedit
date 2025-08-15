#pragma once

#include <memory>
#include <span>
#include <string>
#include <variant>

#include "ChunkItem.h"

// Parses chunk->data as an array of T. On success returns a span over the
// elements. On failure returns an error description string.
template <typename T>
inline std::variant<std::span<const T>, std::string>
ParseChunkArray(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk) {
        return std::variant<std::span<const T>, std::string>{std::in_place_index<1>,
            "null chunk"};
    }
    const auto& buf = chunk->data;
    if (buf.size() % sizeof(T) != 0) {
        return std::variant<std::span<const T>, std::string>{
            std::in_place_index<1>,
                "size " + std::to_string(buf.size()) +
                " is not a multiple of " + std::to_string(sizeof(T))};
    }
    const T* ptr = reinterpret_cast<const T*>(buf.data());
    size_t count = buf.size() / sizeof(T);
    return std::span<const T>{ptr, count};
}

