#pragma once

namespace SidetonePolicy {
enum class Action { None, Adopt, Apply };

// -1 denotes a missing read or a missing baseline for the active headset.
constexpr Action decide(bool readable, int observed, int lastApplied, int desired, bool userRequested)
{
    if (readable && observed >= 0 && observed <= 128 && observed != lastApplied)
        return Action::Adopt;
    if (desired != lastApplied && (!readable || lastApplied >= 0 || userRequested))
        return Action::Apply;
    return Action::None;
}
}
