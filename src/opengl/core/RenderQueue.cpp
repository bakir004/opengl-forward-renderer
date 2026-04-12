#include "core/RenderQueue.h"
#include "core/SubmissionContext.h"
#include "core/ShaderProgram.h"
#include "core/MeshBuffer.h"
#include "core/Mesh.h"
#include "core/Material.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <spdlog/spdlog.h>

static GLenum ToGLPrimitive(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::Triangles:     return GL_TRIANGLES;
        case PrimitiveTopology::Lines:         return GL_LINES;
        case PrimitiveTopology::Points:        return GL_POINTS;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveTopology::LineStrip:     return GL_LINE_STRIP;
    }
    return GL_TRIANGLES;
}

static uint64_t ApproxTriangleCount(const RenderItem& item) {
    if (item.meshMulti) {
        if (item.subMeshIndex >= item.meshMulti->SubMeshCount())
            return 0;
        return static_cast<uint64_t>(item.meshMulti->GetSubMesh(item.subMeshIndex).indexCount) / 3ull;
    }

    if (!item.mesh)
        return 0;

    const uint64_t primitiveCount = item.mesh->IsIndexed()
        ? static_cast<uint64_t>(item.mesh->GetIndexCount())
        : static_cast<uint64_t>(item.mesh->GetVertexCount());

    // We only estimate triangle-like primitives here. Non-triangle topologies intentionally
    // contribute 0 so the UI stays honest instead of inventing a fake triangle equivalence.
    switch (item.topology) {
        case PrimitiveTopology::Triangles:
            return primitiveCount / 3ull;
        case PrimitiveTopology::TriangleStrip:
            return primitiveCount >= 3ull ? (primitiveCount - 2ull) : 0ull;
        case PrimitiveTopology::Lines:
        case PrimitiveTopology::LineStrip:
        case PrimitiveTopology::Points:
            return 0ull;
    }

    return 0ull;
}

bool RenderQueue::Add(const RenderItem& item) {
    if (!item.flags.visible || (!item.mesh && !item.meshMulti)) return false;
    if (!item.material && !item.shader) {
        if (m_errorShader) {
            RenderItem fallback  = item;
            fallback.shader      = m_errorShader;
            fallback.material    = nullptr;
            m_items.push_back(fallback);
            return true;
        }
        return false;
    }
    m_items.push_back(item);
    return true;
}

void RenderQueue::SetErrorShader(const ShaderProgram* shader) {
    m_errorShader = shader;
}

void RenderQueue::Sort() {
    // Sort by the resolved shader pointer to minimise program switches.
    std::sort(m_items.begin(), m_items.end(),
            [](const RenderItem& a, const RenderItem& b) {
            const ShaderProgram* sa = a.ResolvedShader();
            const ShaderProgram* sb = b.ResolvedShader();
            if (!sa && !sb) return false;
            if (!sa)        return false;
            if (!sb)        return true;
            return sa < sb;
            });
}

RenderQueueFrameStats RenderQueue::Flush(SubmissionContext& /*current*/) {
    const ShaderProgram*    lastShader   = nullptr;
    const MaterialInstance* lastMaterial = nullptr;
    DrawMode                lastMode     = DrawMode::Fill;
    RenderQueueFrameStats   stats{};

    for (const RenderItem& item : m_items) {
        // ── Draw mode (only on change) ────────────────────────────────────
        if (item.drawMode != lastMode) {
            switch (item.drawMode) {
                case DrawMode::Fill:      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  break;
                case DrawMode::Wireframe: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  break;
                case DrawMode::Points:    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); break;
            }
            lastMode = item.drawMode;
        }

        // ── Material / shader binding ─────────────────────────────────────
        if (item.material) {
            // Material path: bind full material (shader + textures + params).
            if (item.material != lastMaterial) {
                item.material->Bind();
                lastMaterial = item.material;
                lastShader   = item.material->GetShader();
            }
        } else {
            // Legacy shader-only path.
            if (item.shader != lastShader) {
                item.shader->Bind();
                lastShader   = item.shader;
                lastMaterial = nullptr;
            }
        }

        // ── Per-object uniforms ───────────────────────────────────────────
        const ShaderProgram* activeShader = item.ResolvedShader();
        if (activeShader) {
            glm::mat4 model = item.transform.GetModelMatrix();

            const glm::vec3& tOff = item.translationOffset;
            if (tOff != glm::vec3(0.0f))
                model = model * glm::translate(glm::mat4(1.0f), tOff);

            const glm::vec3& rOff = item.rotationOffsetDeg;
            if (rOff.x != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.x), {1,0,0});
            if (rOff.y != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.y), {0,1,0});
            if (rOff.z != 0.0f) model = model * glm::rotate(glm::mat4(1.0f), glm::radians(rOff.z), {0,0,1});

            activeShader->SetUniform("u_Model", model);

            const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
            activeShader->SetUniform("u_NormalMatrix", normalMat);
        }

        if (item.meshMulti) {
            if (item.subMeshIndex >= item.meshMulti->SubMeshCount())
                continue;
            item.meshMulti->DrawSubMesh(item.subMeshIndex);
            ++stats.processedItemCount;
            ++stats.drawCallCount;
            stats.approxTriangleCount += ApproxTriangleCount(item);
        } else if (item.mesh) {
            item.mesh->Draw(ToGLPrimitive(item.topology));
            ++stats.processedItemCount;
            ++stats.drawCallCount;
            stats.approxTriangleCount += ApproxTriangleCount(item);
        }
    }

    // Restore fill mode so the next frame starts clean.
    if (lastMode != DrawMode::Fill)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    return stats;
}

void RenderQueue::Clear() {
    m_items.clear();
}

bool RenderQueue::IsEmpty() const {
    return m_items.empty();
}
