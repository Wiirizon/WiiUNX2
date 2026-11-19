// WiiUNX2 GPL-3.0-or-later. Stable cache keys for homebrew metadata freshness.
#pragma once
#include <cstdint>
#include <string>

namespace nx {
struct HomebrewFileStamp {
    bool valid=false;
    uint64_t size=0;
    uint64_t inode=0;
    int64_t modified=0;
    int64_t changed=0;
};

inline std::string homebrewCacheKey(const std::string &path,bool bundle,
                                   const HomebrewFileStamp &app,
                                   const HomebrewFileStamp &metadata) {
    if(path.empty() || !app.valid || (!bundle && !metadata.valid)) return {};
    std::string key="homebrew|"+path+"|"+std::to_string(app.size)+":"+
                    std::to_string(app.inode)+":"+std::to_string(app.modified)+":"+
                    std::to_string(app.changed);
    if(!bundle) {
        key+="|"+std::to_string(metadata.size)+":"+
             std::to_string(metadata.inode)+":"+std::to_string(metadata.modified)+":"+
             std::to_string(metadata.changed);
    }
    return key.size()<=256 ? key:std::string();
}
}
