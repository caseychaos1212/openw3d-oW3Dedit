#include "D3D11RenderBackend.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <d3dcompiler.h>
#include <windows.h>

#include <QFile>
#include <QFileInfo>
#include <QImage>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace OW3D::Render {
namespace {

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(static_cast<uint8_t>(a))
        | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 8)
        | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 16)
        | (static_cast<uint32_t>(static_cast<uint8_t>(d)) << 24);
}

#pragma pack(push, 1)
struct DdsPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DdsHeader {
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DdsPixelFormat ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};
#pragma pack(pop)

uint32_t HashString32(const std::string& text) {
    uint32_t h = 2166136261u;
    for (const unsigned char c : text) {
        h ^= static_cast<uint32_t>(c);
        h *= 16777619u;
    }
    return h;
}

Vec3 CameraForwardFromYawPitch(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {
        std::cos(yaw) * cp,
        std::sin(yaw) * cp,
        std::sin(pitch)
    };
}

float ClampPitch(float pitch) {
    const float limit = 1.55334303f; // ~89 degrees
    return std::clamp(pitch, -limit, limit);
}

Vec3 TransformDirection(const Mat4& m, const Vec3& v) {
    return {
        v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8],
        v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9],
        v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10]
    };
}

Mat4 TTSkinIdentity() {
    return Mat4::Identity();
}

Mat4 TTSkinQuaternionToMatrix(float x, float y, float z, float w) {
    Mat4 out = Mat4::Identity();
    out.m[0] = 1.0f - 2.0f * (y * y + z * z);
    out.m[1] = 2.0f * (x * y - z * w);
    out.m[2] = 2.0f * (z * x + y * w);

    out.m[4] = 2.0f * (x * y + z * w);
    out.m[5] = 1.0f - 2.0f * (z * z + x * x);
    out.m[6] = 2.0f * (y * z - x * w);

    out.m[8] = 2.0f * (z * x - y * w);
    out.m[9] = 2.0f * (y * z + x * w);
    out.m[10] = 1.0f - 2.0f * (y * y + x * x);
    return out;
}

Mat4 TTSkinTransformFromTranslationRotation(const Vec3& translation, const Vec4& rotation) {
    Mat4 out = TTSkinQuaternionToMatrix(rotation.x, rotation.y, rotation.z, rotation.w);
    out.m[3] = translation.x;
    out.m[7] = translation.y;
    out.m[11] = translation.z;
    return out;
}

Mat4 TTSkinMultiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            out.m[r * 4 + c] =
                a.m[r * 4 + 0] * b.m[0 * 4 + c] +
                a.m[r * 4 + 1] * b.m[1 * 4 + c] +
                a.m[r * 4 + 2] * b.m[2 * 4 + c] +
                a.m[r * 4 + 3] * b.m[3 * 4 + c];
        }
    }
    return out;
}

Vec3 TTSkinTransformPoint(const Mat4& m, const Vec3& p) {
    return {
        m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
        m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
        m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11]
    };
}

Vec3 TTSkinTransformDirection(const Mat4& m, const Vec3& v) {
    return {
        m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z,
        m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z,
        m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z
    };
}

float DotQuat(const Vec4& a, const Vec4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

Vec4 NormalizeQuat(Vec4 q) {
    const float len = std::sqrt(DotQuat(q, q));
    if (len <= 1.0e-6f) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    const float invLen = 1.0f / len;
    q.x *= invLen;
    q.y *= invLen;
    q.z *= invLen;
    q.w *= invLen;
    return q;
}

Vec4 SlerpQuat(Vec4 a, Vec4 b, float t) {
    a = NormalizeQuat(a);
    b = NormalizeQuat(b);

    float cosTheta = DotQuat(a, b);
    if (cosTheta < 0.0f) {
        cosTheta = -cosTheta;
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }

    if (cosTheta > 0.9995f) {
        Vec4 out{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
        return NormalizeQuat(out);
    }

    const float theta = std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
    const float sinTheta = std::sin(theta);
    if (sinTheta <= 1.0e-6f) {
        return a;
    }

    const float weightA = std::sin((1.0f - t) * theta) / sinTheta;
    const float weightB = std::sin(t * theta) / sinTheta;
    return {
        a.x * weightA + b.x * weightB,
        a.y * weightA + b.y * weightB,
        a.z * weightA + b.z * weightB,
        a.w * weightA + b.w * weightB
    };
}

float SampleFloatTrack(const std::vector<RenderFloatKeyframe>& track, float frame) {
    if (track.empty()) {
        return 0.0f;
    }
    if (track.size() == 1 || frame <= track.front().frame) {
        return track.front().value;
    }

    for (std::size_t i = 0; i + 1u < track.size(); ++i) {
        const auto& current = track[i];
        const auto& next = track[i + 1u];
        if (frame > next.frame) {
            continue;
        }
        if (current.hold || next.frame <= current.frame) {
            return current.value;
        }
        const float t = std::clamp(
            (frame - current.frame) / std::max(1.0e-6f, next.frame - current.frame),
            0.0f,
            1.0f);
        return current.value + (next.value - current.value) * t;
    }

    return track.back().value;
}

Vec4 SampleQuatTrack(const std::vector<RenderQuatKeyframe>& track, float frame) {
    if (track.empty()) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    if (track.size() == 1 || frame <= track.front().frame) {
        return NormalizeQuat(track.front().value);
    }

    for (std::size_t i = 0; i + 1u < track.size(); ++i) {
        const auto& current = track[i];
        const auto& next = track[i + 1u];
        if (frame > next.frame) {
            continue;
        }
        if (current.hold || next.frame <= current.frame) {
            return NormalizeQuat(current.value);
        }
        const float t = std::clamp(
            (frame - current.frame) / std::max(1.0e-6f, next.frame - current.frame),
            0.0f,
            1.0f);
        return NormalizeQuat(SlerpQuat(current.value, next.value, t));
    }

    return NormalizeQuat(track.back().value);
}

float WrapAnimationFrame(const RenderAnimationClip& clip, float timeSeconds) {
    if (clip.numFrames == 0u || clip.frameRate <= 0.0f) {
        return 0.0f;
    }

    const float totalFrames = static_cast<float>(clip.numFrames);
    if (totalFrames <= 1.0f) {
        return 0.0f;
    }

    float frame = std::fmod(timeSeconds * clip.frameRate, totalFrames);
    if (frame < 0.0f) {
        frame += totalFrames;
    }
    return frame;
}

const RenderAnimationClip* ResolveActiveClipForHierarchy(
    const RenderScene& scene,
    const RenderHierarchy& hierarchy,
    const AnimationPlaybackState& playback,
    float timeSeconds,
    float& outAnimationFrame)
{
    outAnimationFrame = 0.0f;
    if (playback.activeAnimationIndex < 0
        || playback.activeAnimationIndex >= static_cast<int>(scene.animations.size())) {
        return nullptr;
    }
    if (std::find(
        hierarchy.compatibleAnimationIndices.begin(),
        hierarchy.compatibleAnimationIndices.end(),
        playback.activeAnimationIndex) == hierarchy.compatibleAnimationIndices.end()) {
        return nullptr;
    }

    const RenderAnimationClip& clip = scene.animations[static_cast<std::size_t>(playback.activeAnimationIndex)];
    if (!clip.supportedForPlayback) {
        return nullptr;
    }

    if (clip.numFrames == 0u || clip.frameRate <= 0.0f) {
        outAnimationFrame = 0.0f;
        return &clip;
    }

    const float rawFrame = timeSeconds * clip.frameRate;
    if (playback.loop) {
        outAnimationFrame = WrapAnimationFrame(clip, timeSeconds);
    }
    else {
        outAnimationFrame = std::clamp(
            rawFrame,
            0.0f,
            std::max(0.0f, static_cast<float>(clip.numFrames) - 1.0f));
    }

    return &clip;
}

bool BuildSrvFromDdsData(
    ID3D11Device* device,
    const QByteArray& bytes,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
{
    outSrv.Reset();
    if (!device) {
        return false;
    }
    if (bytes.size() < static_cast<int>(4 + sizeof(DdsHeader))) {
        return false;
    }
    if (std::memcmp(bytes.constData(), "DDS ", 4) != 0) {
        return false;
    }

    DdsHeader header{};
    std::memcpy(&header, bytes.constData() + 4, sizeof(header));
    if (header.size != 124 || header.ddspf.size != 32) {
        return false;
    }

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t blockSize = 0;
    switch (header.ddspf.fourCC) {
    case MakeFourCC('D', 'X', 'T', '1'):
        format = DXGI_FORMAT_BC1_UNORM;
        blockSize = 8;
        break;
    case MakeFourCC('D', 'X', 'T', '2'):
    case MakeFourCC('D', 'X', 'T', '3'):
        format = DXGI_FORMAT_BC2_UNORM;
        blockSize = 16;
        break;
    case MakeFourCC('D', 'X', 'T', '4'):
    case MakeFourCC('D', 'X', 'T', '5'):
        format = DXGI_FORMAT_BC3_UNORM;
        blockSize = 16;
        break;
    default:
        return false;
    }

    const uint32_t width = std::max<uint32_t>(1, header.width);
    const uint32_t height = std::max<uint32_t>(1, header.height);
    const uint32_t requestedMipCount = std::max<uint32_t>(1, header.mipMapCount);

    const uint8_t* cursor =
        reinterpret_cast<const uint8_t*>(bytes.constData() + 4 + sizeof(DdsHeader));
    const uint8_t* end =
        reinterpret_cast<const uint8_t*>(bytes.constData() + bytes.size());

    std::vector<D3D11_SUBRESOURCE_DATA> subresources;
    subresources.reserve(requestedMipCount);
    for (uint32_t level = 0; level < requestedMipCount; ++level) {
        const uint32_t levelWidth = std::max<uint32_t>(1, width >> level);
        const uint32_t levelHeight = std::max<uint32_t>(1, height >> level);
        const uint32_t rowBytes = std::max<uint32_t>(1, (levelWidth + 3) / 4) * blockSize;
        const uint32_t rowCount = std::max<uint32_t>(1, (levelHeight + 3) / 4);
        const uint64_t sliceBytes = static_cast<uint64_t>(rowBytes) * rowCount;
        const std::size_t remaining = static_cast<std::size_t>(end - cursor);
        if (sliceBytes > remaining) {
            break;
        }

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = cursor;
        init.SysMemPitch = rowBytes;
        init.SysMemSlicePitch = static_cast<UINT>(sliceBytes);
        subresources.push_back(init);

        cursor += static_cast<std::size_t>(sliceBytes);
    }

    if (subresources.empty()) {
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = static_cast<UINT>(subresources.size());
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    HRESULT hr = device->CreateTexture2D(&texDesc, subresources.data(), texture2d.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
    hr = device->CreateShaderResourceView(texture2d.Get(), &srvDesc, outSrv.GetAddressOf());
    return SUCCEEDED(hr);
}

bool BuildSrvFromImagePath(
    ID3D11Device* device,
    const std::string& path,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
{
    outSrv.Reset();
    if (!device) {
        return false;
    }

    const QImage image(QString::fromStdString(path));
    if (image.isNull()) {
        return false;
    }
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull() || rgba.width() <= 0 || rgba.height() <= 0) {
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = static_cast<UINT>(rgba.width());
    texDesc.Height = static_cast<UINT>(rgba.height());
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rgba.constBits();
    init.SysMemPitch = static_cast<UINT>(rgba.bytesPerLine());

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    HRESULT hr = device->CreateTexture2D(&texDesc, &init, texture2d.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = device->CreateShaderResourceView(texture2d.Get(), nullptr, outSrv.GetAddressOf());
    return SUCCEEDED(hr);
}

bool BuildSrvFromFilePath(
    ID3D11Device* device,
    const std::string& path,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
{
    outSrv.Reset();
    if (!device || path.empty()) {
        return false;
    }

    const QString qPath = QString::fromStdString(path);
    QFile file(qPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();
    if (bytes.isEmpty()) {
        return false;
    }

    const QString ext = QFileInfo(qPath).suffix().toLower();
    if (ext == QStringLiteral("dds")
        || (bytes.size() >= 4 && std::memcmp(bytes.constData(), "DDS ", 4) == 0)) {
        if (BuildSrvFromDdsData(device, bytes, outSrv)) {
            return true;
        }
    }

    return BuildSrvFromImagePath(device, path, outSrv);
}

} // namespace

D3D11RenderBackend::D3D11RenderBackend() = default;

D3D11RenderBackend::~D3D11RenderBackend() {
    Shutdown();
}

bool D3D11RenderBackend::Initialize(void* nativeWindow, uint32_t width, uint32_t height) {
    if (m_initialized) {
        return true;
    }

    if (!CreateDeviceAndSwapchain(nativeWindow, width, height)) {
        return false;
    }

    if (!CreateRenderTargets(width, height)) {
        return false;
    }

    if (!CreateShaders()) {
        return false;
    }

    if (!CreateDefaultTexture()) {
        return false;
    }

    if (!CreateConstantBuffers()) {
        return false;
    }

    m_viewWidth = std::max<uint32_t>(1, width);
    m_viewHeight = std::max<uint32_t>(1, height);
    m_initialized = true;
    return true;
}

void D3D11RenderBackend::Shutdown() {
    m_gpuMeshes.clear();
    m_gpuTextures.clear();
    m_scene = {};
    m_lastFrameStats = {};
    m_selectedInstance.reset();
    m_transformOverrides.clear();
    m_hiddenInstances.clear();

    m_defaultTexture = {};
    m_objectCBuffer.Reset();
    m_frameCBuffer.Reset();
    m_blendOpaque.Reset();
    m_depthState.Reset();
    m_rsCullNone.Reset();
    m_rsCullBack.Reset();
    m_sampler.Reset();
    m_inputLayout.Reset();
    m_ps.Reset();
    m_vs.Reset();

    ReleaseRenderTargets();

    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();

    m_initialized = false;
}

bool D3D11RenderBackend::UploadScene(const RenderScene& scene) {
    if (!m_initialized || !m_device || !m_context) {
        return false;
    }

    m_scene = scene;
    m_gpuMeshes.clear();
    m_gpuTextures.clear();

    m_gpuMeshes.reserve(scene.meshes.size());
    for (const auto& mesh : scene.meshes) {
        GpuMesh gpuMesh{};
        if (!BuildGpuMesh(mesh, gpuMesh)) {
            return false;
        }
        m_gpuMeshes.push_back(std::move(gpuMesh));
    }

    m_gpuTextures.reserve(scene.textures.size());
    for (const auto& texture : scene.textures) {
        GpuTexture gpuTexture{};
        if (!BuildGpuTexture(texture, gpuTexture)) {
            return false;
        }
        m_gpuTextures.push_back(std::move(gpuTexture));
    }

    return true;
}

void D3D11RenderBackend::Resize(uint32_t width, uint32_t height) {
    if (!m_initialized || !m_swapChain) {
        return;
    }

    width = std::max<uint32_t>(1, width);
    height = std::max<uint32_t>(1, height);

    if (width == m_viewWidth && height == m_viewHeight) {
        return;
    }

    ReleaseRenderTargets();
    (void)m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    (void)CreateRenderTargets(width, height);

    m_viewWidth = width;
    m_viewHeight = height;
}

void D3D11RenderBackend::SetCamera(const CameraState& camera) {
    m_camera = camera;
    m_camera.pitch = ClampPitch(m_camera.pitch);
    m_camera.distance = std::max(0.01f, m_camera.distance);
    m_camera.nearPlane = std::max(0.01f, m_camera.nearPlane);
    m_camera.farPlane = std::max(m_camera.nearPlane + 1.0f, m_camera.farPlane);
    m_camera.fovDeg = std::clamp(m_camera.fovDeg, 15.0f, 130.0f);
}

void D3D11RenderBackend::SetRenderSettings(const RenderSettings& settings) {
    m_settings = settings;
}

void D3D11RenderBackend::SetAnimationPlayback(const AnimationPlaybackState& playback) {
    m_animationPlayback = playback;
    m_animationPlayback.activeAnimationIndex = std::clamp(
        m_animationPlayback.activeAnimationIndex,
        -1,
        static_cast<int>(m_scene.animations.size()) - 1);
    m_animationPlayback.timeSeconds = std::max(0.0f, m_animationPlayback.timeSeconds);
    m_animationPlayback.speed = std::max(0.01f, m_animationPlayback.speed);
}

void D3D11RenderBackend::SetSelectedInstance(const std::optional<RenderInstanceKey>& selected) {
    m_selectedInstance = selected;
}

void D3D11RenderBackend::SetTransformOverrides(
    const std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash>& overrides)
{
    m_transformOverrides = overrides;
}

void D3D11RenderBackend::SetHiddenInstances(
    const std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash>& hidden)
{
    m_hiddenInstances = hidden;
}

void D3D11RenderBackend::RenderFrame(const std::function<void()>& overlayCallback) {
    if (!m_initialized || !m_context || !m_rtv || !m_dsv || !m_swapChain) {
        return;
    }

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_viewWidth);
    viewport.Height = static_cast<float>(m_viewHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    const float clearColor[4] = { 0.08f, 0.09f, 0.12f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), clearColor);
    m_context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), m_dsv.Get());
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs.Get(), nullptr, 0);
    m_context->PSSetShader(m_ps.Get(), nullptr, 0);
    m_context->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    m_context->OMSetDepthStencilState(m_depthState.Get(), 0);
    const float blendFactors[4] = { 0, 0, 0, 0 };
    m_context->OMSetBlendState(m_blendOpaque.Get(), blendFactors, 0xFFFFFFFFu);

    const Vec3 cameraForward = CameraForwardFromYawPitch(m_camera.yaw, m_camera.pitch);
    const Vec3 cameraPos = m_camera.target - cameraForward * m_camera.distance;

    const Mat4 view = LookAtLH(cameraPos, m_camera.target, { 0.0f, 0.0f, 1.0f });
    const float aspect = static_cast<float>(m_viewWidth) / static_cast<float>(std::max<uint32_t>(1, m_viewHeight));
    const Mat4 projection = PerspectiveFovLH(DegToRad(m_camera.fovDeg), aspect, m_camera.nearPlane, m_camera.farPlane);

    CBufferFrame frameData{};
    frameData.viewProj = Multiply(view, projection);
    frameData.cameraWorldPos = { cameraPos.x, cameraPos.y, cameraPos.z, 1.0f };
    frameData.ambient = { m_scene.ambientLight.x, m_scene.ambientLight.y, m_scene.ambientLight.z, 1.0f };
    frameData.directionalLightDir = {
        m_scene.directionalLightDir.x,
        m_scene.directionalLightDir.y,
        m_scene.directionalLightDir.z,
        0.0f
    };
    frameData.directionalLightColor = {
        m_scene.directionalLightColor.x,
        m_scene.directionalLightColor.y,
        m_scene.directionalLightColor.z,
        1.0f
    };
    frameData.fogColor = {
        m_scene.fog.color.x,
        m_scene.fog.color.y,
        m_scene.fog.color.z,
        1.0f
    };
    frameData.fogParams = {
        (m_settings.enableFog && m_scene.fog.enabled) ? 1.0f : 0.0f,
        m_scene.fog.nearDistance,
        m_scene.fog.farDistance,
        0.0f
    };

    m_context->UpdateSubresource(m_frameCBuffer.Get(), 0, nullptr, &frameData, 0, 0);
    ID3D11Buffer* frameCB = m_frameCBuffer.Get();
    m_context->VSSetConstantBuffers(0, 1, &frameCB);
    m_context->PSSetConstantBuffers(0, 1, &frameCB);

    FrameStats stats{};
    const float timeSeconds = std::max(0.0f, m_animationPlayback.timeSeconds);
    const std::vector<std::vector<Mat4>> hierarchyWorld =
        BuildAnimatedHierarchyWorldTransforms(timeSeconds);
    const std::vector<std::vector<Mat4>> cpuSkinHierarchyWorld =
        BuildCpuSkinHierarchyWorldTransforms(timeSeconds);

    auto getWorldForBinding = [&](int hierarchyIndex, int pivotIndex, const Mat4& fallback) -> Mat4 {
        if (hierarchyIndex < 0 || hierarchyIndex >= static_cast<int>(hierarchyWorld.size())) {
            return fallback;
        }
        const auto& worlds = hierarchyWorld[hierarchyIndex];
        if (pivotIndex < 0 || pivotIndex >= static_cast<int>(worlds.size())) {
            return fallback;
        }
        return worlds[pivotIndex];
    };

    for (std::size_t lodGroupIndex = 0; lodGroupIndex < m_scene.lodGroups.size(); ++lodGroupIndex) {
        const auto& lodGroup = m_scene.lodGroups[lodGroupIndex];
        int forcedEntryIndex = -1;
        if (m_settings.lockLodLevel && !lodGroup.entries.empty()) {
            forcedEntryIndex = std::clamp(
                m_settings.lockedLodLevel,
                0,
                static_cast<int>(lodGroup.entries.size()) - 1);
        }

        for (std::size_t entryIndex = 0; entryIndex < lodGroup.entries.size(); ++entryIndex) {
            if (forcedEntryIndex >= 0 && static_cast<int>(entryIndex) != forcedEntryIndex) {
                continue;
            }

            const auto& entry = lodGroup.entries[entryIndex];
            if (entry.meshIndex < 0 || entry.meshIndex >= static_cast<int>(m_gpuMeshes.size())) {
                continue;
            }

            GpuMesh& gpuMesh = m_gpuMeshes[entry.meshIndex];
            if (gpuMesh.hidden) {
                continue;
            }

            RenderInstanceKey key{};
            key.kind = RenderInstanceKind::LodEntry;
            key.primaryIndex = static_cast<int>(lodGroupIndex);
            key.secondaryIndex = static_cast<int>(entryIndex);
            if (m_hiddenInstances.contains(key)) {
                continue;
            }

            Mat4 world = gpuMesh.skinned
                ? Mat4::Identity()
                : getWorldForBinding(entry.hierarchyIndex, entry.pivotIndex, Mat4::Identity());
            if (const auto overrideIt = m_transformOverrides.find(key); overrideIt != m_transformOverrides.end()) {
                world = overrideIt->second;
            }
            Mat4 boundsWorld = world;
            if (gpuMesh.skinned
                && entry.hierarchyIndex >= 0
                && entry.hierarchyIndex < static_cast<int>(hierarchyWorld.size())
                && !hierarchyWorld[static_cast<std::size_t>(entry.hierarchyIndex)].empty())
            {
                boundsWorld = hierarchyWorld[static_cast<std::size_t>(entry.hierarchyIndex)].front();
            }
            const Vec3 worldCenter = TransformPoint(boundsWorld, gpuMesh.boundsCenter);
            const float distanceToCamera = Length(worldCenter - cameraPos);

            if (forcedEntryIndex < 0 && !ShouldRenderLodEntry(entry, gpuMesh, distanceToCamera)) {
                continue;
            }

            const RenderMaterial* material = nullptr;
            if (gpuMesh.materialIndex >= 0 && gpuMesh.materialIndex < static_cast<int>(m_scene.materials.size())) {
                material = &m_scene.materials[gpuMesh.materialIndex];
            }

            if (gpuMesh.skinned
                && !UpdateSkinnedMeshVertices(
                    m_scene.meshes[static_cast<std::size_t>(entry.meshIndex)],
                    gpuMesh,
                    entry.hierarchyIndex,
                    cpuSkinHierarchyWorld))
            {
                continue;
            }

            const bool selected = m_selectedInstance.has_value() && *m_selectedInstance == key;
            DrawMesh(gpuMesh, world, material, frameData, timeSeconds, selected, stats);
        }
    }

    for (std::size_t nodeIndex = 0; nodeIndex < m_scene.looseNodes.size(); ++nodeIndex) {
        const auto& node = m_scene.looseNodes[nodeIndex];
        if (node.meshIndex < 0 || node.meshIndex >= static_cast<int>(m_gpuMeshes.size())) {
            continue;
        }

        GpuMesh& gpuMesh = m_gpuMeshes[node.meshIndex];
        if (gpuMesh.hidden) {
            continue;
        }

        RenderInstanceKey key{};
        key.kind = RenderInstanceKind::LooseNode;
        key.primaryIndex = static_cast<int>(nodeIndex);
        key.secondaryIndex = -1;
        if (m_hiddenInstances.contains(key)) {
            continue;
        }

        Mat4 world = gpuMesh.skinned
            ? Mat4::Identity()
            : getWorldForBinding(node.hierarchyIndex, node.pivotIndex, node.localTransform);
        if (const auto overrideIt = m_transformOverrides.find(key); overrideIt != m_transformOverrides.end()) {
            world = overrideIt->second;
        }
        const RenderMaterial* material = nullptr;
        if (gpuMesh.materialIndex >= 0 && gpuMesh.materialIndex < static_cast<int>(m_scene.materials.size())) {
            material = &m_scene.materials[gpuMesh.materialIndex];
        }

        if (gpuMesh.skinned
            && !UpdateSkinnedMeshVertices(
                m_scene.meshes[static_cast<std::size_t>(node.meshIndex)],
                gpuMesh,
                node.hierarchyIndex,
                cpuSkinHierarchyWorld))
        {
            continue;
        }

        const bool selected = m_selectedInstance.has_value() && *m_selectedInstance == key;
        DrawMesh(gpuMesh, world, material, frameData, timeSeconds, selected, stats);
    }

    m_lastFrameStats = stats;
    if (overlayCallback) {
        overlayCallback();
    }
    (void)m_swapChain->Present(1, 0);
}

FrameStats D3D11RenderBackend::GetFrameStats() const {
    return m_lastFrameStats;
}

bool D3D11RenderBackend::CreateDeviceAndSwapchain(void* nativeWindow, uint32_t width, uint32_t height) {
    HWND hwnd = reinterpret_cast<HWND>(nativeWindow);
    if (!hwnd) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Width = std::max<uint32_t>(1, width);
    swapDesc.BufferDesc.Height = std::max<uint32_t>(1, height);
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = hwnd;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const std::array<D3D_FEATURE_LEVEL, 3> levels = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1
    };

    D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels.data(),
        static_cast<UINT>(levels.size()),
        D3D11_SDK_VERSION,
        &swapDesc,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &selectedLevel,
        m_context.GetAddressOf());

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            levels.data(),
            static_cast<UINT>(levels.size()),
            D3D11_SDK_VERSION,
            &swapDesc,
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &selectedLevel,
            m_context.GetAddressOf());
    }

    return SUCCEEDED(hr);
}

bool D3D11RenderBackend::CreateRenderTargets(uint32_t width, uint32_t height) {
    if (!m_swapChain || !m_device) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = std::max<uint32_t>(1, width);
    depthDesc.Height = std::max<uint32_t>(1, height);
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = m_device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateDepthStencilView(m_depthTexture.Get(), nullptr, m_dsv.GetAddressOf());
    return SUCCEEDED(hr);
}

bool D3D11RenderBackend::CreateShaders() {
    if (!m_device) {
        return false;
    }

    constexpr const char* kShaderSource = R"(
cbuffer FrameCB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCameraWorldPos;
    float4 gAmbient;
    float4 gDirectionalLightDir;
    float4 gDirectionalLightColor;
    float4 gFogColor;
    float4 gFogParams;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 gWorld;
    float4 gBaseColor;
    float4 gFlags;
    float4 gUvAnim0;
    float4 gUvAnim1;
    float4 gUvAnim2;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn input)
{
    VSOut output;
    float4 worldPos = mul(float4(input.pos, 1.0), gWorld);
    output.pos = mul(worldPos, gViewProj);
    output.worldPos = worldPos.xyz;
    output.worldNormal = normalize(mul(float4(input.normal, 0.0), gWorld).xyz);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 PSMain(VSOut input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 L = normalize(-gDirectionalLightDir.xyz);
    float ndotl = saturate(dot(N, L));

    float2 mappedUv = input.uv;
    if (gUvAnim0.x > 0.5 && gUvAnim0.x < 1.5)
    {
        mappedUv = (input.uv * gUvAnim2.xy) + gUvAnim1.xy + (gUvAnim1.zw * gUvAnim0.y);
    }
    else if (gUvAnim0.x > 1.5)
    {
        float2 centered = (input.uv - gUvAnim2.zw) * gUvAnim2.xy;
        float angle = gUvAnim0.z * gUvAnim0.y;
        float s = sin(angle);
        float c = cos(angle);
        float2 rotated = float2(
            centered.x * c - centered.y * s,
            centered.x * s + centered.y * c);
        mappedUv = rotated + gUvAnim2.zw + gUvAnim1.xy;
    }

    float4 texColor = (gFlags.x > 0.5) ? gTexture.Sample(gSampler, mappedUv) : float4(1.0, 1.0, 1.0, 1.0);
    if (gFlags.x > 0.5 && texColor.a <= 0.001)
    {
        discard;
    }
    float4 baseColor = texColor * input.color * gBaseColor;

    float3 lit = baseColor.rgb * (gAmbient.rgb + gDirectionalLightColor.rgb * ndotl);

    if (gFlags.z > 0.5)
    {
        const float2 uv = frac(input.uv);
        const float2 uvCell = frac(input.uv * 8.0);
        const float gridLine = (uvCell.x < 0.03 || uvCell.x > 0.97 || uvCell.y < 0.03 || uvCell.y > 0.97) ? 1.0 : 0.0;
        const float3 uvColor = float3(uv.x, uv.y, 1.0 - uv.x);
        lit = lerp(uvColor, float3(0.0, 0.0, 0.0), gridLine);
    }

    if (gFlags.y > 0.5)
    {
        const float3 highlightTint = float3(1.0, 0.85, 0.35);
        lit = lerp(lit, lit * highlightTint + float3(0.06, 0.04, 0.0), 0.45);
    }

    if (gFogParams.x > 0.5)
    {
        float distance = length(gCameraWorldPos.xyz - input.worldPos);
        float fog = saturate((distance - gFogParams.y) / max(1.0, gFogParams.z - gFogParams.y));
        lit = lerp(lit, gFogColor.rgb, fog);
    }

    return float4(lit, baseColor.a);
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        kShaderSource,
        std::strlen(kShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        0,
        0,
        vsBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = D3DCompile(
        kShaderSource,
        std::strlen(kShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        0,
        0,
        psBlob.GetAddressOf(),
        errorBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        m_vs.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        m_ps.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 4> kInputLayout = {
        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(CpuVertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(CpuVertex, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(CpuVertex, uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(CpuVertex, color)), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = m_device->CreateInputLayout(
        kInputLayout.data(),
        static_cast<UINT>(kInputLayout.size()),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_device->CreateSamplerState(&samplerDesc, m_sampler.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_BACK;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    hr = m_device->CreateRasterizerState(&rsDesc, m_rsCullBack.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    rsDesc.CullMode = D3D11_CULL_NONE;
    hr = m_device->CreateRasterizerState(&rsDesc, m_rsCullNone.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    hr = m_device->CreateDepthStencilState(&depthDesc, m_depthState.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState(&blendDesc, m_blendOpaque.GetAddressOf());

    return SUCCEEDED(hr);
}

bool D3D11RenderBackend::CreateDefaultTexture() {
    if (!m_device) {
        return false;
    }

    constexpr uint32_t kPixels[4] = {
        0xFF2A2A2A,
        0xFFD6D6D6,
        0xFFD6D6D6,
        0xFF2A2A2A,
    };

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 2;
    texDesc.Height = 2;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = kPixels;
    init.SysMemPitch = sizeof(uint32_t) * 2;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = m_device->CreateTexture2D(&texDesc, &init, texture.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateShaderResourceView(texture.Get(), nullptr, m_defaultTexture.srv.GetAddressOf());
    return SUCCEEDED(hr);
}

bool D3D11RenderBackend::CreateConstantBuffers() {
    if (!m_device) {
        return false;
    }

    D3D11_BUFFER_DESC frameDesc{};
    frameDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    frameDesc.ByteWidth = static_cast<UINT>((sizeof(CBufferFrame) + 15u) & ~15u);
    frameDesc.Usage = D3D11_USAGE_DEFAULT;

    HRESULT hr = m_device->CreateBuffer(&frameDesc, nullptr, m_frameCBuffer.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_BUFFER_DESC objectDesc{};
    objectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    objectDesc.ByteWidth = static_cast<UINT>((sizeof(CBufferObject) + 15u) & ~15u);
    objectDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = m_device->CreateBuffer(&objectDesc, nullptr, m_objectCBuffer.GetAddressOf());
    return SUCCEEDED(hr);
}

void D3D11RenderBackend::ReleaseRenderTargets() {
    m_dsv.Reset();
    m_depthTexture.Reset();
    m_rtv.Reset();
}

bool D3D11RenderBackend::BuildGpuMesh(const RenderMesh& mesh, GpuMesh& outMesh) {
    if (!m_device || mesh.vertices.empty() || mesh.indices.empty()) {
        return false;
    }

    std::vector<CpuVertex> vertices;
    vertices.reserve(mesh.vertices.size());
    for (const auto& src : mesh.vertices) {
        CpuVertex dst{};
        dst.position[0] = src.position.x;
        dst.position[1] = src.position.y;
        dst.position[2] = src.position.z;
        dst.normal[0] = src.normal.x;
        dst.normal[1] = src.normal.y;
        dst.normal[2] = src.normal.z;
        dst.uv[0] = src.uv.x;
        dst.uv[1] = src.uv.y;
        dst.color[0] = src.color.x;
        dst.color[1] = src.color.y;
        dst.color[2] = src.color.z;
        dst.color[3] = src.color.w;
        vertices.push_back(dst);
    }

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(CpuVertex));
    vbDesc.Usage = mesh.skinned ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
    vbDesc.CPUAccessFlags = mesh.skinned ? D3D11_CPU_ACCESS_WRITE : 0u;

    D3D11_SUBRESOURCE_DATA vbInit{};
    vbInit.pSysMem = vertices.data();

    HRESULT hr = m_device->CreateBuffer(&vbDesc, &vbInit, outMesh.vertexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;

    D3D11_SUBRESOURCE_DATA ibInit{};
    ibInit.pSysMem = mesh.indices.data();

    hr = m_device->CreateBuffer(&ibDesc, &ibInit, outMesh.indexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    outMesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    outMesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
    outMesh.materialIndex = mesh.materialIndex;
    outMesh.twoSided = mesh.twoSided;
    outMesh.hidden = mesh.hidden;
    outMesh.skinned = mesh.skinned;
    outMesh.boundsCenter = mesh.boundsCenter;
    outMesh.boundsRadius = mesh.boundsRadius;
    outMesh.cpuVertices = std::move(vertices);

    return true;
}

bool D3D11RenderBackend::BuildGpuTexture(const RenderTexture& texture, GpuTexture& outTexture) {
    if (!m_device) {
        return false;
    }

    if (!texture.resolvedPath.empty()) {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> loadedSrv;
        if (BuildSrvFromFilePath(m_device.Get(), texture.resolvedPath, loadedSrv)) {
            outTexture.srv = std::move(loadedSrv);
            return true;
        }
    }

    const uint32_t hash = HashString32(texture.name);
    const uint8_t r = static_cast<uint8_t>(64 + (hash & 0x7F));
    const uint8_t g = static_cast<uint8_t>(64 + ((hash >> 8) & 0x7F));
    const uint8_t b = static_cast<uint8_t>(64 + ((hash >> 16) & 0x7F));

    const uint32_t pixel =
        (0xFFu << 24)
        | (static_cast<uint32_t>(b) << 16)
        | (static_cast<uint32_t>(g) << 8)
        | static_cast<uint32_t>(r);

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel;
    init.SysMemPitch = sizeof(uint32_t);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
    HRESULT hr = m_device->CreateTexture2D(&texDesc, &init, texture2d.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateShaderResourceView(texture2d.Get(), nullptr, outTexture.srv.GetAddressOf());
    return SUCCEEDED(hr);
}

std::vector<std::vector<Mat4>> D3D11RenderBackend::BuildAnimatedHierarchyWorldTransforms(
    float timeSeconds) const
{
    std::vector<std::vector<Mat4>> hierarchyWorld;
    hierarchyWorld.resize(m_scene.hierarchies.size());

    for (std::size_t h = 0; h < m_scene.hierarchies.size(); ++h) {
        const auto& hierarchy = m_scene.hierarchies[h];
        auto& worlds = hierarchyWorld[h];
        worlds.resize(hierarchy.pivots.size(), Mat4::Identity());

        float animationFrame = 0.0f;
        const RenderAnimationClip* activeClip =
            ResolveActiveClipForHierarchy(
                m_scene,
                hierarchy,
                m_animationPlayback,
                timeSeconds,
                animationFrame);

        std::vector<uint8_t> state(hierarchy.pivots.size(), 0);
        std::function<void(int)> buildPivot = [&](int pivotIndex) {
            if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
                return;
            }
            if (state[static_cast<std::size_t>(pivotIndex)] == 2) {
                return;
            }
            if (state[static_cast<std::size_t>(pivotIndex)] == 1) {
                worlds[static_cast<std::size_t>(pivotIndex)] =
                    hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;
                state[static_cast<std::size_t>(pivotIndex)] = 2;
                return;
            }

            state[static_cast<std::size_t>(pivotIndex)] = 1;
            Mat4 local = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;

            if (activeClip
                && pivotIndex > 0
                && pivotIndex < static_cast<int>(activeClip->pivots.size())) {
                const auto& animPivot = activeClip->pivots[static_cast<std::size_t>(pivotIndex)];
                const Vec3 translation = {
                    SampleFloatTrack(animPivot.translationX, animationFrame),
                    SampleFloatTrack(animPivot.translationY, animationFrame),
                    SampleFloatTrack(animPivot.translationZ, animationFrame)
                };
                const Vec4 rotation = SampleQuatTrack(animPivot.rotation, animationFrame);
                const Mat4 animationLocal = TransformFromTranslationRotation(
                    translation,
                    rotation.x,
                    rotation.y,
                    rotation.z,
                    rotation.w);
                local = Multiply(local, animationLocal);
            }

            const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size())) {
                buildPivot(parentIndex);
                worlds[static_cast<std::size_t>(pivotIndex)] =
                    Multiply(worlds[static_cast<std::size_t>(parentIndex)], local);
            }
            else {
                worlds[static_cast<std::size_t>(pivotIndex)] = local;
            }
            state[static_cast<std::size_t>(pivotIndex)] = 2;
        };

        for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
            buildPivot(i);
        }
    }

    return hierarchyWorld;
}

std::vector<std::vector<Mat4>> D3D11RenderBackend::BuildCpuSkinHierarchyWorldTransforms(
    float timeSeconds) const
{
    std::vector<std::vector<Mat4>> hierarchyWorld;
    hierarchyWorld.resize(m_scene.hierarchies.size());

    for (std::size_t h = 0; h < m_scene.hierarchies.size(); ++h) {
        const auto& hierarchy = m_scene.hierarchies[h];
        auto& worlds = hierarchyWorld[h];
        worlds.resize(hierarchy.pivots.size(), TTSkinIdentity());

        float animationFrame = 0.0f;
        const RenderAnimationClip* activeClip =
            ResolveActiveClipForHierarchy(
                m_scene,
                hierarchy,
                m_animationPlayback,
                timeSeconds,
                animationFrame);

        std::vector<uint8_t> state(hierarchy.pivots.size(), 0);
        std::function<void(int)> buildPivot = [&](int pivotIndex) {
            if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
                return;
            }
            if (state[static_cast<std::size_t>(pivotIndex)] == 2) {
                return;
            }
            if (state[static_cast<std::size_t>(pivotIndex)] == 1) {
                worlds[static_cast<std::size_t>(pivotIndex)] = TTSkinIdentity();
                state[static_cast<std::size_t>(pivotIndex)] = 2;
                return;
            }

            state[static_cast<std::size_t>(pivotIndex)] = 1;
            const auto& pivot = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)];
            Mat4 local = (pivotIndex == 0)
                ? TTSkinIdentity()
                : TTSkinTransformFromTranslationRotation(pivot.baseTranslation, pivot.baseRotation);

            if (activeClip
                && pivotIndex > 0
                && pivotIndex < static_cast<int>(activeClip->pivots.size())) {
                const auto& animPivot = activeClip->pivots[static_cast<std::size_t>(pivotIndex)];
                const Vec3 translation = {
                    SampleFloatTrack(animPivot.translationX, animationFrame),
                    SampleFloatTrack(animPivot.translationY, animationFrame),
                    SampleFloatTrack(animPivot.translationZ, animationFrame)
                };
                const Vec4 rotation = SampleQuatTrack(animPivot.rotation, animationFrame);
                const Mat4 animationLocal = TTSkinTransformFromTranslationRotation(translation, rotation);
                local = TTSkinMultiply(local, animationLocal);
            }

            const int parentIndex = pivot.parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size())) {
                buildPivot(parentIndex);
                worlds[static_cast<std::size_t>(pivotIndex)] =
                    TTSkinMultiply(worlds[static_cast<std::size_t>(parentIndex)], local);
            }
            else {
                worlds[static_cast<std::size_t>(pivotIndex)] = local;
            }
            state[static_cast<std::size_t>(pivotIndex)] = 2;
        };

        for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
            buildPivot(i);
        }
    }

    return hierarchyWorld;
}

bool D3D11RenderBackend::UpdateSkinnedMeshVertices(
    const RenderMesh& mesh,
    GpuMesh& gpuMesh,
    int hierarchyIndex,
    const std::vector<std::vector<Mat4>>& hierarchyWorld)
{
    if (!m_context || !gpuMesh.vertexBuffer || gpuMesh.cpuVertices.size() != mesh.vertices.size()) {
        return false;
    }

    const std::vector<Mat4>* bones = nullptr;
    if (hierarchyIndex >= 0 && hierarchyIndex < static_cast<int>(hierarchyWorld.size())) {
        bones = &hierarchyWorld[static_cast<std::size_t>(hierarchyIndex)];
    }

    const int influencesPerVertex =
        std::clamp<int>(static_cast<int>(mesh.bonesPerVertex), 1, 4);

    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto& src = mesh.vertices[i];
        auto& dst = gpuMesh.cpuVertices[i];

        Vec3 positionAccum{ 0.0f, 0.0f, 0.0f };
        Vec3 normalAccum{ 0.0f, 0.0f, 0.0f };
        bool wroteSkinning = false;

        if (bones && !bones->empty()) {
            for (int influence = 0; influence < influencesPerVertex; ++influence) {
                const uint16_t boneIndex = src.boneIndices[static_cast<std::size_t>(influence)];
                const float weight = src.boneWeights[static_cast<std::size_t>(influence)];
                if (boneIndex == 0xFFFFu || weight <= 0.0f) {
                    continue;
                }
                if (boneIndex >= bones->size()) {
                    continue;
                }

                const bool useSecondary = (influence == 1) && mesh.hasSecondaryVertexStream;
                const Vec3& sourcePosition = useSecondary ? src.secondaryPosition : src.position;
                const Vec3& sourceNormal = useSecondary ? src.secondaryNormal : src.normal;
                const Mat4& boneTransform = (*bones)[boneIndex];

                positionAccum = positionAccum + (TTSkinTransformPoint(boneTransform, sourcePosition) * weight);
                normalAccum = normalAccum + (TTSkinTransformDirection(boneTransform, sourceNormal) * weight);
                wroteSkinning = true;
            }
        }

        if (!wroteSkinning) {
            positionAccum = src.position;
            normalAccum = src.normal;
        }

        normalAccum = Normalize(normalAccum);
        dst.position[0] = positionAccum.x;
        dst.position[1] = positionAccum.y;
        dst.position[2] = positionAccum.z;
        dst.normal[0] = normalAccum.x;
        dst.normal[1] = normalAccum.y;
        dst.normal[2] = normalAccum.z;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT hr = m_context->Map(
        gpuMesh.vertexBuffer.Get(),
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &mapped);
    if (FAILED(hr) || !mapped.pData) {
        return false;
    }

    std::memcpy(
        mapped.pData,
        gpuMesh.cpuVertices.data(),
        gpuMesh.cpuVertices.size() * sizeof(CpuVertex));
    m_context->Unmap(gpuMesh.vertexBuffer.Get(), 0);
    return true;
}

void D3D11RenderBackend::DrawMesh(
    const GpuMesh& mesh,
    const Mat4& world,
    const RenderMaterial* material,
    const CBufferFrame&,
    float timeSeconds,
    bool selected,
    FrameStats& stats)
{
    if (!m_context || !mesh.vertexBuffer || !mesh.indexBuffer) {
        return;
    }

    const UINT stride = sizeof(CpuVertex);
    const UINT offset = 0;

    ID3D11Buffer* vb = mesh.vertexBuffer.Get();
    ID3D11Buffer* ib = mesh.indexBuffer.Get();
    m_context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    m_context->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

    ID3D11RasterizerState* rs = mesh.twoSided ? m_rsCullNone.Get() : m_rsCullBack.Get();
    m_context->RSSetState(rs);

    CBufferObject objectData{};
    objectData.world = world;
    objectData.baseColor = material
        ? material->diffuseColor
        : Vec4{ 1.0f, 1.0f, 1.0f, 1.0f };

    ID3D11ShaderResourceView* srv = m_defaultTexture.srv.Get();
    objectData.flags = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (material && material->textureIndex >= 0
        && material->textureIndex < static_cast<int>(m_gpuTextures.size())) {
        const auto& texture = m_gpuTextures[material->textureIndex];
        if (texture.srv) {
            srv = texture.srv.Get();
            objectData.flags.x = 1.0f;
        }
    }
    objectData.flags.y = selected ? 1.0f : 0.0f;
    objectData.flags.z = m_settings.debugShowUv ? 1.0f : 0.0f;
    if (material) {
        objectData.uvAnim0 = {
            static_cast<float>(material->uvAnimMode),
            timeSeconds,
            material->uvRotateRadPerSec,
            0.0f
        };
        objectData.uvAnim1 = {
            material->uvOffsetU,
            material->uvOffsetV,
            material->uvScrollU,
            material->uvScrollV
        };
        objectData.uvAnim2 = {
            material->uvScaleU,
            material->uvScaleV,
            material->uvCenterU,
            material->uvCenterV
        };
    }

    m_context->UpdateSubresource(m_objectCBuffer.Get(), 0, nullptr, &objectData, 0, 0);
    ID3D11Buffer* objectCB = m_objectCBuffer.Get();
    m_context->VSSetConstantBuffers(1, 1, &objectCB);
    m_context->PSSetConstantBuffers(1, 1, &objectCB);
    m_context->PSSetShaderResources(0, 1, &srv);

    m_context->DrawIndexed(mesh.indexCount, 0, 0);
    stats.drawCalls += 1;
    stats.triangles += mesh.indexCount / 3;
}

Mat4 D3D11RenderBackend::BuildPivotWorldTransform(int hierarchyIndex, int pivotIndex) const {
    if (hierarchyIndex < 0 || hierarchyIndex >= static_cast<int>(m_scene.hierarchies.size())) {
        return Mat4::Identity();
    }

    const auto& hierarchy = m_scene.hierarchies[hierarchyIndex];
    if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        return Mat4::Identity();
    }

    Mat4 transform = hierarchy.pivots[pivotIndex].localTransform;
    int parent = hierarchy.pivots[pivotIndex].parentIndex;
    int guard = 0;
    while (parent >= 0 && parent < static_cast<int>(hierarchy.pivots.size()) && guard++ < 256) {
        transform = Multiply(hierarchy.pivots[parent].localTransform, transform);
        parent = hierarchy.pivots[parent].parentIndex;
    }
    return transform;
}

bool D3D11RenderBackend::ShouldRenderLodEntry(
    const RenderLodEntry& entry,
    const GpuMesh&,
    float cameraDistance) const
{
    if (!m_settings.enableLod) {
        return true;
    }

    const float bias = std::max(0.01f, m_settings.lodBias);
    const float minDistance = entry.minDistance * bias;
    const float maxDistance = std::isfinite(entry.maxDistance)
        ? entry.maxDistance * bias
        : std::numeric_limits<float>::max();

    if (cameraDistance < minDistance) {
        return false;
    }
    if (cameraDistance >= maxDistance) {
        return false;
    }
    return true;
}

} // namespace OW3D::Render
