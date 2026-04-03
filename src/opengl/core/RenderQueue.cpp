#include "core/RenderQueue.h"
#include "core/SubmissionContext.h"
#include "core/ShaderProgram.h"
#include "core/MeshBuffer.h"
#include "core/Material.h"
#include "scene/RenderItem.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>

void RenderQueue::Add(const RenderItem& item) {
    if (!item.flags.visible || !item.mesh) return;
    if (!item.material && !item.shader)   return;
    m_items.push_back(item);
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

void RenderQueue::Flush(SubmissionContext& /*current*/) {
    const ShaderProgram*    lastShader   = nullptr;
    const MaterialInstance* lastMaterial = nullptr;
    DrawMode                lastMode     = DrawMode::Fill;

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
            const glm::mat4& model = item.transform.GetModelMatrix();
            activeShader->SetUniform("u_Model", model);

            const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
            activeShader->SetUniform("u_NormalMatrix", normalMat);
        }

        item.mesh->Draw();
    }

    // Restore fill mode so the next frame starts clean.
    if (lastMode != DrawMode::Fill)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void RenderQueue::Clear() {
    m_items.clear();
}

bool RenderQueue::IsEmpty() const {
    return m_items.empty();
}