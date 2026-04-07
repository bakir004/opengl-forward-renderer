// Primitives.cpp
#include "core/Primitives.h"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// ─── PrimitiveMeshData helpers ────────────────────────────────────────────────

GLsizeiptr PrimitiveMeshData::GetVertexBufferSize() const {
    return static_cast<GLsizeiptr>(vertices.size() * sizeof(VertexPC));
}
GLsizei PrimitiveMeshData::GetVertexCount() const {
    return static_cast<GLsizei>(vertices.size());
}
GLsizei PrimitiveMeshData::GetIndexCount() const {
    return static_cast<GLsizei>(indices.size());
}
bool PrimitiveMeshData::IsIndexed() const {
    return !indices.empty();
}
VertexLayout PrimitiveMeshData::CreateLayout() {
    return VertexLayout({
        {0, 3, GL_FLOAT, GL_FALSE},  // position
        {1, 3, GL_FLOAT, GL_FALSE},  // color
    });
}
MeshBuffer PrimitiveMeshData::CreateMeshBuffer(GLenum usage) const {
    if (vertices.empty())
        spdlog::error("[Primitives] CreateMeshBuffer() called on empty PrimitiveMeshData");

    return MeshBuffer(
        vertices.empty() ? nullptr : vertices.data(),
        GetVertexBufferSize(),
        GetVertexCount(),
        indices.empty()  ? nullptr : indices.data(),
        GetIndexCount(),
        CreateLayout(),
        usage);
}

// ─── Internal colour helpers ──────────────────────────────────────────────────

/// Returns a hue-shifted tint of @p base for face @p faceIdx out of @p numFaces.
/// Each face gets a small sinusoidal offset across R/G/B channels (120° apart)
/// so the faces are visually distinguishable while still reading as one colour.
static glm::vec3 SolidFaceColor(const glm::vec3& base, int faceIdx, int numFaces) {
    const float t    = (numFaces > 1) ? (float(faceIdx) / numFaces) : 0.0f;
    const float vary = 0.18f;
    const float pi2  = 6.2831853f;
    glm::vec3 tint{
        vary * std::sin(pi2 * t),
        vary * std::sin(pi2 * t + 2.0944f),   // +120°
        vary * std::sin(pi2 * t + 4.1888f),   // +240°
    };
    return glm::clamp(base + tint, 0.0f, 1.0f);
}

// ─── Double-sided helper ──────────────────────────────────────────────────────

/// Duplicates every triangle with reversed winding so it is also visible from behind.
/// Works on both indexed and non-indexed meshes.
static void MakeDoubleSided(PrimitiveMeshData& mesh) {
    if (mesh.IsIndexed()) {
        const size_t origCount = mesh.indices.size();
        mesh.indices.reserve(origCount * 2);
        for (size_t i = 0; i < origCount; i += 3) {
            mesh.indices.push_back(mesh.indices[i + 2]);
            mesh.indices.push_back(mesh.indices[i + 1]);
            mesh.indices.push_back(mesh.indices[i]);
        }
    } else {
        // Non-indexed: replicate vertices in reverse order per triangle.
        const size_t origCount = mesh.vertices.size();
        mesh.vertices.reserve(origCount * 2);
        for (size_t i = 0; i < origCount; i += 3) {
            mesh.vertices.push_back(mesh.vertices[i + 2]);
            mesh.vertices.push_back(mesh.vertices[i + 1]);
            mesh.vertices.push_back(mesh.vertices[i]);
        }
    }
}

// ─── Generators ──────────────────────────────────────────────────────────────

PrimitiveMeshData GenerateTriangle(const PrimitiveOptions& opts) {
    PrimitiveMeshData mesh;
    if (opts.colorMode == ColorMode::Solid) {
        const glm::vec3 c = opts.baseColor;
        mesh.vertices = {
            {{-0.6f, -0.5f, 0.0f}, c},
            {{ 0.6f, -0.5f, 0.0f}, c},
            {{ 0.0f,  0.6f, 0.0f}, c},
        };
    } else {
        mesh.vertices = {
            {{-0.6f, -0.5f, 0.0f}, {1.0f, 0.2f, 0.2f}},
            {{ 0.6f, -0.5f, 0.0f}, {0.2f, 1.0f, 0.2f}},
            {{ 0.0f,  0.6f, 0.0f}, {0.2f, 0.4f, 1.0f}},
        };
    }
    if (opts.doubleSided) MakeDoubleSided(mesh);
    return mesh;
}

PrimitiveMeshData GenerateQuad(const PrimitiveOptions& opts) {
    PrimitiveMeshData mesh;
    if (opts.colorMode == ColorMode::Solid) {
        const glm::vec3 c = opts.baseColor;
        mesh.vertices = {
            {{-0.5f, -0.5f, 0.0f}, c},
            {{ 0.5f, -0.5f, 0.0f}, c},
            {{ 0.5f,  0.5f, 0.0f}, c},
            {{-0.5f,  0.5f, 0.0f}, c},
        };
    } else {
        mesh.vertices = {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.2f, 0.2f}},
            {{ 0.5f, -0.5f, 0.0f}, {0.2f, 1.0f, 0.2f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.2f, 0.4f, 1.0f}},
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.9f, 0.2f}},
        };
    }
    mesh.indices = { 0, 1, 2,  2, 3, 0 };
    if (opts.doubleSided) MakeDoubleSided(mesh);
    return mesh;
}

PrimitiveMeshData GenerateCube(const PrimitiveOptions& opts) {
    auto faceColor = [&](int f) -> glm::vec3 {
        if (opts.colorMode == ColorMode::Solid)
            return SolidFaceColor(opts.baseColor, f, 6);
        static const glm::vec3 kColors[6] = {
            {1.0f, 0.2f, 0.2f},  // 0 Back   (+Z)
            {0.2f, 1.0f, 0.2f},  // 1 Front  (-Z)
            {0.2f, 0.4f, 1.0f},  // 2 Left   (-X)
            {1.0f, 0.9f, 0.2f},  // 3 Right  (+X)
            {0.9f, 0.2f, 1.0f},  // 4 Top    (+Y)
            {0.2f, 1.0f, 1.0f},  // 5 Bottom (-Y)
        };
        return kColors[f];
    };

    const auto c0 = faceColor(0), c1 = faceColor(1), c2 = faceColor(2),
               c3 = faceColor(3), c4 = faceColor(4), c5 = faceColor(5);

    PrimitiveMeshData mesh;
    mesh.vertices = {
        // Back (+Z)
        {{-0.5f, -0.5f,  0.5f}, c0}, {{ 0.5f, -0.5f,  0.5f}, c0},
        {{ 0.5f,  0.5f,  0.5f}, c0}, {{-0.5f,  0.5f,  0.5f}, c0},
        // Front (-Z)
        {{ 0.5f, -0.5f, -0.5f}, c1}, {{-0.5f, -0.5f, -0.5f}, c1},
        {{-0.5f,  0.5f, -0.5f}, c1}, {{ 0.5f,  0.5f, -0.5f}, c1},
        // Left (-X)
        {{-0.5f, -0.5f, -0.5f}, c2}, {{-0.5f, -0.5f,  0.5f}, c2},
        {{-0.5f,  0.5f,  0.5f}, c2}, {{-0.5f,  0.5f, -0.5f}, c2},
        // Right (+X)
        {{ 0.5f, -0.5f,  0.5f}, c3}, {{ 0.5f, -0.5f, -0.5f}, c3},
        {{ 0.5f,  0.5f, -0.5f}, c3}, {{ 0.5f,  0.5f,  0.5f}, c3},
        // Top (+Y)
        {{-0.5f,  0.5f,  0.5f}, c4}, {{ 0.5f,  0.5f,  0.5f}, c4},
        {{ 0.5f,  0.5f, -0.5f}, c4}, {{-0.5f,  0.5f, -0.5f}, c4},
        // Bottom (-Y)
        {{-0.5f, -0.5f, -0.5f}, c5}, {{ 0.5f, -0.5f, -0.5f}, c5},
        {{ 0.5f, -0.5f,  0.5f}, c5}, {{-0.5f, -0.5f,  0.5f}, c5},
    };
    mesh.indices = {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20,
    };
    if (opts.doubleSided) MakeDoubleSided(mesh);
    return mesh;
}

PrimitiveMeshData GeneratePyramid(const PrimitiveOptions& opts) {
    // Square base at y = -0.5, apex at y = +0.5.
    // Corner labels: BL=back-left, BR=back-right, FR=front-right, FL=front-left.
    const glm::vec3 BL(-0.5f, -0.5f, -0.5f);
    const glm::vec3 BR( 0.5f, -0.5f, -0.5f);
    const glm::vec3 FR( 0.5f, -0.5f,  0.5f);
    const glm::vec3 FL(-0.5f, -0.5f,  0.5f);
    const glm::vec3 AP( 0.0f,  0.5f,  0.0f);  // apex

    auto faceColor = [&](int f) -> glm::vec3 {
        if (opts.colorMode == ColorMode::Solid)
            return SolidFaceColor(opts.baseColor, f, 5);
        static const glm::vec3 kColors[5] = {
            {0.2f, 1.0f, 1.0f},  // 0 Base  (-Y)
            {1.0f, 0.2f, 0.2f},  // 1 Front (+Z)
            {1.0f, 0.9f, 0.2f},  // 2 Right (+X)
            {0.2f, 0.4f, 1.0f},  // 3 Back  (-Z)
            {0.2f, 1.0f, 0.2f},  // 4 Left  (-X)
        };
        return kColors[f];
    };

    const auto cBase  = faceColor(0);
    const auto cFront = faceColor(1);
    const auto cRight = faceColor(2);
    const auto cBack  = faceColor(3);
    const auto cLeft  = faceColor(4);

    PrimitiveMeshData mesh;
    mesh.vertices = {
        // Base (-Y normal): winding BL,BR,FR,FL gives CCW normal = -Y
        {BL, cBase}, {BR, cBase}, {FR, cBase}, {FL, cBase},       // 0-3
        // Front (+Z normal): FL, FR, apex
        {FL, cFront}, {FR, cFront}, {AP, cFront},                  // 4-6
        // Right (+X normal): FR, BR, apex
        {FR, cRight}, {BR, cRight}, {AP, cRight},                  // 7-9
        // Back (-Z normal): BR, BL, apex
        {BR, cBack},  {BL, cBack},  {AP, cBack},                   // 10-12
        // Left (-X normal): BL, FL, apex
        {BL, cLeft},  {FL, cLeft},  {AP, cLeft},                   // 13-15
    };
    mesh.indices = {
         0,  1,  2,   0,  2,  3,   // Base (2 triangles)
         4,  5,  6,                 // Front
         7,  8,  9,                 // Right
        10, 11, 12,                 // Back
        13, 14, 15,                 // Left
    };
    if (opts.doubleSided) MakeDoubleSided(mesh);
    return mesh;
}

PrimitiveMeshData GenerateSphere(float radius, int detail, const PrimitiveOptions& opts) {
    const int stacks = std::max(2, detail);
    const int slices = std::max(3, detail * 2);

    PrimitiveMeshData mesh;
    mesh.vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));
    mesh.indices.reserve(static_cast<size_t>(stacks * slices * 6));

    const float pi = std::atan(1.0f) * 4.0f;

    // Vertices: ring by ring, top (phi=0) to bottom (phi=PI).
    for (int i = 0; i <= stacks; ++i) {
        const float phi = pi * float(i) / float(stacks);
        const float y   = radius * std::cos(phi);
        const float r   = radius * std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            const float theta = 2.0f * pi * float(j) / float(slices);
            const glm::vec3 pos(r * std::cos(theta), y, r * std::sin(theta));

            glm::vec3 color;
            if (opts.colorMode == ColorMode::Solid) {
                // Subtle top-bright/bottom-dark gradient to show shape in 3-D.
                const float brightness = 0.70f + 0.30f * (pos.y / radius);
                color = glm::clamp(opts.baseColor * brightness, 0.0f, 1.0f);
            } else {
                // Rainbow: map outward normal (pos/radius) from [-1,1] to [0,1].
                color = (pos / radius) * 0.5f + glm::vec3(0.5f);
            }
            mesh.vertices.push_back({pos, color});
        }
    }

    // Indices: stitch adjacent rings into quads (2 outward-facing triangles each).
    // vertex [i][j] lives at index i*(slices+1)+j.
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const uint32_t a = uint32_t(i * (slices + 1) + j);
            const uint32_t b = a + uint32_t(slices + 1);

            // Tri 1 — outward-facing
            mesh.indices.push_back(a);
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b);

            // Tri 2 — outward-facing
            mesh.indices.push_back(a + 1);
            mesh.indices.push_back(b + 1);
            mesh.indices.push_back(b);
        }
    }

    if (opts.doubleSided) MakeDoubleSided(mesh);
    return mesh;
}
