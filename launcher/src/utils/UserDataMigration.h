// WiiUNX2 GPL-3.0-or-later. Keep migration non-overwriting and failure-visible.
#pragma once
#include <string>

namespace nx {
constexpr const char *kUserDataMigrationMarker="layout-v1.complete";
constexpr const char *kUserDataMigrationMarkerContents="WiiUNX2 user data layout v1\n";
enum class UserDataMigrationResult {NotFound,Moved,Conflict,InvalidSource,InvalidDestination,Failed};

inline bool validUserDataMigrationMarker(const std::string &contents) {
    return contents==kUserDataMigrationMarkerContents;
}

inline bool isLegacyRecentHistoryLeaf(const std::string &leaf) {
    const std::string prefix="wiiunx2-recent-";
    if(leaf.compare(0,prefix.size(),prefix)!=0) return false;
    const size_t digitsEnd=leaf.find(".txt",prefix.size());
    if(digitsEnd==std::string::npos) return false;
    const size_t digits=digitsEnd-prefix.size();
    if(digits==0||digits>3||(digits>1&&leaf[prefix.size()]=='0')) return false;
    unsigned slot=0;
    for(size_t i=prefix.size();i<digitsEnd;++i) {
        if(leaf[i]<'0'||leaf[i]>'9') return false;
        slot=slot*10+unsigned(leaf[i]-'0');
    }
    if(slot>255) return false;
    const std::string suffix=leaf.substr(digitsEnd);
    return suffix==".txt"||suffix==".txt.bak"||suffix==".txt.tmp";
}

template<typename FileOps>
UserDataMigrationResult migrateLegacyUserFile(FileOps &files,const std::string &source,const std::string &destination) {
    if(!files.exists(source)) return UserDataMigrationResult::NotFound;
    if(!files.isRegular(source)) return UserDataMigrationResult::InvalidSource;
    if(files.exists(destination))
        return files.isRegular(destination)?UserDataMigrationResult::Conflict:UserDataMigrationResult::InvalidDestination;
    return files.renameNoReplace(source,destination)?UserDataMigrationResult::Moved:UserDataMigrationResult::Failed;
}
}
