#include "RenderViewportWidget.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "D3D11RenderBackend.h"
#include "../../backend/render/AnimationPose.h"

#include "../../thirdparty/imgui/imgui.h"
#include "../../thirdparty/imgui/backends/imgui_impl_dx11.h"
#include "../../thirdparty/ImGuizmo/ImGuizmo.h"

namespace OW3D::Render {
namespace {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

constexpr float kCameraPitchLimit = 1.55334303f; // ~89 degrees

std::optional<ImGuiKey> ImGuiKeyFromQtKey(int key, Qt::KeyboardModifiers modifiers) {
    const bool keypad = (modifiers & Qt::KeypadModifier) != 0;
    switch (key) {
    case Qt::Key_Tab: return ImGuiKey_Tab;
    case Qt::Key_Left: return ImGuiKey_LeftArrow;
    case Qt::Key_Right: return ImGuiKey_RightArrow;
    case Qt::Key_Up: return ImGuiKey_UpArrow;
    case Qt::Key_Down: return ImGuiKey_DownArrow;
    case Qt::Key_PageUp: return ImGuiKey_PageUp;
    case Qt::Key_PageDown: return ImGuiKey_PageDown;
    case Qt::Key_Home: return ImGuiKey_Home;
    case Qt::Key_End: return ImGuiKey_End;
    case Qt::Key_Insert: return ImGuiKey_Insert;
    case Qt::Key_Delete: return ImGuiKey_Delete;
    case Qt::Key_Backspace: return ImGuiKey_Backspace;
    case Qt::Key_Space: return ImGuiKey_Space;
    case Qt::Key_Enter: return keypad ? ImGuiKey_KeypadEnter : ImGuiKey_Enter;
    case Qt::Key_Return: return ImGuiKey_Enter;
    case Qt::Key_Escape: return ImGuiKey_Escape;
    case Qt::Key_Apostrophe: return ImGuiKey_Apostrophe;
    case Qt::Key_Comma: return keypad ? ImGuiKey_KeypadDecimal : ImGuiKey_Comma;
    case Qt::Key_Minus: return keypad ? ImGuiKey_KeypadSubtract : ImGuiKey_Minus;
    case Qt::Key_Period: return keypad ? ImGuiKey_KeypadDecimal : ImGuiKey_Period;
    case Qt::Key_Slash: return keypad ? ImGuiKey_KeypadDivide : ImGuiKey_Slash;
    case Qt::Key_Semicolon: return ImGuiKey_Semicolon;
    case Qt::Key_Equal: return keypad ? ImGuiKey_KeypadEqual : ImGuiKey_Equal;
    case Qt::Key_BracketLeft: return ImGuiKey_LeftBracket;
    case Qt::Key_Backslash: return ImGuiKey_Backslash;
    case Qt::Key_BracketRight: return ImGuiKey_RightBracket;
    case Qt::Key_QuoteLeft: return ImGuiKey_GraveAccent;
    case Qt::Key_CapsLock: return ImGuiKey_CapsLock;
    case Qt::Key_ScrollLock: return ImGuiKey_ScrollLock;
    case Qt::Key_NumLock: return ImGuiKey_NumLock;
    case Qt::Key_Print: return ImGuiKey_PrintScreen;
    case Qt::Key_Pause: return ImGuiKey_Pause;
    case Qt::Key_0: return keypad ? ImGuiKey_Keypad0 : ImGuiKey_0;
    case Qt::Key_1: return keypad ? ImGuiKey_Keypad1 : ImGuiKey_1;
    case Qt::Key_2: return keypad ? ImGuiKey_Keypad2 : ImGuiKey_2;
    case Qt::Key_3: return keypad ? ImGuiKey_Keypad3 : ImGuiKey_3;
    case Qt::Key_4: return keypad ? ImGuiKey_Keypad4 : ImGuiKey_4;
    case Qt::Key_5: return keypad ? ImGuiKey_Keypad5 : ImGuiKey_5;
    case Qt::Key_6: return keypad ? ImGuiKey_Keypad6 : ImGuiKey_6;
    case Qt::Key_7: return keypad ? ImGuiKey_Keypad7 : ImGuiKey_7;
    case Qt::Key_8: return keypad ? ImGuiKey_Keypad8 : ImGuiKey_8;
    case Qt::Key_9: return keypad ? ImGuiKey_Keypad9 : ImGuiKey_9;
    case Qt::Key_A: return ImGuiKey_A;
    case Qt::Key_B: return ImGuiKey_B;
    case Qt::Key_C: return ImGuiKey_C;
    case Qt::Key_D: return ImGuiKey_D;
    case Qt::Key_E: return ImGuiKey_E;
    case Qt::Key_F: return ImGuiKey_F;
    case Qt::Key_G: return ImGuiKey_G;
    case Qt::Key_H: return ImGuiKey_H;
    case Qt::Key_I: return ImGuiKey_I;
    case Qt::Key_J: return ImGuiKey_J;
    case Qt::Key_K: return ImGuiKey_K;
    case Qt::Key_L: return ImGuiKey_L;
    case Qt::Key_M: return ImGuiKey_M;
    case Qt::Key_N: return ImGuiKey_N;
    case Qt::Key_O: return ImGuiKey_O;
    case Qt::Key_P: return ImGuiKey_P;
    case Qt::Key_Q: return ImGuiKey_Q;
    case Qt::Key_R: return ImGuiKey_R;
    case Qt::Key_S: return ImGuiKey_S;
    case Qt::Key_T: return ImGuiKey_T;
    case Qt::Key_U: return ImGuiKey_U;
    case Qt::Key_V: return ImGuiKey_V;
    case Qt::Key_W: return ImGuiKey_W;
    case Qt::Key_X: return ImGuiKey_X;
    case Qt::Key_Y: return ImGuiKey_Y;
    case Qt::Key_Z: return ImGuiKey_Z;
    case Qt::Key_F1: return ImGuiKey_F1;
    case Qt::Key_F2: return ImGuiKey_F2;
    case Qt::Key_F3: return ImGuiKey_F3;
    case Qt::Key_F4: return ImGuiKey_F4;
    case Qt::Key_F5: return ImGuiKey_F5;
    case Qt::Key_F6: return ImGuiKey_F6;
    case Qt::Key_F7: return ImGuiKey_F7;
    case Qt::Key_F8: return ImGuiKey_F8;
    case Qt::Key_F9: return ImGuiKey_F9;
    case Qt::Key_F10: return ImGuiKey_F10;
    case Qt::Key_F11: return ImGuiKey_F11;
    case Qt::Key_F12: return ImGuiKey_F12;
    case Qt::Key_Control: return ImGuiKey_LeftCtrl;
    case Qt::Key_Shift: return ImGuiKey_LeftShift;
    case Qt::Key_Alt: return ImGuiKey_LeftAlt;
    case Qt::Key_Meta: return ImGuiKey_LeftSuper;
    default:
        return std::nullopt;
    }
}

void SubmitImGuiModifierEvents(ImGuiIO& io, Qt::KeyboardModifiers modifiers) {
    io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & Qt::ControlModifier) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (modifiers & Qt::ShiftModifier) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (modifiers & Qt::AltModifier) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (modifiers & Qt::MetaModifier) != 0);
}

void SubmitImGuiKeyboardEvent(QKeyEvent* event, bool down) {
    ImGuiIO& io = ImGui::GetIO();
    SubmitImGuiModifierEvents(io, event->modifiers());
    const std::optional<ImGuiKey> imguiKey =
        ImGuiKeyFromQtKey(event->key(), event->modifiers());
    if (!imguiKey.has_value()) {
        return;
    }

    io.AddKeyEvent(*imguiKey, down);
    io.SetKeyEventNativeData(
        *imguiKey,
        event->nativeVirtualKey(),
        event->nativeScanCode());
}

Vec3 WorldUp() {
    return { 0.0f, 0.0f, 1.0f };
}

float RadToDeg(float radians) {
    return radians * 57.29577951308232f;
}

Vec3 CameraForward(float yaw, float pitch) {
    const float cp = std::cos(pitch);
    return {
        std::cos(yaw) * cp,
        std::sin(yaw) * cp,
        std::sin(pitch)
    };
}

void Mat4ToFloatArray(const Mat4& m, float out[16]) {
    std::memcpy(out, m.m, sizeof(float) * 16);
}

Mat4 Mat4FromFloatArray(const float in[16]) {
    Mat4 out{};
    std::memcpy(out.m, in, sizeof(float) * 16);
    return out;
}

bool ProjectWorldPointToScreen(
    const Mat4& viewProjection,
    const Vec3& worldPos,
    float viewportWidth,
    float viewportHeight,
    ImVec2& outScreen,
    float* outDepth = nullptr)
{
    const float clipX =
        worldPos.x * viewProjection.m[0]
        + worldPos.y * viewProjection.m[4]
        + worldPos.z * viewProjection.m[8]
        + viewProjection.m[12];
    const float clipY =
        worldPos.x * viewProjection.m[1]
        + worldPos.y * viewProjection.m[5]
        + worldPos.z * viewProjection.m[9]
        + viewProjection.m[13];
    const float clipZ =
        worldPos.x * viewProjection.m[2]
        + worldPos.y * viewProjection.m[6]
        + worldPos.z * viewProjection.m[10]
        + viewProjection.m[14];
    const float clipW =
        worldPos.x * viewProjection.m[3]
        + worldPos.y * viewProjection.m[7]
        + worldPos.z * viewProjection.m[11]
        + viewProjection.m[15];

    if (clipW <= 1.0e-5f) {
        return false;
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    const float ndcZ = clipZ / clipW;
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)) {
        return false;
    }
    if (ndcZ < -0.25f || ndcZ > 1.25f) {
        return false;
    }

    outScreen.x = (ndcX * 0.5f + 0.5f) * viewportWidth;
    outScreen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * viewportHeight;
    if (outDepth) {
        *outDepth = ndcZ;
    }
    return true;
}

Mat4 OrthonormalizeRigidTransform(const Mat4& m) {
    Vec3 x = { m.m[0], m.m[1], m.m[2] };
    Vec3 y = { m.m[4], m.m[5], m.m[6] };
    Vec3 z = { m.m[8], m.m[9], m.m[10] };

    if (Length(x) <= 1e-6f) {
        x = { 1.0f, 0.0f, 0.0f };
    }
    x = Normalize(x);

    y = y - x * Dot(x, y);
    if (Length(y) <= 1e-6f) {
        y = Cross({ 0.0f, 0.0f, 1.0f }, x);
        if (Length(y) <= 1e-6f) {
            y = Cross({ 0.0f, 1.0f, 0.0f }, x);
        }
    }
    y = Normalize(y);

    z = Cross(x, y);
    if (Length(z) <= 1e-6f) {
        z = { 0.0f, 0.0f, 1.0f };
    }
    z = Normalize(z);

    y = Normalize(Cross(z, x));

    Mat4 out = Mat4::Identity();
    out.m[0] = x.x;
    out.m[1] = x.y;
    out.m[2] = x.z;
    out.m[4] = y.x;
    out.m[5] = y.y;
    out.m[6] = y.z;
    out.m[8] = z.x;
    out.m[9] = z.y;
    out.m[10] = z.z;
    out.m[12] = m.m[12];
    out.m[13] = m.m[13];
    out.m[14] = m.m[14];
    return out;
}

Quaternion QuaternionFromMatrix(const Mat4& m) {
    Quaternion q{};
    const float trace = m.m[0] + m.m[5] + m.m[10];

    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m.m[6] - m.m[9]) / s;
        q.y = (m.m[8] - m.m[2]) / s;
        q.z = (m.m[1] - m.m[4]) / s;
    }
    else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
        const float s = std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
        q.w = (m.m[6] - m.m[9]) / s;
        q.x = 0.25f * s;
        q.y = (m.m[4] + m.m[1]) / s;
        q.z = (m.m[8] + m.m[2]) / s;
    }
    else if (m.m[5] > m.m[10]) {
        const float s = std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
        q.w = (m.m[8] - m.m[2]) / s;
        q.x = (m.m[4] + m.m[1]) / s;
        q.y = 0.25f * s;
        q.z = (m.m[9] + m.m[6]) / s;
    }
    else {
        const float s = std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
        q.w = (m.m[1] - m.m[4]) / s;
        q.x = (m.m[8] + m.m[2]) / s;
        q.y = (m.m[9] + m.m[6]) / s;
        q.z = 0.25f * s;
    }

    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-6f) {
        const float inv = 1.0f / len;
        q.x *= inv;
        q.y *= inv;
        q.z *= inv;
        q.w *= inv;
    }
    else {
        q = {};
    }

    return q;
}

Quaternion NormalizeQuaternion(const Quaternion& qIn) {
    Quaternion q = qIn;
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-6f) {
        const float inv = 1.0f / len;
        q.x *= inv;
        q.y *= inv;
        q.z *= inv;
        q.w *= inv;
    }
    else {
        q = {};
    }
    return q;
}

Vec3 EulerDegreesFromQuaternion(const Quaternion& qIn) {
    const Quaternion q = NormalizeQuaternion(qIn);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;

    const float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    const float cosr_cosp = 1.0f - 2.0f * (xx + yy);
    const float roll = std::atan2(sinr_cosp, cosr_cosp);

    const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    const float pitch = (std::fabs(sinp) >= 1.0f)
        ? std::copysign(1.57079632679f, sinp)
        : std::asin(sinp);

    const float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    const float cosy_cosp = 1.0f - 2.0f * (yy + zz);
    const float yaw = std::atan2(siny_cosp, cosy_cosp);

    constexpr float kRadToDeg = 57.29577951308232f;
    return {
        roll * kRadToDeg,
        pitch * kRadToDeg,
        yaw * kRadToDeg
    };
}

Quaternion QuaternionFromEulerDegrees(const Vec3& eulerDegrees) {
    constexpr float kDegToRad = 0.01745329251994329577f;
    const float roll = eulerDegrees.x * kDegToRad;
    const float pitch = eulerDegrees.y * kDegToRad;
    const float yaw = eulerDegrees.z * kDegToRad;

    const float cy = std::cos(yaw * 0.5f);
    const float sy = std::sin(yaw * 0.5f);
    const float cp = std::cos(pitch * 0.5f);
    const float sp = std::sin(pitch * 0.5f);
    const float cr = std::cos(roll * 0.5f);
    const float sr = std::sin(roll * 0.5f);

    Quaternion q{};
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return NormalizeQuaternion(q);
}

bool RigidTransformsNearlyEqual(
    const Mat4& a,
    const Mat4& b,
    float epsilon = 1.0e-4f)
{
    constexpr int indices[] = {
        0, 1, 2,
        4, 5, 6,
        8, 9, 10,
        12, 13, 14
    };
    for (const int index : indices) {
        if (std::fabs(a.m[index] - b.m[index]) > epsilon) {
            return false;
        }
    }
    return true;
}

} // namespace

RenderViewportWidget::RenderViewportWidget(QWidget* parent)
    : QWidget(parent)
    , m_backend(std::make_unique<D3D11RenderBackend>())
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_camera.yaw = DegToRad(45.0f);
    m_camera.pitch = DegToRad(-22.0f);
    m_camera.distance = 24.0f;
    m_camera.fovDeg = 60.0f;
    m_camera.nearPlane = 0.1f;
    m_camera.farPlane = 10000.0f;
    SyncCameraInspectorStateFromCamera();

    m_settings.profile = ParityProfile::W3DViewD3D11Baseline;
    m_settings.enableFog = true;
    m_settings.enableLod = true;
    m_settings.lodBias = 1.0f;

    m_frameTimer = new QTimer(this);
    m_frameTimer->setInterval(16);
    connect(m_frameTimer, &QTimer::timeout, this, &RenderViewportWidget::TickFrame);

    m_statsTimer.start();
    m_deltaTimer.start();

    EmitSelectionStatus(QStringLiteral("Selection: none"));
}

RenderViewportWidget::~RenderViewportWidget() {
    if (m_frameTimer) {
        m_frameTimer->stop();
    }

    ShutdownImGui();

    if (m_backend) {
        m_backend->Shutdown();
    }
}

QSize RenderViewportWidget::sizeHint() const {
    return { 640, 360 };
}

QSize RenderViewportWidget::minimumSizeHint() const {
    return { 320, 180 };
}

void RenderViewportWidget::SetSceneResult(const SceneBuildResult& sceneResult) {
    m_sceneResult = sceneResult;
    m_sceneDirty = true;

    ClearPivotOverrides();
    m_hiddenInstances.clear();
    m_selectedVisibleIndex = -1;
    m_selectedPivotRepresentativeVisibleIndex = -1;

    if (m_backend) {
        m_backend->SetSelectedInstance(m_selectedInstance);
        m_backend->SetTransformOverrides({});
        m_backend->SetPivotLocalOverrides({});
        m_backend->SetHiddenInstances({});
    }

    if (IsAnimationEditModeActive()
        && !m_selectedPivot.has_value()
        && !m_selectedInstance.has_value()) {
        m_autoSelectEditablePivotPending = true;
    }

    EmitWarnings();
}

void RenderViewportWidget::SetRenderSettings(const RenderSettings& settings) {
    m_settings = settings;
    if (m_backendInitialized && m_backend) {
        m_backend->SetRenderSettings(m_settings);
    }
}

void RenderViewportWidget::SetAnimationPlayback(const AnimationPlaybackState& playback) {
    m_animationPlayback = playback;
    if (m_backendInitialized && m_backend) {
        m_backend->SetAnimationPlayback(m_animationPlayback);
    }
}

void RenderViewportWidget::SetAnimationEditDraft(
    const std::optional<RenderAnimationEditDraft>& draft)
{
    m_animationEditDraft = draft;
    if (m_backendInitialized && m_backend) {
        m_backend->SetAnimationEditDraft(m_animationEditDraft);
    }
}

void RenderViewportWidget::SetCameraState(const CameraState& camera) {
    m_camera = camera;
    SyncCameraInspectorStateFromCamera();
    if (m_backendInitialized && m_backend) {
        m_backend->SetCamera(m_camera);
    }
}

void RenderViewportWidget::SetContinuousRenderingEnabled(bool enabled) {
    m_continuousRenderingEnabled = enabled;
    if (!m_frameTimer) {
        return;
    }
    if (!enabled && m_frameTimer->isActive()) {
        m_frameTimer->stop();
    }
    else if (enabled && isVisible() && !m_frameTimer->isActive()) {
        m_frameTimer->start();
    }
}

void RenderViewportWidget::SetOverlayUiEnabled(bool enabled) {
    m_overlayUiEnabled = enabled;
    if (!enabled) {
        m_gizmoOver = false;
        m_gizmoUsing = false;
        m_gizmoWasUsing = false;
        m_pendingPick = false;
    }
    else {
        EnsureBackendInitialized();
    }
}

void RenderViewportWidget::SetAnimationEditingState(
    bool editKeysEnabled,
    bool clipEditable,
    const QString& readOnlyReason)
{
    const bool changed =
        m_animationEditKeysEnabled != editKeysEnabled
        || m_animationClipEditable != clipEditable
        || m_animationClipReadOnlyReason != readOnlyReason;
    m_animationEditKeysEnabled = editKeysEnabled;
    m_animationClipEditable = clipEditable;
    m_animationClipReadOnlyReason = readOnlyReason;

    if (changed) {
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
        if (IsAnimationEditModeActive()
            && !m_selectedPivot.has_value()
            && !m_selectedInstance.has_value()) {
            m_autoSelectEditablePivotPending = true;
        }
    }
}

void RenderViewportWidget::FocusScene() {
    const Vec3 center = ComputeSceneCenter();
    const float radius = ComputeSceneRadius(center);

    m_camera.target = center;
    m_camera.distance = std::max(6.0f, radius * 2.5f);
    m_camera.nearPlane = std::max(0.1f, radius * 0.002f);
    m_camera.farPlane = std::max(1000.0f, radius * 24.0f + m_camera.distance);
    SyncCameraInspectorStateFromCamera();
}

void RenderViewportWidget::RenderOnce() {
    TickFrame();
}

bool RenderViewportWidget::CaptureCurrentFrame(QImage& outImage) {
    EnsureBackendInitialized();
    if (!m_backendInitialized || !m_backend) {
        outImage = QImage();
        return false;
    }
    return m_backend->CaptureFrame(outImage);
}

const RenderAnimationClip* RenderViewportWidget::CurrentActiveAnimationClip() const {
    const int activeIndex = m_animationPlayback.activeAnimationIndex;
    if (activeIndex < 0 || activeIndex >= static_cast<int>(m_sceneResult.scene.animations.size())) {
        return nullptr;
    }
    return &m_sceneResult.scene.animations[static_cast<std::size_t>(activeIndex)];
}

const RenderAnimationClip* RenderViewportWidget::ActiveClipForHierarchy(
    int hierarchyIndex,
    float* outAnimationFrame) const
{
    if (outAnimationFrame) {
        *outAnimationFrame = 0.0f;
    }
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        return nullptr;
    }

    float animationFrame = 0.0f;
    const RenderAnimationClip* clip = ResolveActiveAnimationClipForHierarchy(
        m_sceneResult.scene,
        m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)],
        m_animationPlayback,
        m_animationPlayback.timeSeconds,
        animationFrame,
        m_animationEditDraft);
    if (outAnimationFrame) {
        *outAnimationFrame = animationFrame;
    }
    return clip;
}

int RenderViewportWidget::CurrentAnimationFrameIndex() const {
    const RenderAnimationClip* clip = CurrentActiveAnimationClip();
    const RenderAnimationEditDraft* activeDraft =
        ResolveAnimationEditDraftForClip(m_animationEditDraft, clip);
    const uint32_t numFrames = ResolveAnimationFrameCount(clip, activeDraft);
    const float frameRate = ResolveAnimationFrameRate(clip, activeDraft);
    if (!clip || numFrames == 0u || frameRate <= 0.0f) {
        return 0;
    }

    const int maxFrame = std::max(0, static_cast<int>(numFrames) - 1);
    const float frameFloat = m_animationPlayback.timeSeconds * frameRate;
    return std::clamp(static_cast<int>(std::round(frameFloat)), 0, maxFrame);
}

bool RenderViewportWidget::IsAnimationEditModeActive() const {
    return m_animationEditKeysEnabled
        && m_animationClipEditable
        && CurrentActiveAnimationClip() != nullptr;
}

RenderViewportWidget::PivotEditability RenderViewportWidget::EvaluatePivotEditability(
    int hierarchyIndex,
    int pivotIndex,
    const ::ChunkItem* pivotsChunk) const
{
    PivotEditability out{};

    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        out.reason = tr("The selected target is not bound to a valid hierarchy.");
        return out;
    }

    const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        out.reason = tr("The selected target is not bound to a valid pivot.");
        return out;
    }

    if (m_animationEditKeysEnabled) {
        if (!m_animationClipEditable) {
            out.reason = m_animationClipReadOnlyReason.isEmpty()
                ? tr("The active animation clip is read-only.")
                : m_animationClipReadOnlyReason;
            return out;
        }

        float animationFrame = 0.0f;
        if (!ActiveClipForHierarchy(hierarchyIndex, &animationFrame)) {
            out.reason = tr("The active animation clip does not target this hierarchy.");
            return out;
        }

        out.editable = true;
        return out;
    }

    if (!pivotsChunk) {
        out.reason = tr("The selected render item does not have an editable skeleton pivot.");
        return out;
    }

    out.editable = true;
    return out;
}

Mat4 RenderViewportWidget::ComputeUnderlyingLocalTransform(int hierarchyIndex, int pivotIndex) const {
    if (hierarchyIndex < 0
        || hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        return Mat4::Identity();
    }

    const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
    if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        return Mat4::Identity();
    }

    float animationFrame = 0.0f;
    const RenderAnimationClip* activeClip = ActiveClipForHierarchy(hierarchyIndex, &animationFrame);
    const RenderAnimationEditDraft* activeDraft =
        ResolveAnimationEditDraftForClip(m_animationEditDraft, activeClip);
    return OrthonormalizeRigidTransform(ComposeAnimatedPivotLocalTransform(
        hierarchy,
        pivotIndex,
        activeClip,
        animationFrame,
        activeDraft));
}

Mat4 RenderViewportWidget::ComputeDisplayedLocalTransform(int hierarchyIndex, int pivotIndex) const {
    Mat4 local = ComputeUnderlyingLocalTransform(hierarchyIndex, pivotIndex);

    PivotKey key{};
    key.hierarchyIndex = hierarchyIndex;
    key.pivotIndex = pivotIndex;
    if (const auto overrideIt = m_pivotLocalOverrides.find(key);
        overrideIt != m_pivotLocalOverrides.end()) {
        local = overrideIt->second;
    }

    return OrthonormalizeRigidTransform(local);
}

void RenderViewportWidget::MaybePauseAnimationForEditing() {
    if (!IsAnimationEditModeActive() || !m_animationPlayback.playing) {
        return;
    }

    m_animationPlayback.playing = false;
    emit animationPlaybackPauseRequested();
}

void RenderViewportWidget::CommitDisplayedLocalTransform(
    const PivotKey& key,
    const Mat4& displayedLocal)
{
    if (key.hierarchyIndex < 0
        || key.hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        return;
    }

    const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(key.hierarchyIndex)];
    if (key.pivotIndex < 0 || key.pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        return;
    }

    MaybePauseAnimationForEditing();

    const Mat4 local = OrthonormalizeRigidTransform(displayedLocal);
    const bool hadPreviewOverride = m_pivotLocalOverrides.find(key) != m_pivotLocalOverrides.end();
    const Mat4 currentDisplayedLocal =
        ComputeDisplayedLocalTransform(key.hierarchyIndex, key.pivotIndex);
    if (!hadPreviewOverride
        && RigidTransformsNearlyEqual(local, currentDisplayedLocal))
    {
        ClearPivotOverrides();
        return;
    }

    if (IsAnimationEditModeActive()) {
        const Mat4 baseLocal = hierarchy.pivots[static_cast<std::size_t>(key.pivotIndex)].localTransform;
        const Mat4 animationLocal = OrthonormalizeRigidTransform(Multiply(Inverse(baseLocal), local));
        const Quaternion q = QuaternionFromMatrix(animationLocal);

        emit animationKeyframeCommitRequested(
            key.hierarchyIndex,
            key.pivotIndex,
            CurrentAnimationFrameIndex(),
            animationLocal.m[12],
            animationLocal.m[13],
            animationLocal.m[14],
            q.x,
            q.y,
            q.z,
            q.w);
        ClearPivotOverrides();
        return;
    }

    const ::ChunkItem* pivotsChunk = hierarchy.sourcePivotsChunk;
    if (!pivotsChunk) {
        return;
    }

    Mat4 baseCommitLocal = local;
    float animationFrame = 0.0f;
    if (const RenderAnimationClip* activeClip = ActiveClipForHierarchy(key.hierarchyIndex, &animationFrame);
        activeClip && key.pivotIndex < static_cast<int>(activeClip->pivots.size())) {
        const Mat4 animationLocal = BuildPivotAnimationLocalTransform(activeClip, key.pivotIndex, animationFrame);
        baseCommitLocal = OrthonormalizeRigidTransform(Multiply(local, Inverse(animationLocal)));
    }

    const Quaternion q = QuaternionFromMatrix(baseCommitLocal);
    emit pivotTransformCommitRequested(
        const_cast<::ChunkItem*>(pivotsChunk),
        key.pivotIndex,
        baseCommitLocal.m[12],
        baseCommitLocal.m[13],
        baseCommitLocal.m[14],
        q.x,
        q.y,
        q.z,
        q.w);

    ClearPivotOverrides();
}

int RenderViewportWidget::FindRepresentativeVisibleInstance(const PivotKey& key) const {
    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        if (instance.hierarchyIndex == key.hierarchyIndex && instance.pivotIndex == key.pivotIndex) {
            return i;
        }
    }
    return -1;
}

bool RenderViewportWidget::TryGetSelectedEditablePivot(
    PivotKey& outKey,
    const ::ChunkItem*& outPivotsChunk,
    int& outRepresentativeVisibleIndex,
    Mat4& outLocal,
    PivotEditability& outEditability,
    QString* outError) const
{
    outPivotsChunk = nullptr;
    outRepresentativeVisibleIndex = -1;
    outLocal = Mat4::Identity();
    outEditability = {};

    if (!m_selectedPivot.has_value()) {
        if (outError) {
            *outError = tr("Select a render mesh or pivot first.");
        }
        return false;
    }

    outKey = *m_selectedPivot;
    outPivotsChunk = m_selectedPivotsChunk;
    outRepresentativeVisibleIndex = m_selectedPivotRepresentativeVisibleIndex;

    if (outKey.hierarchyIndex < 0
        || outKey.hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        if (outError) {
            *outError = tr("The selected render item is not bound to a valid hierarchy.");
        }
        return false;
    }

    const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(outKey.hierarchyIndex)];
    if (outKey.pivotIndex < 0
        || outKey.pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        if (outError) {
            *outError = tr("The selected render item is not bound to a valid pivot.");
        }
        return false;
    }

    outLocal = ComputeDisplayedLocalTransform(outKey.hierarchyIndex, outKey.pivotIndex);
    outEditability = EvaluatePivotEditability(
        outKey.hierarchyIndex,
        outKey.pivotIndex,
        outPivotsChunk);

    if (!outEditability.editable && outError) {
        *outError = outEditability.reason;
    }

    return true;
}

void RenderViewportWidget::OpenManualPivotRotationDialog() {
    PivotKey key{};
    const ::ChunkItem* pivotsChunk = nullptr;
    int representativeVisibleIndex = -1;
    Mat4 local = Mat4::Identity();
    PivotEditability editability{};
    QString errorText;
    if (!TryGetSelectedEditablePivot(
        key,
        pivotsChunk,
        representativeVisibleIndex,
        local,
        editability,
        &errorText)) {
        QMessageBox::information(
            this,
            tr("No Editable Pivot"),
            errorText.isEmpty() ? tr("Select a render mesh or pivot first.") : errorText);
        return;
    }

    if (!editability.editable) {
        QMessageBox::information(
            this,
            tr("Read-Only Selection"),
            editability.reason.isEmpty() ? tr("The selected render item is read-only.") : editability.reason);
        return;
    }

    const Quaternion currentRotation = QuaternionFromMatrix(local);
    const Vec3 currentEulerDegrees = EulerDegreesFromQuaternion(currentRotation);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Set Pivot Rotation"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* noteLabel = new QLabel(
        tr("Local axes: X = forward, Y = right, Z = up.\n"
           "To make the model face you, adjust Z rotation."),
        &dialog);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    auto* formLayout = new QFormLayout();
    auto* xSpin = new QDoubleSpinBox(&dialog);
    auto* ySpin = new QDoubleSpinBox(&dialog);
    auto* zSpin = new QDoubleSpinBox(&dialog);
    for (QDoubleSpinBox* spin : { xSpin, ySpin, zSpin }) {
        spin->setDecimals(2);
        spin->setRange(-3600.0, 3600.0);
        spin->setSingleStep(5.0);
    }
    xSpin->setValue(currentEulerDegrees.x);
    ySpin->setValue(currentEulerDegrees.y);
    zSpin->setValue(currentEulerDegrees.z);
    formLayout->addRow(tr("Rotation X (deg)"), xSpin);
    formLayout->addRow(tr("Rotation Y (deg)"), ySpin);
    formLayout->addRow(tr("Rotation Z (deg)"), zSpin);
    layout->addLayout(formLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const Vec3 newEulerDegrees{
        static_cast<float>(xSpin->value()),
        static_cast<float>(ySpin->value()),
        static_cast<float>(zSpin->value())
    };
    const Quaternion newRotation = QuaternionFromEulerDegrees(newEulerDegrees);
    const Mat4 committedLocal = TransformFromTranslationRotation(
        { local.m[12], local.m[13], local.m[14] },
        newRotation.x,
        newRotation.y,
        newRotation.z,
        newRotation.w);

    CommitDisplayedLocalTransform(key, committedLocal);
}

void RenderViewportWidget::SyncTransformInspectorState(const PivotKey& key, const Mat4& local) {
    if (key.hierarchyIndex < 0 || key.pivotIndex < 0) {
        m_transformInspectorHasSelection = false;
        return;
    }

    if (!m_transformInspectorHasSelection || !(m_transformInspectorPivot == key)) {
        m_transformInspectorHasSelection = true;
        m_transformInspectorPivot = key;
        m_transformInspectorTranslation = { local.m[12], local.m[13], local.m[14] };
        m_transformInspectorRotationDegrees =
            EulerDegreesFromQuaternion(QuaternionFromMatrix(local));
    }
}

void RenderViewportWidget::ApplyTransformInspectorEdits() {
    PivotKey key{};
    const ::ChunkItem* pivotsChunk = nullptr;
    int representativeVisibleIndex = -1;
    Mat4 local = Mat4::Identity();
    PivotEditability editability{};
    QString errorText;
    if (!TryGetSelectedEditablePivot(
        key,
        pivotsChunk,
        representativeVisibleIndex,
        local,
        editability,
        &errorText)) {
        QMessageBox::information(
            this,
            tr("No Editable Pivot"),
            errorText.isEmpty() ? tr("Select a render mesh or pivot first.") : errorText);
        return;
    }

    if (!editability.editable) {
        QMessageBox::information(
            this,
            tr("Read-Only Selection"),
            editability.reason.isEmpty() ? tr("The selected render item is read-only.") : editability.reason);
        return;
    }

    const Quaternion newRotation = QuaternionFromEulerDegrees(m_transformInspectorRotationDegrees);
    Mat4 committedLocal = TransformFromTranslationRotation(
        m_transformInspectorTranslation,
        newRotation.x,
        newRotation.y,
        newRotation.z,
        newRotation.w);
    const bool hadPreviewOverride = m_pivotLocalOverrides.find(key) != m_pivotLocalOverrides.end();
    if (!hadPreviewOverride
        && RigidTransformsNearlyEqual(
            OrthonormalizeRigidTransform(committedLocal),
            local))
    {
        return;
    }
    CommitDisplayedLocalTransform(key, committedLocal);
}

void RenderViewportWidget::PreviewTransformInspectorEdits() {
    PivotKey key{};
    const ::ChunkItem* pivotsChunk = nullptr;
    int representativeVisibleIndex = -1;
    Mat4 local = Mat4::Identity();
    PivotEditability editability{};
    if (!TryGetSelectedEditablePivot(
        key,
        pivotsChunk,
        representativeVisibleIndex,
        local,
        editability,
        nullptr)
        || !editability.editable) {
        return;
    }

    (void)pivotsChunk;
    (void)representativeVisibleIndex;
    MaybePauseAnimationForEditing();
    const Quaternion newRotation = QuaternionFromEulerDegrees(m_transformInspectorRotationDegrees);
    const Mat4 previewLocal = OrthonormalizeRigidTransform(TransformFromTranslationRotation(
        m_transformInspectorTranslation,
        newRotation.x,
        newRotation.y,
        newRotation.z,
        newRotation.w));
    const bool hadPreviewOverride = m_pivotLocalOverrides.find(key) != m_pivotLocalOverrides.end();
    if (!hadPreviewOverride && RigidTransformsNearlyEqual(previewLocal, local)) {
        return;
    }
    m_pivotLocalOverrides[key] = previewLocal;
    RebuildBackendOverrides();
}

void RenderViewportWidget::ResetTransformInspectorToOriginal() {
    PivotKey key{};
    const ::ChunkItem* pivotsChunk = nullptr;
    int representativeVisibleIndex = -1;
    Mat4 local = Mat4::Identity();
    PivotEditability editability{};
    if (!TryGetSelectedEditablePivot(
        key,
        pivotsChunk,
        representativeVisibleIndex,
        local,
        editability,
        nullptr)) {
        return;
    }

    (void)pivotsChunk;
    (void)representativeVisibleIndex;
    m_pivotLocalOverrides.erase(key);
    const Mat4 originalLocal = ComputeUnderlyingLocalTransform(key.hierarchyIndex, key.pivotIndex);
    m_transformInspectorHasSelection = true;
    m_transformInspectorPivot = key;
    m_transformInspectorTranslation = {
        originalLocal.m[12],
        originalLocal.m[13],
        originalLocal.m[14]
    };
    m_transformInspectorRotationDegrees =
        EulerDegreesFromQuaternion(QuaternionFromMatrix(originalLocal));
    RebuildBackendOverrides();
}

void RenderViewportWidget::SyncCameraInspectorStateFromCamera() {
    m_cameraInspectorTarget = m_camera.target;
    m_cameraInspectorYawDegrees = RadToDeg(m_camera.yaw);
    m_cameraInspectorPitchDegrees = RadToDeg(m_camera.pitch);
    m_cameraInspectorDistance = m_camera.distance;
}

void RenderViewportWidget::ApplyCameraInspectorEdits() {
    m_camera.target = m_cameraInspectorTarget;
    m_camera.yaw = DegToRad(m_cameraInspectorYawDegrees);
    m_camera.pitch = std::clamp(DegToRad(m_cameraInspectorPitchDegrees), -kCameraPitchLimit, kCameraPitchLimit);
    m_camera.distance = std::max(0.01f, m_cameraInspectorDistance);
}

QPaintEngine* RenderViewportWidget::paintEngine() const {
    return nullptr;
}

void RenderViewportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    EnsureBackendInitialized();
    if (m_continuousRenderingEnabled && m_frameTimer && !m_frameTimer->isActive()) {
        m_frameTimer->start();
    }
}

void RenderViewportWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (m_frameTimer && m_frameTimer->isActive()) {
        m_frameTimer->stop();
    }
}

void RenderViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    EnsureBackendInitialized();
    if (m_backendInitialized && m_backend) {
        m_backend->Resize(
            static_cast<uint32_t>(std::max(1, width())),
            static_cast<uint32_t>(std::max(1, height())));
    }
}

void RenderViewportWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (m_overlayUiEnabled && m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (event->button() == Qt::LeftButton) {
            io.AddMouseButtonEvent(0, true);
        }
        else if (event->button() == Qt::RightButton) {
            io.AddMouseButtonEvent(1, true);
        }
        else if (event->button() == Qt::MiddleButton) {
            io.AddMouseButtonEvent(2, true);
        }

        if (io.WantCaptureMouse) {
            m_dragMode = DragMode::None;
            m_pendingPick = false;
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if ((event->modifiers() & Qt::AltModifier) != 0) {
            m_dragMode = DragMode::Orbit;
        }
        else {
            m_pendingPick = !(m_gizmoOver || m_gizmoUsing);
            m_pickPressPos = event->pos();
        }
    }
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_dragMode = DragMode::Pan;
    }

    m_lastMousePos = event->pos();
    event->accept();
}

void RenderViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_overlayUiEnabled && m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (io.WantCaptureMouse && m_dragMode == DragMode::None) {
            event->accept();
            return;
        }
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_pendingPick) {
        if ((event->pos() - m_pickPressPos).manhattanLength() > 4) {
            m_pendingPick = false;
        }
    }

    if (m_dragMode == DragMode::Orbit) {
        m_camera.yaw += static_cast<float>(delta.x()) * 0.0075f;
        m_camera.pitch += static_cast<float>(delta.y()) * 0.0075f;
        m_camera.pitch = std::clamp(m_camera.pitch, -kCameraPitchLimit, kCameraPitchLimit);
    }
    else if (m_dragMode == DragMode::Pan) {
        const float panScale = std::max(0.05f, m_camera.distance * 0.0018f);
        const Vec3 forward = Normalize(CameraForward(m_camera.yaw, m_camera.pitch));
        Vec3 right = Normalize(Cross(WorldUp(), forward));
        if (Length(right) < 0.001f) {
            right = { 0.0f, 1.0f, 0.0f };
        }
        const Vec3 up = Normalize(Cross(forward, right));

        m_camera.target = m_camera.target
            + right * static_cast<float>(-delta.x()) * panScale
            + up * static_cast<float>(delta.y()) * panScale;
    }

    event->accept();
}

void RenderViewportWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_overlayUiEnabled && m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (event->button() == Qt::LeftButton) {
            io.AddMouseButtonEvent(0, false);
        }
        else if (event->button() == Qt::RightButton) {
            io.AddMouseButtonEvent(1, false);
        }
        else if (event->button() == Qt::MiddleButton) {
            io.AddMouseButtonEvent(2, false);
        }

        if (io.WantCaptureMouse && m_dragMode == DragMode::None) {
            m_pendingPick = false;
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::LeftButton) {
        if (m_dragMode == DragMode::Orbit) {
            m_dragMode = DragMode::None;
        }

        const bool shouldPick = m_pendingPick && !m_gizmoUsing && !m_gizmoOver;
        m_pendingPick = false;
        if (shouldPick) {
            PerformPick(event->pos());
        }
    }
    else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        if (m_dragMode == DragMode::Pan) {
            m_dragMode = DragMode::None;
        }
    }

    event->accept();
}

void RenderViewportWidget::wheelEvent(QWheelEvent* event) {
    if (m_overlayUiEnabled && m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        const QPoint angle = event->angleDelta();
        io.AddMouseWheelEvent(0.0f, static_cast<float>(angle.y()) / 120.0f);
        if (io.WantCaptureMouse) {
            event->accept();
            return;
        }
    }

    const QPoint angle = event->angleDelta();
    if (!angle.isNull()) {
        const float notches = static_cast<float>(angle.y()) / 120.0f;
        const float scale = std::pow(0.88f, notches);
        m_camera.distance = std::clamp(m_camera.distance * scale, 0.25f, 200000.0f);
    }
    event->accept();
}

void RenderViewportWidget::keyPressEvent(QKeyEvent* event) {
    if (m_overlayUiEnabled && m_imguiInitialized) {
        SubmitImGuiKeyboardEvent(event, true);
        const bool allowTextInput =
            (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) == 0;
        if (allowTextInput && !event->text().isEmpty()) {
            ImGui::GetIO().AddInputCharactersUTF8(event->text().toUtf8().constData());
        }
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) {
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_F) {
        FocusScene();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_B) {
        m_showSceneBrowser = !m_showSceneBrowser;
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_W) {
        m_gizmoMode = GizmoMode::Translate;
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_E) {
        m_gizmoMode = GizmoMode::Rotate;
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void RenderViewportWidget::keyReleaseEvent(QKeyEvent* event) {
    if (m_overlayUiEnabled && m_imguiInitialized) {
        SubmitImGuiKeyboardEvent(event, false);
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard || io.WantTextInput) {
            event->accept();
            return;
        }
    }

    QWidget::keyReleaseEvent(event);
}

void RenderViewportWidget::EnsureBackendInitialized() {
    if (!m_backend) {
        return;
    }

    if (!m_backendInitialized) {
        if (!windowHandle()) {
            return;
        }

        const bool ok = m_backend->Initialize(
            reinterpret_cast<void*>(winId()),
            static_cast<uint32_t>(std::max(1, width())),
            static_cast<uint32_t>(std::max(1, height())));
        if (!ok) {
            return;
        }

        m_backendInitialized = true;
        m_backend->SetRenderSettings(m_settings);
        m_backend->SetAnimationPlayback(m_animationPlayback);
        m_backend->SetAnimationEditDraft(m_animationEditDraft);
        m_backend->SetCamera(m_camera);
        m_sceneDirty = true;
        m_deltaTimer.restart();
    }

    if (m_overlayUiEnabled && !m_imguiInitialized) {
        auto* d3dBackend = dynamic_cast<D3D11RenderBackend*>(m_backend.get());
        if (d3dBackend) {
            auto* device = static_cast<ID3D11Device*>(d3dBackend->NativeDeviceHandle());
            auto* context = static_cast<ID3D11DeviceContext*>(d3dBackend->NativeDeviceContextHandle());
            if (device && context) {
                IMGUI_CHECKVERSION();
                if (ImGui::GetCurrentContext() == nullptr) {
                    ImGui::CreateContext();
                    ImGui::StyleColorsDark();
                }
                ImGuiIO& io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.LogFilename = nullptr;
                m_imguiInitialized = ImGui_ImplDX11_Init(device, context);
            }
        }
    }
}

void RenderViewportWidget::ShutdownImGui() {
    if (!m_imguiInitialized) {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    m_imguiInitialized = false;
}

void RenderViewportWidget::TickFrame() {
    EnsureBackendInitialized();
    if (!m_backendInitialized || !m_backend) {
        return;
    }

    if (m_sceneDirty) {
        m_backend->UploadScene(m_sceneResult.scene);
        m_sceneDirty = false;
    }

    m_camera.pitch = std::clamp(m_camera.pitch, -kCameraPitchLimit, kCameraPitchLimit);
    m_camera.distance = std::max(0.01f, m_camera.distance);

    Vec3 cameraPos = m_camera.target - Normalize(CameraForward(m_camera.yaw, m_camera.pitch)) * m_camera.distance;
    BuildVisibleInstances(cameraPos);
    SyncSelectedInstanceToVisibleList();

    bool viewOrTransformChanged = false;
    if (m_overlayUiEnabled && m_imguiInitialized) {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(std::max(1, width())), static_cast<float>(std::max(1, height())));
        const float dt = std::max(1.0e-4f, static_cast<float>(m_deltaTimer.nsecsElapsed()) / 1.0e9f);
        io.DeltaTime = dt;
        m_deltaTimer.restart();

        const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
        io.AddKeyEvent(ImGuiMod_Ctrl, (mods & Qt::ControlModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mods & Qt::ShiftModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mods & Qt::AltModifier) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mods & Qt::MetaModifier) != 0);

        const QPoint localMousePos = mapFromGlobal(QCursor::pos());
        io.AddMousePosEvent(static_cast<float>(localMousePos.x()), static_cast<float>(localMousePos.y()));

        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        const Mat4 view = LookAtLH(cameraPos, m_camera.target, WorldUp());
        const float aspect = static_cast<float>(std::max(1, width())) / static_cast<float>(std::max(1, height()));
        const Mat4 projection = PerspectiveFovLH(
            DegToRad(m_camera.fovDeg),
            aspect,
            m_camera.nearPlane,
            m_camera.farPlane);

        viewOrTransformChanged = HandleGizmos(view, projection);

        ImGui::Render();
    }

    if (viewOrTransformChanged) {
        cameraPos = m_camera.target - Normalize(CameraForward(m_camera.yaw, m_camera.pitch)) * m_camera.distance;
        BuildVisibleInstances(cameraPos);
        SyncSelectedInstanceToVisibleList();
    }

    RebuildBackendOverrides();

    m_backend->SetRenderSettings(m_settings);
    m_backend->SetAnimationPlayback(m_animationPlayback);
    m_backend->SetCamera(m_camera);

    if (m_overlayUiEnabled && m_imguiInitialized) {
        m_backend->RenderFrame([this]() {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        });
    }
    else {
        m_backend->RenderFrame();
    }

    if (m_statsTimer.hasExpired(200)) {
        m_statsTimer.restart();
        const FrameStats stats = m_backend->GetFrameStats();
        emit frameStatsChanged(
            QStringLiteral("Draws: %1 | Tris: %2")
            .arg(stats.drawCalls)
            .arg(stats.triangles));
    }
}

void RenderViewportWidget::EmitWarnings() {
    QStringList lines;
    lines.reserve(static_cast<int>(m_sceneResult.warnings.size()));

    for (const auto& warning : m_sceneResult.warnings) {
        QString prefix;
        switch (warning.code) {
        case SceneBuildWarningCode::MissingPayload:
            prefix = QStringLiteral("MissingPayload");
            break;
        case SceneBuildWarningCode::InvalidIndex:
            prefix = QStringLiteral("InvalidIndex");
            break;
        case SceneBuildWarningCode::MissingTexture:
            prefix = QStringLiteral("MissingTexture");
            break;
        case SceneBuildWarningCode::MissingHierarchy:
            prefix = QStringLiteral("MissingHierarchy");
            break;
        case SceneBuildWarningCode::CyclicHierarchy:
            prefix = QStringLiteral("CyclicHierarchy");
            break;
        case SceneBuildWarningCode::UnsupportedChunk:
        default:
            prefix = QStringLiteral("Unsupported");
            break;
        }

        lines.push_back(
            QStringLiteral("[%1] %2 :: %3")
            .arg(prefix,
                QString::fromStdString(warning.chunkPath),
                QString::fromStdString(warning.message)));
    }

    emit sceneWarningsChanged(lines);
}

void RenderViewportWidget::BuildVisibleInstances(const Vec3& cameraPos) {
    m_visibleInstances.clear();

    auto populateInstanceEditability = [&](VisibleInstance& instance) {
        const PivotEditability editability =
            EvaluatePivotEditability(
                instance.hierarchyIndex,
                instance.pivotIndex,
                instance.pivotsChunk);
        instance.editable = editability.editable;
        instance.readOnlyReason = editability.reason;
    };

    const auto hierarchyWorld = BuildHierarchyWorldTransforms();
    auto getWorldForBinding = [&](int hierarchyIndex, int pivotIndex, const Mat4& fallback) -> Mat4 {
        if (hierarchyIndex < 0 || hierarchyIndex >= static_cast<int>(hierarchyWorld.size())) {
            return fallback;
        }
        const auto& worlds = hierarchyWorld[static_cast<std::size_t>(hierarchyIndex)];
        if (pivotIndex < 0 || pivotIndex >= static_cast<int>(worlds.size())) {
            return fallback;
        }
        return Multiply(worlds[static_cast<std::size_t>(pivotIndex)], fallback);
    };

    for (std::size_t lodGroupIndex = 0; lodGroupIndex < m_sceneResult.scene.lodGroups.size(); ++lodGroupIndex) {
        const auto& lodGroup = m_sceneResult.scene.lodGroups[lodGroupIndex];
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
            if (entry.meshIndex < 0 || entry.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
                continue;
            }

            const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(entry.meshIndex)];
            if (mesh.hidden) {
                continue;
            }

            const Mat4 world =
                getWorldForBinding(entry.hierarchyIndex, entry.pivotIndex, entry.localTransform);
            const Vec3 worldCenter = TransformPoint(world, mesh.boundsCenter);
            const float distanceToCamera = Length(worldCenter - cameraPos);

            if (forcedEntryIndex < 0 && m_settings.enableLod) {
                const float bias = std::max(0.01f, m_settings.lodBias);
                const float minDistance = entry.minDistance * bias;
                const float maxDistance = std::isfinite(entry.maxDistance)
                    ? entry.maxDistance * bias
                    : std::numeric_limits<float>::max();
                if (distanceToCamera < minDistance || distanceToCamera >= maxDistance) {
                    continue;
                }
            }

            VisibleInstance instance{};
            instance.key.kind = RenderInstanceKind::LodEntry;
            instance.key.primaryIndex = static_cast<int>(lodGroupIndex);
            instance.key.secondaryIndex = static_cast<int>(entryIndex);
            instance.meshIndex = entry.meshIndex;
            instance.hierarchyIndex = entry.hierarchyIndex;
            instance.pivotIndex = entry.pivotIndex;
            instance.world = world;
            instance.meshChunk = mesh.sourceMeshHeaderChunk;
            instance.meshFromSupplemental = mesh.sourceFromSupplemental;
            instance.hiddenByUser = m_hiddenInstances.contains(instance.key);

            if (instance.hierarchyIndex >= 0
                && instance.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
                const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(instance.hierarchyIndex)];
                instance.pivotsChunk = hierarchy.sourcePivotsChunk;
            }

            populateInstanceEditability(instance);

            m_visibleInstances.push_back(instance);
        }
    }

    for (std::size_t nodeIndex = 0; nodeIndex < m_sceneResult.scene.looseNodes.size(); ++nodeIndex) {
        const auto& node = m_sceneResult.scene.looseNodes[nodeIndex];
        if (node.meshIndex < 0 || node.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
            continue;
        }

        const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(node.meshIndex)];
        if (mesh.hidden) {
            continue;
        }

        VisibleInstance instance{};
            instance.key.kind = RenderInstanceKind::LooseNode;
            instance.key.primaryIndex = static_cast<int>(nodeIndex);
            instance.key.secondaryIndex = -1;
            instance.meshIndex = node.meshIndex;
            instance.hierarchyIndex = node.hierarchyIndex;
            instance.pivotIndex = node.pivotIndex;
            instance.world = getWorldForBinding(node.hierarchyIndex, node.pivotIndex, node.localTransform);
            instance.meshChunk = mesh.sourceMeshHeaderChunk;
            instance.meshFromSupplemental = mesh.sourceFromSupplemental;
            instance.hiddenByUser = m_hiddenInstances.contains(instance.key);

        if (instance.hierarchyIndex >= 0
            && instance.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(instance.hierarchyIndex)];
            instance.pivotsChunk = hierarchy.sourcePivotsChunk;
        }

        populateInstanceEditability(instance);

        m_visibleInstances.push_back(instance);
    }

    AutoSelectFirstEditablePivotIfNeeded();
}

void RenderViewportWidget::RebuildBackendOverrides() {
    m_backendWorldOverrides.clear();
    std::unordered_map<RenderPivotOverrideKey, Mat4, RenderPivotOverrideKeyHash> pivotOverrides;
    pivotOverrides.reserve(m_pivotLocalOverrides.size());

    if (!m_pivotLocalOverrides.empty()) {
        for (const auto& instance : m_visibleInstances) {
            m_backendWorldOverrides[instance.key] = instance.world;
        }
        for (const auto& [key, local] : m_pivotLocalOverrides) {
            pivotOverrides.emplace(
                RenderPivotOverrideKey{ key.hierarchyIndex, key.pivotIndex },
                local);
        }
    }

    if (m_backend) {
        m_backend->SetSelectedInstance(m_selectedInstance);
        m_backend->SetTransformOverrides(m_backendWorldOverrides);
        m_backend->SetPivotLocalOverrides(pivotOverrides);
        m_backend->SetHiddenInstances(m_hiddenInstances);
    }
}

void RenderViewportWidget::PerformPick(const QPoint& pos) {
    if (m_visibleInstances.empty()) {
        SetSelectedVisibleInstance(-1, false);
        return;
    }

    Vec3 rayOrigin{};
    Vec3 rayDir{};
    if (!BuildRayFromScreen(pos, rayOrigin, rayDir)) {
        return;
    }

    float bestT = std::numeric_limits<float>::max();
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        if (instance.hiddenByUser) {
            continue;
        }
        if (instance.meshIndex < 0 || instance.meshIndex >= static_cast<int>(m_sceneResult.scene.meshes.size())) {
            continue;
        }

        const auto& mesh = m_sceneResult.scene.meshes[static_cast<std::size_t>(instance.meshIndex)];
        const Vec3 worldCenter = TransformPoint(instance.world, mesh.boundsCenter);
        if (!RayIntersectsSphere(rayOrigin, rayDir, worldCenter, std::max(0.1f, mesh.boundsRadius))) {
            continue;
        }

        for (std::size_t tri = 0; tri + 2 < mesh.indices.size(); tri += 3) {
            const uint32_t ia = mesh.indices[tri + 0];
            const uint32_t ib = mesh.indices[tri + 1];
            const uint32_t ic = mesh.indices[tri + 2];
            if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) {
                continue;
            }

            const Vec3 v0 = TransformPoint(instance.world, mesh.vertices[ia].position);
            const Vec3 v1 = TransformPoint(instance.world, mesh.vertices[ib].position);
            const Vec3 v2 = TransformPoint(instance.world, mesh.vertices[ic].position);

            float t = 0.0f;
            if (RayIntersectsTriangle(rayOrigin, rayDir, v0, v1, v2, t)) {
                if (t > 0.0f && t < bestT) {
                    bestT = t;
                    bestIndex = i;
                }
            }
        }
    }

    SetSelectedVisibleInstance(bestIndex, true);
}

void RenderViewportWidget::DrawSceneBrowserOverlay() {
    if (!m_showSceneBrowser) {
        ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("Scene Browser Toggle", nullptr, flags)) {
            if (ImGui::Button("Scene Browser")) {
                m_showSceneBrowser = true;
            }
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(360.0f, std::max(220.0f, static_cast<float>(height()) * 0.62f)),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.86f);

    if (!ImGui::Begin("Scene Browser", &m_showSceneBrowser, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const int totalInstances = static_cast<int>(m_visibleInstances.size());
    const int hiddenInstances = static_cast<int>(std::count_if(
        m_visibleInstances.begin(),
        m_visibleInstances.end(),
        [](const VisibleInstance& instance) { return instance.hiddenByUser; }));
    ImGui::Text("Scene instances: %d", totalInstances);
    ImGui::Text("Drawn: %d  Hidden: %d", totalInstances - hiddenInstances, hiddenInstances);
    bool visibilityChanged = false;
    if (hiddenInstances > 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Unhide All")) {
            m_hiddenInstances.clear();
            visibilityChanged = true;
        }
    }
    ImGui::TextUnformatted("Click a mesh row to select the gizmo target. B toggles this panel.");
    ImGui::Separator();
    ImGui::BeginChild("##SceneBrowserList", ImVec2(0.0f, 0.0f), false);

    int requestedSelection = -1;
    std::optional<PivotKey> requestedPivot;
    const ::ChunkItem* requestedPivotsChunk = nullptr;
    int requestedRepresentativeVisibleIndex = -1;

    auto hierarchyNameFor = [&](int hierarchyIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const QString name =
                QString::fromStdString(m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)].name);
            if (!name.isEmpty()) {
                return name;
            }
        }
        return QStringLiteral("Hierarchy %1").arg(hierarchyIndex);
    };

    auto pivotNameFor = [&](int hierarchyIndex, int pivotIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            if (pivotIndex >= 0 && pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                const QString name = QString::fromStdString(hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name);
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
        return QStringLiteral("Pivot %1").arg(pivotIndex);
    };

    auto meshNameFor = [&](const VisibleInstance& instance) -> QString {
        if (instance.meshIndex >= 0 && instance.meshIndex < static_cast<int>(m_sceneResult.scene.meshes.size())) {
            const QString fullName =
                QString::fromStdString(m_sceneResult.scene.meshes[static_cast<std::size_t>(instance.meshIndex)].fullName);
            if (!fullName.isEmpty()) {
                return fullName;
            }
        }
        return QStringLiteral("Mesh %1").arg(instance.meshIndex);
    };

    auto drawInstanceItem = [&](int visibleIndex) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(visibleIndex)];
        bool isVisibleInViewport = !m_hiddenInstances.contains(instance.key);
        QString label = meshNameFor(instance);
        label += (instance.key.kind == RenderInstanceKind::LodEntry)
            ? QStringLiteral(" [LOD]")
            : QStringLiteral(" [Node]");
        if (!isVisibleInViewport) {
            label += QStringLiteral(" [HIDDEN]");
        }
        if (!instance.editable) {
            label += QStringLiteral(" [RO]");
        }

        const QByteArray labelUtf8 = label.toUtf8();
        ImGui::PushID(visibleIndex);
        if (ImGui::Checkbox("##Visible", &isVisibleInViewport)) {
            if (isVisibleInViewport) {
                m_hiddenInstances.erase(instance.key);
            }
            else {
                m_hiddenInstances.insert(instance.key);
            }
            visibilityChanged = true;
        }
        ImGui::SameLine();

        const bool dimmed = !isVisibleInViewport;
        if (dimmed) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.6f);
        }
        if (!instance.editable) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.74f, 0.55f, 1.0f));
        }
        if (ImGui::Selectable(labelUtf8.constData(), visibleIndex == m_selectedVisibleIndex)) {
            requestedSelection = visibleIndex;
        }
        if (ImGui::IsItemHovered()) {
            if (!instance.readOnlyReason.isEmpty()) {
                const QByteArray reason = instance.readOnlyReason.toUtf8();
                ImGui::SetTooltip("%s", reason.constData());
            }
            else if (!isVisibleInViewport) {
                ImGui::SetTooltip("Hidden in viewport");
            }
        }
        if (!instance.editable) {
            ImGui::PopStyleColor();
        }
        if (dimmed) {
            ImGui::PopStyleVar();
        }
        ImGui::PopID();
    };

    auto requestPivotSelection = [&](int hierarchyIndex, int pivotIndex, const ::ChunkItem* pivotsChunk) {
        PivotKey key{};
        key.hierarchyIndex = hierarchyIndex;
        key.pivotIndex = pivotIndex;
        requestedPivot = key;
        requestedPivotsChunk = pivotsChunk;
        requestedRepresentativeVisibleIndex = FindRepresentativeVisibleInstance(key);
    };

    std::vector<int> looseInstances;
    std::vector<int> hierarchyIndices;
    looseInstances.reserve(m_visibleInstances.size());
    hierarchyIndices.reserve(m_sceneResult.scene.hierarchies.size());

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        const int hierarchyIndex = instance.hierarchyIndex;
        if (hierarchyIndex >= 0 && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            if (std::find(hierarchyIndices.begin(), hierarchyIndices.end(), hierarchyIndex) == hierarchyIndices.end()) {
                hierarchyIndices.push_back(hierarchyIndex);
            }
        }
        else {
            looseInstances.push_back(i);
        }
    }

    std::sort(
        hierarchyIndices.begin(),
        hierarchyIndices.end(),
        [&](int a, int b) {
            return QString::compare(hierarchyNameFor(a), hierarchyNameFor(b), Qt::CaseInsensitive) < 0;
        });

    if (looseInstances.empty() && hierarchyIndices.empty()) {
        ImGui::TextUnformatted("No instances available.");
    }

    if (!looseInstances.empty()) {
        if (ImGui::TreeNodeEx("##LooseInstances", ImGuiTreeNodeFlags_DefaultOpen, "Unbound (%d)", static_cast<int>(looseInstances.size()))) {
            for (const int visibleIndex : looseInstances) {
                drawInstanceItem(visibleIndex);
            }
            ImGui::TreePop();
        }
    }

    for (const int hierarchyIndex : hierarchyIndices) {
        std::vector<int> hierarchyInstances;
        for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
            if (m_visibleInstances[static_cast<std::size_t>(i)].hierarchyIndex == hierarchyIndex) {
                hierarchyInstances.push_back(i);
            }
        }

        const QString hierarchyLabelText = QStringLiteral("%1 (%2)")
            .arg(hierarchyNameFor(hierarchyIndex))
            .arg(static_cast<int>(hierarchyInstances.size()));
        const QByteArray hierarchyLabel = hierarchyLabelText.toUtf8();

        ImGui::PushID(hierarchyIndex);
        if (ImGui::TreeNodeEx("##HierarchyNode", ImGuiTreeNodeFlags_DefaultOpen, "%s", hierarchyLabel.constData())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            std::vector<std::vector<int>> pivotChildren(hierarchy.pivots.size());
            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                const int parent = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
                if (parent >= 0 && parent < static_cast<int>(hierarchy.pivots.size())) {
                    pivotChildren[static_cast<std::size_t>(parent)].push_back(pivotIndex);
                }
            }

            for (auto& children : pivotChildren) {
                std::sort(
                    children.begin(),
                    children.end(),
                    [&](int a, int b) {
                        return QString::compare(
                            pivotNameFor(hierarchyIndex, a),
                            pivotNameFor(hierarchyIndex, b),
                            Qt::CaseInsensitive) < 0;
                    });
            }

            std::vector<std::vector<int>> pivotInstances(hierarchy.pivots.size());
            std::vector<int> unboundInstances;
            for (const int visibleIndex : hierarchyInstances) {
                const auto& instance = m_visibleInstances[static_cast<std::size_t>(visibleIndex)];
                if (instance.pivotIndex >= 0
                    && instance.pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                    pivotInstances[static_cast<std::size_t>(instance.pivotIndex)].push_back(visibleIndex);
                }
                else {
                    unboundInstances.push_back(visibleIndex);
                }
            }

            if (!unboundInstances.empty()) {
                if (ImGui::TreeNodeEx("##HierarchyUnbound", ImGuiTreeNodeFlags_DefaultOpen, "Unbound (%d)", static_cast<int>(unboundInstances.size()))) {
                    for (const int visibleIndex : unboundInstances) {
                        drawInstanceItem(visibleIndex);
                    }
                    ImGui::TreePop();
                }
            }

            std::vector<uint8_t> drawn(hierarchy.pivots.size(), 0);
            std::function<void(int)> drawPivot = [&](int pivotIndex) {
                if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
                    return;
                }
                if (drawn[static_cast<std::size_t>(pivotIndex)] != 0) {
                    return;
                }
                drawn[static_cast<std::size_t>(pivotIndex)] = 1;

                const auto& instances = pivotInstances[static_cast<std::size_t>(pivotIndex)];
                const auto& children = pivotChildren[static_cast<std::size_t>(pivotIndex)];
                PivotKey key{};
                key.hierarchyIndex = hierarchyIndex;
                key.pivotIndex = pivotIndex;
                const PivotEditability editability =
                    EvaluatePivotEditability(
                        hierarchyIndex,
                        pivotIndex,
                        hierarchy.sourcePivotsChunk);

                const QString pivotLabelText = QStringLiteral("%1 (%2)")
                    .arg(pivotNameFor(hierarchyIndex, pivotIndex))
                    .arg(static_cast<int>(instances.size()))
                    + (editability.editable ? QString() : QStringLiteral(" [RO]"));
                const QByteArray pivotLabel = pivotLabelText.toUtf8();

                ImGui::PushID(pivotIndex);
                ImGuiTreeNodeFlags flags =
                    ImGuiTreeNodeFlags_DefaultOpen
                    | ImGuiTreeNodeFlags_OpenOnArrow
                    | ImGuiTreeNodeFlags_OpenOnDoubleClick
                    | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (instances.empty() && children.empty()) {
                    flags |= ImGuiTreeNodeFlags_Leaf;
                }
                if (m_selectedPivot.has_value() && *m_selectedPivot == key) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                if (!editability.editable) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.74f, 0.55f, 1.0f));
                }
                const bool open = ImGui::TreeNodeEx("##PivotNode", flags, "%s", pivotLabel.constData());
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    requestPivotSelection(hierarchyIndex, pivotIndex, hierarchy.sourcePivotsChunk);
                }
                if (ImGui::IsItemHovered() && !editability.reason.isEmpty()) {
                    const QByteArray reason = editability.reason.toUtf8();
                    ImGui::SetTooltip("%s", reason.constData());
                }
                if (!editability.editable) {
                    ImGui::PopStyleColor();
                }
                if (open) {
                    for (const int visibleIndex : instances) {
                        drawInstanceItem(visibleIndex);
                    }
                    for (const int childPivotIndex : children) {
                        drawPivot(childPivotIndex);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            };

            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                const int parent = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].parentIndex;
                if (parent < 0 || parent >= static_cast<int>(hierarchy.pivots.size())) {
                    drawPivot(pivotIndex);
                }
            }
            for (int pivotIndex = 0; pivotIndex < static_cast<int>(hierarchy.pivots.size()); ++pivotIndex) {
                if (drawn[static_cast<std::size_t>(pivotIndex)] == 0) {
                    drawPivot(pivotIndex);
                }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();

    if (visibilityChanged) {
        if (m_selectedPivot.has_value()) {
            SetSelectedPivot(
                *m_selectedPivot,
                m_selectedPivotsChunk,
                FindRepresentativeVisibleInstance(*m_selectedPivot),
                false);
        }
        else if (m_selectedVisibleIndex >= 0
            && m_selectedVisibleIndex < static_cast<int>(m_visibleInstances.size())) {
            SetSelectedVisibleInstance(m_selectedVisibleIndex, false);
        }
    }
    if (requestedSelection >= 0) {
        SetSelectedVisibleInstance(requestedSelection, true);
    }
    else if (requestedPivot.has_value()) {
        SetSelectedPivot(
            *requestedPivot,
            requestedPivotsChunk,
            requestedRepresentativeVisibleIndex,
            true);
    }
}

void RenderViewportWidget::DrawPivotMarkersOverlay(
    const std::vector<std::vector<Mat4>>& hierarchyWorld,
    const Mat4& viewProjection)
{
    if (!m_settings.showPivotMarkers || hierarchyWorld.empty()) {
        return;
    }

    struct PivotMarker {
        ImVec2 screen{};
        float depth = 0.0f;
        bool selected = false;
        bool editable = false;
    };

    std::vector<PivotMarker> markers;
    for (int hierarchyIndex = 0; hierarchyIndex < static_cast<int>(hierarchyWorld.size()); ++hierarchyIndex) {
        if (hierarchyIndex < 0
            || hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            continue;
        }

        const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
        const auto& worlds = hierarchyWorld[static_cast<std::size_t>(hierarchyIndex)];
        for (int pivotIndex = 0; pivotIndex < static_cast<int>(worlds.size()); ++pivotIndex) {
            const Vec3 worldPos =
                TransformPoint(worlds[static_cast<std::size_t>(pivotIndex)], { 0.0f, 0.0f, 0.0f });

            ImVec2 screen{};
            float depth = 0.0f;
            if (!ProjectWorldPointToScreen(
                viewProjection,
                worldPos,
                static_cast<float>(std::max(1, width())),
                static_cast<float>(std::max(1, height())),
                screen,
                &depth))
            {
                continue;
            }

            const PivotKey key{ hierarchyIndex, pivotIndex };
            const PivotEditability editability =
                EvaluatePivotEditability(hierarchyIndex, pivotIndex, hierarchy.sourcePivotsChunk);
            markers.push_back({
                screen,
                depth,
                m_selectedPivot.has_value() && *m_selectedPivot == key,
                editability.editable
                });
        }
    }

    if (markers.empty()) {
        return;
    }

    std::sort(
        markers.begin(),
        markers.end(),
        [](const PivotMarker& a, const PivotMarker& b) {
            return a.depth > b.depth;
        });

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    for (const PivotMarker& marker : markers) {
        const ImU32 fillColor = marker.selected
            ? IM_COL32(255, 214, 102, 220)
            : (marker.editable
                ? IM_COL32(96, 208, 255, 190)
                : IM_COL32(186, 156, 116, 170));
        const float radius = marker.selected ? 6.0f : 4.0f;
        drawList->AddCircleFilled(marker.screen, radius, fillColor, 10);
        drawList->AddCircle(marker.screen, radius + 1.5f, IM_COL32(12, 16, 24, 235), 10, 2.0f);
    }
}

void RenderViewportWidget::DrawTransformInspectorOverlay() {
    if (!m_showTransformInspector) {
        ImGui::SetNextWindowPos(
            ImVec2(std::max(12.0f, static_cast<float>(width()) - 210.0f), 12.0f),
            ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.45f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("Viewport Inspector Toggle", nullptr, flags)) {
            if (ImGui::Button("Viewport Inspector")) {
                m_showTransformInspector = true;
            }
        }
        ImGui::End();
        return;
    }

    ImGui::SetNextWindowPos(
        ImVec2(std::max(12.0f, static_cast<float>(width()) - 380.0f), 150.0f),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 430.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.9f);
    if (!ImGui::Begin("Viewport Inspector", &m_showTransformInspector, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const float currentYawDegrees = RadToDeg(m_camera.yaw);
    const float currentPitchDegrees = RadToDeg(m_camera.pitch);
    ImGui::TextUnformatted("Scene View");
    ImGui::Text("Current View: yaw %.2f  pitch %.2f  dist %.3f",
        currentYawDegrees,
        currentPitchDegrees,
        m_camera.distance);
    ImGui::Text("Current Target: X %.3f  Y %.3f  Z %.3f",
        m_camera.target.x,
        m_camera.target.y,
        m_camera.target.z);
    ImGui::TextUnformatted("Axes: X = forward, Y = right, Z = up");
    ImGui::TextUnformatted("Alt+LMB orbit, RMB/MMB pan, wheel zoom, drag cube to orbit.");

    if (ImGui::Button("Front")) {
        m_camera.yaw = DegToRad(180.0f);
        m_camera.pitch = 0.0f;
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back")) {
        m_camera.yaw = 0.0f;
        m_camera.pitch = 0.0f;
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Left")) {
        m_camera.yaw = DegToRad(90.0f);
        m_camera.pitch = 0.0f;
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Right")) {
        m_camera.yaw = DegToRad(-90.0f);
        m_camera.pitch = 0.0f;
        SyncCameraInspectorStateFromCamera();
    }
    if (ImGui::Button("Top")) {
        m_camera.yaw = DegToRad(180.0f);
        m_camera.pitch = -kCameraPitchLimit;
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Bottom")) {
        m_camera.yaw = DegToRad(180.0f);
        m_camera.pitch = kCameraPitchLimit;
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Iso")) {
        m_camera.yaw = DegToRad(-135.0f);
        m_camera.pitch = DegToRad(-35.2643897f);
        SyncCameraInspectorStateFromCamera();
    }
    ImGui::SameLine();
    if (ImGui::Button("Use Current View")) {
        SyncCameraInspectorStateFromCamera();
    }

    float cameraTarget[3] = {
        m_cameraInspectorTarget.x,
        m_cameraInspectorTarget.y,
        m_cameraInspectorTarget.z
    };
    float cameraAngles[2] = {
        m_cameraInspectorYawDegrees,
        m_cameraInspectorPitchDegrees
    };
    ImGui::InputFloat3("View Target", cameraTarget, "%.3f");
    ImGui::InputFloat2("Yaw/Pitch (deg)", cameraAngles, "%.2f");
    ImGui::InputFloat("View Distance", &m_cameraInspectorDistance, 0.1f, 1.0f, "%.3f");
    m_cameraInspectorTarget = { cameraTarget[0], cameraTarget[1], cameraTarget[2] };
    m_cameraInspectorYawDegrees = cameraAngles[0];
    m_cameraInspectorPitchDegrees = cameraAngles[1];
    if (ImGui::Button("Apply View")) {
        ApplyCameraInspectorEdits();
        SyncCameraInspectorStateFromCamera();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Mesh/Pivot Target");

    PivotKey selectedKey{};
    int representativeVisibleIndex = -1;
    Mat4 currentLocal = Mat4::Identity();
    PivotEditability editability{};
    QString selectionError;
    const ::ChunkItem* selectedPivotsChunk = nullptr;
    if (!TryGetSelectedEditablePivot(
        selectedKey,
        selectedPivotsChunk,
        representativeVisibleIndex,
        currentLocal,
        editability,
        &selectionError)) {
        m_transformInspectorHasSelection = false;
        ImGui::TextUnformatted("No render target selected.");
        ImGui::TextUnformatted("Click a mesh or pivot in the viewport or Scene Browser.");
        ImGui::End();
        return;
    }

    const VisibleInstance* representative =
        (representativeVisibleIndex >= 0
            && representativeVisibleIndex < static_cast<int>(m_visibleInstances.size()))
        ? &m_visibleInstances[static_cast<std::size_t>(representativeVisibleIndex)]
        : nullptr;
    (void)selectedPivotsChunk;

    auto meshNameFor = [&](const VisibleInstance& instance) -> QString {
        if (instance.meshIndex >= 0 && instance.meshIndex < static_cast<int>(m_sceneResult.scene.meshes.size())) {
            const QString fullName =
                QString::fromStdString(m_sceneResult.scene.meshes[static_cast<std::size_t>(instance.meshIndex)].fullName);
            if (!fullName.isEmpty()) {
                return fullName;
            }
        }
        return QStringLiteral("Mesh %1").arg(instance.meshIndex);
    };

    auto hierarchyNameFor = [&](int hierarchyIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const QString name =
                QString::fromStdString(m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)].name);
            if (!name.isEmpty()) {
                return name;
            }
        }
        return QStringLiteral("Hierarchy %1").arg(hierarchyIndex);
    };

    auto pivotNameFor = [&](int hierarchyIndex, int pivotIndex) -> QString {
        if (hierarchyIndex >= 0
            && hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(hierarchyIndex)];
            if (pivotIndex >= 0 && pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                const QString name = QString::fromStdString(hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].name);
                if (!name.isEmpty()) {
                    return name;
                }
            }
        }
        return QStringLiteral("Pivot %1").arg(pivotIndex);
    };

    const QString meshName = representative
        ? meshNameFor(*representative)
        : tr("(no bound mesh row)");
    const QString hierarchyName = hierarchyNameFor(selectedKey.hierarchyIndex);
    const QString pivotName = pivotNameFor(selectedKey.hierarchyIndex, selectedKey.pivotIndex);
    ImGui::TextWrapped("Target Mesh: %s", meshName.toUtf8().constData());
    ImGui::TextWrapped("Hierarchy: %s", hierarchyName.toUtf8().constData());
    ImGui::TextWrapped("Pivot: %s (%d)", pivotName.toUtf8().constData(), selectedKey.pivotIndex);
    ImGui::TextUnformatted(editability.editable ? "Target: editable" : "Target: read-only");
    if (!editability.reason.isEmpty()) {
        ImGui::TextWrapped("Reason: %s", editability.reason.toUtf8().constData());
    }
    if (representative && representative->hiddenByUser) {
        ImGui::TextUnformatted("Viewport state: hidden");
    }
    if (IsAnimationEditModeActive()) {
        ImGui::Text("Edit Mode: clip keys at frame %d", CurrentAnimationFrameIndex());
    }
    else if (m_animationEditKeysEnabled) {
        ImGui::TextUnformatted("Edit Mode: requested, but current clip is read-only.");
    }
    else {
        ImGui::TextUnformatted("Edit Mode: base pivot transforms.");
    }

    ImGui::Separator();
    if (ImGui::Button(m_gizmoMode == GizmoMode::Translate ? "Translate (W) [active]" : "Translate (W)")) {
        m_gizmoMode = GizmoMode::Translate;
    }
    ImGui::SameLine();
    if (ImGui::Button(m_gizmoMode == GizmoMode::Rotate ? "Rotate (E) [active]" : "Rotate (E)")) {
        m_gizmoMode = GizmoMode::Rotate;
    }

    const auto& hierarchy =
        m_sceneResult.scene.hierarchies[static_cast<std::size_t>(selectedKey.hierarchyIndex)];
    const bool rootPivot = selectedKey.pivotIndex >= 0
        && selectedKey.pivotIndex < static_cast<int>(hierarchy.pivots.size())
        && hierarchy.pivots[static_cast<std::size_t>(selectedKey.pivotIndex)].parentIndex < 0;
    SyncTransformInspectorState(selectedKey, currentLocal);

    const Vec3 currentTranslation{ currentLocal.m[12], currentLocal.m[13], currentLocal.m[14] };
    const Vec3 currentRotationDegrees =
        EulerDegreesFromQuaternion(QuaternionFromMatrix(currentLocal));
    if (m_gizmoUsing) {
        m_transformInspectorTranslation = currentTranslation;
        m_transformInspectorRotationDegrees = currentRotationDegrees;
    }

    ImGui::TextUnformatted(
        (m_gizmoMode == GizmoMode::Rotate)
        ? (rootPivot ? "Rotate Space: world (root pivot)" : "Rotate Space: local")
        : "Translate Space: world");
    ImGui::Text("Current Pos: X %.3f  Y %.3f  Z %.3f",
        currentTranslation.x,
        currentTranslation.y,
        currentTranslation.z);
    ImGui::Text("Current Rot: X %.2f  Y %.2f  Z %.2f deg",
        currentRotationDegrees.x,
        currentRotationDegrees.y,
        currentRotationDegrees.z);
    ImGui::TextUnformatted("Axes: X = forward, Y = right, Z = up");

    ImGui::Separator();
    ImGui::TextUnformatted("Direct Local Transform Edit");
    float editTranslation[3] = {
        m_transformInspectorTranslation.x,
        m_transformInspectorTranslation.y,
        m_transformInspectorTranslation.z
    };
    float editRotation[3] = {
        m_transformInspectorRotationDegrees.x,
        m_transformInspectorRotationDegrees.y,
        m_transformInspectorRotationDegrees.z
    };
    bool manualEditChanged = false;
    if (!editability.editable) {
        ImGui::BeginDisabled();
    }
    ImGui::TextUnformatted("Position");
    manualEditChanged |= ImGui::InputFloat("Pos X", &editTranslation[0], 0.01f, 0.10f, "%.3f");
    manualEditChanged |= ImGui::InputFloat("Pos Y", &editTranslation[1], 0.01f, 0.10f, "%.3f");
    manualEditChanged |= ImGui::InputFloat("Pos Z", &editTranslation[2], 0.01f, 0.10f, "%.3f");
    ImGui::TextUnformatted("Rotation (deg)");
    manualEditChanged |= ImGui::InputFloat("Rot X", &editRotation[0], 0.25f, 5.0f, "%.2f");
    manualEditChanged |= ImGui::InputFloat("Rot Y", &editRotation[1], 0.25f, 5.0f, "%.2f");
    manualEditChanged |= ImGui::InputFloat("Rot Z", &editRotation[2], 0.25f, 5.0f, "%.2f");
    if (!editability.editable) {
        ImGui::EndDisabled();
    }
    m_transformInspectorTranslation = { editTranslation[0], editTranslation[1], editTranslation[2] };
    m_transformInspectorRotationDegrees = { editRotation[0], editRotation[1], editRotation[2] };
    if (manualEditChanged) {
        PreviewTransformInspectorEdits();
    }

    if (ImGui::Button("Use Current Transform")) {
        m_transformInspectorTranslation = currentTranslation;
        m_transformInspectorRotationDegrees = currentRotationDegrees;
        PreviewTransformInspectorEdits();
    }
    ImGui::SameLine();
    if (!editability.editable) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Reset to Original")) {
        ResetTransformInspectorToOriginal();
    }
    if (!editability.editable) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!editability.editable) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Apply")) {
        ApplyTransformInspectorEdits();
    }
    if (!editability.editable) {
        ImGui::EndDisabled();
    }

    if (IsAnimationEditModeActive()) {
        ImGui::SameLine();
        if (!editability.editable) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Delete Current Key")) {
            MaybePauseAnimationForEditing();
            emit animationKeyframeDeleteRequested(
                selectedKey.hierarchyIndex,
                selectedKey.pivotIndex,
                CurrentAnimationFrameIndex());
            ClearPivotOverrides();
        }
        if (!editability.editable) {
            ImGui::EndDisabled();
        }
    }

    ImGui::End();
}

bool RenderViewportWidget::HandleGizmos(const Mat4& view, const Mat4& projection) {
    bool changed = false;

    m_gizmoOver = false;
    m_gizmoUsing = false;

    ImGuizmo::BeginFrame();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(0.0f, 0.0f, static_cast<float>(width()), static_cast<float>(height()));

    float viewMatrix[16];
    float projectionMatrix[16];
    Mat4ToFloatArray(view, viewMatrix);
    Mat4ToFloatArray(projection, projectionMatrix);

    float viewMatrixBefore[16];
    std::memcpy(viewMatrixBefore, viewMatrix, sizeof(viewMatrixBefore));
    if (m_settings.showCameraGizmo) {
        ImGuizmo::ViewManipulate(
            viewMatrix,
            m_camera.distance,
            ImVec2(static_cast<float>(std::max(0, width() - 140)), 20.0f),
            ImVec2(120.0f, 120.0f),
            0x30303080u);
    }

    DrawSceneBrowserOverlay();
    DrawTransformInspectorOverlay();

    if (std::memcmp(viewMatrixBefore, viewMatrix, sizeof(viewMatrixBefore)) != 0) {
        const Mat4 manipulatedView = Mat4FromFloatArray(viewMatrix);
        const Mat4 invView = Inverse(manipulatedView);
        const Vec3 forward = Normalize({ invView.m[8], invView.m[9], invView.m[10] });
        if (Length(forward) > 1e-4f) {
            m_camera.yaw = std::atan2(forward.y, forward.x);
            m_camera.pitch = std::asin(std::clamp(forward.z, -1.0f, 1.0f));
            changed = true;
        }
    }

    const Mat4 currentView = Mat4FromFloatArray(viewMatrix);
    const auto worlds = BuildHierarchyWorldTransforms();
    DrawPivotMarkersOverlay(worlds, Multiply(projection, currentView));

    PivotKey key{};
    const ::ChunkItem* pivotsChunk = nullptr;
    int representativeVisibleIndex = -1;
    Mat4 currentLocal = Mat4::Identity();
    PivotEditability editability{};
    if (TryGetSelectedEditablePivot(
        key,
        pivotsChunk,
        representativeVisibleIndex,
        currentLocal,
        editability,
        nullptr)
        && editability.editable
        && key.hierarchyIndex >= 0
        && key.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        (void)pivotsChunk;
        if (key.pivotIndex >= 0
            && key.hierarchyIndex < static_cast<int>(worlds.size())
            && key.pivotIndex < static_cast<int>(worlds[static_cast<std::size_t>(key.hierarchyIndex)].size())) {
            float modelMatrix[16];
            Mat4ToFloatArray(
                worlds[static_cast<std::size_t>(key.hierarchyIndex)][static_cast<std::size_t>(key.pivotIndex)],
                modelMatrix);

            const Qt::KeyboardModifiers mods = QApplication::keyboardModifiers();
            const bool snap = (mods & Qt::ControlModifier) != 0;
            float snapValues[3] = { 1.0f, 1.0f, 1.0f };
            ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
            ImGuizmo::MODE mode = ImGuizmo::WORLD;
            if (m_gizmoMode == GizmoMode::Rotate) {
                operation = ImGuizmo::ROTATE;
                mode = ImGuizmo::LOCAL;
                const auto& hierarchy =
                    m_sceneResult.scene.hierarchies[static_cast<std::size_t>(key.hierarchyIndex)];
                if (key.pivotIndex >= 0
                    && key.pivotIndex < static_cast<int>(hierarchy.pivots.size())
                    && hierarchy.pivots[static_cast<std::size_t>(key.pivotIndex)].parentIndex < 0) {
                    mode = ImGuizmo::WORLD;
                }
                snapValues[0] = 15.0f;
                snapValues[1] = 15.0f;
                snapValues[2] = 15.0f;
            }

            ImGuizmo::Manipulate(
                viewMatrix,
                projectionMatrix,
                operation,
                mode,
                modelMatrix,
                nullptr,
                snap ? snapValues : nullptr);

            m_gizmoOver = ImGuizmo::IsOver();
            m_gizmoUsing = ImGuizmo::IsUsing();

            if (m_gizmoUsing) {
                if (!m_gizmoWasUsing) {
                    MaybePauseAnimationForEditing();
                }
                Mat4 parentWorld = Mat4::Identity();
                const auto& hierarchy = m_sceneResult.scene.hierarchies[static_cast<std::size_t>(key.hierarchyIndex)];
                if (key.pivotIndex >= 0
                    && key.pivotIndex < static_cast<int>(hierarchy.pivots.size())) {
                    const int parentIndex = hierarchy.pivots[static_cast<std::size_t>(key.pivotIndex)].parentIndex;
                    if (parentIndex >= 0
                        && parentIndex < static_cast<int>(worlds[static_cast<std::size_t>(key.hierarchyIndex)].size())) {
                        parentWorld =
                            worlds[static_cast<std::size_t>(key.hierarchyIndex)][static_cast<std::size_t>(parentIndex)];
                    }
                }

                const Mat4 manipulatedWorld = Mat4FromFloatArray(modelMatrix);
                const Mat4 newLocal = OrthonormalizeRigidTransform(
                    Multiply(Inverse(parentWorld), manipulatedWorld));

                m_pivotLocalOverrides[key] = newLocal;
                changed = true;
            }
        }
    }

    if (!m_gizmoUsing && m_gizmoWasUsing) {
        CommitPivotOverrideIfNeeded();
        changed = true;
    }

    m_gizmoWasUsing = m_gizmoUsing;
    return changed;
}

void RenderViewportWidget::CommitPivotOverrideIfNeeded() {
    if (!m_selectedPivot.has_value()) {
        ClearPivotOverrides();
        return;
    }

    const PivotKey key = *m_selectedPivot;
    const auto it = m_pivotLocalOverrides.find(key);
    if (it == m_pivotLocalOverrides.end()) {
        ClearPivotOverrides();
        return;
    }

    const Mat4 local = OrthonormalizeRigidTransform(it->second);
    QTimer::singleShot(0, this, [this, key, local]() {
        CommitDisplayedLocalTransform(key, local);
    });

    ClearPivotOverrides();
}

void RenderViewportWidget::ClearPivotOverrides() {
    m_pivotLocalOverrides.clear();
    m_backendWorldOverrides.clear();
    if (m_backend) {
        m_backend->SetTransformOverrides({});
        m_backend->SetPivotLocalOverrides({});
    }
}

void RenderViewportWidget::SyncSelectedInstanceToVisibleList() {
    if (m_selectedPivot.has_value()) {
        const PivotKey key = *m_selectedPivot;
        if (key.hierarchyIndex < 0
            || key.hierarchyIndex >= static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
            m_selectedPivot.reset();
            m_selectedPivotsChunk = nullptr;
            m_selectedPivotRepresentativeVisibleIndex = -1;
            m_selectedVisibleIndex = -1;
            m_selectedInstance.reset();
            emit pivotSelectionChanged(-1, -1);
            EmitSelectionStatus(QStringLiteral("Selection: none"));
            return;
        }

        const ::ChunkItem* pivotsChunk =
            m_sceneResult.scene.hierarchies[static_cast<std::size_t>(key.hierarchyIndex)].sourcePivotsChunk;
        SetSelectedPivot(
            key,
            pivotsChunk,
            FindRepresentativeVisibleInstance(key),
            false);
        return;
    }

    m_selectedVisibleIndex = -1;
    if (!m_selectedInstance.has_value()) {
        return;
    }

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        if (m_visibleInstances[static_cast<std::size_t>(i)].key == *m_selectedInstance) {
            SetSelectedVisibleInstance(i, false);
            break;
        }
    }
}

void RenderViewportWidget::SetSelectedVisibleInstance(int index, bool emitChunkSignal) {
    if (index < 0 || index >= static_cast<int>(m_visibleInstances.size())) {
        const bool hadPivotSelection = m_selectedPivot.has_value();
        m_selectedVisibleIndex = -1;
        m_selectedInstance.reset();
        m_selectedPivot.reset();
        m_selectedPivotsChunk = nullptr;
        m_selectedPivotRepresentativeVisibleIndex = -1;
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
        if (hadPivotSelection) {
            emit pivotSelectionChanged(-1, -1);
        }
        EmitSelectionStatus(QStringLiteral("Selection: none"));
        return;
    }

    if (m_selectedVisibleIndex != index) {
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
    }

    const auto& selected = m_visibleInstances[static_cast<std::size_t>(index)];
    if (selected.hierarchyIndex >= 0
        && selected.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())
        && selected.pivotIndex >= 0
        && selected.pivotIndex < static_cast<int>(
            m_sceneResult.scene.hierarchies[static_cast<std::size_t>(selected.hierarchyIndex)].pivots.size())) {
        PivotKey key{};
        key.hierarchyIndex = selected.hierarchyIndex;
        key.pivotIndex = selected.pivotIndex;
        SetSelectedPivot(key, selected.pivotsChunk, index, emitChunkSignal);
        return;
    }

    m_selectedVisibleIndex = index;
    m_selectedInstance = selected.key;
    const bool hadPivotSelection = m_selectedPivot.has_value();
    m_selectedPivot.reset();
    m_selectedPivotsChunk = nullptr;
    m_selectedPivotRepresentativeVisibleIndex = -1;
    if (hadPivotSelection) {
        emit pivotSelectionChanged(-1, -1);
    }

    if (emitChunkSignal && selected.meshChunk) {
        emit sceneChunkActivated(const_cast<::ChunkItem*>(selected.meshChunk));
    }

    const bool hiddenByUser = m_hiddenInstances.contains(selected.key);
    const QString meshName =
        (selected.meshIndex >= 0 && selected.meshIndex < static_cast<int>(m_sceneResult.scene.meshes.size()))
        ? QString::fromStdString(m_sceneResult.scene.meshes[static_cast<std::size_t>(selected.meshIndex)].fullName)
        : QStringLiteral("Mesh %1").arg(selected.meshIndex);
    const QString status = selected.editable
        ? QStringLiteral("Selection: %1 | editable%2")
            .arg(meshName.isEmpty() ? QStringLiteral("Unnamed Mesh") : meshName)
            .arg(hiddenByUser ? QStringLiteral(", hidden") : QString())
        : QStringLiteral("Selection: %1 | read-only (%2)%3")
            .arg(meshName.isEmpty() ? QStringLiteral("Unnamed Mesh") : meshName)
            .arg(selected.readOnlyReason)
            .arg(hiddenByUser ? QStringLiteral(", hidden") : QString());
    EmitSelectionStatus(status);
}

void RenderViewportWidget::SetSelectedPivot(
    const PivotKey& key,
    const ::ChunkItem* pivotsChunk,
    int representativeVisibleIndex,
    bool emitChunkSignal)
{
    const bool pivotChanged = !m_selectedPivot.has_value() || *m_selectedPivot != key;
    if (pivotChanged) {
        m_gizmoWasUsing = false;
        ClearPivotOverrides();
    }

    m_selectedPivot = key;
    m_selectedPivotsChunk = pivotsChunk;
    m_selectedPivotRepresentativeVisibleIndex =
        (representativeVisibleIndex >= 0 && representativeVisibleIndex < static_cast<int>(m_visibleInstances.size()))
        ? representativeVisibleIndex
        : -1;
    m_selectedVisibleIndex = m_selectedPivotRepresentativeVisibleIndex;

    const VisibleInstance* representative =
        (m_selectedPivotRepresentativeVisibleIndex >= 0)
        ? &m_visibleInstances[static_cast<std::size_t>(m_selectedPivotRepresentativeVisibleIndex)]
        : nullptr;
    if (representative) {
        m_selectedInstance = representative->key;
    }
    else {
        m_selectedInstance.reset();
    }

    if (pivotChanged) {
        emit pivotSelectionChanged(key.hierarchyIndex, key.pivotIndex);
    }

    if (emitChunkSignal) {
        if (representative && representative->meshChunk) {
            emit sceneChunkActivated(const_cast<::ChunkItem*>(representative->meshChunk));
        }
        else if (pivotsChunk) {
            emit sceneChunkActivated(const_cast<::ChunkItem*>(pivotsChunk));
        }
    }

    QString hierarchyName = QStringLiteral("Hierarchy %1").arg(key.hierarchyIndex);
    QString pivotName = QStringLiteral("Pivot %1").arg(key.pivotIndex);
    if (key.hierarchyIndex >= 0
        && key.hierarchyIndex < static_cast<int>(m_sceneResult.scene.hierarchies.size())) {
        const auto& hierarchy =
            m_sceneResult.scene.hierarchies[static_cast<std::size_t>(key.hierarchyIndex)];
        if (!hierarchy.name.empty()) {
            hierarchyName = QString::fromStdString(hierarchy.name);
        }
        if (key.pivotIndex >= 0
            && key.pivotIndex < static_cast<int>(hierarchy.pivots.size())
            && !hierarchy.pivots[static_cast<std::size_t>(key.pivotIndex)].name.empty()) {
            pivotName =
                QString::fromStdString(hierarchy.pivots[static_cast<std::size_t>(key.pivotIndex)].name);
        }
    }

    const PivotEditability editability =
        EvaluatePivotEditability(key.hierarchyIndex, key.pivotIndex, pivotsChunk);
    const bool hiddenByUser = representative && m_hiddenInstances.contains(representative->key);
    const QString meshName = representative
        ? ((representative->meshIndex >= 0
            && representative->meshIndex < static_cast<int>(m_sceneResult.scene.meshes.size()))
            ? QString::fromStdString(
                m_sceneResult.scene.meshes[static_cast<std::size_t>(representative->meshIndex)].fullName)
            : QStringLiteral("Mesh %1").arg(representative->meshIndex))
        : QStringLiteral("(pivot only)");

    const QString baseStatus = representative
        ? QStringLiteral("Selection: %1 | %2 | %3 (%4)")
            .arg(meshName.isEmpty() ? QStringLiteral("Unnamed Mesh") : meshName)
            .arg(hierarchyName)
            .arg(pivotName)
            .arg(key.pivotIndex)
        : QStringLiteral("Selection: %1 | %2 (%3)")
            .arg(hierarchyName)
            .arg(pivotName)
            .arg(key.pivotIndex);
    const QString status = editability.editable
        ? baseStatus + QStringLiteral(" | editable")
        : baseStatus + QStringLiteral(" | read-only (%1)").arg(editability.reason);
    EmitSelectionStatus(hiddenByUser ? (status + QStringLiteral(", hidden")) : status);
}

void RenderViewportWidget::EmitSelectionStatus(const QString& text) {
    emit selectionStatusChanged(text);
}

void RenderViewportWidget::AutoSelectFirstEditablePivotIfNeeded() {
    if (!m_autoSelectEditablePivotPending) {
        return;
    }
    m_autoSelectEditablePivotPending = false;

    if (m_selectedPivot.has_value() || m_selectedInstance.has_value()) {
        return;
    }

    for (int i = 0; i < static_cast<int>(m_visibleInstances.size()); ++i) {
        const auto& instance = m_visibleInstances[static_cast<std::size_t>(i)];
        if (instance.hierarchyIndex < 0
            || instance.pivotIndex < 0
            || !instance.editable) {
            continue;
        }

        PivotKey key{};
        key.hierarchyIndex = instance.hierarchyIndex;
        key.pivotIndex = instance.pivotIndex;
        SetSelectedPivot(key, instance.pivotsChunk, i, false);
        return;
    }
}

Vec3 RenderViewportWidget::ComputeSceneCenter() const {
    if (m_sceneResult.scene.meshes.empty()) {
        return { 0.0f, 0.0f, 0.0f };
    }

    Vec3 sum{};
    for (const auto& mesh : m_sceneResult.scene.meshes) {
        sum = sum + mesh.boundsCenter;
    }

    const float invCount = 1.0f / static_cast<float>(m_sceneResult.scene.meshes.size());
    return sum * invCount;
}

float RenderViewportWidget::ComputeSceneRadius(const Vec3& center) const {
    float radius = 10.0f;
    for (const auto& mesh : m_sceneResult.scene.meshes) {
        const float d = Length(mesh.boundsCenter - center) + std::max(1.0f, mesh.boundsRadius);
        radius = std::max(radius, d);
    }
    return radius;
}

bool RenderViewportWidget::BuildRayFromScreen(const QPoint& pos, Vec3& rayOrigin, Vec3& rayDir) const {
    const int w = std::max(1, width());
    const int h = std::max(1, height());
    if (w <= 0 || h <= 0) {
        return false;
    }

    const float ndcX = (2.0f * (static_cast<float>(pos.x()) + 0.5f) / static_cast<float>(w)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * (static_cast<float>(pos.y()) + 0.5f) / static_cast<float>(h));

    const Vec3 forward = Normalize(CameraForward(m_camera.yaw, m_camera.pitch));
    Vec3 right = Normalize(Cross(WorldUp(), forward));
    if (Length(right) < 0.001f) {
        right = { 0.0f, 1.0f, 0.0f };
    }
    const Vec3 up = Normalize(Cross(forward, right));

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float tanHalfFov = std::tan(DegToRad(m_camera.fovDeg) * 0.5f);

    rayDir = Normalize(
        forward
        + right * (ndcX * aspect * tanHalfFov)
        + up * (ndcY * tanHalfFov));
    rayOrigin = m_camera.target - forward * m_camera.distance;
    return true;
}

bool RenderViewportWidget::RayIntersectsSphere(
    const Vec3& rayOrigin,
    const Vec3& rayDir,
    const Vec3& center,
    float radius) const
{
    const Vec3 toCenter = center - rayOrigin;
    const float projection = Dot(toCenter, rayDir);
    const float distSq = Dot(toCenter, toCenter) - projection * projection;
    return distSq <= (radius * radius);
}

bool RenderViewportWidget::RayIntersectsTriangle(
    const Vec3& rayOrigin,
    const Vec3& rayDir,
    const Vec3& v0,
    const Vec3& v1,
    const Vec3& v2,
    float& outT) const
{
    constexpr float kEpsilon = 1.0e-6f;
    const Vec3 edge1 = v1 - v0;
    const Vec3 edge2 = v2 - v0;
    const Vec3 pvec = Cross(rayDir, edge2);
    const float det = Dot(edge1, pvec);

    if (std::fabs(det) < kEpsilon) {
        return false;
    }

    const float invDet = 1.0f / det;
    const Vec3 tvec = rayOrigin - v0;
    const float u = Dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const Vec3 qvec = Cross(tvec, edge1);
    const float v = Dot(rayDir, qvec) * invDet;
    if (v < 0.0f || (u + v) > 1.0f) {
        return false;
    }

    const float t = Dot(edge2, qvec) * invDet;
    if (t <= kEpsilon) {
        return false;
    }

    outT = t;
    return true;
}

std::vector<std::vector<Mat4>> RenderViewportWidget::BuildHierarchyWorldTransforms() const {
    return BuildDisplayHierarchyWorldTransforms(
        m_sceneResult.scene,
        m_animationPlayback,
        m_animationPlayback.timeSeconds,
        m_animationEditDraft,
        [this](int hierarchyIndex, int pivotIndex, const Mat4&) -> std::optional<Mat4> {
            PivotKey key{};
            key.hierarchyIndex = hierarchyIndex;
            key.pivotIndex = pivotIndex;
            if (const auto it = m_pivotLocalOverrides.find(key);
                it != m_pivotLocalOverrides.end()) {
                return OrthonormalizeRigidTransform(it->second);
            }
            return std::nullopt;
        });
}

} // namespace OW3D::Render
