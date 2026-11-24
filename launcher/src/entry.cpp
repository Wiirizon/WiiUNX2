#include "Application.h"
#include "common/common.h"
#include "fs/FSUtils.h"
#include "utils/logger.h"
#include "utils/UserDataMigration.h"
#include "utils/UserDataPaths.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

static bool diagnosticsEnabled=false;
static bool userDataDirectoryReady=false;
static bool userDataMigrationComplete=false;
extern "C" bool WiiUNX2DiagnosticsEnabled() {return diagnosticsEnabled;}
extern "C" bool WiiUNX2UserDataDirectoryReady() {return userDataDirectoryReady;}
extern "C" bool WiiUNX2UserDataMigrationComplete() {return userDataMigrationComplete;}

namespace {
struct SdMigrationFiles {
    bool exists(const std::string &path) const {
        struct stat status{};
        if(lstat(path.c_str(),&status)==0) return true;
        return errno!=ENOENT;
    }
    bool isRegular(const std::string &path) const {
        struct stat status{};
        return lstat(path.c_str(),&status)==0&&S_ISREG(status.st_mode);
    }
    bool renameNoReplace(const std::string &source,const std::string &destination) const {
        return !exists(destination)&&rename(source.c_str(),destination.c_str())==0;
    }
};

static bool migrateFile(SdMigrationFiles &files,const std::string &oldLeaf,const std::string &newLeaf) {
    const std::string source=std::string(nx::kWiiuDirectory)+"/"+oldLeaf;
    const std::string destination=std::string(nx::kUserDataDirectory)+"/"+newLeaf;
    const auto result=nx::migrateLegacyUserFile(files,source,destination);
    if(result==nx::UserDataMigrationResult::NotFound||result==nx::UserDataMigrationResult::Moved)
        return true;
    if(result==nx::UserDataMigrationResult::Conflict) {
        DEBUG_FUNCTION_LINE("Both legacy and organized WiiUNX2 data exist for %s; keeping both",newLeaf.c_str());
        return true;
    }
    if(result==nx::UserDataMigrationResult::InvalidSource)
        DEBUG_FUNCTION_LINE("Legacy WiiUNX2 data is not a regular file: %s",oldLeaf.c_str());
    else if(result==nx::UserDataMigrationResult::InvalidDestination)
        DEBUG_FUNCTION_LINE("Organized WiiUNX2 destination is not a regular file: %s",newLeaf.c_str());
    else if(result==nx::UserDataMigrationResult::Failed)
        DEBUG_FUNCTION_LINE("Could not move legacy WiiUNX2 data: %s",oldLeaf.c_str());
    return false;
}

static bool migrateFileWithRecoveryFiles(SdMigrationFiles &files,const std::string &oldLeaf,const std::string &newLeaf) {
    bool complete=migrateFile(files,oldLeaf,newLeaf);
    const char *suffixes[]={".bak",".tmp"};
    for(const char *suffix:suffixes)
        if(!migrateFile(files,oldLeaf+suffix,newLeaf+suffix)) complete=false;
    return complete;
}

static bool migrateRecentHistory(SdMigrationFiles &files) {
    DIR *directory=opendir(nx::kWiiuDirectory);
    if(!directory) {
        DEBUG_FUNCTION_LINE("Could not scan SD:/wiiu for legacy recent-history files");
        return false;
    }
    std::vector<std::string> legacyFiles;
    bool overflow=false;
    errno=0;
    while(auto *entry=readdir(directory)) {
        if(!nx::isLegacyRecentHistoryLeaf(entry->d_name)) continue;
        if(legacyFiles.size()>=4096) {overflow=true;break;}
        legacyFiles.emplace_back(entry->d_name);
    }
    const bool readComplete=errno==0;
    const bool closed=closedir(directory)==0;
    if(overflow||!readComplete||!closed) {
        DEBUG_FUNCTION_LINE("Legacy recent-history scan was incomplete; remaining files will stay in SD:/wiiu");
    }
    bool complete=!overflow&&readComplete&&closed;
    for(const auto &oldLeaf:legacyFiles) {
        const std::string newLeaf=oldLeaf.substr(std::string("wiiunx2-").size());
        if(!migrateFile(files,oldLeaf,newLeaf)) complete=false;
    }
    return complete;
}

static bool migrationMarkerValid(SdMigrationFiles &files) {
    const std::string path=std::string(nx::kUserDataDirectory)+"/"+nx::kUserDataMigrationMarker;
    if(!files.isRegular(path)) return false;
    FILE *file=std::fopen(path.c_str(),"rb");
    if(!file) return false;
    char contents[64]{};
    const size_t size=std::fread(contents,1,sizeof(contents),file);
    const bool eof=std::fgetc(file)==EOF&&!std::ferror(file);
    const bool closed=std::fclose(file)==0;
    return eof&&closed&&nx::validUserDataMigrationMarker(std::string(contents,size));
}

static bool writeMigrationMarker() {
    const std::string path=std::string(nx::kUserDataDirectory)+"/"+nx::kUserDataMigrationMarker;
    FILE *file=std::fopen(path.c_str(),"wb");
    if(!file) return false;
    const size_t length=std::strlen(nx::kUserDataMigrationMarkerContents);
    bool ok=std::fwrite(nx::kUserDataMigrationMarkerContents,1,length,file)==length;
    if(std::fflush(file)!=0||fsync(fileno(file))!=0) ok=false;
    if(std::fclose(file)!=0) ok=false;
    return ok;
}

static void initializeUserDataDirectory() {
    userDataDirectoryReady=FSUtils::CreateSubfolder(nx::kUserDataDirectory);
    if(!userDataDirectoryReady) {
        DEBUG_FUNCTION_LINE("Could not create SD:/wiiu/WiiUNX2; keeping existing data paths");
        return;
    }

    SdMigrationFiles files;
    if(migrationMarkerValid(files)) {
        userDataMigrationComplete=true;
        return;
    }

    bool complete=true;
    const char *legacyNames[]={"wiiunx2-debug.enabled","wiiunx2.log",
        "wiiunx2-launch-log.enable","wiiunx2-launch.log","wiiunx2-account-chosen.txt"};
    const char *newNames[]={"debug.enabled","debug.log","launch-log.enable","launch.log","account-chosen.txt"};
    for(size_t i=0;i<sizeof(legacyNames)/sizeof(legacyNames[0]);++i)
        if(!migrateFile(files,legacyNames[i],newNames[i])) complete=false;

    if(!migrateFileWithRecoveryFiles(files,"wiiunx2-names.cache","names.cache")) complete=false;
    if(!migrateRecentHistory(files)) complete=false;
    if(!complete) {
        DEBUG_FUNCTION_LINE("WiiUNX2 data migration was incomplete; it will retry on next launch");
        return;
    }
    if(!writeMigrationMarker()) {
        DEBUG_FUNCTION_LINE("Could not save WiiUNX2 data migration marker; it will retry on next launch");
        return;
    }
    userDataMigrationComplete=true;
}
}

// Opt-in local usage diagnostics. Title IDs and profile slots can be private.
static void sdLog(const char *message) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> guard(mutex);
    const std::string path=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),
        WiiUNX2UserDataMigrationComplete(),"debug.log","wiiunx2.log");
    FILE *file=std::fopen(path.c_str(),"a");
    if(!file) return;
    auto position=std::fseek(file,0,SEEK_END)==0 ? std::ftell(file) : -1;
    if(position>=0 && position<1024*1024) std::fprintf(file,"%s\n",message);
    std::fclose(file);
}

int32_t main(int32_t argc, char **argv) {
    initializeUserDataDirectory();
    const std::string debugFlagPath=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),
        WiiUNX2UserDataMigrationComplete(),"debug.enabled","wiiunx2-debug.enabled");
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
