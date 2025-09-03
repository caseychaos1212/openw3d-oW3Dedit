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
    // Serializer for chunk 0x0010 (W3D_CHUNK_DAMAGE_HEADER)
    struct DamageHeaderSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dDamageHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dDamageHeaderStruct*>(item.data.data());
                obj["NUMDAMAGEMATERIALS"] = int(h->NumDamageMaterials);
                obj["NUMDAMAGEVERTS"] = int(h->NumDamageVerts);
                obj["NUMDAMAGECOLORS"] = int(h->NumDamageColors);
                obj["DAMAGEINDEX"] = int(h->DamageIndex);
                QJsonArray fu;
                for (int i = 0; i < 4; ++i) fu.append(int(h->FutureUse[i]));
                obj["FUTUREUSE"] = fu;
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            W3dDamageHeaderStruct h{};
            h.NumDamageMaterials = dataObj.value("NUMDAMAGEMATERIALS").toInt();
            h.NumDamageVerts = dataObj.value("NUMDAMAGEVERTS").toInt();
            h.NumDamageColors = dataObj.value("NUMDAMAGECOLORS").toInt();
            h.DamageIndex = dataObj.value("DAMAGEINDEX").toInt();
            QJsonArray fu = dataObj.value("FUTUREUSE").toArray();
            for (int i = 0; i < 4 && i < fu.size(); ++i) h.FutureUse[i] = fu[i].toInt();
            item.length = sizeof(W3dDamageHeaderStruct);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), &h, sizeof(h));
        }
    };

    // Serializer for chunk 0x0011 (W3D_CHUNK_DAMAGE_VERTICES)
    struct DamageVerticesSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DAMAGE_VERTICES"] = structsToJsonArray<W3dMeshDamageVertexStruct>(
                item.data,
                [](const W3dMeshDamageVertexStruct& v) {
                    QJsonObject o;
                    o["VERTEXINDEX"] = int(v.VertexIndex);
                    o["NEWVERTEX"] = QJsonArray{ v.NewVertex.X, v.NewVertex.Y, v.NewVertex.Z };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DAMAGE_VERTICES").toArray();
            item.data = jsonArrayToStructs<W3dMeshDamageVertexStruct>(arr, [](const QJsonValue& val) {
                W3dMeshDamageVertexStruct v{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DAMAGE_COLORS"] = structsToJsonArray<W3dMeshDamageColorStruct>(
                item.data,
                [](const W3dMeshDamageColorStruct& c) {
                    QJsonObject o;
                    o["VERTEXINDEX"] = int(c.VertexIndex);
                    o["NEWCOLOR"] = QJsonArray{ int(c.NewColor.R), int(c.NewColor.G), int(c.NewColor.B) };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DAMAGE_COLORS").toArray();
            item.data = jsonArrayToStructs<W3dMeshDamageColorStruct>(arr, [](const QJsonValue& val) {
                W3dMeshDamageColorStruct c{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["MATERIALS2"] = structsToJsonArray<W3dMaterial2Struct>(
                item.data,
                [](const W3dMaterial2Struct& m) {
                    QJsonObject mo;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("MATERIALS2").toArray();
            item.data = jsonArrayToStructs<W3dMaterial2Struct>(arr, [](const QJsonValue& val) {
                W3dMaterial2Struct m{};
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0018 (W3D_CHUNK_MATERIAL3_INFO)
    struct Material3InfoSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["FILENAME"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("FILENAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x001B (W3D_CHUNK_MAP3_INFO)
    struct Map3InfoSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dMap3Struct)) {
                const auto* m = reinterpret_cast<const W3dMap3Struct*>(item.data.data());
                obj["MAPPINGTYPE"] = int(m->MappingType);
                obj["FRAMECOUNT"] = int(m->FrameCount);
                obj["FRAMERATE"] = int(m->FrameRate);
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject dataObj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["TRIANGLES"] = structsToJsonArray<W3dTriStruct>(
                item.data,
                [](const W3dTriStruct& t) {
                    QJsonObject to;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("TRIANGLES").toArray();
            item.data = jsonArrayToStructs<W3dTriStruct>(arr, [](const QJsonValue& val) {
                W3dTriStruct t{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint16_t) == 0) {
                const auto* begin = reinterpret_cast<const uint16_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint16_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["PER_TRI_MATERIALS"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["VERTEX_SHADE_INDICES"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dMaterialInfoStruct)) {
                const auto* m = reinterpret_cast<const W3dMaterialInfoStruct*>(item.data.data());
                obj["PASSCOUNT"] = int(m->PassCount);
                obj["VERTEXMATERIALCOUNT"] = int(m->VertexMaterialCount);
                obj["SHADERCOUNT"] = int(m->ShaderCount);
                obj["TEXTURECOUNT"] = int(m->TextureCount);
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["SHADERS"] = structsToJsonArray<W3dShaderStruct>(
                item.data,
                [](const W3dShaderStruct& s) {
                    QJsonObject o;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("SHADERS").toArray();
            item.data = jsonArrayToStructs<W3dShaderStruct>(arr, [](const QJsonValue& val) {
                W3dShaderStruct s{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x002D (W3D_CHUNK_VERTEX_MATERIAL_INFO)
    struct VertexMaterialInfoSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["ARGS"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("ARGS").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x002F (W3D_CHUNK_VERTEX_MAPPER_ARGS1)
    struct VertexMapperArgs1Serializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["ARGS"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("ARGS").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0032 (W3D_CHUNK_TEXTURE_NAME)
    struct TextureNameSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QString text = QString::fromUtf8(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["NAME"] = text;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray text = dataObj.value("NAME").toString().toUtf8();
            item.length = uint32_t(text.size() + 1);
            item.data.resize(item.length);
            std::memcpy(item.data.data(), text.constData(), text.size());
            item.data[text.size()] = 0;
        }
    };

    // Serializer for chunk 0x0033 (W3D_CHUNK_TEXTURE_INFO)
    struct TextureInfoSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dTextureInfoStruct)) {
                const auto* t = reinterpret_cast<const W3dTextureInfoStruct*>(item.data.data());
                obj["ATTRIBUTES"] = int(t->Attributes);
                obj["ANIMTYPE"] = int(t->AnimType);
                obj["FRAMECOUNT"] = int(t->FrameCount);
                obj["FRAMERATE"] = t->FrameRate;
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["VERTEX_MATERIAL_IDS"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["SHADER_IDS"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DCG"] = structsToJsonArray<W3dRGBAStruct>(
                item.data,
                [](const W3dRGBAStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B), int(c.A) }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DIG"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B) }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["SCG"] = structsToJsonArray<W3dRGBStruct>(
                item.data,
                [](const W3dRGBStruct& c) { return QJsonArray{ int(c.R), int(c.G), int(c.B) }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["SHADER_MATERIAL_ID"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["TEXTURE_IDS"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["STAGE_TEXCOORDS"] = structsToJsonArray<W3dTexCoordStruct>(
                item.data,
                [](const W3dTexCoordStruct& t) { return QJsonArray{ t.U, t.V }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["PER_FACE_TEXCOORD_IDS"] = structsToJsonArray<Vector3i>(
                item.data,
                [](const Vector3i& v) { return QJsonArray{ v.I, v.J, v.K }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dShaderMaterialHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dShaderMaterialHeaderStruct*>(item.data.data());
                obj["VERSION"] = int(h->Version);
                obj["SHADERNAME"] = QString::fromUtf8(h->ShaderName, strnlen(h->ShaderName, 32));
                obj["TECHNIQUE"] = int(h->Technique);
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dMeshDeform)) {
                const auto* d = reinterpret_cast<const W3dMeshDeform*>(item.data.data());
                obj["SETCOUNT"] = int(d->SetCount);
                obj["ALPHAPASSES"] = int(d->AlphaPasses);
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DEFORM_SET"] = structsToJsonArray<W3dDeformSetInfo>(
                item.data,
                [](const W3dDeformSetInfo& s) {
                    QJsonObject o; o["KEYFRAMECOUNT"] = int(s.KeyframeCount); o["FLAGS"] = int(s.flags); return o; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_SET").toArray();
            item.data = jsonArrayToStructs<W3dDeformSetInfo>(arr, [](const QJsonValue& val) {
                W3dDeformSetInfo s{};
                QJsonObject o = val.toObject();
                s.KeyframeCount = o.value("KEYFRAMECOUNT").toInt();
                s.flags = o.value("FLAGS").toInt();
                return s;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x005A (W3D_CHUNK_DEFORM_KEYFRAME)
    struct DeformKeyframeSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DEFORM_KEYFRAME"] = structsToJsonArray<W3dDeformKeyframeInfo>(
                item.data,
                [](const W3dDeformKeyframeInfo& k) {
                    QJsonObject o; o["DEFORMPERCENT"] = k.DeformPercent; o["DATACOUNT"] = int(k.DataCount); return o; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_KEYFRAME").toArray();
            item.data = jsonArrayToStructs<W3dDeformKeyframeInfo>(arr, [](const QJsonValue& val) {
                W3dDeformKeyframeInfo k{};
                QJsonObject o = val.toObject();
                k.DeformPercent = float(o.value("DEFORMPERCENT").toDouble());
                k.DataCount = o.value("DATACOUNT").toInt();
                return k;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x005B (W3D_CHUNK_DEFORM_DATA)
    struct DeformDataSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["DEFORM_DATA"] = structsToJsonArray<W3dDeformData>(
                item.data,
                [](const W3dDeformData& d) {
                    QJsonObject o;
                    o["VERTEXINDEX"] = int(d.VertexIndex);
                    o["POSITION"] = QJsonArray{ d.Position.X, d.Position.Y, d.Position.Z };
                    o["COLOR"] = QJsonArray{ int(d.Color.R), int(d.Color.G), int(d.Color.B), int(d.Color.A) };
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("DEFORM_DATA").toArray();
            item.data = jsonArrayToStructs<W3dDeformData>(arr, [](const QJsonValue& val) {
                W3dDeformData d{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["TANGENTS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["BINORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) { return QJsonArray{ v.X, v.Y, v.Z }; }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["PS2_SHADERS"] = structsToJsonArray<W3dPS2ShaderStruct>(
                item.data,
                [](const W3dPS2ShaderStruct& s) {
                    QJsonObject o;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PS2_SHADERS").toArray();
            item.data = jsonArrayToStructs<W3dPS2ShaderStruct>(arr, [](const QJsonValue& val) {
                W3dPS2ShaderStruct s{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["POLY_INDICES"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["AABTREE_NODES"] = structsToJsonArray<W3dMeshAABTreeNode>(
                item.data,
                [](const W3dMeshAABTreeNode& n) {
                    QJsonObject o;
                    o["MIN"] = QJsonArray{ n.Min.X, n.Min.Y, n.Min.Z };
                    o["MAX"] = QJsonArray{ n.Max.X, n.Max.Y, n.Max.Z };
                    o["FRONTORPOLY0"] = int(n.FrontOrPoly0);
                    o["BACKORPOLYCOUNT"] = int(n.BackOrPolyCount);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("AABTREE_NODES").toArray();
            item.data = jsonArrayToStructs<W3dMeshAABTreeNode>(arr, [](const QJsonValue& val) {
                W3dMeshAABTreeNode n{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            if (item.data.size() >= sizeof(W3dHierarchyStruct)) {
                const auto* h = reinterpret_cast<const W3dHierarchyStruct*>(item.data.data());
                obj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                obj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                obj["NUMPIVOTS"] = int(h->NumPivots);
                obj["CENTER"] = QJsonArray{ h->Center.X, h->Center.Y, h->Center.Z };
            }
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["PIVOTS"] = structsToJsonArray<W3dPivotStruct>(
                item.data,
                [](const W3dPivotStruct& p) {
                    QJsonObject o;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOTS").toArray();
            item.data = jsonArrayToStructs<W3dPivotStruct>(arr, [](const QJsonValue& val) {
                W3dPivotStruct p{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["PIVOT_FIXUPS"] = structsToJsonArray<W3dPivotFixupStruct>(
                item.data,
                [](const W3dPivotFixupStruct& f) {
                    QJsonObject o;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOT_FIXUPS").toArray();
            item.data = jsonArrayToStructs<W3dPivotFixupStruct>(arr, [](const QJsonValue& val) {
                W3dPivotFixupStruct f{};
                QJsonObject o = val.toObject();
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
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

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
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
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QByteArray arr(reinterpret_cast<const char*>(item.data.data()), int(item.data.size()));
            obj["POSENAME"] = QString::fromUtf8(arr.constData());
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QByteArray name = dataObj.value("POSENAME").toString().toUtf8();
            item.data.resize(name.size() + 1);
            std::memcpy(item.data.data(), name.constData(), name.size());
            item.data[name.size()] = 0;
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x02C4 (W3D_CHUNK_MORPHANIM_KEYDATA)
    struct MorphAnimKeyDataSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            obj["KEYS"] = structsToJsonArray<W3dMorphAnimKeyStruct>(
                item.data,
                [](const W3dMorphAnimKeyStruct& k) {
                    QJsonObject o;
                    o["MORPHFRAME"] = int(k.MorphFrame);
                    o["POSEFRAME"] = int(k.PoseFrame);
                    return o;
                }
            );
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("KEYS").toArray();
            item.data = jsonArrayToStructs<W3dMorphAnimKeyStruct>(arr, [](const QJsonValue& val) {
                W3dMorphAnimKeyStruct k{};
                QJsonObject o = val.toObject();
                k.MorphFrame = o.value("MORPHFRAME").toInt();
                k.PoseFrame = o.value("POSEFRAME").toInt();
                return k;
                });
            item.length = uint32_t(item.data.size());
        }
    };

    // Serializer for chunk 0x02C5 (W3D_CHUNK_MORPHANIM_PIVOTCHANNELDATA)
    struct MorphAnimPivotChannelDataSerializer : ChunkSerializer {
        QJsonObject toJson(const ChunkItem& item) const override {
            QJsonObject obj;
            QJsonArray arr;
            if (item.data.size() % sizeof(uint32_t) == 0) {
                const auto* begin = reinterpret_cast<const uint32_t*>(item.data.data());
                int count = int(item.data.size() / sizeof(uint32_t));
                for (int i = 0; i < count; ++i) arr.append(int(begin[i]));
            }
            obj["PIVOTCHANNELDATA"] = arr;
            return obj;
        }

        void fromJson(const QJsonObject& dataObj, ChunkItem& item) const override {
            QJsonArray arr = dataObj.value("PIVOTCHANNELDATA").toArray();
            std::vector<uint32_t> temp(arr.size());
            for (int i = 0; i < arr.size(); ++i) temp[i] = uint32_t(arr[i].toInt());
            item.data.resize(temp.size() * sizeof(uint32_t));
            if (!temp.empty()) std::memcpy(item.data.data(), temp.data(), item.data.size());
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
    static const DamageHeaderSerializer damageHeaderSerializerInstance;
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
    };
    return registry;
}
