#pragma once

#include <algorithm>
#include <cmath>

namespace Ghost {
namespace Signal {
namespace Audio {

inline float fromRackVolts(float volts) {
    return volts / 5.f;
}

inline float toRackVolts(float sample) {
    return sample * 5.f;
}

class ConstantPowerMix {
public:
    explicit ConstantPowerMix(float mix)
    : mix_(std::max(0.f, std::min(1.f, mix))) {
    }

    float dryGain() const {
        return std::cos(mix_ * 1.57079632679f);
    }

    float wetGain() const {
        return std::sin(mix_ * 1.57079632679f);
    }

private:
    float mix_;
};

} // namespace Audio
} // namespace Signal
} // namespace Ghost
