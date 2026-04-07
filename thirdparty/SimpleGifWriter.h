#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QString>

namespace OW3D::Gif {

class Writer {
public:
    Writer() = default;
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    ~Writer() {
        QString ignoredError;
        Close(&ignoredError);
    }

    bool Open(const QString& path, int width, int height, QString* outError = nullptr) {
        QString ignoredError;
        Close(&ignoredError);

        if (width <= 0 || height <= 0 || width > 65535 || height > 65535) {
            if (outError) {
                *outError = QStringLiteral("GIF dimensions are invalid.");
            }
            return false;
        }

        const QFileInfo info(path);
        if (!QDir().mkpath(info.path())) {
            if (outError) {
                *outError = QStringLiteral("Failed to create output directory: %1").arg(info.path());
            }
            return false;
        }

        m_file.setFileName(path);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (outError) {
                *outError = QStringLiteral("Failed to open GIF for writing: %1").arg(m_file.errorString());
            }
            return false;
        }

        m_width = width;
        m_height = height;
        m_open = true;

        if (!WriteHeader(outError)) {
            m_file.close();
            m_open = false;
            return false;
        }

        return true;
    }

    bool AddFrame(const QImage& sourceImage, int delayCentiseconds, QString* outError = nullptr) {
        if (!m_open) {
            if (outError) {
                *outError = QStringLiteral("GIF writer is not open.");
            }
            return false;
        }
        if (sourceImage.isNull()) {
            if (outError) {
                *outError = QStringLiteral("Cannot add a null image to the GIF.");
            }
            return false;
        }
        if (sourceImage.width() != m_width || sourceImage.height() != m_height) {
            if (outError) {
                *outError = QStringLiteral("GIF frame size mismatch.");
            }
            return false;
        }

        const int clampedDelay = std::clamp(delayCentiseconds, 1, 65535);
        if (!WriteGraphicsControlExtension(clampedDelay, outError)) {
            return false;
        }
        if (!WriteImageDescriptor(outError)) {
            return false;
        }

        const QImage rgba = sourceImage.convertToFormat(QImage::Format_RGBA8888);
        QByteArray indices;
        indices.resize(m_width * m_height);
        for (int y = 0; y < m_height; ++y) {
            const auto* row = reinterpret_cast<const uint8_t*>(rgba.constScanLine(y));
            auto* dst = reinterpret_cast<uint8_t*>(indices.data() + (y * m_width));
            for (int x = 0; x < m_width; ++x) {
                const uint8_t r = row[(x * 4) + 0];
                const uint8_t g = row[(x * 4) + 1];
                const uint8_t b = row[(x * 4) + 2];
                const uint8_t r3 = static_cast<uint8_t>(r >> 5);
                const uint8_t g3 = static_cast<uint8_t>(g >> 5);
                const uint8_t b2 = static_cast<uint8_t>(b >> 6);
                dst[x] = static_cast<uint8_t>((r3 << 5) | (g3 << 2) | b2);
            }
        }

        if (!WriteByte(8, outError)) {
            return false;
        }

        QByteArray compressed;
        EncodeLzw(indices, 8, compressed);
        return WriteSubBlocks(compressed, outError);
    }

    bool Close(QString* outError = nullptr) {
        if (!m_open) {
            return true;
        }

        const bool wroteTrailer = WriteByte(0x3B, outError);
        m_file.close();
        m_open = false;
        m_width = 0;
        m_height = 0;
        return wroteTrailer;
    }

private:
    struct BitWriter {
        QByteArray data;
        uint32_t bitBuffer = 0;
        int bitCount = 0;

        void WriteCode(int code, int codeSize) {
            bitBuffer |= static_cast<uint32_t>(code) << bitCount;
            bitCount += codeSize;
            while (bitCount >= 8) {
                data.push_back(static_cast<char>(bitBuffer & 0xFFu));
                bitBuffer >>= 8;
                bitCount -= 8;
            }
        }

        void Flush() {
            if (bitCount > 0) {
                data.push_back(static_cast<char>(bitBuffer & 0xFFu));
                bitBuffer = 0;
                bitCount = 0;
            }
        }
    };

    static void EncodeLzw(const QByteArray& indices, int minCodeSize, QByteArray& outData) {
        outData.clear();

        const int clearCode = 1 << minCodeSize;
        const int endCode = clearCode + 1;
        const int firstFreeCode = endCode + 1;

        BitWriter writer;
        writer.WriteCode(clearCode, minCodeSize + 1);

        if (indices.isEmpty()) {
            writer.WriteCode(endCode, minCodeSize + 1);
            writer.Flush();
            outData = std::move(writer.data);
            return;
        }

        std::unordered_map<uint32_t, int> dictionary;
        dictionary.reserve(4096);

        int nextCode = firstFreeCode;
        int codeSize = minCodeSize + 1;
        int prefix = static_cast<unsigned char>(indices.at(0));

        auto resetDictionary = [&]() {
            dictionary.clear();
            nextCode = firstFreeCode;
            codeSize = minCodeSize + 1;
        };

        for (int i = 1; i < indices.size(); ++i) {
            const int suffix = static_cast<unsigned char>(indices.at(i));
            const uint32_t key =
                (static_cast<uint32_t>(prefix) << 8u) | static_cast<uint32_t>(suffix);

            const auto it = dictionary.find(key);
            if (it != dictionary.end()) {
                prefix = it->second;
                continue;
            }

            writer.WriteCode(prefix, codeSize);

            if (nextCode < 4096) {
                dictionary.emplace(key, nextCode++);
                // GIF decoders grow the code width one symbol later than the
                // encoder's dictionary insertion point, so keep the current
                // width until the next free code moves past the current range.
                if (nextCode > (1 << codeSize) && codeSize < 12) {
                    ++codeSize;
                }
            }
            else {
                writer.WriteCode(clearCode, codeSize);
                resetDictionary();
            }

            prefix = suffix;
        }

        writer.WriteCode(prefix, codeSize);
        writer.WriteCode(endCode, codeSize);
        writer.Flush();
        outData = std::move(writer.data);
    }

    static std::array<uint8_t, 256 * 3> BuildPalette() {
        std::array<uint8_t, 256 * 3> palette{};
        for (int i = 0; i < 256; ++i) {
            const uint8_t r = static_cast<uint8_t>(((i >> 5) & 0x7) * 255 / 7);
            const uint8_t g = static_cast<uint8_t>(((i >> 2) & 0x7) * 255 / 7);
            const uint8_t b = static_cast<uint8_t>((i & 0x3) * 255 / 3);
            palette[(i * 3) + 0] = r;
            palette[(i * 3) + 1] = g;
            palette[(i * 3) + 2] = b;
        }
        return palette;
    }

    bool WriteHeader(QString* outError) {
        static const std::array<uint8_t, 6> kSignature = {
            'G', 'I', 'F', '8', '9', 'a'
        };
        static const std::array<uint8_t, 256 * 3> kPalette = BuildPalette();

        if (!WriteBytes(reinterpret_cast<const char*>(kSignature.data()), kSignature.size(), outError)) {
            return false;
        }
        if (!WriteLe16(static_cast<uint16_t>(m_width), outError)) {
            return false;
        }
        if (!WriteLe16(static_cast<uint16_t>(m_height), outError)) {
            return false;
        }

        constexpr uint8_t kPacked =
            0x80u  // global color table present
            | 0x70u // 8-bit color resolution
            | 0x07u; // table size = 256
        if (!WriteByte(kPacked, outError)
            || !WriteByte(0x00, outError) // background color index
            || !WriteByte(0x00, outError)) // aspect ratio
        {
            return false;
        }

        if (!WriteBytes(reinterpret_cast<const char*>(kPalette.data()), kPalette.size(), outError)) {
            return false;
        }

        static const std::array<uint8_t, 19> kLoopExtension = {
            0x21, 0xFF, 0x0B,
            'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0',
            0x03, 0x01, 0x00, 0x00, 0x00
        };
        return WriteBytes(reinterpret_cast<const char*>(kLoopExtension.data()), kLoopExtension.size(), outError);
    }

    bool WriteGraphicsControlExtension(int delayCentiseconds, QString* outError) {
        return WriteByte(0x21, outError)
            && WriteByte(0xF9, outError)
            && WriteByte(0x04, outError)
            && WriteByte(0x00, outError) // no transparency, no disposal override
            && WriteLe16(static_cast<uint16_t>(delayCentiseconds), outError)
            && WriteByte(0x00, outError)
            && WriteByte(0x00, outError);
    }

    bool WriteImageDescriptor(QString* outError) {
        return WriteByte(0x2C, outError)
            && WriteLe16(0, outError)
            && WriteLe16(0, outError)
            && WriteLe16(static_cast<uint16_t>(m_width), outError)
            && WriteLe16(static_cast<uint16_t>(m_height), outError)
            && WriteByte(0x00, outError); // use global palette
    }

    bool WriteSubBlocks(const QByteArray& data, QString* outError) {
        int offset = 0;
        while (offset < data.size()) {
            const int remaining = data.size() - offset;
            const int chunkSize = std::min(255, remaining);
            if (!WriteByte(static_cast<uint8_t>(chunkSize), outError)
                || !WriteBytes(data.constData() + offset, static_cast<std::size_t>(chunkSize), outError))
            {
                return false;
            }
            offset += chunkSize;
        }
        return WriteByte(0x00, outError);
    }

    bool WriteLe16(uint16_t value, QString* outError) {
        const char bytes[2] = {
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8u) & 0xFFu)
        };
        return WriteBytes(bytes, 2, outError);
    }

    bool WriteByte(uint8_t value, QString* outError) {
        const char byte = static_cast<char>(value);
        return WriteBytes(&byte, 1, outError);
    }

    bool WriteBytes(const char* data, std::size_t size, QString* outError) {
        if (!m_open) {
            if (outError) {
                *outError = QStringLiteral("GIF file is not open.");
            }
            return false;
        }
        const qint64 written = m_file.write(data, static_cast<qint64>(size));
        if (written != static_cast<qint64>(size)) {
            if (outError) {
                *outError = QStringLiteral("Failed to write GIF data: %1").arg(m_file.errorString());
            }
            return false;
        }
        return true;
    }

    QFile m_file;
    int m_width = 0;
    int m_height = 0;
    bool m_open = false;
};

} // namespace OW3D::Gif
