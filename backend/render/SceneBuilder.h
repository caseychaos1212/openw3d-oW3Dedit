#pragma once

#include <memory>
#include <vector>

#include "RenderScene.h"
#include "../ChunkItem.h"

namespace OW3D::Render {

using W3DChunk = std::vector<std::shared_ptr<ChunkItem>>;

SceneBuildResult BuildRenderScene(
    const W3DChunk& root,
    const SceneBuildOptions& options,
    const W3DChunk* supplementalRoot = nullptr);

} // namespace OW3D::Render
