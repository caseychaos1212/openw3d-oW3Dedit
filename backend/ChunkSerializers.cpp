#include "ChunkSerializers.h"
#include "ChunkSerializer.h"
#include "ChunkItem.h"
#include "FormatUtils.h"
#include "W3DStructs.h"

#include <QJsonArray>
#include <QByteArray>
#include <QString>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace {

    // helper function: convert array of structs to JSON array (ordered_json)
    template <typename T, typename Converter>
    ordered_json structsToJsonArray(const std::vector<uint8_t>& data, Converter&& conv) {
        ordered_json arr = ordered_json::array();
        if (data.size() % sizeof(T) != 0) {
            return arr;
        }
        const auto* begin = reinterpret_cast<const T*>(data.data());
        int count = static_cast<int>(data.size() / sizeof(T));
        for (int i = 0; i < count; ++i) {
            arr.push_back(conv(begin[i]));
        }
        return arr;
    }
/*
    // Qt compatibility overload
    template <typename T, typename Converter>
    QJsonArray structsToJsonArray(const std::vector<uint8_t>& data, Converter&& conv) {
        QJsonArray arr;
        if (data.size() % sizeof(T) != 0) {
            return arr;
        }
        const auto* begin = reinterpret_cast<const T*>(data.data());
        int count = static_cast<int>(data.size() / sizeof(T));
        for (int i = 0; i < count; ++i) {
            arr.append(conv(begin[i]));
        }
        return arr;
    }
    */

    // helper function: convert JSON array to byte vector of structs (ordered_json)
    template <typename T, typename Converter>
    std::vector<uint8_t> jsonArrayToStructs(const ordered_json& arr, Converter&& conv) {
        std::vector<T> temp(arr.size());
        for (size_t i = 0; i < arr.size(); ++i) {
            temp[i] = conv(arr[i]);
        }
        std::vector<uint8_t> out(arr.size() * sizeof(T));
        if (!out.empty()) {
            std::memcpy(out.data(), temp.data(), out.size());
        }
        return out;
    }
/*
    // Qt compatibility overload
    template <typename T, typename Converter>
    std::vector<uint8_t> jsonArrayToStructs(const QJsonArray& arr, Converter&& conv) {
        std::vector<T> temp(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            temp[i] = conv(arr[i]);
        }
        std::vector<uint8_t> out(arr.size() * sizeof(T));
        if (!out.empty()) {
            std::memcpy(out.data(), temp.data(), out.size());
        }
        return out;
    }
*/
    // Serializer for chunk 0x0001 (W3dMeshHeader1)
    struct MeshHeader1Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json dataObj;
            if (item.data.size() >= sizeof(W3dMeshHeader1)) {
                const auto* h = reinterpret_cast<const W3dMeshHeader1*>(item.data.data());
                dataObj["VERSION"] = FormatUtils::FormatVersion(h->Version);
                dataObj["MESHNAME"] = std::string(h->MeshName, strnlen(h->MeshName, W3D_NAME_LEN));
                dataObj["ATTRIBUTES"] = static_cast<int>(h->Attributes);
                dataObj["NUMTRIANGLES"] = static_cast<int>(h->NumTriangles);
                dataObj["NUMQUADS"] = static_cast<int>(h->NumQuads);
                dataObj["NUMSRTRIS"] = static_cast<int>(h->NumSrTris);
                dataObj["NUMPOVQUADS"] = static_cast<int>(h->NumPovQuads);
                dataObj["NUMVERTICES"] = static_cast<int>(h->NumVertices);
                dataObj["NUMNORMALS"] = static_cast<int>(h->NumNormals);
                dataObj["NUMSRNORMALS"] = static_cast<int>(h->NumSrNormals);
                dataObj["NUMTEXCOORDS"] = static_cast<int>(h->NumTexCoords);
                dataObj["NUMMATERIALS"] = static_cast<int>(h->NumMaterials);
                dataObj["NUMVERTCOLORS"] = static_cast<int>(h->NumVertColors);
                dataObj["NUMVERTINFLUENCES"] = static_cast<int>(h->NumVertInfluences);
                dataObj["NUMDAMAGESTAGES"] = static_cast<int>(h->NumDamageStages);
                ordered_json fc = ordered_json::array();
                for (int i = 0; i < 8; ++i) fc.push_back(static_cast<int>(h->FutureCounts[i]));
                dataObj["FUTURECOUNTS"] = fc;
                dataObj["LODMIN"] = h->LODMin;
                dataObj["LODMAX"] = h->LODMax;
                dataObj["MIN"] = { h->Min.X, h->Min.Y, h->Min.Z };
                dataObj["MAX"] = { h->Max.X, h->Max.Y, h->Max.Z };
                dataObj["SPHCENTER"] = { h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
                dataObj["SPHRADIUS"] = h->SphRadius;
                dataObj["TRANSLATION"] = { h->Translation.X, h->Translation.Y, h->Translation.Z };
                ordered_json rot = ordered_json::array();
                for (int i = 0; i < 9; ++i) rot.push_back(h->Rotation[i]);
                dataObj["ROTATION"] = rot;
                dataObj["MASSCENTER"] = { h->MassCenter.X, h->MassCenter.Y, h->MassCenter.Z };
                ordered_json inertia = ordered_json::array();
                for (int i = 0; i < 9; ++i) inertia.push_back(h->Inertia[i]);
                dataObj["INERTIA"] = inertia;
                dataObj["VOLUME"] = h->Volume;
                dataObj["HIERARCHYTREENAME"] = std::string(h->HierarchyTreeName, strnlen(h->HierarchyTreeName, W3D_NAME_LEN));
                dataObj["HIERARCHYMODELNAME"] = std::string(h->HierarchyModelName, strnlen(h->HierarchyModelName, W3D_NAME_LEN));
                ordered_json fu = ordered_json::array();
                for (int i = 0; i < 24; ++i) fu.push_back(static_cast<int>(h->FutureUse[i]));
                dataObj["FUTUREUSE"] = fu;
            }
            return dataObj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMeshHeader1 h{};
            std::string verStr = dataObj.value("VERSION", "");
            auto pos = verStr.find('.');
            if (pos != std::string::npos) {
                uint32_t major = std::stoul(verStr.substr(0, pos));
                uint32_t minor = std::stoul(verStr.substr(pos + 1));
                h.Version = (major << 16) | minor;
            }
            std::string meshName = dataObj.value("MESHNAME", "");
            std::memset(h.MeshName, 0, 16);
            std::memcpy(h.MeshName, meshName.c_str(), std::min<size_t>(meshName.size(), 16));
            h.Attributes = dataObj.value("ATTRIBUTES", 0);
            h.NumTriangles = dataObj.value("NUMTRIANGLES", 0);
            h.NumQuads = dataObj.value("NUMQUADS", 0);
            h.NumSrTris = dataObj.value("NUMSRTRIS", 0);
            h.NumPovQuads = dataObj.value("NUMPOVQUADS", 0);
            h.NumVertices = dataObj.value("NUMVERTICES", 0);
            h.NumNormals = dataObj.value("NUMNORMALS", 0);
            h.NumSrNormals = dataObj.value("NUMSRNORMALS", 0);
            h.NumTexCoords = dataObj.value("NUMTEXCOORDS", 0);
            h.NumMaterials = dataObj.value("NUMMATERIALS", 0);
            h.NumVertColors = dataObj.value("NUMVERTCOLORS", 0);
            h.NumVertInfluences = dataObj.value("NUMVERTINFLUENCES", 0);
            h.NumDamageStages = dataObj.value("NUMDAMAGESTAGES", 0);
            auto fc = dataObj.value("FUTURECOUNTS", ordered_json::array());
            for (size_t i = 0; i < 8 && i < fc.size(); ++i)
                h.FutureCounts[i] = fc[i].get<int>();
            h.LODMin = dataObj.value("LODMIN", 0.0f);
            h.LODMax = dataObj.value("LODMAX", 0.0f);
            auto min = dataObj.value("MIN", ordered_json::array());
            if (min.size() >= 3) { h.Min.X = min[0].get<float>(); h.Min.Y = min[1].get<float>(); h.Min.Z = min[2].get<float>(); }
            auto max = dataObj.value("MAX", ordered_json::array());
            if (max.size() >= 3) { h.Max.X = max[0].get<float>(); h.Max.Y = max[1].get<float>(); h.Max.Z = max[2].get<float>(); }
            auto sph = dataObj.value("SPHCENTER", ordered_json::array());
            if (sph.size() >= 3) { h.SphCenter.X = sph[0].get<float>(); h.SphCenter.Y = sph[1].get<float>(); h.SphCenter.Z = sph[2].get<float>(); }
            h.SphRadius = dataObj.value("SPHRADIUS", 0.0f);
            auto trans = dataObj.value("TRANSLATION", ordered_json::array());
            if (trans.size() >= 3) { h.Translation.X = trans[0].get<float>(); h.Translation.Y = trans[1].get<float>(); h.Translation.Z = trans[2].get<float>(); }
            auto rot = dataObj.value("ROTATION", ordered_json::array());
            for (size_t i = 0; i < 9 && i < rot.size(); ++i) h.Rotation[i] = rot[i].get<float>();
            auto mass = dataObj.value("MASSCENTER", ordered_json::array());
            if (mass.size() >= 3) { h.MassCenter.X = mass[0].get<float>(); h.MassCenter.Y = mass[1].get<float>(); h.MassCenter.Z = mass[2].get<float>(); }
            auto inertia = dataObj.value("INERTIA", ordered_json::array());
            for (size_t i = 0; i < 9 && i < inertia.size(); ++i) h.Inertia[i] = inertia[i].get<float>();
            h.Volume = dataObj.value("VOLUME", 0.0f);
            std::string ht = dataObj.value("HIERARCHYTREENAME", "");
            std::memset(h.HierarchyTreeName, 0, 16);
            std::memcpy(h.HierarchyTreeName, ht.c_str(), std::min<size_t>(ht.size(), 16));
            std::string hm = dataObj.value("HIERARCHYMODELNAME", "");
            std::memset(h.HierarchyModelName, 0, 16);
            std::memcpy(h.HierarchyModelName, hm.c_str(), std::min<size_t>(hm.size(), 16));
            auto fu = dataObj.value("FUTUREUSE", ordered_json::array());
            for (size_t i = 0; i < 24 && i < fu.size(); ++i) h.FutureUse[i] = fu[i].get<int>();
            item.length = sizeof(W3dMeshHeader1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0002 (VERTICES)
    struct VerticesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json dataObj;
            dataObj["VERTICES"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return ordered_json::array({ v.X, v.Y, v.Z }); }
            );
            return dataObj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("VERTICES", ordered_json::array());
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const ordered_json& val) {
                W3dVectorStruct v{};
                if (val.is_array() && val.size() >= 3) { v.X = val[0].get<float>(); v.Y = val[1].get<float>(); v.Z = val[2].get<float>(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0003 (VERTEX_NORMALS)
    struct VertexNormalsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["VERTEX_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return ordered_json::array({ v.X, v.Y, v.Z }); }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("VERTEX_NORMALS", ordered_json::array());
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const ordered_json& val) {
                W3dVectorStruct v{};
                if (val.is_array() && val.size() >= 3) { v.X = val[0].get<float>(); v.Y = val[1].get<float>(); v.Z = val[2].get<float>(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0004 (SURRENDER_NORMALS)
    struct SurrenderNormalsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["SURRENDER_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return ordered_json::array({ v.X, v.Y, v.Z }); }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("SURRENDER_NORMALS", ordered_json::array());
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const ordered_json& val) {
                W3dVectorStruct v{};
                if (val.is_array() && val.size() >= 3) { v.X = val[0].get<float>(); v.Y = val[1].get<float>(); v.Z = val[2].get<float>(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0005 (TEXCOORDS)
    struct TexCoordsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["TEXCOORDS"] = structsToJsonArray<W3dTexCoordStruct>(
                item.data,
                [](const W3dTexCoordStruct& t) { return ordered_json::array({ t.U, t.V }); }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("TEXCOORDS", ordered_json::array());
            item.data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const ordered_json& val) {
                W3dTexCoordStruct t{};
                if (val.is_array() && val.size() >= 2) { t.U = val[0].get<float>(); t.V = val[1].get<float>(); }
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0006 (MATERIALS)
    struct MaterialsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["MATERIALS"] = structsToJsonArray<W3dMaterial1Struct>(
                item.data,
                [](const W3dMaterial1Struct& m) {
                    ordered_json mo;
                    mo["MATERIALNAME"] = std::string(m.MaterialName, strnlen(m.MaterialName, 16));
                    mo["PRIMARYNAME"] = std::string(m.PrimaryName, strnlen(m.PrimaryName, 16));
                    mo["SECONDARYNAME"] = std::string(m.SecondaryName, strnlen(m.SecondaryName, 16));
                    mo["RENDERFLAGS"] = int(m.RenderFlags);
                    mo["COLOR"] = ordered_json::array({ int(m.Red), int(m.Green), int(m.Blue) });
                    return mo;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("MATERIALS", ordered_json::array());
            item.data = jsonArrayToStructs<W3dMaterial1Struct>(arr, [](const ordered_json& val) {
                W3dMaterial1Struct m{};
                if (val.is_object()) {
                    auto o = val;
                    std::string mn = o.value("MATERIALNAME", "");
                    std::memset(m.MaterialName, 0, sizeof m.MaterialName);
                    std::memcpy(m.MaterialName, mn.c_str(), std::min<size_t>(mn.size(), sizeof m.MaterialName));
                    std::string pn = o.value("PRIMARYNAME", "");
                    std::memset(m.PrimaryName, 0, sizeof m.PrimaryName);
                    std::memcpy(m.PrimaryName, pn.c_str(), std::min<size_t>(pn.size(), sizeof m.PrimaryName));
                    std::string sn = o.value("SECONDARYNAME", "");
                    std::memset(m.SecondaryName, 0, sizeof m.SecondaryName);
                    std::memcpy(m.SecondaryName, sn.c_str(), std::min<size_t>(sn.size(), sizeof m.SecondaryName));
                    m.RenderFlags = o.value("RENDERFLAGS", 0);
                    auto c = o.value("COLOR", ordered_json::array());
                    if (c.size() >= 3) { m.Red = c[0].get<int>(); m.Green = c[1].get<int>(); m.Blue = c[2].get<int>(); }
                }
                return m;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    

    // Serializer for chunk 0x0009 (O_W3D_CHUNK_SURRENDER_TRIANGLES)
    struct SurrenderTrianglesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["SURRENDER_TRIANGLES"] = structsToJsonArray<W3dSurrenderTriStruct>(
                item.data,
                [](const W3dSurrenderTriStruct& t) {
                    ordered_json to;
                    ordered_json vi = ordered_json::array(); for (int i = 0; i < 3; ++i) vi.push_back(int(t.VIndex[i]));
                    to["VINDEX"] = vi;
                    ordered_json tc = ordered_json::array();
                    for (int i = 0; i < 3; ++i) tc.push_back(ordered_json::array({ t.TexCoord[i].U, t.TexCoord[i].V }));
                    to["TEXCOORD"] = tc;
                    to["MATERIALIDX"] = int(t.MaterialIDx);
                    to["NORMAL"] = ordered_json::array({ t.Normal.X, t.Normal.Y, t.Normal.Z });
                    to["ATTRIBUTES"] = int(t.Attributes);
                    ordered_json g = ordered_json::array();
                    for (int i = 0; i < 3; ++i) g.push_back(ordered_json::array({ int(t.Gouraud[i].R), int(t.Gouraud[i].G), int(t.Gouraud[i].B) }));
                    to["GOURAUD"] = g;
                    return to;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("SURRENDER_TRIANGLES", ordered_json::array());
            item.data = jsonArrayToStructs<W3dSurrenderTriStruct>(arr, [](const ordered_json& val) {
                W3dSurrenderTriStruct t{};
                if (val.is_object()) {
                    auto o = val;
                    auto vi = o.value("VINDEX", ordered_json::array());
                    for (size_t i = 0; i < 3 && i < vi.size(); ++i) t.VIndex[i] = vi[i].get<int>();
                    auto tcArr = o.value("TEXCOORD", ordered_json::array());
                    for (size_t i = 0; i < 3 && i < tcArr.size(); ++i) {
                        auto a = tcArr[i];
                        if (a.is_array() && a.size() >= 2) { t.TexCoord[i].U = a[0].get<float>(); t.TexCoord[i].V = a[1].get<float>(); }
                    }
                    t.MaterialIDx = o.value("MATERIALIDX", 0);
                    auto n = o.value("NORMAL", ordered_json::array());
                    if (n.size() >= 3) { t.Normal.X = n[0].get<float>(); t.Normal.Y = n[1].get<float>(); t.Normal.Z = n[2].get<float>(); }
                    t.Attributes = o.value("ATTRIBUTES", 0);
                    auto gArr = o.value("GOURAUD", ordered_json::array());
                    for (size_t i = 0; i < 3 && i < gArr.size(); ++i) {
                        auto ga = gArr[i];
                        if (ga.is_array() && ga.size() >= 3) { t.Gouraud[i].R = ga[0].get<int>(); t.Gouraud[i].G = ga[1].get<int>(); t.Gouraud[i].B = ga[2].get<int>(); }
                    }
                }
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x000C (W3D_CHUNK_MESH_USER_TEXT)
    struct MeshUserTextSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            std::string text(reinterpret_cast<const char*>(item.data.data()), item.data.size());
            obj["TEXT"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            std::string text = dataObj.value("TEXT", "");
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.data(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x000D (W3D_CHUNK_VERTEX_COLORS)
    struct VertexColorsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["VERTEX_COLORS"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return ordered_json::array({ int(c.R), int(c.G), int(c.B) }); }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("VERTEX_COLORS", ordered_json::array());
            item.data = jsonArrayToStructs<W3dRGBStruct>(arr, [](const ordered_json& val) {
                W3dRGBStruct c{};
                if (val.is_array() && val.size() >= 3) { c.R = val[0].get<int>(); c.G = val[1].get<int>(); c.B = val[2].get<int>(); }
                return c;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x000E (W3D_CHUNK_VERTEX_INFLUENCES)
    struct VertexInfluencesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["VERTEX_INFLUENCES"] = structsToJsonArray<W3dVertInfStruct>(
                item.data,
                [](const W3dVertInfStruct& v) {
                    ordered_json o;
                    ordered_json b = ordered_json::array(); for (int i = 0; i < 2; ++i) b.push_back(int(v.BoneIdx[i]));
                    ordered_json w = ordered_json::array(); for (int i = 0; i < 2; ++i) w.push_back(int(v.Weight[i]));
                    o["BONEIDX"] = b;
                    o["WEIGHT"] = w;
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            auto arr = dataObj.value("VERTEX_INFLUENCES", ordered_json::array());
            item.data = jsonArrayToStructs<W3dVertInfStruct>(arr, [](const ordered_json& val) {
                W3dVertInfStruct v{};
                if (val.is_object()) {
                    auto o = val;
                    auto b = o.value("BONEIDX", ordered_json::array());
                    for (size_t i = 0; i < 2 && i < b.size(); ++i) v.BoneIdx[i] = static_cast<uint16_t>(b[i].get<int>());
                    auto w = o.value("WEIGHT", ordered_json::array());
                    for (size_t i = 0; i < 2 && i < w.size(); ++i) v.Weight[i] = static_cast<uint16_t>(w[i].get<int>());
                }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };
    // Serializer for chunk 0x0010 (W3D_CHUNK_DAMAGE_HEADER)
    struct DamageHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dDamageHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dDamageHeaderStruct*>(item.data.data());
                obj["NUMDAMAGEMATERIALS"] = static_cast<int>(h->NumDamageMaterials);
                obj["NUMDAMAGEVERTS"] = static_cast<int>(h->NumDamageVerts);
                obj["NUMDAMAGECOLORS"] = static_cast<int>(h->NumDamageColors);
                obj["DAMAGEINDEX"] = static_cast<int>(h->DamageIndex);

                ordered_json fu = ordered_json::array();
                for (int i = 0; i < 4; ++i) {
                    fu.push_back(static_cast<int>(h->FutureUse[i]));
                }
                obj["FUTUREUSE"] = fu;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dDamageHeaderStruct h{}; // zero-init

            h.NumDamageMaterials = dataObj.value("NUMDAMAGEMATERIALS", 0);
            h.NumDamageVerts = dataObj.value("NUMDAMAGEVERTS", 0);
            h.NumDamageColors = dataObj.value("NUMDAMAGECOLORS", 0);
            h.DamageIndex = dataObj.value("DAMAGEINDEX", 0);

            auto fu = dataObj.value("FUTUREUSE", ordered_json::array());
            for (size_t i = 0; i < 4 && i < fu.size(); ++i) {
                // adjust cast if FutureUse is unsigned
                h.FutureUse[i] = static_cast<int32_t>(fu[i].get<int>());
            }

            item.length = static_cast<uint32_t>(sizeof(W3dDamageHeaderStruct));
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    /*
    // Serializer for chunk 0x0011 (W3D_CHUNK_DAMAGE_VERTICES)
    struct DamageVerticesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DAMAGE_VERTICES"] = structsToJsonArray<W3dMeshDamageVertexStruct>(
                item.data,
                [](const W3dMeshDamageVertexStruct& v) {
                    ordered_json o;
                    o["VERTEXINDEX"] = int(v.VertexIndex);
                    o["NEWVERTEX"] = QJsonArray{ v.NewVertex.X, v.NewVertex.Y, v.NewVertex.Z };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DAMAGE_VERTICES").toArray();
            item.data = jsonArrayToStructs<W3dMeshDamageVertexStruct>(arr, [](const QJsonValue& val) {
                W3dMeshDamageVertexStruct v{};
                ordered_json o = val.toObject();
                v.VertexIndex = o.value("VERTEXINDEX").toInt();
                QJsonArray nv = o.value("NEWVERTEX").toArray();
                if (nv.size() >= 3) { v.NewVertex.X = nv[0].toDouble(); v.NewVertex.Y = nv[1].toDouble(); v.NewVertex.Z = nv[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0012 (W3D_CHUNK_DAMAGE_COLORS)
    struct DamageColorsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DAMAGE_COLORS"] = structsToJsonArray<W3dMeshDamageColorStruct>(
                item.data,
                [](const W3dMeshDamageColorStruct& c) {
                    ordered_json o;
                    o["VERTEXINDEX"] = int(c.VertexIndex);
                    o["NEWCOLOR"] = QJsonArray{ int(c.NewColor.R), int(c.NewColor.G), int(c.NewColor.B) };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DAMAGE_COLORS").toArray();
            item.data = jsonArrayToStructs<W3dMeshDamageColorStruct>(arr, [](const QJsonValue& val) {
                W3dMeshDamageColorStruct c{};
                ordered_json o = val.toObject();
                c.VertexIndex = o.value("VERTEXINDEX").toInt();
                QJsonArray nc = o.value("NEWCOLOR").toArray();
                if (nc.size() >= 3) { c.NewColor.R = nc[0].toInt(); c.NewColor.G = nc[1].toInt(); c.NewColor.B = nc[2].toInt(); }
                return c;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0014 (O_W3D_CHUNK_MATERIALS2)
    struct Materials2Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["MATERIALS2"] = structsToJsonArray<W3dMaterial2Struct>(
                item.data,
                [](const W3dMaterial2Struct& m) {
                    ordered_json mo;
                    mo["MATERIALNAME"] = QString::fromUtf8(m.MaterialName, int(strnlen(m.MaterialName, 16)));
                    mo["PRIMARYNAME"] = QString::fromUtf8(m.PrimaryName, int(strnlen(m.PrimaryName, 16)));
                    mo["SECONDARYNAME"] = QString::fromUtf8(m.SecondaryName, int(strnlen(m.SecondaryName, 16)));
                    mo["RENDERFLAGS"] = int(m.RenderFlags);
                    mo["COLOR"] = QJsonArray{ int(m.Red), int(m.Green), int(m.Blue), int(m.Alpha) };
                    mo["PRIMARYNUMFRAMES"] = int(m.PrimaryNumFrames);
                    mo["SECONDARYNUMFRAMES"] = int(m.SecondaryNumFrames);
                    return mo;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("MATERIALS2").toArray();
            item.data = jsonArrayToStructs<W3dMaterial2Struct>(arr, [](const QJsonValue& val) {
                W3dMaterial2Struct m{};
                ordered_json o = val.toObject();
                auto mn = o.value("MATERIALNAME").toString().toUtf8();
                std::memset(m.MaterialName, 0, sizeof m.MaterialName);
                std::memcpy(m.MaterialName, mn.constData(), std::min<int>(mn.size(), sizeof m.MaterialName));
                auto pn = o.value("PRIMARYNAME").toString().toUtf8();
                std::memset(m.PrimaryName, 0, sizeof m.PrimaryName);
                std::memcpy(m.PrimaryName, pn.constData(), std::min<int>(pn.size(), sizeof m.PrimaryName));
                auto sn = o.value("SECONDARYNAME").toString().toUtf8();
                std::memset(m.SecondaryName, 0, sizeof m.SecondaryName);
                std::memcpy(m.SecondaryName, sn.constData(), std::min<int>(sn.size(), sizeof m.SecondaryName));
                m.RenderFlags = o.value("RENDERFLAGS").toInt();
                QJsonArray c = o.value("COLOR").toArray();
                if (c.size() >= 4) { m.Red = c[0].toInt(); m.Green = c[1].toInt(); m.Blue = c[2].toInt(); m.Alpha = c[3].toInt(); }
                m.PrimaryNumFrames = uint16_t(o.value("PRIMARYNUMFRAMES").toInt());
                m.SecondaryNumFrames = uint16_t(o.value("SECONDARYNUMFRAMES").toInt());
                return m;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0017 (W3D_CHUNK_MATERIAL3_NAME)
    struct Material3NameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0018 (W3D_CHUNK_MATERIAL3_INFO)
    struct Material3InfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMaterial3Struct)) {
                const auto* m = reinterpret_cast<const W3dMaterial3Struct*>(item.data.data());
                obj["MATERIAL3FLAGS"] = int(m->Material3Flags);
                obj["DIFFUSECOLOR"] = QJsonArray{ int(m->DiffuseColor.R), int(m->DiffuseColor.G), int(m->DiffuseColor.B) };
                obj["SPECULARCOLOR"] = QJsonArray{ int(m->SpecularColor.R), int(m->SpecularColor.G), int(m->SpecularColor.B) };
                obj["EMISSIVECOEFFICIENTS"] = QJsonArray{ int(m->EmissiveCoefficients.R), int(m->EmissiveCoefficients.G), int(m->EmissiveCoefficients.B) };
                obj["AMBIENTCOEFFICIENTS"] = QJsonArray{ int(m->AmbientCoefficients.R), int(m->AmbientCoefficients.G), int(m->AmbientCoefficients.B) };
                obj["DIFFUSECOEFFICIENTS"] = QJsonArray{ int(m->DiffuseCoefficients.R), int(m->DiffuseCoefficients.G), int(m->DiffuseCoefficients.B) };
                obj["SPECULARCOEFFICIENTS"] = QJsonArray{ int(m->SpecularCoefficients.R), int(m->SpecularCoefficients.G), int(m->SpecularCoefficients.B) };
                obj["SHININESS"] = m->Shininess;
                obj["OPACITY"] = m->Opacity;
                obj["TRANSLUCENCY"] = m->Translucency;
                obj["FOGCOEFF"] = m->FogCoeff;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMaterial3Struct m{};
            m.Material3Flags = dataObj.value("MATERIAL3FLAGS").toInt();
            QJsonArray dc = dataObj.value("DIFFUSECOLOR").toArray();
            if (dc.size() >= 3) { m.DiffuseColor.R = dc[0].toInt(); m.DiffuseColor.G = dc[1].toInt(); m.DiffuseColor.B = dc[2].toInt(); }
            QJsonArray sc = dataObj.value("SPECULARCOLOR").toArray();
            if (sc.size() >= 3) { m.SpecularColor.R = sc[0].toInt(); m.SpecularColor.G = sc[1].toInt(); m.SpecularColor.B = sc[2].toInt(); }
            QJsonArray ec = dataObj.value("EMISSIVECOEFFICIENTS").toArray();
            if (ec.size() >= 3) { m.EmissiveCoefficients.R = ec[0].toInt(); m.EmissiveCoefficients.G = ec[1].toInt(); m.EmissiveCoefficients.B = ec[2].toInt(); }
            QJsonArray ac = dataObj.value("AMBIENTCOEFFICIENTS").toArray();
            if (ac.size() >= 3) { m.AmbientCoefficients.R = ac[0].toInt(); m.AmbientCoefficients.G = ac[1].toInt(); m.AmbientCoefficients.B = ac[2].toInt(); }
            QJsonArray dc2 = dataObj.value("DIFFUSECOEFFICIENTS").toArray();
            if (dc2.size() >= 3) { m.DiffuseCoefficients.R = dc2[0].toInt(); m.DiffuseCoefficients.G = dc2[1].toInt(); m.DiffuseCoefficients.B = dc2[2].toInt(); }
            QJsonArray sc2 = dataObj.value("SPECULARCOEFFICIENTS").toArray();
            if (sc2.size() >= 3) { m.SpecularCoefficients.R = sc2[0].toInt(); m.SpecularCoefficients.G = sc2[1].toInt(); m.SpecularCoefficients.B = sc2[2].toInt(); }
            m.Shininess = dataObj.value("SHININESS").toDouble();
            m.Opacity = dataObj.value("OPACITY").toDouble();
            m.Translucency = dataObj.value("TRANSLUCENCY").toDouble();
            m.FogCoeff = dataObj.value("FOGCOEFF").toDouble();
            item.length = sizeof(W3dMaterial3Struct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &m, sizeof(m));
        }
    };

    // Serializer for chunk 0x001A (W3D_CHUNK_MAP3_FILENAME)
    struct Map3FilenameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["FILENAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("FILENAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x001B (W3D_CHUNK_MAP3_INFO)
    struct Map3InfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMap3Struct)) {
                const auto* m = reinterpret_cast<const W3dMap3Struct*>(item.data.data());
                obj["MAPPINGTYPE"] = int(m->MappingType);
                obj["FRAMECOUNT"] = int(m->FrameCount);
                obj["FRAMERATE"] = int(m->FrameRate);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMap3Struct m{};
            m.MappingType = uint16_t(dataObj.value("MAPPINGTYPE").toInt());
            m.FrameCount = uint16_t(dataObj.value("FRAMECOUNT").toInt());
            m.FrameRate = dataObj.value("FRAMERATE").toInt();
            item.length = sizeof(W3dMap3Struct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &m, sizeof(m));
        }
    };

    // Serializer for chunk 0x001F (W3D_CHUNK_MESH_HEADER3)
    struct MeshHeader3Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json dataObj;
            if (item.data.size() >= sizeof(W3dMeshHeader3Struct)) {
                const auto* h = reinterpret_cast<const W3dMeshHeader3Struct*>(item.data.data());
                dataObj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                dataObj["ATTRIBUTES"] = int(h->Attributes);
                dataObj["MESHNAME"] = QString::fromUtf8(h->MeshName, strnlen(h->MeshName, W3D_NAME_LEN));
                dataObj["CONTAINERNAME"] = QString::fromUtf8(h->ContainerName, strnlen(h->ContainerName, W3D_NAME_LEN));
                dataObj["NUMTRIS"] = int(h->NumTris);
                dataObj["NUMVERTICES"] = int(h->NumVertices);
                dataObj["NUMMATERIALS"] = int(h->NumMaterials);
                dataObj["NUMDAMAGESTAGES"] = int(h->NumDamageStages);
                dataObj["SORTLEVEL"] = int(h->SortLevel);
                dataObj["PRELITVERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->PrelitVersion));
                QJsonArray fc; for (int i = 0; i < 1; ++i) fc.append(int(h->FutureCounts[i]));
                dataObj["FUTURECOUNTS"] = fc;
                dataObj["VERTEXCHANNELS"] = int(h->VertexChannels);
                dataObj["FACECHANNELS"] = int(h->FaceChannels);
                dataObj["MIN"] = QJsonArray{ h->Min.X, h->Min.Y, h->Min.Z };
                dataObj["MAX"] = QJsonArray{ h->Max.X, h->Max.Y, h->Max.Z };
                dataObj["SPHCENTER"] = QJsonArray{ h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
                dataObj["SPHRADIUS"] = h->SphRadius;
            }
            return dataObj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMeshHeader3Struct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) { h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt(); }
            h.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QByteArray mn = dataObj.value("MESHNAME").toString().toUtf8();
            std::memset(h.MeshName, 0, W3D_NAME_LEN);
            std::memcpy(h.MeshName, mn.constData(), std::min<int>(mn.size(), W3D_NAME_LEN));
            QByteArray cn = dataObj.value("CONTAINERNAME").toString().toUtf8();
            std::memset(h.ContainerName, 0, W3D_NAME_LEN);
            std::memcpy(h.ContainerName, cn.constData(), std::min<int>(cn.size(), W3D_NAME_LEN));
            h.NumTris = dataObj.value("NUMTRIS").toInt();
            h.NumVertices = dataObj.value("NUMVERTICES").toInt();
            h.NumMaterials = dataObj.value("NUMMATERIALS").toInt();
            h.NumDamageStages = dataObj.value("NUMDAMAGESTAGES").toInt();
            h.SortLevel = dataObj.value("SORTLEVEL").toInt();
            QString prStr = dataObj.value("PRELITVERSION").toString();
            parts = prStr.split('.');
            if (parts.size() == 2) { h.PrelitVersion = (parts[0].toUInt() << 16) | parts[1].toUInt(); }
            QJsonArray fc = dataObj.value("FUTURECOUNTS").toArray();
            for (int i = 0; i < 1 && i < fc.size(); ++i) h.FutureCounts[i] = fc[i].toInt();
            h.VertexChannels = dataObj.value("VERTEXCHANNELS").toInt();
            h.FaceChannels = dataObj.value("FACECHANNELS").toInt();
            QJsonArray min = dataObj.value("MIN").toArray();
            if (min.size() >= 3) { h.Min.X = min[0].toDouble(); h.Min.Y = min[1].toDouble(); h.Min.Z = min[2].toDouble(); }
            QJsonArray max = dataObj.value("MAX").toArray();
            if (max.size() >= 3) { h.Max.X = max[0].toDouble(); h.Max.Y = max[1].toDouble(); h.Max.Z = max[2].toDouble(); }
            QJsonArray sph = dataObj.value("SPHCENTER").toArray();
            if (sph.size() >= 3) { h.SphCenter.X = sph[0].toDouble(); h.SphCenter.Y = sph[1].toDouble(); h.SphCenter.Z = sph[2].toDouble(); }
            h.SphRadius = dataObj.value("SPHRADIUS").toDouble();
            item.length = sizeof(W3dMeshHeader3Struct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0020 (W3D_CHUNK_TRIANGLES)
    struct TrianglesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["TRIANGLES"] = structsToJsonArray<W3dTriStruct>(
                item.data,
                [](const W3dTriStruct& t) {
                    ordered_json to;
                    QJsonArray vi; for (int i = 0; i < 3; ++i) vi.append(int(t.Vindex[i]));
                    to["VINDEX"] = vi;
                    to["ATTRIBUTES"] = int(t.Attributes);
                    to["NORMAL"] = QJsonArray{ t.Normal.X, t.Normal.Y, t.Normal.Z };
                    to["DIST"] = t.Dist;
                    return to;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("TRIANGLES").toArray();
            item.data = jsonArrayToStructs<W3dTriStruct>(arr, [](const QJsonValue& val) {
                W3dTriStruct t{};
                ordered_json o = val.toObject();
                QJsonArray vi = o.value("VINDEX").toArray();
                for (int i = 0; i < 3 && i < vi.size(); ++i) t.Vindex[i] = vi[i].toInt();
                t.Attributes = o.value("ATTRIBUTES").toInt();
                QJsonArray n = o.value("NORMAL").toArray();
                if (n.size() >= 3) { t.Normal.X = n[0].toDouble(); t.Normal.Y = n[1].toDouble(); t.Normal.Z = n[2].toDouble(); }
                t.Dist = o.value("DIST").toDouble();
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0021 (W3D_CHUNK_PER_TRI_MATERIALS)
    struct PerTriMaterialsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint16_t) == 0) {
                const auto* begin = reinterpret_cast<const uint16_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint16_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["PER_TRI_MATERIALS"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PER_TRI_MATERIALS").toArray();
            std::vector<uint16_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint16_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint16_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0022 (W3D_CHUNK_VERTEX_SHADE_INDICES)
    struct VertexShadeIndicesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["VERTEX_SHADE_INDICES"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTEX_SHADE_INDICES").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0028 (W3D_CHUNK_MATERIAL_INFO)
    struct MaterialInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMaterialInfoStruct)) {
                const auto* m = reinterpret_cast<const W3dMaterialInfoStruct*>(item.data.data());
                obj["PASSCOUNT"] = int(m->PassCount);
                obj["VERTEXMATERIALCOUNT"] = int(m->VertexMaterialCount);
                obj["SHADERCOUNT"] = int(m->ShaderCount);
                obj["TEXTURECOUNT"] = int(m->TextureCount);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMaterialInfoStruct m{};
            m.PassCount = dataObj.value("PASSCOUNT").toInt();
            m.VertexMaterialCount = dataObj.value("VERTEXMATERIALCOUNT").toInt();
            m.ShaderCount = dataObj.value("SHADERCOUNT").toInt();
            m.TextureCount = dataObj.value("TEXTURECOUNT").toInt();
            item.length = sizeof(W3dMaterialInfoStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &m, sizeof(m));
        }
    };

    // Serializer for chunk 0x0029 (W3D_CHUNK_SHADERS)
    struct ShadersSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["SHADERS"] = structsToJsonArray<W3dShaderStruct>(
                item.data,
                [](const W3dShaderStruct& s) {
                    ordered_json o;
                    o["DEPTHCOMPARE"] = int(s.DepthCompare);
                    o["DEPTHMASK"] = int(s.DepthMask);
                    o["COLORMASK"] = int(s.ColorMask);
                    o["DESTBLEND"] = int(s.DestBlend);
                    o["FOGFUNC"] = int(s.FogFunc);
                    o["PRIGRADIENT"] = int(s.PriGradient);
                    o["SECGRADIENT"] = int(s.SecGradient);
                    o["SRCBLEND"] = int(s.SrcBlend);
                    o["TEXTURING"] = int(s.Texturing);
                    o["DETAILCOLORFUNC"] = int(s.DetailColorFunc);
                    o["DETAILALPHAFUNC"] = int(s.DetailAlphaFunc);
                    o["SHADERPRESET"] = int(s.ShaderPreset);
                    o["ALPHATEST"] = int(s.AlphaTest);
                    o["POSTDETAILCOLORFUNC"] = int(s.PostDetailColorFunc);
                    o["POSTDETAILALPHAFUNC"] = int(s.PostDetailAlphaFunc);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SHADERS").toArray();
            item.data = jsonArrayToStructs<W3dShaderStruct>(arr, [](const QJsonValue& val) {
                W3dShaderStruct s{};
                ordered_json o = val.toObject();
                s.DepthCompare = uint8_t(o.value("DEPTHCOMPARE").toInt());
                s.DepthMask = uint8_t(o.value("DEPTHMASK").toInt());
                s.ColorMask = uint8_t(o.value("COLORMASK").toInt());
                s.DestBlend = uint8_t(o.value("DESTBLEND").toInt());
                s.FogFunc = uint8_t(o.value("FOGFUNC").toInt());
                s.PriGradient = uint8_t(o.value("PRIGRADIENT").toInt());
                s.SecGradient = uint8_t(o.value("SECGRADIENT").toInt());
                s.SrcBlend = uint8_t(o.value("SRCBLEND").toInt());
                s.Texturing = uint8_t(o.value("TEXTURING").toInt());
                s.DetailColorFunc = uint8_t(o.value("DETAILCOLORFUNC").toInt());
                s.DetailAlphaFunc = uint8_t(o.value("DETAILALPHAFUNC").toInt());
                s.ShaderPreset = uint8_t(o.value("SHADERPRESET").toInt());
                s.AlphaTest = uint8_t(o.value("ALPHATEST").toInt());
                s.PostDetailColorFunc = uint8_t(o.value("POSTDETAILCOLORFUNC").toInt());
                s.PostDetailAlphaFunc = uint8_t(o.value("POSTDETAILALPHAFUNC").toInt());
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x002C (W3D_CHUNK_VERTEX_MATERIAL_NAME)
    struct VertexMaterialNameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x002D (W3D_CHUNK_VERTEX_MATERIAL_INFO)
    struct VertexMaterialInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dVertexMaterialStruct)) {
                const auto* m = reinterpret_cast<const W3dVertexMaterialStruct*>(item.data.data());
                obj["ATTRIBUTES"] = int(m->Attributes);
                obj["AMBIENT"] = QJsonArray{ int(m->Ambient.R), int(m->Ambient.G), int(m->Ambient.B) };
                obj["DIFFUSE"] = QJsonArray{ int(m->Diffuse.R), int(m->Diffuse.G), int(m->Diffuse.B) };
                obj["SPECULAR"] = QJsonArray{ int(m->Specular.R), int(m->Specular.G), int(m->Specular.B) };
                obj["EMISSIVE"] = QJsonArray{ int(m->Emissive.R), int(m->Emissive.G), int(m->Emissive.B) };
                obj["SHININESS"] = m->Shininess;
                obj["OPACITY"] = m->Opacity;
                obj["TRANSLUCENCY"] = m->Translucency;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dVertexMaterialStruct m{};
            m.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QJsonArray amb = dataObj.value("AMBIENT").toArray();
            if (amb.size() >= 3) { m.Ambient.R = amb[0].toInt(); m.Ambient.G = amb[1].toInt(); m.Ambient.B = amb[2].toInt(); }
            QJsonArray dif = dataObj.value("DIFFUSE").toArray();
            if (dif.size() >= 3) { m.Diffuse.R = dif[0].toInt(); m.Diffuse.G = dif[1].toInt(); m.Diffuse.B = dif[2].toInt(); }
            QJsonArray spec = dataObj.value("SPECULAR").toArray();
            if (spec.size() >= 3) { m.Specular.R = spec[0].toInt(); m.Specular.G = spec[1].toInt(); m.Specular.B = spec[2].toInt(); }
            QJsonArray emis = dataObj.value("EMISSIVE").toArray();
            if (emis.size() >= 3) { m.Emissive.R = emis[0].toInt(); m.Emissive.G = emis[1].toInt(); m.Emissive.B = emis[2].toInt(); }
            m.Shininess = dataObj.value("SHININESS").toDouble();
            m.Opacity = dataObj.value("OPACITY").toDouble();
            m.Translucency = dataObj.value("TRANSLUCENCY").toDouble();
            item.length = sizeof(W3dVertexMaterialStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &m, sizeof(m));
        }
    };

    // Serializer for chunk 0x002E (W3D_CHUNK_VERTEX_MAPPER_ARGS0)
    struct VertexMapperArgs0Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["ARGS"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("ARGS").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x002F (W3D_CHUNK_VERTEX_MAPPER_ARGS1)
    struct VertexMapperArgs1Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["ARGS"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("ARGS").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0032 (W3D_CHUNK_TEXTURE_NAME)
    struct TextureNameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0033 (W3D_CHUNK_TEXTURE_INFO)
    struct TextureInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dTextureInfoStruct)) {
                const auto* t = reinterpret_cast<const W3dTextureInfoStruct*>(item.data.data());
                obj["ATTRIBUTES"] = int(t->Attributes);
                obj["ANIMTYPE"] = int(t->AnimType);
                obj["FRAMECOUNT"] = int(t->FrameCount);
                obj["FRAMERATE"] = t->FrameRate;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dTextureInfoStruct t{};
            t.Attributes = uint16_t(dataObj.value("ATTRIBUTES").toInt());
            t.AnimType = uint16_t(dataObj.value("ANIMTYPE").toInt());
            t.FrameCount = dataObj.value("FRAMECOUNT").toInt();
            t.FrameRate = float(dataObj.value("FRAMERATE").toDouble());
            item.length = sizeof(W3dTextureInfoStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &t, sizeof(t));
        }
    };

    // Serializer for chunk 0x0039 (W3D_CHUNK_VERTEX_MATERIAL_IDS)
    struct VertexMaterialIdsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["VERTEX_MATERIAL_IDS"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTEX_MATERIAL_IDS").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x003A (W3D_CHUNK_SHADER_IDS)
    struct ShaderIdsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["SHADER_IDS"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SHADER_IDS").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x003B (W3D_CHUNK_DCG)
    struct DcgSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DCG"] = structsToJsonArray<W3dRGBAStruct>(
                item.data,
                [](const W3dRGBAStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B), int(c.A) }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DCG").toArray();
            item.data = jsonArrayToStructs<W3dRGBAStruct>(arr, [](const QJsonValue& val) {
                QJsonArray c = val.toArray();
                W3dRGBAStruct s{
                    uint8_t(c.size() > 0 ? c[0].toInt() : 0),
                    uint8_t(c.size() > 1 ? c[1].toInt() : 0),
                    uint8_t(c.size() > 2 ? c[2].toInt() : 0),
                    uint8_t(c.size() > 3 ? c[3].toInt() : 0)
                };
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x003C (W3D_CHUNK_DIG)
    struct DigSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DIG"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B) }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DIG").toArray();
            item.data = jsonArrayToStructs<W3dRGBStruct>(arr, [](const QJsonValue& val) {
                QJsonArray c = val.toArray();
                W3dRGBStruct s{
                    uint8_t(c.size() > 0 ? c[0].toInt() : 0),
                    uint8_t(c.size() > 1 ? c[1].toInt() : 0),
                    uint8_t(c.size() > 2 ? c[2].toInt() : 0)
                };
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x003E (W3D_CHUNK_SCG)
    struct ScgSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["SCG"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B) }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SCG").toArray();
            item.data = jsonArrayToStructs<W3dRGBStruct>(arr, [](const QJsonValue& val) {
                QJsonArray c = val.toArray();
                W3dRGBStruct s{
                    uint8_t(c.size() > 0 ? c[0].toInt() : 0),
                    uint8_t(c.size() > 1 ? c[1].toInt() : 0),
                    uint8_t(c.size() > 2 ? c[2].toInt() : 0)
                };
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x003F (W3D_CHUNK_SHADER_MATERIAL_ID)
    struct ShaderMaterialIdSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["SHADER_MATERIAL_ID"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SHADER_MATERIAL_ID").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };
    // Serializer for chunk 0x0049 (W3D_CHUNK_TEXTURE_IDS)
    struct TextureIdsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["TEXTURE_IDS"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("TEXTURE_IDS").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x004A (W3D_CHUNK_STAGE_TEXCOORDS)
    struct StageTexCoordsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["STAGE_TEXCOORDS"] = structsToJsonArray<W3dTexCoordStruct>(
                item.data,
                [](const W3dTexCoordStruct& t) { return QJsonArray{ t.U, t.V }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("STAGE_TEXCOORDS").toArray();
            item.data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const QJsonValue& val) {
                W3dTexCoordStruct t{};
                QJsonArray a = val.toArray();
                if (a.size() >= 2) { t.U = a[0].toDouble(); t.V = a[1].toDouble(); }
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x004B (W3D_CHUNK_PER_FACE_TEXCOORD_IDS)
    struct PerFaceTexCoordIdsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["PER_FACE_TEXCOORD_IDS"] = structsToJsonArray<Vector3i>(
                item.data,
                [](const Vector3i& v) { return QJsonArray{ v.I, v.J, v.K }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PER_FACE_TEXCOORD_IDS").toArray();
            item.data = jsonArrayToStructs<Vector3i>(arr, [](const QJsonValue& val) {
                Vector3i v{};
                QJsonArray a = val.toArray();
                if (a.size() >= 3) { v.I = a[0].toInt(); v.J = a[1].toInt(); v.K = a[2].toInt(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0052 (W3D_CHUNK_SHADER_MATERIAL_HEADER)
    struct ShaderMaterialHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dShaderMaterialHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dShaderMaterialHeaderStruct*>(item.data.data());
                obj["VERSION"] = int(h->Version);
                obj["SHADERNAME"] = QString::fromUtf8(h->ShaderName, strnlen(h->ShaderName, 32));
                obj["TECHNIQUE"] = int(h->Technique);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dShaderMaterialHeaderStruct h{};
            h.Version = uint8_t(dataObj.value("VERSION").toInt());
            QByteArray sn = dataObj.value("SHADERNAME").toString().toUtf8();
            std::memset(h.ShaderName, 0, 32);
            std::memcpy(h.ShaderName, sn.constData(), std::min<int>(sn.size(), 32));
            h.Technique = uint8_t(dataObj.value("TECHNIQUE").toInt());
            item.length = sizeof(W3dShaderMaterialHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0053 (W3D_CHUNK_SHADER_MATERIAL_PROPERTY)
    struct ShaderMaterialPropertySerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& data = item.data;
            if (data.size() < 8) return obj;
            const uint8_t* base = data.data();
            uint32_t type = 0; std::memcpy(&type, base, 4);
            uint32_t nameLen = 0; std::memcpy(&nameLen, base + 4, 4);
            if (data.size() < size_t(8) + nameLen) return obj;
            QString name = QString::fromUtf8(reinterpret_cast<const char*>(base + 8), int(nameLen));
            int nul = name.indexOf(QChar('\0')); if (nul != -1) name.truncate(nul);
            obj["TYPE"] = int(type);
            obj["NAME"] = name;
            size_t pos = 8 + nameLen;
            switch (static_cast<ShaderMaterialFlag>(type)) {
            case ShaderMaterialFlag::CONSTANT_TYPE_TEXTURE: {
                if (data.size() >= pos + 4) {
                    uint32_t texLen = 0; std::memcpy(&texLen, base + pos, 4); pos += 4;
                    if (data.size() >= pos + texLen) {
                        QString tex = QString::fromUtf8(reinterpret_cast<const char*>(base + pos), int(texLen));
                        int n2 = tex.indexOf(QChar('\0')); if (n2 != -1) tex.truncate(n2);
                        obj["TEXTURE"] = tex;
                    }
                }
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT1:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT2:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT3:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT4: {
                int count = int(type) - 1;
                if (data.size() >= pos + size_t(count) * 4) {
                    QJsonArray arr;
                    for (int i = 0; i < count; ++i) {
                        float f; std::memcpy(&f, base + pos + i * 4, 4);
                        arr.append(f);
                    }
                    obj["FLOATS"] = arr;
                }
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_INT: {
                if (data.size() >= pos + 4) {
                    uint32_t v; std::memcpy(&v, base + pos, 4);
                    obj["INT"] = int(v);
                }
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_BOOL: {
                if (data.size() >= pos + 4) {
                    uint32_t v; std::memcpy(&v, base + pos, 4);
                    obj["BOOL"] = (v != 0);
                }
                break;
            }
            default:
                break;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            uint32_t type = uint32_t(dataObj.value("TYPE").toInt());
            QByteArray nameBytes = dataObj.value("NAME").toString().toUtf8();
            std::vector<uint8_t> buf;
            auto appendU32 = [&](uint32_t v) {
                size_t p = buf.size(); buf.resize(p + 4); std::memcpy(buf.data() + p, &v, 4);
                };
            auto appendBytes = [&](const QByteArray& b) {
                size_t p = buf.size(); buf.resize(p + b.size()); std::memcpy(buf.data() + p, b.constData(), b.size());
                };
            appendU32(type);
            appendU32(uint32_t(nameBytes.size() + 1));
            appendBytes(nameBytes);
            buf.push_back(0);

            switch (static_cast<ShaderMaterialFlag>(type)) {
            case ShaderMaterialFlag::CONSTANT_TYPE_TEXTURE: {
                QByteArray texBytes = dataObj.value("TEXTURE").toString().toUtf8();
                appendU32(uint32_t(texBytes.size() + 1));
                appendBytes(texBytes);
                buf.push_back(0);
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT1:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT2:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT3:
            case ShaderMaterialFlag::CONSTANT_TYPE_FLOAT4: {
                int count = int(type) - 1;
                QJsonArray arr = dataObj.value("FLOATS").toArray();
                for (int i = 0; i < count; ++i) {
                    float f = i < arr.size() ? float(arr[i].toDouble()) : 0.0f;
                    size_t p = buf.size(); buf.resize(p + 4); std::memcpy(buf.data() + p, &f, 4);
                }
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_INT: {
                uint32_t v = uint32_t(dataObj.value("INT").toInt());
                appendU32(v);
                break;
            }
            case ShaderMaterialFlag::CONSTANT_TYPE_BOOL: {
                uint32_t v = dataObj.value("BOOL").toBool() ? 1u : 0u;
                appendU32(v);
                break;
            }
            default:
                break;
            }

            item.data = std::move(buf);
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0058 (W3D_CHUNK_DEFORM)
    struct DeformSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMeshDeform)) {
                const auto* d = reinterpret_cast<const W3dMeshDeform*>(item.data.data());
                obj["SETCOUNT"] = int(d->SetCount);
                obj["ALPHAPASSES"] = int(d->AlphaPasses);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMeshDeform d{};
            d.SetCount = dataObj.value("SETCOUNT").toInt();
            d.AlphaPasses = dataObj.value("ALPHAPASSES").toInt();
            item.length = sizeof(W3dMeshDeform);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &d, sizeof(d));
        }
    };

    // Serializer for chunk 0x0059 (W3D_CHUNK_DEFORM_SET)
    struct DeformSetSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DEFORM_SET"] = structsToJsonArray<W3dDeformSetInfo>(
                item.data,
                [](const W3dDeformSetInfo& s) {
                    ordered_json o; o["KEYFRAMECOUNT"] = int(s.KeyframeCount); o["FLAGS"] = int(s.flags); return o; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_SET").toArray();
            item.data = jsonArrayToStructs<W3dDeformSetInfo>(arr, [](const QJsonValue& val) {
                W3dDeformSetInfo s{};
                ordered_json o = val.toObject();
                s.KeyframeCount = o.value("KEYFRAMECOUNT").toInt();
                s.flags = o.value("FLAGS").toInt();
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x005A (W3D_CHUNK_DEFORM_KEYFRAME)
    struct DeformKeyframeSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DEFORM_KEYFRAME"] = structsToJsonArray<W3dDeformKeyframeInfo>(
                item.data,
                [](const W3dDeformKeyframeInfo& k) {
                    ordered_json o; o["DEFORMPERCENT"] = k.DeformPercent; o["DATACOUNT"] = int(k.DataCount); return o; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_KEYFRAME").toArray();
            item.data = jsonArrayToStructs<W3dDeformKeyframeInfo>(arr, [](const QJsonValue& val) {
                W3dDeformKeyframeInfo k{};
                ordered_json o = val.toObject();
                k.DeformPercent = float(o.value("DEFORMPERCENT").toDouble());
                k.DataCount = o.value("DATACOUNT").toInt();
                return k;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x005B (W3D_CHUNK_DEFORM_DATA)
    struct DeformDataSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["DEFORM_DATA"] = structsToJsonArray<W3dDeformData>(
                item.data,
                [](const W3dDeformData& d) {
                    ordered_json o;
                    o["VERTEXINDEX"] = int(d.VertexIndex);
                    o["POSITION"] = QJsonArray{ d.Position.X, d.Position.Y, d.Position.Z };
                    o["COLOR"] = QJsonArray{ int(d.Color.R), int(d.Color.G), int(d.Color.B), int(d.Color.A) };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_DATA").toArray();
            item.data = jsonArrayToStructs<W3dDeformData>(arr, [](const QJsonValue& val) {
                W3dDeformData d{};
                ordered_json o = val.toObject();
                d.VertexIndex = o.value("VERTEXINDEX").toInt();
                QJsonArray p = o.value("POSITION").toArray();
                if (p.size() >= 3) { d.Position.X = p[0].toDouble(); d.Position.Y = p[1].toDouble(); d.Position.Z = p[2].toDouble(); }
                QJsonArray c = o.value("COLOR").toArray();
                if (c.size() >= 4) { d.Color.R = uint8_t(c[0].toInt()); d.Color.G = uint8_t(c[1].toInt()); d.Color.B = uint8_t(c[2].toInt()); d.Color.A = uint8_t(c[3].toInt()); }
                return d;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0060 (W3D_CHUNK_TANGENTS)
    struct TangentsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["TANGENTS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("TANGENTS").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                QJsonArray a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0061 (W3D_CHUNK_BINORMALS)
    struct BinormalsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["BINORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("BINORMALS").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                QJsonArray a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0080 (W3D_CHUNK_PS2_SHADERS)
    struct Ps2ShadersSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["PS2_SHADERS"] = structsToJsonArray<W3dPS2ShaderStruct>(
                item.data,
                [](const W3dPS2ShaderStruct& s) {
                    ordered_json o;
                    o["DEPTH_COMPARE"] = int(s.DepthCompare);
                    o["DEPTH_MASK"] = int(s.DepthMask);
                    o["PRI_GRADIENT"] = int(s.PriGradient);
                    o["TEXTURING"] = int(s.Texturing);
                    o["ALPHA_TEST"] = int(s.AlphaTest);
                    o["APARAM"] = int(s.AParam);
                    o["BPARAM"] = int(s.BParam);
                    o["CPARAM"] = int(s.CParam);
                    o["DPARAM"] = int(s.DParam);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PS2_SHADERS").toArray();
            item.data = jsonArrayToStructs<W3dPS2ShaderStruct>(arr, [](const QJsonValue& val) {
                W3dPS2ShaderStruct s{};
                ordered_json o = val.toObject();
                s.DepthCompare = uint8_t(o.value("DEPTH_COMPARE").toInt());
                s.DepthMask = uint8_t(o.value("DEPTH_MASK").toInt());
                s.PriGradient = uint8_t(o.value("PRI_GRADIENT").toInt());
                s.Texturing = uint8_t(o.value("TEXTURING").toInt());
                s.AlphaTest = uint8_t(o.value("ALPHA_TEST").toInt());
                s.AParam = uint8_t(o.value("APARAM").toInt());
                s.BParam = uint8_t(o.value("BPARAM").toInt());
                s.CParam = uint8_t(o.value("CPARAM").toInt());
                s.DParam = uint8_t(o.value("DPARAM").toInt());
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0091 (W3D_CHUNK_AABTREE_HEADER)
    struct AABTreeHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMeshAABTreeHeader)) {
                const auto* h = reinterpret_cast<const W3dMeshAABTreeHeader*>(item.data.data());
                obj["NODECOUNT"] = int(h->NodeCount);
                obj["POLYCOUNT"] = int(h->PolyCount);
                QJsonArray pad;
                for (int i = 0; i < 6; ++i) pad.append(int(h->Padding[i]));
                obj["PADDING"] = pad;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMeshAABTreeHeader h{};
            h.NodeCount = dataObj.value("NODECOUNT").toInt();
            h.PolyCount = dataObj.value("POLYCOUNT").toInt();
            QJsonArray pad = dataObj.value("PADDING").toArray();
            for (int i = 0; i < 6 && i < pad.size(); ++i) h.Padding[i] = pad[i].toInt();
            item.length = sizeof(W3dMeshAABTreeHeader);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0092 (W3D_CHUNK_AABTREE_POLYINDICES)
    struct AABTreePolyIndicesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["POLY_INDICES"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("POLY_INDICES").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0093 (W3D_CHUNK_AABTREE_NODES)
    struct AABTreeNodesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["AABTREE_NODES"] = structsToJsonArray<W3dMeshAABTreeNode>(
                item.data,
                [](const W3dMeshAABTreeNode& n) {
                    ordered_json o;
                    o["MIN"] = QJsonArray{ n.Min.X, n.Min.Y, n.Min.Z };
                    o["MAX"] = QJsonArray{ n.Max.X, n.Max.Y, n.Max.Z };
                    o["FRONTORPOLY0"] = int(n.FrontOrPoly0);
                    o["BACKORPOLYCOUNT"] = int(n.BackOrPolyCount);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("AABTREE_NODES").toArray();
            item.data = jsonArrayToStructs<W3dMeshAABTreeNode>(arr, [](const QJsonValue& val) {
                W3dMeshAABTreeNode n{};
                ordered_json o = val.toObject();
                QJsonArray min = o.value("MIN").toArray();
                if (min.size() >= 3) { n.Min.X = min[0].toDouble(); n.Min.Y = min[1].toDouble(); n.Min.Z = min[2].toDouble(); }
                QJsonArray max = o.value("MAX").toArray();
                if (max.size() >= 3) { n.Max.X = max[0].toDouble(); n.Max.Y = max[1].toDouble(); n.Max.Z = max[2].toDouble(); }
                n.FrontOrPoly0 = uint32_t(o.value("FRONTORPOLY0").toInt());
                n.BackOrPolyCount = uint32_t(o.value("BACKORPOLYCOUNT").toInt());
                return n;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0101 (W3D_CHUNK_HIERARCHY_HEADER)
    struct HierarchyHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHierarchyStruct)) {
                const auto* h = reinterpret_cast<const W3dHierarchyStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["NUMPIVOTS"] = int(h->NumPivots);
                obj["CENTER"] = QJsonArray{ h->Center.X, h->Center.Y, h->Center.Z };
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHierarchyStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            h.NumPivots = dataObj.value("NUMPIVOTS").toInt();
            QJsonArray center = dataObj.value("CENTER").toArray();
            if (center.size() >= 3) { h.Center.X = center[0].toDouble(); h.Center.Y = center[1].toDouble(); h.Center.Z = center[2].toDouble(); }
            item.length = sizeof(W3dHierarchyStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0102 (W3D_CHUNK_PIVOTS)
    struct PivotsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["PIVOTS"] = structsToJsonArray<W3dPivotStruct>(
                item.data,
                [](const W3dPivotStruct& p) {
                    ordered_json o;
                    o["NAME"] = QString::fromUtf8(p.Name, strnlen(p.Name, W3D_NAME_LEN));
                    o["PARENTIDX"] = int(p.ParentIdx);
                    o["TRANSLATION"] = QJsonArray{ p.Translation.X, p.Translation.Y, p.Translation.Z };
                    o["EULERANGLES"] = QJsonArray{ p.EulerAngles.X, p.EulerAngles.Y, p.EulerAngles.Z };
                    QJsonArray rot;
                    for (int i = 0; i < 4; ++i) rot.append(p.Rotation.Q[i]);
                    o["ROTATION"] = rot;
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOTS").toArray();
            item.data = jsonArrayToStructs<W3dPivotStruct>(arr, [](const QJsonValue& val) {
                W3dPivotStruct p{};
                ordered_json o = val.toObject();
                QByteArray name = o.value("NAME").toString().toUtf8();
                std::memset(p.Name, 0, W3D_NAME_LEN);
                std::memcpy(p.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
                p.ParentIdx = uint32_t(o.value("PARENTIDX").toInt());
                QJsonArray trans = o.value("TRANSLATION").toArray();
                if (trans.size() >= 3) { p.Translation.X = trans[0].toDouble(); p.Translation.Y = trans[1].toDouble(); p.Translation.Z = trans[2].toDouble(); }
                QJsonArray euler = o.value("EULERANGLES").toArray();
                if (euler.size() >= 3) { p.EulerAngles.X = euler[0].toDouble(); p.EulerAngles.Y = euler[1].toDouble(); p.EulerAngles.Z = euler[2].toDouble(); }
                QJsonArray rot = o.value("ROTATION").toArray();
                for (int i = 0; i < 4 && i < rot.size(); ++i) p.Rotation.Q[i] = rot[i].toDouble();
                return p;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0103 (W3D_CHUNK_PIVOT_FIXUPS)
    struct PivotFixupsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["PIVOT_FIXUPS"] = structsToJsonArray<W3dPivotFixupStruct>(
                item.data,
                [](const W3dPivotFixupStruct& f) {
                    ordered_json o;
                    QJsonArray tm;
                    for (int i = 0; i < 4; ++i) {
                        QJsonArray row;
                        for (int j = 0; j < 3; ++j) row.append(f.TM[i][j]);
                        tm.append(row);
                    }
                    o["TM"] = tm;
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOT_FIXUPS").toArray();
            item.data = jsonArrayToStructs<W3dPivotFixupStruct>(arr, [](const QJsonValue& val) {
                W3dPivotFixupStruct f{};
                ordered_json o = val.toObject();
                QJsonArray tm = o.value("TM").toArray();
                for (int i = 0; i < 4 && i < tm.size(); ++i) {
                    QJsonArray row = tm[i].toArray();
                    for (int j = 0; j < 3 && j < row.size(); ++j) f.TM[i][j] = row[j].toDouble();
                }
                return f;
                });
            item.length = uint32_t(item.data.size());
        }
    };
    // Serializer for chunk 0x0201 (W3D_CHUNK_ANIMATION_HEADER)
    struct AnimationHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dAnimHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dAnimHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["HIERARCHYNAME"] = QString::fromUtf8(h->HierarchyName, strnlen(h->HierarchyName, W3D_NAME_LEN));
                obj["NUMFRAMES"] = int(h->NumFrames);
                obj["FRAMERATE"] = int(h->FrameRate);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dAnimHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            QByteArray hname = dataObj.value("HIERARCHYNAME").toString().toUtf8();
            std::memset(h.HierarchyName, 0, W3D_NAME_LEN);
            std::memcpy(h.HierarchyName, hname.constData(), std::min<int>(hname.size(), W3D_NAME_LEN));
            h.NumFrames = dataObj.value("NUMFRAMES").toInt();
            h.FrameRate = dataObj.value("FRAMERATE").toInt();
            item.length = sizeof(W3dAnimHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0202 (W3D_CHUNK_ANIMATION_CHANNEL)
    struct AnimationChannelSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& buf = item.data;
            if (buf.size() >= sizeof(W3dAnimChannelStruct)) {
                W3dAnimChannelStruct hdr{};
                std::memcpy(&hdr, buf.data(), sizeof(W3dAnimChannelStruct));
                obj["FIRSTFRAME"] = int(hdr.FirstFrame);
                obj["LASTFRAME"] = int(hdr.LastFrame);
                obj["VECTORLEN"] = int(hdr.VectorLen);
                obj["FLAGS"] = int(hdr.Flags);
                obj["PIVOT"] = int(hdr.Pivot);

                int frameCount = int(hdr.LastFrame) - int(hdr.FirstFrame) + 1;
                int vectorLen = int(hdr.VectorLen);
                size_t valueCount = size_t(frameCount) * size_t(vectorLen);
                size_t headerBytes = sizeof(W3dAnimChannelStruct);
                size_t neededBytes = headerBytes + (valueCount > 0 ? (valueCount - 1) * sizeof(float) : 0);
                if (buf.size() >= neededBytes && valueCount > 0) {
                    std::vector<float> values(valueCount);
                    values[0] = hdr.Data[0];
                    if (valueCount > 1) {
                        const float* tail = reinterpret_cast<const float*>(buf.data() + headerBytes);
                        std::memcpy(values.data() + 1, tail, (valueCount - 1) * sizeof(float));
                    }
                    QJsonArray dataArr;
                    for (int f = 0; f < frameCount; ++f) {
                        QJsonArray frameArr;
                        for (int v = 0; v < vectorLen; ++v) {
                            frameArr.append(values[size_t(f) * vectorLen + v]);
                        }
                        dataArr.append(frameArr);
                    }
                    obj["DATA"] = dataArr;
                }
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dAnimChannelStruct hdr{};
            hdr.FirstFrame = dataObj.value("FIRSTFRAME").toInt();
            hdr.LastFrame = dataObj.value("LASTFRAME").toInt();
            hdr.VectorLen = dataObj.value("VECTORLEN").toInt();
            hdr.Flags = dataObj.value("FLAGS").toInt();
            hdr.Pivot = dataObj.value("PIVOT").toInt();
            hdr.pad = 0;

            QJsonArray dataArr = dataObj.value("DATA").toArray();
            int frameCount = dataArr.size();
            int vectorLen = int(hdr.VectorLen);
            size_t valueCount = size_t(frameCount) * size_t(vectorLen);
            std::vector<float> values(valueCount);
            for (int f = 0; f < frameCount; ++f) {
                QJsonArray frameArr = dataArr[f].toArray();
                for (int v = 0; v < vectorLen && v < frameArr.size(); ++v) {
                    values[size_t(f) * vectorLen + v] = float(frameArr[v].toDouble());
                }
            }

            hdr.Data[0] = valueCount > 0 ? values[0] : 0.0f;
            size_t headerBytes = sizeof(W3dAnimChannelStruct);
            item.length = uint32_t(headerBytes + (valueCount > 1 ? (valueCount - 1) * sizeof(float) : 0));
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &hdr, sizeof(W3dAnimChannelStruct));
            if (valueCount > 1) {
                std::memcpy(item.data.data() + headerBytes, values.data() + 1, (valueCount - 1) * sizeof(float));
            }
        }
    };

    // Serializer for chunk 0x0203 (W3D_CHUNK_BIT_CHANNEL)
    struct BitChannelSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& buf = item.data;
            if (buf.size() >= sizeof(W3dBitChannelStruct)) {
                W3dBitChannelStruct hdr{};
                std::memcpy(&hdr, buf.data(), sizeof(W3dBitChannelStruct));
                obj["FIRSTFRAME"] = int(hdr.FirstFrame);
                obj["LASTFRAME"] = int(hdr.LastFrame);
                obj["FLAGS"] = int(hdr.Flags);
                obj["PIVOT"] = int(hdr.Pivot);
                obj["DEFAULTVAL"] = int(hdr.DefaultVal);

                int count = int(hdr.LastFrame) - int(hdr.FirstFrame) + 1;
                size_t bitBytes = size_t((count + 7) / 8);
                size_t headerBytes = sizeof(W3dBitChannelStruct);
                size_t neededBytes = (headerBytes - 1) + bitBytes;
                if (buf.size() >= neededBytes && count > 0) {
                    std::vector<uint8_t> bits(bitBytes);
                    bits[0] = hdr.Data[0];
                    if (bitBytes > 1) {
                        const uint8_t* tail = reinterpret_cast<const uint8_t*>(buf.data() + headerBytes);
                        std::memcpy(bits.data() + 1, tail, bitBytes - 1);
                    }
                    QJsonArray arr;
                    for (int i = 0; i < count; ++i) {
                        bool val = (bits[size_t(i) / 8] >> (i % 8)) & 1;
                        arr.append(val);
                    }
                    obj["DATA"] = arr;
                }
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dBitChannelStruct hdr{};
            hdr.FirstFrame = dataObj.value("FIRSTFRAME").toInt();
            hdr.LastFrame = dataObj.value("LASTFRAME").toInt();
            hdr.Flags = dataObj.value("FLAGS").toInt();
            hdr.Pivot = dataObj.value("PIVOT").toInt();
            hdr.DefaultVal = uint8_t(dataObj.value("DEFAULTVAL").toInt());

            QJsonArray arr = dataObj.value("DATA").toArray();
            int count = arr.size();
            size_t bitBytes = size_t((count + 7) / 8);
            std::vector<uint8_t> bits(bitBytes, 0);
            for (int i = 0; i < count; ++i) {
                if (arr[i].toBool()) bits[size_t(i) / 8] |= uint8_t(1 << (i % 8));
            }

            std::vector<uint8_t> out((sizeof(W3dBitChannelStruct) - 1) + bitBytes);
            std::memcpy(out.data(), &hdr, sizeof(W3dBitChannelStruct) - 1);
            if (bitBytes > 0) out[sizeof(W3dBitChannelStruct) - 1] = bits[0];
            if (bitBytes > 1) std::memcpy(out.data() + sizeof(W3dBitChannelStruct), bits.data() + 1, bitBytes - 1);
            item.data = std::move(out);
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0281 (W3D_CHUNK_COMPRESSED_ANIMATION_HEADER)
    struct CompressedAnimHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dCompressedAnimHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dCompressedAnimHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["HIERARCHYNAME"] = QString::fromUtf8(h->HierarchyName, strnlen(h->HierarchyName, W3D_NAME_LEN));
                obj["NUMFRAMES"] = int(h->NumFrames);
                obj["FRAMERATE"] = int(h->FrameRate);
                obj["FLAVOR"] = int(h->Flavor);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dCompressedAnimHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            QByteArray hname = dataObj.value("HIERARCHYNAME").toString().toUtf8();
            std::memset(h.HierarchyName, 0, W3D_NAME_LEN);
            std::memcpy(h.HierarchyName, hname.constData(), std::min<int>(hname.size(), W3D_NAME_LEN));
            h.NumFrames = dataObj.value("NUMFRAMES").toInt();
            h.FrameRate = uint16_t(dataObj.value("FRAMERATE").toInt());
            h.Flavor = uint16_t(dataObj.value("FLAVOR").toInt());
            item.length = sizeof(W3dCompressedAnimHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0282 (W3D_CHUNK_COMPRESSED_ANIMATION_CHANNEL)
    struct CompressedAnimChannelSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& buf = item.data;
            if (buf.size() >= 8) {
                const uint32_t* u32 = reinterpret_cast<const uint32_t*>(buf.data());
                obj["NUMTIMECODES"] = int(u32[0]);
                obj["PIVOT"] = int(*reinterpret_cast<const uint16_t*>(buf.data() + 4));
                obj["VECTORLEN"] = int(*(buf.data() + 6));
                obj["FLAGS"] = int(*(buf.data() + 7));
                size_t count = (buf.size() - 8) / 4;
                const uint32_t* data = reinterpret_cast<const uint32_t*>(buf.data() + 8);
                QJsonArray arr;
                for (size_t i = 0; i < count; ++i) arr.append(int(data[i]));
                obj["DATA"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            uint32_t numTimeCodes = dataObj.value("NUMTIMECODES").toInt();
            uint16_t pivot = uint16_t(dataObj.value("PIVOT").toInt());
            uint8_t vectorLen = uint8_t(dataObj.value("VECTORLEN").toInt());
            uint8_t flags = uint8_t(dataObj.value("FLAGS").toInt());
            QJsonArray arr = dataObj.value("DATA").toArray();
            std::vector<uint32_t> data(arr.size());
            for (int i = 0; i < arr.size(); ++i) data[i] = uint32_t(arr[i].toInt());
            std::vector<uint8_t> out(8 + data.size() * 4);
            std::memcpy(out.data(), &numTimeCodes, 4);
            std::memcpy(out.data() + 4, &pivot, 2);
            out[6] = vectorLen;
            out[7] = flags;
            if (!data.empty()) std::memcpy(out.data() + 8, data.data(), data.size() * 4);
            item.data = std::move(out);
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0283 (W3D_CHUNK_COMPRESSED_BIT_CHANNEL)
    struct CompressedBitChannelSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& buf = item.data;
            if (buf.size() >= 8) {
                const uint32_t* u32 = reinterpret_cast<const uint32_t*>(buf.data());
                obj["NUMTIMECODES"] = int(u32[0]);
                obj["PIVOT"] = int(*reinterpret_cast<const uint16_t*>(buf.data() + 4));
                obj["FLAGS"] = int(*(buf.data() + 6));
                obj["DEFAULTVAL"] = int(*(buf.data() + 7));
                size_t count = (buf.size() - 8) / 4;
                const uint32_t* data = reinterpret_cast<const uint32_t*>(buf.data() + 8);
                QJsonArray arr;
                for (size_t i = 0; i < count; ++i) arr.append(int(data[i]));
                obj["DATA"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            uint32_t numTimeCodes = dataObj.value("NUMTIMECODES").toInt();
            uint16_t pivot = uint16_t(dataObj.value("PIVOT").toInt());
            uint8_t flags = uint8_t(dataObj.value("FLAGS").toInt());
            uint8_t defVal = uint8_t(dataObj.value("DEFAULTVAL").toInt());
            QJsonArray arr = dataObj.value("DATA").toArray();
            std::vector<uint32_t> data(arr.size());
            for (int i = 0; i < arr.size(); ++i) data[i] = uint32_t(arr[i].toInt());
            std::vector<uint8_t> out(8 + data.size() * 4);
            std::memcpy(out.data(), &numTimeCodes, 4);
            std::memcpy(out.data() + 4, &pivot, 2);
            out[6] = flags;
            out[7] = defVal;
            if (!data.empty()) std::memcpy(out.data() + 8, data.data(), data.size() * 4);
            item.data = std::move(out);
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0284 (W3D_CHUNK_COMPRESSED_ANIMATION_MOTION_CHANNEL)
    struct CompressedAnimMotionChannelSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const auto& buf = item.data;
            if (buf.size() >= 12) {
                uint32_t numFrames = *reinterpret_cast<const uint32_t*>(buf.data());
                uint16_t pivot = *reinterpret_cast<const uint16_t*>(buf.data() + 4);
                uint8_t vectorLen = *(buf.data() + 6);
                uint8_t flags = *(buf.data() + 7);
                float scale = *reinterpret_cast<const float*>(buf.data() + 8);
                obj["NUMFRAMES"] = int(numFrames);
                obj["PIVOT"] = int(pivot);
                obj["VECTORLEN"] = int(vectorLen);
                obj["FLAGS"] = int(flags);
                obj["SCALE"] = scale;
                size_t count = (buf.size() - 12) / 4;
                const uint32_t* data = reinterpret_cast<const uint32_t*>(buf.data() + 12);
                QJsonArray arr;
                for (size_t i = 0; i < count; ++i) arr.append(int(data[i]));
                obj["DATA"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            uint32_t numFrames = dataObj.value("NUMFRAMES").toInt();
            uint16_t pivot = uint16_t(dataObj.value("PIVOT").toInt());
            uint8_t vectorLen = uint8_t(dataObj.value("VECTORLEN").toInt());
            uint8_t flags = uint8_t(dataObj.value("FLAGS").toInt());
            float scale = float(dataObj.value("SCALE").toDouble());
            QJsonArray arr = dataObj.value("DATA").toArray();
            std::vector<uint32_t> data(arr.size());
            for (int i = 0; i < arr.size(); ++i) data[i] = uint32_t(arr[i].toInt());
            std::vector<uint8_t> out(12 + data.size() * 4);
            std::memcpy(out.data(), &numFrames, 4);
            std::memcpy(out.data() + 4, &pivot, 2);
            out[6] = vectorLen;
            out[7] = flags;
            std::memcpy(out.data() + 8, &scale, 4);
            if (!data.empty()) std::memcpy(out.data() + 12, data.data(), data.size() * 4);
            item.data = std::move(out);
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x02C1 (W3D_CHUNK_MORPHANIM_HEADER)
    struct MorphAnimHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dMorphAnimHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dMorphAnimHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["HIERARCHYNAME"] = QString::fromUtf8(h->HierarchyName, strnlen(h->HierarchyName, W3D_NAME_LEN));
                obj["FRAMECOUNT"] = int(h->FrameCount);
                obj["FRAMERATE"] = h->FrameRate;
                obj["CHANNELCOUNT"] = int(h->ChannelCount);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dMorphAnimHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            QByteArray hname = dataObj.value("HIERARCHYNAME").toString().toUtf8();
            std::memset(h.HierarchyName, 0, W3D_NAME_LEN);
            std::memcpy(h.HierarchyName, hname.constData(), std::min<int>(hname.size(), W3D_NAME_LEN));
            h.FrameCount = dataObj.value("FRAMECOUNT").toInt();
            h.FrameRate = float(dataObj.value("FRAMERATE").toDouble());
            h.ChannelCount = dataObj.value("CHANNELCOUNT").toInt();
            item.length = sizeof(W3dMorphAnimHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x02C3 (W3D_CHUNK_MORPHANIM_POSENAME)
    struct MorphAnimPoseNameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QByteArray arr(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["POSENAME"] = QString::fromUtf8(arr.constData());
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray name = dataObj.value("POSENAME").toString().toUtf8();
            item.data.resize(name.size() + 1);
            std::memcpy(item.data.data(), name.constData(), name.size());
            item.data[name.size()] = 0;
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x02C4 (W3D_CHUNK_MORPHANIM_KEYDATA)
    struct MorphAnimKeyDataSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["KEYS"] = structsToJsonArray<W3dMorphAnimKeyStruct>(
                item.data,
                [](const W3dMorphAnimKeyStruct& k) {
                    ordered_json o;
                    o["MORPHFRAME"] = int(k.MorphFrame);
                    o["POSEFRAME"] = int(k.PoseFrame);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("KEYS").toArray();
            item.data = jsonArrayToStructs<W3dMorphAnimKeyStruct>(arr, [](const QJsonValue& val) {
                W3dMorphAnimKeyStruct k{};
                ordered_json o = val.toObject();
                k.MorphFrame = o.value("MORPHFRAME").toInt();
                k.PoseFrame = o.value("POSEFRAME").toInt();
                return k;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x02C5 (W3D_CHUNK_MORPHANIM_PIVOTCHANNELDATA)
    struct MorphAnimPivotChannelDataSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["PIVOTCHANNELDATA"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOTCHANNELDATA").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
            item.length = uint32_t(item.data.size());
        }
    };
    struct HModelHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHModelHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dHModelHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["HIERARCHYNAME"] = QString::fromUtf8(h->HierarchyName, strnlen(h->HierarchyName, W3D_NAME_LEN));
                obj["NUMCONNECTIONS"] = int(h->NumConnections);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHModelHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            QByteArray hname = dataObj.value("HIERARCHYNAME").toString().toUtf8();
            std::memset(h.HierarchyName, 0, W3D_NAME_LEN);
            std::memcpy(h.HierarchyName, hname.constData(), std::min<int>(hname.size(), W3D_NAME_LEN));
            h.NumConnections = uint16_t(dataObj.value("NUMCONNECTIONS").toInt());
            item.length = sizeof(W3dHModelHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct NodeSerializer : ChunkSerializer {
        const char* fieldName;
        NodeSerializer(const char* f) : fieldName(f) {}
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHModelNodeStruct)) {
                const auto* n = reinterpret_cast<const W3dHModelNodeStruct*>(item.data.data());
                obj[fieldName] = QString::fromUtf8(n->RenderObjName, strnlen(n->RenderObjName, W3D_NAME_LEN));
                obj["PIVOTIDX"] = int(n->PivotIdx);
            }
            return obj;
        }
        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHModelNodeStruct n{};
            QByteArray name = dataObj.value(fieldName).toString().toUtf8();
            std::memset(n.RenderObjName, 0, W3D_NAME_LEN);
            std::memcpy(n.RenderObjName, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            n.PivotIdx = uint16_t(dataObj.value("PIVOTIDX").toInt());
            item.length = sizeof(W3dHModelNodeStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &n, sizeof(n));
        }
    };

    struct HModelAuxDataSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHModelAuxDataStruct)) {
                const auto* h = reinterpret_cast<const W3dHModelAuxDataStruct*>(item.data.data());
                obj["ATTRIBUTES"] = int(h->Attributes);
                obj["MESHCOUNT"] = int(h->MeshCount);
                obj["COLLISIONCOUNT"] = int(h->CollisionCount);
                obj["SKINCOUNT"] = int(h->SkinCount);
                obj["SHADOWCOUNT"] = int(h->ShadowCount);
                obj["NULLCOUNT"] = int(h->NullCount);
                QJsonArray fc;
                for (int i = 0; i < 6; ++i) fc.append(int(h->FutureCounts[i]));
                obj["FUTURECOUNTS"] = fc;
                obj["LODMIN"] = h->LODMin;
                obj["LODMAX"] = h->LODMax;
                QJsonArray fu;
                for (int i = 0; i < 32; ++i) fu.append(int(h->FutureUse[i]));
                obj["FUTUREUSE"] = fu;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHModelAuxDataStruct h{};
            h.Attributes = dataObj.value("ATTRIBUTES").toInt();
            h.MeshCount = dataObj.value("MESHCOUNT").toInt();
            h.CollisionCount = dataObj.value("COLLISIONCOUNT").toInt();
            h.SkinCount = dataObj.value("SKINCOUNT").toInt();
            h.ShadowCount = dataObj.value("SHADOWCOUNT").toInt();
            h.NullCount = dataObj.value("NULLCOUNT").toInt();
            QJsonArray fc = dataObj.value("FUTURECOUNTS").toArray();
            for (int i = 0; i < 6 && i < fc.size(); ++i) h.FutureCounts[i] = fc[i].toInt();
            h.LODMin = float(dataObj.value("LODMIN").toDouble());
            h.LODMax = float(dataObj.value("LODMAX").toDouble());
            QJsonArray fu = dataObj.value("FUTUREUSE").toArray();
            for (int i = 0; i < 32 && i < fu.size(); ++i) h.FutureUse[i] = fu[i].toInt();
            item.length = sizeof(W3dHModelAuxDataStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct LodModelHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLODModelHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dLODModelHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["NUMLODS"] = int(h->NumLODs);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLODModelHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            h.NumLODs = uint16_t(dataObj.value("NUMLODS").toInt());
            item.length = sizeof(W3dLODModelHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct LodSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLODStruct)) {
                const auto* h = reinterpret_cast<const W3dLODStruct*>(item.data.data());
                obj["RENDEROBJNAME"] = QString::fromUtf8(h->RenderObjName, strnlen(h->RenderObjName, 2 * W3D_NAME_LEN));
                obj["LODMIN"] = h->LODMin;
                obj["LODMAX"] = h->LODMax;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLODStruct h{};
            QByteArray name = dataObj.value("RENDEROBJNAME").toString().toUtf8();
            std::memset(h.RenderObjName, 0, 2 * W3D_NAME_LEN);
            std::memcpy(h.RenderObjName, name.constData(), std::min<int>(name.size(), 2 * W3D_NAME_LEN));
            h.LODMin = float(dataObj.value("LODMIN").toDouble());
            h.LODMax = float(dataObj.value("LODMAX").toDouble());
            item.length = sizeof(W3dLODStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct CollectionHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dCollectionHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dCollectionHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["RENDEROBJECTCOUNT"] = int(h->RenderObjectCount);
                QJsonArray pad;
                for (int i = 0; i < 2; ++i) pad.append(int(h->pad[i]));
                obj["PADDING"] = pad;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dCollectionHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            h.RenderObjectCount = dataObj.value("RENDEROBJECTCOUNT").toInt();
            QJsonArray padArr = dataObj.value("PADDING").toArray();
            for (int i = 0; i < 2 && i < padArr.size(); ++i) h.pad[i] = padArr[i].toInt();
            item.length = sizeof(W3dCollectionHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct CollectionObjNameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    struct PlaceholderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dPlaceholderStruct)) {
                const auto* h = reinterpret_cast<const W3dPlaceholderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->version));
                QJsonArray tf;
                for (int i = 0; i < 4; ++i) {
                    QJsonArray row;
                    for (int j = 0; j < 3; ++j) row.append(h->transform[i][j]);
                    tf.append(row);
                }
                obj["TRANSFORM"] = tf;
                size_t headerBytes = sizeof(W3dPlaceholderStruct);
                size_t available = item.data.size() > headerBytes ? item.data.size() - headerBytes : 0;
                uint32_t nameLen = h->name_len;
                if (nameLen > available) nameLen = uint32_t(available);
                obj["NAME"] = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data() + headerBytes), int(nameLen));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dPlaceholderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QJsonArray tf = dataObj.value("TRANSFORM").toArray();
            for (int i = 0; i < 4 && i < tf.size(); ++i) {
                QJsonArray row = tf[i].toArray();
                for (int j = 0; j < 3 && j < row.size(); ++j) h.transform[i][j] = float(row[j].toDouble());
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            h.name_len = uint32_t(name.size());
            item.length = sizeof(W3dPlaceholderStruct) + h.name_len;
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
            if (h.name_len > 0) std::memcpy(item.data.data() + sizeof(h), name.constData(), name.size());
        }
    };

    struct TransformNodeSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dTransformNodeStruct)) {
                const auto* h = reinterpret_cast<const W3dTransformNodeStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->version));
                QJsonArray tf;
                for (int i = 0; i < 4; ++i) {
                    QJsonArray row;
                    for (int j = 0; j < 3; ++j) row.append(h->transform[i][j]);
                    tf.append(row);
                }
                obj["TRANSFORM"] = tf;
                size_t headerBytes = sizeof(W3dTransformNodeStruct);
                size_t available = item.data.size() > headerBytes ? item.data.size() - headerBytes : 0;
                uint32_t nameLen = h->name_len;
                if (nameLen > available) nameLen = uint32_t(available);
                obj["NAME"] = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data() + headerBytes), int(nameLen));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dTransformNodeStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QJsonArray tf = dataObj.value("TRANSFORM").toArray();
            for (int i = 0; i < 4 && i < tf.size(); ++i) {
                QJsonArray row = tf[i].toArray();
                for (int j = 0; j < 3 && j < row.size(); ++j) h.transform[i][j] = float(row[j].toDouble());
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            h.name_len = uint32_t(name.size());
            item.length = sizeof(W3dTransformNodeStruct) + h.name_len;
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
            if (h.name_len > 0) std::memcpy(item.data.data() + sizeof(h), name.constData(), name.size());
        }
    };
    struct PointsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["POINTS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("POINTS").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = float(a[0].toDouble()); v.Y = float(a[1].toDouble()); v.Z = float(a[2].toDouble()); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    struct LightInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLightStruct)) {
                const auto* L = reinterpret_cast<const W3dLightStruct*>(item.data.data());
                obj["ATTRIBUTES"] = int(L->Attributes);
                obj["UNUSED"] = int(L->Unused);
                obj["AMBIENT"] = QJsonArray{ int(L->Ambient.R), int(L->Ambient.G), int(L->Ambient.B) };
                obj["DIFFUSE"] = QJsonArray{ int(L->Diffuse.R), int(L->Diffuse.G), int(L->Diffuse.B) };
                obj["SPECULAR"] = QJsonArray{ int(L->Specular.R), int(L->Specular.G), int(L->Specular.B) };
                obj["INTENSITY"] = L->Intensity;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLightStruct L{};
            L.Attributes = dataObj.value("ATTRIBUTES").toInt();
            L.Unused = dataObj.value("UNUSED").toInt();
            auto amb = dataObj.value("AMBIENT").toArray();
            if (amb.size() >= 3) { L.Ambient.R = amb[0].toInt(); L.Ambient.G = amb[1].toInt(); L.Ambient.B = amb[2].toInt(); }
            auto diff = dataObj.value("DIFFUSE").toArray();
            if (diff.size() >= 3) { L.Diffuse.R = diff[0].toInt(); L.Diffuse.G = diff[1].toInt(); L.Diffuse.B = diff[2].toInt(); }
            auto spec = dataObj.value("SPECULAR").toArray();
            if (spec.size() >= 3) { L.Specular.R = spec[0].toInt(); L.Specular.G = spec[1].toInt(); L.Specular.B = spec[2].toInt(); }
            L.Intensity = float(dataObj.value("INTENSITY").toDouble());
            item.length = sizeof(W3dLightStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &L, sizeof(L));
        }
    };

    struct SpotLightInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dSpotLightStruct)) {
                const auto* S = reinterpret_cast<const W3dSpotLightStruct*>(item.data.data());
                obj["SPOT_DIRECTION"] = QJsonArray{ S->SpotDirection.X, S->SpotDirection.Y, S->SpotDirection.Z };
                obj["SPOT_ANGLE"] = S->SpotAngle;
                obj["SPOT_EXPONENT"] = S->SpotExponent;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dSpotLightStruct S{};
            auto dir = dataObj.value("SPOT_DIRECTION").toArray();
            if (dir.size() >= 3) { S.SpotDirection.X = float(dir[0].toDouble()); S.SpotDirection.Y = float(dir[1].toDouble()); S.SpotDirection.Z = float(dir[2].toDouble()); }
            S.SpotAngle = float(dataObj.value("SPOT_ANGLE").toDouble());
            S.SpotExponent = float(dataObj.value("SPOT_EXPONENT").toDouble());
            item.length = sizeof(W3dSpotLightStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &S, sizeof(S));
        }
    };

    struct LightAttenuationSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLightAttenuationStruct)) {
                const auto* A = reinterpret_cast<const W3dLightAttenuationStruct*>(item.data.data());
                obj["START"] = A->Start;
                obj["END"] = A->End;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLightAttenuationStruct A{};
            A.Start = float(dataObj.value("START").toDouble());
            A.End = float(dataObj.value("END").toDouble());
            item.length = sizeof(W3dLightAttenuationStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &A, sizeof(A));
        }
    };

    struct SpotLightInfo50Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dSpotLightTTStruct)) {
                const auto* S = reinterpret_cast<const W3dSpotLightTTStruct*>(item.data.data());
                obj["SPOT_OUTER_ANGLE"] = S->SpotOuterAngle;
                obj["SPOT_INNER_ANGLE"] = S->SpotInnerAngle;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dSpotLightTTStruct S{};
            S.SpotOuterAngle = float(dataObj.value("SPOT_OUTER_ANGLE").toDouble());
            S.SpotInnerAngle = float(dataObj.value("SPOT_INNER_ANGLE").toDouble());
            item.length = sizeof(W3dSpotLightTTStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &S, sizeof(S));
        }
    };

    struct LightPulseSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLightPulseTTStruct)) {
                const auto* P = reinterpret_cast<const W3dLightPulseTTStruct*>(item.data.data());
                obj["MIN_INTENSITY"] = P->MinIntensity;
                obj["MAX_INTENSITY"] = P->MaxIntensity;
                obj["INTENSITY_TIME"] = P->IntensityTime;
                obj["INTENSITY_TIME_RANDOM"] = P->IntensityTimeRandom;
                obj["INTENSITY_ADJUST"] = P->IntensityAdjust;
                obj["INTENSITY_STOPS_AT_MAX"] = int(P->IntensityStopsAtMax);
                obj["INTENSITY_STOPS_AT_MIN"] = int(P->IntensityStopsAtMin);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLightPulseTTStruct P{};
            P.MinIntensity = float(dataObj.value("MIN_INTENSITY").toDouble());
            P.MaxIntensity = float(dataObj.value("MAX_INTENSITY").toDouble());
            P.IntensityTime = float(dataObj.value("INTENSITY_TIME").toDouble());
            P.IntensityTimeRandom = float(dataObj.value("INTENSITY_TIME_RANDOM").toDouble());
            P.IntensityAdjust = float(dataObj.value("INTENSITY_ADJUST").toDouble());
            P.IntensityStopsAtMax = char(dataObj.value("INTENSITY_STOPS_AT_MAX").toInt());
            P.IntensityStopsAtMin = char(dataObj.value("INTENSITY_STOPS_AT_MIN").toInt());
            item.length = sizeof(W3dLightPulseTTStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &P, sizeof(P));
        }
    };

    // --- Emitter chunk serializers ---

    struct EmitterHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dEmitterHeaderStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterHeaderStruct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, W3D_NAME_LEN);
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            item.length = sizeof(W3dEmitterHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct EmitterUserDataSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            obj["USER_DATA"] = QString::fromUtf8(
                reinterpret_cast<const char*>(item.data.data()),
                item.data.size());
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray arr = dataObj.value("USER_DATA").toString().toUtf8();
            item.length = uint32_t(arr.size() + 1);
            item.data.resize(item.length);
            if (!arr.isEmpty()) {
                std::memcpy(item.data.data(), arr.constData(), arr.size());
            }
            item.data[item.length - 1] = 0;
        }
    };

    struct EmitterInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterInfoStruct)) {
                const auto* info = reinterpret_cast<const W3dEmitterInfoStruct*>(item.data.data());
                obj["TEXTURE_FILENAME"] = QString::fromUtf8(info->TextureFilename, strnlen(info->TextureFilename, 260));
                obj["START_SIZE"] = info->StartSize;
                obj["END_SIZE"] = info->EndSize;
                obj["LIFETIME"] = info->Lifetime;
                obj["EMISSION_RATE"] = info->EmissionRate;
                obj["MAX_EMISSIONS"] = info->MaxEmissions;
                obj["VELOCITY_RANDOM"] = info->VelocityRandom;
                obj["POSITION_RANDOM"] = info->PositionRandom;
                obj["FADE_TIME"] = info->FadeTime;
                obj["GRAVITY"] = info->Gravity;
                obj["ELASTICITY"] = info->Elasticity;
                obj["VELOCITY"] = QJsonArray{ info->Velocity.X, info->Velocity.Y, info->Velocity.Z };
                obj["ACCELERATION"] = QJsonArray{ info->Acceleration.X, info->Acceleration.Y, info->Acceleration.Z };
                obj["START_COLOR"] = QJsonArray{ info->StartColor.R, info->StartColor.G, info->StartColor.B, info->StartColor.A };
                obj["END_COLOR"] = QJsonArray{ info->EndColor.R, info->EndColor.G, info->EndColor.B, info->EndColor.A };
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterInfoStruct info{};
            QByteArray tex = dataObj.value("TEXTURE_FILENAME").toString().toUtf8();
            std::memset(info.TextureFilename, 0, sizeof(info.TextureFilename));
            std::memcpy(info.TextureFilename, tex.constData(), std::min<int>(tex.size(), int(sizeof(info.TextureFilename))));
            info.StartSize = float(dataObj.value("START_SIZE").toDouble());
            info.EndSize = float(dataObj.value("END_SIZE").toDouble());
            info.Lifetime = float(dataObj.value("LIFETIME").toDouble());
            info.EmissionRate = float(dataObj.value("EMISSION_RATE").toDouble());
            info.MaxEmissions = float(dataObj.value("MAX_EMISSIONS").toDouble());
            info.VelocityRandom = float(dataObj.value("VELOCITY_RANDOM").toDouble());
            info.PositionRandom = float(dataObj.value("POSITION_RANDOM").toDouble());
            info.FadeTime = float(dataObj.value("FADE_TIME").toDouble());
            info.Gravity = float(dataObj.value("GRAVITY").toDouble());
            info.Elasticity = float(dataObj.value("ELASTICITY").toDouble());
            auto vel = dataObj.value("VELOCITY").toArray();
            if (vel.size() >= 3) { info.Velocity.X = float(vel[0].toDouble()); info.Velocity.Y = float(vel[1].toDouble()); info.Velocity.Z = float(vel[2].toDouble()); }
            auto acc = dataObj.value("ACCELERATION").toArray();
            if (acc.size() >= 3) { info.Acceleration.X = float(acc[0].toDouble()); info.Acceleration.Y = float(acc[1].toDouble()); info.Acceleration.Z = float(acc[2].toDouble()); }
            auto sc = dataObj.value("START_COLOR").toArray();
            if (sc.size() >= 4) { info.StartColor.R = sc[0].toInt(); info.StartColor.G = sc[1].toInt(); info.StartColor.B = sc[2].toInt(); info.StartColor.A = sc[3].toInt(); }
            auto ec = dataObj.value("END_COLOR").toArray();
            if (ec.size() >= 4) { info.EndColor.R = ec[0].toInt(); info.EndColor.G = ec[1].toInt(); info.EndColor.B = ec[2].toInt(); info.EndColor.A = ec[3].toInt(); }
            item.length = sizeof(W3dEmitterInfoStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &info, sizeof(info));
        }
    };

    struct EmitterInfoV2Serializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterInfoStructV2)) {
                const auto* info = reinterpret_cast<const W3dEmitterInfoStructV2*>(item.data.data());
                obj["BURST_SIZE"] = int(info->BurstSize);

                auto volToJson = [](const W3dVolumeRandomizerStruct& v) {
                    return ordered_json{
                        {"CLASS_ID", int(v.ClassID)},
                        {"VALUE1", v.Value1},
                        {"VALUE2", v.Value2},
                        {"VALUE3", v.Value3}
                    };
                    };

                obj["CREATION_VOLUME"] = volToJson(info->CreationVolume);
                obj["VEL_RANDOM"] = volToJson(info->VelRandom);
                obj["OUTWARD_VEL"] = info->OutwardVel;
                obj["VEL_INHERIT"] = info->VelInherit;

                ordered_json shader;
                shader["DEPTH_COMPARE"] = info->Shader.DepthCompare;
                shader["DEPTH_MASK"] = info->Shader.DepthMask;
                shader["DEST_BLEND"] = info->Shader.DestBlend;
                shader["PRI_GRADIENT"] = info->Shader.PriGradient;
                shader["SEC_GRADIENT"] = info->Shader.SecGradient;
                shader["SRC_BLEND"] = info->Shader.SrcBlend;
                shader["TEXTURING"] = info->Shader.Texturing;
                shader["DETAIL_COLOR_FUNC"] = info->Shader.DetailColorFunc;
                shader["DETAIL_ALPHA_FUNC"] = info->Shader.DetailAlphaFunc;
                shader["ALPHA_TEST"] = info->Shader.AlphaTest;
                obj["SHADER"] = shader;

                obj["RENDER_MODE"] = int(info->RenderMode);
                obj["FRAME_MODE"] = int(info->FrameMode);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterInfoStructV2 info{};
            info.BurstSize = dataObj.value("BURST_SIZE").toInt();

            auto parseVol = [](const ordered_json& o) {
                W3dVolumeRandomizerStruct v{};
                v.ClassID = o.value("CLASS_ID").toInt();
                v.Value1 = float(o.value("VALUE1").toDouble());
                v.Value2 = float(o.value("VALUE2").toDouble());
                v.Value3 = float(o.value("VALUE3").toDouble());
                return v;
                };

            info.CreationVolume = parseVol(dataObj.value("CREATION_VOLUME").toObject());
            info.VelRandom = parseVol(dataObj.value("VEL_RANDOM").toObject());
            info.OutwardVel = float(dataObj.value("OUTWARD_VEL").toDouble());
            info.VelInherit = float(dataObj.value("VEL_INHERIT").toDouble());
            ordered_json shader = dataObj.value("SHADER").toObject();
            info.Shader.DepthCompare = shader.value("DEPTH_COMPARE").toInt();
            info.Shader.DepthMask = shader.value("DEPTH_MASK").toInt();
            info.Shader.DestBlend = shader.value("DEST_BLEND").toInt();
            info.Shader.PriGradient = shader.value("PRI_GRADIENT").toInt();
            info.Shader.SecGradient = shader.value("SEC_GRADIENT").toInt();
            info.Shader.SrcBlend = shader.value("SRC_BLEND").toInt();
            info.Shader.Texturing = shader.value("TEXTURING").toInt();
            info.Shader.DetailColorFunc = shader.value("DETAIL_COLOR_FUNC").toInt();
            info.Shader.DetailAlphaFunc = shader.value("DETAIL_ALPHA_FUNC").toInt();
            info.Shader.AlphaTest = shader.value("ALPHA_TEST").toInt();
            info.RenderMode = dataObj.value("RENDER_MODE").toInt();
            info.FrameMode = dataObj.value("FRAME_MODE").toInt();
            item.length = sizeof(W3dEmitterInfoStructV2);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &info, sizeof(info));
        }
    };

    struct EmitterPropsSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterPropertyStruct)) {
                const auto* h = reinterpret_cast<const W3dEmitterPropertyStruct*>(item.data.data());
                obj["COLOR_KEYFRAMES"] = int(h->ColorKeyframes);
                obj["OPACITY_KEYFRAMES"] = int(h->OpacityKeyframes);
                obj["SIZE_KEYFRAMES"] = int(h->SizeKeyframes);
                obj["COLOR_RANDOM"] = QJsonArray{ h->ColorRandom.R, h->ColorRandom.G, h->ColorRandom.B, h->ColorRandom.A };
                obj["OPACITY_RANDOM"] = h->OpacityRandom;
                obj["SIZE_RANDOM"] = h->SizeRandom;

                const uint8_t* ptr = item.data.data() + sizeof(W3dEmitterPropertyStruct);
                size_t off = 0;
                QJsonArray cArr;
                for (uint32_t i = 0; i < h->ColorKeyframes; ++i) {
                    float t; W3dRGBAStruct c{};
                    std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
                    std::memcpy(&c, ptr + off, sizeof(W3dRGBAStruct)); off += sizeof(W3dRGBAStruct);
                    cArr.append(ordered_json{ {"TIME", t}, {"COLOR", QJsonArray{ c.R, c.G, c.B, c.A }} });
                }
                obj["COLOR_KEYS"] = cArr;

                QJsonArray oArr;
                for (uint32_t i = 0; i < h->OpacityKeyframes; ++i) {
                    float t, v;
                    std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
                    std::memcpy(&v, ptr + off, sizeof(float)); off += sizeof(float);
                    oArr.append(ordered_json{ {"TIME", t}, {"OPACITY", v} });
                }
                obj["OPACITY_KEYS"] = oArr;

                QJsonArray sArr;
                for (uint32_t i = 0; i < h->SizeKeyframes; ++i) {
                    float t, v;
                    std::memcpy(&t, ptr + off, sizeof(float)); off += sizeof(float);
                    std::memcpy(&v, ptr + off, sizeof(float)); off += sizeof(float);
                    sArr.append(ordered_json{ {"TIME", t}, {"SIZE", v} });
                }
                obj["SIZE_KEYS"] = sArr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterPropertyStruct h{};
            auto cArr = dataObj.value("COLOR_KEYS").toArray();
            auto oArr = dataObj.value("OPACITY_KEYS").toArray();
            auto sArr = dataObj.value("SIZE_KEYS").toArray();
            h.ColorKeyframes = cArr.size();
            h.OpacityKeyframes = oArr.size();
            h.SizeKeyframes = sArr.size();
            auto cr = dataObj.value("COLOR_RANDOM").toArray();
            if (cr.size() >= 4) { h.ColorRandom.R = cr[0].toInt(); h.ColorRandom.G = cr[1].toInt(); h.ColorRandom.B = cr[2].toInt(); h.ColorRandom.A = cr[3].toInt(); }
            h.OpacityRandom = float(dataObj.value("OPACITY_RANDOM").toDouble());
            h.SizeRandom = float(dataObj.value("SIZE_RANDOM").toDouble());

            size_t total = sizeof(W3dEmitterPropertyStruct) +
                size_t(h.ColorKeyframes) * (sizeof(float) + sizeof(W3dRGBAStruct)) +
                size_t(h.OpacityKeyframes) * (sizeof(float) * 2) +
                size_t(h.SizeKeyframes) * (sizeof(float) * 2);
            item.data.resize(total);
            item.length = uint32_t(total);
            std::memcpy(item.data.data(), &h, sizeof(h));
            size_t off = sizeof(W3dEmitterPropertyStruct);

            for (int i = 0; i < cArr.size(); ++i) {
                ordered_json o = cArr[i].toObject();
                float t = float(o.value("TIME").toDouble());
                auto col = o.value("COLOR").toArray();
                W3dRGBAStruct c{};
                if (col.size() >= 4) { c.R = col[0].toInt(); c.G = col[1].toInt(); c.B = col[2].toInt(); c.A = col[3].toInt(); }
                std::memcpy(item.data.data() + off, &t, sizeof(float)); off += sizeof(float);
                std::memcpy(item.data.data() + off, &c, sizeof(W3dRGBAStruct)); off += sizeof(W3dRGBAStruct);
            }

            for (int i = 0; i < oArr.size(); ++i) {
                ordered_json o = oArr[i].toObject();
                float t = float(o.value("TIME").toDouble());
                float v = float(o.value("OPACITY").toDouble());
                std::memcpy(item.data.data() + off, &t, sizeof(float)); off += sizeof(float);
                std::memcpy(item.data.data() + off, &v, sizeof(float)); off += sizeof(float);
            }

            for (int i = 0; i < sArr.size(); ++i) {
                ordered_json o = sArr[i].toObject();
                float t = float(o.value("TIME").toDouble());
                float v = float(o.value("SIZE").toDouble());
                std::memcpy(item.data.data() + off, &t, sizeof(float)); off += sizeof(float);
                std::memcpy(item.data.data() + off, &v, sizeof(float)); off += sizeof(float);
            }
        }
    };

    struct EmitterColorKeyframeSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            size_t count = item.data.size() / sizeof(W3dEmitterColorKeyframeStruct);
            const auto* begin = reinterpret_cast<const W3dEmitterColorKeyframeStruct*>(item.data.data());
            for (size_t i = 0; i < count; ++i) {
                const auto& k = begin[i];
                arr.append(ordered_json{ {"TIME", k.Time}, {"COLOR", QJsonArray{ k.Color.R, k.Color.G, k.Color.B, k.Color.A }} });
            }
            obj["KEYFRAMES"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("KEYFRAMES").toArray();
            std::vector<W3dEmitterColorKeyframeStruct> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                temp[i].Time = float(o.value("TIME").toDouble());
                auto c = o.value("COLOR").toArray();
                if (c.size() >= 4) { temp[i].Color.R = c[0].toInt(); temp[i].Color.G = c[1].toInt(); temp[i].Color.B = c[2].toInt(); temp[i].Color.A = c[3].toInt(); }
            }
            item.length = uint32_t(temp.size() * sizeof(W3dEmitterColorKeyframeStruct));
            item.data.resize(item.length);
            if (!temp.empty()) {
                std::memcpy(item.data.data(), temp.data(), item.length);
            }
        }
    };

    struct EmitterOpacityKeyframeSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            size_t count = item.data.size() / sizeof(W3dEmitterOpacityKeyframeStruct);
            const auto* begin = reinterpret_cast<const W3dEmitterOpacityKeyframeStruct*>(item.data.data());
            for (size_t i = 0; i < count; ++i) {
                arr.append(ordered_json{ {"TIME", begin[i].Time}, {"OPACITY", begin[i].Opacity} });
            }
            obj["KEYFRAMES"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("KEYFRAMES").toArray();
            std::vector<W3dEmitterOpacityKeyframeStruct> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                temp[i].Time = float(o.value("TIME").toDouble());
                temp[i].Opacity = float(o.value("OPACITY").toDouble());
            }
            item.length = uint32_t(temp.size() * sizeof(W3dEmitterOpacityKeyframeStruct));
            item.data.resize(item.length);
            if (!temp.empty()) {
                std::memcpy(item.data.data(), temp.data(), item.length);
            }
        }
    };

    struct EmitterSizeKeyframeSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QJsonArray arr;
            size_t count = item.data.size() / sizeof(W3dEmitterSizeKeyframeStruct);
            const auto* begin = reinterpret_cast<const W3dEmitterSizeKeyframeStruct*>(item.data.data());
            for (size_t i = 0; i < count; ++i) {
                arr.append(ordered_json{ {"TIME", begin[i].Time}, {"SIZE", begin[i].Size} });
            }
            obj["KEYFRAMES"] = arr;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("KEYFRAMES").toArray();
            std::vector<W3dEmitterSizeKeyframeStruct> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                temp[i].Time = float(o.value("TIME").toDouble());
                temp[i].Size = float(o.value("SIZE").toDouble());
            }
            item.length = uint32_t(temp.size() * sizeof(W3dEmitterSizeKeyframeStruct));
            item.data.resize(item.length);
            if (!temp.empty()) {
                std::memcpy(item.data.data(), temp.data(), item.length);
            }
        }
    };

    struct EmitterLinePropertiesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterLinePropertiesStruct)) {
                const auto* p = reinterpret_cast<const W3dEmitterLinePropertiesStruct*>(item.data.data());
                obj["FLAGS"] = int(p->Flags);
                obj["SUBDIVISION_LEVEL"] = int(p->SubdivisionLevel);
                obj["NOISE_AMPLITUDE"] = p->NoiseAmplitude;
                obj["MERGE_ABORT_FACTOR"] = p->MergeAbortFactor;
                obj["TEXTURE_TILE_FACTOR"] = p->TextureTileFactor;
                obj["U_PER_SEC"] = p->UPerSec;
                obj["V_PER_SEC"] = p->VPerSec;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterLinePropertiesStruct p{};
            p.Flags = dataObj.value("FLAGS").toInt();
            p.SubdivisionLevel = dataObj.value("SUBDIVISION_LEVEL").toInt();
            p.NoiseAmplitude = float(dataObj.value("NOISE_AMPLITUDE").toDouble());
            p.MergeAbortFactor = float(dataObj.value("MERGE_ABORT_FACTOR").toDouble());
            p.TextureTileFactor = float(dataObj.value("TEXTURE_TILE_FACTOR").toDouble());
            p.UPerSec = float(dataObj.value("U_PER_SEC").toDouble());
            p.VPerSec = float(dataObj.value("V_PER_SEC").toDouble());
            item.length = sizeof(W3dEmitterLinePropertiesStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &p, sizeof(p));
        }
    };

    struct EmitterRotationKeyframesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterRotationHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dEmitterRotationHeaderStruct*>(item.data.data());
                obj["KEYFRAME_COUNT"] = int(h->KeyframeCount);
                obj["RANDOM"] = h->Random;
                obj["ORIENTATION_RANDOM"] = h->OrientationRandom;
                const float* f = reinterpret_cast<const float*>(item.data.data() + sizeof(W3dEmitterRotationHeaderStruct));
                size_t pairs = (item.data.size() - sizeof(W3dEmitterRotationHeaderStruct)) / (2 * sizeof(float));
                QJsonArray arr;
                for (size_t i = 0; i < pairs; ++i) {
                    arr.append(ordered_json{ {"TIME", f[i * 2]}, {"ROTATION", f[i * 2 + 1]} });
                }
                obj["KEYS"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterRotationHeaderStruct h{};
            h.Random = float(dataObj.value("RANDOM").toDouble());
            h.OrientationRandom = float(dataObj.value("ORIENTATION_RANDOM").toDouble());
            QJsonArray arr = dataObj.value("KEYS").toArray();
            h.KeyframeCount = arr.size() > 0 ? arr.size() - 1 : 0;
            size_t total = sizeof(W3dEmitterRotationHeaderStruct) + arr.size() * 2 * sizeof(float);
            item.data.resize(total);
            item.length = uint32_t(total);
            std::memcpy(item.data.data(), &h, sizeof(h));
            float* f = reinterpret_cast<float*>(item.data.data() + sizeof(W3dEmitterRotationHeaderStruct));
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                f[i * 2] = float(o.value("TIME").toDouble());
                f[i * 2 + 1] = float(o.value("ROTATION").toDouble());
            }
        }
    };

    struct EmitterFrameKeyframesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterFrameHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dEmitterFrameHeaderStruct*>(item.data.data());
                obj["KEYFRAME_COUNT"] = int(h->KeyframeCount);
                obj["RANDOM"] = h->Random;
                const auto* keys = reinterpret_cast<const W3dEmitterFrameKeyframeStruct*>(item.data.data() + sizeof(W3dEmitterFrameHeaderStruct));
                size_t count = (item.data.size() - sizeof(W3dEmitterFrameHeaderStruct)) / sizeof(W3dEmitterFrameKeyframeStruct);
                QJsonArray arr;
                for (size_t i = 0; i < count; ++i) {
                    arr.append(ordered_json{ {"TIME", keys[i].Time}, {"FRAME", keys[i].Frame} });
                }
                obj["KEYS"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterFrameHeaderStruct h{};
            h.Random = float(dataObj.value("RANDOM").toDouble());
            QJsonArray arr = dataObj.value("KEYS").toArray();
            h.KeyframeCount = arr.size() > 0 ? arr.size() - 1 : 0;
            size_t total = sizeof(W3dEmitterFrameHeaderStruct) + arr.size() * sizeof(W3dEmitterFrameKeyframeStruct);
            item.data.resize(total);
            item.length = uint32_t(total);
            std::memcpy(item.data.data(), &h, sizeof(h));
            auto* keys = reinterpret_cast<W3dEmitterFrameKeyframeStruct*>(item.data.data() + sizeof(W3dEmitterFrameHeaderStruct));
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                keys[i].Time = float(o.value("TIME").toDouble());
                keys[i].Frame = float(o.value("FRAME").toDouble());
            }
        }
    };

    struct EmitterBlurTimeKeyframesSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterBlurTimeHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dEmitterBlurTimeHeaderStruct*>(item.data.data());
                obj["KEYFRAME_COUNT"] = int(h->KeyframeCount);
                obj["RANDOM"] = h->Random;
                const auto* keys = reinterpret_cast<const W3dEmitterBlurTimeKeyframeStruct*>(item.data.data() + sizeof(W3dEmitterBlurTimeHeaderStruct));
                size_t count = (item.data.size() - sizeof(W3dEmitterBlurTimeHeaderStruct)) / sizeof(W3dEmitterBlurTimeKeyframeStruct);
                QJsonArray arr;
                for (size_t i = 0; i < count; ++i) {
                    arr.append(ordered_json{ {"TIME", keys[i].Time}, {"BLUR_TIME", keys[i].BlurTime} });
                }
                obj["KEYS"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterBlurTimeHeaderStruct h{};
            h.Random = float(dataObj.value("RANDOM").toDouble());
            QJsonArray arr = dataObj.value("KEYS").toArray();
            h.KeyframeCount = arr.size() > 0 ? arr.size() - 1 : 0;
            size_t total = sizeof(W3dEmitterBlurTimeHeaderStruct) + arr.size() * sizeof(W3dEmitterBlurTimeKeyframeStruct);
            item.data.resize(total);
            item.length = uint32_t(total);
            std::memcpy(item.data.data(), &h, sizeof(h));
            auto* keys = reinterpret_cast<W3dEmitterBlurTimeKeyframeStruct*>(item.data.data() + sizeof(W3dEmitterBlurTimeHeaderStruct));
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                keys[i].Time = float(o.value("TIME").toDouble());
                keys[i].BlurTime = float(o.value("BLUR_TIME").toDouble());
            }
        }
    };

    struct EmitterExtraInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dEmitterExtraInfoStruct)) {
                const auto* info = reinterpret_cast<const W3dEmitterExtraInfoStruct*>(item.data.data());
                obj["FUTURE_START_TIME"] = info->FutureStartTime;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dEmitterExtraInfoStruct info{};
            info.FutureStartTime = float(dataObj.value("FUTURE_START_TIME").toDouble());
            item.length = sizeof(W3dEmitterExtraInfoStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &info, sizeof(info));
        }
    };

    struct AggregateHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dAggregateHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dAggregateHeaderStruct*>(item.data.data());
                obj["VERSION"] = int(h->Version);
                obj["NAME"] = QString::fromUtf8(h->Name, int(strnlen(h->Name, W3D_NAME_LEN)));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dAggregateHeaderStruct h{};
            h.Version = dataObj.value("VERSION").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, sizeof(h.Name));
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            item.length = sizeof(W3dAggregateHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct AggregateInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dAggregateInfoStruct)) {
                const auto* hdr = reinterpret_cast<const W3dAggregateInfoStruct*>(item.data.data());
                obj["BASE_MODEL_NAME"] = QString::fromUtf8(hdr->BaseModelName, int(strnlen(hdr->BaseModelName, W3D_NAME_LEN * 2)));
                obj["SUBOBJECT_COUNT"] = int(hdr->SubobjectCount);
                QJsonArray subsArr;
                const size_t entrySize = sizeof(W3dAggregateSubobjectStruct);
                const size_t avail = item.data.size() - sizeof(W3dAggregateInfoStruct);
                size_t n = std::min<size_t>(hdr->SubobjectCount, avail / entrySize);
                const auto* subs = reinterpret_cast<const W3dAggregateSubobjectStruct*>(item.data.data() + sizeof(W3dAggregateInfoStruct));
                for (size_t i = 0; i < n; ++i) {
                    ordered_json so;
                    so["SUBOBJECT_NAME"] = QString::fromUtf8(subs[i].SubobjectName, int(strnlen(subs[i].SubobjectName, W3D_NAME_LEN * 2)));
                    so["BONE_NAME"] = QString::fromUtf8(subs[i].BoneName, int(strnlen(subs[i].BoneName, W3D_NAME_LEN * 2)));
                    subsArr.append(so);
                }
                obj["SUBOBJECTS"] = subsArr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dAggregateInfoStruct hdr{};
            QByteArray base = dataObj.value("BASE_MODEL_NAME").toString().toUtf8();
            std::memset(hdr.BaseModelName, 0, sizeof(hdr.BaseModelName));
            std::memcpy(hdr.BaseModelName, base.constData(), std::min<int>(base.size(), int(sizeof(hdr.BaseModelName))));
            QJsonArray arr = dataObj.value("SUBOBJECTS").toArray();
            hdr.SubobjectCount = arr.size();
            std::vector<W3dAggregateSubobjectStruct> subs(hdr.SubobjectCount);
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json so = arr[i].toObject();
                QByteArray sn = so.value("SUBOBJECT_NAME").toString().toUtf8();
                std::memset(subs[i].SubobjectName, 0, sizeof(subs[i].SubobjectName));
                std::memcpy(subs[i].SubobjectName, sn.constData(), std::min<int>(sn.size(), int(sizeof(subs[i].SubobjectName))));
                QByteArray bn = so.value("BONE_NAME").toString().toUtf8();
                std::memset(subs[i].BoneName, 0, sizeof(subs[i].BoneName));
                std::memcpy(subs[i].BoneName, bn.constData(), std::min<int>(bn.size(), int(sizeof(subs[i].BoneName))));
            }
            item.length = uint32_t(sizeof(W3dAggregateInfoStruct) + subs.size() * sizeof(W3dAggregateSubobjectStruct));
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &hdr, sizeof(hdr));
            if (!subs.empty()) {
                std::memcpy(item.data.data() + sizeof(W3dAggregateInfoStruct), subs.data(), subs.size() * sizeof(W3dAggregateSubobjectStruct));
            }
        }
    };

    struct TextureReplacerInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dTextureReplacerHeaderStruct)) {
                const auto* hdr = reinterpret_cast<const W3dTextureReplacerHeaderStruct*>(item.data.data());
                obj["COUNT"] = int(hdr->ReplacedTexturesCount);
                QJsonArray arr;
                size_t offset = sizeof(W3dTextureReplacerHeaderStruct);
                for (uint32_t i = 0; i < hdr->ReplacedTexturesCount && offset + sizeof(W3dTextureReplacerStruct) <= item.data.size(); ++i) {
                    const auto* r = reinterpret_cast<const W3dTextureReplacerStruct*>(item.data.data() + offset);
                    ordered_json o;
                    QJsonArray meshPath, bonePath;
                    for (int j = 0; j < 15; ++j) meshPath.append(QString::fromUtf8(r->MeshPath[j], int(strnlen(r->MeshPath[j], 32))));
                    for (int j = 0; j < 15; ++j) bonePath.append(QString::fromUtf8(r->BonePath[j], int(strnlen(r->BonePath[j], 32))));
                    o["MESHPATH"] = meshPath;
                    o["BONEPATH"] = bonePath;
                    o["OLD_TEXTURE_NAME"] = QString::fromUtf8(r->OldTextureName, int(strnlen(r->OldTextureName, 260)));
                    o["NEW_TEXTURE_NAME"] = QString::fromUtf8(r->NewTextureName, int(strnlen(r->NewTextureName, 260)));
                    ordered_json tp;
                    tp["ATTRIBUTES"] = int(r->TextureParams.Attributes);
                    tp["ANIMTYPE"] = int(r->TextureParams.AnimType);
                    tp["FRAMECOUNT"] = int(r->TextureParams.FrameCount);
                    tp["FRAMERATE"] = r->TextureParams.FrameRate;
                    o["TEXTURE_PARAMS"] = tp;
                    arr.append(o);
                    offset += sizeof(W3dTextureReplacerStruct);
                }
                obj["REPLACERS"] = arr;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dTextureReplacerHeaderStruct hdr{};
            QJsonArray arr = dataObj.value("REPLACERS").toArray();
            hdr.ReplacedTexturesCount = arr.size();
            std::vector<W3dTextureReplacerStruct> reps(hdr.ReplacedTexturesCount);
            for (int i = 0; i < arr.size(); ++i) {
                ordered_json o = arr[i].toObject();
                QJsonArray mesh = o.value("MESHPATH").toArray();
                for (int j = 0; j < 15 && j < mesh.size(); ++j) {
                    QByteArray s = mesh[j].toString().toUtf8();
                    std::memset(reps[i].MeshPath[j], 0, sizeof(reps[i].MeshPath[j]));
                    std::memcpy(reps[i].MeshPath[j], s.constData(), std::min<int>(s.size(), int(sizeof(reps[i].MeshPath[j]))));
                }
                QJsonArray bone = o.value("BONEPATH").toArray();
                for (int j = 0; j < 15 && j < bone.size(); ++j) {
                    QByteArray s = bone[j].toString().toUtf8();
                    std::memset(reps[i].BonePath[j], 0, sizeof(reps[i].BonePath[j]));
                    std::memcpy(reps[i].BonePath[j], s.constData(), std::min<int>(s.size(), int(sizeof(reps[i].BonePath[j]))));
                }
                QByteArray old = o.value("OLD_TEXTURE_NAME").toString().toUtf8();
                std::memset(reps[i].OldTextureName, 0, sizeof(reps[i].OldTextureName));
                std::memcpy(reps[i].OldTextureName, old.constData(), std::min<int>(old.size(), int(sizeof(reps[i].OldTextureName))));
                QByteArray neu = o.value("NEW_TEXTURE_NAME").toString().toUtf8();
                std::memset(reps[i].NewTextureName, 0, sizeof(reps[i].NewTextureName));
                std::memcpy(reps[i].NewTextureName, neu.constData(), std::min<int>(neu.size(), int(sizeof(reps[i].NewTextureName))));
                ordered_json tp = o.value("TEXTURE_PARAMS").toObject();
                reps[i].TextureParams.Attributes = uint16_t(tp.value("ATTRIBUTES").toInt());
                reps[i].TextureParams.AnimType = uint16_t(tp.value("ANIMTYPE").toInt());
                reps[i].TextureParams.FrameCount = tp.value("FRAMECOUNT").toInt();
                reps[i].TextureParams.FrameRate = float(tp.value("FRAMERATE").toDouble());
            }
            item.length = uint32_t(sizeof(W3dTextureReplacerHeaderStruct) + reps.size() * sizeof(W3dTextureReplacerStruct));
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &hdr, sizeof(hdr));
            if (!reps.empty()) {
                std::memcpy(item.data.data() + sizeof(W3dTextureReplacerHeaderStruct), reps.data(), reps.size() * sizeof(W3dTextureReplacerStruct));
            }
        }
    };

    struct AggregateClassInfoSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dAggregateMiscInfo)) {
                const auto* info = reinterpret_cast<const W3dAggregateMiscInfo*>(item.data.data());
                obj["ORIGINAL_CLASS_ID"] = int(info->OriginalClassID);
                obj["FLAGS"] = int(info->Flags);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dAggregateMiscInfo info{};
            info.OriginalClassID = dataObj.value("ORIGINAL_CLASS_ID").toInt();
            info.Flags = dataObj.value("FLAGS").toInt();
            item.length = sizeof(W3dAggregateMiscInfo);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &info, sizeof(info));
        }
    };

    struct HLodHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHLodHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dHLodHeaderStruct*>(item.data.data());
                obj["VERSION"] = int(h->Version);
                obj["LOD_COUNT"] = int(h->LodCount);
                obj["NAME"] = QString::fromUtf8(h->Name, int(strnlen(h->Name, W3D_NAME_LEN)));
                obj["HIERARCHY_NAME"] = QString::fromUtf8(h->HierarchyName, int(strnlen(h->HierarchyName, W3D_NAME_LEN)));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHLodHeaderStruct h{};
            h.Version = dataObj.value("VERSION").toInt();
            h.LodCount = dataObj.value("LOD_COUNT").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, sizeof(h.Name));
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            QByteArray hier = dataObj.value("HIERARCHY_NAME").toString().toUtf8();
            std::memset(h.HierarchyName, 0, sizeof(h.HierarchyName));
            std::memcpy(h.HierarchyName, hier.constData(), std::min<int>(hier.size(), W3D_NAME_LEN));
            item.length = sizeof(W3dHLodHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct HLodLodArraySerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHLodArrayHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dHLodArrayHeaderStruct*>(item.data.data());
                obj["MODEL_COUNT"] = int(h->ModelCount);
                obj["MAX_SCREEN_SIZE"] = h->MaxScreenSize;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHLodArrayHeaderStruct h{};
            h.ModelCount = dataObj.value("MODEL_COUNT").toInt();
            h.MaxScreenSize = float(dataObj.value("MAX_SCREEN_SIZE").toDouble());
            item.length = sizeof(W3dHLodArrayHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct HLodSubObjectSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dHLodSubObjectStruct)) {
                const auto* h = reinterpret_cast<const W3dHLodSubObjectStruct*>(item.data.data());
                obj["BONE_INDEX"] = int(h->BoneIndex);
                obj["NAME"] = QString::fromUtf8(h->Name, int(strnlen(h->Name, W3D_NAME_LEN * 2)));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dHLodSubObjectStruct h{};
            h.BoneIndex = dataObj.value("BONE_INDEX").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, sizeof(h.Name));
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), int(sizeof(h.Name))));
            item.length = sizeof(W3dHLodSubObjectStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct BoxSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dBoxStruct)) {
                const auto* b = reinterpret_cast<const W3dBoxStruct*>(item.data.data());
                obj["VERSION"] = int(b->Version);
                obj["ATTRIBUTES"] = int(b->Attributes);
                obj["NAME"] = QString::fromUtf8(b->Name, int(strnlen(b->Name, 2 * W3D_NAME_LEN)));
                obj["COLOR"] = QJsonArray{ int(b->Color.R), int(b->Color.G), int(b->Color.B) };
                obj["CENTER"] = QJsonArray{ b->Center.X, b->Center.Y, b->Center.Z };
                obj["EXTENT"] = QJsonArray{ b->Extent.X, b->Extent.Y, b->Extent.Z };
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dBoxStruct b{};
            b.Version = dataObj.value("VERSION").toInt();
            b.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(b.Name, 0, sizeof(b.Name));
            std::memcpy(b.Name, name.constData(), std::min<int>(name.size(), int(sizeof(b.Name))));
            QJsonArray color = dataObj.value("COLOR").toArray();
            if (color.size() >= 3) {
                b.Color.R = uint8_t(color[0].toInt());
                b.Color.G = uint8_t(color[1].toInt());
                b.Color.B = uint8_t(color[2].toInt());
            }
            QJsonArray center = dataObj.value("CENTER").toArray();
            if (center.size() >= 3) { b.Center.X = center[0].toDouble(); b.Center.Y = center[1].toDouble(); b.Center.Z = center[2].toDouble(); }
            QJsonArray extent = dataObj.value("EXTENT").toArray();
            if (extent.size() >= 3) { b.Extent.X = extent[0].toDouble(); b.Extent.Y = extent[1].toDouble(); b.Extent.Z = extent[2].toDouble(); }
            item.length = sizeof(W3dBoxStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &b, sizeof(b));
        }
    };

    struct SphereSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dSphereStruct)) {
                const auto* s = reinterpret_cast<const W3dSphereStruct*>(item.data.data());
                obj["VERSION"] = int(s->Version);
                obj["ATTRIBUTES"] = int(s->Attributes);
                obj["NAME"] = QString::fromUtf8(s->Name, int(strnlen(s->Name, 2 * W3D_NAME_LEN)));
                obj["CENTER"] = QJsonArray{ s->Center.X, s->Center.Y, s->Center.Z };
                obj["EXTENT"] = QJsonArray{ s->Extent.X, s->Extent.Y, s->Extent.Z };
                obj["ANIM_DURATION"] = s->AnimDuration;
                obj["DEFAULT_COLOR"] = QJsonArray{ int(s->DefaultColor.X), int(s->DefaultColor.Y), int(s->DefaultColor.Z) };
                obj["DEFAULT_ALPHA"] = s->DefaultAlpha;
                obj["DEFAULT_SCALE"] = QJsonArray{ s->DefaultScale.X, s->DefaultScale.Y, s->DefaultScale.Z };
                ordered_json dv;
                dv["ANGLE"] = QJsonArray{ s->DefaultVector.angle.x, s->DefaultVector.angle.y, s->DefaultVector.angle.z, s->DefaultVector.angle.w };
                dv["INTENSITY"] = s->DefaultVector.intensity;
                obj["DEFAULT_VECTOR"] = dv;
                obj["TEXTURE_NAME"] = QString::fromUtf8(s->TextureName, int(strnlen(s->TextureName, 2 * W3D_NAME_LEN)));
                QJsonArray sh;
                const uint32_t* d = reinterpret_cast<const uint32_t*>(&s->Shader);
                for (size_t i = 0; i < sizeof(W3dShaderStruct) / 4; ++i) sh.append(int(d[i]));
                obj["SHADER_RAW"] = sh;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dSphereStruct s{};
            s.Version = dataObj.value("VERSION").toInt();
            s.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(s.Name, 0, sizeof(s.Name));
            std::memcpy(s.Name, name.constData(), std::min<int>(name.size(), int(sizeof(s.Name))));
            QJsonArray center = dataObj.value("CENTER").toArray();
            if (center.size() >= 3) { s.Center.X = center[0].toDouble(); s.Center.Y = center[1].toDouble(); s.Center.Z = center[2].toDouble(); }
            QJsonArray extent = dataObj.value("EXTENT").toArray();
            if (extent.size() >= 3) { s.Extent.X = extent[0].toDouble(); s.Extent.Y = extent[1].toDouble(); s.Extent.Z = extent[2].toDouble(); }
            s.AnimDuration = dataObj.value("ANIM_DURATION").toDouble();
            QJsonArray dc = dataObj.value("DEFAULT_COLOR").toArray();
            if (dc.size() >= 3) { s.DefaultColor.X = uint8_t(dc[0].toInt()); s.DefaultColor.Y = uint8_t(dc[1].toInt()); s.DefaultColor.Z = uint8_t(dc[2].toInt()); }
            s.DefaultAlpha = dataObj.value("DEFAULT_ALPHA").toDouble();
            QJsonArray ds = dataObj.value("DEFAULT_SCALE").toArray();
            if (ds.size() >= 3) { s.DefaultScale.X = ds[0].toDouble(); s.DefaultScale.Y = ds[1].toDouble(); s.DefaultScale.Z = ds[2].toDouble(); }
            ordered_json dv = dataObj.value("DEFAULT_VECTOR").toObject();
            QJsonArray ang = dv.value("ANGLE").toArray();
            if (ang.size() >= 4) { s.DefaultVector.angle.x = ang[0].toDouble(); s.DefaultVector.angle.y = ang[1].toDouble(); s.DefaultVector.angle.z = ang[2].toDouble(); s.DefaultVector.angle.w = ang[3].toDouble(); }
            s.DefaultVector.intensity = dv.value("INTENSITY").toDouble();
            QByteArray tex = dataObj.value("TEXTURE_NAME").toString().toUtf8();
            std::memset(s.TextureName, 0, sizeof(s.TextureName));
            std::memcpy(s.TextureName, tex.constData(), std::min<int>(tex.size(), int(sizeof(s.TextureName))));
            QJsonArray sh = dataObj.value("SHADER_RAW").toArray();
            uint32_t* d = reinterpret_cast<uint32_t*>(&s.Shader);
            for (int i = 0; i < sh.size() && i < int(sizeof(W3dShaderStruct) / 4); ++i) d[i] = uint32_t(sh[i].toInt());
            item.length = sizeof(W3dSphereStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &s, sizeof(s));
        }
    };

    struct RingSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dRingStruct)) {
                const auto* r = reinterpret_cast<const W3dRingStruct*>(item.data.data());
                obj["VERSION"] = int(r->Version);
                obj["ATTRIBUTES"] = int(r->Attributes);
                obj["NAME"] = QString::fromUtf8(r->Name, int(strnlen(r->Name, 2 * W3D_NAME_LEN)));
                obj["CENTER"] = QJsonArray{ r->Center.X, r->Center.Y, r->Center.Z };
                obj["EXTENT"] = QJsonArray{ r->Extent.X, r->Extent.Y, r->Extent.Z };
                obj["ANIM_DURATION"] = r->AnimDuration;
                obj["DEFAULT_COLOR"] = QJsonArray{ int(r->DefaultColor.X), int(r->DefaultColor.Y), int(r->DefaultColor.Z) };
                obj["DEFAULT_ALPHA"] = r->DefaultAlpha;
                obj["DEFAULT_INNER_SCALE"] = QJsonArray{ r->DefaultInnerScale.x, r->DefaultInnerScale.y };
                obj["DEFAULT_OUTER_SCALE"] = QJsonArray{ r->DefaultOuterScale.x, r->DefaultOuterScale.y };
                obj["INNER_EXTENT"] = QJsonArray{ r->InnerExtent.x, r->InnerExtent.y };
                obj["OUTER_EXTENT"] = QJsonArray{ r->OuterExtent.x, r->OuterExtent.y };
                obj["TEXTURE_NAME"] = QString::fromUtf8(r->TextureName, int(strnlen(r->TextureName, 2 * W3D_NAME_LEN)));
                QJsonArray sh;
                const uint32_t* d = reinterpret_cast<const uint32_t*>(&r->Shader);
                for (size_t i = 0; i < sizeof(W3dShaderStruct) / 4; ++i) sh.append(int(d[i]));
                obj["SHADER_RAW"] = sh;
                obj["TEXTURE_TILE_COUNT"] = r->TextureTileCount;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dRingStruct r{};
            r.Version = dataObj.value("VERSION").toInt();
            r.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(r.Name, 0, sizeof(r.Name));
            std::memcpy(r.Name, name.constData(), std::min<int>(name.size(), int(sizeof(r.Name))));
            QJsonArray center = dataObj.value("CENTER").toArray();
            if (center.size() >= 3) { r.Center.X = center[0].toDouble(); r.Center.Y = center[1].toDouble(); r.Center.Z = center[2].toDouble(); }
            QJsonArray extent = dataObj.value("EXTENT").toArray();
            if (extent.size() >= 3) { r.Extent.X = extent[0].toDouble(); r.Extent.Y = extent[1].toDouble(); r.Extent.Z = extent[2].toDouble(); }
            r.AnimDuration = dataObj.value("ANIM_DURATION").toDouble();
            QJsonArray dc = dataObj.value("DEFAULT_COLOR").toArray();
            if (dc.size() >= 3) { r.DefaultColor.X = uint8_t(dc[0].toInt()); r.DefaultColor.Y = uint8_t(dc[1].toInt()); r.DefaultColor.Z = uint8_t(dc[2].toInt()); }
            r.DefaultAlpha = dataObj.value("DEFAULT_ALPHA").toDouble();
            QJsonArray dis = dataObj.value("DEFAULT_INNER_SCALE").toArray();
            if (dis.size() >= 2) { r.DefaultInnerScale.x = dis[0].toDouble(); r.DefaultInnerScale.y = dis[1].toDouble(); }
            QJsonArray dos = dataObj.value("DEFAULT_OUTER_SCALE").toArray();
            if (dos.size() >= 2) { r.DefaultOuterScale.x = dos[0].toDouble(); r.DefaultOuterScale.y = dos[1].toDouble(); }
            QJsonArray inext = dataObj.value("INNER_EXTENT").toArray();
            if (inext.size() >= 2) { r.InnerExtent.x = inext[0].toDouble(); r.InnerExtent.y = inext[1].toDouble(); }
            QJsonArray outext = dataObj.value("OUTER_EXTENT").toArray();
            if (outext.size() >= 2) { r.OuterExtent.x = outext[0].toDouble(); r.OuterExtent.y = outext[1].toDouble(); }
            QByteArray tex = dataObj.value("TEXTURE_NAME").toString().toUtf8();
            std::memset(r.TextureName, 0, sizeof(r.TextureName));
            std::memcpy(r.TextureName, tex.constData(), std::min<int>(tex.size(), int(sizeof(r.TextureName))));
            QJsonArray sh = dataObj.value("SHADER_RAW").toArray();
            uint32_t* d = reinterpret_cast<uint32_t*>(&r.Shader);
            for (int i = 0; i < sh.size() && i < int(sizeof(W3dShaderStruct) / 4); ++i) d[i] = uint32_t(sh[i].toInt());
            r.TextureTileCount = dataObj.value("TEXTURE_TILE_COUNT").toInt();
            item.length = sizeof(W3dRingStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &r, sizeof(r));
        }
    };

    struct NullObjectSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dNullObjectStruct)) {
                const auto* n = reinterpret_cast<const W3dNullObjectStruct*>(item.data.data());
                obj["VERSION"] = int(n->Version);
                obj["ATTRIBUTES"] = int(n->Attributes);
                obj["NAME"] = QString::fromUtf8(n->Name, int(strnlen(n->Name, 2 * W3D_NAME_LEN)));
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dNullObjectStruct n{};
            n.Version = dataObj.value("VERSION").toInt();
            n.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(n.Name, 0, sizeof(n.Name));
            std::memcpy(n.Name, name.constData(), std::min<int>(name.size(), int(sizeof(n.Name))));
            item.length = sizeof(W3dNullObjectStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &n, sizeof(n));
        }
    };

    struct LightTransformSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dLightTransformStruct)) {
                const auto* m = reinterpret_cast<const W3dLightTransformStruct*>(item.data.data());
                QJsonArray rows;
                for (int i = 0; i < 3; ++i) {
                    rows.append(QJsonArray{ m->Transform[i][0], m->Transform[i][1], m->Transform[i][2], m->Transform[i][3] });
                }
                obj["MATRIX"] = rows;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dLightTransformStruct m{};
            QJsonArray rows = dataObj.value("MATRIX").toArray();
            for (int i = 0; i < 3 && i < rows.size(); ++i) {
                QJsonArray r = rows[i].toArray();
                for (int j = 0; j < 4 && j < r.size(); ++j) {
                    m.Transform[i][j] = float(r[j].toDouble());
                }
            }
            item.length = sizeof(W3dLightTransformStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &m, sizeof(m));
        }
    };

    struct DazzleNameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    struct DazzleTypenameSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["TYPENAME"] = text;
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("TYPENAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    struct SoundRObjHeaderSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            if (item.data.size() >= sizeof(W3dSoundRObjHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dSoundRObjHeaderStruct*>(item.data.data());
                obj["VERSION"] = int(h->Version);
                obj["NAME"] = QString::fromUtf8(h->Name, int(strnlen(h->Name, W3D_NAME_LEN)));
                obj["FLAGS"] = int(h->Flags);
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            W3dSoundRObjHeaderStruct h{};
            h.Version = dataObj.value("VERSION").toInt();
            QByteArray name = dataObj.value("NAME").toString().toUtf8();
            std::memset(h.Name, 0, sizeof(h.Name));
            std::memcpy(h.Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
            h.Flags = dataObj.value("FLAGS").toInt();
            item.length = sizeof(W3dSoundRObjHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    struct SoundRObjDefinitionSerializer : ChunkSerializer {
        ordered_json toJson(const ChunkItem& item) const override {
            ordered_json obj;
            const uint8_t* cur = item.data.data();
            const uint8_t* end = cur + item.data.size();
            auto need = [&](size_t n) { return size_t(end - cur) >= n; };
            while (need(2)) {
                uint8_t id = cur[0];
                uint8_t size = cur[1];
                cur += 2;
                if (!need(size)) break;
                const uint8_t* pay = cur;
                switch (id) {
                case 0x01: if (size == 4) { uint32_t v; std::memcpy(&v, pay, 4); obj["m_ID"] = int(v); } break;
                case 0x03:
                    if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_Priority"] = f; }
                    else { obj["m_Name"] = QString::fromUtf8(reinterpret_cast<const char*>(pay), int(std::min<size_t>(size, strnlen(reinterpret_cast<const char*>(pay), size)))); }
                    break;
                case 0x04: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_Volume"] = f; } break;
                case 0x05: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_Pan"] = f; } break;
                case 0x06: if (size == 4) { uint32_t v; std::memcpy(&v, pay, 4); obj["m_LoopCount"] = int(v); } break;
                case 0x07: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_DropoffRadius"] = f; } break;
                case 0x08: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_MaxVolRadius"] = f; } break;
                case 0x09: if (size == 4) { uint32_t v; std::memcpy(&v, pay, 4); obj["m_Type"] = int(v); } break;
                case 0x0A: if (size >= 1) { obj["m_is3DSound"] = int(pay[0]); } break;
                case 0x0B: obj["m_Filename"] = QString::fromUtf8(reinterpret_cast<const char*>(pay), int(std::min<size_t>(size, strnlen(reinterpret_cast<const char*>(pay), size)))); break;
                case 0x0C: obj["m_DisplayText"] = QString::fromUtf8(reinterpret_cast<const char*>(pay), int(std::min<size_t>(size, strnlen(reinterpret_cast<const char*>(pay), size)))); break;
                case 0x12: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_StartOffset"] = f; } break;
                case 0x13: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_PitchFactor"] = f; } break;
                case 0x14: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_PitchFactorRandomizer"] = f; } break;
                case 0x15: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_VolumeRandomizer"] = f; } break;
                case 0x16: if (size == 4) { uint32_t v; std::memcpy(&v, pay, 4); obj["m_VirtualChannel"] = int(v); } break;
                case 0x0D: if (size == 4) { uint32_t v; std::memcpy(&v, pay, 4); obj["m_LogicalType"] = int(v); } break;
                case 0x0E: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_LogicalNotifDelay"] = f; } break;
                case 0x0F: if (size >= 1) { obj["m_CreateLogicalSound"] = int(pay[0]); } break;
                case 0x10: if (size == 4) { float f; std::memcpy(&f, pay, 4); obj["m_LogicalDropoffRadius"] = f; } break;
                case 0x11:
                    if (size == 12) {
                        const float* f = reinterpret_cast<const float*>(pay);
                        obj["m_SphereColor"] = QJsonArray{ f[0], f[1], f[2] };
                    }
                    break;
                default: {
                    QJsonArray raw;
                    for (int i = 0; i < size; ++i) raw.append(int(pay[i]));
                    obj[QString("UNKNOWN_%1").arg(id, 2, 16, QChar('0'))] = raw;
                    break;
                }
                }
                cur += size;
            }
            return obj;
        }

        void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
            QByteArray out;
            auto append = [&](uint8_t id, const QByteArray& payload) {
                out.append(char(id));
                out.append(char(payload.size()));
                out.append(payload);
                };

            if (dataObj.contains("m_ID")) {
                uint32_t v = dataObj.value("m_ID").toInt();
                append(0x01, QByteArray(reinterpret_cast<const char*>(&v), 4));
            }
            if (dataObj.contains("m_Name")) {
                QByteArray s = dataObj.value("m_Name").toString().toUtf8();
                s.append('\0');
                append(0x03, s);
            }
            if (dataObj.contains("m_Priority")) {
                float f = float(dataObj.value("m_Priority").toDouble());
                append(0x03, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_Volume")) {
                float f = float(dataObj.value("m_Volume").toDouble());
                append(0x04, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_Pan")) {
                float f = float(dataObj.value("m_Pan").toDouble());
                append(0x05, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_LoopCount")) {
                uint32_t v = dataObj.value("m_LoopCount").toInt();
                append(0x06, QByteArray(reinterpret_cast<const char*>(&v), 4));
            }
            if (dataObj.contains("m_DropoffRadius")) {
                float f = float(dataObj.value("m_DropoffRadius").toDouble());
                append(0x07, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_MaxVolRadius")) {
                float f = float(dataObj.value("m_MaxVolRadius").toDouble());
                append(0x08, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_Type")) {
                uint32_t v = dataObj.value("m_Type").toInt();
                append(0x09, QByteArray(reinterpret_cast<const char*>(&v), 4));
            }
            if (dataObj.contains("m_is3DSound")) {
                QByteArray b(1, '\0');
                b[0] = char(dataObj.value("m_is3DSound").toInt());
                append(0x0A, b);
            }
            if (dataObj.contains("m_Filename")) {
                QByteArray s = dataObj.value("m_Filename").toString().toUtf8();
                s.append('\0');
                append(0x0B, s);
            }
            if (dataObj.contains("m_DisplayText")) {
                QByteArray s = dataObj.value("m_DisplayText").toString().toUtf8();
                s.append('\0');
                append(0x0C, s);
            }
            if (dataObj.contains("m_StartOffset")) {
                float f = float(dataObj.value("m_StartOffset").toDouble());
                append(0x12, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_PitchFactor")) {
                float f = float(dataObj.value("m_PitchFactor").toDouble());
                append(0x13, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_PitchFactorRandomizer")) {
                float f = float(dataObj.value("m_PitchFactorRandomizer").toDouble());
                append(0x14, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_VolumeRandomizer")) {
                float f = float(dataObj.value("m_VolumeRandomizer").toDouble());
                append(0x15, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_VirtualChannel")) {
                uint32_t v = dataObj.value("m_VirtualChannel").toInt();
                append(0x16, QByteArray(reinterpret_cast<const char*>(&v), 4));
            }
            if (dataObj.contains("m_LogicalType")) {
                uint32_t v = dataObj.value("m_LogicalType").toInt();
                append(0x0D, QByteArray(reinterpret_cast<const char*>(&v), 4));
            }
            if (dataObj.contains("m_LogicalNotifDelay")) {
                float f = float(dataObj.value("m_LogicalNotifDelay").toDouble());
                append(0x0E, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_CreateLogicalSound")) {
                QByteArray b(1, '\0');
                b[0] = char(dataObj.value("m_CreateLogicalSound").toInt());
                append(0x0F, b);
            }
            if (dataObj.contains("m_LogicalDropoffRadius")) {
                float f = float(dataObj.value("m_LogicalDropoffRadius").toDouble());
                append(0x10, QByteArray(reinterpret_cast<const char*>(&f), 4));
            }
            if (dataObj.contains("m_SphereColor")) {
                QJsonArray sc = dataObj.value("m_SphereColor").toArray();
                if (sc.size() >= 3) {
                    float f[3];
                    for (int i = 0; i < 3; ++i) f[i] = float(sc[i].toDouble());
                    append(0x11, QByteArray(reinterpret_cast<const char*>(f), 12));
                }
            }

            item.length = out.size();
            item.data.assign(out.begin(), out.end());
}
};


struct ShdMeshNameSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
        obj["NAME"] = text;
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QByteArray text = dataObj.value("NAME").toString().toUtf8();
        item.length = uint32_t(text.size() + 1);
        item.data.resize(item.length);
        std::memcpy(item.data.data(), text.constData(), text.size());
        item.data[text.size()] = 0;
    }
};

struct ShdMeshHeaderSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        if (item.data.size() >= sizeof(W3dShdMeshHeaderStruct)) {
            const auto* h = reinterpret_cast<const W3dShdMeshHeaderStruct*>(item.data.data());
            obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
            obj["MESHFLAGS"] = int(h->MeshFlags);
            obj["NUMTRIANGLES"] = int(h->NumTriangles);
            obj["NUMVERTICES"] = int(h->NumVertices);
            obj["NUMSUBMESHES"] = int(h->NumSubMeshes);
            QJsonArray fc; for (int i = 0; i < 5; ++i) fc.append(int(h->FutureCounts[i]));
            obj["FUTURECOUNTS"] = fc;
            obj["BOXMIN"] = QJsonArray{ h->BoxMin.X, h->BoxMin.Y, h->BoxMin.Z };
            obj["BOXMAX"] = QJsonArray{ h->BoxMax.X, h->BoxMax.Y, h->BoxMax.Z };
            obj["SPHCENTER"] = QJsonArray{ h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
            obj["SPHRADIUS"] = h->SphRadius;
        }
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        W3dShdMeshHeaderStruct h{};
        QString verStr = dataObj.value("VERSION").toString();
        auto parts = verStr.split('.');
        if (parts.size() == 2) h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
        h.MeshFlags = dataObj.value("MESHFLAGS").toInt();
        h.NumTriangles = dataObj.value("NUMTRIANGLES").toInt();
        h.NumVertices = dataObj.value("NUMVERTICES").toInt();
        h.NumSubMeshes = dataObj.value("NUMSUBMESHES").toInt();
        QJsonArray fc = dataObj.value("FUTURECOUNTS").toArray();
        for (int i = 0; i < 5 && i < fc.size(); ++i) h.FutureCounts[i] = fc[i].toInt();
        QJsonArray bmin = dataObj.value("BOXMIN").toArray();
        if (bmin.size() >= 3) { h.BoxMin.X = bmin[0].toDouble(); h.BoxMin.Y = bmin[1].toDouble(); h.BoxMin.Z = bmin[2].toDouble(); }
        QJsonArray bmax = dataObj.value("BOXMAX").toArray();
        if (bmax.size() >= 3) { h.BoxMax.X = bmax[0].toDouble(); h.BoxMax.Y = bmax[1].toDouble(); h.BoxMax.Z = bmax[2].toDouble(); }
        QJsonArray sph = dataObj.value("SPHCENTER").toArray();
        if (sph.size() >= 3) { h.SphCenter.X = sph[0].toDouble(); h.SphCenter.Y = sph[1].toDouble(); h.SphCenter.Z = sph[2].toDouble(); }
        h.SphRadius = dataObj.value("SPHRADIUS").toDouble();
        item.length = sizeof(W3dShdMeshHeaderStruct);
        item.data.resize(item.length);
        std::memcpy(item.data.data(), &h, sizeof(h));
    }
};

struct ShdSubMeshHeaderSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        if (item.data.size() >= sizeof(W3dShdSubMeshHeaderStruct)) {
            const auto* h = reinterpret_cast<const W3dShdSubMeshHeaderStruct*>(item.data.data());
            obj["NUMTRIANGLES"] = int(h->NumTriangles);
            obj["NUMVERTICES"] = int(h->NumVertices);
            QJsonArray fc; for (int i = 0; i < 2; ++i) fc.append(int(h->FutureCounts[i]));
            obj["FUTURECOUNTS"] = fc;
            obj["BOXMIN"] = QJsonArray{ h->BoxMin.X, h->BoxMin.Y, h->BoxMin.Z };
            obj["BOXMAX"] = QJsonArray{ h->BoxMax.X, h->BoxMax.Y, h->BoxMax.Z };
            obj["SPHCENTER"] = QJsonArray{ h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
            obj["SPHRADIUS"] = h->SphRadius;
        }
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        W3dShdSubMeshHeaderStruct h{};
        h.NumTriangles = dataObj.value("NUMTRIANGLES").toInt();
        h.NumVertices = dataObj.value("NUMVERTICES").toInt();
        QJsonArray fc = dataObj.value("FUTURECOUNTS").toArray();
        for (int i = 0; i < 2 && i < fc.size(); ++i) h.FutureCounts[i] = fc[i].toInt();
        QJsonArray bmin = dataObj.value("BOXMIN").toArray();
        if (bmin.size() >= 3) { h.BoxMin.X = bmin[0].toDouble(); h.BoxMin.Y = bmin[1].toDouble(); h.BoxMin.Z = bmin[2].toDouble(); }
        QJsonArray bmax = dataObj.value("BOXMAX").toArray();
        if (bmax.size() >= 3) { h.BoxMax.X = bmax[0].toDouble(); h.BoxMax.Y = bmax[1].toDouble(); h.BoxMax.Z = bmax[2].toDouble(); }
        QJsonArray sph = dataObj.value("SPHCENTER").toArray();
        if (sph.size() >= 3) { h.SphCenter.X = sph[0].toDouble(); h.SphCenter.Y = sph[1].toDouble(); h.SphCenter.Z = sph[2].toDouble(); }
        h.SphRadius = dataObj.value("SPHRADIUS").toDouble();
        item.length = sizeof(W3dShdSubMeshHeaderStruct);
        item.data.resize(item.length);
        std::memcpy(item.data.data(), &h, sizeof(h));
    }
};

struct ShdSubMeshShaderClassIdSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        if (item.data.size() >= sizeof(W3dShdSubMeshShaderClassIdStruct)) {
            const auto* s = reinterpret_cast<const W3dShdSubMeshShaderClassIdStruct*>(item.data.data());
            obj["CLASSID"] = int(s->ShaderClass);
        }
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        W3dShdSubMeshShaderClassIdStruct s{};
        s.ShaderClass = dataObj.value("CLASSID").toInt();
        item.length = sizeof(W3dShdSubMeshShaderClassIdStruct);
        item.data.resize(item.length);
        std::memcpy(item.data.data(), &s, sizeof(s));
    }
};

struct ShdSubMeshTrianglesSerializer : ChunkSerializer {
    struct W3dTri16Struct { uint16_t I, J, K; };
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["TRIANGLES"] = structsToJsonArray<W3dTri16Struct>(
            item.data,
            [](const W3dTri16Struct& t) {
                QJsonArray arr; arr.append(int(t.I)); arr.append(int(t.J)); arr.append(int(t.K));
                return QJsonValue(arr);
            }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("TRIANGLES").toArray();
        item.data = jsonArrayToStructs<W3dTri16Struct>(arr, [](const QJsonValue& val) {
            W3dTri16Struct t{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { t.I = uint16_t(a[0].toInt()); t.J = uint16_t(a[1].toInt()); t.K = uint16_t(a[2].toInt()); }
            return t;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct ShdSubMeshUV0Serializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["UV0"] = structsToJsonArray<W3dTexCoordStruct>(
            item.data,
            [](const W3dTexCoordStruct& t) { return QJsonArray{ t.U, t.V }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("UV0").toArray();
        item.data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const QJsonValue& val) {
            W3dTexCoordStruct t{}; QJsonArray a = val.toArray();
            if (a.size() >= 2) { t.U = a[0].toDouble(); t.V = a[1].toDouble(); }
            return t;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct ShdSubMeshUV1Serializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["UV1"] = structsToJsonArray<W3dTexCoordStruct>(
            item.data,
            [](const W3dTexCoordStruct& t) { return QJsonArray{ t.U, t.V }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("UV1").toArray();
        item.data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const QJsonValue& val) {
            W3dTexCoordStruct t{}; QJsonArray a = val.toArray();
            if (a.size() >= 2) { t.U = a[0].toDouble(); t.V = a[1].toDouble(); }
            return t;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct ShdSubMeshTangentBasisSSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["TANGENT_BASIS_S"] = structsToJsonArray<W3dVectorStruct>(
            item.data,
            [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("TANGENT_BASIS_S").toArray();
        item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
            W3dVectorStruct v{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
            return v;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct ShdSubMeshTangentBasisTSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["TANGENT_BASIS_T"] = structsToJsonArray<W3dVectorStruct>(
            item.data,
            [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("TANGENT_BASIS_T").toArray();
        item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
            W3dVectorStruct v{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
            return v;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct ShdSubMeshTangentBasisSXTSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["TANGENT_BASIS_SXT"] = structsToJsonArray<W3dVectorStruct>(
            item.data,
            [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("TANGENT_BASIS_SXT").toArray();
        item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
            W3dVectorStruct v{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
            return v;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct SecondaryVerticesSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["SECONDARY_VERTICES"] = structsToJsonArray<W3dVectorStruct>(
            item.data,
            [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("SECONDARY_VERTICES").toArray();
        item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
            W3dVectorStruct v{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
            return v;
            });
        item.length = uint32_t(item.data.size());
    }
};

struct SecondaryVertexNormalsSerializer : ChunkSerializer {
    ordered_json toJson(const ChunkItem& item) const override {
        ordered_json obj;
        obj["SECONDARY_VERTEX_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
            item.data,
            [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
        );
        return obj;
    }
    void fromJson(const ordered_json& dataObj, ChunkItem& item) const override {
        QJsonArray arr = dataObj.value("SECONDARY_VERTEX_NORMALS").toArray();
        item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
            W3dVectorStruct v{}; QJsonArray a = val.toArray();
            if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
            return v;
            });
        item.length = uint32_t(item.data.size());
    }
};

*/




    // static serializer instances
    static const MeshHeader1Serializer meshHeader1SerializerInstance;
    static const VerticesSerializer verticesSerializerInstance;
    static const VertexNormalsSerializer vertexNormalsSerializerInstance;
    static const SurrenderNormalsSerializer surrenderNormalsSerializerInstance;
    static const TexCoordsSerializer texCoordsSerializerInstance;
    static const MaterialsSerializer materialsSerializerInstance;
    static const SurrenderTrianglesSerializer surrenderTrianglesSerializerInstance;
    static const MeshUserTextSerializer meshUserTextSerializerInstance;
    static const VertexColorsSerializer vertexColorsSerializerInstance;
    static const VertexInfluencesSerializer vertexInfluencesSerializerInstance;
    static const DamageHeaderSerializer damageHeaderSerializerInstance;
  /*
    static const DamageVerticesSerializer damageVerticesSerializerInstance;
    static const DamageColorsSerializer damageColorsSerializerInstance;
    static const Materials2Serializer materials2SerializerInstance;
    static const Material3NameSerializer material3NameSerializerInstance;
    static const Material3InfoSerializer material3InfoSerializerInstance;
    static const Map3FilenameSerializer map3FilenameSerializerInstance;
    static const Map3InfoSerializer map3InfoSerializerInstance;
    static const MeshHeader3Serializer meshHeader3SerializerInstance;
    static const TrianglesSerializer trianglesSerializerInstance;
    static const PerTriMaterialsSerializer perTriMaterialsSerializerInstance;
    static const VertexShadeIndicesSerializer vertexShadeIndicesSerializerInstance;
    static const MaterialInfoSerializer materialInfoSerializerInstance;
    static const ShadersSerializer shadersSerializerInstance;
    static const VertexMaterialNameSerializer vertexMaterialNameSerializerInstance;
    static const VertexMaterialInfoSerializer vertexMaterialInfoSerializerInstance;
    static const VertexMapperArgs0Serializer vertexMapperArgs0SerializerInstance;
    static const VertexMapperArgs1Serializer vertexMapperArgs1SerializerInstance;
    static const TextureNameSerializer textureNameSerializerInstance;
    static const TextureInfoSerializer textureInfoSerializerInstance;
    static const VertexMaterialIdsSerializer vertexMaterialIdsSerializerInstance;
    static const ShaderIdsSerializer shaderIdsSerializerInstance;
    static const DcgSerializer dcgSerializerInstance;
    static const DigSerializer digSerializerInstance;
    static const ScgSerializer scgSerializerInstance;
    static const ShaderMaterialIdSerializer shaderMaterialIdSerializerInstance;
    static const TextureIdsSerializer textureIdsSerializerInstance;
    static const StageTexCoordsSerializer stageTexCoordsSerializerInstance;
    static const PerFaceTexCoordIdsSerializer perFaceTexCoordIdsSerializerInstance;
    static const ShaderMaterialHeaderSerializer shaderMaterialHeaderSerializerInstance;
    static const ShaderMaterialPropertySerializer shaderMaterialPropertySerializerInstance;
    static const DeformSerializer deformSerializerInstance;
    static const DeformSetSerializer deformSetSerializerInstance;
    static const DeformKeyframeSerializer deformKeyframeSerializerInstance;
    static const DeformDataSerializer deformDataSerializerInstance;
    static const TangentsSerializer tangentsSerializerInstance;
    static const BinormalsSerializer binormalsSerializerInstance;
    static const Ps2ShadersSerializer ps2ShadersSerializerInstance;
    static const AABTreeHeaderSerializer aabTreeHeaderSerializerInstance;
    static const AABTreePolyIndicesSerializer aabTreePolyIndicesSerializerInstance;
    static const AABTreeNodesSerializer aabTreeNodesSerializerInstance;
    static const HierarchyHeaderSerializer hierarchyHeaderSerializerInstance;
    static const PivotsSerializer pivotsSerializerInstance;
    static const PivotFixupsSerializer pivotFixupsSerializerInstance;
    static const AnimationHeaderSerializer animationHeaderSerializerInstance;
    static const AnimationChannelSerializer animationChannelSerializerInstance;
    static const BitChannelSerializer bitChannelSerializerInstance;
    static const CompressedAnimHeaderSerializer compressedAnimHeaderSerializerInstance;
    static const CompressedAnimChannelSerializer compressedAnimChannelSerializerInstance;
    static const CompressedBitChannelSerializer compressedBitChannelSerializerInstance;
    static const CompressedAnimMotionChannelSerializer compressedAnimMotionChannelSerializerInstance;
    static const MorphAnimHeaderSerializer morphAnimHeaderSerializerInstance;
    static const MorphAnimPoseNameSerializer morphAnimPoseNameSerializerInstance;
    static const MorphAnimKeyDataSerializer morphAnimKeyDataSerializerInstance;
    static const MorphAnimPivotChannelDataSerializer morphAnimPivotChannelDataSerializerInstance;
    static const HModelHeaderSerializer hModelHeaderSerializerInstance;
    static const NodeSerializer nodeSerializerInstance("RENDEROBJNAME");
    static const NodeSerializer collisionNodeSerializerInstance("COLLISIONMESHNAME");
    static const NodeSerializer skinNodeSerializerInstance("SKINMESHNAME");
    static const NodeSerializer shadowNodeSerializerInstance("SHADOWMESHNAME");
    static const HModelAuxDataSerializer hModelAuxDataSerializerInstance;
    static const LodModelHeaderSerializer lodModelHeaderSerializerInstance;
    static const LodSerializer lodSerializerInstance;
    static const CollectionHeaderSerializer collectionHeaderSerializerInstance;
    static const CollectionObjNameSerializer collectionObjNameSerializerInstance;
    static const PlaceholderSerializer placeholderSerializerInstance;
    static const TransformNodeSerializer transformNodeSerializerInstance;
    static const PointsSerializer pointsSerializerInstance;
    static const LightInfoSerializer lightInfoSerializerInstance;
    static const SpotLightInfoSerializer spotLightInfoSerializerInstance;
    static const LightAttenuationSerializer nearAttenuationSerializerInstance;
    static const LightAttenuationSerializer farAttenuationSerializerInstance;
    static const SpotLightInfo50Serializer spotLightInfo50SerializerInstance;
    static const LightPulseSerializer lightPulseSerializerInstance;
    static const EmitterHeaderSerializer emitterHeaderSerializerInstance;
    static const EmitterUserDataSerializer emitterUserDataSerializerInstance;
    static const EmitterInfoSerializer emitterInfoSerializerInstance;
    static const EmitterInfoV2Serializer emitterInfoV2SerializerInstance;
    static const EmitterPropsSerializer emitterPropsSerializerInstance;
    static const EmitterColorKeyframeSerializer emitterColorKeyframeSerializerInstance;
    static const EmitterOpacityKeyframeSerializer emitterOpacityKeyframeSerializerInstance;
    static const EmitterSizeKeyframeSerializer emitterSizeKeyframeSerializerInstance;
    static const EmitterLinePropertiesSerializer emitterLinePropertiesSerializerInstance;
    static const EmitterRotationKeyframesSerializer emitterRotationKeyframesSerializerInstance;
    static const EmitterFrameKeyframesSerializer emitterFrameKeyframesSerializerInstance;
    static const EmitterBlurTimeKeyframesSerializer emitterBlurTimeKeyframesSerializerInstance;
    static const EmitterExtraInfoSerializer emitterExtraInfoSerializerInstance;
    static const AggregateHeaderSerializer aggregateHeaderSerializerInstance;
    static const AggregateInfoSerializer aggregateInfoSerializerInstance;
    static const TextureReplacerInfoSerializer textureReplacerInfoSerializerInstance;
    static const AggregateClassInfoSerializer aggregateClassInfoSerializerInstance;
    static const HLodHeaderSerializer hLodHeaderSerializerInstance;
    static const HLodLodArraySerializer hLodLodArraySerializerInstance;
    static const HLodSubObjectSerializer hLodSubObjectSerializerInstance;
    static const BoxSerializer boxSerializerInstance;
    static const SphereSerializer sphereSerializerInstance;
    static const RingSerializer ringSerializerInstance;
    static const NullObjectSerializer nullObjectSerializerInstance;
    static const LightTransformSerializer lightTransformSerializerInstance;
    static const DazzleNameSerializer dazzleNameSerializerInstance;
    static const DazzleTypenameSerializer dazzleTypenameSerializerInstance;
    static const SoundRObjHeaderSerializer soundRObjHeaderSerializerInstance;
    static const SoundRObjDefinitionSerializer soundRObjDefinitionSerializerInstance;
    static const ShdMeshNameSerializer shdMeshNameSerializerInstance;
    static const ShdMeshHeaderSerializer shdMeshHeaderSerializerInstance;
    static const ShdSubMeshHeaderSerializer shdSubMeshHeaderSerializerInstance;
    static const ShdSubMeshShaderClassIdSerializer shdSubMeshShaderClassIdSerializerInstance;
    static const ShdSubMeshTrianglesSerializer shdSubMeshTrianglesSerializerInstance;
    static const ShdSubMeshUV0Serializer shdSubMeshUV0SerializerInstance;
    static const ShdSubMeshUV1Serializer shdSubMeshUV1SerializerInstance;
    static const ShdSubMeshTangentBasisSSerializer shdSubMeshTangentBasisSSerializerInstance;
    static const ShdSubMeshTangentBasisTSerializer shdSubMeshTangentBasisTSerializerInstance;
    static const ShdSubMeshTangentBasisSXTSerializer shdSubMeshTangentBasisSXTSerializerInstance;
    static const SecondaryVerticesSerializer secondaryVerticesSerializerInstance;
    static const SecondaryVertexNormalsSerializer secondaryVertexNormalsSerializerInstance;
    */
} // namespace

const std::unordered_map<uint32_t, const ChunkSerializer*>& chunkSerializerRegistry() {
    static const std::unordered_map<uint32_t, const ChunkSerializer*> registry = {
        {0x0001, &meshHeader1SerializerInstance},
        {0x0002, &verticesSerializerInstance},
        {0x0003, &vertexNormalsSerializerInstance},
        {0x0004, &surrenderNormalsSerializerInstance},
        {0x0005, &texCoordsSerializerInstance},
        {0x0006, &materialsSerializerInstance},
        {0x0009, &surrenderTrianglesSerializerInstance},
        {0x000C, &meshUserTextSerializerInstance},
        {0x000D, &vertexColorsSerializerInstance},
        {0x000E, &vertexInfluencesSerializerInstance},
        {0x0010, &damageHeaderSerializerInstance},
        /*
        {0x0011, &damageVerticesSerializerInstance},
        {0x0012, &damageColorsSerializerInstance},
        {0x0014, &materials2SerializerInstance},
        {0x0017, &material3NameSerializerInstance},
        {0x0018, &material3InfoSerializerInstance},
        {0x001A, &map3FilenameSerializerInstance},
        {0x001B, &map3InfoSerializerInstance},
        {0x001F, &meshHeader3SerializerInstance},
        {0x0020, &trianglesSerializerInstance},
        {0x0021, &perTriMaterialsSerializerInstance},
        {0x0022, &vertexShadeIndicesSerializerInstance},
        {0x0028, &materialInfoSerializerInstance},
        {0x0029, &shadersSerializerInstance},
        {0x002C, &vertexMaterialNameSerializerInstance},
        {0x002D, &vertexMaterialInfoSerializerInstance},
        {0x002E, &vertexMapperArgs0SerializerInstance},
        {0x002F, &vertexMapperArgs1SerializerInstance},
        {0x0032, &textureNameSerializerInstance},
        {0x0033, &textureInfoSerializerInstance},
        {0x0039, &vertexMaterialIdsSerializerInstance},
        {0x003A, &shaderIdsSerializerInstance},
        {0x003B, &dcgSerializerInstance},
        {0x003C, &digSerializerInstance},
        {0x003E, &scgSerializerInstance},
        {0x003F, &shaderMaterialIdSerializerInstance},
        {0x0049, &textureIdsSerializerInstance},
        {0x004A, &stageTexCoordsSerializerInstance},
        {0x004B, &perFaceTexCoordIdsSerializerInstance},
        {0x0052, &shaderMaterialHeaderSerializerInstance},
        {0x0053, &shaderMaterialPropertySerializerInstance},
        {0x0058, &deformSerializerInstance},
        {0x0059, &deformSetSerializerInstance},
        {0x005A, &deformKeyframeSerializerInstance},
        {0x005B, &deformDataSerializerInstance},
        {0x0060, &tangentsSerializerInstance},
        {0x0061, &binormalsSerializerInstance},
        {0x0080, &ps2ShadersSerializerInstance},
        {0x0091, &aabTreeHeaderSerializerInstance},
        {0x0092, &aabTreePolyIndicesSerializerInstance},
        {0x0093, &aabTreeNodesSerializerInstance},
        {0x0101, &hierarchyHeaderSerializerInstance},
        {0x0102, &pivotsSerializerInstance},
        {0x0103, &pivotFixupsSerializerInstance},
        {0x0201, &animationHeaderSerializerInstance},
        {0x0202, &animationChannelSerializerInstance},
        {0x0203, &bitChannelSerializerInstance},
        {0x0281, &compressedAnimHeaderSerializerInstance},
        {0x0282, &compressedAnimChannelSerializerInstance},
        {0x0283, &compressedBitChannelSerializerInstance},
        {0x0284, &compressedAnimMotionChannelSerializerInstance},
        {0x02C1, &morphAnimHeaderSerializerInstance},
        {0x02C3, &morphAnimPoseNameSerializerInstance},
        {0x02C4, &morphAnimKeyDataSerializerInstance},
        {0x02C5, &morphAnimPivotChannelDataSerializerInstance},
        {0x0301, &hModelHeaderSerializerInstance},
        {0x0302, &nodeSerializerInstance},
        {0x0303, &collisionNodeSerializerInstance},
        {0x0304, &skinNodeSerializerInstance},
        {0x0305, &hModelAuxDataSerializerInstance},
        {0x0306, &shadowNodeSerializerInstance},
        {0x0401, &lodModelHeaderSerializerInstance},
        {0x0402, &lodSerializerInstance},
        {0x0421, &collectionHeaderSerializerInstance},
        {0x0422, &collectionObjNameSerializerInstance},
        {0x0423, &placeholderSerializerInstance},
        {0x0424, &transformNodeSerializerInstance},
        {0x0440, &pointsSerializerInstance},
        {0x0461, &lightInfoSerializerInstance},
        {0x0462, &spotLightInfoSerializerInstance},
        {0x0463, &nearAttenuationSerializerInstance},
        {0x0464, &farAttenuationSerializerInstance},
        {0x0465, &spotLightInfo50SerializerInstance},
        {0x0466, &lightPulseSerializerInstance},
        {0x0501, &emitterHeaderSerializerInstance},
        {0x0502, &emitterUserDataSerializerInstance},
        {0x0503, &emitterInfoSerializerInstance},
        {0x0504, &emitterInfoV2SerializerInstance},
        {0x0505, &emitterPropsSerializerInstance},
        {0x0506, &emitterColorKeyframeSerializerInstance},
        {0x0507, &emitterOpacityKeyframeSerializerInstance},
        {0x0508, &emitterSizeKeyframeSerializerInstance},
        {0x0509, &emitterLinePropertiesSerializerInstance},
        {0x050A, &emitterRotationKeyframesSerializerInstance},
        {0x050B, &emitterFrameKeyframesSerializerInstance},
        {0x050C, &emitterBlurTimeKeyframesSerializerInstance},
        {0x050D, &emitterExtraInfoSerializerInstance},
        {0x0601, &aggregateHeaderSerializerInstance},
        {0x0602, &aggregateInfoSerializerInstance},
        {0x0603, &textureReplacerInfoSerializerInstance},
        {0x0604, &aggregateClassInfoSerializerInstance},
        {0x0701, &hLodHeaderSerializerInstance},
        {0x0702, &hLodLodArraySerializerInstance},
        {0x0704, &hLodSubObjectSerializerInstance},
        {0x0740, &boxSerializerInstance},
        {0x0741, &sphereSerializerInstance},
        {0x0742, &ringSerializerInstance},
        {0x0750, &nullObjectSerializerInstance},
        {0x0802, &lightTransformSerializerInstance},
        {0x0901, &dazzleNameSerializerInstance},
        {0x0902, &dazzleTypenameSerializerInstance},
        {0x0A01, &soundRObjHeaderSerializerInstance},
        {0x0A02, &soundRObjDefinitionSerializerInstance},
        {0x0B01, &shdMeshNameSerializerInstance },
        {0x0B02, &shdMeshHeaderSerializerInstance },
        {0x0B03, &meshUserTextSerializerInstance },
        {0x0B21, &shdSubMeshHeaderSerializerInstance },
        {0x0B41, &shdSubMeshShaderClassIdSerializerInstance },
        {0x0B43, &verticesSerializerInstance },
        {0x0B44, &vertexNormalsSerializerInstance },
        {0x0B45, &shdSubMeshTrianglesSerializerInstance },
        {0x0B46, &vertexShadeIndicesSerializerInstance },
        {0x0B47, &shdSubMeshUV0SerializerInstance },
        {0x0B48, &shdSubMeshUV1SerializerInstance },
        {0x0B49, &shdSubMeshTangentBasisSSerializerInstance },
        {0x0B4A, &shdSubMeshTangentBasisTSerializerInstance },
        {0x0B4B, &shdSubMeshTangentBasisSXTSerializerInstance },
        {0x0B4C, &vertexColorsSerializerInstance },
        {0x0B4D, &vertexInfluencesSerializerInstance },
        {0x0C00, &secondaryVerticesSerializerInstance },
        {0x0C01, &secondaryVertexNormalsSerializerInstance },
        */
    };
    return registry;
}
