#include <Geode/Geode.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

namespace minimum {
    extern unsigned char const kLogoRoundPng[];
    extern std::size_t const kLogoRoundPngSize;
}

// The logo for the pause menu button.
//
// Sprites only exist in a mod if they are listed under "resources" > "sprites" in mod.json and
// the PNG is in the repository. When one of the two is missing, CCSprite::create does not
// return null: it returns a 2 x 2 pink and black "missing texture" sprite, which then got
// scaled up to the size of the button. So a sprite is only trusted when it is a real image
// (taller than a few pixels). Otherwise the PNG that is built into the mod is used.

namespace minimum {

    CCSprite* createLogoSprite() {
        if (auto* packaged = CCSprite::create("logo_round.png"_spr)) {
            if (packaged->getContentSize().height >= 8.f) return packaged;
        }

        static char const* const key = "minimum-embedded-logo-round";
        auto* cache = CCTextureCache::sharedTextureCache();
        CCTexture2D* texture = cache->textureForKey(key);
        if (!texture) {
            auto* image = new CCImage();
            if (image->initWithImageData(
                    const_cast<unsigned char*>(kLogoRoundPng),
                    static_cast<int>(kLogoRoundPngSize),
                    CCImage::kFmtPng
                )) {
                texture = cache->addUIImage(image, key);
            }
            image->release();
        }
        if (!texture) {
            log::warn("Minimum: could not load the logo, using a text button instead");
            return nullptr;
        }
        return CCSprite::createWithTexture(texture);
    }

}
