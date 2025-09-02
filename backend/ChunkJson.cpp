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