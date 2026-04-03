#include "core/SubmissionContext.h"
#include <glad/glad.h>
#include <spdlog/spdlog.h>

void SubmissionContext::Apply(SubmissionContext& current) const {

    // ── Depth test ────────────────────────────────────────────────────────
    if (depthTestEnabled != current.depthTestEnabled) {
        if (depthTestEnabled) {
            glEnable(GL_DEPTH_TEST);
            spdlog::debug("[SubmissionContext] Depth test enabled");
        } else {
            glDisable(GL_DEPTH_TEST);
            spdlog::debug("[SubmissionContext] Depth test disabled");
        }
        current.depthTestEnabled = depthTestEnabled;
    }

    // Depth func (only meaningful when depth testing is on)
    if (depthTestEnabled && depthFunc != current.depthFunc) {
        GLenum glFunc = GL_LESS;
        const char* name = "Less";
        switch (depthFunc) {
            case DepthFunc::Less:         glFunc = GL_LESS;     name = "Less";         break;
            case DepthFunc::LessEqual:    glFunc = GL_LEQUAL;   name = "LessEqual";    break;
            case DepthFunc::Greater:      glFunc = GL_GREATER;  name = "Greater";      break;
            case DepthFunc::GreaterEqual: glFunc = GL_GEQUAL;   name = "GreaterEqual"; break;
            case DepthFunc::Always:       glFunc = GL_ALWAYS;   name = "Always";       break;
            case DepthFunc::Never:        glFunc = GL_NEVER;    name = "Never";        break;
            case DepthFunc::Equal:        glFunc = GL_EQUAL;    name = "Equal";        break;
            case DepthFunc::NotEqual:     glFunc = GL_NOTEQUAL; name = "NotEqual";     break;
        }
        glDepthFunc(glFunc);
        current.depthFunc = depthFunc;
        spdlog::debug("[SubmissionContext] Depth func: {}", name);
    }

    // ── Depth write mask ──────────────────────────────────────────────────
    if (depthWrite != current.depthWrite) {
        glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);
        current.depthWrite = depthWrite;
        spdlog::debug("[SubmissionContext] Depth write: {}", depthWrite ? "on" : "off");
    }

    // ── Blend mode ────────────────────────────────────────────────────────
    if (blendMode != current.blendMode) {
        const char* name = "Disabled";
        switch (blendMode) {
            case BlendMode::Disabled:
                glDisable(GL_BLEND);
                name = "Disabled";
                break;
            case BlendMode::Alpha:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBlendEquation(GL_FUNC_ADD);
                name = "Alpha";
                break;
            case BlendMode::Additive:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glBlendEquation(GL_FUNC_ADD);
                name = "Additive";
                break;
            case BlendMode::Multiply:
                glEnable(GL_BLEND);
                glBlendFunc(GL_DST_COLOR, GL_ZERO);
                glBlendEquation(GL_FUNC_ADD);
                name = "Multiply";
                break;
        }
        current.blendMode = blendMode;
        spdlog::debug("[SubmissionContext] Blend mode: {}", name);
    }

    // ── Cull mode ─────────────────────────────────────────────────────────
    if (cullMode != current.cullMode) {
        const char* name = "Disabled";
        switch (cullMode) {
            case CullMode::Disabled:
                glDisable(GL_CULL_FACE);
                name = "Disabled";
                break;
            case CullMode::Back:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                name = "Back";
                break;
            case CullMode::Front:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                name = "Front";
                break;
            case CullMode::FrontAndBack:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT_AND_BACK);
                name = "FrontAndBack";
                break;
        }
        current.cullMode = cullMode;
        spdlog::debug("[SubmissionContext] Cull mode: {}", name);
    }
}
