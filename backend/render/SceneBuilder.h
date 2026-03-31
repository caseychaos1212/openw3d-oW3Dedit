#pragma once

#include <memory>
#include <vector>

#include "RenderScene.h"
#include "../ChunkItem.h"

namespace OW3D::Render {

using W3DChunk = std::vector<std::shared_ptr<ChunkItem>>;

SceneBuildResult BuildRenderScene(
    const W3DChunk& primaryRoots,
    const SceneBuildOptions& options,
    const W3DChunk* skeletonSupplementalRoots = nullptr,
    const W3DChunk* animationLibraryRoots = nullptr,
    const W3DChunk* referenceOnlyRoots = nullptr);

} // namespace OW3D::Render
