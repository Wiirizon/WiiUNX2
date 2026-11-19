// WiiUNX2 GPL-3.0-or-later. Smooth color progression for the fixed tile outline.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace nx {
inline float selectionGradientPhase(unsigned frame) {
    return float(frame%480u)*(8.0f/480.0f);
}

inline float positiveFraction(float value) {
    return value-std::floor(value);
}

inline std::array<float,4> selectionGradientColor(float segment) {
    constexpr float stops[4][3]={{0.35f,0.83f,0.91f},{0.60f,0.50f,0.94f},
                                 {0.96f,0.60f,0.76f},{0.35f,0.83f,0.91f}};
    const float position=positiveFraction(segment/8.0f)*3.0f;
    const int index=std::min(2,int(position));
    const float blend=position-index;
    return {{stops[index][0]+(stops[index+1][0]-stops[index][0])*blend,
             stops[index][1]+(stops[index+1][1]-stops[index][1])*blend,
             stops[index][2]+(stops[index+1][2]-stops[index][2])*blend,1.0f}};
}
}
