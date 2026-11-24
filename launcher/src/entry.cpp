#include "Application.h"
#include "common/common.h"
#include "fs/FSUtils.h"
#include "utils/logger.h"
#include "utils/UserDataMigration.h"
#include "utils/UserDataPaths.h"
#include <cstdio>
#include <dirent.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static bool diagnosticsEnabled=false;
static bool userDataDirectoryReady=false;
extern "C" bool WiiUNX2DiagnosticsEnabled() {return diagnosticsEnabled;}
extern "C" bool WiiUNX2UserDataDirectoryReady() {return userDataDirectoryReady;}

namespace {
struct SdMigrationFiles {
    bool exists(const std::string &path) const {
        struct stat status{};
        return lstat(path.c_str(),&status)==0;
    }
    bool isRegular(const std::string &path) const {
        struct stat status{};
        return lstat(path.c_str(),&status)==0&&S_ISREG(status.st_mode);
    }
    bool renameNoReplace(const std::string &source,const std::string &destination) const {
        return !exists(destination)&&rename(source.c_str(),destination.c_str())==0;
    }
};

static void migrateFile(SdMigrationFiles &files,const std::string &oldLeaf,const std::string &newLeaf) {
    const std::string source=std::string(nx::kWiiuDirectory)+"/"+oldLeaf;
    const std::string destination=std::string(nx::kUserDataDirectory)+"/"+newLeaf;
    const auto result=nx::migrateLegacyUserFile(files,source,destination);
    if(result==nx::UserDataMigrationResult::Conflict)
        DEBUG_FUNCTION_LINE("Both legacy and organized WiiUNX2 data exist for %s; keeping both",newLeaf.c_str());
    else if(result==nx::UserDataMigrationResult::InvalidSource)
        DEBUG_FUNCTION_LINE("Legacy WiiUNX2 data is not a regular file: %s",oldLeaf.c_str());
    else if(result==nx::UserDataMigrationResult::Failed)
        DEBUG_FUNCTION_LINE("Could not move legacy WiiUNX2 data: %s",oldLeaf.c_str());
}

static void migrateFileWithRecoveryFiles(SdMigrationFiles &files,const std::string &oldLeaf,const std::string &newLeaf) {
    migrateFile(files,oldLeaf,newLeaf);
    const char *suffixes[]={".bak",".tmp"};
    for(const char *suffix:suffixes)
        migrateFile(files,oldLeaf+suffix,newLeaf+suffix);
}

static void migrateRecentHistory(SdMigrationFiles &files) {
    DIR *directory=opendir(nx::kWiiuDirectory);
    if(!directory) {
        DEBUG_FUNCTION_LINE("Could not scan SD:/wiiu for legacy recent-history files");
        return;
    }
    std::vector<std::string> legacyFiles;
    bool overflow=false;
    while(auto *entry=readdir(directory)) {
        if(!nx::isLegacyRecentHistoryLeaf(entry->d_name)) continue;
        if(legacyFiles.size()>=4096) {overflow=true;break;}
        legacyFiles.emplace_back(entry->d_name);
    }
    const bool closed=closedir(directory)==0;
    if(overflow||!closed) {
        DEBUG_FUNCTION_LINE("Legacy recent-history scan was incomplete; remaining files will stay in SD:/wiiu");
    }
    for(const auto &oldLeaf:legacyFiles) {
        const std::string newLeaf=oldLeaf.substr(std::string("wiiunx2-").size());
        migrateFile(files,oldLeaf,newLeaf);
    }
}

static void initializeUserDataDirectory() {
    userDataDirectoryReady=FSUtils::CreateSubfolder(nx::kUserDataDirectory);
    if(!userDataDirectoryReady) {
        DEBUG_FUNCTION_LINE("Could not create SD:/wiiu/WiiUNX2; keeping existing data paths");
        return;
    }

    SdMigrationFiles files;
    const char *legacyNames[]={"wiiunx2-debug.enabled","wiiunx2.log",
        "wiiunx2-launch-log.enable","wiiunx2-launch.log","wiiunx2-account-chosen.txt"};
    const char *newNames[]={"debug.enabled","debug.log","launch-log.enable","launch.log","account-chosen.txt"};
    for(size_t i=0;i<sizeof(legacyNames)/sizeof(legacyNames[0]);++i)
        migrateFile(files,legacyNames[i],newNames[i]);

    migrateFileWithRecoveryFiles(files,"wiiunx2-names.cache","names.cache");
    migrateRecentHistory(files);
}
}

// Opt-in local usage diagnostics. Title IDs and profile slots can be private.
static void sdLog(const char *message) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> guard(mutex);
    const std::string path=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),"debug.log","wiiunx2.log");
    FILE *file=std::fopen(path.c_str(),"a");
    if(!file) return;
    auto position=std::fseek(file,0,SEEK_END)==0 ? std::ftell(file) : -1;
    if(position>=0 && position<1024*1024) std::fprintf(file,"%s\n",message);
    std::fclose(file);
}

int32_t main(int32_t argc, char **argv) {
    initializeUserDataDirectory();
    const std::string debugFlagPath=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),"debug.enabled","wiiunx2-debug.enabled");
    FILE *flag=std::fopen(debugFlagPath.c_str(),"r");
    diagnosticsEnabled=flag!=nullptr;
    if(flag) std::fclose(flag);
    if(diagnosticsEnabled) WHBAddLogHandler(sdLog);
    DEBUG_FUNCTION_LINE("Starting WiiUNX2 experimental (local diagnostics enabled)");

    DEBUG_FUNCTION_LINE("Start main application");
    Application::instance()->exec();

    DEBUG_FUNCTION_LINE("Main application stopped");
    Application::destroyInstance();

    DEBUG_FUNCTION_LINE("Peace out...");

    if(diagnosticsEnabled) WHBRemoveLogHandler(sdLog);
    return 0;
}
