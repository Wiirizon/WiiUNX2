// WiiUNX2 GPL-3.0-or-later. Host tests for homebrew metadata cache freshness keys.
#include "../src/game/HomebrewCache.h"
#include <cassert>

int main() {
    nx::HomebrewFileStamp app{true,4096,12,100,101};
    nx::HomebrewFileStamp metadata{true,256,13,200,201};
    const std::string path="wiiu/apps/example/main.rpx";
    const std::string initial=nx::homebrewCacheKey(path,false,app,metadata);
    assert(!initial.empty());
    assert(initial==nx::homebrewCacheKey(path,false,app,metadata));

    auto changedApp=app;
    ++changedApp.modified;
    assert(initial!=nx::homebrewCacheKey(path,false,changedApp,metadata));
    auto changedMetadata=metadata;
    ++changedMetadata.size;
    assert(initial!=nx::homebrewCacheKey(path,false,app,changedMetadata));
    assert(nx::homebrewCacheKey(path,false,app,{ }).empty());

    const std::string bundle=nx::homebrewCacheKey("wiiu/apps/example.wuhb",true,app,{});
    assert(!bundle.empty());
    assert(bundle==nx::homebrewCacheKey("wiiu/apps/example.wuhb",true,app,metadata));
    assert(bundle!=nx::homebrewCacheKey("wiiu/apps/renamed.wuhb",true,app,{}));
    assert(nx::homebrewCacheKey(path,false,{},metadata).empty());
    assert(nx::homebrewCacheKey(std::string(250,'x'),true,app,{}).empty());
    return 0;
}
