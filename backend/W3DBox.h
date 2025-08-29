#pragma once
#include "W3DStructs.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>

// TODO: Collision should be an attribute add on.
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

    // Collision type (masked + shifted equality)
 //   {
 //       const uint32_t mask = static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_MASK);
 //       const uint32_t shift = static_cast<uint32_t>(BoxAttr::W3D_BOX_ATTRIBUTE_COLLISION_TYPE_SHIFT);
 //       const uint32_t type = (attr & mask) >> shift;
//
 //       // Map the enumerated values after shift
 //       switch (type) {
 //       case  1: B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PHYSICAL");   break;
  //      case  2: B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_PROJECTILE"); break;
  //      case  4: B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VIS");        break;
  //      case  8: B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_CAMERA");     break;
  //      case 16: B.Push("CollisionType", "flag", "W3D_BOX_ATTRIBUTE_COLLISION_TYPE_VEHICLE");    break;
   //     default: B.Push("CollisionType", "string", "Unknown");                                   break;
  //      }
  //  }

    // Name (fixed-size, NUL-terminated)
    B.Name("Name", box.Name, 2 * W3D_NAME_LEN);

    // Color
    B.RGB("Color", box.Color.R, box.Color.G, box.Color.B);

    // Center / Extent
    B.Vec3("Center", box.Center);
    B.Vec3("Extent", box.Extent);

    return fields;
}
