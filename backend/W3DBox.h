#pragma once
#include "W3DStructs.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>


inline std::vector<ChunkField> InterpretBox(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;
    if (!chunk) return fields;

    // Parse once, validate size
    auto v = ParseChunkStruct<W3dBoxStruct>(chunk);
    if (auto err = std::get_if<std::string>(&v)) {
        fields.emplace_back("error", "string", "Malformed BOX chunk: " + *err);
        return fields;
    }
    const auto& box = std::get<W3dBoxStruct>(v);

    ChunkFieldBuilder B(fields);

    // Version )
    B.Version("Version", box.Version);

    // Raw attributes first
    const uint32_t attr = box.Attributes;
    B.UInt32("Box.Attributes", attr);

    // Individual one-bit flags
    B.Flag(attr, static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_ORIENTED), "W3D_BOX_ATTRIBUTE_ORIENTED");
    B.Flag(attr, static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_ALIGNED), "W3D_BOX_ATTRIBUTE_ALIGNED");

    // Collision type (masked + shifted bitmask)
    {
        const uint32_t mask = static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_MASK);
        const uint32_t shift = static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_SHIFT);
        const uint32_t type = (attr & mask) >> shift;

        // Emit the raw post-shift value for reference
        B.UInt32("CollisionTypeMask", type);

        // Bitwise decomposition (multiple may apply)
        if (type & 0x01) B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PHYSICAL");
        if (type & 0x02) B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PROJECTILE");
        if (type & 0x04) B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VIS");
        if (type & 0x08) B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_CAMERA");
        if (type & 0x10) B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VEHICLE");

        // If any unknown bits are set, surface them explicitly
        if (type & ~0x1Fu) {
            B.Push("CollisionType.UnknownBits", "hex", ToHex(type & ~0x1Fu));
        }

        // If no known bits matched at all, note it
        if ((type & 0x1Fu) == 0) {
            B.Push("CollisionType", "string", "None");
        }
    }


    // Name (fixed-size, NUL-terminated)
    B.Name("Name", box.Name, 2 * W3D_NAME_LEN);

    // Color
    B.RGB("Color", box.Color.R, box.Color.G, box.Color.B);

    // Center / Extent
    B.Vec3("Center", box.Center);
    B.Vec3("Extent", box.Extent);

    return fields;
}
