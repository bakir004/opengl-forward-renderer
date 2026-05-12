#include "core/RenderQueue.h"
#include "core/SubmissionContext.h"
#include "core/ShaderProgram.h"
#include "core/MeshBuffer.h"
#include "core/Mesh.h"
#include "core/Material.h"
#include "core/Texture2D.h"
#include "core/TextureCubemap.h"
#include "scene/RenderItem.h"
#include "scene/ReflectionProbe.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <string>
#include <spdlog/spdlog.h>

namespace
{

    constexpr glm::vec3 kDefaultPbrAlbedoColor(1.0f, 1.0f, 1.0f);
    constexpr float kDefaultPbrMetallicValue = 0.0f;
    constexpr float kDefaultPbrRoughnessValue = 0.5f;
    constexpr glm::vec3 kDefaultPbrEmissiveColor(0.0f, 0.0f, 0.0f);

    void SetOptionalIntUniform(GLuint programId, const char *name, int value)
    {
        const GLint location = glGetUniformLocation(programId, name);
        if (location != -1)
            glUniform1i(location, value);
    }

    void SetOptionalFloatUniform(GLuint programId, const char *name, float value)
    {
        const GLint location = glGetUniformLocation(programId, name);
        if (location != -1)
            glUniform1f(location, value);
    }

    void SetOptionalVec3Uniform(GLuint programId, const char *name, const glm::vec3 &value)
    {
        const GLint location = glGetUniformLocation(programId, name);
        if (location != -1)
            glUniform3f(location, value.x, value.y, value.z);
    }

    bool IsCubemapPresent(const std::shared_ptr<TextureCubemap>& texture)
    {
        return texture && texture->IsValid();
    }

    bool IsTexture2DPresent(const std::shared_ptr<Texture2D>& texture)
    {
        return texture && texture->IsValid();
    }

    void BindEnvironmentResources(const ShaderProgram &shader, const ReflectionProbe *probe)
    {
        const GLuint programId = shader.GetID();
        if (programId == 0)
            return;

        const bool hasIrradiance = probe && IsCubemapPresent(probe->irradianceCubemap);
        const bool hasPrefiltered = probe && IsCubemapPresent(probe->prefilteredCubemap);
        const bool hasBrdfLut = probe && IsTexture2DPresent(probe->brdfLut);
        const bool hasIbl = hasIrradiance || (hasPrefiltered && hasBrdfLut);

        if (hasIrradiance)
            probe->irradianceCubemap->Bind(EnvironmentTextureUnit::Irradiance);
        else
            TextureCubemap::Unbind(EnvironmentTextureUnit::Irradiance);

        if (hasPrefiltered)
            probe->prefilteredCubemap->Bind(EnvironmentTextureUnit::Prefiltered);
        else
            TextureCubemap::Unbind(EnvironmentTextureUnit::Prefiltered);

        if (hasBrdfLut)
            probe->brdfLut->Bind(EnvironmentTextureUnit::BrdfLut);
        else
            Texture2D::Unbind(EnvironmentTextureUnit::BrdfLut);

        SetOptionalIntUniform(programId, EnvironmentTextureSlot::Irradiance, EnvironmentTextureUnit::Irradiance);
        SetOptionalIntUniform(programId, EnvironmentTextureSlot::Prefiltered, EnvironmentTextureUnit::Prefiltered);
        SetOptionalIntUniform(programId, EnvironmentTextureSlot::BrdfLut, EnvironmentTextureUnit::BrdfLut);
        SetOptionalIntUniform(programId, "u_HasIrradianceMap", hasIrradiance ? 1 : 0);
        SetOptionalIntUniform(programId, "u_HasPrefilteredMap", hasPrefiltered ? 1 : 0);
        SetOptionalIntUniform(programId, "u_HasBRDFLUT", hasBrdfLut ? 1 : 0);
        SetOptionalIntUniform(programId, "u_HasIBL", hasIbl ? 1 : 0);
        SetOptionalFloatUniform(programId, "u_IBLIntensity", (probe && hasIbl) ? probe->intensity : 0.0f);

        const glm::vec3 position = probe ? probe->position : glm::vec3(0.0f);
        const glm::vec3 boxMin = probe ? probe->boxMin : glm::vec3(0.0f);
        const glm::vec3 boxMax = probe ? probe->boxMax : glm::vec3(0.0f);
        SetOptionalVec3Uniform(programId, "u_ProbePosition", position);
        SetOptionalFloatUniform(programId, "u_ProbeRadius", probe ? probe->radius : 0.0f);
        SetOptionalVec3Uniform(programId, "u_ProbeBoxMin", boxMin);
        SetOptionalVec3Uniform(programId, "u_ProbeBoxMax", boxMax);
        SetOptionalIntUniform(programId, "u_ProbeInfluenceType", probe ? static_cast<int>(probe->influenceType) : 0);
    }

    void ApplyPbrFallbackUniformDefaults(const ShaderProgram &shader)
    {
        const GLuint programId = shader.GetID();
        if (programId == 0)
            return;

        SetOptionalIntUniform(programId, TextureSlot::Albedo, MaterialTextureUnit::Albedo);
        SetOptionalIntUniform(programId, TextureSlot::Normal, MaterialTextureUnit::Normal);
        SetOptionalIntUniform(programId, TextureSlot::Metallic, MaterialTextureUnit::Metallic);
        SetOptionalIntUniform(programId, TextureSlot::Roughness, MaterialTextureUnit::Roughness);
        SetOptionalIntUniform(programId, TextureSlot::AO, MaterialTextureUnit::AO);
        SetOptionalIntUniform(programId, TextureSlot::Emissive, MaterialTextureUnit::Emissive);
        SetOptionalIntUniform(programId, TextureSlot::SpecularGlossiness, MaterialTextureUnit::SpecularGlossiness);
        SetOptionalIntUniform(programId, "u_CascadeShadowMaps", 7);
        SetOptionalIntUniform(programId, EnvironmentTextureSlot::Irradiance, EnvironmentTextureUnit::Irradiance);
        SetOptionalIntUniform(programId, EnvironmentTextureSlot::Prefiltered, EnvironmentTextureUnit::Prefiltered);
        SetOptionalIntUniform(programId, EnvironmentTextureSlot::BrdfLut, EnvironmentTextureUnit::BrdfLut);
        SetOptionalVec3Uniform(programId, "u_AlbedoColor", kDefaultPbrAlbedoColor);
        SetOptionalFloatUniform(programId, "u_MetallicValue", kDefaultPbrMetallicValue);
        SetOptionalFloatUniform(programId, "u_RoughnessValue", kDefaultPbrRoughnessValue);
        SetOptionalVec3Uniform(programId, "u_EmissiveColor", kDefaultPbrEmissiveColor);
        SetOptionalIntUniform(programId, "u_HasAlbedoMap", 0);
        SetOptionalIntUniform(programId, "u_HasNormalMap", 0);
        SetOptionalIntUniform(programId, "u_HasMetallicMap", 0);
        SetOptionalIntUniform(programId, "u_HasRoughnessMap", 0);
        SetOptionalIntUniform(programId, "u_HasAoMap", 0);
        SetOptionalIntUniform(programId, "u_HasEmissiveMap", 0);
        SetOptionalIntUniform(programId, "u_HasSpecularGlossinessMap", 0);
    }

} // namespace

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

void RenderQueue::SetEnvironmentData(const ReflectionProbe *probe)
{
    m_activeReflectionProbe = probe;
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
                if (const ShaderProgram *shader = item.material->GetShader())
                    BindEnvironmentResources(*shader, m_activeReflectionProbe);
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
                ApplyPbrFallbackUniformDefaults(*item.shader);
                Texture2D::Unbind(MaterialTextureUnit::Albedo);
                Texture2D::Unbind(MaterialTextureUnit::Normal);
                Texture2D::Unbind(MaterialTextureUnit::Metallic);
                Texture2D::Unbind(MaterialTextureUnit::Roughness);
                Texture2D::Unbind(MaterialTextureUnit::AO);
                Texture2D::Unbind(MaterialTextureUnit::Emissive);
                Texture2D::Unbind(MaterialTextureUnit::SpecularGlossiness);
                BindEnvironmentResources(*item.shader, m_activeReflectionProbe);
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

            activeShader->SetUniform("u_ReceiveShadow", item.flags.receiveShadow ? 1 : 0);

            // ── Cascaded shadow data (directional light only) ─────────────
            if (m_hasShadowData)
            {
                // Always set these if shadow data is present, even if this object 
                // doesn't receive shadows, to ensure u_CascadeShadowMaps stays on unit 7.
                for (uint32_t i = 0; i < CascadedShadowMap::kNumCascades; ++i)
                {
                    const std::string idx = "[" + std::to_string(i) + "]";
                    activeShader->SetUniform(("u_CascadeViewProj" + idx).c_str(), m_cascadeViewProj[i]);
                    activeShader->SetUniform(("u_CascadeSplits" + idx).c_str(), m_cascadeSplits[i]);
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
            // Guard: if this submesh has no tangent data (no UV channel), disable
            // normal map sampling so the TBN is never applied on bad geometry.
            if (activeShader)
            {
                const SubMesh &sm = item.meshMulti->GetSubMesh(item.subMeshIndex);
                if (!sm.hasTangents)
                    SetOptionalIntUniform(activeShader->GetID(), "u_HasNormalMap", 0);
            }
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
    m_activeReflectionProbe = nullptr;
}

bool RenderQueue::IsEmpty() const
{
    return m_items.empty();
}

void RenderQueue::SetDirectionalShadowData(
    const std::array<glm::mat4, CascadedShadowMap::kNumCascades> &cascadeViewProj,
    const std::array<float, CascadedShadowMap::kNumCascades> &cascadeSplits,
    uint32_t shadowMapTextureArrayId,
    int pcfRadius)
{
    m_cascadeViewProj = cascadeViewProj;
    m_cascadeSplits = cascadeSplits;
    m_shadowMapTextureArrayId = shadowMapTextureArrayId;
    m_pcfRadius = pcfRadius;
    m_hasShadowData = (shadowMapTextureArrayId != 0);
}
