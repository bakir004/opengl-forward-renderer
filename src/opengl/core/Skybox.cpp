#include "core/Skybox.h"
#include "core/MeshBuffer.h"
#include "core/TextureCubemap.h"
#include "core/ShaderProgram.h"
#include "core/Primitives.h"
#include "core/Texture2D.h"
#include "assets/AssetImporter.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

Skybox::Skybox(const std::vector<std::string>& facePaths)
{
    if (facePaths.size() == 6) {
        std::array<std::string, 6> paths;
        for (int i = 0; i < 6; ++i) paths[i] = facePaths[i];
        m_cubemap = AssetImporter::LoadCubemap(paths, TextureColorSpace::sRGB);
    }
    Init();
}

Skybox::Skybox(std::shared_ptr<TextureCubemap> cubemap)
    : m_cubemap(std::move(cubemap))
{
    Init();
}

Skybox::~Skybox() = default;

void Skybox::Init() {
    m_shader = AssetImporter::LoadShader("assets/shaders/skybox.vert", "assets/shaders/skybox.frag");
    if (!m_shader) {
        spdlog::error("[Skybox] Failed to load skybox shader!");
    }
    
    if (!m_cubemap) {
        spdlog::warn("[Skybox] Cubemap is null in Init()");
    }
    
    // We use the primitive cube. 
    auto cubeData = GenerateCube();
    m_cubeMesh = std::make_shared<MeshBuffer>(cubeData.CreateMeshBuffer());
}

void Skybox::Draw(const glm::mat4& projection, const glm::mat4& view) const {
    if (!m_cubemap || !m_shader || !m_cubeMesh) return;

    // Remove translation from view matrix
    glm::mat4 rotationOnlyView = glm::mat4(glm::mat3(view));

    glDepthFunc(GL_LEQUAL); 
    glDepthMask(GL_FALSE); // Don't write to depth buffer
    glDisable(GL_CULL_FACE); // See the inside of the cube
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // Ensure fill mode

    m_shader->Bind();
    m_shader->SetUniform("u_Projection", projection);
    m_shader->SetUniform("u_View", rotationOnlyView);

    m_cubemap->Bind(0);
    m_shader->SetUniform("u_Skybox", 0);

    m_cubeMesh->Draw();

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS); // Set depth function back to default
}
