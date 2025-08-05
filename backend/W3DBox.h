#pragma once
#include "W3DStructs.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>

inline std::vector<ChunkField> InterpretBox(const std::shared_ptr<ChunkItem>& chunk) {
    std::vector<ChunkField> fields;

    // 1) must be big enough to hold our struct
    if (!chunk || chunk->data.size() < sizeof(W3dBoxStruct)) {
        fields.emplace_back("Error", "string", "Box chunk too small");
        return fields;
    }

    // 2) reinterpret the raw bytes as your struct
    auto const* box = reinterpret_cast<const W3dBoxStruct*>(chunk->data.data());

    // 3) Version
    fields.emplace_back("Version", "uint32", std::to_string(box->Version));

    // 4) Raw Attributes in hex
    auto to_hex = [&](uint32_t x, int width) {
        std::ostringstream o;
        o << "0x"
            << std::hex << std::uppercase << std::setw(width) << std::setfill('0')
            << x;
        return o.str();
        };
    uint32_t attr = box->Attributes;
    fields.emplace_back("Attributes", "hex", to_hex(attr, 8));

    // 5) Individual flags
    if (attr & static_cast<uint32_t>(BoxAtrr::W3D_BOX_ATTRIBUTE_ORIENTED)) {
        fields.emplace_back("AttributeFlag", "flag", "W3D_BOX_ATTRIBUTE_ORIENTED");
    }
    if (attr & static_cast<uint32_t>(BoxAtrr::W3D_BOX_ATTRIBUTE_ALIGNED)) {
        fields.emplace_back("AttributeFlag", "flag", "W3D_BOX_ATTRIBUTE_ALIGNED");
    }

    // 6) Collision type bits (mask+shift)
    {
        uint32_t mask = static_cast<uint32_t>(BoxAtrr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_MASK);
        uint32_t shift = static_cast<uint32_t>(BoxAtrr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_SHIFT);
        uint32_t type = (attr & mask) >> shift;

        switch (type) {
        case  1: fields.emplace_back("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PHYSICAL");   break;
        case  2: fields.emplace_back("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PROJECTILE"); break;
        case  4: fields.emplace_back("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VIS");        break;
        case  8: fields.emplace_back("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_CAMERA");     break;
        case 16: fields.emplace_back("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VEHICLE");    break;
        default: fields.emplace_back("CollisionType", "flag", "Unknown");                                     break;
        }
    }

    // 7) Name (null terminated, fixed length)
    {
        size_t n = ::strnlen(box->Name, sizeof(box->Name));
        fields.emplace_back("Name", "string", std::string(box->Name, n));
    }

    // 8) Color (R,G,B)
    {
        auto const& c = box->Color;
        std::ostringstream o;
        o << "("
            << int(c.R) << " "
            << int(c.G) << " "
            << int(c.B) << ")";
        fields.emplace_back("Color", "RGB", o.str());
    }

    // 9) Center and Extent (vector3)
    auto fmt3 = [&](const W3dVectorStruct& v) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(6)
            << v.X << " " << v.Y << " " << v.Z;
        return o.str();
        };
    fields.emplace_back("Center", "vector3", fmt3(box->Center));
    fields.emplace_back("Extent", "vector3", fmt3(box->Extent));

    return fields;
}