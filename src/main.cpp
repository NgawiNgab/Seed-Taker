#include "core/Runtime.hpp"
#include <pl/Mod.hpp>

class SeedTakerMod {
public:
    static SeedTakerMod& instance() {
        static SeedTakerMod mod;
        return mod;
    }

    bool load(pl::mod::ModContext& context) { return seedtaker::core::Runtime::get().load(context); }
    bool enable(pl::mod::ModContext& context) { return seedtaker::core::Runtime::get().enable(context); }
    bool disable(pl::mod::ModContext& context) { return seedtaker::core::Runtime::get().disable(context); }
    bool unload(pl::mod::ModContext& context) { return seedtaker::core::Runtime::get().unload(context); }
};

PL_REGISTER_MOD(SeedTakerMod, SeedTakerMod::instance())
