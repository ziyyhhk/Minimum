#include <Geode/Geode.hpp>
using namespace geode::prelude;
#include <Geode/modify/CCTexture2D.hpp>

#ifdef GEODE_IS_WINDOWS
struct ForceRGBA8888 : Modify<ForceRGBA8888, CCTexture2D> {
    static void setDefaultAlphaPixelFormat(CCTexture2DPixelFormat format) {
        if (Mod::get()->getSettingValue<bool>("mod-enabled")
            && Mod::get()->getSettingValue<bool>("force-rgba8888"))
            format = kCCTexture2DPixelFormat_RGBA8888;
        CCTexture2D::setDefaultAlphaPixelFormat(format);
    }

    static CCTexture2DPixelFormat defaultAlphaPixelFormat() {
        if (Mod::get()->getSettingValue<bool>("mod-enabled")
            && Mod::get()->getSettingValue<bool>("force-rgba8888"))
            return kCCTexture2DPixelFormat_RGBA8888;
        return CCTexture2D::defaultAlphaPixelFormat();
    }
};
#endif
