// WiiUNX2 GPL-3.0-or-later. Host tests for resolving the updater's destination.
#include "utils/UpdateTarget.h"
#include <cstdio>

namespace {
int failures=0;
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#value); ++failures; } } while(0)

void testStyleMiiUPathIsUsedEvenWhenStandardInstallExists() {
    nx::UpdateTarget target;
    CHECK(nx::resolveUpdateTarget(true,"wiiu/themes/WiiUNX2/men.rpx",true,target));
    CHECK(target.kind==nx::UpdateTargetKind::StyleMiiUTheme);
    CHECK(target.path=="fs:/vol/external01/wiiu/themes/WiiUNX2/men.rpx");
    CHECK(target.displayPath=="SD:/wiiu/themes/WiiUNX2/men.rpx");
}
void testStyleMiiUPathDoesNotRequireStandardInstall() {
    nx::UpdateTarget target;
    CHECK(nx::resolveUpdateTarget(true,"wiiu/themes/WiiUNX2/men.rpx",false,target));
    CHECK(target.kind==nx::UpdateTargetKind::StyleMiiUTheme);
}
void testStandardPathFallbackRequiresExistingFile() {
    nx::UpdateTarget target;
    CHECK(nx::resolveUpdateTarget(false,"",true,target));
    CHECK(target.kind==nx::UpdateTargetKind::StandardAroma);
    CHECK(nx::resolveUpdateTarget(true,"wiiu/environments/aroma/men.rpx",true,target));
    CHECK(target.kind==nx::UpdateTargetKind::StandardAroma);
    CHECK(!nx::resolveUpdateTarget(false,"",false,target));
    CHECK(!nx::resolveUpdateTarget(true,"wiiu/environments/aroma/men.rpx",false,target));
}
void testKnownButUnsupportedPathFailsClosed() {
    nx::UpdateTarget target;
    CHECK(nx::resolveUpdateTarget(true,"wiiu/themes/Other/men.rpx",true,target));
    CHECK(target.path=="fs:/vol/external01/wiiu/themes/Other/men.rpx");
    CHECK(!nx::resolveUpdateTarget(true,"wiiu/themes/WiiUNX2/../Other/men.rpx",true,target));
    CHECK(nx::resolveUpdateTarget(true,"wiiu/themes/WiiUNX2/code/men.rpx",true,target));
    CHECK(target.kind==nx::UpdateTargetKind::StyleMiiUTheme);
    CHECK(!nx::resolveUpdateTarget(true,"wiiu/apps/WiiUNX2/men.rpx",true,target));
}
void testThemePathMustBeUnderThemesAndHaveExactFilename() {
    CHECK(nx::isSafeThemeExecutablePath("wiiu/themes/WiiUNX2/men.rpx"));
    CHECK(nx::isSafeThemeExecutablePath("wiiu/themes/WiiUNX2/code/men.rpx"));
    CHECK(!nx::isSafeThemeExecutablePath("wiiu/environments/aroma/men.rpx"));
    CHECK(!nx::isSafeThemeExecutablePath("wiiu/themes/men.rpx"));
    CHECK(!nx::isSafeThemeExecutablePath("wiiu/themes/WiiUNX2/not-men.rpx"));
    CHECK(!nx::isSafeThemeExecutablePath("wiiu/themes/WiiUNX2//men.rpx"));
    CHECK(!nx::isSafeThemeExecutablePath("wiiu/themes/WiiUNX2\\men.rpx"));
}
}

int main() {
    testStyleMiiUPathIsUsedEvenWhenStandardInstallExists();
    testStyleMiiUPathDoesNotRequireStandardInstall();
    testStandardPathFallbackRequiresExistingFile();
    testKnownButUnsupportedPathFailsClosed();
    testThemePathMustBeUnderThemesAndHaveExactFilename();
    return failures?1:0;
}
