// WiiUNX2 GPL-3.0-or-later. Testable backup-and-replace transaction.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "UpdateCore.h"

namespace nx {
enum class UpdateReplaceResult {
    InvalidState,BackupFailed,BackupPublishFailed,ReplaceFailed,Installed,Restored,RecoveryRequired
};

inline bool isUpdaterBackupLeaf(const std::string &name) {
    static const std::string prefix="men.rpx.backup-";
    if(name.size()<=prefix.size()||name.size()>prefix.size()+16||
       name.compare(0,prefix.size(),prefix)!=0) return false;
    for(size_t i=prefix.size();i<name.size();++i) {
        const char c=name[i];
        if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false;
    }
    return true;
}

template<typename FileOps>
bool retainLatestUpdaterBackup(FileOps &files,const std::string &directory,const std::string &keepPath) {
    std::vector<std::string> entries;
    if(!files.listDirectory(directory,entries)) return false;
    bool ok=true;
    for(const auto &leaf:entries) {
        if(!isUpdaterBackupLeaf(leaf)) continue;
        const std::string path=directory+"/"+leaf;
        if(path==keepPath||!files.isRegular(path)) continue;
        if(!files.removeFile(path)) ok=false;
    }
    return ok;
}

// FileOps copies must be complete, and atomicReplace must leave the target intact on failure.
// A verified backup is published before the non-overwriting rename fallback is attempted.
template<typename FileOps>
UpdateReplaceResult replaceWithVerifiedBackup(FileOps &files,const std::string &target,const std::string &staged,
                                               const std::string &backup,const std::string &backupPart,
                                               const std::string &restorePart,const std::string &displacedPart,
                                               uint64_t newSize,const std::string &newHash,uint64_t oldSize,
                                               const std::string &oldHash) {
    if(!files.isRegular(target)||!files.isRegular(staged)||files.exists(backup)||
       files.exists(backupPart)||files.exists(restorePart)||files.exists(displacedPart))
        return UpdateReplaceResult::InvalidState;
    if(!files.copyVerified(target,backupPart)||!files.hashMatches(backupPart,oldSize,oldHash)) {
        files.removeFile(backupPart);
        return UpdateReplaceResult::BackupFailed;
    }
    if(!files.renameNoReplace(backupPart,backup)) {
        files.removeFile(backupPart);
        return UpdateReplaceResult::BackupPublishFailed;
    }

    bool displaced=false;
    if(!files.atomicReplace(staged,target)) {
        if(!files.renameNoReplace(target,displacedPart)) return UpdateReplaceResult::ReplaceFailed;
        displaced=true;
        if(!files.renameNoReplace(staged,target)) {
            if(!files.renameNoReplace(displacedPart,target)) return UpdateReplaceResult::RecoveryRequired;
            return UpdateReplaceResult::ReplaceFailed;
        }
    }
    if(files.hashMatches(target,newSize,newHash)) {
        if(displaced) files.removeFile(displacedPart);
        return UpdateReplaceResult::Installed;
    }

    if(displaced) {
        if(!files.renameNoReplace(target,staged)) return UpdateReplaceResult::RecoveryRequired;
        if(!files.renameNoReplace(displacedPart,target)) {
            files.renameNoReplace(staged,target);
            return UpdateReplaceResult::RecoveryRequired;
        }
        files.removeFile(staged);
        return files.hashMatches(target,oldSize,oldHash)?UpdateReplaceResult::Restored:UpdateReplaceResult::RecoveryRequired;
    }

    if(!files.copyVerified(backup,restorePart)) return UpdateReplaceResult::RecoveryRequired;
    if(files.atomicReplace(restorePart,target))
        return files.hashMatches(target,oldSize,oldHash)?UpdateReplaceResult::Restored:UpdateReplaceResult::RecoveryRequired;
    if(!files.renameNoReplace(target,staged)) return UpdateReplaceResult::RecoveryRequired;
    if(!files.renameNoReplace(restorePart,target)) {
        files.renameNoReplace(staged,target);
        return UpdateReplaceResult::RecoveryRequired;
    }
    files.removeFile(staged);
    return files.hashMatches(target,oldSize,oldHash)?UpdateReplaceResult::Restored:UpdateReplaceResult::RecoveryRequired;
}
}
