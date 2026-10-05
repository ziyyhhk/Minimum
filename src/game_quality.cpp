#include <Geode/Geode.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Low detail mode.
//
// The game has its own Low Detail Mode (Options). It skips decoration effects such as the
// speed particles and the ring wave when the player lands, and it is the one setting that
// makes a big difference in decoration heavy levels. The setting here only switches that
// same option on for as long as Minimum is on, so it can be flipped from the pause menu.
//
// Only the in-memory value is changed. The saved game variable is never written, so closing
// the game always leaves the player's own Options exactly as they were. When the setting is
// switched off (or Minimum is) the value from before is put back.
//
// Some parts of a level are built when it starts, so the full effect shows from the next
// attempt after a level is entered.

namespace minimum {

    namespace {
        bool g_forced = false;
        bool g_valueBefore = false;
    }

    void applyGameQuality() {
        auto const& cfg = config();
        bool const want = cfg.enabled && cfg.lowDetail;
        if (want == g_forced) return;

        auto* manager = GameManager::get();
        if (!manager) return;

        if (want) {
            g_valueBefore = manager->m_performanceMode;
            manager->m_performanceMode = true;
            g_forced = true;
            log::info("Minimum: low detail mode on (was {})", g_valueBefore ? "on" : "off");
        }
        else {
            manager->m_performanceMode = g_valueBefore;
            g_forced = false;
            log::info("Minimum: low detail mode back to {}", g_valueBefore ? "on" : "off");
        }
    }

}
