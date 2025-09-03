#include "ChunkJson.h"

#include <QJsonArray>
#include <QByteArray>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cstdint>
#include "ChunkItem.h"
#include "ChunkNames.h"
#include "FormatUtils.h"
#include "W3DStructs.h"

namespace {

    // Convert a contiguous array of structs in a byte vector to a QJsonArray
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

    // Convert a QJsonArray to a byte vector containing an array of structs
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

} // namespace


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
        case 0x0002: { // VERTICES: array of W3dVectorStruct
            dataObj["VERTICES"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) {
                    return QJsonArray{ v.X, v.Y, v.Z };
                }
            );
            break;
        }
        case 0x0003: { // VERTEX_NORMALS
            dataObj["VERTEX_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) {
                    return QJsonArray{ v.X, v.Y, v.Z };
                }
            );
            break;
        }
        case 0x0004: { // SURRENDER_NORMALS
            dataObj["SURRENDER_NORMALS"] = structsToJsonArray<W3dVectorStruct>(
                item.data,
                [](const W3dVectorStruct& v) {
                    return QJsonArray{ v.X, v.Y, v.Z };
                }
            );
            break;
        }
        case 0x0005: { // TEXCOORDS: array of W3dTexCoordStruct
            dataObj["TEXCOORDS"] = structsToJsonArray<W3dTexCoordStruct>(
                item.data,
                [](const W3dTexCoordStruct& t) {
                    return QJsonArray{ t.U, t.V };
                }
            );
            break;
        }
        case 0x0006: { // MATERIALS: array of W3dMaterial1Struct
            dataObj["MATERIALS"] = structsToJsonArray<W3dMaterial1Struct>(
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
        case 0x000C: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["MESH_USER_TEXT"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
            break;
        }
        case 0x000D: {
            if (item.data.size() % sizeof(W3dRGBStruct) == 0) {
                int count = item.data.size() / sizeof(W3dRGBStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* c = reinterpret_cast<const W3dRGBStruct*>(item.data.data()) + i;
                    arr.append(QJsonArray{ int(c->R), int(c->G), int(c->B) });
                }
                dataObj["VERTEX_COLORS"] = arr;
            }
            break;
        }
        case 0x000E: {
            if (item.data.size() % sizeof(W3dVertInfStruct) == 0) {
                int count = item.data.size() / sizeof(W3dVertInfStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* v = reinterpret_cast<const W3dVertInfStruct*>(item.data.data()) + i;
                    QJsonObject vo;
                    vo["BONEIDX"] = QJsonArray{ int(v->BoneIdx[0]), int(v->BoneIdx[1]) };
                    vo["WEIGHT"] = QJsonArray{ int(v->Weight[0]), int(v->Weight[1]) };
                    arr.append(vo);
                }
                dataObj["VERTEX_INFLUENCES"] = arr;
            }
            break;
        }
        case 0x0010: {
            if (item.data.size() >= sizeof(W3dDamageHeaderStruct)) {
                const auto* h = reinterpret_cast<const W3dDamageHeaderStruct*>(item.data.data());
                dataObj["NUMDAMAGEMATERIALS"] = static_cast<int>(h->NumDamageMaterials);
                dataObj["NUMDAMAGEVERTS"] = static_cast<int>(h->NumDamageVerts);
                dataObj["NUMDAMAGECOLORS"] = static_cast<int>(h->NumDamageColors);
                dataObj["DAMAGEINDEX"] = static_cast<int>(h->DamageIndex);
                QJsonArray fu;
                for (int i = 0; i < 4; ++i) fu.append(static_cast<int>(h->FutureUse[i]));
                dataObj["FUTUREUSE"] = fu;
            }
            break;
        }
        case 0x0011: {
            if (item.data.size() % sizeof(W3dMeshDamageVertexStruct) == 0) {
                int count = item.data.size() / sizeof(W3dMeshDamageVertexStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* v = reinterpret_cast<const W3dMeshDamageVertexStruct*>(item.data.data()) + i;
                    QJsonObject vo;
                    vo["VERTEXINDEX"] = static_cast<int>(v->VertexIndex);
                    vo["NEWVERTEX"] = QJsonArray{ v->NewVertex.X, v->NewVertex.Y, v->NewVertex.Z };
                    arr.append(vo);
                }
                dataObj["DAMAGE_VERTICES"] = arr;
            }
            break;
        }
        case 0x0012: {
            if (item.data.size() % sizeof(W3dMeshDamageColorStruct) == 0) {
                int count = item.data.size() / sizeof(W3dMeshDamageColorStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* c = reinterpret_cast<const W3dMeshDamageColorStruct*>(item.data.data()) + i;
                    QJsonObject co;
                    co["VERTEXINDEX"] = static_cast<int>(c->VertexIndex);
                    co["NEWCOLOR"] = QJsonArray{ int(c->NewColor.R), int(c->NewColor.G), int(c->NewColor.B) };
                    arr.append(co);
                }
                dataObj["DAMAGE_COLORS"] = arr;
            }
            break;
        }
        case 0x0014: {
            if (item.data.size() % sizeof(W3dMaterial2Struct) == 0) {
                int count = item.data.size() / sizeof(W3dMaterial2Struct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* m = reinterpret_cast<const W3dMaterial2Struct*>(item.data.data()) + i;
                    QJsonObject mo;
                    mo["MATERIALNAME"] = QString::fromUtf8(m->MaterialName, strnlen(m->MaterialName, 16));
                    mo["PRIMARYNAME"] = QString::fromUtf8(m->PrimaryName, strnlen(m->PrimaryName, 16));
                    mo["SECONDARYNAME"] = QString::fromUtf8(m->SecondaryName, strnlen(m->SecondaryName, 16));
                    mo["RENDERFLAGS"] = static_cast<int>(m->RenderFlags);
                    mo["COLOR"] = QJsonArray{ int(m->Red), int(m->Green), int(m->Blue), int(m->Alpha) };
                    mo["PRIMARYNUMFRAMES"] = static_cast<int>(m->PrimaryNumFrames);
                    mo["SECONDARYNUMFRAMES"] = static_cast<int>(m->SecondaryNumFrames);
                    arr.append(mo);
                }
                dataObj["MATERIALS2"] = arr;
            }
            break;
        }

        case 0x0017: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["MATERIAL3_NAME"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
            break;
        }

        case 0x0018: {
            if (item.data.size() >= sizeof(W3dMaterial3Struct)) {
                const auto* m = reinterpret_cast<const W3dMaterial3Struct*>(item.data.data());
                dataObj["MATERIAL3FLAGS"] = static_cast<int>(m->Material3Flags);
                dataObj["DIFFUSECOLOR"] = QJsonArray{ int(m->DiffuseColor.R), int(m->DiffuseColor.G), int(m->DiffuseColor.B) };
                dataObj["SPECULARCOLOR"] = QJsonArray{ int(m->SpecularColor.R), int(m->SpecularColor.G), int(m->SpecularColor.B) };
                dataObj["EMISSIVECOEFFICIENTS"] = QJsonArray{ int(m->EmissiveCoefficients.R), int(m->EmissiveCoefficients.G), int(m->EmissiveCoefficients.B) };
                dataObj["AMBIENTCOEFFICIENTS"] = QJsonArray{ int(m->AmbientCoefficients.R), int(m->AmbientCoefficients.G), int(m->AmbientCoefficients.B) };
                dataObj["DIFFUSECOEFFICIENTS"] = QJsonArray{ int(m->DiffuseCoefficients.R), int(m->DiffuseCoefficients.G), int(m->DiffuseCoefficients.B) };
                dataObj["SPECULARCOEFFICIENTS"] = QJsonArray{ int(m->SpecularCoefficients.R), int(m->SpecularCoefficients.G), int(m->SpecularCoefficients.B) };
                dataObj["SHININESS"] = m->Shininess;
                dataObj["OPACITY"] = m->Opacity;
                dataObj["TRANSLUCENCY"] = m->Translucency;
                dataObj["FOGCOEFF"] = m->FogCoeff;
            }
            break;
        }

        case 0x001A: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["MAP3_FILENAME"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
            break;
        }

        case 0x001B: {
            if (item.data.size() >= sizeof(W3dMap3Struct)) {
                const auto* m = reinterpret_cast<const W3dMap3Struct*>(item.data.data());
                dataObj["MAPPINGTYPE"] = static_cast<int>(m->MappingType);
                dataObj["FRAMECOUNT"] = static_cast<int>(m->FrameCount);
                dataObj["FRAMERATE"] = static_cast<int>(m->FrameRate);
            }
            break;
        }

        case 0x001F: {
            if (item.data.size() >= sizeof(W3dMeshHeader3Struct)) {
                const auto* h = reinterpret_cast<const W3dMeshHeader3Struct*>(item.data.data());
                dataObj["VERSION"] = QString::fromStdString(FormatUtils::FormatVersion(h->Version));
                dataObj["ATTRIBUTES"] = static_cast<int>(h->Attributes);
                dataObj["MESHNAME"] = QString::fromUtf8(h->MeshName, strnlen(h->MeshName, W3D_NAME_LEN));
                dataObj["CONTAINERNAME"] = QString::fromUtf8(h->ContainerName, strnlen(h->ContainerName, W3D_NAME_LEN));
                dataObj["NUMTRIS"] = static_cast<int>(h->NumTris);
                dataObj["NUMVERTICES"] = static_cast<int>(h->NumVertices);
                dataObj["NUMMATERIALS"] = static_cast<int>(h->NumMaterials);
                dataObj["NUMDAMAGESTAGES"] = static_cast<int>(h->NumDamageStages);
                dataObj["SORTLEVEL"] = static_cast<int>(h->SortLevel);
                dataObj["PRELITVERSION"] = static_cast<int>(h->PrelitVersion);
                QJsonArray fc;
                for (int i = 0; i < 1; ++i) fc.append(static_cast<int>(h->FutureCounts[i]));
                dataObj["FUTURECOUNTS"] = fc;
                dataObj["VERTEXCHANNELS"] = static_cast<int>(h->VertexChannels);
                dataObj["FACECHANNELS"] = static_cast<int>(h->FaceChannels);
                dataObj["MIN"] = QJsonArray{ h->Min.X, h->Min.Y, h->Min.Z };
                dataObj["MAX"] = QJsonArray{ h->Max.X, h->Max.Y, h->Max.Z };
                dataObj["SPHCENTER"] = QJsonArray{ h->SphCenter.X, h->SphCenter.Y, h->SphCenter.Z };
                dataObj["SPHRADIUS"] = h->SphRadius;
            }
            break;
        }

        case 0x0020: {
            if (item.data.size() % sizeof(W3dTriStruct) == 0) {
                int count = item.data.size() / sizeof(W3dTriStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* t = reinterpret_cast<const W3dTriStruct*>(item.data.data()) + i;
                    QJsonObject to;
                    QJsonArray vidx; for (int j = 0; j < 3; ++j) vidx.append(int(t->Vindex[j]));
                    to["VINDEX"] = vidx;
                    to["ATTRIBUTES"] = static_cast<int>(t->Attributes);
                    to["NORMAL"] = QJsonArray{ t->Normal.X, t->Normal.Y, t->Normal.Z };
                    to["DIST"] = t->Dist;
                    arr.append(to);
                }
                dataObj["TRIANGLES"] = arr;
            }
            break;
        }

        case 0x0021: {
            if (item.data.size() % sizeof(uint16_t) == 0) {
                int count = item.data.size() / sizeof(uint16_t);
                QJsonArray arr;
                const auto* m = reinterpret_cast<const uint16_t*>(item.data.data());
                for (int i = 0; i < count; ++i) arr.append(int(m[i]));
                dataObj["PER_TRI_MATERIALS"] = arr;
            }
            break;
        }

        case 0x0022: {
            if (item.data.size() % sizeof(uint32_t) == 0) {
                int count = item.data.size() / sizeof(uint32_t);
                QJsonArray arr;
                const auto* s = reinterpret_cast<const uint32_t*>(item.data.data());
                for (int i = 0; i < count; ++i) arr.append(int(s[i]));
                dataObj["VERTEX_SHADE_INDICES"] = arr;
            }
            break;
        }

        case 0x0028: {
            if (item.data.size() >= sizeof(W3dMaterialInfoStruct)) {
                const auto* mi = reinterpret_cast<const W3dMaterialInfoStruct*>(item.data.data());
                dataObj["PASSCOUNT"] = static_cast<int>(mi->PassCount);
                dataObj["VERTEXMATERIALCOUNT"] = static_cast<int>(mi->VertexMaterialCount);
                dataObj["SHADERCOUNT"] = static_cast<int>(mi->ShaderCount);
                dataObj["TEXTURECOUNT"] = static_cast<int>(mi->TextureCount);
            }
            break;
        }

        case 0x0029: {
            if (item.data.size() % sizeof(W3dShaderStruct) == 0) {
                int count = item.data.size() / sizeof(W3dShaderStruct);
                QJsonArray arr;
                for (int i = 0; i < count; ++i) {
                    const auto* s = reinterpret_cast<const W3dShaderStruct*>(item.data.data()) + i;
                    QJsonObject so;
                    so["DEPTHCOMPARE"] = static_cast<int>(s->DepthCompare);
                    so["DEPTHMASK"] = static_cast<int>(s->DepthMask);
                    so["COLORMASK"] = static_cast<int>(s->ColorMask);
                    so["DESTBLEND"] = static_cast<int>(s->DestBlend);
                    so["FOGFUNC"] = static_cast<int>(s->FogFunc);
                    so["PRIGRADIENT"] = static_cast<int>(s->PriGradient);
                    so["SECGRADIENT"] = static_cast<int>(s->SecGradient);
                    so["SRCBLEND"] = static_cast<int>(s->SrcBlend);
                    so["TEXTURING"] = static_cast<int>(s->Texturing);
                    so["DETAILCOLORFUNC"] = static_cast<int>(s->DetailColorFunc);
                    so["DETAILALPHAFUNC"] = static_cast<int>(s->DetailAlphaFunc);
                    so["SHADERPRESET"] = static_cast<int>(s->ShaderPreset);
                    so["ALPHATEST"] = static_cast<int>(s->AlphaTest);
                    so["POSTDETAILCOLORFUNC"] = static_cast<int>(s->PostDetailColorFunc);
                    so["POSTDETAILALPHAFUNC"] = static_cast<int>(s->PostDetailAlphaFunc);
                    arr.append(so);
                }
                dataObj["SHADERS"] = arr;
            }
            break;
        }

        case 0x002C: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["VERTEX_MATERIAL_NAME"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
            break;
        }

        case 0x002D: {
            if (item.data.size() >= sizeof(W3dVertexMaterialStruct)) {
                const auto* vm = reinterpret_cast<const W3dVertexMaterialStruct*>(item.data.data());
                dataObj["ATTRIBUTES"] = static_cast<int>(vm->Attributes);
                dataObj["AMBIENT"] = QJsonArray{ int(vm->Ambient.R), int(vm->Ambient.G), int(vm->Ambient.B) };
                dataObj["DIFFUSE"] = QJsonArray{ int(vm->Diffuse.R), int(vm->Diffuse.G), int(vm->Diffuse.B) };
                dataObj["SPECULAR"] = QJsonArray{ int(vm->Specular.R), int(vm->Specular.G), int(vm->Specular.B) };
                dataObj["EMISSIVE"] = QJsonArray{ int(vm->Emissive.R), int(vm->Emissive.G), int(vm->Emissive.B) };
                dataObj["SHININESS"] = vm->Shininess;
                dataObj["OPACITY"] = vm->Opacity;
                dataObj["TRANSLUCENCY"] = vm->Translucency;
            }
            break;
        }

        case 0x002E: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["VERTEX_MAPPER_ARGS0"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
            break;
        }

        case 0x002F: {
            const char* text = reinterpret_cast<const char*>(item.data.data());
            dataObj["VERTEX_MAPPER_ARGS1"] = QString::fromUtf8(text, strnlen(text, item.data.size()));
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
        case 0x0002: { // VERTICES
            QJsonArray arr = dataObj.value("VERTICES").toArray();
            item->data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item->length = uint32_t(item->data.size());
            break;
        }
        case 0x0003: { // VERTEX_NORMALS
            QJsonArray arr = dataObj.value("VERTEX_NORMALS").toArray();
            item->data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item->length = uint32_t(item->data.size());
            break;
        }
        case 0x0004: { // SURRENDER_NORMALS
            QJsonArray arr = dataObj.value("SURRENDER_NORMALS").toArray();
            item->data = jsonArrayToStructs<W3dVectorStruct>(arr, [](const QJsonValue& val) {
                W3dVectorStruct v{};
                auto a = val.toArray();
                if (a.size() >= 3) { v.X = a[0].toDouble(); v.Y = a[1].toDouble(); v.Z = a[2].toDouble(); }
                return v;
                });
            item->length = uint32_t(item->data.size());
            break;
        }
        case 0x0005: { // TEXCOORDS
            QJsonArray arr = dataObj.value("TEXCOORDS").toArray();
            item->data = jsonArrayToStructs<W3dTexCoordStruct>(arr, [](const QJsonValue& val) {
                W3dTexCoordStruct t{};
                auto a = val.toArray();
                if (a.size() >= 2) { t.U = a[0].toDouble(); t.V = a[1].toDouble(); }
                return t;
                });
            item->length = uint32_t(item->data.size());
            break;
        }
        case 0x0006: { // MATERIALS
            QJsonArray arr = dataObj.value("MATERIALS").toArray();
            item->data = jsonArrayToStructs<W3dMaterial1Struct>(arr, [](const QJsonValue& val) {
                W3dMaterial1Struct m{};
                auto o = val.toObject();
                auto mn = o.value("MATERIALNAME").toString().toUtf8();
                auto pn = o.value("PRIMARYNAME").toString().toUtf8();
                auto sn = o.value("SECONDARYNAME").toString().toUtf8();
                // MaterialName
                std::memset(m.MaterialName, 0, sizeof m.MaterialName);
                const size_t n0 = (std::min)(
                    static_cast<size_t>(mn.size()),
                    sizeof m.MaterialName
                    );
                if (n0) std::memcpy(m.MaterialName, mn.constData(), n0);

                // PrimaryName
                std::memset(m.PrimaryName, 0, sizeof m.PrimaryName);
                const size_t n1 = (std::min)(
                    static_cast<size_t>(pn.size()),
                    sizeof m.PrimaryName
                    );
                if (n1) std::memcpy(m.PrimaryName, pn.constData(), n1);

                // SecondaryName
                std::memset(m.SecondaryName, 0, sizeof m.SecondaryName);
                const size_t n2 = (std::min)(
                    static_cast<size_t>(sn.size()),
                    sizeof m.SecondaryName
                    );
                if (n2) std::memcpy(m.SecondaryName, sn.constData(), n2);
                m.RenderFlags = o.value("RENDERFLAGS").toInt();
                auto c = o.value("COLOR").toArray();
                if (c.size() >= 3) { m.Red = c[0].toInt(); m.Green = c[1].toInt(); m.Blue = c[2].toInt(); }
                return m;
                });
            item->length = uint32_t(item->data.size());
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
        case 0x000C: {
            QByteArray txt = dataObj.value("MESH_USER_TEXT").toString().toUtf8();
            item->length = txt.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), txt.constData(), txt.size());
            item->data[txt.size()] = 0;
            break;
        }
        case 0x000D: {
            QJsonArray arr = dataObj.value("VERTEX_COLORS").toArray();
            std::vector<W3dRGBStruct> cols(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonArray c = arr[i].toArray();
                if (c.size() >= 3) {
                    cols[i].R = c[0].toInt();
                    cols[i].G = c[1].toInt();
                    cols[i].B = c[2].toInt();
                }
            }
            item->length = cols.size() * sizeof(W3dRGBStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), cols.data(), item->length);
            break;
        }
        case 0x000E: {
            QJsonArray arr = dataObj.value("VERTEX_INFLUENCES").toArray();
            std::vector<W3dVertInfStruct> inf(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject o = arr[i].toObject();
                QJsonArray bi = o.value("BONEIDX").toArray();
                QJsonArray wt = o.value("WEIGHT").toArray();
                for (int j = 0; j < 2 && j < bi.size(); ++j) inf[i].BoneIdx[j] = bi[j].toInt();
                for (int j = 0; j < 2 && j < wt.size(); ++j) inf[i].Weight[j] = wt[j].toInt();
            }
            item->length = inf.size() * sizeof(W3dVertInfStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), inf.data(), item->length);
            break;
        }
        case 0x0010: {
            W3dDamageHeaderStruct h{};
            h.NumDamageMaterials = dataObj.value("NUMDAMAGEMATERIALS").toInt();
            h.NumDamageVerts = dataObj.value("NUMDAMAGEVERTS").toInt();
            h.NumDamageColors = dataObj.value("NUMDAMAGECOLORS").toInt();
            h.DamageIndex = dataObj.value("DAMAGEINDEX").toInt();
            QJsonArray fu = dataObj.value("FUTUREUSE").toArray();
            for (int i = 0; i < 4 && i < fu.size(); ++i) h.FutureUse[i] = fu[i].toInt();
            item->length = sizeof(W3dDamageHeaderStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &h, sizeof(h));
            break;
        }
        case 0x0011: {
            QJsonArray arr = dataObj.value("DAMAGE_VERTICES").toArray();
            std::vector<W3dMeshDamageVertexStruct> verts(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject v = arr[i].toObject();
                verts[i].VertexIndex = v.value("VERTEXINDEX").toInt();
                QJsonArray nv = v.value("NEWVERTEX").toArray();
                if (nv.size() >= 3) {
                    verts[i].NewVertex.X = nv[0].toDouble();
                    verts[i].NewVertex.Y = nv[1].toDouble();
                    verts[i].NewVertex.Z = nv[2].toDouble();
                }
            }
            item->length = verts.size() * sizeof(W3dMeshDamageVertexStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), verts.data(), item->length);
            break;
        }
        case 0x0012: {
            QJsonArray arr = dataObj.value("DAMAGE_COLORS").toArray();
            std::vector<W3dMeshDamageColorStruct> cols(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject c = arr[i].toObject();
                cols[i].VertexIndex = c.value("VERTEXINDEX").toInt();
                QJsonArray nc = c.value("NEWCOLOR").toArray();
                if (nc.size() >= 3) {
                    cols[i].NewColor.R = nc[0].toInt();
                    cols[i].NewColor.G = nc[1].toInt();
                    cols[i].NewColor.B = nc[2].toInt();
                }
            }
            item->length = cols.size() * sizeof(W3dMeshDamageColorStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), cols.data(), item->length);
            break;
        }
        case 0x0014: {
            QJsonArray arr = dataObj.value("MATERIALS2").toArray();
            std::vector<W3dMaterial2Struct> mats(arr.size());
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
                QJsonArray col = m.value("COLOR").toArray();
                if (col.size() >= 4) {
                    mats[i].Red = col[0].toInt();
                    mats[i].Green = col[1].toInt();
                    mats[i].Blue = col[2].toInt();
                    mats[i].Alpha = col[3].toInt();
                }
                mats[i].PrimaryNumFrames = m.value("PRIMARYNUMFRAMES").toInt();
                mats[i].SecondaryNumFrames = m.value("SECONDARYNUMFRAMES").toInt();
            }
            item->length = mats.size() * sizeof(W3dMaterial2Struct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), mats.data(), item->length);
            break;
        }

        case 0x0017: {
            QByteArray name = dataObj.value("MATERIAL3_NAME").toString().toUtf8();
            item->length = name.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), name.constData(), name.size());
            item->data[name.size()] = 0;
            break;
        }

        case 0x0018: {
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
            QJsonArray dcf = dataObj.value("DIFFUSECOEFFICIENTS").toArray();
            if (dcf.size() >= 3) { m.DiffuseCoefficients.R = dcf[0].toInt(); m.DiffuseCoefficients.G = dcf[1].toInt(); m.DiffuseCoefficients.B = dcf[2].toInt(); }
            QJsonArray scf = dataObj.value("SPECULARCOEFFICIENTS").toArray();
            if (scf.size() >= 3) { m.SpecularCoefficients.R = scf[0].toInt(); m.SpecularCoefficients.G = scf[1].toInt(); m.SpecularCoefficients.B = scf[2].toInt(); }
            m.Shininess = dataObj.value("SHININESS").toDouble();
            m.Opacity = dataObj.value("OPACITY").toDouble();
            m.Translucency = dataObj.value("TRANSLUCENCY").toDouble();
            m.FogCoeff = dataObj.value("FOGCOEFF").toDouble();
            item->length = sizeof(W3dMaterial3Struct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &m, sizeof(m));
            break;
        }

        case 0x001A: {
            QByteArray name = dataObj.value("MAP3_FILENAME").toString().toUtf8();
            item->length = name.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), name.constData(), name.size());
            item->data[name.size()] = 0;
            break;
        }

        case 0x001B: {
            W3dMap3Struct m{};
            m.MappingType = dataObj.value("MAPPINGTYPE").toInt();
            m.FrameCount = dataObj.value("FRAMECOUNT").toInt();
            m.FrameRate = static_cast<uint32_t>(dataObj.value("FRAMERATE").toInt());
            item->length = sizeof(W3dMap3Struct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &m, sizeof(m));
            break;
        }

        case 0x001F: {
            W3dMeshHeader3Struct h{};
            QString verStr = dataObj.value("VERSION").toString();
            auto parts = verStr.split('.');
            if (parts.size() == 2) h.Version = (parts[0].toUInt() << 16) | parts[1].toUInt();
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
            h.PrelitVersion = dataObj.value("PRELITVERSION").toInt();
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
            item->length = sizeof(W3dMeshHeader3Struct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &h, sizeof(h));
            break;
        }

        case 0x0020: {
            QJsonArray arr = dataObj.value("TRIANGLES").toArray();
            std::vector<W3dTriStruct> tris(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject t = arr[i].toObject();
                QJsonArray vidx = t.value("VINDEX").toArray();
                for (int j = 0; j < 3 && j < vidx.size(); ++j) tris[i].Vindex[j] = vidx[j].toInt();
                tris[i].Attributes = t.value("ATTRIBUTES").toInt();
                QJsonArray norm = t.value("NORMAL").toArray();
                if (norm.size() >= 3) { tris[i].Normal.X = norm[0].toDouble(); tris[i].Normal.Y = norm[1].toDouble(); tris[i].Normal.Z = norm[2].toDouble(); }
                tris[i].Dist = t.value("DIST").toDouble();
            }
            item->length = tris.size() * sizeof(W3dTriStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), tris.data(), item->length);
            break;
        }

        case 0x0021: {
            QJsonArray arr = dataObj.value("PER_TRI_MATERIALS").toArray();
            std::vector<uint16_t> mats(arr.size());
            for (int i = 0; i < arr.size(); ++i) mats[i] = arr[i].toInt();
            item->length = mats.size() * sizeof(uint16_t);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), mats.data(), item->length);
            break;
        }

        case 0x0022: {
            QJsonArray arr = dataObj.value("VERTEX_SHADE_INDICES").toArray();
            std::vector<uint32_t> indices(arr.size());
            for (int i = 0; i < arr.size(); ++i) indices[i] = arr[i].toInt();
            item->length = indices.size() * sizeof(uint32_t);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), indices.data(), item->length);
            break;
        }

        case 0x0028: {
            W3dMaterialInfoStruct mi{};
            mi.PassCount = dataObj.value("PASSCOUNT").toInt();
            mi.VertexMaterialCount = dataObj.value("VERTEXMATERIALCOUNT").toInt();
            mi.ShaderCount = dataObj.value("SHADERCOUNT").toInt();
            mi.TextureCount = dataObj.value("TEXTURECOUNT").toInt();
            item->length = sizeof(W3dMaterialInfoStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &mi, sizeof(mi));
            break;
        }

        case 0x0029: {
            QJsonArray arr = dataObj.value("SHADERS").toArray();
            std::vector<W3dShaderStruct> shaders(arr.size());
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject s = arr[i].toObject();
                shaders[i].DepthCompare = static_cast<uint8_t>(s.value("DEPTHCOMPARE").toInt());
                shaders[i].DepthMask = static_cast<uint8_t>(s.value("DEPTHMASK").toInt());
                shaders[i].ColorMask = static_cast<uint8_t>(s.value("COLORMASK").toInt());
                shaders[i].DestBlend = static_cast<uint8_t>(s.value("DESTBLEND").toInt());
                shaders[i].FogFunc = static_cast<uint8_t>(s.value("FOGFUNC").toInt());
                shaders[i].PriGradient = static_cast<uint8_t>(s.value("PRIGRADIENT").toInt());
                shaders[i].SecGradient = static_cast<uint8_t>(s.value("SECGRADIENT").toInt());
                shaders[i].SrcBlend = static_cast<uint8_t>(s.value("SRCBLEND").toInt());
                shaders[i].Texturing = static_cast<uint8_t>(s.value("TEXTURING").toInt());
                shaders[i].DetailColorFunc = static_cast<uint8_t>(s.value("DETAILCOLORFUNC").toInt());
                shaders[i].DetailAlphaFunc = static_cast<uint8_t>(s.value("DETAILALPHAFUNC").toInt());
                shaders[i].ShaderPreset = static_cast<uint8_t>(s.value("SHADERPRESET").toInt());
                shaders[i].AlphaTest = static_cast<uint8_t>(s.value("ALPHATEST").toInt());
                shaders[i].PostDetailColorFunc = static_cast<uint8_t>(s.value("POSTDETAILCOLORFUNC").toInt());
                shaders[i].PostDetailAlphaFunc = static_cast<uint8_t>(s.value("POSTDETAILALPHAFUNC").toInt());
            }
            item->length = shaders.size() * sizeof(W3dShaderStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), shaders.data(), item->length);
            break;
        }

        case 0x002C: {
            QByteArray name = dataObj.value("VERTEX_MATERIAL_NAME").toString().toUtf8();
            item->length = name.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), name.constData(), name.size());
            item->data[name.size()] = 0;
            break;
        }

        case 0x002D: {
            W3dVertexMaterialStruct vm{};
            vm.Attributes = dataObj.value("ATTRIBUTES").toInt();
            QJsonArray amb = dataObj.value("AMBIENT").toArray();
            if (amb.size() >= 3) { vm.Ambient.R = static_cast<uint8_t>(amb[0].toInt()); vm.Ambient.G = static_cast<uint8_t>(amb[1].toInt()); vm.Ambient.B = static_cast<uint8_t>(amb[2].toInt()); }
            QJsonArray dif = dataObj.value("DIFFUSE").toArray();
            if (dif.size() >= 3) { vm.Diffuse.R = static_cast<uint8_t>(dif[0].toInt()); vm.Diffuse.G = static_cast<uint8_t>(dif[1].toInt()); vm.Diffuse.B = static_cast<uint8_t>(dif[2].toInt()); }
            QJsonArray spec = dataObj.value("SPECULAR").toArray();
            if (spec.size() >= 3) { vm.Specular.R = static_cast<uint8_t>(spec[0].toInt()); vm.Specular.G = static_cast<uint8_t>(spec[1].toInt()); vm.Specular.B = static_cast<uint8_t>(spec[2].toInt()); }
            QJsonArray emi = dataObj.value("EMISSIVE").toArray();
            if (emi.size() >= 3) { vm.Emissive.R = static_cast<uint8_t>(emi[0].toInt()); vm.Emissive.G = static_cast<uint8_t>(emi[1].toInt()); vm.Emissive.B = static_cast<uint8_t>(emi[2].toInt()); }
            vm.Shininess = dataObj.value("SHININESS").toDouble();
            vm.Opacity = dataObj.value("OPACITY").toDouble();
            vm.Translucency = dataObj.value("TRANSLUCENCY").toDouble();
            item->length = sizeof(W3dVertexMaterialStruct);
            item->data.resize(item->length);
            std::memcpy(item->data.data(), &vm, sizeof(vm));
            break;
        }

        case 0x002E: {
            QByteArray args = dataObj.value("VERTEX_MAPPER_ARGS0").toString().toUtf8();
            item->length = args.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), args.constData(), args.size());
            item->data[args.size()] = 0;
            break;
        }

        case 0x002F: {
            QByteArray args = dataObj.value("VERTEX_MAPPER_ARGS1").toString().toUtf8();
            item->length = args.size() + 1;
            item->data.resize(item->length);
            std::memcpy(item->data.data(), args.constData(), args.size());
            item->data[args.size()] = 0;
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