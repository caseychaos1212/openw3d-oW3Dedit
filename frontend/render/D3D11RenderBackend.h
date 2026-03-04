#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

#include "IRenderBackend.h"

namespace OW3D::Render {

class D3D11RenderBackend final : public IRenderBackend {
public:
    D3D11RenderBackend();
    ~D3D11RenderBackend() override;

    bool Initialize(void* nativeWindow, uint32_t width, uint32_t height) override;
    void Shutdown() override;
    bool UploadScene(const RenderScene& scene) override;
    void Resize(uint32_t width, uint32_t height) override;
    void SetCamera(const CameraState& camera) override;
    void SetRenderSettings(const RenderSettings& settings) override;
    void SetSelectedInstance(const std::optional<RenderInstanceKey>& selected) override;
    void SetTransformOverrides(
        const std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash>& overrides) override;
    void SetHiddenInstances(
        const std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash>& hidden) override;
    void RenderFrame(const std::function<void()>& overlayCallback = {}) override;
    FrameStats GetFrameStats() const override;
    void* NativeDeviceHandle() const override { return m_device.Get(); }
    void* NativeDeviceContextHandle() const override { return m_context.Get(); }

private:
    struct GpuMesh {
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        int materialIndex = -1;
        bool twoSided = false;
        bool hidden = false;
        Vec3 boundsCenter{};
        float boundsRadius = 0.0f;
    };

    struct GpuTexture {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct CBufferFrame {
        Mat4 viewProj{};
        Vec4 cameraWorldPos{};
        Vec4 ambient{};
        Vec4 directionalLightDir{};
        Vec4 directionalLightColor{};
        Vec4 fogColor{};
        Vec4 fogParams{}; // x=enabled, y=near, z=far
    };

    struct CBufferObject {
        Mat4 world{};
        Vec4 baseColor{};
        Vec4 flags{}; // x=has texture, y=selected highlight, z=uv debug view
        Vec4 uvAnim0{}; // x=mode (0 none, 1 scroll, 2 rotate), y=timeSeconds, z=rotateRadPerSec
        Vec4 uvAnim1{}; // x=offsetU, y=offsetV, z=scrollU, w=scrollV
        Vec4 uvAnim2{}; // x=scaleU, y=scaleV, z=centerU, w=centerV
    };

    bool CreateDeviceAndSwapchain(void* nativeWindow, uint32_t width, uint32_t height);
    bool CreateRenderTargets(uint32_t width, uint32_t height);
    bool CreateShaders();
    bool CreateDefaultTexture();
    bool CreateConstantBuffers();
    void ReleaseRenderTargets();

    bool BuildGpuMesh(const RenderMesh& mesh, GpuMesh& outMesh);
    bool BuildGpuTexture(const RenderTexture& texture, GpuTexture& outTexture);

    void DrawMesh(
        const GpuMesh& mesh,
        const Mat4& world,
        const RenderMaterial* material,
        const CBufferFrame& frameData,
        float timeSeconds,
        bool selected,
        FrameStats& stats);

    Mat4 BuildPivotWorldTransform(int hierarchyIndex, int pivotIndex) const;
    bool ShouldRenderLodEntry(const RenderLodEntry& entry, const GpuMesh& mesh, float cameraDistance) const;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rsCullBack;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rsCullNone;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendOpaque;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_frameCBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_objectCBuffer;

    GpuTexture m_defaultTexture;

    RenderScene m_scene{};
    std::vector<GpuMesh> m_gpuMeshes;
    std::vector<GpuTexture> m_gpuTextures;

    CameraState m_camera{};
    RenderSettings m_settings{};
    FrameStats m_lastFrameStats{};
    std::optional<RenderInstanceKey> m_selectedInstance;
    std::unordered_map<RenderInstanceKey, Mat4, RenderInstanceKeyHash> m_transformOverrides;
    std::unordered_set<RenderInstanceKey, RenderInstanceKeyHash> m_hiddenInstances;

    std::chrono::steady_clock::time_point m_startTime = std::chrono::steady_clock::now();

    uint32_t m_viewWidth = 1;
    uint32_t m_viewHeight = 1;
    bool m_initialized = false;
};

} // namespace OW3D::Render
