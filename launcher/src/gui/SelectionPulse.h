#pragma once

#include <cmath>
#include <cstdint>

// Selected-tile outline brightness pulse. Pure math, no state, no I/O.
// Keep the outline gentle but visible: the multiplier ranges from 0.80 to 1.20.
constexpr uint32_t kSelectionPulsePeriodMs = 2800;
constexpr float kSelectionPulseAmplitude   = 0.20f;
constexpr double kSelectionPulsePi         = 3.14159265358979323846;

// Returns a multiplier in [0.80, 1.20]; 1.0 at timeMs == 0; periodic with
// kSelectionPulsePeriodMs. Takes the full-width ms value so the phase stays
// continuous past the 32-bit wrap (~49.7 days); the modulo runs in integer
// arithmetic so the float phase never drifts on long uptimes.
inline float selectionPulseScale(uint64_t timeMs) {
    const uint32_t phaseMs = static_cast<uint32_t>(timeMs % kSelectionPulsePeriodMs);
    const double phase     = 2.0 * kSelectionPulsePi * static_cast<double>(phaseMs) / static_cast<double>(kSelectionPulsePeriodMs);
    return 1.0f + kSelectionPulseAmplitude * static_cast<float>(std::sin(phase));
}
