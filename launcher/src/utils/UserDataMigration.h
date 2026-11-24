// WiiUNX2 GPL-3.0-or-later. Keep migration non-overwriting and failure-visible.
#pragma once
#include <string>

namespace nx {
enum class UserDataMigrationResult {NotFound,Moved,Conflict,InvalidSource,Failed};

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
    if(files.exists(destination)) return UserDataMigrationResult::Conflict;
    return files.renameNoReplace(source,destination)?UserDataMigrationResult::Moved:UserDataMigrationResult::Failed;
}
}
