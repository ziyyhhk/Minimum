#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <minimum.hpp>
#include <minimum_logic.hpp>

using namespace geode::prelude;

// Pause menu button + Minimum popup (all platforms).
//
// A round logo button in the pause menu opens a popup with a live FPS card, the presets, the quick
// toggles, a button to the full settings page, and the credits.
//
// The button used to be dropped at one fixed point of the screen. Other buttons (the game's own
// ones and the ones other mods add) live in the same corner, so it could sit on top of them, and
// because it was added last it also lost the tap. Now it
//   * looks at every other button in the pause menu and slides to the nearest free spot,
//   * gets a touch priority that is checked before all the other menus,
//   * can be moved to another corner (or hidden) in the settings.
//
// Hidden: five quick taps on the live FPS card start a 60 second benchmark recording (see
// benchmark.cpp). The card shows the countdown while it runs.

namespace {

    // Credits shown in the popup.
    constexpr char const* kDeveloper = "ziyyhhk";
    constexpr char const* kHelper = "Rafa";
    constexpr char const* kTesters = "Rafa, Broken Team, ziyyhhk, Valicc, L4ZY";

    // ---- Look of the popup. The layout itself (sizes, spacing, rows) is computed by
    // minimum::planPopup in minimum_logic.hpp and unit tested; these are only the colors. -----
    constexpr uint8_t kCardOpacity = 26;      // toggle cards, over the dark popup (0-255)
    constexpr uint8_t kLiveOpacity = 44;      // the live FPS card
    constexpr uint8_t kPresetOpacity = 26;    // preset buttons that are not selected
    constexpr uint8_t kPresetSelectedOpacity = 150;

    // ---- The pause menu button -------------------------------------------------------------
    // kButtonSize is the round logo you see, kButtonHit is the area that reacts to a tap. The
    // tap area is bigger than the picture so it is easy to hit with a thumb without the logo
    // itself looking huge.
#ifdef GEODE_IS_MOBILE
    constexpr float kButtonSize = 30.f;
    constexpr float kButtonHit = 42.f;
    constexpr float kButtonMargin = 38.f; // stay clear of notches and rounded corners
#else
    constexpr float kButtonSize = 26.f;
    constexpr float kButtonHit = 34.f;
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

    // The round mod logo, scaled so its height is `size`. minimum::createLogoSprite never hands
    // out the pink "missing texture" square; if even the built-in copy fails, a gold "M".
    CCNode* makeLogo(float size) {
        if (auto* logo = minimum::createLogoSprite()) {
            float height = logo->getContentSize().height;
            if (height > 0.f) logo->setScale(size / height);
            return logo;
        }
        auto* fallback = CCLabelBMFont::create("M", "goldFont.fnt");
        fallback->setScale(size / 45.f);
        return fallback;
    }

    // What the pause menu button is made of: the round logo in the middle of a bigger
    // (invisible) tap area.
    CCNode* makeButtonVisual() {
        auto* holder = CCNode::create();
        holder->setContentSize({kButtonHit, kButtonHit});
        auto* logo = makeLogo(kButtonSize);
        logo->setPosition({kButtonHit / 2.f, kButtonHit / 2.f});
        holder->addChild(logo);
        return holder;
    }

    // A soft rounded rectangle, used behind the toggles and the credits.
    CCScale9Sprite* makePanel(float width, float height, uint8_t opacity = kCardOpacity) {
        auto* panel = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        if (!panel) return nullptr;
        panel->setColor(ccc3(255, 255, 255));
        panel->setOpacity(opacity);
        panel->setContentSize({width, height});
        return panel;
    }

    // ---- Finding a free spot for the button -------------------------------------------------

    // True when `node` or one of its parents, up to `layer`, is hidden. Such a button is not on
    // screen, so it must not block a spot.
    bool hasHiddenAncestor(CCNode* layer, CCNode* node) {
        for (CCNode* current = node; current && current != layer; current = current->getParent()) {
            if (!current->isVisible()) return true;
        }
        return false;
    }

    // Collects the on-screen rectangle (in `layer` coordinates) of every visible button under
    // `node`, except the ones below `skip` (our own menu) and the ones with a hidden ancestor.
    void collectBlockers(
        CCNode* layer, CCNode* node, CCNode* skip, float maxSize, std::vector<minimum::Box>& out
    ) {
        if (!node || node == skip) return;
        if (hasHiddenAncestor(layer, node)) return;

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

    // Where the button goes before the free-spot search has run: the chosen corner itself.
    CCPoint preferredPoint() {
        auto const win = CCDirector::get()->getWinSize();
        Corner const corner = readCorner();
        bool const right = corner == Corner::TopRight || corner == Corner::BottomRight;
        bool const top = corner == Corner::TopRight || corner == Corner::TopLeft;
        return ccp(right ? win.width - kButtonMargin : kButtonMargin, top ? win.height - kButtonMargin : kButtonMargin);
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
            preferred, kButtonHit, blockers, area,
            right ? -1.f : 1.f, top ? -1.f : 1.f,
            kButtonHit + 6.f, 8, 3, 4.f
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

// The popup opens in small steps. The frame that opens it only builds the frame, the title and
// the live FPS card; presets, toggles and the footer follow one per frame, so no single frame
// has to create every label and card at once (that is what made it hitch). Each step that takes
// more than 10 ms is written to the Geode log ("Minimum popup: ... took ... ms") so a slow
// device can be diagnosed.
class MinimumPopup : public Popup {
protected:
    using Clock = std::chrono::steady_clock;

    // How long the game's show animation runs before the fit scale is applied for good.
    // UNVERIFIED against the game: see the report.
    static constexpr float kShowAnimationSec = 0.6f;

    struct ToggleDef {
        char const* key;
        char const* label;
    };
    struct PresetButton {
        CCScale9Sprite* background = nullptr;
        char const* name = "";
    };

    static constexpr size_t kPresetCount = 5;
    static constexpr char const* kPresetNames[kPresetCount] = {
        "Custom", "Balanced", "Performance", "Extreme", "Super Performance"
    };

    std::vector<ToggleDef> m_toggles;
    std::vector<CCMenuItemToggler*> m_togglers;
    std::vector<PresetButton> m_presetButtons;
    minimum::PopupPlan m_plan;
    CCSize m_size;
    int m_stage = 0;
    CCLabelBMFont* m_fpsLabel = nullptr;
    CCLabelBMFont* m_lowLabel = nullptr;
    CCLabelBMFont* m_frameLabel = nullptr;
    std::string m_lastFps;
    std::string m_lastLow;
    std::string m_lastFrame;

    // What a toggle is really doing right now. A preset can override a few settings, so the
    // stored value is not always the truth, the active config is.
    static bool effectiveValue(char const* key) {
        auto const& cfg = minimum::config();
        if (!std::strcmp(key, "mod-enabled")) return cfg.enabled;
        if (!std::strcmp(key, "show-stats")) return cfg.showStats;
        if (!std::strcmp(key, "skip-idle-particles")) return cfg.skipIdleParticles;
        if (!std::strcmp(key, "cap-particles")) return cfg.capParticles;
        if (!std::strcmp(key, "low-latency")) return cfg.lowLatency;
        if (!std::strcmp(key, "low-detail")) return cfg.lowDetail;
        return Mod::get()->getSettingValue<bool>(key);
    }

    static bool controlledByPreset(char const* key) {
        return !std::strcmp(key, "skip-idle-particles")
            || !std::strcmp(key, "cap-particles")
            || !std::strcmp(key, "low-detail");
    }

    // Offset of the center of `box` (popup coordinates, origin = lower left corner) from the
    // center of the popup.
    CCPoint centerOf(minimum::Box const& box) const {
        return ccp(box.x + box.w / 2.f - m_size.width / 2.f, box.y + box.h / 2.f - m_size.height / 2.f);
    }

    static void fitLabel(CCLabelBMFont* label, float maxScale, float room) {
        float const natural = label->getContentSize().width;
        label->setScale(natural > 0.f ? std::min(maxScale, room / natural) : maxScale);
    }

    static void logSlow(char const* what, Clock::time_point since) {
        double const ms = std::chrono::duration<double, std::milli>(Clock::now() - since).count();
        if (ms >= 10.0) log::info("Minimum popup: {} took {:.1f} ms", what, ms);
    }

    static double steadySeconds() {
        return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    }

    bool init() {
        auto const started = Clock::now();

        m_toggles = {
            {"mod-enabled", "Minimum"},
            {"show-stats", "FPS counter"},
            {"skip-idle-particles", "Skip idle draws"},
            {"cap-particles", "Cap particles"},
            {"low-latency", "Low latency"},
            {"low-detail", "Low detail"},
        };

        auto const win = CCDirector::get()->getWinSize();
        m_plan = minimum::planPopup(win.height - 16.f, m_toggles.size(), kPresetCount);
        m_size = CCSize{m_plan.width, m_plan.height};

        if (!Popup::init(m_plan.width, m_plan.height, "square01_001.png")) return false;
        this->setTitle("Minimum");

        // Logo, top right, level with the title (the close button lives in the top left).
        m_mainLayer->addChildAtPosition(makeLogo(28.f), Anchor::TopRight, ccp(-26.f, -22.f));

        // On a screen shorter than the popup the whole popup is scaled down by the fit scale, so
        // it fits (see planPopup). The scale is on the popup's own layer, which is centered.
        this->applyFit(0.f);

        this->buildLive();
        this->refreshLive(0.f);
        this->schedule(schedule_selector(MinimumPopup::refreshLive), 0.25f);

        // The rest follows one part per frame.
        this->schedule(schedule_selector(MinimumPopup::buildStep), 0.f);

        logSlow("opening frame", started);
        return true;
    }

    // Applies the fit scale to the popup layer. Runs once at creation and again after the show
    // animation (which scales the layer in from nothing), so the fit is what the player sees.
    void applyFit(float) {
        if (m_mainLayer) m_mainLayer->setScale(m_plan.fitScale);
    }

    void buildStep(float) {
        auto const started = Clock::now();
        char const* what = "";
        switch (m_stage) {
            case 0: this->buildPresets(); what = "presets"; break;
            case 1: this->buildToggles(0, std::min<size_t>(3, m_toggles.size())); what = "toggles 1"; break;
            case 2: this->buildToggles(std::min<size_t>(3, m_toggles.size()), m_toggles.size()); what = "toggles 2"; break;
            case 3: this->buildFooter(); what = "footer"; break;
            default: break;
        }
        logSlow(what, started);

        if (++m_stage > 3) this->unschedule(schedule_selector(MinimumPopup::buildStep));
    }

    // ---- Live card ---------------------------------------------------------------------------
    void buildLive() {
        auto const c = centerOf(m_plan.live);
        // The card is a button in disguise: five quick taps on it start the hidden benchmark.
        if (auto* panel = makePanel(m_plan.live.w, m_plan.live.h, kLiveOpacity)) {
            auto* card = CCMenuItemSpriteExtra::create(panel, this, menu_selector(MinimumPopup::onLiveTap));
            m_buttonMenu->addChildAtPosition(card, Anchor::Center, c);
        }

        float const left = c.x - m_plan.live.w / 2.f;
        m_fpsLabel = CCLabelBMFont::create("--", "bigFont.fnt");
        m_fpsLabel->setScale(.85f);
        m_buttonMenu->addChildAtPosition(m_fpsLabel, Anchor::Center, ccp(left + 62.f, c.y + 5.f));

        auto* caption = CCLabelBMFont::create("FPS", "goldFont.fnt");
        caption->setScale(.4f);
        m_buttonMenu->addChildAtPosition(caption, Anchor::Center, ccp(left + 62.f, c.y - m_plan.live.h / 2.f + 11.f));

        float const right = c.x + m_plan.live.w / 2.f;
        m_lowLabel = CCLabelBMFont::create("1% low --", "chatFont.fnt");
        m_lowLabel->setScale(.8f);
        m_buttonMenu->addChildAtPosition(m_lowLabel, Anchor::Center, ccp(right - 14.f, c.y + 8.f), ccp(1.f, .5f));

        m_frameLabel = CCLabelBMFont::create("frame -- ms", "chatFont.fnt");
        m_frameLabel->setScale(.8f);
        m_frameLabel->setOpacity(190);
        m_buttonMenu->addChildAtPosition(m_frameLabel, Anchor::Center, ccp(right - 14.f, c.y - 8.f), ccp(1.f, .5f));
    }

    // ---- Presets: 3 in the first row, 2 in the second ---------------------------------------
    void buildPresets() {
        for (size_t i = 0; i < m_plan.presets.size() && i < kPresetCount; ++i) {
            auto const& box = m_plan.presets[i];

            auto* holder = CCNode::create();
            holder->setContentSize({box.w, box.h});

            auto* background = makePanel(box.w, box.h, kPresetOpacity);
            if (background) {
                background->setPosition({box.w / 2.f, box.h / 2.f});
                holder->addChild(background);
            }
            auto* label = CCLabelBMFont::create(kPresetNames[i], "bigFont.fnt");
            fitLabel(label, .4f, box.w - 12.f);
            label->setPosition({box.w / 2.f, box.h / 2.f + 1.f});
            holder->addChild(label);

            auto* button = CCMenuItemSpriteExtra::create(holder, this, menu_selector(MinimumPopup::onPreset));
            button->setTag(static_cast<int>(i));
            m_buttonMenu->addChildAtPosition(button, Anchor::Center, centerOf(box));

            m_presetButtons.push_back({background, kPresetNames[i]});
        }
        this->syncPresetButtons();
    }

    // ---- Quick toggles: one card each -------------------------------------------------------
    void buildToggles(size_t first, size_t last) {
        for (size_t i = first; i < last; ++i) {
            auto const& box = m_plan.toggles[i];
            auto const c = centerOf(box);
            float const left = c.x - box.w / 2.f;

            // Card first, so it is drawn behind the toggle and the label.
            if (auto* panel = makePanel(box.w, box.h)) {
                m_buttonMenu->addChildAtPosition(panel, Anchor::Center, c);
            }

            auto* toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(MinimumPopup::onToggle), .55f
            );
            toggler->setTag(static_cast<int>(i));
            toggler->toggle(effectiveValue(m_toggles[i].key));
            m_buttonMenu->addChildAtPosition(toggler, Anchor::Center, ccp(left + 20.f, c.y));
            m_togglers.push_back(toggler);

            // Label: as large as fits inside the card.
            auto* label = CCLabelBMFont::create(m_toggles[i].label, "bigFont.fnt");
            fitLabel(label, .45f, box.w - 38.f - 8.f);
            m_buttonMenu->addChildAtPosition(label, Anchor::Center, ccp(left + 38.f, c.y), ccp(0.f, .5f));
        }
    }

    // ---- Full settings button and the credits (two lines) -----------------------------------
    void buildFooter() {
        {
            auto* sprite = ButtonSprite::create("All Settings", "goldFont.fnt", "GJ_button_01.png", .8f);
            sprite->setScale(.75f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(MinimumPopup::onAllSettings));
            m_buttonMenu->addChildAtPosition(button, Anchor::Center, centerOf(m_plan.settingsButton));
        }

        auto const c = centerOf(m_plan.credits);
        std::string const first = std::string("Developer ") + kDeveloper + "   |   Helper " + kHelper;
        std::string const second = std::string("Testers: ") + kTesters;

        auto* line1 = CCLabelBMFont::create(first.c_str(), "chatFont.fnt");
        fitLabel(line1, .62f, m_plan.credits.w);
        line1->setOpacity(190);
        m_buttonMenu->addChildAtPosition(line1, Anchor::Center, ccp(c.x, c.y + 6.5f));

        auto* line2 = CCLabelBMFont::create(second.c_str(), "chatFont.fnt");
        fitLabel(line2, .62f, m_plan.credits.w);
        line2->setOpacity(190);
        m_buttonMenu->addChildAtPosition(line2, Anchor::Center, ccp(c.x, c.y - 6.5f));
    }

    // Live numbers, refreshed four times a second while the popup is open.
    void refreshLive(float) {
        if (!m_fpsLabel) return;
        auto const& cfg = minimum::config();
        auto& counters = minimum::counters();
        auto& stats = minimum::frameStats();

        uint32_t const fps = counters.fps.load();
        char buffer[96];

        if (fps == 0) std::snprintf(buffer, sizeof(buffer), "--");
        else std::snprintf(buffer, sizeof(buffer), "%u", fps);
        if (m_lastFps != buffer) {
            m_lastFps = buffer;
            m_fpsLabel->setString(buffer);
        }
        // No reading yet is shown in a neutral color. It is not the Bad (red) band, which would say
        // the game is slow when nothing has been measured yet.
        if (fps == 0) {
            m_fpsLabel->setColor(ccc3(235, 235, 235));
        }
        else {
            switch (minimum::classifyFps(fps, cfg.targetFps)) {
                case minimum::FpsBand::Good: m_fpsLabel->setColor(ccc3(120, 255, 120)); break;
                case minimum::FpsBand::Okay: m_fpsLabel->setColor(ccc3(255, 225, 90)); break;
                case minimum::FpsBand::Bad: m_fpsLabel->setColor(ccc3(255, 100, 100)); break;
            }
        }

        double const low = stats.lowFps(240);
        if (low > 0.5) std::snprintf(buffer, sizeof(buffer), "1%% low %u", static_cast<uint32_t>(low + 0.5));
        else std::snprintf(buffer, sizeof(buffer), "1%% low --");
        if (m_lastLow != buffer) {
            m_lastLow = buffer;
            m_lowLabel->setString(buffer);
        }

        // Second line: frame time, whether Low Latency Mode is really running right now, and what
        // limits the frame (CPU or GPU). While a benchmark records, its countdown replaces it.
        char const* sync = "";
        if (cfg.enabled && cfg.lowLatency) {
            switch (static_cast<minimum::LatencyState>(counters.latencyState.load(std::memory_order_relaxed))) {
                case minimum::LatencyState::Active: sync = "  |  sync on"; break;
                case minimum::LatencyState::Waiting: sync = "  |  sync waiting"; break;
                case minimum::LatencyState::Unavailable: sync = "  |  sync n/a"; break;
                default: break;
            }
        }
        char logic[24] = "";
        uint32_t const logicRate = counters.logicFps.load(std::memory_order_relaxed);
        if (minimum::logicRateDiffers(fps, logicRate)) {
            std::snprintf(logic, sizeof(logic), "  |  logic %u", logicRate);
        }
        char limit[32] = "";
        char const* limitLabel = minimum::bottleneckLabel(
            static_cast<minimum::Bottleneck>(counters.bottleneck.load(std::memory_order_relaxed))
        );
        if (*limitLabel) std::snprintf(limit, sizeof(limit), "  |  %s", limitLabel);

        double const frameMs = stats.averageFrameMs(60);
        double const recording = minimum::benchmarkRemainingSec(steadySeconds());
        if (!cfg.enabled) std::snprintf(buffer, sizeof(buffer), "Minimum is off");
        else if (recording > 0.0) std::snprintf(buffer, sizeof(buffer), "benchmark: %.0f s left", std::ceil(recording));
        else if (frameMs > 0.05) std::snprintf(buffer, sizeof(buffer), "frame %.1f ms%s%s%s", frameMs, logic, sync, limit);
        else std::snprintf(buffer, sizeof(buffer), "frame -- ms%s%s%s", logic, sync, limit);
        if (m_lastFrame != buffer) {
            m_lastFrame = buffer;
            m_frameLabel->setString(buffer);
            // Keep the line inside the card, to the right of the FPS number.
            fitLabel(m_frameLabel, .8f, m_plan.live.w - 104.f);
        }
    }

    void syncPresetButtons() {
        auto const current = Mod::get()->getSettingValue<std::string>("preset");
        for (auto const& button : m_presetButtons) {
            if (!button.background) continue;
            bool const selected = current == button.name;
            button.background->setColor(selected ? ccc3(255, 200, 60) : ccc3(255, 255, 255));
            button.background->setOpacity(selected ? kPresetSelectedOpacity : kPresetOpacity);
        }
    }

    void syncTogglers() {
        for (size_t i = 0; i < m_togglers.size(); ++i) {
            m_togglers[i]->toggle(effectiveValue(m_toggles[i].key));
        }
    }

    void onPreset(CCObject* sender) {
        int const index = static_cast<CCNode*>(sender)->getTag();
        if (index < 0 || index >= static_cast<int>(kPresetCount)) return;

        if (index == 0) {
            // "Custom" keeps what the previous preset was doing instead of snapping back to
            // whatever the individual settings happened to say.
            minimum::leavePreset();
        }
        else {
            Mod::get()->setSettingValue<std::string>("preset", kPresetNames[index]);
            minimum::refreshConfig();
        }
        this->syncPresetButtons();
        this->syncTogglers();
    }

    void onToggle(CCObject* sender) {
        auto* toggler = static_cast<CCMenuItemToggler*>(sender);
        size_t const index = static_cast<size_t>(toggler->getTag());
        if (index >= m_toggles.size()) return;
        char const* key = m_toggles[index].key;

        // A preset would silently undo this click on the next refresh, so step out of it first.
        if (controlledByPreset(key)) {
            minimum::leavePreset();
            this->syncPresetButtons();
        }

        // Flip from the stored setting (the source of truth), not from the toggler state:
        // the toggler reports its old state inside its own callback.
        auto* mod = Mod::get();
        bool const next = !mod->getSettingValue<bool>(key);
        mod->setSettingValue<bool>(key, next);
        minimum::refreshConfig();
    }

    void onAllSettings(CCObject*) {
        this->onClose(nullptr);
        openSettingsPopup(Mod::get(), true);
    }

    // Hidden benchmark: each tap on the live card counts; five quick taps start the recording.
    void onLiveTap(CCObject*) {
        minimum::noteSecretTap(steadySeconds());
        this->refreshLive(0.f);
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

    // Geode's show animation scales the popup in, so the fit scale is applied again once it has
    // played.
    void show() {
        Popup::show();
        this->applyFit(0.f);
        this->scheduleOnce(schedule_selector(MinimumPopup::applyFit), kShowAnimationSec);
    }
};

class $modify(MinimumPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        if (readCorner() == Corner::Hidden) return;
        auto const started = std::chrono::steady_clock::now();

        auto* button = CCMenuItemSpriteExtra::create(
            makeButtonVisual(), this, menu_selector(MinimumPauseLayer::onMinimumButton)
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

        // The button starts in the chosen corner right away (so it is never missing). A frame
        // later, when the other mods have added their buttons too, it slides to the nearest
        // free spot. Only that second step looks at the other buttons, which keeps opening the
        // pause menu cheap.
        menu->setPosition(preferredPoint());
        this->addChild(menu, 100);
        if (auto* placer = ButtonPlacer::create(menu)) {
            this->addChild(placer);
        }

        double const ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        if (ms >= 10.0) log::info("Minimum: pause button setup took {:.1f} ms", ms);
    }

    void onMinimumButton(CCObject*) {
        if (auto* popup = MinimumPopup::create()) {
            popup->show();
        }
    }
};
