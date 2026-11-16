// WiiUNX2 GPL-3.0-or-later. Testable backup-and-replace transaction.
#pragma once
#include <cstdint>
#include <string>

namespace nx {
enum class UpdateReplaceResult {
    InvalidState,BackupFailed,BackupPublishFailed,ReplaceFailed,Installed,Restored,RecoveryRequired
};

// FileOps must not leave partial copies and atomicReplace must leave target intact on failure.
template<typename FileOps>
UpdateReplaceResult replaceWithVerifiedBackup(FileOps &files,const std::string &target,const std::string &staged,
                                               const std::string &backup,const std::string &backupPart,
                                               const std::string &restorePart,uint64_t newSize,const std::string &newHash,
                                               uint64_t oldSize,const std::string &oldHash) {
    if(!files.isRegular(target)||!files.isRegular(staged)||files.exists(backup)||
       files.exists(backupPart)||files.exists(restorePart)) return UpdateReplaceResult::InvalidState;
    if(!files.copyVerified(target,backupPart)||!files.hashMatches(backupPart,oldSize,oldHash)) {
        files.removeFile(backupPart);
        return UpdateReplaceResult::BackupFailed;
    }
    if(!files.renameNoReplace(backupPart,backup)) {
        files.removeFile(backupPart);
        return UpdateReplaceResult::BackupPublishFailed;
    }
    if(!files.atomicReplace(staged,target)) return UpdateReplaceResult::ReplaceFailed;
    if(files.hashMatches(target,newSize,newHash)) return UpdateReplaceResult::Installed;
    if(!files.copyVerified(backup,restorePart)) return UpdateReplaceResult::RecoveryRequired;
    if(!files.atomicReplace(restorePart,target)) {
        files.removeFile(restorePart);
        return UpdateReplaceResult::RecoveryRequired;
    }
    return files.hashMatches(target,oldSize,oldHash)?UpdateReplaceResult::Restored:UpdateReplaceResult::RecoveryRequired;
}
}
