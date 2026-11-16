// WiiUNX2 GPL-3.0-or-later. Host tests for the selected-tile outline pulse curve.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "../src/gui/SelectionPulse.h"

int main() {
    assert(std::fabs(selectionPulseScale(0) - 1.0f) <= 1e-6f);

    float minValue = 2.0f;
    float maxValue = -1.0f;
    float previous = selectionPulseScale(0);
    for (uint32_t t = 0; t <= 8000; ++t) {
        const float value = selectionPulseScale(t);
        assert(std::isfinite(value));
        assert(value >= 0.80f && value <= 1.20f);
        assert(std::fabs(value - previous) < 0.001f);
        previous = value;
        if (value < minValue) minValue = value;
        if (value > maxValue) maxValue = value;
    }
    assert(maxValue > 1.19f);
    assert(minValue < 0.81f);

    for (uint64_t t = 0; t < 4294967295ull - kSelectionPulsePeriodMs; t += 9973u) {
        assert(std::fabs(selectionPulseScale(t) - selectionPulseScale(t + 2800u)) <= 1e-5f);
    }

    const float huge = selectionPulseScale(4294967295u);
    assert(std::isfinite(huge));
    assert(huge >= 0.80f && huge <= 1.20f);

    // The 32-bit millisecond wrap (2^32) is not a multiple of the period, so the
    // phase must come from the full-width value, not a truncated one.
    const uint64_t wrapMs = 4294967296ull;
    assert(std::fabs(selectionPulseScale(wrapMs) - selectionPulseScale(static_cast<uint32_t>(wrapMs % kSelectionPulsePeriodMs))) <= 1e-6f);
    for (uint64_t t = wrapMs - 2800u; t < wrapMs + 2800u; ++t) {
        assert(std::fabs(selectionPulseScale(t + 1u) - selectionPulseScale(t)) < 0.001f);
    }

    std::printf("selection-pulse: min=%f max=%f huge=%f\n", static_cast<double>(minValue), static_cast<double>(maxValue), static_cast<double>(huge));
    return 0;
}
