// WiiUNX2 GPL-3.0-or-later. Host tests for color flow along the fixed tile border.
#include "../src/gui/SelectionGradient.h"
#include <cassert>
#include <cmath>
#include <cstddef>

int main() {
    assert(nx::selectionGradientPhase(0)==0.0f);
    assert(nx::selectionGradientPhase(480)==0.0f);
    assert(nx::selectionGradientPhase(60)==1.0f);
    assert(nx::selectionGradientColor(-nx::selectionGradientPhase(60))[0]==
           nx::selectionGradientColor(7.0f)[0]);

    const auto first=nx::selectionGradientColor(0.0f);
    const auto wrap=nx::selectionGradientColor(8.0f);
    const auto moved=nx::selectionGradientColor(7.0f);
    for(std::size_t i=0;i<4;++i) {
        assert(std::isfinite(first[i]));
        assert(std::fabs(first[i]-wrap[i])<0.00001f);
        assert(std::fabs(moved[i]-nx::selectionGradientColor(-1.0f)[i])<0.00001f);
    }

    bool different=false;
    for(std::size_t i=0;i<3;++i) different=different||std::fabs(first[i]-nx::selectionGradientColor(2.0f)[i])>0.01f;
    assert(different);
    return 0;
}
