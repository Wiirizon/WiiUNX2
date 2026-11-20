// WiiUNX2 GPL-3.0-or-later. HTTPS, verification, staging, and rollback for issue #7.
#include "UpdateService.h"
#include "UpdateInstall.h"
#include "UpdateTarget.h"
#include "UpdateCaBundle.h"
#include "common/common.h"
#include <curl/curl.h>
#include <coreinit/ios.h>
#include <coreinit/time.h>
#include <rpxloader/rpxloader.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <fcntl.h>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace nx {
namespace {
constexpr const char *kLatestReleaseUrl="https://api.github.com/repos/Wiirizon/WiiUNX2/releases/latest";
constexpr const char *kStandardEnvironment="fs:/vol/external01/wiiu/environments/aroma";
constexpr long kConnectTimeoutSeconds=20;
constexpr long kTransferTimeoutSeconds=180;
constexpr unsigned kMaxRedirects=5;

struct ScopedFile {
    FILE *value=nullptr;
    ~ScopedFile() {if(value) std::fclose(value);}
};

struct HttpResponse {
    long status=0;
    std::string body;
    std::map<std::string,std::string> headers;
    std::string error;
    bool overflow=false,cancelled=false;
};
struct TransferContext {
    HttpResponse *response=nullptr;
    FILE *file=nullptr;
    uint64_t maxBytes=0,bytes=0;
    const std::atomic<bool> *cancel=nullptr;
};

bool cancelled(const std::atomic<bool> *flag) {return flag&&flag->load(std::memory_order_relaxed);}
std::string trim(const std::string &value) {
    const size_t first=value.find_first_not_of(" \t\r\n");
    if(first==std::string::npos) return {};
    const size_t last=value.find_last_not_of(" \t\r\n");
    return value.substr(first,last-first+1);
}
std::string lowerAscii(std::string value) {
    for(char &c:value) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return value;
}

size_t receiveBody(char *data,size_t size,size_t count,void *opaque) {
    auto *ctx=static_cast<TransferContext *>(opaque);
    if(!ctx||!ctx->response||(size!=0&&count>std::numeric_limits<size_t>::max()/size)) return 0;
    const size_t bytes=size*count;
    if(cancelled(ctx->cancel)) {ctx->response->cancelled=true;return 0;}
    if(bytes>ctx->maxBytes-std::min<uint64_t>(ctx->bytes,ctx->maxBytes)) {
        ctx->response->overflow=true;return 0;
    }
    if(ctx->file) {
        if(bytes&&std::fwrite(data,1,bytes,ctx->file)!=bytes) {ctx->response->error="Could not write the update download to SD.";return 0;}
    } else {
        try {ctx->response->body.append(data,bytes);}
        catch(const std::exception &) {ctx->response->error="Not enough memory to receive the update response.";return 0;}
    }
    ctx->bytes+=bytes;
    return bytes;
}
size_t receiveHeader(char *data,size_t size,size_t count,void *opaque) {
    auto *response=static_cast<HttpResponse *>(opaque);
    if(!response||(size!=0&&count>std::numeric_limits<size_t>::max()/size)) return 0;
    const size_t bytes=size*count;
    try {
        const std::string line(data,bytes);
        if(line.size()>=5&&line.compare(0,5,"HTTP/")==0) response->headers.clear();
        const size_t colon=line.find(':');
        if(colon!=std::string::npos&&colon>0) {
            const std::string key=lowerAscii(trim(line.substr(0,colon)));
            const std::string value=trim(line.substr(colon+1));
            if(key=="location"||key=="retry-after"||key=="x-ratelimit-remaining"||key=="x-ratelimit-reset"||key=="content-length") {
                const auto prior=response->headers.find(key);
                if(prior!=response->headers.end()&&prior->second!=value) response->error="The server returned conflicting HTTP headers.";
                else response->headers[key]=value;
            }
        }
    } catch(const std::exception &) {
        response->error="Could not process the GitHub response headers.";return 0;
    }
    return bytes;
}
int transferProgress(void *opaque,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {
    auto *context=static_cast<TransferContext *>(opaque);
    if(context&&cancelled(context->cancel)) {
        context->response->cancelled=true;
        return 1;
    }
    return 0;
}

bool configureCurl(CURL *curl,TransferContext &ctx,HttpResponse &response,curl_slist *headers) {
    curl_blob caBundle{
        const_cast<char *>(kGithubUpdateCaBundle),
        sizeof(kGithubUpdateCaBundle)-1,
        CURL_BLOB_COPY
    };
    return curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https")==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,"https")==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_CAINFO_BLOB,&caBundle)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,kConnectTimeoutSeconds)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_TIMEOUT,kTransferTimeoutSeconds)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,128L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,transferProgress)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&ctx)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_1)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_USERAGENT,"WiiUNX2-Updater/" WIIUNX2_VERSION)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receiveBody)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_WRITEDATA,&ctx)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,receiveHeader)==CURLE_OK&&
           curl_easy_setopt(curl,CURLOPT_HEADERDATA,&response)==CURLE_OK;
}

bool requestUrl(const std::string &initialUrl,uint64_t maxBytes,const std::atomic<bool> *cancel,
                HttpResponse &response,const std::string &filePath={}) {
    static std::once_flag initOnce;
    static CURLcode initResult=CURLE_FAILED_INIT;
    std::call_once(initOnce,[] {initResult=curl_global_init(CURL_GLOBAL_DEFAULT);});
    if(initResult!=CURLE_OK) {response.error="HTTPS initialization failed.";return false;}
    if(!isAllowedDownloadUrl(initialUrl)) {response.error="The update server URL is not allowed.";return false;}
    ScopedFile download;
    if(!filePath.empty()) {
        const int fd=open(filePath.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd<0) {response.error="Could not create the update download on SD.";return false;}
        download.value=fdopen(fd,"wb");
        if(!download.value) {close(fd);unlink(filePath.c_str());response.error="Could not open the update download on SD.";return false;}
    }
    std::string url=initialUrl;
    for(unsigned hop=0;hop<=kMaxRedirects;++hop) {
        if(cancelled(cancel)) {response.cancelled=true;return false;}
        if(download.value) {
            if(std::fflush(download.value)!=0||ftruncate(fileno(download.value),0)!=0||
               std::fseek(download.value,0,SEEK_SET)!=0) {
                response.error="Could not reset the update download after a redirect.";return false;
            }
            std::clearerr(download.value);
        }
        response.status=0;response.body.clear();response.headers.clear();response.error.clear();response.overflow=false;
        std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
        if(!curl) {response.error="Could not create an HTTPS request.";return false;}
        TransferContext context{&response,download.value,maxBytes,0,cancel};
        curl_slist *headers=curl_slist_append(nullptr,"Accept: application/vnd.github+json");
        curl_slist *versionHeader=headers?curl_slist_append(headers,"X-GitHub-Api-Version: 2022-11-28"):nullptr;
        if(!versionHeader) {
            curl_slist_free_all(headers);
            response.error="Could not allocate HTTPS request headers.";return false;
        }
        headers=versionHeader;
        const bool configured=headers&&configureCurl(curl.get(),context,response,headers)&&
                               curl_easy_setopt(curl.get(),CURLOPT_URL,url.c_str())==CURLE_OK;
        CURLcode transfer=CURLE_FAILED_INIT;
        if(configured) transfer=curl_easy_perform(curl.get());
        curl_slist_free_all(headers);
        if(!configured) {response.error="Could not configure a verified HTTPS request.";return false;}
        if(transfer==CURLE_PEER_FAILED_VERIFICATION) {
            response.error="GitHub was reachable, but Wii U TLS certificate verification failed. Check the console date and time and ensure its system certificates are current.";
            return false;
        }
        curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&response.status);
        long verifyResult=-1;
        if(curl_easy_getinfo(curl.get(),CURLINFO_SSL_VERIFYRESULT,&verifyResult)!=CURLE_OK||verifyResult!=0) {
            response.error="The GitHub TLS certificate could not be verified.";return false;
        }
        if(response.cancelled||cancelled(cancel)) {response.cancelled=true;return false;}
        if(response.overflow) {response.error="The server response exceeded the size limit.";return false;}
        if(transfer!=CURLE_OK) {
            if(response.error.empty()) response.error="Could not reach GitHub over HTTPS ("+std::string(curl_easy_strerror(transfer))+"). Check the Wii U internet connection.";
            return false;
        }
        if(!response.error.empty()) return false;
        if(response.status==301||response.status==302||response.status==303||response.status==307||response.status==308) {
            const auto location=response.headers.find("location");
            if(filePath.empty()||hop==kMaxRedirects||location==response.headers.end()||
               !isTrustedRedirect(url,location->second)) {
                response.error="GitHub returned an unsafe or excessive redirect.";return false;
            }
            url=location->second;
            continue;
        }
        if(response.status!=200) {
            if(response.status==403||response.status==429) {
                const int wait=rateLimitWaitSeconds(static_cast<int>(response.status),response.headers,static_cast<int64_t>(std::time(nullptr)));
                response.error=wait>0?"GitHub rate limit reached. Try again in about "+std::to_string(wait)+" seconds.":"GitHub rejected the update request.";
            } else response.error="GitHub returned HTTP "+std::to_string(response.status)+".";
            return false;
        }
        if(download.value) {
            if(std::fflush(download.value)!=0||fsync(fileno(download.value))!=0) {
                response.error="Could not sync the update download to SD.";return false;
            }
            if(std::fclose(download.value)!=0) {
                download.value=nullptr;response.error="Could not close the update download on SD.";return false;
            }
            download.value=nullptr;
        }
        return true;
    }
    response.error="GitHub redirected the request too many times.";
    return false;
}

bool readRegularFile(const std::string &path,uint64_t maxBytes,std::vector<uint8_t> &out) {
    struct stat status{};
    if(lstat(path.c_str(),&status)!=0||!S_ISREG(status.st_mode)||status.st_size<0||
       static_cast<uint64_t>(status.st_size)>maxBytes) return false;
    FILE *file=std::fopen(path.c_str(),"rb");
    if(!file) return false;
    std::vector<uint8_t> data(static_cast<size_t>(status.st_size));
    const bool readOk=data.empty()||std::fread(data.data(),1,data.size(),file)==data.size();
    const int extra=std::fgetc(file);
    const bool closeOk=std::fclose(file)==0;
    if(!readOk||extra!=EOF||!closeOk) return false;
    out=std::move(data);
    return true;
}
bool hashRegularFile(const std::string &path,uint64_t expectedSize,const std::string &expectedHash) {
    struct stat status{};
    if(lstat(path.c_str(),&status)!=0||!S_ISREG(status.st_mode)||status.st_size<0||
       static_cast<uint64_t>(status.st_size)!=expectedSize) return false;
    FILE *file=std::fopen(path.c_str(),"rb");
    if(!file) return false;
    Sha256 sha;uint8_t block[16384];bool ok=true;uint64_t total=0;
    for(;;) {
        const size_t read=std::fread(block,1,sizeof(block),file);
        if(read) {sha.update(block,read);total+=read;}
        if(read<sizeof(block)) {if(std::ferror(file)) ok=false;break;}
    }
    if(std::fclose(file)!=0) ok=false;
    return ok&&total==expectedSize&&sha.finalHex()==expectedHash;
}
bool hashRegularFile(const std::string &path,uint64_t maxBytes,uint64_t &size,std::string &hash) {
    struct stat status{};
    if(lstat(path.c_str(),&status)!=0||!S_ISREG(status.st_mode)||status.st_size<0||
       static_cast<uint64_t>(status.st_size)>maxBytes) return false;
    FILE *file=std::fopen(path.c_str(),"rb");
    if(!file) return false;
    Sha256 sha;uint8_t block[16384];bool ok=true;uint64_t total=0;
    for(;;) {
        const size_t read=std::fread(block,1,sizeof(block),file);
        if(read) {sha.update(block,read);total+=read;}
        if(read<sizeof(block)) {if(std::ferror(file)) ok=false;break;}
    }
    if(std::fclose(file)!=0) ok=false;
    if(!ok||total!=static_cast<uint64_t>(status.st_size)) return false;
    size=total;hash=sha.finalHex();return true;
}

bool fileExists(const std::string &path) {
    struct stat status{};
    if(lstat(path.c_str(),&status)==0) return true;
    return errno!=ENOENT;
}

bool copyFileExclusive(const std::string &source,const std::string &destination) {
    struct stat sourceStat{};
    if(lstat(source.c_str(),&sourceStat)!=0||!S_ISREG(sourceStat.st_mode)||sourceStat.st_size<1||
       static_cast<uint64_t>(sourceStat.st_size)>256ull*1024*1024) return false;
    const int in=open(source.c_str(),O_RDONLY);
    if(in<0) return false;
    const int out=open(destination.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(out<0) {close(in);return false;}
    uint8_t block[16384];bool ok=true;uint64_t copied=0;
    for(;;) {
        const ssize_t count=read(in,block,sizeof(block));
        if(count<0&&errno==EINTR) continue;
        if(count<0) {ok=false;break;}
        if(count==0) break;
        copied+=static_cast<uint64_t>(count);
        size_t written=0;
        while(written<static_cast<size_t>(count)) {
            const ssize_t part=write(out,block+written,static_cast<size_t>(count)-written);
            if(part<0&&errno==EINTR) continue;
            if(part<=0) {ok=false;break;}
            written+=static_cast<size_t>(part);
        }
        if(!ok) break;
    }
    if(copied!=static_cast<uint64_t>(sourceStat.st_size)) ok=false;
    if(ok&&fsync(out)!=0) ok=false;
    if(close(in)!=0) ok=false;
    if(close(out)!=0) ok=false;
    struct stat backupStat{};
    if(ok&&(lstat(destination.c_str(),&backupStat)!=0||!S_ISREG(backupStat.st_mode)||
            backupStat.st_size!=sourceStat.st_size)) ok=false;
    uint64_t verifiedSize=0;std::string sourceHash,backupHash;
    if(ok&&(!hashRegularFile(source,256ull*1024*1024,verifiedSize,sourceHash)||
            !hashRegularFile(destination,256ull*1024*1024,verifiedSize,backupHash)||sourceHash!=backupHash)) ok=false;
    if(!ok) unlink(destination.c_str());
    return ok;
}
struct SdUpdateFiles {
    bool isRegular(const std::string &path) const {
        struct stat status{};
        return lstat(path.c_str(),&status)==0&&S_ISREG(status.st_mode);
    }
    bool exists(const std::string &path) const {return fileExists(path);}
    bool copyVerified(const std::string &source,const std::string &destination) const {
        return copyFileExclusive(source,destination);
    }
    bool renameNoReplace(const std::string &source,const std::string &destination) const {
        return !fileExists(destination)&&rename(source.c_str(),destination.c_str())==0;
    }
    bool atomicReplace(const std::string &source,const std::string &destination) const {
        return rename(source.c_str(),destination.c_str())==0;
    }
    void removeFile(const std::string &path) const {unlink(path.c_str());}
    bool hashMatches(const std::string &path,uint64_t size,const std::string &hash) const {
        return hashRegularFile(path,size,hash);
    }
};

bool activeStandardEnvironment() {
    alignas(64) char environment[256]{};
    alignas(64) int command=0xf9;
    const auto handle=IOS_Open("/dev/mcp",IOS_OPEN_READ);
    if(handle<0) return false;
    const auto result=IOS_Ioctl(handle,100,&command,sizeof(command),environment,sizeof(environment));
    IOS_Close(handle);
    if(result!=IOS_ERROR_OK) return false;
    environment[sizeof(environment)-1]=0;
    return std::strcmp(environment,kStandardEnvironment)==0;
}

bool resolveActiveUpdateTarget(UpdateTarget &target,std::string &error) {
    if(!activeStandardEnvironment()) {
        error="Automatic update disabled: the active environment could not be verified as the standard Aroma environment.";return false;
    }

    struct stat standardStat{};
    const std::string standardPath=std::string(kUpdateSdRoot)+kStandardAromaExecutable;
    const bool standardExists=lstat(standardPath.c_str(),&standardStat)==0&&
        S_ISREG(standardStat.st_mode)&&standardStat.st_size>0;

    static const RPXLoaderStatus loaderStatus=RPXLoader_InitLibrary();
    char executablePath[256]{};
    bool executablePathAvailable=false;
    std::string reportedPath;
    if(loaderStatus==RPX_LOADER_RESULT_SUCCESS&&
       RPXLoader_GetPathOfRunningExecutable(executablePath,sizeof(executablePath))==RPX_LOADER_RESULT_SUCCESS) {
        const auto *terminator=static_cast<const char *>(std::memchr(executablePath,0,sizeof(executablePath)));
        if(!terminator) {
            error="Automatic update disabled: RPXLoadingModule returned an unterminated executable path.";return false;
        }
        executablePathAvailable=true;
        reportedPath.assign(executablePath,static_cast<size_t>(terminator-executablePath));
    }

    if(!resolveUpdateTarget(executablePathAvailable,reportedPath,standardExists,target)) {
        error="Automatic update disabled: WiiUNX2's active RPX location could not be identified safely. Install manually or use RPXLoadingModule API v2 or newer.";return false;
    }

    struct stat targetStat{};
    if(lstat(target.path.c_str(),&targetStat)!=0||!S_ISREG(targetStat.st_mode)||targetStat.st_size<=0) {
        error="Automatic update disabled: the detected WiiUNX2 RPX is missing or is not a regular file.";return false;
    }
    return true;
}

std::string stampNow() {
    const uint64_t stamp=static_cast<uint64_t>(OSGetTime());
    static const char digits[]="0123456789abcdef";
    std::string result;
    uint64_t value=stamp;
    do {result.push_back(digits[value&0xf]);value>>=4;} while(value);
    std::reverse(result.begin(),result.end());
    return result;
}

bool hasWiiURpxHeader(const std::string &path) {
    struct stat status{};
    if(lstat(path.c_str(),&status)!=0||!S_ISREG(status.st_mode)||status.st_size<6) return false;
    const int fd=open(path.c_str(),O_RDONLY);
    if(fd<0) return false;
    uint8_t header[6]{};size_t offset=0;bool ok=true;
    while(offset<sizeof(header)) {
        const ssize_t count=read(fd,header+offset,sizeof(header)-offset);
        if(count<0&&errno==EINTR) continue;
        if(count<=0) {ok=false;break;}
        offset+=static_cast<size_t>(count);
    }
    if(close(fd)!=0) ok=false;
    return ok&&isWiiURpxHeader(header,sizeof(header));
}

bool prepareSelection(const std::string &version,const UpdateSelection &selection,std::string &error) {
    Version parsed;
    std::string rpxDigest,checksumDigest;
    if(!parseVersion(version,parsed)||selection.rpx.name!="men.rpx"||
       selection.checksum.name!="men.rpx.sha256"||selection.rpx.state!="uploaded"||
       selection.checksum.state!="uploaded"||selection.rpx.size<6||selection.rpx.size>kMaxRpxBytes||
       selection.checksum.size==0||selection.checksum.size>kMaxChecksumBytes||
       !detail::normalizeSha256Hex(selection.rpx.sha256,rpxDigest)||!detail::normalizeSha256Hex(selection.checksum.sha256,checksumDigest)||
       !isAllowedDownloadUrl(selection.rpx.downloadUrl)||!isAllowedDownloadUrl(selection.checksum.downloadUrl)) {
        error="The selected release metadata is incomplete or unsafe.";return false;
    }
    return true;
}

UpdateOperationResult installSelected(const std::string &version,const UpdateSelection &selection,
                                      const std::string &expectedTargetPath,const std::atomic<bool> *cancel) {
    UpdateOperationResult result;
    std::string error;
    if(!prepareSelection(version,selection,error)) {result.message=error;return result;}
    UpdateTarget target;
    if(!resolveActiveUpdateTarget(target,error)) {result.message=error;return result;}
    if(target.path!=expectedTargetPath) {
        result.message="Install stopped: the detected WiiUNX2 RPX location changed after confirmation. Check for updates again.";return result;
    }
    if(cancelled(cancel)) {result.message="Update cancelled. The installed menu was not changed.";return result;}

    const std::string stamp=stampNow();
    const std::string rpxPath=target.path+".download-"+stamp+".rpx";
    const std::string checksumPath=target.path+".checksum-"+stamp+".tmp";
    const std::string stagePath=target.path+".update-"+stamp+".tmp";
    const std::string backupLeaf=backupName("men.rpx",stamp);
    const size_t targetSlash=target.path.find_last_of('/');
    const std::string targetDirectory=targetSlash==std::string::npos?std::string():target.path.substr(0,targetSlash);
    const std::string backupPath=backupLeaf.empty()||targetDirectory.empty()?std::string():targetDirectory+"/"+backupLeaf;
    const std::string backupPart=backupPath.empty()?std::string():backupPath+".part";
    if(backupPath.empty()||fileExists(rpxPath)||fileExists(checksumPath)||fileExists(stagePath)||
       fileExists(backupPath)||fileExists(backupPart)) {
        result.message="Install stopped: a temporary or backup path already exists.";return result;
    }
    auto cleanup=[&] {unlink(rpxPath.c_str());unlink(checksumPath.c_str());unlink(stagePath.c_str());};

    HttpResponse response;
    if(!requestUrl(selection.rpx.downloadUrl,selection.rpx.size,cancel,response,rpxPath)) {
        cleanup();result.message=response.cancelled?"Update cancelled. The installed menu was not changed.":response.error;return result;
    }
    std::string apiRpxDigest;
    detail::normalizeSha256Hex(selection.rpx.sha256,apiRpxDigest);
    if(!hashRegularFile(rpxPath,selection.rpx.size,apiRpxDigest)) {
        cleanup();result.message="The downloaded RPX failed its GitHub SHA-256 or size check.";return result;
    }
    if(cancelled(cancel)) {cleanup();result.message="Update cancelled. The installed menu was not changed.";return result;}

    response=HttpResponse{};
    if(!requestUrl(selection.checksum.downloadUrl,selection.checksum.size,cancel,response,checksumPath)) {
        cleanup();result.message=response.cancelled?"Update cancelled. The installed menu was not changed.":response.error;return result;
    }
    std::string apiChecksumDigest;
    detail::normalizeSha256Hex(selection.checksum.sha256,apiChecksumDigest);
    if(!hashRegularFile(checksumPath,selection.checksum.size,apiChecksumDigest)) {
        cleanup();result.message="The downloaded checksum asset failed its GitHub SHA-256 or size check.";return result;
    }
    std::vector<uint8_t> checksumBytes;
    if(!readRegularFile(checksumPath,kMaxChecksumBytes,checksumBytes)) {
        cleanup();result.message="The release checksum file could not be read safely.";return result;
    }
    const std::string checksumText(checksumBytes.begin(),checksumBytes.end());
    std::string expectedRpxHash;
    if(!parseSha256File(checksumText,"men.rpx",expectedRpxHash)||expectedRpxHash!=apiRpxDigest) {
        cleanup();result.message="The RPX sidecar checksum does not match the GitHub asset digest.";return result;
    }
    if(!hasWiiURpxHeader(rpxPath)) {
        cleanup();result.message="The downloaded file is not a valid Wii U ELF32 big-endian RPX.";return result;
    }
    if(cancelled(cancel)) {cleanup();result.message="Update cancelled. The installed menu was not changed.";return result;}
    if(!copyFileExclusive(rpxPath,stagePath)) {
        cleanup();result.message="The verified menu binary could not be staged on SD.";return result;
    }
    uint64_t oldRpxSize=0;std::string oldRpxHash;
    if(!hashRegularFile(target.path,256ull*1024*1024,oldRpxSize,oldRpxHash)) {
        cleanup();result.message="The existing menu could not be hashed safely; it was not replaced.";return result;
    }
    const std::string restorePart=target.path+".restore-"+stamp+".part";
    SdUpdateFiles files;
    const UpdateReplaceResult replacement=replaceWithVerifiedBackup(files,target.path,stagePath,backupPath,
        backupPart,restorePart,selection.rpx.size,apiRpxDigest,oldRpxSize,oldRpxHash);
    if(replacement!=UpdateReplaceResult::Installed) {
        cleanup();
        if(replacement==UpdateReplaceResult::BackupFailed)
            result.message="The existing menu could not be backed up and verified; it was not replaced.";
        else if(replacement==UpdateReplaceResult::BackupPublishFailed)
            result.message="The verified backup could not be published; the installed menu was not replaced.";
        else if(replacement==UpdateReplaceResult::ReplaceFailed)
            result.message="Atomic replacement was not supported. The original menu remains installed and its verified backup is at "+target.displayPath+".backup-"+stamp+".";
        else if(replacement==UpdateReplaceResult::Restored)
            result.message="The replacement failed verification and the previous menu was restored. The verified backup is preserved at "+target.displayPath+".backup-"+stamp+".";
        else if(replacement==UpdateReplaceResult::RecoveryRequired)
            result.message="The replacement failed verification. Power off and manually restore the verified backup at "+target.displayPath+".backup-"+stamp+".";
        else result.message="The update transaction found an unexpected path state; the installed menu was not intentionally replaced.";
        return result;
    }
    cleanup();
    result.success=true;
    result.message="WiiUNX2 v"+version+" was installed at "+target.displayPath+". Restart the console to load it. The previous menu is preserved beside it as men.rpx.backup-"+stamp+".";
    return result;
}
}  // namespace

UpdateCheckResult checkLatestUpdate(const std::string &currentVersion,const std::atomic<bool> *cancel) {
    UpdateCheckResult result;
    Version parsedCurrent;
    if(!parseVersion(currentVersion,parsedCurrent)) {result.message="The installed version is invalid; update check stopped.";return result;}
    HttpResponse response;
    if(!requestUrl(kLatestReleaseUrl,kMaxReleaseJsonBytes,cancel,response)) {
        result.message=response.cancelled?"Update check cancelled.":response.error;return result;
    }
    Release release;
    if(!parseLatestRelease(response.body,release)) {result.message="GitHub returned invalid or unsupported release metadata.";return result;}
    UpdateSelection selection;
    const SelectResult selected=selectAssets(release,currentVersion,selection);
    if(selected==SelectResult::UpToDate) {
        result.success=true;result.message="WiiUNX2 is up to date (v"+currentVersion+").";return result;
    }
    if(selected==SelectResult::NoAsset) {result.message="The latest release does not contain both required update assets.";return result;}
    if(selected!=SelectResult::UpdateAvailable) {result.message="The latest release metadata failed validation.";return result;}
    std::string rpxDigest,checksumDigest;
    if(!detail::normalizeSha256Hex(selection.rpx.sha256,rpxDigest)||!detail::normalizeSha256Hex(selection.checksum.sha256,checksumDigest)) {
        result.message="This release has no valid GitHub SHA-256 asset digests; installation is disabled.";return result;
    }
    UpdateTarget target;
    if(!resolveActiveUpdateTarget(target,result.message)) return result;
    result.success=true;result.available=true;result.version=release.tag[0]=='v'?release.tag.substr(1):release.tag;
    result.selection=std::move(selection);
    result.targetPath=std::move(target.path);
    result.targetDisplayPath=std::move(target.displayPath);
    result.message="WiiUNX2 v"+result.version+" is available.";
    return result;
}

UpdateOperationResult installUpdate(const std::string &currentVersion,const std::string &version,
                                    const UpdateSelection &selection,const std::string &expectedTargetPath,
                                    const std::atomic<bool> *cancel) {
    Version current,latest;
    if(!parseVersion(currentVersion,current)||!parseVersion(version,latest)||compareVersions(latest,current)<=0) {
        return {false,"Install stopped: the update version is not newer than the installed version."};
    }
    return installSelected(version,selection,expectedTargetPath,cancel);
}
}  // namespace nx
