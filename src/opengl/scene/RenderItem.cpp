#include "scene/RenderItem.h"
#include "core/Material.h"
#include "core/ShaderProgram.h"

const ShaderProgram* RenderItem::ResolvedShader() const {
    if (material)
        return material->GetShader();
    return shader;
}