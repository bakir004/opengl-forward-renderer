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
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <string>
#include <spdlog/spdlog.h>

static GLenum ToGLPrimitive(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::Triangles:
        return GL_TRIANGLES;
    case PrimitiveTopology::Lines:
        return GL_LINES;
    case PrimitiveTopology::Points:
        return GL_POINTS;
    case PrimitiveTopology::TriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveTopology::LineStrip:
        return GL_LINE_STRIP;
    }
    return GL_TRIANGLES;
}

static uint64_t ApproxTriangleCount(const RenderItem &item)
{
    if (item.meshMulti)
    {
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
    switch (item.topology)
    {
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

bool RenderQueue::Add(const RenderItem &item)
{
    if (!item.flags.visible || (!item.mesh && !item.meshMulti))
        return false;
    if (!item.material && !item.shader)
    {
        if (m_errorShader)
        {
            RenderItem fallback = item;
            fallback.shader = m_errorShader;
            fallback.material = nullptr;
            m_items.push_back(fallback);
            return true;
        }
        return false;
    }
    m_items.push_back(item);
    return true;
}

void RenderQueue::SetErrorShader(const ShaderProgram *shader)
{
    m_errorShader = shader;
}

void RenderQueue::Sort()
{
    // Sort by the resolved shader pointer to minimise program switches.
    std::sort(m_items.begin(), m_items.end(),
              [](const RenderItem &a, const RenderItem &b)
              {
                  const ShaderProgram *sa = a.ResolvedShader();
                  const ShaderProgram *sb = b.ResolvedShader();
                  if (!sa && !sb)
                      return false;
                  if (!sa)
                      return false;
                  if (!sb)
                      return true;
                  return sa < sb;
              });
}

RenderQueueFrameStats RenderQueue::Flush(SubmissionContext & /*current*/)
{
    const ShaderProgram *lastShader = nullptr;
    const MaterialInstance *lastMaterial = nullptr;
    DrawMode lastMode = DrawMode::Fill;
    RenderQueueFrameStats stats{};

    for (const RenderItem &item : m_items)
    {
        // ── Draw mode (only on change) ────────────────────────────────────
        if (item.drawMode != lastMode)
        {
            switch (item.drawMode)
            {
            case DrawMode::Fill:
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                break;
            case DrawMode::Wireframe:
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                break;
            case DrawMode::Points:
                glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
                break;
            }
            lastMode = item.drawMode;
        }

        // ── Material / shader binding ─────────────────────────────────────
        if (item.material)
        {
            // Material path: bind full material (shader + textures + params).
            if (item.material != lastMaterial)
            {
                item.material->Bind();
                lastMaterial = item.material;
                lastShader = item.material->GetShader();
            }
        }
        else
        {
            // Legacy shader-only path.
            if (item.shader != lastShader)
            {
                item.shader->Bind();
                lastShader = item.shader;
                lastMaterial = nullptr;
            }
        }

        // ── Per-object uniforms ───────────────────────────────────────────
        const ShaderProgram *activeShader = item.ResolvedShader();
        if (activeShader)
        {
            glm::mat4 model = item.transform.GetModelMatrix();

            const glm::vec3 &tOff = item.translationOffset;
            if (tOff != glm::vec3(0.0f))
                model = model * glm::translate(glm::mat4(1.0f), tOff);

            // Apply the mesh-local orientation fix as a single quaternion→matrix conversion.
            // One glm::mat4_cast replaces three separate glm::rotate matrix multiplications,
            // and the quaternion stores the intent (axis + angle) without any order ambiguity.
            static const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
            if (item.rotationOffset != kIdentity)
                model = model * glm::mat4_cast(item.rotationOffset);

            activeShader->SetUniform("u_Model", model);

            const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
            activeShader->SetUniform("u_NormalMatrix", normalMat);

            // ── Cascaded shadow data (directional light only) ─────────────
            if (m_hasShadowData && item.flags.receiveShadow)
            {
                for (uint32_t i = 0; i < CascadedShadowMap::kNumCascades; ++i)
                {
                    const std::string idx = "[" + std::to_string(i) + "]";
                    activeShader->SetUniform(("u_CascadeViewProj" + idx).c_str(), m_cascadeViewProj[i]);
                    activeShader->SetUniform(("u_CascadeSplits"   + idx).c_str(), m_cascadeSplits[i]);
                }

                // Bind the cascade depth array to texture unit 7.
                glActiveTexture(GL_TEXTURE0 + 7);
                glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowMapTextureArrayId);
                activeShader->SetUniform("u_CascadeShadowMaps", 7);
                activeShader->SetUniform("u_PCFRadius", m_pcfRadius);
                glActiveTexture(GL_TEXTURE0);
            }
        }

        if (item.meshMulti)
        {
            if (item.subMeshIndex >= item.meshMulti->SubMeshCount())
                continue;
            item.meshMulti->DrawSubMesh(item.subMeshIndex);
            ++stats.processedItemCount;
            ++stats.drawCallCount;
            stats.approxTriangleCount += ApproxTriangleCount(item);
        }
        else if (item.mesh)
        {
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

void RenderQueue::Clear()
{
    m_items.clear();
}

bool RenderQueue::IsEmpty() const
{
    return m_items.empty();
}

void RenderQueue::SetDirectionalShadowData(
    const std::array<glm::mat4, CascadedShadowMap::kNumCascades> &cascadeViewProj,
    const std::array<float,     CascadedShadowMap::kNumCascades> &cascadeSplits,
    uint32_t shadowMapTextureArrayId,
    int pcfRadius)
{
    m_cascadeViewProj = cascadeViewProj;
    m_cascadeSplits   = cascadeSplits;
    m_shadowMapTextureArrayId = shadowMapTextureArrayId;
    m_pcfRadius = pcfRadius;
    m_hasShadowData = (shadowMapTextureArrayId != 0);
}
