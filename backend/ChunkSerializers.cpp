#include "ChunkSerializers.h"
#include "ChunkSerializer.h"
#include "ChunkItem.h"
#include "FormatUtils.h"
#include "W3DStructs.h"

#include <QJsonArray>
#include <QByteArray>
#include <vector>
#include <cstring>
#include <algorithm>

namespace {

    // helper function: convert array of structs to QJsonArray
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

    // helper function: convert QJsonArray to byte vector of structs
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

    // Serializer for chunk 0x0001 (W3dMeshHeader1)
    struct MeshHeader1Serializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject dataObj;
            if (item.data.size() >= sizeof(W3dMeshHeader1)) {
                const auto* h = reinterpret_cast<const W3dMeshHeader1*>(item.data.data());
                dataObj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                dataObj["MESHNAME"] = QString::fromUtf8(h->MeshName, strnlen(h->MeshName, W3D_NAME_LEN));
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
                QJsonArray fc;
                for (int i = 0; i < 8; ++i) fc.append(static_cast<int>(h->FutureCounts[i]));
                dataObj["FUTURECOUNTS"] = fc;
                dataObj["LODMIN"] = h->LODMin;
                dataObj["LODMAX"] = h->LODMax;
                dataObj["MIN"] = QJsonArray{ h->Min.X, h->Min.Y, h->Min.Z };
                dataObj["MAX"] = QJsonArray{ h->Max.X, h->Max.Y, h->Max.Z };
                dataObj["SPHCENTER"] = QJsonArray{ h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
                dataObj["SPHRADIUS"] = h->SphRadius;
                dataObj["TRANSLATION"] = QJsonArray{ h->Translation.X, h->Translation.Y, h->Translation.Z };
                QJsonArray rot;
                for (int i = 0; i < 9; ++i) rot.append(h->Rotation[i]);
                dataObj["ROTATION"] = rot;
                dataObj["MASSCENTER"] = QJsonArray{ h->MassCenter.X, h->MassCenter.Y, h->MassCenter.Z };
                QJsonArray inertia;
                for (int i = 0; i < 9; ++i) inertia.append(h->Inertia[i]);
                dataObj["INERTIA"] = inertia;
                dataObj["VOLUME"] = h->Volume;
                dataObj["HIERARCHYTREENAME"] = QString::fromUtf8(h->HierarchyTreeName, strnlen(h->HierarchyTreeName, W3D_NAME_LEN));
                dataObj["HIERARCHYMODELNAME"] = QString::fromUtf8(h->HierarchyModelName, strnlen(h->HierarchyModelName, W3D_NAME_LEN));
                QJsonArray fu;
                for (int i = 0; i < 24; ++i) fu.append(static_cast<int>(h->FutureUse[i]));
                dataObj["FUTUREUSE"] = fu;
            }
            return dataObj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            W3dMeshHeader1 h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) {
                h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
            }
            QByteArray meshName = dataObj.value("MESHNAME").toString().toUtf8();
            std::memset(h.MeshName, 0, 16);
            std::memcpy(h.MeshName, meshName.constData(), std::min<int>(meshName.size(), 16));
            h.Attributes = dataObj.value("ATTRIBUTES").toInt();
            h.NumTriangles = dataObj.value("NUMTRIANGLES").toInt();
            h.NumQuads = dataObj.value("NUMQUADS").toInt();
            h.NumSrTris = dataObj.value("NUMSRTRIS").toInt();
            h.NumPovQuads = dataObj.value("NUMPOVQUADS").toInt();
            h.NumVertices = dataObj.value("NUMVERTICES").toInt();
            h.NumNormals = dataObj.value("NUMNORMALS").toInt();
            h.NumSrNormals = dataObj.value("NUMSRNORMALS").toInt();
            h.NumTexCoords = dataObj.value("NUMTEXCOORDS").toInt();
            h.NumMaterials = dataObj.value("NUMMATERIALS").toInt();
            h.NumVertColors = dataObj.value("NUMVERTCOLORS").toInt();
            h.NumVertInfluences = dataObj.value("NUMVERTINFLUENCES").toInt();
            h.NumDamageStages = dataObj.value("NUMDAMAGESTAGES").toInt();
            QJsonArray fc = dataObj.value("FUTURECOUNTS").toArray();
            for (int i = 0; i < 8 && i < fc.size(); ++i)
                h.FutureCounts[i] = fc[i].toInt();
            h.LODMin = dataObj.value("LODMIN").toDouble();
            h.LODMax = dataObj.value("LODMAX").toDouble();
            QJsonArray min = dataObj.value("MIN").toArray();
            if (min.size() >= 3) { h.Min.X = min[0].toDouble(); h.Min.Y = min[1].toDouble(); h.Min.Z = min[2].toDouble(); }
            QJsonArray max = dataObj.value("MAX").toArray();
            if (max.size() >= 3) { h.Max.X = max[0].toDouble(); h.Max.Y = max[1].toDouble(); h.Max.Z = max[2].toDouble(); }
            QJsonArray sph = dataObj.value("SPHCENTER").toArray();
            if (sph.size() >= 3) { h.SphCenter.X = sph[0].toDouble(); h.SphCenter.Y = sph[1].toDouble(); h.SphCenter.Z = sph[2].toDouble(); }
            h.SphRadius = dataObj.value("SPHRADIUS").toDouble();
            QJsonArray trans = dataObj.value("TRANSLATION").toArray();
            if (trans.size() >= 3) { h.Translation.X = trans[0].toDouble(); h.Translation.Y = trans[1].toDouble(); h.Translation.Z = trans[2].toDouble(); }
            QJsonArray rot = dataObj.value("ROTATION").toArray();
            for (int i = 0; i < 9 && i < rot.size(); ++i) h.Rotation[i] = rot[i].toDouble();
            QJsonArray mass = dataObj.value("MASSCENTER").toArray();
            if (mass.size() >= 3) { h.MassCenter.X = mass[0].toDouble(); h.MassCenter.Y = mass[1].toDouble(); h.MassCenter.Z = mass[2].toDouble(); }
            QJsonArray inertia = dataObj.value("INERTIA").toArray();
            for (int i = 0; i < 9 && i < inertia.size(); ++i) h.Inertia[i] = inertia[i].toDouble();
            h.Volume = dataObj.value("VOLUME").toDouble();
            QByteArray ht = dataObj.value("HIERARCHYTREENAME").toString().toUtf8();
            std::memset(h.HierarchyTreeName, 0, 16);
            std::memcpy(h.HierarchyTreeName, ht.constData(), std::min<int>(ht.size(), 16));
            QByteArray hm = dataObj.value("HIERARCHYMODELNAME").toString().toUtf8();
            std::memset(h.HierarchyModelName, 0, 16);
            std::memcpy(h.HierarchyModelName, hm.constData(), std::min<int>(hm.size(), 16));
            QJsonArray fu = dataObj.value("FUTUREUSE").toArray();
            for (int i = 0; i < 24 && i < fu.size(); ++i) h.FutureUse[i] = fu[i].toInt();
            item.length = sizeof(W3dMeshHeader1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0002 (VERTICES)
    struct VerticesSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["VERTICES"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTICES").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0003 (VERTEX_NORMALS)
    struct VertexNormalsSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["VERTEX_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTEX_NORMALS").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0004 (SURRENDER_NORMALS)
    struct SurrenderNormalsSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["SURRENDER_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SURRENDER_NORMALS").toArray();
            item.data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0005 (TEXCOORDS)
    struct TexCoordsSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["TEXCOORDS"] = structsToJsonArray<W3dTexCoordStruct>(
                item.data,
                [](const W3dTexCoordStruct& t) { return QJsonArray{ t.U, t.V }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("TEXCOORDS").toArray();
            item.data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const QJsonValue& val) {
                W3dTexCoordStruct t{};
                auto a = val.toArray();
                if (a.size() >= 2) { t.U = a[0].toDouble(); t.V = a[1].toDouble(); }
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x0006 (MATERIALS)
    struct MaterialsSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["MATERIALS"] = structsToJsonArray<W3dMaterial1Struct>(
                item.data,
                [](const W3dMaterial1Struct& m) {
                    QJsonObject mo;
                    mo["MATERIALNAME"] = QString::fromUtf8(m.MaterialName, int(strnlen(m.MaterialName, 16)));
                    mo["PRIMARYNAME"] = QString::fromUtf8(m.PrimaryName, int(strnlen(m.PrimaryName, 16)));
                    mo["SECONDARYNAME"] = QString::fromUtf8(m.SecondaryName, int(strnlen(m.SecondaryName, 16)));
                    mo["RENDERFLAGS"] = int(m.RenderFlags);
                    mo["COLOR"] = QJsonArray{ int(m.Red), int(m.Green), int(m.Blue) };
                    return mo;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("MATERIALS").toArray();
            item.data = jsonArrayToStructs<W3dMaterial1Struct>(arr, [](const QJsonValue& val) {
                W3dMaterial1Struct m{};
                QJsonObject o = val.toObject();
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
                auto c = o.value("COLOR").toArray();
                if (c.size() >= 3) { m.Red = c[0].toInt(); m.Green = c[1].toInt(); m.Blue = c[2].toInt(); }
                return m;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // static serializer instances
    static const MeshHeader1Serializer meshHeader1SerializerInstance;
    static const VerticesSerializer verticesSerializerInstance;
    static const VertexNormalsSerializer vertexNormalsSerializerInstance;
    static const SurrenderNormalsSerializer surrenderNormalsSerializerInstance;
    static const TexCoordsSerializer texCoordsSerializerInstance;
    static const MaterialsSerializer materialsSerializerInstance;

} // namespace

const std::unordered_map<uint32_t, const ChunkSerializer*>& chunkSerializerRegistry() {
    static const std::unordered_map<uint32_t, const ChunkSerializer*> registry = {
        {0x0001, &meshHeader1SerializerInstance},
        {0x0002, &verticesSerializerInstance},
        {0x0003, &vertexNormalsSerializerInstance},
        {0x0004, &surrenderNormalsSerializerInstance},
        {0x0005, &texCoordsSerializerInstance},
        {0x0006, &materialsSerializerInstance},
    };
    return registry;
}