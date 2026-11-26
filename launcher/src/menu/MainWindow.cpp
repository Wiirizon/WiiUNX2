// WiiUNX2, GPL-3.0-or-later. Original menu implementation for launchiine.
#include "MainWindow.h"
#include "HomeGeometry.h"
#include "AccountPrompt.h"
#include "gui/SelectionGradient.h"
#include "IconPlan.h"
#include "Library.h"
#include "Reconcile.h"
#include "DiscState.h"
#include "game/SystemTitles.h"
#include "game/Homebrew.h"
#include "utils/AtomicHistory.h"
#include "utils/LaunchLog.h"
#include "utils/UserDataPaths.h"
#include <rpxloader/rpxloader.h>
#include <tuple>
#include "game/Metadata.h"
#include "fs/FSUtils.h"
#include "utils/logger.h"
#include <nn/act.h>
#include <nn/pdm/pdm_cpp.h>
#include <sysapp/title.h>
#include <malloc.h>
#include "resources/Resources.h"
#include <gui/GuiController.h>
#include <gui/VPadController.h>
#include <nn/ac.h>
#include <coreinit/time.h>
#include <coreinit/launch.h>
#include <coreinit/dynload.h>
#include <nn/acp/title.h>
#include <sysapp/launch.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include "common/common.h"
extern "C" int32_t ACPCheckTitleLaunchByTitleListTypeEx(MCPTitleListType*,uint32_t);

static const char *dockNames[] = {"Friend List", "Internet Browser", "Download Management", "Nintendo eShop", "Mii Maker", "Data Management", "Controllers", "System Settings", "Power"};
static const char *dockFiles[] = {"nxFriends.png", "nxBrowser.png", "nxDownloads.png", "nxShop.png", "nxMii.png", "nxData.png", "nxControllers.png", "nxSettings.png", "nxPower.png"};
static const char *keys=nx::keyboardCharacters();
static std::string accountChoicePath() {
    return nx::userDataPath(WiiUNX2UserDataDirectoryReady(),WiiUNX2UserDataMigrationComplete(),
                           "account-chosen.txt","wiiunx2-account-chosen.txt");
}
static bool isMenu(uint64_t id) { return (id >> 32) == 0x00050010 && ((id & 0xffffff00) == 0x10040000 || (id & 0xffffff00) == 0x10040100 || (id & 0xffffff00) == 0x10040200); }
static constexpr uint64_t discTileId=0xfffffffffffffffeULL;
static uint64_t bootTimeTicks() {
    return static_cast<uint64_t>(OSGetTime()-OSGetSystemTime());
}
static bool accountWasChosenThisBoot() {
    uint8_t *buffer=nullptr;uint32_t size=0;
    const std::string path=accountChoicePath();
    if(FSUtils::LoadFileToMem(path.c_str(),&buffer,&size,64)<=0 || !buffer) return false;
    std::string text(reinterpret_cast<char *>(buffer),size);free(buffer);
    char *end=nullptr;
    const unsigned long long saved=std::strtoull(text.c_str(),&end,16);
    if(end==text.c_str()) return false;
    while(*end=='\r'||*end=='\n'||*end==' '||*end=='\t') ++end;
    return *end=='\0' && nx::accountChoiceMatchesBoot(saved,bootTimeTicks(),OSSecondsToTicks(5));
}
static void rememberAccountChoiceThisBoot() {
    FSUtils::CreateSubfolder("fs:/vol/external01/wiiu");
    char text[32];const int length=std::snprintf(text,sizeof(text),"%016llX\n",
                                                static_cast<unsigned long long>(bootTimeTicks()));
    const std::string path=accountChoicePath();
    if(length<=0 || length>=int(sizeof(text)) ||
       FSUtils::saveBufferToFile(path.c_str(),text,uint32_t(length))!=length)
        DEBUG_FUNCTION_LINE("Could not save the boot account-choice marker");
}
static void logAutoDiscCandidates(int handle) {
    constexpr size_t capacity=8192;
    const size_t bytes=capacity*sizeof(MCPTitleListType);
    std::unique_ptr<void,decltype(&free)> storage(memalign(64,bytes),&free);
    if(!storage) {
        DEBUG_FUNCTION_LINE("MCP AUTO diagnostic allocation failed for %u bytes",unsigned(bytes));
        return;
    }
    uint32_t count=0;
    const auto result=MCP_TitleListByDeviceType(handle,MCP_DEVICE_TYPE_AUTO,&count,
        static_cast<MCPTitleListType *>(storage.get()),uint32_t(bytes));
    if(result<0) {
        DEBUG_FUNCTION_LINE("MCP AUTO diagnostic query failed: %d",int(result));
        return;
    }
    if(count>capacity) {
        DEBUG_FUNCTION_LINE("MCP AUTO diagnostic returned too many entries: %u",unsigned(count));
        return;
    }
    uint32_t wiiEntries=0,oddEntries=0;
    for(uint32_t i=0;i<count;++i) {
        const auto &entry=static_cast<MCPTitleListType *>(storage.get())[i];
        const bool wii=entry.appType==MCP_APP_TYPE_GAME_WII;
        const bool odd=std::strncmp(entry.indexedDevice,"odd",3)==0;
        if(!wii&&!odd) continue;
        if(wii) ++wiiEntries;
        if(odd) ++oddEntries;
        DEBUG_FUNCTION_LINE("MCP AUTO candidate type 0x%08X device '%.*s'",
            unsigned(entry.appType),int(sizeof(entry.indexedDevice)),entry.indexedDevice);
    }
    if(!wiiEntries&&!oddEntries)
        DEBUG_FUNCTION_LINE("MCP AUTO diagnostic listed %u titles; no Wii-game or ODD-tagged entries",unsigned(count));
    else
        DEBUG_FUNCTION_LINE("MCP AUTO diagnostic listed %u titles; Wii-game entries=%u ODD-tagged entries=%u",
            unsigned(count),unsigned(wiiEntries),unsigned(oddEntries));
}
static nx::DiscSample readDisc(MCPTitleListType &title,uint32_t *appTypeOut=nullptr) {
    static int lastOpenError=0,lastListError=0;
    static bool openErrorLogged=false,listErrorLogged=false,emptyLogged=false,overflowLogged=false,autoDiagnosticDone=false;
    alignas(64) MCPTitleListType entries[4]{};uint32_t count=0;
    int handle=MCP_Open();
    if(handle<0) {
        if(!openErrorLogged||handle!=lastOpenError) DEBUG_FUNCTION_LINE("Disc MCP_Open failed: %d",handle);
        openErrorLogged=true;lastOpenError=handle;
        listErrorLogged=false;emptyLogged=false;overflowLogged=false;
        return nx::DiscSample::error();
    }
    openErrorLogged=false;
    auto result=MCP_TitleListByDeviceType(handle,MCP_DEVICE_TYPE_ODD,&count,entries,sizeof(entries));
    if(result<0) {
        MCP_Close(handle);
        if(!listErrorLogged||result!=lastListError) DEBUG_FUNCTION_LINE("Disc ODD query failed: %d",int(result));
        listErrorLogged=true;lastListError=result;
        emptyLogged=false;overflowLogged=false;
        return nx::DiscSample::error();
    }
    listErrorLogged=false;
    if(count>4) {
        MCP_Close(handle);
        if(!overflowLogged) DEBUG_FUNCTION_LINE("Disc ODD query returned too many entries: %u",unsigned(count));
        overflowLogged=true;
        return nx::DiscSample::error();
    }
    overflowLogged=false;
    if(count==0) {
        if(!emptyLogged) DEBUG_FUNCTION_LINE("Disc ODD query returned no entries");
        emptyLogged=true;
        if(!autoDiagnosticDone) {
            autoDiagnosticDone=true;
            logAutoDiscCandidates(handle);
        }
        MCP_Close(handle);
        return nx::DiscSample::empty();
    }
    MCP_Close(handle);
    emptyLogged=false;
    for(uint32_t i=0;i<count;++i) if(entries[i].appType==MCP_APP_TYPE_GAME) {
        title=entries[i];if(appTypeOut) *appTypeOut=uint32_t(entries[i].appType);
        return nx::DiscSample::game(entries[i].titleId);
    }
    for(uint32_t i=0;i<count;++i) if(entries[i].appType==MCP_APP_TYPE_GAME_WII) {
        title=entries[i];if(appTypeOut) *appTypeOut=uint32_t(entries[i].appType);
        return nx::DiscSample::game(entries[i].titleId,nx::DiscPlatform::Wii);
    }
    if(count>0 && appTypeOut) *appTypeOut=uint32_t(entries[0].appType);
    return nx::DiscSample::other();
}

MainWindow::MainWindow(int32_t w, int32_t h, MenuMusic *sound)
 : background(w,h,GX2Color{233,233,233,255}), modalShade(w,h,GX2Color{0,0,0,130}),
   batteryOutline(32,18,GX2Color{70,70,70,255}), batteryInside(28,14,GX2Color{233,233,233,255}),
   batteryFill(24,10,GX2Color{67,160,44,255}), batteryTip(3,8,GX2Color{70,70,70,255}),
   updateProgressTrack(620,12,GX2Color{190,190,190,255}),updateProgressFill(620,12,GX2Color{24,135,205,255}),
   title("Loading software...",26,glm::vec4(.12f,.12f,.12f,1)),
   clockText("",23,glm::vec4(.1f,.1f,.1f,1)),
   label("",23,glm::vec4(.12f,.36f,.62f,1)),
   message("",26,glm::vec4(.12f,.12f,.12f,1)),
   brand("WiiUNX2",19,glm::vec4(.32f,.32f,.32f,1)),
   accountHeading("Select an account",30,glm::vec4(.12f,.12f,.12f,1)),
   profileName("",22,glm::vec4(.12f,.12f,.12f,1)),
   libraryStatus("",18,glm::vec4(.4f,.4f,.4f,1)),
   searchHeading("Find software",30,glm::vec4(.12f,.12f,.12f,1)),
   searchQuery("",27,glm::vec4(.12f,.36f,.62f,1)),
   width(w),height(h) {
    audio=sound;
    title.setMaxWidth(1070,GuiText::DOTTED);
    message.setMaxWidth(840,GuiText::WRAP);
    profileName.setAlignment(ALIGN_LEFT | ALIGN_MIDDLE);
    profileName.setMaxWidth(420,GuiText::DOTTED);
    auto asset = [this](const char *name) {
        auto *data=Resources::GetImageData(name);
        assetData.push_back(data);
        return std::unique_ptr<GuiImage>(new GuiImage(data));
    };
    for (auto name:dockFiles) dock.push_back(asset(name));
    profile=asset("nxProfile.png");profileBacking=asset("nxProfileBacking.png");ring=asset("nxRing.png");
    network=asset("nxNetwork.png");discIcon=asset("nxDisc.png");
    tileBlank=asset("nxTileBlank.png");tileShadow=asset("nxTileShadow.png");dockSurface=asset("nxDock.png");
    for(unsigned i=0;i<8;++i) tileFocusFrames.push_back(asset(("nxTileFocus"+std::to_string(i)+".png").c_str()));
    modalPanel=asset("nxModalPanel.png");accountPanel=asset("nxAccountPanel.png");
    accountCard=asset("nxAccountCard.png");
    searchPanel=asset("nxSearchPanel.png");keyCard=asset("nxKeyCard.png");keyFocus=asset("nxKeyFocus.png");
    const char *badges[]={"nxButtonY.png","nxButtonMinus.png","nxButtonPlus.png","nxButtonA.png"};
    const char *captions[]={"Search","Sort","Options","OK"};
    for(int i=0;i<4;++i) {
        FooterPrompt prompt;prompt.badge=asset(badges[i]);
        prompt.caption=std::make_unique<GuiText>(captions[i],20,glm::vec4(.15f,.15f,.15f,1));
        prompt.caption->setAlignment(ALIGN_LEFT | FTGX_ALIGN_BASELINE);
        prompts.push_back(std::move(prompt));
    }
    auto addPrompt=[&](std::vector<FooterPrompt> &items,const char *badge,const char *caption) {
        FooterPrompt prompt;prompt.badge=asset(badge);
        prompt.caption=std::make_unique<GuiText>(caption,20,glm::vec4(.15f,.15f,.15f,1));
        prompt.caption->setAlignment(ALIGN_LEFT | FTGX_ALIGN_BASELINE);items.push_back(std::move(prompt));
    };
    addPrompt(modalPrompts,"nxButtonB.png","Back");addPrompt(modalPrompts,"nxButtonA.png","OK");
    addPrompt(searchPrompts,"nxButtonDpad.png","Choose key");addPrompt(searchPrompts,"nxButtonX.png","Erase");
    addPrompt(searchPrompts,"nxButtonPlus.png","Done");addPrompt(searchPrompts,"nxButtonB.png","Back");addPrompt(searchPrompts,"nxButtonA.png","Type");
    addPrompt(accountPrompts,"nxButtonL.png","Previous users");addPrompt(accountPrompts,"nxButtonR.png","Next users");
    addPrompt(accountPrompts,"nxButtonB.png","Back");addPrompt(accountPrompts,"nxButtonA.png","Select");
    acReady=NNResult_IsSuccess(ACInitialize());
    for(int i=0;i<40;++i) {
        std::string caption=i<36 ? std::string(1,keys[i]) : i==36 ? "Space":i==37 ? "Erase":i==38 ? "Clear":"Done";
        keyLabels.push_back(std::make_unique<GuiText>(caption.c_str(),i<36?22:16,glm::vec4(.15f,.15f,.15f,1)));
    }
    searchQuery.setMaxWidth(950,GuiText::DOTTED);
    actReady=nn::act::Initialize().IsSuccess();
    refreshAccounts();
    if(actReady && !accounts.empty() &&
       nx::shouldPromptForAccount(nn::act::GetDefaultAccount(),accountWasChosenThisBoot())) {
        accountPromptRequired=true;accountsOpen=true;
        accountPrompts[2].caption->setText("Use current");
    }
}
MainWindow::~MainWindow() {
    updateCancel=true;
    if(updateCheck.valid()) updateCheck.wait();
    if(updateInstall.valid()) updateInstall.wait();
    stopTitleWorkers();
    accounts.clear();
    if(actReady) nn::act::Finalize();
    gameList.titleAdded.disconnect(this);
    gameList.titleUpdated.disconnect(this);
    gameList.titleListChanged.disconnect(this);
    tiles.clear(); dock.clear();tileFocusFrames.clear();prompts.clear();modalPrompts.clear();searchPrompts.clear();accountPrompts.clear(); profile.reset(); profileBacking.reset(); ring.reset(); network.reset();discIcon.reset();tileBlank.reset();tileShadow.reset();dockSurface.reset();
    modalPanel.reset();accountPanel.reset();accountCard.reset();searchPanel.reset();keyCard.reset();keyFocus.reset();
    for(auto *data:assetData) Resources::RemoveImageData(data);
    if(acReady) ACFinalize();
}
void MainWindow::resume(FreeTypeGX *font) {
    launching=false;
    for(auto *text:{&title,&clockText,&label,&message,&brand,&accountHeading,&profileName,&libraryStatus,&searchHeading,&searchQuery}) text->setFont(font);
    for(auto *items:{&prompts,&modalPrompts,&searchPrompts,&accountPrompts}) for(auto &prompt:*items) prompt.caption->setFont(font);
}
void MainWindow::listChanged(GameList *) {}
void MainWindow::titleChanged(gameInfo *info) {
    if(info && info->titleId==discTileId) {
        discRecentId=info->launchId;
        discIsInserted=info->launchId!=0&&!info->name.empty();
        auto it=std::find_if(tiles.begin(),tiles.end(),[](const auto &t){return t.id==discTileId;});
        if(it!=tiles.end() && (info->name.empty() || it->path!=info->gamePath)) {tiles.erase(it);viewDirty=true;}
        if(info->name.empty()) return;
    }
    if(!info || isMenu(info->titleId)) return;
    // Installed games and system applications remain available in the row.
    std::lock_guard<std::recursive_mutex> guard(tilesMutex);
    for(auto &t:tiles) if(t.id==info->titleId) {
        t.name=info->name; viewDirty=true; return;
    }
    tiles.push_back({info->titleId,info->appType,info->name,info->gamePath,nullptr,nullptr});
    viewDirty=true;
}
void MainWindow::drawImage(GuiImage &image,CVideo *v,float x,float y) {
    image.setPosition(x-width/2.f,height/2.f-y); image.draw(v);
}
void MainWindow::drawText(GuiText &text,CVideo *v,float x,float y) {
    text.setPosition(x-width/2.f,height/2.f-y); text.draw(v);
}
void MainWindow::process() {
    if(updateCheck.valid()&&updateCheck.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=updateCheck.get();updateBusy=false;
            if(result.success&&result.available) {
                pendingUpdate=std::move(result.selection);pendingUpdateVersion=std::move(result.version);
                pendingUpdateTargetPath=std::move(result.targetPath);
                showActionMessage("WiiUNX2 v"+pendingUpdateVersion+" is available.\nThe RPX and its checksum are verified over HTTPS. This project does not sign releases.\nInstall target: "+result.targetDisplayPath+"\nA verified backup will be kept beside it.\nKeep the console powered on and do not remove the SD card during installation.",
                                  "Install",ModalAction::InstallUpdate);
            } else showMessage(result.message);
        } catch(const std::exception &) {
            updateBusy=false;showMessage("The update check failed unexpectedly. The installed menu was not changed.");
        }
    }
    if(updateInstall.valid()&&updateInstall.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=updateInstall.get();updateBusy=false;showMessage(result.message);
        } catch(const std::exception &) {
            updateBusy=false;showMessage("The updater failed unexpectedly. Check the Aroma SD folder and restore its verified backup if needed.");
        }
    }
    if(historyRead.valid() && historyRead.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto result=historyRead.get();
        if(historySlot==activeSlot) {nativeDays=std::move(result);viewDirty=true;}
    }
    if(!historyRead.valid() && historySlot!=activeSlot) {
        historySlot=activeSlot;nativeDays.clear();viewDirty=true;
        auto slot=activeSlot;
        historyRead=std::async(std::launch::async,[slot] {
            std::unordered_map<uint64_t,uint32_t> dates;
            if(slot<1 || slot>12 || nn::pdm::Initialize()!=0) return dates;
            uint32_t length=0,actual=0;
            if(nn::pdm::GetPlayStatsLength(&length,slot)==0 && length>0 && length<=8192) {
                std::vector<nn::pdm::PlayStats> stats(length);
                if(nn::pdm::GetPlayStats(&actual,stats.data(),slot,length)==0 && actual<=length)
                    for(uint32_t i=0;i<actual;++i) dates[stats[i].title_id]=stats[i].last_time_played;
            }
            nn::pdm::Finalize();
            DEBUG_FUNCTION_LINE("Native play history: %u titles",unsigned(dates.size()));
            return dates;
        });
    }
    std::lock_guard<std::recursive_mutex> guard(tilesMutex);
    ++frame;
    if(shutdownFrame && frame>=shutdownFrame) OSShutdown();
    if(frame==1) heartbeatTicks=OSGetSystemTime();
    if(!listStarted && frame>=2 && !stopLoader) {
        listStarted=true;focusRow=restore.row;dockIndex=restore.dock;sortMode=restore.sort;query=restore.query;
        // Worker touches only CPU metadata; GUI objects and textures stay on this thread.
        loader=std::thread([this] {
            auto began=OSGetSystemTime();
            loaderFailed=gameList.load()<0;
            DEBUG_FUNCTION_LINE("Installed enumeration: %llu ms",(unsigned long long)OSTicksToMilliseconds(OSGetSystemTime()-began));
            for(int i=0;i<gameList.size() && !stopLoader;++i) {
                gameList.loadNext();
                std::lock_guard<std::mutex> guard(pendingMutex);
                pending.push_back(*gameList.at(i));
                if(i==0) DEBUG_FUNCTION_LINE("First installed title queued: %llu ms",(unsigned long long)OSTicksToMilliseconds(OSGetSystemTime()-began));
            }
            // Publish installed software before any SD traversal or bundle work.
            if(!stopLoader) {
                int firstHomebrew=gameList.size();
                auto scanStart=OSGetSystemTime();
                gameList.loadHomebrew();
                DEBUG_FUNCTION_LINE("SD discovery: %llu ms",(unsigned long long)OSTicksToMilliseconds(OSGetSystemTime()-scanStart));
                for(int i=firstHomebrew;i<gameList.size() && !stopLoader;++i) {
                    gameList.loadNext();
                    std::lock_guard<std::mutex> guard(pendingMutex);
                    pending.push_back(*gameList.at(i));
                }
            }
            DEBUG_FUNCTION_LINE("Metadata scan finished in %llu ms",(unsigned long long)OSTicksToMilliseconds(OSGetSystemTime()-began));
            loaderDone=true;
            gameList.saveNameCache();
            nx::DiscMonitor monitor;
            uint32_t lastOtherType=0;
            bool otherLogged=false;
            while(!stopLoader) {
                MCPTitleListType disc{};
                uint32_t discAppType=0;
                const auto sample=readDisc(disc,&discAppType);
                const bool unsupported=sample.kind==nx::DiscSample::Other;
                const bool reportDiscType=unsupported||sample.platform==nx::DiscPlatform::Wii;
                if(reportDiscType && (!otherLogged || discAppType!=lastOtherType)) {
                    if(sample.platform==nx::DiscPlatform::Wii) DEBUG_FUNCTION_LINE("Wii disc ODD app type 0x%08X",unsigned(discAppType));
                    else DEBUG_FUNCTION_LINE("Unsupported disc entry, app type 0x%08X",unsigned(discAppType));
                }
                if(sample.kind!=nx::DiscSample::Error) {
                    otherLogged=reportDiscType;lastOtherType=reportDiscType?discAppType:0;
                }
                const auto action=monitor.update(sample);
                const uint64_t current=monitor.current();
                // A pending notice is dropped once the disc is gone or replaced by a game; an Error sample keeps it.
                if(sample.kind==nx::DiscSample::Empty || sample.kind==nx::DiscSample::Game) discUnsupportedNotice=false;
                if(action==nx::DiscAction::Unsupported) discUnsupportedNotice=true;
                else if(action!=nx::DiscAction::None) {
                    gameInfo update{};update.titleId=discTileId;update.appType=disc.appType;update.launchId=current;
                    if(current) {
                        if(sample.platform==nx::DiscPlatform::Wii) {
                            update.name="Wii Game Disc";
                        } else {
                            update.name="Wii U Disc";update.gamePath=nx::bounded(disc.path,sizeof(disc.path));
                            uint8_t *xml=nullptr;uint32_t size=0;
                            if(FSUtils::LoadFileToMem(("fs:"+update.gamePath+"/meta/meta.xml").c_str(),&xml,&size,1024*1024)>0) {
                                auto name=nx::xmlTitle(std::string(reinterpret_cast<char*>(xml),size));
                                if(!name.empty()) update.name=name;
                            }
                            free(xml);
                        }
                    }
                    std::lock_guard<std::mutex> guard(pendingMutex);pending.push_back(update);
                }
                for(unsigned waited=0;waited<monitor.pollDelayMs() && !stopLoader;waited+=100) OSSleepTicks(OSMillisecondsToTicks(100));
            }
        });
    }
    std::vector<gameInfo> incoming;
    { std::lock_guard<std::mutex> guard(pendingMutex); incoming.swap(pending); }
    for(auto &info:incoming) titleChanged(&info);
    // Observe completion before draining again next frame; never discard a final queued record.
    bool finished=loaderDone;
    if(finished && !loaded) {
        DEBUG_FUNCTION_LINE("Metadata worker completed: %u published titles",unsigned(tiles.size()));
        viewDirty=true;
    }
    loaded=finished;loadFailed=loaderFailed;
    if(viewDirty) rebuildView();
    if(launching && frame-launchFrame>600) { launching=false; showMessage("The launch did not complete. Try again or restart WiiUNX2."); }
    // Show the Wii disc notice once, and only when no other message or launch is active; otherwise keep it pending.
    if(!modal && !launching && discUnsupportedNotice.exchange(false)) showMessage("This disc type cannot be launched from WiiUNX2.");
    // Keep the selection near the second card, with the next card peeking at the edge.
    float target=dragging?scroll:nx::scrollTarget(selected);
    scroll+=(target-scroll)*.22f;
    if(std::fabs(target-scroll)<.1f) scroll=target;
    loadVisibleIcons(target);
    if(frame%600==0) {
        size_t resident=0;for(const auto &tile:tiles) if(tile.image) ++resident;
        DEBUG_FUNCTION_LINE("Menu alive: titles=%u resident-icons=%u metadata-ready=%u",unsigned(tiles.size()),unsigned(resident),unsigned(loaded));
        DEBUG_FUNCTION_LINE("Last 600 menu frames: %llu ms",(unsigned long long)OSTicksToMilliseconds(OSGetSystemTime()-heartbeatTicks));
        heartbeatTicks=OSGetSystemTime();
    }
    if(frame%60==1) {
        BOOL online=FALSE;
        connected=acReady && NNResult_IsSuccess(ACIsApplicationConnected(&online)) && online;
        OSCalendarTime now; OSTicksToCalendarTime(OSGetTime(),&now);
        clockText.setTextf("%d:%02d %s",now.tm_hour%12 ? now.tm_hour%12 : 12,now.tm_min,now.tm_hour>=12 ? "PM":"AM");
    }
    std::string currentTitle=visible.empty() ? (!query.empty() ? "No matching software" : loadFailed ? "Software list unavailable" : loaded ? "No installed software found":"Loading software...") : selectedTile().name;
    if(currentTitle!=lastTitle) { title.setText(currentTitle.c_str());lastTitle=currentTitle; }
    std::string currentLabel=focusRow==1 ? dockNames[dockIndex]:"";
    if(currentLabel!=lastLabel) { label.setText(currentLabel.c_str());lastLabel=currentLabel; }
}
void MainWindow::draw(CVideo *v) {
    std::lock_guard<std::recursive_mutex> guard(tilesMutex);
    const float gradientAngle=float(frame%480)*0.75f;
    drawImage(background,v,640,360);
    auto current=std::find_if(accounts.begin(),accounts.end(),[this](const Account &a){return a.slot==activeSlot;});
    if(current!=accounts.end() && current->image) {
        profileBacking->setScale(60.f/64.f);
        drawImage(*profileBacking,v,80,75);
        profileBacking->setScale(1.f);
        current->image->setScaleX(52.f/current->data->getWidth());current->image->setScaleY(52.f/current->data->getHeight());
        current->image->setAlpha(1.f);
        drawImage(*current->image,v,80,75);
    } else {
        profile->setAlpha(1.f);
        profile->setScale(52.f/64);drawImage(*profile,v,80,75);profile->setScale(1);
    }
    if(focusRow==2) {
        ring->setAngle(gradientAngle);
        ring->setScale(68.f/84.f);
        drawImage(*ring,v,80,75);
        ring->setScale(1.f);
        ring->setAngle(0.f);
    }
    if(focusRow==2) drawText(profileName,v,128,75);
    drawText(clockText,v,1070,76);
    network->setAlpha(connected ? 1.f : .22f);
    drawImage(*network,v,1160,76);
    drawImage(batteryOutline,v,1215,76); drawImage(batteryInside,v,1215,76); drawImage(batteryTip,v,1233,76);
    if(battery>=0) {
        // VPAD reports discrete levels, not a percentage; values >=5 represent full/charging.
        float fraction=std::max(.08f,std::min(1.f,battery/5.f));
        batteryFill.setScaleX(fraction);
        drawImage(batteryFill,v,1203+12*fraction,76);
    }
    if(focusRow==0) {
        if(!visible.empty()&&selectedTile().id==discTileId&&discIcon) {
            constexpr float iconSize=30.f,gap=9.f;
            const float titleWidth=title.getTextWidth();
            const float groupWidth=titleWidth+gap+iconSize;
            discIcon->setScale(iconSize/48.f);
            drawImage(*discIcon,v,640-groupWidth/2+iconSize/2,166);
            discIcon->setScale(1.f);
            drawText(title,v,640+(iconSize+gap)/2,166);
        } else drawText(title,v,640,166);
    }
    size_t count=std::max(size_t(5),visible.size());
    for(size_t i=0;i<count;++i) {
        float x=nx::tileFirstX+i*float(nx::tileStep)-scroll;
        if(x < nx::cullLeft || x > nx::cullRight) continue;
        drawImage(*tileShadow,v,x,330);
        if(i==selected && focusRow==0 && !visible.empty()) {
            const float phase=nx::selectionGradientPhase(frame);
            const size_t current=size_t(phase);
            const size_t next=(current+1)%tileFocusFrames.size();
            const float blend=phase-float(current);
            constexpr float focusFrameScale=280.f/288.f;
            tileFocusFrames[current]->setScale(focusFrameScale);
            tileFocusFrames[current]->setAlpha(1.f);
            drawImage(*tileFocusFrames[current],v,x,330);
            tileFocusFrames[current]->setScale(1.f);
            if(blend>0.f) {
                tileFocusFrames[next]->setScale(focusFrameScale);
                tileFocusFrames[next]->setAlpha(blend);
                drawImage(*tileFocusFrames[next],v,x,330);
                tileFocusFrames[next]->setAlpha(1.f);
                tileFocusFrames[next]->setScale(1.f);
            }
        }
        if(i<visible.size() && tiles[visible[i]].image) drawImage(*tiles[visible[i]].image,v,x,330);
        else drawImage(*tileBlank,v,x,330);
    }
    drawImage(*dockSurface,v,640,560);
    for(int i=0;i<int(dock.size());++i) {
        float x=nx::dockFirst+i*nx::dockStep;
        if(focusRow==1 && i==dockIndex) {
            ring->setAngle(gradientAngle);
            drawImage(*ring,v,x,560);
            ring->setAngle(0.f);
        }
        drawImage(*dock[i],v,x,560);
    }
    drawText(label,v,nx::dockFirst+dockIndex*nx::dockStep,626);
    drawFooter(v);
    drawText(libraryStatus,v,640,487);
    drawText(brand,v,95,683);
    if(accountsOpen) drawAccounts(v);
    if(searchOpen) drawSearch(v);
    if(modal) {
        drawImage(modalShade,v,640,360); drawImage(*modalPanel,v,640,360);
        if(updateBusy) {
            std::string status;
            const auto stage=updateProgressStage.load();
            if(updateCancel.load()) status="Cancellation requested. Waiting for a safe stopping point.";
            else switch(stage) {
                case nx::UpdateProgressStage::Checking:
                    status="Checking for updates over verified HTTPS.\nPlease keep the console powered on.";
                    break;
                case nx::UpdateProgressStage::Preparing:
                    status="Preparing the verified update download.\nThe installed menu has not been changed.";
                    break;
                case nx::UpdateProgressStage::DownloadingRpx: {
                    const uint32_t received=updateBytesReceived.load(),total=updateBytesTotal.load();
                    const double fraction=total?std::min(1.0,double(received)/double(total)):0.0;
                    char text[128];
                    std::snprintf(text,sizeof(text),"Downloading update: %llu%%  (%.1f / %.1f MiB)",
                                  static_cast<unsigned long long>(fraction*100.0),
                                  double(received)/(1024.0*1024.0),double(total)/(1024.0*1024.0));
                    status=text;
                    drawImage(updateProgressTrack,v,640,430);
                    updateProgressFill.setScaleX(float(fraction));
                    drawImage(updateProgressFill,v,330.f+310.f*float(fraction),430);
                    updateProgressFill.setScaleX(1.f);
                    break;
                }
                case nx::UpdateProgressStage::DownloadingChecksum:
                    status="Downloading and checking the release checksum...";
                    break;
                case nx::UpdateProgressStage::Verifying:
                    status="Verifying update files before changing the installed menu...";
                    break;
                case nx::UpdateProgressStage::Installing:
                    status="Creating a verified backup and installing.\nDo not power off or remove the SD card.\nThis step has no percentage.";
                    break;
            }
            message.setText(status.c_str());
        }
        drawText(message,v,640,335);
        drawPrompts(v,modalPrompts,1040,480);
    }
}
void MainWindow::showMessage(const std::string &text,bool power) {
    message.setFontSize(26);message.setText(text.c_str()); modal=true; powerConfirm=power;
    modalAction=ModalAction::None;
    requestSound(MenuSound::Click);
    modalPrompts[0].caption->setText(power?"Cancel":"Back");
    modalPrompts[1].caption->setText(power?"Power off":"OK");
}
void MainWindow::showActionMessage(const std::string &text,const char *action,ModalAction next) {
    message.setFontSize(26);message.setText(text.c_str());modal=true;powerConfirm=false;modalAction=next;
    requestSound(MenuSound::Click);
    modalPrompts[0].caption->setText("Back");modalPrompts[1].caption->setText(action);
}
void MainWindow::showUpdateProgress(const std::string &text) {
    message.setFontSize(26);message.setText(text.c_str());modal=true;powerConfirm=false;modalAction=ModalAction::None;
    requestSound(MenuSound::Click);
    modalPrompts[0].caption->setText("Cancel");modalPrompts[1].caption->setText("Please wait");
}
void MainWindow::updateProgressCallback(void *context,nx::UpdateProgressStage stage,uint64_t received,uint64_t total) {
    auto *window=static_cast<MainWindow *>(context);
    window->updateBytesReceived.store(static_cast<uint32_t>(received));
    window->updateBytesTotal.store(static_cast<uint32_t>(total));
    window->updateProgressStage.store(stage);
}
void MainWindow::startUpdateCheck() {
    if(updateBusy) {
        showUpdateProgress("An update operation is still in progress. Wait for it to finish before checking again.");
        return;
    }
    updateCancel=false;updateProgressStage=nx::UpdateProgressStage::Checking;updateBusy=true;
    showUpdateProgress("Checking the latest WiiUNX2 release over verified HTTPS.\nPlease wait; keep the console powered on.");
    try {
        updateCheck=std::async(std::launch::async,[this] {return nx::checkLatestUpdate(WIIUNX2_VERSION,&updateCancel);});
    } catch(const std::exception &) {
        updateBusy=false;showMessage("The update check could not be started. The installed menu was not changed.");
    }
}
void MainWindow::startUpdateInstall() {
    if(updateBusy) {
        showUpdateProgress("An update operation is still in progress. Wait for it to finish before installing again.");
        return;
    }
    if(pendingUpdateVersion.empty()) {
        showMessage("No verified update is ready to install. Check for updates again.");
        return;
    }
    updateCancel=false;updateBytesReceived=0;updateBytesTotal=0;
    updateProgressStage=nx::UpdateProgressStage::Preparing;updateBusy=true;
    showUpdateProgress("Downloading and verifying WiiUNX2 v"+pendingUpdateVersion+".\nThis may take a few minutes. Keep the console powered on; the existing menu is not replaced until verification and backup complete.");
    const auto selection=pendingUpdate;
    const auto version=pendingUpdateVersion;
    const auto targetPath=pendingUpdateTargetPath;
    try {
        updateInstall=std::async(std::launch::async,[this,selection,version,targetPath] {
            return nx::installUpdate(WIIUNX2_VERSION,version,selection,targetPath,&updateCancel,
                                     updateProgressCallback,this);
        });
    } catch(const std::exception &) {
        updateBusy=false;showMessage("The updater could not be started. The installed menu was not changed.");
    }
}
void MainWindow::move(int delta) {
    userNavigated=true;
    restore.titleId=0;
    if(focusRow==1) dockIndex=nx::moveSelection(dockIndex,delta,dock.size());
    else if(!visible.empty()) selected=nx::moveSelection(selected,delta,visible.size());
}
void MainWindow::requestSound(MenuSound sound) {
    if(int(sound)>int(pendingSound)) pendingSound=sound;
}
void MainWindow::update(GuiController *c) {
    auto before=std::make_tuple(focusRow,selected,dockIndex,accountIndex,keyboardIndex);
    pendingSound=MenuSound::None;
    handleInput(c);
    if(before!=std::make_tuple(focusRow,selected,dockIndex,accountIndex,keyboardIndex)) requestSound(MenuSound::Navigate);
    if(audio && pendingSound!=MenuSound::None) audio->playEffect(pendingSound);
}
void MainWindow::handleInput(GuiController *c) {
    using T=GuiTrigger;
    std::lock_guard<std::recursive_mutex> guard(tilesMutex);
    if(c->chanIdx==0) battery=static_cast<VPadController *>(c)->batteryLevel();
    if(launching || shutdownFrame) return;
    uint32_t pressed=c->data.buttons_d;
    if(modal) {
        bool click=(c->chanIdx==0 && c->data.touched && !c->lastData.touched)||(c->chanIdx!=0 && c->data.validPointer && (pressed&T::BUTTON_A));
        if(updateBusy) {
            const bool cancelRequested=(pressed&T::BUTTON_B)||
                (click&&hitPrompt(modalPrompts,c->data.x+640,360-c->data.y,480)==0);
            if(cancelRequested&&!updateCancel.exchange(true)) {
                message.setText("Cancellation requested. The updater may finish a protected file operation before it can stop. Keep the console powered on until a result appears.");
                modalPrompts[0].caption->setText("Cancelling");
                requestSound(MenuSound::Back);
            }
            return;
        }
        if(pressed&T::BUTTON_B) { modal=false;modalAction=ModalAction::None;requestSound(MenuSound::Back);return; }
        if(click) {int i=hitPrompt(modalPrompts,c->data.x+640,360-c->data.y,480);if(i==0){modal=false;modalAction=ModalAction::None;requestSound(MenuSound::Back);}else if(i==1) confirmModal();}
        else if(pressed&T::BUTTON_A) confirmModal();
        return;
    }
    if(searchOpen) {
        if(pressed&(T::BUTTON_B|T::BUTTON_PLUS)) { searchOpen=false;requestSound(MenuSound::Back); return; }
        if(pressed&T::BUTTON_X) {searchKey(37);return;}
        unsigned channel=std::min(4,std::max(0,c->chanIdx));
        uint32_t directions=c->data.buttons_h & (T::BUTTON_LEFT|T::BUTTON_RIGHT|T::BUTTON_UP|T::BUTTON_DOWN|
                                                  T::STICK_L_LEFT|T::STICK_L_RIGHT|T::STICK_L_UP|T::STICK_L_DOWN);
        if(directions!=keyboardHeldDirection[channel]) {
            keyboardRepeat[channel]=0;
            keyboardHeldDirection[channel]=directions;
        }
        if(directions && nx::shouldRepeat(++keyboardRepeat[channel])) {
            int dx=(directions&(T::BUTTON_LEFT|T::STICK_L_LEFT))?-1:
                   (directions&(T::BUTTON_RIGHT|T::STICK_L_RIGHT))?1:0;
            int dy=(directions&(T::BUTTON_UP|T::STICK_L_UP))?-1:
                   (directions&(T::BUTTON_DOWN|T::STICK_L_DOWN))?1:0;
            keyboardIndex=nx::keyMove(keyboardIndex,dx,dy);
        }
        bool click=(c->chanIdx==0 && c->data.touched && !c->lastData.touched) || (c->chanIdx!=0 && c->data.validPointer && (pressed&T::BUTTON_A));
        if(click) {int key=nx::keyHit(c->data.x+640,360-c->data.y);if(key>=0) {keyboardIndex=key;searchKey(key);} else {int i=hitPrompt(searchPrompts,c->data.x+640,360-c->data.y,584);if(i==1) searchKey(37);else if(i==2||i==3){searchOpen=false;requestSound(MenuSound::Back);}else if(i==4) searchKey(keyboardIndex);}}
        else if(pressed&T::BUTTON_A) searchKey(keyboardIndex);
        return;
    }
    if(!accountsOpen && (pressed&T::BUTTON_Y)) {searchOpen=true;requestSound(MenuSound::Click);searchQuery.setText(query.empty()?"Type a title...":query.c_str());return;}
    if(!accountsOpen && (pressed&T::BUTTON_MINUS)) {sortMode=(sortMode+1)%4;requestSound(MenuSound::Click);rebuildView();return;}
    if(accountsOpen) {
        if(pressed&T::BUTTON_B) {
            if(accountPromptRequired) continueWithActiveAccount();
            else { accountsOpen=false;requestSound(MenuSound::Back); }
            return;
        }
        if(pressed&(T::BUTTON_LEFT|T::STICK_L_LEFT)) accountIndex=nx::moveSelection(accountIndex,-1,accounts.size());
        if(pressed&(T::BUTTON_RIGHT|T::STICK_L_RIGHT)) accountIndex=nx::moveSelection(accountIndex,1,accounts.size());
        if(pressed&T::BUTTON_L) accountIndex=nx::moveSelection(accountIndex,-6,accounts.size());
        if(pressed&T::BUTTON_R) accountIndex=nx::moveSelection(accountIndex,6,accounts.size());
        bool click=(c->chanIdx==0 && c->data.touched && !c->lastData.touched) || (c->chanIdx!=0 && c->data.validPointer && (pressed&T::BUTTON_A));
        if(click) {
            float x=c->data.x+640,y=360-c->data.y;
            int i=nx::hitAccount(x,y,accountIndex/6,accounts.size());
            if(i>=0) { if(i==accountIndex) selectAccount(); else accountIndex=i; }
            else {int action=hitPrompt(accountPrompts,x,y,516);if(action==0) accountIndex=nx::moveSelection(accountIndex,-6,accounts.size());else if(action==1) accountIndex=nx::moveSelection(accountIndex,6,accounts.size());else if(action==2){if(accountPromptRequired) continueWithActiveAccount();else {accountsOpen=false;requestSound(MenuSound::Back);}}else if(action==3) selectAccount();}
        } else if(pressed&T::BUTTON_A) selectAccount();
        return;
    }
    if(pressed&T::BUTTON_X) { openAccounts(); return; }
    // Touch and Wii Remote pointing use the same centered 1280x720 coordinates.
    if(c->chanIdx==0) {
        float x=c->data.x+640,y=360-c->data.y;
        if(c->data.touched && !c->lastData.touched && y>=202 && y<=458) {
            userNavigated=true;dragging=true;dragMoved=false;dragStartX=x;dragStartScroll=scroll;return;
        }
        if(dragging && c->data.touched) {
            float distance=x-dragStartX;
            if(std::fabs(distance)>14) dragMoved=true;
            if(dragMoved) {
                scroll=std::max(0.f,std::min(nx::scrollTarget(visible.empty()?0:visible.size()-1),dragStartScroll-distance));
                if(!visible.empty()) selected=nx::moveSelection(0,int(std::lround(scroll/272.f))+2,visible.size());
                focusRow=0;
            }
            return;
        }
        if(dragging && !c->data.touched) {
            dragging=false;
            if(!dragMoved) {
                int i=nx::hitTile(dragStartX,330,dragStartScroll,visible.size());
                if(i>=0) {if(focusRow==0 && selected==size_t(i)) activate();else {selected=i;focusRow=0;}}
            }
            return;
        }
    }
    bool touch=c->chanIdx==0 && c->data.touched && !c->lastData.touched;
    bool pointer=c->chanIdx!=0 && c->data.validPointer && (pressed&T::BUTTON_A);
    if(touch || pointer) {
        userNavigated=true;
        float x=c->data.x+640,y=360-c->data.y;
        if(y>=202 && y<=458) {
            int i=nx::hitTile(x,y,scroll,visible.size());
            if(i>=0) {
                if(focusRow==0 && selected==size_t(i)) activate();
                else { selected=i; focusRow=0; }
            }
        } else if(y>=520 && y<=600) {
            int i=nx::hitDock(x,y);
            if(i>=0 && i<int(dock.size())) {
                if(focusRow==1 && dockIndex==i) activate();
                else { dockIndex=i; focusRow=1; }
            }
        } else if(y>=650 && y<=704) {
            for(size_t i=0;i<prompts.size();++i) if(x>=prompts[i].left-8 && x<=prompts[i].right+8) {footerAction(i);break;}
        } else if(x>=35 && x<125 && y>=30 && y<125) openAccounts();
        return;
    }
    uint32_t directions=c->data.buttons_h & (T::BUTTON_LEFT|T::BUTTON_RIGHT|T::STICK_L_LEFT|T::STICK_L_RIGHT);
    unsigned channel=std::min(4,std::max(0,c->chanIdx));
    if(directions!=heldDirection[channel]) { repeat[channel]=0; heldDirection[channel]=directions; }
    if(directions && nx::shouldRepeat(++repeat[channel])) {
        move(directions&(T::BUTTON_LEFT|T::STICK_L_LEFT) ? -1:1);
    }
    if(pressed & (T::BUTTON_DOWN|T::STICK_L_DOWN)) focusRow=focusRow==2 ? 0:1;
    if(pressed & (T::BUTTON_UP|T::STICK_L_UP)) focusRow=focusRow==1 ? 0:2;
    if(pressed&T::BUTTON_B) {if(focusRow!=0||!query.empty())requestSound(MenuSound::Back);focusRow=0;if(!query.empty()){query.clear();rebuildView();}}
    // Hold-to-accelerate for L/R: initial press moves four tiles, then
    // repeats after an initial delay and accelerates while held.
    uint32_t lr=c->data.buttons_h & (T::BUTTON_L|T::BUTTON_R);
    if(lr!=heldLR[channel]) { repeatLR[channel]=0; heldLR[channel]=lr; }
    if(lr && nx::shouldRepeat(++repeatLR[channel])) {
        move(lr&T::BUTTON_L ? -4:4);
    }
    if(pressed&T::BUTTON_PLUS) showOptions();
    if(pressed&T::BUTTON_A) activate();
}
void MainWindow::activate() {
    requestSound(MenuSound::Click);
    if(focusRow==2) { openAccounts(); return; }
    if(focusRow==0) { if(!visible.empty()) launch(selectedTile().id); return; }
    switch(dockIndex) {
    case 4: launch(_SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_MII_MAKER)); return;
    case 5: { SysAppSettingsArgs args{}; args.jumpTo=SYS_SETTINGS_JUMP_TO_DATA_MANAGEMENT; _SYSLaunchSettings(&args); return; }
    case 6: if(SYSSwitchToSyncControllerOnHBM()<0) showMessage("Unable to open controller pairing."); return;
    case 7: { SysAppSettingsArgs args{}; _SYSLaunchSettings(&args); return; }
    case 8: showMessage("Power off the Wii U?",true); return;
    }
    MCPAppType type=dockIndex==0 ? MCP_APP_TYPE_FRIEND_LIST : dockIndex==1 ? MCP_APP_TYPE_BROWSER : dockIndex==2 ? MCP_APP_TYPE_DOWNLOAD_MANAGEMENT : MCP_APP_TYPE_ESHOP;
    for(auto &t:tiles) if(t.type==type) { launch(t.id); return; }
    showMessage(std::string(dockNames[dockIndex])+" is not installed or could not be found.");
}
namespace {
// Opt-in only. Called before the launch call, which may replace this process.
void recordLaunch(const char *stage,uint64_t id,int appType,int patchResult,const char *path) {
    const std::string marker=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),WiiUNX2UserDataMigrationComplete(),
        "launch-log.enable","wiiunx2-launch-log.enable");
    const std::string log=nx::userDataPath(WiiUNX2UserDataDirectoryReady(),WiiUNX2UserDataMigrationComplete(),
        "launch.log","wiiunx2-launch.log");
    if(!nx::launchLogEnabled(marker)) return;
    if(!nx::appendLaunchLog(log,nx::formatLaunchRecord(id,stage,appType,patchResult,path))) DEBUG_FUNCTION_LINE("Launch log append failed");
}
}
void MainWindow::launch(uint64_t id) {
    if(nx::isHomebrew(id)) {
        auto tile=std::find_if(tiles.begin(),tiles.end(),[id](const auto &t){return t.id==id;});
        if(tile==tiles.end()) return;
        stopTitleWorkers();
        static bool ready=false;
        auto result=ready?RPX_LOADER_RESULT_SUCCESS:RPXLoader_InitLibrary();
        if(result==RPX_LOADER_RESULT_SUCCESS) {ready=true;result=RPXLoader_LaunchHomebrew(tile->path.c_str());}
        DEBUG_FUNCTION_LINE("Direct Aroma launch result %d",int(result));
        if(result!=RPX_LOADER_RESULT_SUCCESS) {showMessage(std::string("Aroma homebrew launch failed: ")+RPXLoader_GetStatusStr(result));return;}
        markPlayed(id);launching=true;launchFrame=frame;return;
    }
    if(id==discTileId) {
        MCPTitleListType disc{};const auto sample=readDisc(disc);
        if(sample.kind!=nx::DiscSample::Game) {showMessage("Insert a supported, readable game disc and try again.");return;}
        if(sample.platform==nx::DiscPlatform::Wii) {
            markPlayed(disc.titleId);launching=true;launchFrame=frame;
            stopTitleWorkers();
            recordLaunch("wii-disc",disc.titleId,disc.appType,nx::kLaunchNotCalled,disc.path);
            SYSLaunchTitle(disc.titleId);
            recordLaunch("wii-disc-returned",disc.titleId,disc.appType,nx::kLaunchNotCalled,disc.path);
            return;
        }
        int32_t patchResult=ACPAssignTitlePatch(&disc);markPlayed(disc.titleId);launching=true;launchFrame=frame;
        stopTitleWorkers();
        recordLaunch("disc",disc.titleId,disc.appType,patchResult,disc.path);
        _SYSLaunchTitleByPathFromLauncher(disc.path,0);return;
    }
    if((id>>32)==0x0005000f) {
        // Run the plugin's preparation hook before launching its /custom/ path.
        MCPTitleListType homebrew{};homebrew.titleId=id;
        std::snprintf(homebrew.path,sizeof(homebrew.path),"/custom/%016llX",(unsigned long long)id);
        // Use the statically imported function so WUPS can replace this call.
        int result=ACPCheckTitleLaunchByTitleListTypeEx(&homebrew,0);
        DEBUG_FUNCTION_LINE("Aroma prepare %016llX result %d",(unsigned long long)id,result);
        if(result<0) {showMessage("Homebrew could not prepare. Enable Aroma's Homebrew on Menu plugin.");return;}
        markPlayed(id);launching=true;launchFrame=frame;
        stopTitleWorkers();
        recordLaunch("homebrew-custom",id,homebrew.appType,nx::kLaunchNotCalled,homebrew.path);
        _SYSLaunchTitleByPathFromLauncher(homebrew.path,0);return;
    }
    DEBUG_FUNCTION_LINE("Launch requested %016llX",(unsigned long long)id);
    if(isMenu(id)) return;
    MCPTitleListType info{};
    int handle=MCP_Open();
    if(handle<0) { showMessage("Could not open the title service."); return; }
    auto error=MCP_GetTitleInfo(handle,id,&info); MCP_Close(handle);
    if(error!=0) { showMessage("This title is unavailable. Reconnect its storage and restart WiiUNX2."); return; }
    int32_t result=0;
    if(nx::isTvii(id)) {
        if(_SYSSwitchTo(SYSAPP_PFID_TVII)<0) showMessage("Nintendo TVii could not open. Check TVii installation and Project Rose settings.");
        return;
    }
    switch(info.appType) {
    case MCP_APP_TYPE_BROWSER: result=SYSSwitchToBrowser(nullptr); break;
    case MCP_APP_TYPE_ESHOP: result=SYSSwitchToEShop(nullptr); break;
    case MCP_APP_TYPE_DOWNLOAD_MANAGEMENT: result=_SYSSwitchTo(SYSAPP_PFID_DOWNLOAD_MANAGEMENT); break;
    case MCP_APP_TYPE_FRIEND_LIST: result=_SYSSwitchTo(SYSAPP_PFID_FRIENDLIST); break;
    case MCP_APP_TYPE_MIIVERSE: result=_SYSSwitchTo(SYSAPP_PFID_MIIVERSE); break;
    default: {
        int32_t patchResult=ACPAssignTitlePatch(&info);
        DEBUG_FUNCTION_LINE("Assign title patch %016llX result %d",(unsigned long long)id,int(patchResult));
        markPlayed(id);
        launching=true;
        launchFrame=frame;
        stopTitleWorkers();
        recordLaunch("regular",id,info.appType,patchResult,info.path);
        SYSLaunchTitle(id);
        recordLaunch("regular-returned",id,info.appType,patchResult,info.path);
        return;
    }
    }
    if(result<0) showMessage("The system could not open this application.");
    return;
}

MainWindow::ViewState MainWindow::saveView() const {
    return {restore.titleId&&!userNavigated?restore.titleId:selectedTitleId(),0,0,sortMode,{}};
}
uint64_t MainWindow::selectedTitleId() const {
    return visible.empty()||selected>=visible.size()||visible[selected]>=tiles.size()?0:selectedTile().id;
}
void MainWindow::markPlayed(uint64_t id) {
    recent.erase(std::remove(recent.begin(),recent.end(),id),recent.end());
    recent.insert(recent.begin(),id);if(recent.size()>512) recent.resize(512);
    localDays[id]=uint32_t(OSTicksToSeconds(OSGetTime())/86400);
    const std::string path=nx::recentHistoryPath(WiiUNX2UserDataDirectoryReady(),
        WiiUNX2UserDataMigrationComplete(),unsigned(activeSlot));
    if(!nx::saveHistory(path,recent,localDays)) DEBUG_FUNCTION_LINE("Recent history save failed; previous file retained");
    viewDirty=true;
}
void MainWindow::loadVisibleIcons(float target) {
    // Keep the on-screen cards plus a directional prefetch window; the plan is bounded (see IconPlan.h).
    const nx::IconPlan plan=nx::planIcons(int(visible.size()),scroll,target);
    std::vector<bool> needed(tiles.size(),false);
    for(int pos:plan.keep) needed[visible[pos]]=true;
    for(size_t i=0;i<tiles.size();++i) if(!needed[i]) {
        tiles[i].image.reset();tiles[i].data.reset();tiles[i].attempted=false;
    }
    // File I/O and validation happen off-thread; only the renderer imports a texture.
    if(iconRead.valid() && iconRead.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto result=iconRead.get();
        for(size_t i=0;i<tiles.size();++i) if(tiles[i].id==result.id && needed[i]) {
            auto &tile=tiles[i];tile.attempted=true;
            if(!result.bytes.empty()) {
                auto data=std::make_unique<GuiImageData>(result.bytes.data(),result.bytes.size(),GX2_TEX_CLAMP_MODE_CLAMP);
                if(data->getTexture() && data->getSampler() && data->getWidth()>0 && data->getHeight()>0) {
                    data->roundCorners();
                    tile.data=std::move(data);tile.image=std::make_unique<GuiImage>(tile.data.get());
                    tile.image->setScaleX(256.f/tile.data->getWidth());tile.image->setScaleY(256.f/tile.data->getHeight());
                }
            }
            break;
        }
    }
    if(iconRead.valid()) return;
    for(int pos:plan.loadOrder) {
        auto i=visible[pos];auto &tile=tiles[i];
        if(!needed[i] || tile.attempted || tile.path.empty()) continue;
        tile.attempted=true;
        auto path=nx::isHomebrew(tile.id)?tile.path:"fs:"+tile.path+"/meta/iconTex.tga";auto id=tile.id;
        iconRead=std::async(std::launch::async,[path,id] {
            IconRead result;result.id=id;
            if(nx::isHomebrew(id)) {result.bytes=nx::homebrewIcon(path);return result;}
            uint8_t *bytes=nullptr;uint32_t size=0;
            if(FSUtils::LoadFileToMem(path.c_str(),&bytes,&size,1024*1024)>0 && nx::validIconTga(bytes,size))
                result.bytes.assign(bytes,bytes+size);
            free(bytes);return result;
        });
        break;
    }
}
void MainWindow::refreshAccounts() {
    accounts.clear();
    if(!actReady) return;
    activeSlot=nn::act::GetSlotNo();
    for(uint8_t slot=1;slot<=12;++slot) {
        if(!nn::act::IsSlotOccupied(slot)) continue;
        Account account;account.slot=slot;
        int16_t name[nn::act::MiiNameSize]{};
        if(nn::act::GetMiiNameEx(name,slot).IsSuccess()) account.name=nx::utf16Name(name,nn::act::MiiNameSize);
        if(account.name.empty()) account.name=L"User "+std::to_wstring(slot);
        account.caption=std::make_unique<GuiText>(account.name.c_str(),20,glm::vec4(.12f,.12f,.12f,1));
        account.caption->setMaxWidth(138,GuiText::DOTTED);
        constexpr size_t capacity=65554;
        std::unique_ptr<void,decltype(&free)> buffer(memalign(64,capacity),&free);
        size_t actual=0;
        if(buffer && nn::act::GetMiiImageEx(&actual,buffer.get(),capacity,0,slot).IsSuccess() && actual>0 && actual<=capacity) {
            auto data=std::make_unique<GuiImageData>(static_cast<uint8_t *>(buffer.get()),actual);
            if(data->getTexture() && data->getSampler()) {
                data->roundCorners(.5f);
                account.data=std::move(data);account.image=std::make_unique<GuiImage>(account.data.get());
            }
        }
        if(slot==activeSlot) {
            accountIndex=accounts.size();
            profileName.setText(account.name.c_str());
        }
        accounts.push_back(std::move(account));
    }
    accountIndex=nx::moveSelection(accountIndex,0,accounts.size());
}
void MainWindow::openAccounts() {
    if(!actReady || accounts.empty()) {showMessage("No Wii U users could be read. Open the original Wii U Menu to configure an account.");return;}
    accountPromptRequired=false;accountPrompts[2].caption->setText("Back");
    accountsOpen=true;requestSound(MenuSound::Click);
}
void MainWindow::selectAccount() {
    requestSound(MenuSound::Click);
    if(accounts.empty()) return;
    auto &account=accounts[accountIndex];
    if(account.slot==activeSlot) {
        rememberAccountChoiceThisBoot();accountPromptRequired=false;
        accountPrompts[2].caption->setText("Back");accountsOpen=false;return;
    }
    // Same load operation used by Aroma's AutobootModule. Let ACT enforce sign-in requirements.
    auto result=nn::act::LoadConsoleAccount(account.slot,0,nullptr,false);
    uint8_t actual=nn::act::GetSlotNo();
    if(result.IsFailure() || actual!=account.slot) {
        DEBUG_FUNCTION_LINE("Account load slot %u failed/unchanged (current %u)",unsigned(account.slot),unsigned(actual));
        showMessage("The Wii U could not switch to this user. The account may require sign-in through the original Wii U Menu.");
        return;
    }
    activeSlot=actual;profileName.setText(account.name.c_str());accountsOpen=false;focusRow=2;
    rememberAccountChoiceThisBoot();accountPromptRequired=false;accountPrompts[2].caption->setText("Back");
    recent.clear();localDays.clear();nativeDays.clear();recentLoaded=false;viewDirty=true;
    DEBUG_FUNCTION_LINE("Active account slot changed to %u",unsigned(activeSlot));
}
void MainWindow::continueWithActiveAccount() {
    auto current=std::find_if(accounts.begin(),accounts.end(),[this](const Account &account) {
        return account.slot==activeSlot;
    });
    if(current==accounts.end()) {
        showMessage("The active Wii U user could not be read. Select an available account.");
        return;
    }
    accountIndex=int(current-accounts.begin());
    selectAccount();
}
void MainWindow::stopTitleWorkers() {
    stopLoader=true;
    if(loader.joinable()) loader.join();
    if(iconRead.valid()) iconRead.wait();
    if(historyRead.valid()) historyRead.wait();
}
void MainWindow::drawAccounts(CVideo *v) {
    drawImage(modalShade,v,640,360);drawImage(*accountPanel,v,640,360);
    drawText(accountHeading,v,640,220);
    const int first=(accountIndex/6)*6;
    for(int i=first;i<std::min(first+6,int(accounts.size()));++i) {
        auto &account=accounts[i];float x=230+(i-first)*164;
        if(i==accountIndex) {
            const float phase=nx::selectionGradientPhase(frame);
            const size_t current=size_t(phase);
            const size_t next=(current+1)%tileFocusFrames.size();
            const float blend=phase-float(current);
            tileFocusFrames[current]->setScaleX(156.f/288.f);
            tileFocusFrames[current]->setScaleY(172.f/288.f);
            tileFocusFrames[current]->setAlpha(1.f);
            drawImage(*tileFocusFrames[current],v,x,360);
            tileFocusFrames[current]->setScaleX(1.f);
            tileFocusFrames[current]->setScaleY(1.f);
            if(blend>0.f) {
                tileFocusFrames[next]->setScaleX(156.f/288.f);
                tileFocusFrames[next]->setScaleY(172.f/288.f);
                tileFocusFrames[next]->setAlpha(blend);
                drawImage(*tileFocusFrames[next],v,x,360);
                tileFocusFrames[next]->setAlpha(1.f);
                tileFocusFrames[next]->setScaleX(1.f);
                tileFocusFrames[next]->setScaleY(1.f);
            }
        }
        drawImage(*accountCard,v,x,360);
        if(account.image) {
            account.image->setScaleX(88.f/account.data->getWidth());account.image->setScaleY(88.f/account.data->getHeight());
            drawImage(*account.image,v,x,333);
        } else {profile->setScale(88.f/64);drawImage(*profile,v,x,333);profile->setScale(1); }
        drawText(*account.caption,v,x,411);
    }
    // These strings have stable storage across both screen draws.
    drawPrompts(v,accountPrompts,1120,516);
}

void MainWindow::rebuildView() {
    if(!recentLoaded) {
        recentLoaded=true;
        const std::string path=nx::recentHistoryPath(WiiUNX2UserDataDirectoryReady(),
            WiiUNX2UserDataMigrationComplete(),unsigned(activeSlot));
        FILE *file=nx::openHistory(path);
        if(file) {unsigned long long id;unsigned day;while(recent.size()<512 && std::fscanf(file,"%llx %u",&id,&day)==2) {recent.push_back(id);localDays[id]=day;}std::fclose(file);}
    }
    const uint64_t wanted=userNavigated?0:restore.titleId;
    uint64_t id=wanted ? wanted:selectedTitleId();
    visible.clear();
    for(size_t i=0;i<tiles.size();++i) if(nx::matches(tiles[i].name,query)) visible.push_back(i);
    // Cache ranks once per rebuild, not a linear history scan per comparison.
    std::unordered_map<uint64_t,size_t> ranks;
    for(size_t i=0;i<recent.size();++i) ranks.emplace(recent[i],i);
    auto dayFor=[this](uint64_t id) {
        auto native=nativeDays.find(id),local=localDays.find(id);
        return std::max(native==nativeDays.end()?0u:native->second,local==localDays.end()?0u:local->second);
    };
    if(sortMode) std::stable_sort(visible.begin(),visible.end(),[this,&ranks,&dayFor](size_t a,size_t b) {
        if(sortMode==3) {
            const bool aIsDisc=tiles[a].id==discTileId,bIsDisc=tiles[b].id==discTileId;
            if(nx::recentlyInsertedDiscBefore(aIsDisc,bIsDisc,discIsInserted)) return true;
            if(nx::recentlyInsertedDiscBefore(bIsDisc,aIsDisc,discIsInserted)) return false;
            auto aId=aIsDisc?discRecentId:tiles[a].id,bId=bIsDisc?discRecentId:tiles[b].id;
            auto aDay=dayFor(aId),bDay=dayFor(bId);
            if(aDay!=bDay) return aDay>bDay;
            auto left=ranks.find(aId),right=ranks.find(bId);
            size_t aRank=left==ranks.end()?recent.size():left->second,bRank=right==ranks.end()?recent.size():right->second;
            if(aRank!=bRank) return aRank<bRank;
            return nx::titleBefore(tiles[a].name,tiles[a].id,tiles[b].name,tiles[b].id,1);
        }
        return nx::titleBefore(tiles[a].name,tiles[a].id,tiles[b].name,tiles[b].id,sortMode);
    });
    const auto tileId=[this](size_t index){return tiles[index].id;};
    if(wanted && !userNavigated) {
        // Keep the saved title selected; while metadata is still arriving, retry on each rebuild.
        auto restored=nx::restoreSelection(visible,wanted,loaded,tileId);
        selected=restored.index;
        if(restored.settled) restore.titleId=0;
    } else {
        selected=nx::selectionAfterRefresh(visible,id,userNavigated,tileId);
        restore.titleId=0;
    }
    if(!dragging) scroll=nx::scrollTarget(selected);
    viewDirty=false;
    libraryStatus.setTextf("%u software   /   %s%s",unsigned(visible.size()),nx::sortName(sortMode),!loaded ? "   /   Loading...":query.empty()?"":"   /   Filter active (B: clear)");
}
void MainWindow::searchKey(int key) {
    requestSound(MenuSound::Click);
    if(key<36 && query.size()<48) query+=keys[key];
    else if(key==36 && query.size()<48) query+=' ';
    else if(key==37 && !query.empty()) query.pop_back();
    else if(key==38) query.clear();
    else if(key==39) searchOpen=false;
    searchQuery.setText(query.empty()?"Type a title...":query.c_str());rebuildView();
}
void MainWindow::drawSearch(CVideo *v) {
    drawImage(modalShade,v,640,360);drawImage(*searchPanel,v,640,360);
    drawText(searchHeading,v,640,160);drawText(searchQuery,v,640,238);
    for(int i=0;i<40;++i) {
        float x=222+(i%10)*92,y=327+(i/10)*64;
        if(i==keyboardIndex) drawImage(*keyFocus,v,x,y);
        drawImage(*keyCard,v,x,y);drawText(*keyLabels[i],v,x,y);
    }
    drawPrompts(v,searchPrompts,1120,584);
}

void MainWindow::showOptions() {
    requestSound(MenuSound::Click);
    if(focusRow==0 && !visible.empty()) {
        char id[32];snprintf(id,sizeof(id),"%016llX",(unsigned long long)selectedTile().id);
        showMessage(selectedTile().name+"\n\nTitle ID: "+id+"\n"+selectedTile().path);
    } else {
        showActionMessage("WiiUNX2 "+std::string(WIIUNX2_VERSION)+
                          "\n\nCONTROLS\n\nMove: D-pad / left stick\nSelect / open: A    Back: B\n"
                          "Skip 4 titles: L / R\nAccounts: X    Search: Y\nSort: -    Options: +\n"
                          "Touch: Select once; tap again to open",
                          "Check updates",ModalAction::CheckUpdate);
        message.setFontSize(20);
    }
}

void MainWindow::drawFooter(CVideo *v) {drawPrompts(v,prompts,1224,674);}
void MainWindow::drawPrompts(CVideo *v,std::vector<FooterPrompt> &items,float right,float y) {
    for(int i=int(items.size())-1;i>=0;--i) {
        auto &prompt=items[i];float badgeWidth=prompt.badge->getWidth();
        float width=badgeWidth+9+prompt.caption->getTextWidth();
        prompt.left=right-width;prompt.right=right;
        drawImage(*prompt.badge,v,prompt.left+badgeWidth/2,y);
        drawText(*prompt.caption,v,prompt.left+badgeWidth+9,y+7);
        right=prompt.left-28;
    }
}
int MainWindow::hitPrompt(const std::vector<FooterPrompt> &items,float x,float y,float centerY) const {
    if(std::fabs(y-centerY)>25) return -1;
    for(size_t i=0;i<items.size();++i) if(x>=items[i].left-8&&x<=items[i].right+8) return i;
    return -1;
}
void MainWindow::confirmModal() {
    requestSound(MenuSound::Click);
    const ModalAction action=modalAction;
    const bool power=powerConfirm;
    modal=false;powerConfirm=false;modalAction=ModalAction::None;
    if(power) shutdownFrame=frame+12;
    else if(action==ModalAction::CheckUpdate) startUpdateCheck();
    else if(action==ModalAction::InstallUpdate) startUpdateInstall();
}
void MainWindow::footerAction(size_t action) {
    requestSound(MenuSound::Click);
    if(action==0) {searchOpen=true;searchQuery.setText(query.empty()?"Type a title...":query.c_str());}
    else if(action==1) {sortMode=(sortMode+1)%3;rebuildView();}
    else if(action==2) showOptions();
    else if(action==3) activate();
}
