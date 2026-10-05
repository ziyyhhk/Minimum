#include <Geode/Geode.hpp>
#include <minimum.hpp>

using namespace geode::prelude;

// Entry point. All the real work lives in the hooks (frame_gate.cpp,
// particles.cpp, app_delegate.cpp, mobile_fps.cpp). Here we only fill the
// settings snapshot once so the hooks have valid values from the first frame.

$on_mod(Loaded) {
    minimum::refreshConfig();
}
