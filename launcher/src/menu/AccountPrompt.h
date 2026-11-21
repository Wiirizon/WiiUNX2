#pragma once

#include <cstdint>

namespace nx {
constexpr bool shouldPromptForAccount(uint8_t defaultSlot, bool chosenThisBoot) {
    return defaultSlot == 0 && !chosenThisBoot;
}

constexpr bool accountChoiceMatchesBoot(uint64_t savedBootTime, uint64_t currentBootTime, uint64_t tolerance) {
    const uint64_t difference = savedBootTime > currentBootTime
        ? savedBootTime - currentBootTime
        : currentBootTime - savedBootTime;
    return difference < tolerance;
}
}
