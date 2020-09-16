#include "RenderTargetGL.h"

CC_BACKEND_BEGIN

RenderTargetGL::RenderTargetGL(bool generateFBO) : _generatedFBO(generateFBO)
{
    if (_generatedFBO) {
        glGenFramebuffers(1, &_FBO);
    }
    else {
        _FBO = 0;
        // GLint defaultFBO = 0;
        // glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
        // _FBO = defaultFBO;
    }
}
RenderTargetGL::~RenderTargetGL()
{
    if (_generatedFBO) {
        bindFrameBuffer();

        for (auto slot = 0; slot < _ARRAYSIZE(color); ++slot)
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + slot,
                GL_TEXTURE_2D,
                0,
                0);

        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D,
            0,
            0);

        glFramebufferTexture2D(GL_FRAMEBUFFER,
            GL_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D,
            0,
            0);

        glDeleteFramebuffers(1, &_FBO);
        CHECK_GL_ERROR_DEBUG();
    }
}

void RenderTargetGL::bindFrameBuffer() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, _FBO);
}

void RenderTargetGL::unbindFrameBuffer() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTargetGL::setColorAttachment(const ColorAttachment& attachment)
{
    RenderTarget::setColorAttachment(attachment);

    if (_generatedFBO && bitmask::any(targets, TargetBufferFlags::COLOR_ALL)) {
#if 1
        GLenum bufs[4] = { GL_NONE };
        for (size_t i = 0; i < 4; i++) {
            if (bitmask::any(targets, getMRTColorFlag(i))) {
                auto textureInfo = attachment[i];
                auto textureHandler = textureInfo.texture != nullptr ? textureInfo.texture->getHandler() : 0;
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                     GL_COLOR_ATTACHMENT0 + i,
                     GL_TEXTURE_2D,
                    textureHandler,
                    textureInfo.level);
                bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            }
        }
        glDrawBuffers(4, bufs);
#else
        GLenum bufs[4] = { GL_NONE };
        for (size_t i = 0; i < 4; i++) {
            if (bitmask::any(targets, getMRTColorFlag(i))) {
                auto textureInfo = color[i];
                auto rbo = textureInfo.texture != nullptr ? textureInfo.texture->getHandler() : 0;
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0 + i,
                    GL_RENDERBUFFER,
                    rbo);
                bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            }
        }
        glDrawBuffers(4, bufs);
#endif

        CHECK_GL_ERROR_DEBUG();
    }
}

void RenderTargetGL::applyColorAttachment() const
{
}

void RenderTargetGL::setDepthAttachment(TextureBackend* attachment, int level)
{
    RenderTarget::setDepthAttachment(attachment, level);

    if (_generatedFBO && attachment) {
        if (bitmask::none(attachment->getTextureUsage(), TextureUsage::DEPTH_STENCIL_ATTACHMENT)) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                attachment != nullptr ? attachment->getHandler() : 0,
                level);
        }
        else {
            GLuint rbo = attachment != nullptr ? attachment->getHandler() : 0;
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,
                    rbo);
        }
        CHECK_GL_ERROR_DEBUG();
    }
}

void RenderTargetGL::setStencilAttachment(TextureBackend* attachment, int level)
{
    RenderTarget::setStencilAttachment(attachment, level);

    if (_generatedFBO && attachment) {
        if (bitmask::none(attachment->getTextureUsage(), TextureUsage::DEPTH_STENCIL_ATTACHMENT)) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                GL_STENCIL_ATTACHMENT,
                GL_TEXTURE_2D,
                attachment != nullptr ? attachment->getHandler() : 0,
                level);
        }
        else {
            GLuint rbo = attachment != nullptr ? attachment->getHandler() : 0;
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                GL_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                rbo);
        }

        CHECK_GL_ERROR_DEBUG();
    }
}

CC_BACKEND_END
