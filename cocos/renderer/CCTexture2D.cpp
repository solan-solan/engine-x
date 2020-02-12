/****************************************************************************
Copyright (c) 2008      Apple Inc. All Rights Reserved.
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/



/*
* Support for RGBA_4_4_4_4 and RGBA_5_5_5_1 was copied from:
* https://devforums.apple.com/message/37855#37855 by a1studmuffin
*/

#include "renderer/CCTexture2D.h"
#include "platform/CCImage.h"
#include "platform/CCGL.h"
#include "base/ccUtils.h"
#include "platform/CCDevice.h"
#include "base/ccConfig.h"
#include "base/ccMacros.h"
#include "base/ccUTF8.h"
#include "base/CCConfiguration.h"
#include "platform/CCPlatformMacros.h"
#include "base/CCDirector.h"
#include "base/CCNinePatchImageParser.h"
#include "renderer/backend/Device.h"
#include "renderer/backend/ProgramState.h"
#include "renderer/ccShaders.h"
#include "renderer/CCTextureUtils.h"
#include "renderer/CCRenderer.h"

#if CC_ENABLE_CACHE_TEXTURE_DATA
    #include "renderer/CCTextureCache.h"
#endif

NS_CC_BEGIN



namespace {
    typedef Texture2D::PixelFormatInfoMap::value_type PixelFormatInfoMapValue;
    static const PixelFormatInfoMapValue TexturePixelFormatInfoTablesValue[] =
    {
        PixelFormatInfoMapValue(backend::PixelFormat::BGRA8888, Texture2D::PixelFormatInfo(32, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::RGBA8888, Texture2D::PixelFormatInfo(32, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::RGBA4444, Texture2D::PixelFormatInfo(16, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::RGB5A1, Texture2D::PixelFormatInfo(16, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::RGB565, Texture2D::PixelFormatInfo(16, false, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::RGB888, Texture2D::PixelFormatInfo(24, false, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::A8, Texture2D::PixelFormatInfo(8, false, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::I8, Texture2D::PixelFormatInfo(8, false, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::AI88, Texture2D::PixelFormatInfo(16, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::ASTC4, Texture2D::PixelFormatInfo(8, true, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::ASTC8, Texture2D::PixelFormatInfo(2, true, true)),

#if defined( GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG) || (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
        PixelFormatInfoMapValue(backend::PixelFormat::PVRTC2, Texture2D::PixelFormatInfo(2, true, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::PVRTC2A, Texture2D::PixelFormatInfo(2, true, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::PVRTC4, Texture2D::PixelFormatInfo(4, true, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::PVRTC4A, Texture2D::PixelFormatInfo(4, true, true)),
#endif
        
#if defined(GL_ETC1_RGB8_OES) || (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
        PixelFormatInfoMapValue(backend::PixelFormat::ETC, Texture2D::PixelFormatInfo(4, true, false)),
#endif
        
#if defined(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
        PixelFormatInfoMapValue(backend::PixelFormat::S3TC_DXT1, Texture2D::PixelFormatInfo(4, true, false)),
#endif
        
#if defined(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
        PixelFormatInfoMapValue(backend::PixelFormat::S3TC_DXT3, Texture2D::PixelFormatInfo(8, true, false)),
#endif
        
#if defined(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
        PixelFormatInfoMapValue(backend::PixelFormat::S3TC_DXT5, Texture2D::PixelFormatInfo(8, true, false)),
#endif
        
#ifdef GL_ATC_RGB_AMD
        PixelFormatInfoMapValue(backend::PixelFormat::ATC_RGB, Texture2D::PixelFormatInfo(4, true, false)),
#endif
        
#ifdef GL_ATC_RGBA_EXPLICIT_ALPHA_AMD
        PixelFormatInfoMapValue(backend::PixelFormat::ATC_EXPLICIT_ALPHA, Texture2D::PixelFormatInfo(8, true, false)),
#endif
        
#ifdef GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD
        PixelFormatInfoMapValue(backend::PixelFormat::ATC_INTERPOLATED_ALPHA, Texture2D::PixelFormatInfo(8, true, false)),
#endif
        //metal formats
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
        PixelFormatInfoMapValue(backend::PixelFormat::MTL_ABGR4, Texture2D::PixelFormatInfo(16, false, true)),
        PixelFormatInfoMapValue(backend::PixelFormat::MTL_B5G6R5, Texture2D::PixelFormatInfo(16, false, false)),
        PixelFormatInfoMapValue(backend::PixelFormat::MTL_BGR5A1, Texture2D::PixelFormatInfo(16, false, true)),
#endif
    };
}

//CLASS IMPLEMENTATIONS:

//The PixpelFormat corresponding information
const Texture2D::PixelFormatInfoMap Texture2D::_pixelFormatInfoTables(TexturePixelFormatInfoTablesValue,
                                                                     TexturePixelFormatInfoTablesValue + sizeof(TexturePixelFormatInfoTablesValue) / sizeof(TexturePixelFormatInfoTablesValue[0]));

// If the image has alpha, you can create RGBA8 (32-bit) or RGBA4 (16-bit) or RGB5A1 (16-bit)
// Default is: RGBA8888 (32-bit textures)
static backend::PixelFormat g_defaultAlphaPixelFormat = backend::PixelFormat::DEFAULT;


Texture2D::Texture2D()
: _pixelFormat(backend::PixelFormat::DEFAULT)
, _pixelsWide(0)
, _pixelsHigh(0)
, _maxS(0.0)
, _maxT(0.0)
, _flagsAndFormatEXT(TextureFlag::ANTIALIAS_ENABLED)
, _ninePatchInfo(nullptr)
, _valid(true)
{
    backend::TextureDescriptor textureDescriptor;
    textureDescriptor.textureFormat = PixelFormat::NONE;
    _texture = static_cast<backend::Texture2DBackend*>(backend::Device::getInstance()->newTexture(textureDescriptor));
}

Texture2D::~Texture2D()
{
#if CC_ENABLE_CACHE_TEXTURE_DATA
    VolatileTextureMgr::removeTexture(this);
#endif
    CCLOGINFO("deallocing Texture2D: %p - id=%u", this, _name);

    CC_SAFE_DELETE(_ninePatchInfo);

    CC_SAFE_RELEASE(_texture);
    CC_SAFE_RELEASE(_programState);
}

backend::PixelFormat Texture2D::getPixelFormat() const
{
    return _pixelFormat;
}

int Texture2D::getPixelsWide() const
{
    return _pixelsWide;
}

int Texture2D::getPixelsHigh() const
{
    return _pixelsHigh;
}

backend::TextureBackend* Texture2D::getBackendTexture() const
{
    return _texture;
}

Size Texture2D::getContentSize() const
{
    Size ret;
    ret.width = _contentSize.width / CC_CONTENT_SCALE_FACTOR();
    ret.height = _contentSize.height / CC_CONTENT_SCALE_FACTOR();
    
    return ret;
}

const Size& Texture2D::getContentSizeInPixels()
{
    return _contentSize;
}

float Texture2D::getMaxS() const
{
    return _maxS;
}

void Texture2D::setMaxS(float maxS)
{
    _maxS = maxS;
}

float Texture2D::getMaxT() const
{
    return _maxT;
}

void Texture2D::setMaxT(float maxT)
{
    _maxT = maxT;
}

bool Texture2D::hasPremultipliedAlpha() const
{
    return _flagsAndFormatEXT & TextureFlag::PREMULTIPLIEDALPHA;
}

void Texture2D::setPremultipliedAlpha(bool premultipliedAlpha)
{
    if (premultipliedAlpha) _flagsAndFormatEXT |= TextureFlag::PREMULTIPLIEDALPHA;
    else _flagsAndFormatEXT &= ~TextureFlag::PREMULTIPLIEDALPHA;
}

bool Texture2D::initWithData(const void *data, ssize_t dataLen, backend::PixelFormat pixelFormat, backend::PixelFormat renderFormat, int pixelsWide, int pixelsHigh, const Size& /*contentSize*/, bool preMultipliedAlpha)
{
    CCASSERT(dataLen>0 && pixelsWide>0 && pixelsHigh>0, "Invalid size");

    //if data has no mipmaps, we will consider it has only one mipmap
    MipmapInfo mipmap;
    mipmap.address = (unsigned char*)data;
    mipmap.len = static_cast<int>(dataLen);
    return initWithMipmaps(&mipmap, 1, pixelFormat, renderFormat, pixelsWide, pixelsHigh, preMultipliedAlpha);
}

bool Texture2D::initWithMipmaps(MipmapInfo* mipmaps, int mipmapsNum, backend::PixelFormat pixelFormat, backend::PixelFormat renderFormat, int pixelsWide, int pixelsHigh, bool preMultipliedAlpha)
{
    //the pixelFormat must be a certain value 
    updateWithMipmaps(mipmaps, mipmapsNum, pixelFormat, renderFormat, pixelsWide, pixelsHigh, preMultipliedAlpha);
    
    return true;
}

bool Texture2D::updateWithImage(Image* image, backend::PixelFormat format, int index, int formatEXT)
{
    if (image == nullptr)
    {
        CCLOG("cocos2d: Texture2D. Can't create Texture. UIImage is nil");
        return false;
    }

    if(this->_filePath.empty()) this->_filePath = image->getFilePath();

    int imageWidth = image->getWidth();
    int imageHeight = image->getHeight();

    Configuration* conf = Configuration::getInstance();

    int maxTextureSize = conf->getMaxTextureSize();
    if (imageWidth > maxTextureSize || imageHeight > maxTextureSize)
    {
        CCLOG("cocos2d: WARNING: Image (%u x %u) is bigger than the supported %u x %u", imageWidth, imageHeight, maxTextureSize, maxTextureSize);
        return false;
    }

    unsigned char* tempData = image->getData();
    Size             imageSize = Size((float)imageWidth, (float)imageHeight);
    backend::PixelFormat      renderFormat = ((PixelFormat::NONE == format) || (PixelFormat::AUTO == format)) ? image->getPixelFormat() : format;
    backend::PixelFormat      imagePixelFormat = image->getPixelFormat();
    size_t           tempDataLen = image->getDataLen();


#ifdef CC_USE_METAL
    //compressed format does not need conversion
    switch (imagePixelFormat) {
    case PixelFormat::PVRTC4A:
    case PixelFormat::PVRTC4:
    case PixelFormat::PVRTC2A:
    case PixelFormat::PVRTC2:
    case PixelFormat::A8:
    case PixelFormat::ETC:
        renderFormat = imagePixelFormat;
    default:
        break;
    }
    //override renderFormat, since some render format is not supported by metal
    switch (renderFormat)
    {
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS && !TARGET_OS_SIMULATOR)
        //packed 16 bits pixels only available on iOS
    case PixelFormat::RGB565:
        renderFormat = PixelFormat::MTL_B5G6R5;
        break;
    case PixelFormat::RGBA4444:
        renderFormat = PixelFormat::MTL_ABGR4;
        break;
    case PixelFormat::RGB5A1:
        renderFormat = PixelFormat::MTL_BGR5A1;
        break;
#else
    case PixelFormat::RGB565:
    case PixelFormat::RGB5A1:
    case PixelFormat::RGBA4444:
#endif
    case PixelFormat::I8:
    case PixelFormat::AI88:
        //TODO: conversion RGBA8888 -> I8(AI88) -> RGBA8888 may happends
        renderFormat = PixelFormat::RGBA8888;
        break;
    default:
        break;
    }
#endif

    if (image->getNumberOfMipmaps() > 1)
    {
        if (renderFormat != image->getPixelFormat())
        {
            CCLOG("cocos2d: WARNING: This image has more than 1 mipmaps and we will not convert the data format");
        }

        //pixel format of data is not converted, renderFormat can be different from pixelFormat
        //it will be done later
        updateWithMipmaps(image->getMipmaps(), image->getNumberOfMipmaps(), image->getPixelFormat(), renderFormat, imageWidth, imageHeight, image->hasPremultipliedAlpha(), index);
    }
    else if (image->isCompressed())
    {   
        if (renderFormat != image->getPixelFormat())
        {
            CCLOG("cocos2d: WARNING: This image is compressed and we can't convert it for now");
        }

        updateWithData(tempData, tempDataLen, image->getPixelFormat(), image->getPixelFormat(), imageWidth, imageHeight, imageSize, image->hasPremultipliedAlpha(), index);
    }
    else
    {
        //after conversion, renderFormat == pixelFormat of data
        updateWithData(tempData, tempDataLen, imagePixelFormat, renderFormat, imageWidth, imageHeight, imageSize, image->hasPremultipliedAlpha(), index);
    }

    _flagsAndFormatEXT |= formatEXT;

    // pitfall: because we do merge etc1 alpha at shader, so must mark as _hasPremultipliedAlpha = true to makesure alpha blend works well.
    if (formatEXT == TextureFormatEXT::ETC1_ALPHA)
        setPremultipliedAlpha(true);

    return true;
}

bool Texture2D::updateWithData(const void* data, ssize_t dataLen, backend::PixelFormat pixelFormat, backend::PixelFormat renderFormat, int pixelsWide, int pixelsHigh, const Size& /*contentSize*/, bool preMultipliedAlpha, int index)
{
    CCASSERT(dataLen > 0 && pixelsWide > 0 && pixelsHigh > 0, "Invalid size");

    //if data has no mipmaps, we will consider it has only one mipmap
    MipmapInfo mipmap;
    mipmap.address = (unsigned char*)data;
    mipmap.len = static_cast<int>(dataLen);
    return updateWithMipmaps(&mipmap, 1, pixelFormat, renderFormat, pixelsWide, pixelsHigh, preMultipliedAlpha, index);
}

bool Texture2D::updateWithMipmaps(MipmapInfo* mipmaps, int mipmapsNum, backend::PixelFormat pixelFormat, backend::PixelFormat renderFormat, int pixelsWide, int pixelsHigh, bool preMultipliedAlpha, int index)
{
    //the pixelFormat must be a certain value 
    CCASSERT(pixelFormat != PixelFormat::NONE && pixelFormat != PixelFormat::AUTO, "the \"pixelFormat\" param must be a certain value!");
    CCASSERT(pixelsWide > 0 && pixelsHigh > 0, "Invalid size");

    if (mipmapsNum <= 0)
    {
        CCLOG("cocos2d: WARNING: mipmap number is less than 1");
        return false;
    }


    auto formatItr = _pixelFormatInfoTables.find(pixelFormat);
    if (formatItr == _pixelFormatInfoTables.end())
    {
        CCLOG("cocos2d: WARNING: unsupported pixelformat: %lx", (unsigned long)pixelFormat);
#ifdef CC_USE_METAL
        CCASSERT(false, "pixeformat not found in _pixelFormatInfoTables, register required!");
#endif
        return false;
    }

    const PixelFormatInfo& info = formatItr->second;

    if (info.compressed && !Configuration::getInstance()->supportsPVRTC()
        && !Configuration::getInstance()->supportsETC()
        && !Configuration::getInstance()->supportsS3TC()
        && !Configuration::getInstance()->supportsASTC()
        && !Configuration::getInstance()->supportsATITC())
    {
        CCLOG("cocos2d: WARNING: PVRTC/ETC images are not supported");
        return false;
    }

#if CC_ENABLE_CACHE_TEXTURE_DATA
    VolatileTextureMgr::findVolotileTexture(this);
#endif

    backend::TextureDescriptor textureDescriptor;
    textureDescriptor.width = pixelsWide;
    textureDescriptor.height = pixelsHigh;
    
    textureDescriptor.samplerDescriptor.magFilter = (_flagsAndFormatEXT & TextureFlag::ANTIALIAS_ENABLED) ? backend::SamplerFilter::LINEAR : backend::SamplerFilter::NEAREST;
    if (mipmapsNum == 1)
    {
        textureDescriptor.samplerDescriptor.minFilter = (_flagsAndFormatEXT & TextureFlag::ANTIALIAS_ENABLED) ? backend::SamplerFilter::LINEAR : backend::SamplerFilter::NEAREST;
    }
    else
    {
        textureDescriptor.samplerDescriptor.minFilter = (_flagsAndFormatEXT & TextureFlag::ANTIALIAS_ENABLED) ? backend::SamplerFilter::LINEAR_MIPMAP_NEAREST : backend::SamplerFilter::NEAREST_MIPMAP_NEAREST;
    }

    int width = pixelsWide;
    int height = pixelsHigh;
    backend::PixelFormat oriPixelFormat = pixelFormat;
    for (int i = 0; i < mipmapsNum; ++i)
    {
        unsigned char* data = mipmaps[i].address;
        size_t dataLen = mipmaps[i].len;
        unsigned char* outData = data;
        size_t outDataLen = dataLen;

        if (renderFormat != oriPixelFormat && !info.compressed) //need conversion
        {
            auto convertedFormat = backend::PixelFormatUtils::convertDataToFormat(data, dataLen, oriPixelFormat, renderFormat, &outData, &outDataLen);
#ifdef CC_USE_METAL
            CCASSERT(convertedFormat == renderFormat, "PixelFormat convert failed!");
#endif
            if (convertedFormat == renderFormat) pixelFormat = renderFormat;
        }

        textureDescriptor.textureFormat = pixelFormat;
        CCASSERT(textureDescriptor.textureFormat != backend::PixelFormat::NONE, "PixelFormat should not be NONE");

        if (_texture->getTextureFormat() != textureDescriptor.textureFormat)
            _texture->updateTextureDescriptor(textureDescriptor, index);

        if (info.compressed)
        {
            _texture->updateCompressedData(data, width, height, dataLen, i, index);
        }
        else
        {
            _texture->updateData(outData, width, height, i, index);
        }

        if (outData && outData != data && outDataLen > 0)
        {
            free(outData);
            outData = nullptr;
            outDataLen = 0;
        }

        if (i > 0 && (width != height || ccNextPOT(width) != width))
        {
            CCLOG("cocos2d: Texture2D. WARNING. Mipmap level %u is not squared. Texture won't render correctly. width=%d != height=%d", i, width, height);
        }

        width = MAX(width >> 1, 1);
        height = MAX(height >> 1, 1);
    }

    if (index == 0) {
        _contentSize = Size((float)pixelsWide, (float)pixelsHigh);
        _pixelsWide = pixelsWide;
        _pixelsHigh = pixelsHigh;
        _pixelFormat = pixelFormat;
        _maxS = 1;
        _maxT = 1;

        setPremultipliedAlpha(preMultipliedAlpha);
    }

    return true;
}

bool Texture2D::updateWithSubData(void *data,int offsetX,int offsetY,int width,int height, int index)
{
    if (_texture && width > 0 && height > 0)
    {
        uint8_t* textureData = static_cast<uint8_t*>(data);
        _texture->updateSubData(offsetX, offsetY, width, height, 0, textureData, index);
        return true;
    }
    return false;
}

// implementation Texture2D (Image)
bool Texture2D::initWithImage(Image *image)
{
    return initWithImage(image, g_defaultAlphaPixelFormat);
}

bool Texture2D::initWithImage(Image *image, backend::PixelFormat format)
{
    if (image == nullptr)
    {
        CCLOG("cocos2d: Texture2D. Can't create Texture. UIImage is nil");
        return false;
    }

    this->_filePath = image->getFilePath();

    return updateWithImage(image, format);
}

// implementation Texture2D (Text)
bool Texture2D::initWithString(const char *text, const std::string& fontName, float fontSize, const Size& dimensions/* = Size(0, 0)*/, TextHAlignment hAlignment/* =  TextHAlignment::CENTER */, TextVAlignment vAlignment/* =  TextVAlignment::TOP */, bool enableWrap /* = false */, int overflow /* = 0 */)
{
    FontDefinition tempDef;
    
    tempDef._shadow._shadowEnabled = false;
    tempDef._stroke._strokeEnabled = false;
   
    
    tempDef._fontName      = fontName;
    tempDef._fontSize      = fontSize;
    tempDef._dimensions    = dimensions;
    tempDef._alignment     = hAlignment;
    tempDef._vertAlignment = vAlignment;
    tempDef._fontFillColor = Color3B::WHITE;
    tempDef._enableWrap    = enableWrap;
    tempDef._overflow      = overflow;

    return initWithString(text, tempDef);
}

bool Texture2D::initWithString(const char *text, const FontDefinition& textDefinition)
{
    if(!text || 0 == strlen(text))
    {
        return false;
    }

#if CC_ENABLE_CACHE_TEXTURE_DATA
    // cache the texture data
    VolatileTextureMgr::addStringTexture(this, text, textDefinition);
#endif

    bool ret = false;
    Device::TextAlign align;
    
    if (TextVAlignment::TOP == textDefinition._vertAlignment)
    {
        align = (TextHAlignment::CENTER == textDefinition._alignment) ? Device::TextAlign::TOP
        : (TextHAlignment::LEFT == textDefinition._alignment) ? Device::TextAlign::TOP_LEFT : Device::TextAlign::TOP_RIGHT;
    }
    else if (TextVAlignment::CENTER == textDefinition._vertAlignment)
    {
        align = (TextHAlignment::CENTER == textDefinition._alignment) ? Device::TextAlign::CENTER
        : (TextHAlignment::LEFT == textDefinition._alignment) ? Device::TextAlign::LEFT : Device::TextAlign::RIGHT;
    }
    else if (TextVAlignment::BOTTOM == textDefinition._vertAlignment)
    {
        align = (TextHAlignment::CENTER == textDefinition._alignment) ? Device::TextAlign::BOTTOM
        : (TextHAlignment::LEFT == textDefinition._alignment) ? Device::TextAlign::BOTTOM_LEFT : Device::TextAlign::BOTTOM_RIGHT;
    }
    else
    {
        CCASSERT(false, "Not supported alignment format!");
        return false;
    }
    
#if (CC_TARGET_PLATFORM != CC_PLATFORM_ANDROID) && (CC_TARGET_PLATFORM != CC_PLATFORM_IOS)
    CCASSERT(textDefinition._stroke._strokeEnabled == false, "Currently stroke only supported on iOS and Android!");
#endif

    PixelFormat      pixelFormat = g_defaultAlphaPixelFormat;
    unsigned char* outTempData = nullptr;
    size_t outTempDataLen = 0;

    int imageWidth;
    int imageHeight;
    auto textDef = textDefinition;
    auto contentScaleFactor = CC_CONTENT_SCALE_FACTOR();
    textDef._fontSize *= contentScaleFactor;
    textDef._dimensions.width *= contentScaleFactor;
    textDef._dimensions.height *= contentScaleFactor;
    textDef._stroke._strokeSize *= contentScaleFactor;
    textDef._shadow._shadowEnabled = false;
    
    bool hasPremultipliedAlpha;
    Data outData = Device::getTextureDataForText(text, textDef, align, imageWidth, imageHeight, hasPremultipliedAlpha);
    if(outData.isNull())
    {
        return false;
    }

    Size  imageSize = Size((float)imageWidth, (float)imageHeight);
    pixelFormat = backend::PixelFormatUtils::convertDataToFormat(outData.getBytes(), imageWidth*imageHeight*4, PixelFormat::RGBA8888, pixelFormat, &outTempData, &outTempDataLen);

    ret = initWithData(outTempData, outTempDataLen, pixelFormat, imageWidth, imageHeight, imageSize);

    if (outTempData != nullptr && outTempData != outData.getBytes())
    {
        free(outTempData);
    }
    setPremultipliedAlpha(hasPremultipliedAlpha);

    return ret;
}

bool Texture2D::initWithBackendTexture(backend::TextureBackend *texture, bool preMultipliedAlpha)
{
    CC_SAFE_RETAIN(texture);
    CC_SAFE_RELEASE(_texture);
    _texture = dynamic_cast<backend::Texture2DBackend*>(texture);
    CC_ASSERT(_texture);
    _pixelsWide = _contentSize.width = _texture->getWidth();
    _pixelsHigh = _contentSize.height = _texture->getHeight();
    setPremultipliedAlpha(preMultipliedAlpha);

    return true;
}

void Texture2D::setRenderTarget(bool renderTarget)
{
    if (renderTarget) _flagsAndFormatEXT |= TextureFlag::RENDERTARGET;
    else _flagsAndFormatEXT &= TextureFlag::RENDERTARGET;
}

bool Texture2D::hasMipmaps() const
{
    return _texture->hasMipmaps();
}

void Texture2D::setAliasTexParameters()
{

    if ((_flagsAndFormatEXT & TextureFlag::ANTIALIAS_ENABLED) == 0)
    {
        return;
    }

    _flagsAndFormatEXT &= ~TextureFlag::ANTIALIAS_ENABLED;

    backend::SamplerDescriptor descriptor(
        backend::SamplerFilter::NEAREST, //magFilter
        (_texture->hasMipmaps()) ? backend::SamplerFilter::NEAREST_MIPMAP_NEAREST : backend::SamplerFilter::NEAREST, //minFilter
        backend::SamplerAddressMode::DONT_CARE, //sAddressMode
        backend::SamplerAddressMode::DONT_CARE//tAddressMode
    );
    _texture->updateSamplerDescriptor(descriptor);
}

void Texture2D::setAntiAliasTexParameters()
{

    if (_flagsAndFormatEXT & TextureFlag::ANTIALIAS_ENABLED)
    {
        return;
    }
    _flagsAndFormatEXT |= TextureFlag::ANTIALIAS_ENABLED;

    backend::SamplerDescriptor descriptor(
        backend::SamplerFilter::LINEAR, //magFilter
        (_texture->hasMipmaps()) ? backend::SamplerFilter::LINEAR_MIPMAP_NEAREST : backend::SamplerFilter::LINEAR, //minFilter
        backend::SamplerAddressMode::DONT_CARE, //sAddressMode
        backend::SamplerAddressMode::DONT_CARE //tAddressMode
    );
    _texture->updateSamplerDescriptor(descriptor);
}

const char* Texture2D::getStringForFormat() const
{
    switch (_pixelFormat) 
    {
        case backend::PixelFormat::RGBA8888:
            return  "RGBA8888";

        case backend::PixelFormat::RGB888:
            return  "RGB888";

        case backend::PixelFormat::RGB565:
            return  "RGB565";

        case backend::PixelFormat::RGBA4444:
            return  "RGBA4444";

        case backend::PixelFormat::RGB5A1:
            return  "RGB5A1";

        case backend::PixelFormat::AI88:
            return  "AI88";

        case backend::PixelFormat::A8:
            return  "A8";

        case backend::PixelFormat::I8:
            return  "I8";

        case backend::PixelFormat::PVRTC4:
            return  "PVRTC4";

        case backend::PixelFormat::PVRTC2:
            return  "PVRTC2";

        case backend::PixelFormat::PVRTC2A:
            return "PVRTC2A";
        
        case backend::PixelFormat::PVRTC4A:
            return "PVRTC4A";
            
        case backend::PixelFormat::ETC:
            return "ETC";

        case backend::PixelFormat::S3TC_DXT1:
            return "S3TC_DXT1";
            
        case backend::PixelFormat::S3TC_DXT3:
            return "S3TC_DXT3";

        case backend::PixelFormat::S3TC_DXT5:
            return "S3TC_DXT5";
            
        case backend::PixelFormat::ATC_RGB:
            return "ATC_RGB";

        case backend::PixelFormat::ATC_EXPLICIT_ALPHA:
            return "ATC_EXPLICIT_ALPHA";

        case backend::PixelFormat::ATC_INTERPOLATED_ALPHA:
            return "ATC_INTERPOLATED_ALPHA";
        
        case backend::PixelFormat::MTL_ABGR4:
            return "MTL_ABGR4";
        
        case backend::PixelFormat::MTL_B5G6R5:
            return "MTL_RGB565";
        
        case backend::PixelFormat::MTL_BGR5A1:
            return "MTL_BGR5A1";
           
        case backend::PixelFormat::ASTC4:
            return "ASTC4";
        case backend::PixelFormat::ASTC8:
            return "ASTC8";

        default:
            CCASSERT(false , "unrecognized pixel format");
            CCLOG("stringForFormat: %ld, cannot give useful result", (long)_pixelFormat);
            break;
    }

    return  nullptr;
}

//
// Texture options for images that contains alpha
//
// implementation Texture2D (PixelFormat)

void Texture2D::setDefaultAlphaPixelFormat(backend::PixelFormat format)
{
    g_defaultAlphaPixelFormat = format;
}

backend::PixelFormat Texture2D::getDefaultAlphaPixelFormat()
{
    return g_defaultAlphaPixelFormat;
}

unsigned int Texture2D::getBitsPerPixelForFormat(backend::PixelFormat format) const
{
    if (format == PixelFormat::NONE || format == PixelFormat::DEFAULT)
    {
        return 0;
    }
    
    return _pixelFormatInfoTables.at(format).bpp;
}

unsigned int Texture2D::getBitsPerPixelForFormat() const
{
    return this->getBitsPerPixelForFormat(_pixelFormat);
}

const Texture2D::PixelFormatInfoMap& Texture2D::getPixelFormatInfoMap()
{
    return _pixelFormatInfoTables;
}

void Texture2D::addSpriteFrameCapInset(SpriteFrame* spritframe, const Rect& capInsets)
{
    if(nullptr == _ninePatchInfo)
    {
        _ninePatchInfo = new (std::nothrow) NinePatchInfo;
    }
    if(nullptr == spritframe)
    {
        _ninePatchInfo->capInsetSize = capInsets;
    }
    else
    {
        _ninePatchInfo->capInsetMap[spritframe] = capInsets;
    }
}

bool Texture2D::isContain9PatchInfo()const
{
    return nullptr != _ninePatchInfo;
}

const Rect& Texture2D::getSpriteFrameCapInset( cocos2d::SpriteFrame *spriteFrame )const
{
    CCASSERT(_ninePatchInfo != nullptr,
             "Can't get the sprite frame capInset when the texture contains no 9-patch info.");
    if(nullptr == spriteFrame)
    {
        return this->_ninePatchInfo->capInsetSize;
    }
    else
    {
        auto &capInsetMap = this->_ninePatchInfo->capInsetMap;
        if(capInsetMap.find(spriteFrame) != capInsetMap.end())
        {
            return capInsetMap.at(spriteFrame);
        }
        else
        {
            return this->_ninePatchInfo->capInsetSize;
        }
    }
}


void Texture2D::removeSpriteFrameCapInset(SpriteFrame* spriteFrame)
{
    if(nullptr != this->_ninePatchInfo)
    {
        auto capInsetMap = this->_ninePatchInfo->capInsetMap;
        if(capInsetMap.find(spriteFrame) != capInsetMap.end())
        {
            capInsetMap.erase(spriteFrame);
        }
    }
}

void Texture2D::setTexParameters(const Texture2D::TexParams &desc)
{
    _texture->updateSamplerDescriptor(desc);
}

void Texture2D::generateMipmap()
{
    CCASSERT(_pixelsWide == ccNextPOT(_pixelsWide) && _pixelsHigh == ccNextPOT(_pixelsHigh), "Mipmap texture only works in POT textures");

    _texture->generateMipmaps();
}

void Texture2D::initProgram()
{
    if(_programState != nullptr)
        return;
    
    auto& pipelineDescriptor = _customCommand.getPipelineDescriptor();
    //create program state
    auto* program = backend::Program::getBuiltinProgram(backend::ProgramType::POSITION_TEXTURE);
    _programState = new (std::nothrow) cocos2d::backend::ProgramState(program);
    _mvpMatrixLocation = _programState->getUniformLocation("u_MVPMatrix");
    _textureLocation = _programState->getUniformLocation("u_texture");
    
    pipelineDescriptor.programState = _programState;
    
    //setup vertex layout
    auto vertexLayout = _programState->getVertexLayout();
    auto& attributes = _programState->getProgram()->getActiveAttributes();
    auto iter = attributes.find("a_position");
    if(iter != attributes.end())
        vertexLayout->setAttribute("a_position", iter->second.location, backend::VertexFormat::FLOAT2, 0, false);
    
    iter = attributes.find("a_texCoord");
    if(iter != attributes.end())
        vertexLayout->setAttribute("a_texCoord", iter->second.location, backend::VertexFormat::FLOAT2, 2 * sizeof(float), false);
    
    vertexLayout->setLayout(4 * sizeof(float));
    
    //create vertex buffer
    _customCommand.setDrawType(CustomCommand::DrawType::ARRAY);
    _customCommand.setPrimitiveType(CustomCommand::PrimitiveType::TRIANGLE_STRIP);
    _customCommand.createVertexBuffer(4 * sizeof(float), 4, CustomCommand::BufferUsage::DYNAMIC);
    
    //setup blend state
    BlendFunc blendFunc;
    if(hasPremultipliedAlpha())
    {
        blendFunc = BlendFunc::ALPHA_PREMULTIPLIED;
    }
    else
    {
        blendFunc = BlendFunc::ALPHA_NON_PREMULTIPLIED;
    }
    
    auto& blendDescriptor = pipelineDescriptor.blendDescriptor;
    blendDescriptor.blendEnabled = true;
    blendDescriptor.sourceRGBBlendFactor = blendDescriptor.sourceAlphaBlendFactor = blendFunc.src;
    blendDescriptor.destinationRGBBlendFactor = blendDescriptor.destinationAlphaBlendFactor = blendFunc.dst;
    
    _programState->setTexture(_textureLocation, 0, _texture);
}

void Texture2D::drawAtPoint(const Vec2 &point, float globalZOrder)
{
    float width = (float)_pixelsWide * _maxS;
    float height = (float)_pixelsHigh * _maxT;
    Rect rect = { point.x, point.y, width, height };
    drawInRect(rect, globalZOrder);
}

void Texture2D::drawInRect(const Rect& rect, float globalZOrder)
{
    initProgram();
    _customCommand.init(globalZOrder);
    auto director = Director::getInstance();
    const auto& modelView = director->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    const auto& projection = director->getMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_PROJECTION);
    
    Mat4 matrixMVP = projection * modelView;
    
    float vertexData[] = {
        rect.origin.x,                      rect.origin.y,                      0.0f,   _maxT,
        rect.size.width + rect.origin.x,    rect.origin.y,                      _maxS,  _maxT,
        rect.origin.x,                      rect.size.height  + rect.origin.y,  0.0f,   0.0f,
        rect.size.width + rect.origin.x,    rect.size.height  + rect.origin.y,  _maxS,  0.0f };
    
    _programState->setUniform(_mvpMatrixLocation, matrixMVP.m, sizeof(matrixMVP.m));
    _customCommand.updateVertexBuffer(vertexData, sizeof(vertexData));
    Director::getInstance()->getRenderer()->addCommand(&_customCommand);
}

NS_CC_END
