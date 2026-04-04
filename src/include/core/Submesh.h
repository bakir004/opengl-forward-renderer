#pragma once
#include <cstdint>
#include <string>

/// Describes one logical part of a multi-material mesh.
///
/// A SubMesh is a contiguous slice of the parent Mesh's shared index buffer.
/// All indices inside [indexOffset, indexOffset + indexCount) are valid into
/// the shared EBO. baseVertex is added to every index before the VBO lookup,
/// which allows each submesh's indices to start from 0 in the source data.
///
/// Example — a table model with two materials (wood + metal):
///   SubMesh 0: wood   → indexOffset=0,   indexCount=180, baseVertex=0
///   SubMesh 1: metal  → indexOffset=720, indexCount=48,  baseVertex=60
///
/// The parent Mesh owns the VAO/VBO/EBO. SubMesh holds no GPU resources.
struct SubMesh {
    /// Byte offset into the EBO where this submesh's indices begin.
    /// Pass directly as the 'indices' argument to glDrawElementsBaseVertex.
    uint32_t indexByteOffset = 0;

    /// Number of indices in this submesh (not bytes — raw index count).
    uint32_t indexCount      = 0;

    /// Added to every index fetched from the EBO before the VBO lookup.
    /// Allows each submesh's source index array to start from 0 independently.
    int32_t  baseVertex      = 0;

    /// Index into the parent Mesh's material array (not a GL texture unit).
    /// The renderer uses this to look up which MaterialInstance to bind.
    uint32_t materialIndex   = 0;

    /// Human-readable name from the source file (e.g. "Legs_Wood", "Handle_Metal").
    /// Optional — empty string is valid. Used for debugging and scene-graph lookup.
    std::string name;
};