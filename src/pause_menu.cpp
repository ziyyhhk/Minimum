#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <vector>
#include <minimum.hpp>

using namespace geode::prelude;

// Pause menu button + Minimum popup (all platforms).
//
// A round logo button in the top right corner of the pause menu opens a small popup with
// the quick toggles, a button to the full settings page, and the credits.

namespace {

    // Credits shown in the popup.
    constexpr char const* kDeveloper = "ziyyhhk";
    constexpr char const* kHelper = "Rafa";
    constexpr char const* kTesters = "Rafa, Broken Team, ziyyhhk";

    // The mod logo, scaled so its height is `size`. Falls back to a gold "M" if the image
    // is missing, so the button is never invisible.
    CCNode* makeLogo(float size) {
        if (auto* logo = CCSprite::create("logo.png"_spr)) {
            float height = logo->getContentSize().height;
            if (height > 0.f) logo->setScale(size / height);
            return logo;
        }
        auto* fallback = CCLabelBMFont::create("M", "goldFont.fnt");
        fallback->setScale(size / 45.f);
        return fallback;
    }

}

class MinimumPopup : public Popup {
protected:
    std::vector<char const*> m_keys;

    bool init() {
        if (!Popup::init(340.f, 270.f, "square01_001.png")) return false;

        this->setTitle("Minimum");

        // Logo, top right (the close button lives in the top left).
        m_mainLayer->addChildAtPosition(makeLogo(40.f), Anchor::TopRight, ccp(-34.f, -34.f));

        // ---- Quick toggles ---------------------------------------------------------------
        struct ToggleDef {
            char const* key;
            char const* label;
        };
        std::vector<ToggleDef> toggles = {
            {"mod-enabled", "Minimum"},
            {"show-stats", "FPS counter"},
            {"skip-idle-particles", "Skip idle particles"},
            {"cap-particles", "Cap particles"},
        };
#ifdef GEODE_IS_DESKTOP
        toggles.push_back({"low-latency", "Low latency"});
#endif

        auto* mod = Mod::get();
        for (size_t i = 0; i < toggles.size(); ++i) {
            float const column = static_cast<float>(i % 2);
            float const row = static_cast<float>(i / 2);
            CCPoint const position = ccp(-148.f + column * 160.f, 66.f - row * 30.f);

            auto* toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(MinimumPopup::onToggle), .6f
            );
            toggler->setTag(static_cast<int>(i));
            toggler->toggle(mod->getSettingValue<bool>(toggles[i].key));
            m_buttonMenu->addChildAtPosition(toggler, Anchor::Center, position);

            auto* label = CCLabelBMFont::create(toggles[i].label, "bigFont.fnt");
            label->setScale(.35f);
            m_buttonMenu->addChildAtPosition(label, Anchor::Center, position + ccp(20.f, 0.f), ccp(0.f, .5f));

            m_keys.push_back(toggles[i].key);
        }

        // ---- Full settings -----------------------------------------------------------------
        auto* settingsSpr = ButtonSprite::create("All Settings", "goldFont.fnt", "GJ_button_01.png", .8f);
        settingsSpr->setScale(.8f);
        auto* settingsBtn = CCMenuItemSpriteExtra::create(
            settingsSpr, this, menu_selector(MinimumPopup::onAllSettings)
        );
        m_buttonMenu->addChildAtPosition(settingsBtn, Anchor::Center, ccp(0.f, -32.f));

        // ---- Credits -----------------------------------------------------------------------
        auto* creditsTitle = CCLabelBMFont::create("Credits", "goldFont.fnt");
        creditsTitle->setScale(.5f);
        m_mainLayer->addChildAtPosition(creditsTitle, Anchor::Center, ccp(0.f, -66.f));

        float y = -84.f;
        for (auto const& line : {
            std::string("Developer: ") + kDeveloper,
            std::string("Helper: ") + kHelper,
            std::string("Testers: ") + kTesters,
        }) {
            auto* text = CCLabelBMFont::create(line.c_str(), "chatFont.fnt");
            text->setScale(.7f);
            m_mainLayer->addChildAtPosition(text, Anchor::Center, ccp(0.f, y));
            y -= 15.f;
        }

        return true;
    }

    void onToggle(CCObject* sender) {
        auto* toggler = static_cast<CCMenuItemToggler*>(sender);
        size_t const index = static_cast<size_t>(toggler->getTag());
        if (index >= m_keys.size()) return;

        // Flip from the stored setting (the source of truth), not from the toggler state:
        // the toggler reports its old state inside its own callback.
        auto* mod = Mod::get();
        bool const next = !mod->getSettingValue<bool>(m_keys[index]);
        mod->setSettingValue<bool>(m_keys[index], next);
        minimum::refreshConfig();
    }

    void onAllSettings(CCObject*) {
        this->onClose(nullptr);
        openSettingsPopup(Mod::get(), true);
    }

public:
    static MinimumPopup* create() {
        auto* ret = new MinimumPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(MinimumPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto const winSize = CCDirector::get()->getWinSize();
#ifdef GEODE_IS_MOBILE
        constexpr float margin = 38.f; // stay clear of notches and rounded corners
#else
        constexpr float margin = 28.f;
#endif

        auto* button = CCMenuItemSpriteExtra::create(
            makeLogo(34.f), this, menu_selector(MinimumPauseLayer::onMinimumButton)
        );
        button->setID("settings-button"_spr);

        auto* menu = CCMenu::create();
        menu->setID("settings-menu"_spr);
        menu->setPosition({winSize.width - margin, winSize.height - margin});
        button->setPosition({0.f, 0.f});
        menu->addChild(button);

        // Give the new menu the same touch priority as the game's own pause menus,
        // otherwise the pause layer can swallow the tap.
        if (auto* children = this->getChildren()) {
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                if (auto* other = typeinfo_cast<CCMenu*>(child)) {
                    menu->setTouchPriority(other->getTouchPriority());
                    break;
                }
            }
        }

        this->addChild(menu, 100);
    }

    void onMinimumButton(CCObject*) {
        if (auto* popup = MinimumPopup::create()) {
            popup->show();
        }
    }
};
