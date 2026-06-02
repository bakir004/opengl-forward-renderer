#include "terrain/TerrainMeshBuilder.h"
#include "core/MeshData.h"  
#include "core/VertexLayout.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <limits>

namespace TerrainMeshBuilder
{

TerrainMeshData BuildMeshData(const TerrainHeightfield& heightfield)
{
    TerrainMeshData result;

    if (!heightfield.IsValid())
    {
        spdlog::error("[TerrainMeshBuilder] Cannot build mesh from invalid heightfield");
        return result;
    }

    const uint32_t W = heightfield.width;
    const uint32_t H = heightfield.height;
    const float worldW = heightfield.settings.worldWidth;
    const float worldH = heightfield.settings.worldHeight;

    result.vertices.resize(static_cast<size_t>(W) * H);

    glm::vec3 boundsMin( std::numeric_limits<float>::max());
    glm::vec3 boundsMax(-std::numeric_limits<float>::max());

    for (uint32_t z = 0; z < H; ++z)
    {
        for (uint32_t x = 0; x < W; ++x)
        {
            const TerrainSample& sample = heightfield.At(x, z);

           
            const float wx = (static_cast<float>(x) / static_cast<float>(W - 1) - 0.5f) * worldW;
            const float wy = sample.height;
            const float wz = (static_cast<float>(z) / static_cast<float>(H - 1) - 0.5f) * worldH;

            TerrainVertex& v = result.vertices[z * W + x];
            v.position = {wx, wy, wz};
            v.normal   = sample.normal;

            // UV: [0..1] across the full grid
            v.uv = {
                static_cast<float>(x) / static_cast<float>(W - 1),
                static_cast<float>(z) / static_cast<float>(H - 1)
            };

            const uint32_t xl = x > 0     ? x - 1 : x;
            const uint32_t xr = x < W - 1 ? x + 1 : x;
            const float dX = (static_cast<float>(xr - xl)) *
                             (worldW / static_cast<float>(W - 1));
            const float dH = heightfield.At(xr, z).height -
                             heightfield.At(xl, z).height;
            glm::vec3 tangent = glm::normalize(glm::vec3(dX, dH, 0.0f));

            tangent = glm::normalize(tangent - sample.normal * glm::dot(tangent, sample.normal));

            v.tangent = glm::vec4(tangent, 1.0f); // handedness +1

            
            v.materialZone     = static_cast<float>(sample.materialZone);
            v.mountainMask     = sample.mountainMask;
            v.grassSuitability = sample.grassSuitability;
            v.treeSuitability  = sample.treeSuitability;

            boundsMin = glm::min(boundsMin, v.position);
            boundsMax = glm::max(boundsMax, v.position);
        }
    }

    // Indices: two CCW triangles per quad cell.
    // For each quad with corners (x,z), (x+1,z), (x+1,z+1), (x,z+1):
    //   tri 0: (x,z)   -> (x,z+1)   -> (x+1,z)
    //   tri 1: (x+1,z) -> (x,z+1)   -> (x+1,z+1)
    // This winding produces upward-facing normals for a Y-up terrain.
    const uint32_t quadCountX = W - 1;
    const uint32_t quadCountZ = H - 1;
    result.indices.resize(static_cast<size_t>(quadCountX) * quadCountZ * 6);

    size_t idx = 0;
    for (uint32_t z = 0; z < quadCountZ; ++z)
    {
        for (uint32_t x = 0; x < quadCountX; ++x)
        {
            const uint32_t i00 = z       * W + x;
            const uint32_t i10 = z       * W + (x + 1);
            const uint32_t i01 = (z + 1) * W + x;
            const uint32_t i11 = (z + 1) * W + (x + 1);

            result.indices[idx++] = i00;
            result.indices[idx++] = i01;
            result.indices[idx++] = i10;

            result.indices[idx++] = i10;
            result.indices[idx++] = i01;
            result.indices[idx++] = i11;
        }
    }

    result.bounds.min = boundsMin;
    result.bounds.max = boundsMax;

    spdlog::info("[TerrainMeshBuilder] Built terrain mesh: {} vertices, {} indices, "
                 "bounds [{:.1f},{:.1f},{:.1f}] - [{:.1f},{:.1f},{:.1f}]",
                 result.VertexCount(), result.IndexCount(),
                 boundsMin.x, boundsMin.y, boundsMin.z,
                 boundsMax.x, boundsMax.y, boundsMax.z);

    return result;
}

std::unique_ptr<MeshBuffer> UploadMeshData(const TerrainMeshData& meshData)
{
    if (!meshData.IsValid())
    {
        spdlog::error("[TerrainMeshBuilder] Cannot upload invalid TerrainMeshData");
        return nullptr;
    }

    // TerrainVertex packs: position(vec3), normal(vec3), uv(vec2), tangent(vec4),
    // then four extra floats for per-vertex mask data.
    // The renderer only needs locations 0-3 to match mesh.vert; the extra floats
    // sit after tangent in memory and are ignored by the standard shader.
    // A terrain-specific shader can declare additional layout locations to read them.
    const VertexLayout layout({
        { 0, 3, GL_FLOAT, GL_FALSE }, // position
        { 1, 3, GL_FLOAT, GL_FALSE }, // normal
        { 2, 2, GL_FLOAT, GL_FALSE }, // uv
        { 3, 4, GL_FLOAT, GL_FALSE }, // tangent (xyz + handedness)
        { 4, 4, GL_FLOAT, GL_FALSE }, // materialZone, mountainMask, grassSuitable, treeSuitable
    });

    auto buffer = std::make_unique<MeshBuffer>(
        meshData.vertices.data(),
        static_cast<GLsizeiptr>(meshData.vertices.size() * sizeof(TerrainVertex)),
        static_cast<GLsizei>(meshData.vertices.size()),
        meshData.indices.data(),
        static_cast<GLsizei>(meshData.indices.size()),
        layout,
        GL_STATIC_DRAW);

    spdlog::info("[TerrainMeshBuilder] Uploaded terrain mesh: {} vertices, {} indices ({} triangles)",
                 meshData.VertexCount(),
                 meshData.IndexCount(),
                 meshData.IndexCount() / 3);

    return buffer;
}

} 