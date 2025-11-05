#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ChunkItem.h"

namespace W3DEdit {

inline void WriteFixedString(char* dest, std::size_t len, std::string_view value) {
    if (len == 0) return;
    std::memset(dest, 0, len);
    const std::size_t copyLen = std::min(len - 1, value.size());
    if (copyLen > 0) {
        std::memcpy(dest, value.data(), copyLen);
    }
}

inline bool UpdateNullTermStringChunk(const std::shared_ptr<ChunkItem>& chunk, const std::string& value) {
    if (!chunk) return false;
    chunk->data.assign(value.begin(), value.end());
    chunk->data.push_back('\0');
    chunk->length = static_cast<uint32_t>(chunk->data.size());
    return true;
}

template <typename T, typename Func>
bool MutateStructChunk(const std::shared_ptr<ChunkItem>& chunk, Func&& mutator, std::string* error = nullptr) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    if (!chunk) {
        if (error) *error = "null chunk";
        return false;
    }
    if (chunk->data.size() < sizeof(T)) {
        if (error) {
            *error = "chunk size " + std::to_string(chunk->data.size())
                + " smaller than struct size " + std::to_string(sizeof(T));
        }
        return false;
    }

    T payload{};
    std::memcpy(&payload, chunk->data.data(), sizeof(T));

    mutator(payload);

    chunk->data.resize(sizeof(T));
    std::memcpy(chunk->data.data(), &payload, sizeof(T));
    chunk->length = static_cast<uint32_t>(chunk->data.size());
    return true;
}

} // namespace W3DEdit
