#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <minimum.hpp>
#include <minimum_logic.hpp>

using namespace geode::prelude;

// Pause menu button + Minimum popup (all platforms).
//
// A round logo button in the pause menu opens a small popup with the quick toggles, a button to
// the full settings page, and the credits.
//
// The button used to be dropped at one fixed point of the screen. Other buttons (the game's own
// ones and the ones other mods add) live in the same corner, so it could sit on top of them, and
// because it was added last it also lost the tap. Now it
//   * looks at every other button in the pause menu and slides to the nearest free spot,
//   * gets a touch priority that is checked before all the other menus,
//   * can be moved to another corner (or hidden) in the settings.

namespace {

    // ---- Look of the popup ----------------------------------------------------------------
    constexpr uint8_t kPanelOpacity = 26; // toggle cards, over the black popup (0-255)

    // ---- The pause menu button ------------------------------------------------------------
    constexpr float kButtonSize = 36.f;
#ifdef GEODE_IS_MOBILE
    constexpr float kButtonMargin = 38.f; // stay clear of notches and rounded corners
#else
    constexpr float kButtonMargin = 28.f;
#endif

    enum class Corner { TopRight, TopLeft, BottomRight, BottomLeft, Hidden };

    Corner readCorner() {
        auto const value = Mod::get()->getSettingValue<std::string>("pause-button-corner");
        if (value == "Top Left") return Corner::TopLeft;
        if (value == "Bottom Right") return Corner::BottomRight;
        if (value == "Bottom Left") return Corner::BottomLeft;
        if (value == "Hidden") return Corner::Hidden;
        return Corner::TopRight;
    }

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

    // A soft rounded rectangle, used behind the toggles and the credits.
    CCScale9Sprite* makePanel(float width, float height) {
        auto* panel = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        if (!panel) return nullptr;
        panel->setColor(ccc3(255, 255, 255));
        panel->setOpacity(kPanelOpacity);
        panel->setContentSize({width, height});
        return panel;
    }

    // ---- Finding a free spot for the button -------------------------------------------------

    // Collects the on-screen rectangle (in `layer` coordinates) of every visible button under
    // `node`, except the ones below `skip` (our own menu).
    void collectBlockers(
        CCNode* layer, CCNode* node, CCNode* skip, float maxSize, std::vector<minimum::Box>& out
    ) {
        if (!node || node == skip || !node->isVisible()) return;

        bool const isButton = typeinfo_cast<CCMenuItem*>(node)
                           || typeinfo_cast<CCMenuItemSpriteExtra*>(node)
                           || typeinfo_cast<CCMenuItemToggler*>(node);
        if (isButton) {
            if (auto* parent = node->getParent()) {
                CCRect const rect = node->boundingBox();
                CCPoint const lo = layer->convertToNodeSpace(
                    parent->convertToWorldSpace(ccp(rect.origin.x, rect.origin.y))
                );
                CCPoint const hi = layer->convertToNodeSpace(
                    parent->convertToWorldSpace(ccp(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height))
                );
                minimum::Box const box{
                    std::min(lo.x, hi.x), std::min(lo.y, hi.y), std::fabs(hi.x - lo.x), std::fabs(hi.y - lo.y)
                };
                // Ignore empty boxes and huge ones (full screen "tap anywhere" areas), which
                // would block every spot and make the search useless.
                if (box.w > 0.f && box.h > 0.f && box.w < maxSize && box.h < maxSize) {
                    out.push_back(box);
                }
            }
        }

        if (auto* children = node->getChildren()) {
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                collectBlockers(layer, child, skip, maxSize, out);
            }
        }
    }

    // The lowest (= checked first) touch priority used by any menu below `node`.
    void lowestMenuPriority(CCNode* node, CCNode* skip, int& lowest, bool& found) {
        if (!node || node == skip) return;

        if (auto* menu = typeinfo_cast<CCMenu*>(node)) {
            int const priority = menu->getTouchPriority();
            if (!found || priority < lowest) {
                lowest = priority;
                found = true;
            }
        }

        if (auto* children = node->getChildren()) {
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                lowestMenuPriority(child, skip, lowest, found);
            }
        }
    }

    // Moves `menu` (which holds the button at its origin) to the nearest spot in the chosen
    // corner that does not touch another button.
    void placeMenu(CCNode* layer, CCNode* menu) {
        auto const win = CCDirector::get()->getWinSize();
        Corner const corner = readCorner();
        bool const right = corner == Corner::TopRight || corner == Corner::BottomRight;
        bool const top = corner == Corner::TopRight || corner == Corner::TopLeft;

        minimum::Vec2 const preferred{
            right ? win.width - kButtonMargin : kButtonMargin,
            top ? win.height - kButtonMargin : kButtonMargin,
        };

        std::vector<minimum::Box> blockers;
        collectBlockers(layer, layer, menu, win.width * 0.5f, blockers);

        minimum::Box const area{4.f, 4.f, win.width - 8.f, win.height - 8.f};
        // Slide along the edge first (up to 8 steps), then move inward (up to 3 rows).
        auto const spot = minimum::findFreeSpot(
            preferred, kButtonSize, blockers, area,
            right ? -1.f : 1.f, top ? -1.f : 1.f,
            kButtonSize + 8.f, 8, 3, 4.f
        );
        menu->setPosition({spot.center.x, spot.center.y});
    }

    // Runs once, one frame after the pause menu was built. By then every other mod has added
    // its own buttons, so the placement can take them into account.
    class ButtonPlacer : public CCNode {
    protected:
        Ref<CCNode> m_menu;

        bool setup(CCNode* menu) {
            if (!CCNode::init()) return false;
            m_menu = menu;
            this->scheduleOnce(schedule_selector(ButtonPlacer::place), 0.f);
            return true;
        }

        void place(float) {
            auto* layer = this->getParent();
            if (layer && m_menu.data()) placeMenu(layer, m_menu.data());
        }

    public:
        static ButtonPlacer* create(CCNode* menu) {
            auto* ret = new ButtonPlacer();
            if (ret && ret->setup(menu)) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }
    };

}

class MinimumPopup : public Popup {
protected:
    std::vector<std::string> m_keys;
    std::vector<CCMenuItemToggler*> m_togglers;
    CCLabelBMFont* m_fpsLabel = nullptr;

    bool init() {
        // ---- The quick toggles -------------------------------------------------------------
        struct ToggleDef {
            char const* key;
            char const* label;
        };
        std::vector<ToggleDef> toggles = {
            {"mod-enabled", "Minimum"},
            {"show-stats", "FPS counter"},
            {"skip-idle-particles", "Skip idle particles"},
            {"cap-particles", "Particle cap"},
            {"low-latency", "Low latency"},
        };
#ifdef GEODE_IS_MOBILE
        toggles.push_back({"fps-unlock", "Unlock FPS"});
#endif

        // ---- Layout: header, one full-width row per toggle, footer -------------------------
        size_t const count = toggles.size();
        float const width = 320.f;
        float const headerH = 62.f;
        float const rowH = 32.f;
        float const footerH = 78.f;
        float const height = headerH + rowH * static_cast<float>(count) + footerH;

        if (!Popup::init(width, height, "square01_001.png")) return false;

        this->setTitle("Minimum");
        m_title->setPositionY(m_title->getPositionY() + 4.f);

        // Logo next to the title.
        m_mainLayer->addChildAtPosition(makeLogo(30.f), Anchor::TopLeft, ccp(16.f, -20.f));

        // Live FPS, top right. Colored like the corner stats line.
        m_fpsLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_fpsLabel->setScale(.42f);
        m_mainLayer->addChildAtPosition(m_fpsLabel, Anchor::TopRight, ccp(-16.f, -24.f), ccp(1.f, .5f));

        // Version, centered under the title.
        auto* version = CCLabelBMFont::create("v3.2.1", "chatFont.fnt");
        version->setScale(.55f);
        version->setOpacity(140);
        m_mainLayer->addChildAtPosition(version, Anchor::Top, ccp(0.f, -44.f));

        // ---- Toggle rows: card, label left, switch right ------------------------------------
        auto* mod = Mod::get();
        float const rowsTop = height / 2.f - headerH;
        for (size_t i = 0; i < count; ++i) {
            float const cy = rowsTop - rowH * (static_cast<float>(i) + .5f);

            if (auto* panel = makePanel(width - 28.f, 26.f)) {
                m_mainLayer->addChildAtPosition(panel, Anchor::Center, ccp(0.f, cy));
            }

            auto* label = CCLabelBMFont::create(toggles[i].label, "bigFont.fnt");
            float const room = width - 28.f - 76.f;
            float const natural = label->getContentSize().width;
            label->setScale(natural > 0.f ? std::min(.4f, room / natural) : .4f);
            m_mainLayer->addChildAtPosition(
                label, Anchor::Center, ccp(-(width / 2.f - 26.f), cy), ccp(0.f, .5f)
            );

            auto* toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(MinimumPopup::onToggle), .5f
            );
            toggler->setTag(static_cast<int>(i));
            toggler->toggle(mod->getSettingValue<bool>(toggles[i].key));
            m_buttonMenu->addChildAtPosition(
                toggler, Anchor::Center, ccp(width / 2.f - 34.f, cy)
            );

            m_keys.push_back(toggles[i].key);
            m_togglers.push_back(toggler);
        }

        // ---- Buttons -----------------------------------------------------------------------
        float const buttonY = rowsTop - rowH * static_cast<float>(count) - 22.f;

        auto* settingsSpr = ButtonSprite::create("Settings", "goldFont.fnt", "GJ_button_01.png", .7f);
        auto* settingsBtn = CCMenuItemSpriteExtra::create(
            settingsSpr, this, menu_selector(MinimumPopup::onAllSettings)
        );
        m_buttonMenu->addChildAtPosition(settingsBtn, Anchor::Center, ccp(-52.f, buttonY));

        auto* githubSpr = ButtonSprite::create("GitHub", "goldFont.fnt", "GJ_button_02.png", .7f);
        auto* githubBtn = CCMenuItemSpriteExtra::create(
            githubSpr, this, menu_selector(MinimumPopup::onGitHub)
        );
        m_buttonMenu->addChildAtPosition(githubBtn, Anchor::Center, ccp(52.f, buttonY));

        // ---- Credits -----------------------------------------------------------------------
        auto* credits = CCLabelBMFont::create(
            "by ziyyhhk  |  helper: Rafa  |  testers: Broken Team", "chatFont.fnt"
        );
        credits->setScale(.5f);
        credits->setOpacity(160);
        m_mainLayer->addChildAtPosition(credits, Anchor::Bottom, ccp(0.f, 14.f));

        this->tick(0.f);
        this->schedule(schedule_selector(MinimumPopup::tick), 0.5f);
        return true;
    }

    void tick(float) {
        if (!m_fpsLabel) return;
        uint32_t const fps = minimum::counters().fps.load();

        char text[24];
        std::snprintf(text, sizeof(text), "%u FPS", fps);
        m_fpsLabel->setString(text);

        if (fps >= 55) m_fpsLabel->setColor(ccc3(120, 255, 120));
        else if (fps >= 30) m_fpsLabel->setColor(ccc3(255, 225, 90));
        else m_fpsLabel->setColor(ccc3(255, 100, 100));
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

        // Sync the visual state: the callback fired before the toggler flipped itself,
        // so force it to match the value we just stored.
        if (index < m_togglers.size()) m_togglers[index]->toggle(next);
    }

    void onAllSettings(CCObject*) {
        this->onClose(nullptr);
        openSettingsPopup(Mod::get(), true);
    }

    void onGitHub(CCObject*) {
        web::openLinkInBrowser("https://github.com/ziyyhhk/Minimum");
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

        if (readCorner() == Corner::Hidden) return;

        auto* button = CCMenuItemSpriteExtra::create(
            makeLogo(kButtonSize), this, menu_selector(MinimumPauseLayer::onMinimumButton)
        );
        button->setID("settings-button"_spr);

        auto* menu = CCMenu::create();
        menu->setID("settings-menu"_spr);
        button->setPosition({0.f, 0.f});
        menu->addChild(button);

        // Check our menu BEFORE every other menu in the pause layer. With an equal priority the
        // menu that registered first wins an overlap, and ours is added last, so it used to lose.
        int lowest = 0;
        bool found = false;
        lowestMenuPriority(this, menu, lowest, found);
        if (found) menu->setTouchPriority(lowest - 1);

        // First placement right away (so the button is never missing), then once more a frame
        // later when the other mods have added their buttons too.
        placeMenu(this, menu);
        this->addChild(menu, 100);
        if (auto* placer = ButtonPlacer::create(menu)) {
            this->addChild(placer);
        }
    }

    void onMinimumButton(CCObject*) {
        if (auto* popup = MinimumPopup::create()) {
            popup->show();
        }
    }
};
