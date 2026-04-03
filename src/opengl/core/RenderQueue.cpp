#include "core/RenderQueue.h"
#include "core/SubmissionContext.h"
#include "core/ShaderProgram.h"
#include "core/MeshBuffer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <algorithm>

void RenderQueue::Add(const RenderItem& item) {
    if (!item.flags.visible || !item.mesh || !item.shader)
        return;
    m_items.push_back(item);
}

void RenderQueue::Sort() {
    std::sort(m_items.begin(), m_items.end(),
        [](const RenderItem& a, const RenderItem& b) {
            // Null shaders sort to the end (Add() already filters them; this is defensive)
            if (!a.shader && !b.shader) return false;
            if (!a.shader) return false;
            if (!b.shader) return true;
            return a.shader < b.shader;
        });
}

void RenderQueue::Flush(SubmissionContext& /*current*/) {
    const ShaderProgram* lastShader  = nullptr;
    DrawMode             lastMode    = DrawMode::Fill;

    for (const RenderItem& item : m_items) {
        // ── Draw mode (glPolygonMode only on change) ──────────────────────
        if (item.drawMode != lastMode) {
            switch (item.drawMode) {
                case DrawMode::Fill:      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  break;
                case DrawMode::Wireframe: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  break;
                case DrawMode::Points:    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT); break;
            }
            lastMode = item.drawMode;
        }

        // ── Shader (bind only on change) ──────────────────────────────────
        if (item.shader != lastShader) {
            item.shader->Bind();
            lastShader = item.shader;
        }

        // ── Per-object uniforms ───────────────────────────────────────────
        const glm::mat4& model = item.transform.GetModelMatrix();
        item.shader->SetUniform("u_Model", model);

        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
        item.shader->SetUniform("u_NormalMatrix", normalMatrix);

        item.mesh->Draw();
    }

    // Restore default polygon mode so the next frame starts clean.
    if (lastMode != DrawMode::Fill)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void RenderQueue::Clear() {
    m_items.clear();
}

bool RenderQueue::IsEmpty() const {
    return m_items.empty();
}
