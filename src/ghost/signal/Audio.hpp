#pragma once

#include <algorithm>
#include <cmath>

namespace Ghost {
namespace Signal {
namespace Audio {

// Ghost's internal full-scale sample is ±1, mapped to ±5 V (the audio standard).
static constexpr float kVoltsPerSample = 5.f;
// Safety rail in sample units: clamp output to ±2 (= ±10 V, the Rack rail) so an
// accented hit can run hot but a runaway/NaN can never reach the DAC.
static constexpr float kSampleRail = 2.f;

/// Rack volts -> internal ±1 sample.
inline float fromRackVolts(float volts) {
    return volts / kVoltsPerSample;
}

/// Internal sample -> Rack volts, with a NaN/Inf guard and a ±10 V safety clamp
/// (every voice's audio output passes through here, satisfying the "clamp
/// before setVoltage" rule in one place).
inline float toRackVolts(float sample) {
    if (!std::isfinite(sample)) sample = 0.f;
    sample = std::fmax(-kSampleRail, std::fmin(kSampleRail, sample));
    return sample * kVoltsPerSample;
}

/// Equal-power dry/wet crossfade: dryGain^2 + wetGain^2 == 1 across mix 0..1,
/// so a blend keeps perceived loudness constant (no dip in the middle).
class ConstantPowerMix {
public:
    explicit ConstantPowerMix(float mix)
    : mix_(std::max(0.f, std::min(1.f, mix))) {
    }

    float dryGain() const {
        return std::cos(mix_ * kHalfPi);
    }

    float wetGain() const {
        return std::sin(mix_ * kHalfPi);
    }

private:
    static constexpr float kHalfPi = 1.57079632679f;
    float mix_;
};

} // namespace Audio
} // namespace Signal
} // namespace Ghost
