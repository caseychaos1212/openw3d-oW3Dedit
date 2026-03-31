#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

#include "RenderScene.h"

namespace OW3D::Render {

inline float AnimationDotQuat(const Vec4& a, const Vec4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

inline Vec4 AnimationNormalizeQuat(Vec4 q) {
    const float len = std::sqrt(AnimationDotQuat(q, q));
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

inline Vec4 AnimationSlerpQuat(Vec4 a, Vec4 b, float t) {
    a = AnimationNormalizeQuat(a);
    b = AnimationNormalizeQuat(b);

    float cosTheta = AnimationDotQuat(a, b);
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
        return AnimationNormalizeQuat(out);
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

inline float SampleAnimationFloatTrack(const std::vector<RenderFloatKeyframe>& track, float frame) {
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

inline Vec4 SampleAnimationQuatTrack(const std::vector<RenderQuatKeyframe>& track, float frame) {
    if (track.empty()) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    if (track.size() == 1 || frame <= track.front().frame) {
        return AnimationNormalizeQuat(track.front().value);
    }

    for (std::size_t i = 0; i + 1u < track.size(); ++i) {
        const auto& current = track[i];
        const auto& next = track[i + 1u];
        if (frame > next.frame) {
            continue;
        }
        if (current.hold || next.frame <= current.frame) {
            return AnimationNormalizeQuat(current.value);
        }

        const float t = std::clamp(
            (frame - current.frame) / std::max(1.0e-6f, next.frame - current.frame),
            0.0f,
            1.0f);
        return AnimationNormalizeQuat(AnimationSlerpQuat(current.value, next.value, t));
    }

    return AnimationNormalizeQuat(track.back().value);
}

inline float SampleDenseAnimationFloatTrack(const std::vector<float>& samples, float frame) {
    if (samples.empty()) {
        return 0.0f;
    }
    if (samples.size() == 1u || frame <= 0.0f) {
        return samples.front();
    }

    const float maxFrame = static_cast<float>(samples.size() - 1u);
    if (frame >= maxFrame) {
        return samples.back();
    }

    const int leftIndex = std::clamp(static_cast<int>(std::floor(frame)), 0, static_cast<int>(samples.size()) - 1);
    const int rightIndex = std::clamp(leftIndex + 1, 0, static_cast<int>(samples.size()) - 1);
    if (rightIndex <= leftIndex) {
        return samples[static_cast<std::size_t>(leftIndex)];
    }

    const float t = std::clamp(frame - static_cast<float>(leftIndex), 0.0f, 1.0f);
    const float left = samples[static_cast<std::size_t>(leftIndex)];
    const float right = samples[static_cast<std::size_t>(rightIndex)];
    return left + (right - left) * t;
}

inline Vec4 SampleDenseAnimationQuatTrack(const std::vector<Vec4>& samples, float frame) {
    if (samples.empty()) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }
    if (samples.size() == 1u || frame <= 0.0f) {
        return AnimationNormalizeQuat(samples.front());
    }

    const float maxFrame = static_cast<float>(samples.size() - 1u);
    if (frame >= maxFrame) {
        return AnimationNormalizeQuat(samples.back());
    }

    const int leftIndex = std::clamp(static_cast<int>(std::floor(frame)), 0, static_cast<int>(samples.size()) - 1);
    const int rightIndex = std::clamp(leftIndex + 1, 0, static_cast<int>(samples.size()) - 1);
    if (rightIndex <= leftIndex) {
        return AnimationNormalizeQuat(samples[static_cast<std::size_t>(leftIndex)]);
    }

    const float t = std::clamp(frame - static_cast<float>(leftIndex), 0.0f, 1.0f);
    return AnimationNormalizeQuat(AnimationSlerpQuat(
        samples[static_cast<std::size_t>(leftIndex)],
        samples[static_cast<std::size_t>(rightIndex)],
        t));
}

inline const RenderDensePivotAnimationSamples* FindAnimationEditDraftPivotSamples(
    const RenderAnimationEditDraft* draft,
    int pivotIndex)
{
    if (!draft || pivotIndex < 0) {
        return nullptr;
    }

    const auto it = draft->pivotSamples.find(pivotIndex);
    if (it == draft->pivotSamples.end()) {
        return nullptr;
    }
    return &it->second;
}

inline const RenderAnimationEditDraft* ResolveAnimationEditDraftForClip(
    const std::optional<RenderAnimationEditDraft>& draft,
    const RenderAnimationClip* clip)
{
    if (!draft.has_value() || !clip) {
        return nullptr;
    }
    if (!draft->sourceAnimationChunk || !clip->sourceAnimationChunk) {
        return nullptr;
    }
    if (draft->sourceAnimationChunk != clip->sourceAnimationChunk) {
        return nullptr;
    }
    if (draft->numFrames != clip->numFrames) {
        return nullptr;
    }
    return &*draft;
}

inline Vec3 SamplePivotAnimationTranslation(
    const RenderAnimationClip* clip,
    int pivotIndex,
    float animationFrame,
    const RenderAnimationEditDraft* draft = nullptr)
{
    if (const auto* samples = FindAnimationEditDraftPivotSamples(draft, pivotIndex)) {
        return {
            SampleDenseAnimationFloatTrack(samples->translationX, animationFrame),
            SampleDenseAnimationFloatTrack(samples->translationY, animationFrame),
            SampleDenseAnimationFloatTrack(samples->translationZ, animationFrame)
        };
    }

    if (!clip
        || pivotIndex < 0
        || pivotIndex >= static_cast<int>(clip->pivots.size())) {
        return { 0.0f, 0.0f, 0.0f };
    }

    const auto& animPivot = clip->pivots[static_cast<std::size_t>(pivotIndex)];
    return {
        SampleAnimationFloatTrack(animPivot.translationX, animationFrame),
        SampleAnimationFloatTrack(animPivot.translationY, animationFrame),
        SampleAnimationFloatTrack(animPivot.translationZ, animationFrame)
    };
}

inline Vec4 SamplePivotAnimationRotation(
    const RenderAnimationClip* clip,
    int pivotIndex,
    float animationFrame,
    const RenderAnimationEditDraft* draft = nullptr)
{
    if (const auto* samples = FindAnimationEditDraftPivotSamples(draft, pivotIndex)) {
        return SampleDenseAnimationQuatTrack(samples->rotation, animationFrame);
    }

    if (!clip
        || pivotIndex < 0
        || pivotIndex >= static_cast<int>(clip->pivots.size())) {
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    }

    return SampleAnimationQuatTrack(
        clip->pivots[static_cast<std::size_t>(pivotIndex)].rotation,
        animationFrame);
}

inline RenderDensePivotAnimationSamples BuildDensePivotAnimationSamples(
    const RenderAnimationClip& clip,
    int pivotIndex,
    uint32_t numFrames,
    const RenderAnimationEditDraft* draft = nullptr)
{
    if (const auto* draftSamples =
        FindAnimationEditDraftPivotSamples(draft, pivotIndex);
        draftSamples
        && draftSamples->translationX.size() == numFrames
        && draftSamples->translationY.size() == numFrames
        && draftSamples->translationZ.size() == numFrames
        && draftSamples->rotation.size() == numFrames)
    {
        return *draftSamples;
    }

    RenderDensePivotAnimationSamples samples{};
    samples.translationX.resize(numFrames, 0.0f);
    samples.translationY.resize(numFrames, 0.0f);
    samples.translationZ.resize(numFrames, 0.0f);
    samples.rotation.resize(numFrames, Vec4{ 0.0f, 0.0f, 0.0f, 1.0f });

    const RenderPivotAnimation* pivot =
        (pivotIndex >= 0 && pivotIndex < static_cast<int>(clip.pivots.size()))
        ? &clip.pivots[static_cast<std::size_t>(pivotIndex)]
        : nullptr;

    for (uint32_t frame = 0; frame < numFrames; ++frame) {
        const float frameValue = static_cast<float>(frame);
        if (!pivot) {
            continue;
        }

        samples.translationX[frame] =
            SampleAnimationFloatTrack(pivot->translationX, frameValue);
        samples.translationY[frame] =
            SampleAnimationFloatTrack(pivot->translationY, frameValue);
        samples.translationZ[frame] =
            SampleAnimationFloatTrack(pivot->translationZ, frameValue);
        samples.rotation[frame] =
            SampleAnimationQuatTrack(pivot->rotation, frameValue);
    }

    return samples;
}

inline float AnimationFrameFromNormalizedProgress(uint32_t numFrames, float progress) {
    if (numFrames <= 1u) {
        return 0.0f;
    }
    return std::clamp(progress, 0.0f, 1.0f) * static_cast<float>(numFrames - 1u);
}

inline Mat4 TransposeMatrix(const Mat4& in) {
    Mat4 out{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out.m[row * 4 + col] = in.m[col * 4 + row];
        }
    }
    return out;
}

inline Mat4 MatrixToTTSkinSpace(const Mat4& standardMatrix) {
    return TransposeMatrix(standardMatrix);
}

inline Mat4 MatrixFromTTSkinSpace(const Mat4& ttSkinMatrix) {
    return TransposeMatrix(ttSkinMatrix);
}

inline Mat4 TTSkinIdentity() {
    return Mat4::Identity();
}

inline Mat4 TTSkinQuaternionToMatrix(float x, float y, float z, float w) {
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

inline Mat4 TTSkinTransformFromTranslationRotation(const Vec3& translation, const Vec4& rotation) {
    Mat4 out = TTSkinQuaternionToMatrix(rotation.x, rotation.y, rotation.z, rotation.w);
    out.m[3] = translation.x;
    out.m[7] = translation.y;
    out.m[11] = translation.z;
    return out;
}

inline Mat4 TTSkinMultiply(const Mat4& a, const Mat4& b) {
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

inline Vec3 TTSkinTransformPoint(const Mat4& m, const Vec3& p) {
    return {
        m.m[0] * p.x + m.m[1] * p.y + m.m[2] * p.z + m.m[3],
        m.m[4] * p.x + m.m[5] * p.y + m.m[6] * p.z + m.m[7],
        m.m[8] * p.x + m.m[9] * p.y + m.m[10] * p.z + m.m[11]
    };
}

inline Vec3 TTSkinTransformDirection(const Mat4& m, const Vec3& v) {
    return {
        m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z,
        m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z,
        m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z
    };
}

inline float WrapAnimationFrame(const RenderAnimationClip& clip, float timeSeconds) {
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

inline const RenderAnimationClip* ResolveActiveAnimationClipForHierarchy(
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

inline Mat4 BuildPivotAnimationLocalTransform(
    const RenderAnimationClip* clip,
    int pivotIndex,
    float animationFrame,
    const RenderAnimationEditDraft* draft = nullptr)
{
    if ((!clip && !draft)
        || pivotIndex < 0) {
        return Mat4::Identity();
    }

    const Vec3 translation =
        SamplePivotAnimationTranslation(clip, pivotIndex, animationFrame, draft);
    const Vec4 rotation =
        SamplePivotAnimationRotation(clip, pivotIndex, animationFrame, draft);
    return TransformFromTranslationRotation(
        translation,
        rotation.x,
        rotation.y,
        rotation.z,
        rotation.w);
}

inline Mat4 ComposeAnimatedPivotLocalTransform(
    const RenderHierarchy& hierarchy,
    int pivotIndex,
    const RenderAnimationClip* clip,
    float animationFrame,
    const RenderAnimationEditDraft* draft = nullptr)
{
    if (pivotIndex < 0 || pivotIndex >= static_cast<int>(hierarchy.pivots.size())) {
        return Mat4::Identity();
    }

    Mat4 local = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)].localTransform;
    if ((clip && pivotIndex < static_cast<int>(clip->pivots.size()))
        || FindAnimationEditDraftPivotSamples(draft, pivotIndex)) {
        local = Multiply(
            local,
            BuildPivotAnimationLocalTransform(clip, pivotIndex, animationFrame, draft));
    }
    return local;
}

inline std::vector<std::vector<Mat4>> BuildAnimatedHierarchyWorldTransforms(
    const RenderScene& scene,
    const AnimationPlaybackState& playback,
    float timeSeconds,
    const std::optional<RenderAnimationEditDraft>& animationDraft = std::nullopt,
    const std::function<std::optional<Mat4>(int, int, const Mat4&)>& localOverride = {})
{
    std::vector<std::vector<Mat4>> hierarchyWorld;
    hierarchyWorld.resize(scene.hierarchies.size());

    for (std::size_t h = 0; h < scene.hierarchies.size(); ++h) {
        const auto& hierarchy = scene.hierarchies[h];
        auto& worlds = hierarchyWorld[h];
        worlds.resize(hierarchy.pivots.size(), Mat4::Identity());

        float animationFrame = 0.0f;
        const RenderAnimationClip* activeClip =
            ResolveActiveAnimationClipForHierarchy(
                scene,
                hierarchy,
                playback,
                timeSeconds,
                animationFrame);
        const RenderAnimationEditDraft* activeDraft =
            ResolveAnimationEditDraftForClip(animationDraft, activeClip);

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
            Mat4 local = ComposeAnimatedPivotLocalTransform(
                hierarchy,
                pivotIndex,
                activeClip,
                animationFrame,
                activeDraft);

            if (localOverride) {
                if (const std::optional<Mat4> overrideLocal =
                    localOverride(static_cast<int>(h), pivotIndex, local);
                    overrideLocal.has_value())
                {
                    local = *overrideLocal;
                }
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

inline std::vector<std::vector<Mat4>> BuildCpuSkinHierarchyWorldTransforms(
    const RenderScene& scene,
    const AnimationPlaybackState& playback,
    float timeSeconds,
    const std::optional<RenderAnimationEditDraft>& animationDraft = std::nullopt,
    const std::function<std::optional<Mat4>(int, int, const Mat4&)>& localOverride = {})
{
    std::vector<std::vector<Mat4>> hierarchyWorld;
    hierarchyWorld.resize(scene.hierarchies.size());

    for (std::size_t h = 0; h < scene.hierarchies.size(); ++h) {
        const auto& hierarchy = scene.hierarchies[h];
        auto& worlds = hierarchyWorld[h];
        worlds.resize(hierarchy.pivots.size(), TTSkinIdentity());

        float animationFrame = 0.0f;
        const RenderAnimationClip* activeClip =
            ResolveActiveAnimationClipForHierarchy(
                scene,
                hierarchy,
                playback,
                timeSeconds,
                animationFrame);
        const RenderAnimationEditDraft* activeDraft =
            ResolveAnimationEditDraftForClip(animationDraft, activeClip);

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
            Mat4 local = ComposeAnimatedPivotLocalTransform(
                hierarchy,
                pivotIndex,
                activeClip,
                animationFrame,
                activeDraft);

            if (localOverride) {
                if (const std::optional<Mat4> overrideLocal =
                    localOverride(
                        static_cast<int>(h),
                        pivotIndex,
                        local);
                    overrideLocal.has_value()) {
                    local = *overrideLocal;
                }
            }

            const Mat4 ttLocal = MatrixToTTSkinSpace(local);
            const auto& pivot = hierarchy.pivots[static_cast<std::size_t>(pivotIndex)];
            const int parentIndex = pivot.parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(hierarchy.pivots.size())) {
                buildPivot(parentIndex);
                worlds[static_cast<std::size_t>(pivotIndex)] =
                    TTSkinMultiply(worlds[static_cast<std::size_t>(parentIndex)], ttLocal);
            }
            else {
                worlds[static_cast<std::size_t>(pivotIndex)] = ttLocal;
            }
            state[static_cast<std::size_t>(pivotIndex)] = 2;
        };

        for (int i = 0; i < static_cast<int>(hierarchy.pivots.size()); ++i) {
            buildPivot(i);
        }
    }

    return hierarchyWorld;
}

inline std::vector<std::vector<Mat4>> BuildDisplayHierarchyWorldTransforms(
    const RenderScene& scene,
    const AnimationPlaybackState& playback,
    float timeSeconds,
    const std::optional<RenderAnimationEditDraft>& animationDraft = std::nullopt,
    const std::function<std::optional<Mat4>(int, int, const Mat4&)>& localOverride = {})
{
    std::vector<std::vector<Mat4>> ttWorld =
        BuildCpuSkinHierarchyWorldTransforms(
            scene,
            playback,
            timeSeconds,
            animationDraft,
            localOverride);

    for (auto& worlds : ttWorld) {
        for (Mat4& world : worlds) {
            world = MatrixFromTTSkinSpace(world);
        }
    }

    return ttWorld;
}

} // namespace OW3D::Render
