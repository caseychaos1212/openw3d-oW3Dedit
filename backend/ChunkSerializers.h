#pragma once

#include <unordered_map>
#include <cstdint>

struct ChunkSerializer;

const std::unordered_map<uint32_t, const ChunkSerializer*>& chunkSerializerRegistry();

#pragma once
