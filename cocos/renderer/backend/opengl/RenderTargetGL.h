#pragma once
#include "../RenderTarget.h"

CC_BACKEND_BEGIN

class RenderTargetGL : public RenderTarget
{
public:
    /*
    * generateFBO, false, use for screen framebuffer
    */
    RenderTargetGL(bool generateFBO);
    ~RenderTargetGL();

    void bindFrameBuffer() const override;
    void unbindFrameBuffer() const override;

    void setColorAttachment(const ColorAttachment& attachment) override;
    void setDepthAttachment(TextureBackend* attachment, int level = 0) override;
    void setStencilAttachment(TextureBackend* attachment, int level = 0) override;

    void applyColorAttachment() const override;

public:
    bool _generatedFBO = false;
    GLuint _FBO = 0;
};

CC_BACKEND_END
