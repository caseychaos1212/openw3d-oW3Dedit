#pragma once

#include <memory>
#include <span>
#include <string>
#include <variant>

#include "ChunkItem.h"

// Parses chunk->data as an array of T. On success returns a span over the
// elements. On failure returns an error description string.
template <typename T>
inline std::variant<std::vector<T>, std::string>
ParseChunkArray(const std::shared_ptr<ChunkItem>& chunk) {
    if (!chunk) return std::variant<std::vector<T>, std::string>{std::in_place_index<1>, "null chunk"};
    const auto& buf = chunk->data;
    if (buf.size() % sizeof(T) != 0) {
        return std::variant<std::vector<T>, std::string>{
            std::in_place_index<1>,
                "size " + std::to_string(buf.size()) + " is not a multiple of " + std::to_string(sizeof(T))
        };
    }
    const std::size_t count = buf.size() / sizeof(T);
    std::vector<T> out(count);
    std::memcpy(out.data(), buf.data(), buf.size());
    return out;
}

template <typename T>
inline std::variant<T, std::string>
ParseChunkStruct(const std::shared_ptr<ChunkItem>& chunk) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    if (!chunk) return std::variant<T, std::string>{std::in_place_index<1>, "null chunk"};
    const auto& buf = chunk->data;
    if (buf.size() < sizeof(T)) {
        return std::variant<T, std::string>{std::in_place_index<1>,
            "size " + std::to_string(buf.size()) + " < sizeof(T) " + std::to_string(sizeof(T))};
    }
    T out{};
    std::memcpy(&out, buf.data(), sizeof(T));
    return out;
}

