#pragma once

#include "terrain/TerrainData.h"
#include "core/MeshBuffer.h"
#include <memory>

/// Builds a renderable terrain mesh from a heightfield.
/// Produces positions, normals, tangents, UVs, indices, and axis-aligned bounds.
/// The output MeshBuffer uses the VertexPNT layout (locations 0-3) so it is
/// uploadable through the existing renderer mesh pipeline without a special code path.
namespace TerrainMeshBuilder
{
    /// Generates CPU-side TerrainMeshData from a heightfield.
    /// - positions : one vertex per heightfield sample, Y = sample.height
    /// - normals   : copied from sample.normal (already computed by TerrainGenerator)
    /// - tangents  : computed analytically along the +X grid axis, handedness = +1
    /// - UVs       : normalized [0..1] across the full grid
    /// - indices   : two CCW triangles per quad (row-major grid)
    /// - bounds    : tight AABB over all vertex positions
    [[nodiscard]] TerrainMeshData BuildMeshData(const TerrainHeightfield& heightfield);

    /// Uploads a TerrainMeshData to the GPU and returns a MeshBuffer.
    /// The vertex layout matches VertexPNT: location 0=position, 1=normal,
    /// 2=uv, 3=tangent — the same layout used by mesh.vert so the terrain
    /// renders through the standard forward pass with no shader changes.
    [[nodiscard]] std::unique_ptr<MeshBuffer> UploadMeshData(const TerrainMeshData& meshData);
}