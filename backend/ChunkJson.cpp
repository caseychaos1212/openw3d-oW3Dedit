#include "ChunkJson.h"

#include <QJsonArray>
#include <QByteArray>
#include <cstring>
#include <algorithm>
#include <vector>

#include "ChunkItem.h"
#include "ChunkNames.h"
#include "FormatUtils.h"
#include "W3DStructs.h"

QJsonObject ChunkJson::toJson(const ChunkItem& item) {
    QJsonObject obj;
    obj["CHUNK_NAME"] = QString::fromStdString(LabelForChunk(item.id, const_cast<ChunkItem*>(&item)));
    obj["SUBCHUNKS"] = item.hasSubChunks;
    obj["CHUNK_ID"] = static_cast<int>(item.id);
    obj["LENGTH"] = static_cast<int>(item.length);

    if (!item.children.empty()) {
        QJsonArray arr;
        for (const auto& c : item.children) {
            arr.append(ChunkJson::toJson(*c));
        }
        obj["CHILDREN"] = arr;
    }
    else if (!item.data.empty()) {
        QJsonObject dataObj;
        switch (item.id) {
        case 0x0001: {
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
            break;
        }
        case 0x0002: {
            if (item.data.size() % sizeof(W3dVectorStruct) == 0) {
                int count = item.data.size() / sizeof(W3dVectorStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* v = reinterpret_cast<const W3dVectorStruct*>(item.data.data()) + i;
                    arr.append(QJsonArray{ v->X, v->Y, v->Z });
                }
                dataObj["VERTICES"] = arr;
            }
            break;
        }
        case 0x0003: {
            if (item.data.size() % sizeof(W3dVectorStruct) == 0) {
                int count = item.data.size() / sizeof(W3dVectorStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* v = reinterpret_cast<const W3dVectorStruct*>(item.data.data()) + i;
                    arr.append(QJsonArray{ v->X, v->Y, v->Z });
                }
                dataObj["VERTEX_NORMALS"] = arr;
            }
            break;
        }
        case 0x0004: {
            if (item.data.size() % sizeof(W3dVectorStruct) == 0) {
                int count = item.data.size() / sizeof(W3dVectorStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* v = reinterpret_cast<const W3dVectorStruct*>(item.data.data()) + i;
                    arr.append(QJsonArray{ v->X, v->Y, v->Z });
                }
                dataObj["SURRENDER_NORMALS"] = arr;
            }
            break;
        }
        case 0x0005: {
            if (item.data.size() % sizeof(W3dTexCoordStruct) == 0) {
                int count = item.data.size() / sizeof(W3dTexCoordStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* t = reinterpret_cast<const W3dTexCoordStruct*>(item.data.data()) + i;
                    arr.append(QJsonArray{ t->U, t->V });
                }
                dataObj["TEXCOORDS"] = arr;
            }
            break;
        }
        case 0x0006: {
            if (item.data.size() % sizeof(W3dMaterial1Struct) == 0) {
                int count = item.data.size() / sizeof(W3dMaterial1Struct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* m = reinterpret_cast<const W3dMaterial1Struct*>(item.data.data()) + i;
                    QJsonObject mo;
                    mo["MATERIALNAME"] = QString::fromUtf8(m->MaterialName, strnlen(m->MaterialName, 16));
                    mo["PRIMARYNAME"] = QString::fromUtf8(m->PrimaryName, strnlen(m->PrimaryName, 16));
                    mo["SECONDARYNAME"] = QString::fromUtf8(m->SecondaryName, strnlen(m->SecondaryName, 16));
                    mo["RENDERFLAGS"] = static_cast<int>(m->RenderFlags);
                    mo["COLOR"] = QJsonArray{ int(m->Red), int(m->Green), int(m->Blue) };
                    arr.append(mo);
                }
                dataObj["MATERIALS"] = arr;
            }
            break;
        }
        case 0x0009: {
            if (item.data.size() % sizeof(W3dSurrenderTriStruct) == 0) {
                int count = item.data.size() / sizeof(W3dSurrenderTriStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* t = reinterpret_cast<const W3dSurrenderTriStruct*>(item.data.data()) + i;
                    QJsonObject to;
                    QJsonArray vidx;
                    for (int j = 0; j < 3; ++j) vidx.append(int(t->VIndex[j]));
                    to["VINDEX"] = vidx;
                    QJsonArray tc;
                    for (int j = 0; j < 3; ++j) tc.append(QJsonArray{ t->TexCoord[j].U, t->TexCoord[j].V });
                    to["TEXCOORD"] = tc;
                    to["MATERIALIDX"] = static_cast<int>(t->MaterialIDx);
                    to["NORMAL"] = QJsonArray{ t->Normal.X, t->Normal.Y, t->Normal.Z };
                    to["ATTRIBUTES"] = static_cast<int>(t->Attributes);
                    QJsonArray gour;
                    for (int j = 0; j < 3; ++j) gour.append(QJsonArray{ int(t->Gouraud[j].R), int(t->Gouraud[j].G), int(t->Gouraud[j].B) });
                    to["GOURAUD"] = gour;
                    arr.append(to);
                }
                dataObj["SURRENDER_TRIANGLES"] = arr;
            }
            break;
        }


        
        
        
        case 0x0101: {
            if (item.data.size() >= sizeof(W3dHierarchyStruct)) {
                const auto* h = reinterpret_cast<const W3dHierarchyStruct*>(item.data.data());
                dataObj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                dataObj["NAME"] = QString::fromUtf8(h->Name, strnlen(h->Name, W3D_NAME_LEN));
                dataObj["NUMPIVOTS"] = static_cast<int>(h->NumPivots);
                QJsonArray center;
                center.append(h->Center.X);
                center.append(h->Center.Y);
                center.append(h->Center.Z);
                dataObj["Center"] = center;
            }
            break;
        }
        case 0x0102: {
            if (item.data.size() % sizeof(W3dPivotStruct) == 0) {
                int count = item.data.size() / sizeof(W3dPivotStruct);
                QJsonArray pivs;
                for (int i = 0; i < count; ++i) {
                    const auto* p = reinterpret_cast<const W3dPivotStruct*>(item.data.data()) + i;
                    QJsonObject pj;
                    pj["NAME"] = QString::fromUtf8(p->Name, strnlen(p->Name, W3D_NAME_LEN));
                    pj["ID"] = i;
                    pj["PARENTIDX"] = p->ParentIdx == 0xFFFFFFFFu ? -1 : static_cast<int>(p->ParentIdx);
                    QJsonArray t{ p->Translation.X, p->Translation.Y, p->Translation.Z };
                    QJsonArray e{ p->EulerAngles.X, p->EulerAngles.Y, p->EulerAngles.Z };
                    QJsonArray r{ p->Rotation.Q[0], p->Rotation.Q[1], p->Rotation.Q[2], p->Rotation.Q[3] };
                    pj["TRANSLATION"] = t;
                    pj["EULERANGLES"] = e;
                    pj["ROTATION"] = r;
                    pivs.append(pj);
                }
                dataObj["PIVOTS"] = pivs;
            }
            break;
        }
        case 0x0103: {
            if (item.data.size() % sizeof(W3dPivotFixupStruct) == 0) {
                int count = item.data.size() / sizeof(W3dPivotFixupStruct);
                QJsonArray fixes;
                for (int i = 0; i < count; ++i) {
                    const auto* pf = reinterpret_cast<const W3dPivotFixupStruct*>(item.data.data()) + i;
                    QJsonObject fo;
                    fo["ID"] = i;
                    QJsonArray mat;
                    for (int r = 0; r < 4; ++r) {
                        QJsonArray row{ pf->TM[r][0], pf->TM[r][1], pf->TM[r][2] };
                        mat.append(row);
                    }
                    fo["FIXUP"] = mat;
                    fixes.append(fo);
                }
                dataObj["PIVOT_FIXUPS"] = fixes;
            }
            break;
        }
        default: {
            dataObj["RAW"] = QString(QByteArray(reinterpret_cast<const char*>(item.data.data()),
                static_cast<int>(item.data.size())).toBase64());
            break;
        }
        }
        obj["DATA"] = dataObj;
    }

    return obj;
}

std::shared_ptr<ChunkItem> ChunkJson::fromJson(const QJsonObject& obj, ChunkItem* parent) {
    auto item = std::make_shared<ChunkItem>();
    item->id = obj["CHUNK_ID"].toInt();
    item->length = obj["LENGTH"].toInt();
    item->hasSubChunks = obj["SUBCHUNKS"].toBool();
    item->typeName = obj.value("CHUNK_NAME").toString().toStdString();
    item->parent = parent;

    if (obj.contains("CHILDREN")) {
        QJsonArray arr = obj.value("CHILDREN").toArray();
        for (const auto& v : arr) {
            if (v.isObject()) {
                auto child = ChunkJson::fromJson(v.toObject(), item.get());
                item->children.push_back(child);
            }
        }
    }
    else if (obj.contains("DATA")) {
        QJsonObject dataObj = obj.value("DATA").toObject();
        switch (item->id) {
        case 0x0001: {
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
            item->length = sizeof(W3dMeshHeader1);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &h, sizeof(h));
            break;
        }
        case 0x0002: {
            QJsonArray arr = dataObj.value("VERTICES").toArray();
            std::vector<W3dVectorStruct> verts(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray v = arr[i].toArray();
                if (v.size() >= 3) {
                    verts[i].X = v[0].toDouble();
                    verts[i].Y = v[1].toDouble();
                    verts[i].Z = v[2].toDouble();
                }
            }
            item->length = verts.size() * sizeof(W3dVectorStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), verts.data(), item->length);
            break;
        }
        case 0x0003: {
            QJsonArray arr = dataObj.value("VERTEX_NORMALS").toArray();
            std::vector<W3dVectorStruct> norms(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray v = arr[i].toArray();
                if (v.size() >= 3) {
                    norms[i].X = v[0].toDouble();
                    norms[i].Y = v[1].toDouble();
                    norms[i].Z = v[2].toDouble();
                }
            }
            item->length = norms.size() * sizeof(W3dVectorStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), norms.data(), item->length);
            break;
        }
        case 0x0004: {
            QJsonArray arr = dataObj.value("SURRENDER_NORMALS").toArray();
            std::vector<W3dVectorStruct> norms(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray v = arr[i].toArray();
                if (v.size() >= 3) {
                    norms[i].X = v[0].toDouble();
                    norms[i].Y = v[1].toDouble();
                    norms[i].Z = v[2].toDouble();
                }
            }
            item->length = norms.size() * sizeof(W3dVectorStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), norms.data(), item->length);
            break;
        }
        case 0x0005: {
            QJsonArray arr = dataObj.value("TEXCOORDS").toArray();
            std::vector<W3dTexCoordStruct> tex(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray t = arr[i].toArray();
                if (t.size() >= 2) {
                    tex[i].U = t[0].toDouble();
                    tex[i].V = t[1].toDouble();
                }
            }
            item->length = tex.size() * sizeof(W3dTexCoordStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), tex.data(), item->length);
            break;
        }
        case 0x0006: {
            QJsonArray arr = dataObj.value("MATERIALS").toArray();
            std::vector<W3dMaterial1Struct> mats(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject m = arr[i].toObject();
                QByteArray mn = m.value("MATERIALNAME").toString().toUtf8();
                std::memset(mats[i].MaterialName, 0, 16);
                std::memcpy(mats[i].MaterialName, mn.constData(), std::min<int>(mn.size(), 16));
                QByteArray pn = m.value("PRIMARYNAME").toString().toUtf8();
                std::memset(mats[i].PrimaryName, 0, 16);
                std::memcpy(mats[i].PrimaryName, pn.constData(), std::min<int>(pn.size(), 16));
                QByteArray sn = m.value("SECONDARYNAME").toString().toUtf8();
                std::memset(mats[i].SecondaryName, 0, 16);
                std::memcpy(mats[i].SecondaryName, sn.constData(), std::min<int>(sn.size(), 16));
                mats[i].RenderFlags = m.value("RENDERFLAGS").toInt();
                QJsonArray c = m.value("COLOR").toArray();
                if (c.size() >= 3) {
                    mats[i].Red = c[0].toInt();
                    mats[i].Green = c[1].toInt();
                    mats[i].Blue = c[2].toInt();
                }
            }
            item->length = mats.size() * sizeof(W3dMaterial1Struct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), mats.data(), item->length);
            break;
        }
        case 0x0009: {
            QJsonArray arr = dataObj.value("SURRENDER_TRIANGLES").toArray();
            std::vector<W3dSurrenderTriStruct> tris(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject t = arr[i].toObject();
                QJsonArray v = t.value("VINDEX").toArray();
                for (int j = 0; j < 3 && j < v.size(); ++j) tris[i].VIndex[j] = v[j].toInt();
                QJsonArray tc = t.value("TEXCOORD").toArray();
                for (int j = 0; j < 3 && j < tc.size(); ++j) {
                    QJsonArray vv = tc[j].toArray();
                    if (vv.size() >= 2) {
                        tris[i].TexCoord[j].U = vv[0].toDouble();
                        tris[i].TexCoord[j].V = vv[1].toDouble();
                    }
                }
                tris[i].MaterialIDx = t.value("MATERIALIDX").toInt();
                QJsonArray n = t.value("NORMAL").toArray();
                if (n.size() >= 3) {
                    tris[i].Normal.X = n[0].toDouble();
                    tris[i].Normal.Y = n[1].toDouble();
                    tris[i].Normal.Z = n[2].toDouble();
                }
                tris[i].Attributes = t.value("ATTRIBUTES").toInt();
                QJsonArray gour = t.value("GOURAUD").toArray();
                for (int j = 0; j < 3 && j < gour.size(); ++j) {
                    QJsonArray gc = gour[j].toArray();
                    if (gc.size() >= 3) {
                        tris[i].Gouraud[j].R = gc[0].toInt();
                        tris[i].Gouraud[j].G = gc[1].toInt();
                        tris[i].Gouraud[j].B = gc[2].toInt();
                    }
                }
            }
            item->length = tris.size() * sizeof(W3dSurrenderTriStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), tris.data(), item->length);
            break;
        }
        case 0x0101: {
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
            QJsonArray center = dataObj.value("Center").toArray();
            if (center.size() >= 3) {
                h.Center.X = center[0].toDouble();
                h.Center.Y = center[1].toDouble();
                h.Center.Z = center[2].toDouble();
            }
            item->length = sizeof(W3dHierarchyStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &h, sizeof(h));
            break;
        }
        case 0x0102: {
            QJsonArray arr = dataObj.value("PIVOTS").toArray();
            std::vector<W3dPivotStruct> piv(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                const QJsonObject p = arr[i].toObject();
                QByteArray name = p.value("NAME").toString().toUtf8();
                std::memset(piv[i].Name, 0, W3D_NAME_LEN);
                std::memcpy(piv[i].Name, name.constData(), std::min<int>(name.size(), W3D_NAME_LEN));
                int parentIdx = p.value("PARENTIDX").toInt();
                if (parentIdx < 0)
                    piv[i].ParentIdx = 0xFFFFFFFFu;
                else
                    piv[i].ParentIdx = static_cast<uint32_t>(parentIdx);

                QJsonArray t = p.value("TRANSLATION").toArray();
                if (t.size() >= 3) {
                    piv[i].Translation.X = t[0].toDouble();
                    piv[i].Translation.Y = t[1].toDouble();
                    piv[i].Translation.Z = t[2].toDouble();
                }

                QJsonArray e = p.value("EULERANGLES").toArray();
                if (e.size() >= 3) {
                    piv[i].EulerAngles.X = e[0].toDouble();
                    piv[i].EulerAngles.Y = e[1].toDouble();
                    piv[i].EulerAngles.Z = e[2].toDouble();
                }

                QJsonArray r = p.value("ROTATION").toArray();
                if (r.size() >= 4) {
                    piv[i].Rotation.Q[0] = r[0].toDouble();
                    piv[i].Rotation.Q[1] = r[1].toDouble();
                    piv[i].Rotation.Q[2] = r[2].toDouble();
                    piv[i].Rotation.Q[3] = r[3].toDouble();
                }
            }
            item->length = piv.size() * sizeof(W3dPivotStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), piv.data(), item->length);
            break;
        }
        case 0x0103: {
            QJsonArray arr = dataObj.value("PIVOT_FIXUPS").toArray();
            std::vector<W3dPivotFixupStruct> fix(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray mat = arr[i].toObject().value("FIXUP").toArray();
                for (int r = 0; r < 4 && r < mat.size(); ++r) {
                    QJsonArray row = mat[r].toArray();
                    for (int c = 0; c < 3 && c < row.size(); ++c) {
                        fix[i].TM[r][c] = row[c].toDouble();
                    }
                }
            }
            item->length = fix.size() * sizeof(W3dPivotFixupStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), fix.data(), item->length);
            break;
        }
        default: {
            if (dataObj.contains("RAW")) {
                QByteArray ba = QByteArray::fromBase64(dataObj.value("RAW").toString().toUtf8());
                item->data.assign(ba.begin(), ba.end());
            }
            break;
        }
        }
    }

    return item;
}