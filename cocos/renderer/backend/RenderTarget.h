#pragma once

#include "base/CCRef.h"
#include "Texture.h"
#include <assert.h>

CC_BACKEND_BEGIN

class RenderTarget : public cocos2d::Ref {
public:
    struct RenderBuffer {
        TextureBackend* texture = nullptr;
        uint8_t level = 0; // level when attached to a texture
    };
    typedef std::array<RenderBuffer, MAX_COLOR_ATTCHMENT> ColorAttachment;

    ~RenderTarget()
    {
        for (auto colorItem : color)
            CC_SAFE_RELEASE(colorItem.texture);
        CC_SAFE_RELEASE(depth.texture);
        CC_SAFE_RELEASE(stencil.texture);
    }
    void setTargetFlags(TargetBufferFlags flags) { targets = flags; }

    virtual void bindFrameBuffer() const {};
    virtual void unbindFrameBuffer() const {};

    virtual void setColorAttachment(const ColorAttachment& attachment) {
        for (auto colorItem : color)
            CC_SAFE_RELEASE(colorItem.texture);
        this->color = attachment;
        for (auto colorItem : color)
            CC_SAFE_RETAIN(colorItem.texture);
    };
    virtual void setDepthAttachment(TextureBackend* attachment, int level = 0)
    {
        CC_SAFE_RELEASE(depth.texture);
        depth.texture = attachment;
        depth.level = level;
        CC_SAFE_RETAIN(depth.texture);
    };
    virtual void setStencilAttachment(TextureBackend* attachment, int level = 0) {
        CC_SAFE_RELEASE(stencil.texture);
        stencil.texture = attachment;
        stencil.level = level;
        CC_SAFE_RETAIN(depth.texture);
    };

    virtual void applyColorAttachment() const {}

    ColorAttachment color = {};
    RenderBuffer depth = {};
    RenderBuffer stencil = {};
    TargetBufferFlags targets = {};
};

CC_BACKEND_END
