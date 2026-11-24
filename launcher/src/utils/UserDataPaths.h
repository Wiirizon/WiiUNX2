// WiiUNX2 GPL-3.0-or-later. SD locations for WiiUNX2-owned persistent data.
#pragma once
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace nx {
constexpr const char *kUserDataDirectory="fs:/vol/external01/wiiu/WiiUNX2";
constexpr const char *kWiiuDirectory="fs:/vol/external01/wiiu";

inline bool hasRegularDataFile(const std::string &path) {
    const char *suffixes[]={"",".bak",".tmp"};
    for(const char *suffix:suffixes) {
        struct stat status{};
        if(stat((path+suffix).c_str(),&status)==0&&S_ISREG(status.st_mode)) return true;
    }
    return false;
}

inline std::string userDataPath(bool directoryReady,const char *newLeaf,const char *legacyLeaf) {
    const std::string oldPath=std::string(kWiiuDirectory)+"/"+legacyLeaf;
    const std::string newPath=std::string(kUserDataDirectory)+"/"+newLeaf;
    if(!directoryReady) return oldPath;
    if(hasRegularDataFile(newPath)||!hasRegularDataFile(oldPath)) return newPath;
    return oldPath;
}

inline std::string recentHistoryLeaf(unsigned slot) {
    char leaf[32];
    std::snprintf(leaf,sizeof(leaf),"recent-%u.txt",slot);
    return leaf;
}

inline std::string legacyRecentHistoryLeaf(unsigned slot) {
    char leaf[48];
    std::snprintf(leaf,sizeof(leaf),"wiiunx2-recent-%u.txt",slot);
    return leaf;
}

inline std::string recentHistoryPath(bool directoryReady,unsigned slot) {
    return userDataPath(directoryReady,recentHistoryLeaf(slot).c_str(),legacyRecentHistoryLeaf(slot).c_str());
}
}

extern "C" bool WiiUNX2UserDataDirectoryReady();
