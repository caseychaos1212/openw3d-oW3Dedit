#include "ChunkSerializers.h"
#include "ChunkSerializer.h"
#include "ChunkItem.h"
#include "FormatUtils.h"
#include "W3DStructs.h"

#include <QJsonArray>
#include <QByteArray>
#include <QString>
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

    

    // Serializer for chunk 0x0009 (O_W3D_CHUNK_SURRENDER_TRIANGLES)
    struct SurrenderTrianglesSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["SURRENDER_TRIANGLES"] = structsToJsonArray<W3dSurrenderTriStruct>(
                item.data,
                [](const W3dSurrenderTriStruct& t) {
                    QJsonObject to;
                    QJsonArray vi; for (int i = 0; i < 3; ++i) vi.append(int(t.VIndex[i]));
                    to["VINDEX"] = vi;
                    QJsonArray tc;
                    for (int i = 0; i < 3; ++i) tc.append(QJsonArray{ t.TexCoord[i].U, t.TexCoord[i].V });
                    to["TEXCOORD"] = tc;
                    to["MATERIALIDX"] = int(t.MaterialIDx);
                    to["NORMAL"] = QJsonArray{ t.Normal.X, t.Normal.Y, t.Normal.Z };
                    to["ATTRIBUTES"] = int(t.Attributes);
                    QJsonArray g;
                    for (int i = 0; i < 3; ++i) g.append(QJsonArray{ int(t.Gouraud[i].R), int(t.Gouraud[i].G), int(t.Gouraud[i].B) });
                    to["GOURAUD"] = g;
                    return to;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SURRENDER_TRIANGLES").toArray();
            item.data = jsonArrayToStructs<W3dSurrenderTriStruct>(arr, [](const QJsonValue& val) {
                W3dSurrenderTriStruct t{};
                QJsonObject o = val.toObject();
                QJsonArray vi = o.value("VINDEX").toArray();
                for (int i = 0; i < 3 && i < vi.size(); ++i) t.VIndex[i] = vi[i].toInt();
                QJsonArray tcArr = o.value("TEXCOORD").toArray();
                for (int i = 0; i < 3 && i < tcArr.size(); ++i) {
                    auto a = tcArr[i].toArray();
                    if (a.size() >= 2) { t.TexCoord[i].U = a[0].toDouble(); t.TexCoord[i].V = a[1].toDouble(); }
                }
                t.MaterialIDx = o.value("MATERIALIDX").toInt();
                QJsonArray n = o.value("NORMAL").toArray();
                if (n.size() >= 3) { t.Normal.X = n[0].toDouble(); t.Normal.Y = n[1].toDouble(); t.Normal.Z = n[2].toDouble(); }
                t.Attributes = o.value("ATTRIBUTES").toInt();
                QJsonArray gArr = o.value("GOURAUD").toArray();
                for (int i = 0; i < 3 && i < gArr.size(); ++i) {
                    auto ga = gArr[i].toArray();
                    if (ga.size() >= 3) { t.Gouraud[i].R = ga[0].toInt(); t.Gouraud[i].G = ga[1].toInt(); t.Gouraud[i].B = ga[2].toInt(); }
                }
                return t;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x000C (W3D_CHUNK_MESH_USER_TEXT)
    struct MeshUserTextSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["TEXT"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("TEXT").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x000D (W3D_CHUNK_VERTEX_COLORS)
    struct VertexColorsSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["VERTEX_COLORS"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B) }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTEX_COLORS").toArray();
            item.data = jsonArrayToStructs<W3dRGBStruct>(arr, [](const QJsonValue& val) {
                W3dRGBStruct c{};
                auto a = val.toArray();
                if (a.size() >= 3) { c.R = a[0].toInt(); c.G = a[1].toInt(); c.B = a[2].toInt(); }
                return c;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x000E (W3D_CHUNK_VERTEX_INFLUENCES)
    struct VertexInfluencesSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["VERTEX_INFLUENCES"] = structsToJsonArray<W3dVertInfStruct>(
                item.data,
                [](const W3dVertInfStruct& v) {
                    QJsonObject o;
                    QJsonArray b; for (int i = 0; i < 2; ++i) b.append(int(v.BoneIdx[i]));
                    QJsonArray w; for (int i = 0; i < 2; ++i) w.append(int(v.Weight[i]));
                    o["BONEIDX"] = b;
                    o["WEIGHT"] = w;
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("VERTEX_INFLUENCES").toArray();
            item.data = jsonArrayToStructs<W3dVertInfStruct>(arr, [](const QJsonValue& val) {
                W3dVertInfStruct v{};
                QJsonObject o = val.toObject();
                QJsonArray b = o.value("BONEIDX").toArray();
                for (int i = 0; i < 2 && i < b.size(); ++i) v.BoneIdx[i] = uint16_t(b[i].toInt());
                QJsonArray w = o.value("WEIGHT").toArray();
                for (int i = 0; i < 2 && i < w.size(); ++i) v.Weight[i] = uint16_t(w[i].toInt());
                return v;
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
    static const SurrenderTrianglesSerializer surrenderTrianglesSerializerInstance;
    static const MeshUserTextSerializer meshUserTextSerializerInstance;
    static const VertexColorsSerializer vertexColorsSerializerInstance;
    static const VertexInfluencesSerializer vertexInfluencesSerializerInstance;

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
    };
    return registry;
}