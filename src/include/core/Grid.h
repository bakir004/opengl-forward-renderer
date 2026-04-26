#pragma once

#include <memory>

class ShaderProgram;
class MeshBuffer;

/// Grid rendering for coordinate system visualization
/// Renders an infinite-like grid on the XZ plane with major/minor lines and colored axes
class Grid {
public:
    Grid();
    ~Grid() = default;

    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;
    Grid(Grid&&) = default;
    Grid& operator=(Grid&&) = default;

    /// Render the grid
    /// Must be called after camera UBO is bound
    void Draw() const;

    /// Set major grid spacing (default: 1.0)
    void SetGridScale(float scale) { m_gridScale = scale; }

    /// Set minor grid spacing (default: 0.1)
    void SetGridMinorScale(float scale) { m_gridMinorScale = scale; }

    /// Set distance at which grid fades out (default: 100.0)
    void SetFadeDistance(float distance) { m_fadeDistance = distance; }

    /// Set thickness of main axes (default: 0.02)
    void SetAxisThickness(float thickness) { m_axisThickness = thickness; }

private:
    std::shared_ptr<ShaderProgram> m_shader;
    std::shared_ptr<MeshBuffer> m_quadMesh;

    float m_gridScale = 1.0f;
    float m_gridMinorScale = 0.1f;
    float m_fadeDistance = 100.0f;
    float m_axisThickness = 0.02f;
};
