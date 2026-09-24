#include <shared_pool.hpp>

// Thread pool is header-implemented. This TU ensures the pool symbol is linked
// and can be extended with warm-up later.
namespace Minimum {
    void warmPool() {
        (void)pool().size();
    }
}
