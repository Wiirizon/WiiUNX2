// WiiUNX2 GPL-3.0-or-later. Safe resolution of the currently installed RPX path.
#pragma once
#include <cstddef>
#include <string>

namespace nx {
constexpr const char *kUpdateSdRoot="fs:/vol/external01/";
constexpr const char *kStandardAromaExecutable="wiiu/environments/aroma/men.rpx";
constexpr const char *kStyleMiiUThemePrefix="wiiu/themes/";
constexpr size_t kMaxUpdateTargetPathBytes=200;

enum class UpdateTargetKind {StandardAroma,StyleMiiUTheme};
struct UpdateTarget {
    std::string path;
    std::string displayPath;
    UpdateTargetKind kind=UpdateTargetKind::StandardAroma;
};

inline bool isSafeThemeExecutablePath(const std::string &path) {
    const std::string prefix=kStyleMiiUThemePrefix;
    const size_t filenameSize=sizeof("men.rpx")-1;
    const size_t lastSlash=path.find_last_of('/');
    if(path.size()<=prefix.size()||path.compare(0,prefix.size(),prefix)!=0||
       path.size()+std::string(kUpdateSdRoot).size()>kMaxUpdateTargetPathBytes||
       lastSlash<prefix.size()||path.size()-lastSlash-1!=filenameSize||
       path.compare(path.size()-filenameSize,filenameSize,"men.rpx")!=0) return false;
    size_t componentStart=0;
    for(size_t i=0;i<=path.size();++i) {
        if(i<path.size()&&path[i]!='/') {
            const unsigned char c=static_cast<unsigned char>(path[i]);
            if(c<0x20||c=='\\'||c==':') return false;
            continue;
        }
        const std::string component=path.substr(componentStart,i-componentStart);
        if(component.empty()||component=="."||component=="..") return false;
        componentStart=i+1;
    }
    return true;
}

inline bool resolveUpdateTarget(bool executablePathAvailable,const std::string &executablePath,
                                bool standardAromaFileExists,UpdateTarget &target) {
    if(executablePathAvailable) {
        if(executablePath==kStandardAromaExecutable&&standardAromaFileExists) {
            target={std::string(kUpdateSdRoot)+executablePath,"SD:/"+executablePath,UpdateTargetKind::StandardAroma};
            return true;
        }
        if(isSafeThemeExecutablePath(executablePath)) {
            target={std::string(kUpdateSdRoot)+executablePath,"SD:/"+executablePath,UpdateTargetKind::StyleMiiUTheme};
            return true;
        }
        return false;
    }
    if(standardAromaFileExists) {
        target={std::string(kUpdateSdRoot)+kStandardAromaExecutable,
                std::string("SD:/")+kStandardAromaExecutable,UpdateTargetKind::StandardAroma};
        return true;
    }
    return false;
}
}
