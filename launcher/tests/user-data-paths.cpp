// WiiUNX2 GPL-3.0-or-later. Host tests for data-path selection and migration.
#include "utils/UserDataMigration.h"
#include "utils/UserDataPaths.h"
#include <cassert>
#include <map>
#include <string>

namespace {
struct FakeFiles {
    std::map<std::string,bool> files;
    bool failRename=false;
    bool exists(const std::string &path) const {return files.find(path)!=files.end();}
    bool isRegular(const std::string &path) const {
        const auto found=files.find(path);
        return found!=files.end()&&found->second;
    }
    bool renameNoReplace(const std::string &source,const std::string &destination) {
        if(failRename||exists(destination)||!isRegular(source)) return false;
        files[destination]=files[source];
        files.erase(source);
        return true;
    }
};

void testPaths() {
    assert(nx::userDataPath(true,"debug.log","wiiunx2.log")=="fs:/vol/external01/wiiu/WiiUNX2/debug.log");
    assert(nx::userDataPath(false,"debug.log","wiiunx2.log")=="fs:/vol/external01/wiiu/wiiunx2.log");
    assert(nx::recentHistoryPath(true,3)=="fs:/vol/external01/wiiu/WiiUNX2/recent-3.txt");
    assert(nx::recentHistoryPath(false,255)=="fs:/vol/external01/wiiu/wiiunx2-recent-255.txt");
    assert(nx::legacyRecentHistoryLeaf(12)=="wiiunx2-recent-12.txt");
    assert(nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-0.txt"));
    assert(nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-255.txt.bak"));
    assert(nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-12.txt.tmp"));
    assert(!nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-256.txt"));
    assert(!nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-01.txt"));
    assert(!nx::isLegacyRecentHistoryLeaf("wiiunx2-recent-1.txt.exe"));
}

void testMigration() {
    FakeFiles files;
    files.files["legacy"]=true;
    assert(nx::migrateLegacyUserFile(files,"legacy","new")==nx::UserDataMigrationResult::Moved);
    assert(!files.exists("legacy")&&files.isRegular("new"));
    assert(nx::migrateLegacyUserFile(files,"missing","missing-new")==nx::UserDataMigrationResult::NotFound);

    files.files["legacy-conflict"]=true;files.files["destination"]=true;
    assert(nx::migrateLegacyUserFile(files,"legacy-conflict","destination")==nx::UserDataMigrationResult::Conflict);
    assert(files.isRegular("legacy-conflict")&&files.isRegular("destination"));

    files.files["directory"]=false;
    assert(nx::migrateLegacyUserFile(files,"directory","directory-new")==nx::UserDataMigrationResult::InvalidSource);
    assert(files.exists("directory")&&!files.exists("directory-new"));

    files.files["rename-failure"]=true;files.failRename=true;
    assert(nx::migrateLegacyUserFile(files,"rename-failure","rename-destination")==nx::UserDataMigrationResult::Failed);
    assert(files.isRegular("rename-failure")&&!files.exists("rename-destination"));
}
}

int main() {
    testPaths();
    testMigration();
    return 0;
}
