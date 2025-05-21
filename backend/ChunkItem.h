#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

class ChunkItem {
public:
    uint32_t id = 0;
    uint32_t length = 0;
    std::string typeName; // Optional: Resolved name for display
    std::vector<uint8_t> data;
    std::vector<std::shared_ptr<ChunkItem>> children;
    ChunkItem* parent = nullptr; // Add this line

    ChunkItem() = default;
};
