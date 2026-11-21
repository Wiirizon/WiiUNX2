// WiiUNX2 GPL-3.0-or-later. Host tests for failure-safe updater replacement.
#include "utils/UpdateInstall.h"
#include <cstdio>
#include <map>
#include <string>

namespace {
int failures=0;
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#value); ++failures; } } while(0)

struct FakeFiles {
    std::map<std::string,std::string> files{{"target","old"},{"staged","new"}};
    bool failBackupCopy=false,failBackupPublish=false,failReplace=false,failTargetMove=false;
    bool failStagedMove=false,failDisplacedRestore=false,rejectOverwrite=false;
    bool corruptReplacement=false,failRestore=false;
    bool isRegular(const std::string &path) const {return files.find(path)!=files.end();}
    bool exists(const std::string &path) const {return isRegular(path);}
    bool copyVerified(const std::string &source,const std::string &destination) {
        if(destination=="backup.part"&&failBackupCopy) {files[destination]="partial";files.erase(destination);return false;}
        if(destination=="restore.part"&&failRestore) return false;
        const auto found=files.find(source);
        if(found==files.end()||exists(destination)) return false;
        files[destination]=found->second;
        return files[destination]==found->second;
    }
    bool renameNoReplace(const std::string &source,const std::string &destination) {
        if((source=="target"&&destination=="displaced.part"&&failTargetMove)||
           (source=="staged"&&destination=="target"&&failStagedMove)||
           (source=="displaced.part"&&destination=="target"&&failDisplacedRestore)) return false;
        if(failBackupPublish||!isRegular(source)||exists(destination)) return false;
        files[destination]=corruptReplacement&&source=="staged"?"corrupt":files[source];
        files.erase(source);return true;
    }
    bool atomicReplace(const std::string &source,const std::string &destination) {
        if((source=="staged"&&failReplace)||!isRegular(source)||(rejectOverwrite&&exists(destination))) return false;
        files[destination]=corruptReplacement&&source=="staged"?"corrupt":files[source];
        files.erase(source);return true;
    }
    void removeFile(const std::string &path) {files.erase(path);}
    bool hashMatches(const std::string &path,uint64_t size,const std::string &hash) const {
        const auto found=files.find(path);
        return found!=files.end()&&found->second.size()==size&&found->second==hash;
    }
};

nx::UpdateReplaceResult replace(FakeFiles &files) {
    return nx::replaceWithVerifiedBackup(files,"target","staged","backup","backup.part","restore.part",
                                         "displaced.part",3,"new",3,"old");
}
void testSuccessfulInstallRetainsOriginal() {
    FakeFiles files;
    CHECK(replace(files)==nx::UpdateReplaceResult::Installed);
    CHECK(files.files["target"]=="new");
    CHECK(files.files["backup"]=="old");
    CHECK(!files.exists("staged"));
}
void testBackupWriteAndPublishFailuresLeaveTargetUntouched() {
    FakeFiles writeFailure;writeFailure.failBackupCopy=true;
    CHECK(replace(writeFailure)==nx::UpdateReplaceResult::BackupFailed);
    CHECK(writeFailure.files["target"]=="old"&&!writeFailure.exists("backup.part"));

    FakeFiles publishFailure;publishFailure.failBackupPublish=true;
    CHECK(replace(publishFailure)==nx::UpdateReplaceResult::BackupPublishFailed);
    CHECK(publishFailure.files["target"]=="old"&&!publishFailure.exists("backup.part"));
}
void testReplacementFailureRetainsOriginalAndVerifiedBackup() {
    FakeFiles files;files.failReplace=true;files.failTargetMove=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::ReplaceFailed);
    CHECK(files.files["target"]=="old");
    CHECK(files.files["backup"]=="old");
}
void testNonOverwritingFilesystemUsesRollbackProtectedFallback() {
    FakeFiles files;files.rejectOverwrite=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::Installed);
    CHECK(files.files["target"]=="new");
    CHECK(files.files["backup"]=="old");
    CHECK(!files.exists("displaced.part"));
}
void testFallbackInstallFailureRestoresOriginal() {
    FakeFiles files;files.rejectOverwrite=true;files.failStagedMove=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::ReplaceFailed);
    CHECK(files.files["target"]=="old");
    CHECK(files.files["backup"]=="old");
    CHECK(!files.exists("displaced.part"));
}
void testFallbackRecoveryFailureKeepsVerifiedOriginal() {
    FakeFiles files;files.rejectOverwrite=true;files.failStagedMove=true;files.failDisplacedRestore=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::RecoveryRequired);
    CHECK(!files.exists("target"));
    CHECK(files.files["displaced.part"]=="old");
    CHECK(files.files["backup"]=="old");
}
void testPostReplaceVerificationRestoresOriginal() {
    FakeFiles files;files.corruptReplacement=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::Restored);
    CHECK(files.files["target"]=="old");
    CHECK(files.files["backup"]=="old");
}
void testFallbackVerificationRestoresOriginal() {
    FakeFiles files;files.rejectOverwrite=true;files.corruptReplacement=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::Restored);
    CHECK(files.files["target"]=="old");
    CHECK(files.files["backup"]=="old");
    CHECK(!files.exists("displaced.part"));
    CHECK(!files.exists("staged"));
}
void testFailedRestoreRequiresManualRecovery() {
    FakeFiles files;files.corruptReplacement=true;files.failRestore=true;
    CHECK(replace(files)==nx::UpdateReplaceResult::RecoveryRequired);
    CHECK(files.files["target"]=="corrupt");
    CHECK(files.files["backup"]=="old");
}
void testUnexpectedExistingBackupFailsClosed() {
    FakeFiles files;files.files["backup"]="older";
    CHECK(replace(files)==nx::UpdateReplaceResult::InvalidState);
    CHECK(files.files["target"]=="old");
    CHECK(files.files["backup"]=="older");
}
}

int main() {
    testSuccessfulInstallRetainsOriginal();testBackupWriteAndPublishFailuresLeaveTargetUntouched();
    testReplacementFailureRetainsOriginalAndVerifiedBackup();testPostReplaceVerificationRestoresOriginal();
    testNonOverwritingFilesystemUsesRollbackProtectedFallback();testFallbackInstallFailureRestoresOriginal();
    testFallbackRecoveryFailureKeepsVerifiedOriginal();
    testFallbackVerificationRestoresOriginal();testFailedRestoreRequiresManualRecovery();
    testUnexpectedExistingBackupFailsClosed();
    return failures?1:0;
}
