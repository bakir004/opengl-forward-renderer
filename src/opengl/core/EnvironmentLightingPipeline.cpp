#include "core/EnvironmentLightingPipeline.h"

#include "assets/AssetImporter.h"
#include "core/FullscreenQuad.h"
#include "core/Primitives.h"
#include "core/ShaderProgram.h"
#include "core/Texture2D.h"
#include "core/TextureCubemap.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <memory>

namespace
{
    constexpr int kIrradianceSize = 64;
    constexpr int kPrefilterSize = 256;
    constexpr int kBrdfLutSize = 512;

    int ComputeMipLevels(int size)
    {
        int levels = 1;
        while (size > 1)
        {
            size /= 2;
            ++levels;
        }
        return levels;
    }

    struct GLStateGuard
    {
        GLint viewport[4] = {0, 0, 0, 0};
        GLint drawFramebuffer = 0;
        GLint currentProgram = 0;
        GLboolean depthTest = GL_FALSE;
        GLboolean cullFace = GL_FALSE;
        GLboolean depthMask = GL_TRUE;

        GLStateGuard()
        {
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
            depthTest = glIsEnabled(GL_DEPTH_TEST);
            cullFace = glIsEnabled(GL_CULL_FACE);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        }

        ~GLStateGuard()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
            glUseProgram(static_cast<GLuint>(currentProgram));
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            if (depthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            if (cullFace)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);
            glDepthMask(depthMask);
        }
    };
}

struct EnvironmentLightingPipeline::Impl
{
    std::shared_ptr<MeshBuffer> cubeMesh;
    FullscreenQuad fullscreenQuad;
    std::shared_ptr<ShaderProgram> irradianceShader;
    std::shared_ptr<ShaderProgram> prefilterShader;
    std::shared_ptr<ShaderProgram> brdfLutShader;
    std::shared_ptr<Texture2D> sharedBrdfLut;
    GLuint framebuffer = 0;
    glm::mat4 captureProjection{1.0f};
    std::array<glm::mat4, 6> captureViews{};
    uint32_t lastSourceTextureId = 0;

    bool Initialize()
    {
        if (framebuffer == 0)
            glGenFramebuffers(1, &framebuffer);

        if (framebuffer == 0)
        {
            spdlog::error("[EnvironmentLighting] Failed to create capture framebuffer");
            return false;
        }

        cubeMesh = std::make_shared<MeshBuffer>(GenerateCube().CreateMeshBuffer());
        irradianceShader = AssetImporter::LoadShader("assets/shaders/skybox.vert", "assets/shaders/ibl_irradiance.frag");
        prefilterShader = AssetImporter::LoadShader("assets/shaders/skybox.vert", "assets/shaders/ibl_prefilter.frag");
        brdfLutShader = AssetImporter::LoadShader("assets/shaders/ibl_brdf_lut.vert", "assets/shaders/ibl_brdf_lut.frag");

        captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        captureViews = {
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        if (!(cubeMesh && irradianceShader && prefilterShader && brdfLutShader))
            return false;

        sharedBrdfLut = std::make_shared<Texture2D>(Texture2D::CreateRenderTarget(
            kBrdfLutSize,
            kBrdfLutSize,
            GL_RG16F,
            GL_RG,
            GL_FLOAT,
            SamplerDesc{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE}));
        if (!sharedBrdfLut->IsValid())
        {
            spdlog::error("[EnvironmentLighting] Failed to allocate BRDF LUT render target");
            return false;
        }

        RenderBrdfLut(*sharedBrdfLut);
        spdlog::info("[EnvironmentLighting] Generated BRDF integration LUT ({}x{}, RG16F, 1024 samples/texel, IBL k)",
                     kBrdfLutSize,
                     kBrdfLutSize);
        return true;
    }

    void Shutdown()
    {
        if (framebuffer != 0)
        {
            glDeleteFramebuffers(1, &framebuffer);
            framebuffer = 0;
        }

        cubeMesh.reset();
        irradianceShader.reset();
        prefilterShader.reset();
        brdfLutShader.reset();
        sharedBrdfLut.reset();
        lastSourceTextureId = 0;
    }

    void RenderIrradianceCubemap(TextureCubemap& target, const TextureCubemap& source)
    {
        GLStateGuard guard;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_NONE);

        irradianceShader->Bind();
        irradianceShader->SetUniform("u_Projection", captureProjection);
        irradianceShader->SetUniform("u_EnvironmentMap", 0);
        source.Bind(0);

        for (int face = 0; face < 6; ++face)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                   target.GetID(),
                                   0);
            glViewport(0, 0, kIrradianceSize, kIrradianceSize);
            glClear(GL_COLOR_BUFFER_BIT);
            irradianceShader->SetUniform("u_View", captureViews[face]);
            cubeMesh->Draw();
        }

        TextureCubemap::Unbind(0);
    }

    void RenderPrefilteredCubemap(TextureCubemap& target, const TextureCubemap& source)
    {
        GLStateGuard guard;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_NONE);
        // Generate mipmaps on the source cubemap so that textureLod() in the
        // prefilter shader can access intermediate mip levels.
        glBindTexture(GL_TEXTURE_CUBE_MAP, source.GetID());
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        prefilterShader->Bind();
        prefilterShader->SetUniform("u_Projection", captureProjection);
        prefilterShader->SetUniform("u_EnvironmentMap", 0);
        source.Bind(0);

        const int mipLevels = target.GetMipLevels();
        for (int mip = 0; mip < mipLevels; ++mip)
        {
            const int mipSize = std::max(1, kPrefilterSize >> mip);
            prefilterShader->SetUniform("u_Roughness", mipLevels > 1 ? static_cast<float>(mip) / static_cast<float>(mipLevels - 1) : 0.0f);

            for (int face = 0; face < 6; ++face)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                                       target.GetID(),
                                       mip);
                glViewport(0, 0, mipSize, mipSize);
                glClear(GL_COLOR_BUFFER_BIT);
                prefilterShader->SetUniform("u_View", captureViews[face]);
                cubeMesh->Draw();
            }
        }

        TextureCubemap::Unbind(0);
    }

    void RenderBrdfLut(Texture2D& target)
    {
        GLStateGuard guard;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);

        brdfLutShader->Bind();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.GetID(), 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_NONE);
        glViewport(0, 0, kBrdfLutSize, kBrdfLutSize);
        glClear(GL_COLOR_BUFFER_BIT);
        fullscreenQuad.Draw();
    }
};

EnvironmentLightingPipeline::EnvironmentLightingPipeline()
    : m_impl(std::make_unique<Impl>())
{
}

EnvironmentLightingPipeline::~EnvironmentLightingPipeline() = default;

EnvironmentLightingPipeline::EnvironmentLightingPipeline(EnvironmentLightingPipeline&&) noexcept = default;

EnvironmentLightingPipeline& EnvironmentLightingPipeline::operator=(EnvironmentLightingPipeline&&) noexcept = default;

bool EnvironmentLightingPipeline::Initialize()
{
    return m_impl && m_impl->Initialize();
}

void EnvironmentLightingPipeline::Shutdown()
{
    if (m_impl)
        m_impl->Shutdown();
}

bool EnvironmentLightingPipeline::EnsureProbe(ReflectionProbe& probe,
                                             std::shared_ptr<TextureCubemap> fallbackSource)
{
    if (!m_impl)
        return false;

    if (!probe.sourceCubemap && fallbackSource)
        probe.sourceCubemap = std::move(fallbackSource);

    if (!probe.sourceCubemap || !probe.sourceCubemap->IsValid())
    {
        spdlog::warn("[EnvironmentLighting] Probe has no valid source cubemap");
        return false;
    }

    const uint32_t sourceTextureId = probe.sourceCubemap->GetID();
    const bool sourceChanged = sourceTextureId != m_impl->lastSourceTextureId;

    if (!probe.irradianceCubemap || !probe.irradianceCubemap->IsValid() || sourceChanged)
    {
        probe.irradianceCubemap = std::make_shared<TextureCubemap>(
            TextureCubemap::CreateRenderTarget(kIrradianceSize, 1, GL_RGBA16F, GL_LINEAR, GL_LINEAR));
    }

    if (!probe.prefilteredCubemap || !probe.prefilteredCubemap->IsValid() || sourceChanged)
    {
        probe.prefilteredCubemap = std::make_shared<TextureCubemap>(
            TextureCubemap::CreateRenderTarget(kPrefilterSize,
                                               ComputeMipLevels(kPrefilterSize),
                                               GL_RGBA16F,
                                               GL_LINEAR_MIPMAP_LINEAR,
                                               GL_LINEAR));
    }

    // BRDF LUT is independent of the environment; one shared texture baked at pipeline init.
    probe.brdfLut = m_impl->sharedBrdfLut;

    if (!probe.irradianceCubemap || !probe.irradianceCubemap->IsValid() ||
        !probe.prefilteredCubemap || !probe.prefilteredCubemap->IsValid() ||
        !probe.brdfLut || !probe.brdfLut->IsValid())
    {
        spdlog::error("[EnvironmentLighting] Failed to allocate one or more IBL targets");
        return false;
    }

    if (sourceChanged)
    {
        m_impl->RenderIrradianceCubemap(*probe.irradianceCubemap, *probe.sourceCubemap);
        m_impl->RenderPrefilteredCubemap(*probe.prefilteredCubemap, *probe.sourceCubemap);
        m_impl->lastSourceTextureId = sourceTextureId;
        spdlog::info("[EnvironmentLighting] Generated irradiance + prefiltered cubemaps from source {}", sourceTextureId);
    }

    return probe.HasAnyIbl();
}