#pragma once

#include "core/SubMesh.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <cstdint>
#include <string>
#include <vector>

/// Backend-agnostic vertex format.
/// Shader locations: 0=position, 1=normal, 2=uv.
struct VertexPNT {
    glm::vec3 position { 0, 0, 0 };
    glm::vec3 normal   { 0, 1, 0 };
    glm::vec2 uv       { 0, 0   };
};

/// CPU-side mesh description passed to the backend for upload.
/// No GPU objects, no GL types. Safe to include anywhere.
struct MeshData {
    std::vector<VertexPNT> vertices;
    std::vector<uint32_t>  indices;
    std::vector<SubMesh>   submeshes;
    std::string            name;

    [[nodiscard]] bool IsValid() const {
        return !vertices.empty()
            && !indices.empty()
            && !submeshes.empty();
    }

    [[nodiscard]] uint32_t VertexCount() const {
        return static_cast<uint32_t>(vertices.size());
    }

    [[nodiscard]] uint32_t IndexCount() const {
        return static_cast<uint32_t>(indices.size());
    }
};