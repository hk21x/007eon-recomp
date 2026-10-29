// A library in driving.xbe (0x23D460 to 0x240E20) keeps the slot it works on (one of 4, 0x928 bytes from 0x36FE10)
// in 0x3722B0, and its entry points shut each other out with a plain flag at 0x3722C0: busy (-0x12) when it is set,
// else set, and cleared with the slot pointer on the way out. On the console's one CPU that held; here the title's
// threads run at once, two got in, and the one leaving cleared 0x3722B0 under the other: a player's driving mission
// stopped on a read of 0x38 (0x23E420 reading [0x3722B0] + 0x38). The entry points are run one thread at a time. A
// thread that waits too long goes in as before, so no wait of the library's own can deadlock.
#include <chrono>
#include <cstdint>
#include <mutex>

#include "core/log.h"
#include "xdk/hle.h"
#include "common/release.h"

namespace {
std::recursive_timed_mutex g_lock;

struct LibraryEntry {
    bool held;
    LibraryEntry() : held(g_lock.try_lock_for(std::chrono::milliseconds(100))) {
        static bool told = false;
        if (!held && !told) {
            told = true;
            LOGR("driving: a thread waited 100 ms for the library at 0x23D460 and went in alongside");
        }
    }
    ~LibraryEntry() {
        if (held) g_lock.unlock();
    }
};
}  // namespace

// an entry point, its European and USA addresses
#define LIBRARY_ENTRY(eu, us)              \
    X86_EXTERN_FUNC(EON_ORIG(eu, us));     \
    X86_FUNC(EON_FN(eu, us)) {             \
        LibraryEntry entry;                \
        EON_ORIG(eu, us)(ctx, base);       \
    }

LIBRARY_ENTRY(0023D510, 0023D4D0)
LIBRARY_ENTRY(0023D770, 0023D730)
LIBRARY_ENTRY(0023DA80, 0023DA40)
LIBRARY_ENTRY(0023E080, 0023E040)
LIBRARY_ENTRY(0023E1A0, 0023E160)
LIBRARY_ENTRY(0023E260, 0023E220)
LIBRARY_ENTRY(0023E320, 0023E2E0)
LIBRARY_ENTRY(0023E420, 0023E3E0)
LIBRARY_ENTRY(0023E650, 0023E610)
LIBRARY_ENTRY(0023ED80, 0023ED40)
LIBRARY_ENTRY(0023EE80, 0023EE40)
LIBRARY_ENTRY(0023F4E0, 0023F4A0)
LIBRARY_ENTRY(00240E20, 00240DE0)
