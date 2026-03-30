#pragma once

#include "../core/MeshBuffer.h"
#include "../core/ShaderProgram.h"

#include <memory>
#include <string>

class Renderer;

/// Sprint 2 demonstration scene: loads the basic coloured shader and three built-in primitives
/// (triangle, indexed quad, indexed cube). Keeps application / \c main free of draw submission;
/// all draws go through \ref Renderer::SubmitDraw.
///
/// Layout is expressed in clip space by baking translation and scale into vertex positions at
/// setup time so \c basic.vert stays MVP-free (Sprint 3).
class SampleScene {
    public:
        SampleScene() = default;

        /// Loads the shader from disk, uploads primitive geometry to the GPU, and builds mesh handles.
        /// Must be called after \ref Renderer::Initialize while an OpenGL context is current.
        ///
        /// @param vertexShaderPath   Path to the vertex stage (e.g. \c shaders/basic.vert).
        /// @param fragmentShaderPath Path to the fragment stage (e.g. \c shaders/basic.frag).
        /// @return \c false if the linked \ref ShaderProgram is invalid; otherwise \c true.
        bool Setup(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);

        /// Issues one frame of the demo through \p renderer: depth test, back-face culling, three
        /// \ref SubmitDraw calls, then \ref Renderer::UnbindShader.
        ///
        /// @param renderer      Active renderer (must be between \ref Renderer::BeginFrame and \ref Renderer::EndFrame).
        /// @param timeSeconds   Reserved for future per-frame animation (unused; clip-space layout is baked in \ref Setup).
        void Render(Renderer& renderer, float timeSeconds);

        /// @return \c true after a successful \ref Setup; otherwise \c false.
        [[nodiscard]] bool IsReady() const { return m_ready; }

    private:
        std::unique_ptr<ShaderProgram> m_shader;   ///< Basic lit-colour program (\c basic.vert / \c basic.frag).
        std::unique_ptr<MeshBuffer>    m_triangle; ///< Non-indexed triangle mesh.
        std::unique_ptr<MeshBuffer>    m_quad;     ///< Indexed quad mesh.
        std::unique_ptr<MeshBuffer>    m_cube;     ///< Indexed cube mesh (non-trivial index path).
        bool                           m_ready = false; ///< \c true when GPU resources are usable.
};
