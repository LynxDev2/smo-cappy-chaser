#include <al/Library/LiveActor/ActorMovementFunction.h>
#include <al/Library/LiveActor/ActorFlagFunction.h>
#include "al/Project/HitSensor/HitSensor.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "hook/trampoline.hpp"
#include "imgui.h"
#include "lib.hpp"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "math/seadVector.h"
#include "patches.hpp"
#include "logger/Logger.hpp"
#include "fs.h"
#include "helpers/InputHelper.h"
#include "helpers/PlayerHelper.h"
#include "imgui_nvn.h"
#include "ExceptionHandler.h"

#include <basis/seadRawPrint.h>
#include <prim/seadSafeString.h>
#include <resource/seadResourceMgr.h>
#include <filedevice/nin/seadNinSDFileDeviceNin.h>
#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <resource/seadArchiveRes.h>
#include <heap/seadHeapMgr.h>
#include <devenv/seadDebugFontMgrNvn.h>
#include <gfx/seadTextWriter.h>
#include <gfx/seadViewport.h>

#include <al/Library/File/FileLoader.h>
#include <al/Library/File/FileUtil.h>

#include <game/StageScene/StageScene.h>
#include <game/System/GameSystem.h>
#include <game/System/Application.h>
#include <game/HakoniwaSequence/HakoniwaSequence.h>
#include <game/GameData/GameDataFunction.h>

#include "prim/seadRuntimeTypeInfo.h"
#include "rs/util.hpp"
#include <al/Library/LiveActor/LiveActorUtil.h>
#include <al/Library/Controller/JoyPadUtil.h>

bool isCatch = false;

#define IMGUI_ENABLED true

sead::TextWriter *gTextWriter;

namespace al {
    class SensorMsg{
        SEAD_RTTI_BASE(SensorMsg);
};
}

namespace rs {
    bool isMsgPlayerCapCatch(const al::SensorMsg*);
}

int deathCounter = 0;
bool isFlying = false;
bool isInCapture = false;

void drawDebugWindow() {
    HakoniwaSequence *gameSeq = (HakoniwaSequence *) GameSystemFunction::getGameSystem()->mCurSequence;

    ImGui::Begin("Game Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
    
    auto windowSize = ImGui::GetIO().DisplaySize;
    ImGui::SetWindowSize(windowSize, ImGuiCond_FirstUseEver);

    auto curScene = gameSeq->mStageScene;

    bool isInGame =
            curScene && curScene->mIsAlive && !curScene->isPause();

    
    if (isInGame) {
        StageScene *stageScene = gameSeq->mStageScene;
        auto *player = (PlayerActorHakoniwa*) rs::getPlayerActor(stageScene);
        if(player && player->mHackCap){
            if(player->mHackCap->isFlying()){
                isFlying = true;
                ImGui::SetCursorPosX(650);
                                //ImGui::Text("Distance: %dm", (int) std::round(al::calcDistance(player, player->mHackCap) * 0.9f / 100));

                ImGui::Text("Distance: %dm", (int) std::round(al::calcDistance(player, player->mHackCap) * 0.9f) / 100);
            } else {
                
                isFlying = false;
                ImGui::SetCursorPosX(600);
                ImGui::Text("Throw cappy to move!");
            }
            ImGui::SetCursorPos(ImVec2(1400, 800));
            ImGui::Text("Deaths: %d", deathCounter);
        }
    }

    ImGui::End();
}

int flyingFrames = 0;

HOOK_DEFINE_TRAMPOLINE(ControlHook) {
    static void Callback(StageScene *scene) {
        auto player = (PlayerActorHakoniwa*) rs::getPlayerActor(scene);
        if(player && player->mHackKeeper->currentHackActor){
            flyingFrames = 0;
            isInCapture = true;
        }else {
            isInCapture = false;
        }
        if(player && player->mHackCap){
            if(isFlying)
                flyingFrames++;

    
            //if(al::calcDistance(player, player->mHackCap) * 0.9f / 100 > 250)
              //  al::offCollide(player->mHackCap);
            *al::findActorParamF32(player->mHackCap, "戻り最高速度") = 20.0f;
            *al::findActorParamF32(player->mHackCap, "[水中]戻り最高速度") = 10.0f;
            if(flyingFrames > 30 && al::calcDistance(player, player->mHackCap) < 110){
                Logger::log("Should kill!");
                PlayerHelper::killPlayer(player);
                isCatch = false;
            }
        }
        
        Orig(scene);
    }
};

HOOK_DEFINE_TRAMPOLINE(SceneInit){
    static void Callback(al::ActorInitInfo* info, StageScene* curScene, al::PlacementInfo const* placement, al::LayoutInitInfo const* lytInfo,
                         al::ActorFactory const* factory, al::SceneMsgCtrl* sceneMsgCtrl, al::GameDataHolderBase* dataHolder) {
        flyingFrames = 0;
                Orig(info, curScene, placement, lytInfo, factory, sceneMsgCtrl, dataHolder);

    }
};

HOOK_DEFINE_TRAMPOLINE(InputHook){
    static const sead::Vector2f& Callback(int port){
        if(!isFlying && !isInCapture)
            return sead::Vector2f::zero;
        return Orig(port);
    }
};

HOOK_DEFINE_REPLACE(ReplaceSeadPrint) {
    static void Callback(const char *format, ...) {
        va_list args;
        va_start(args, format);
        Logger::log(format, args);
        va_end(args);
    }
};

HOOK_DEFINE_TRAMPOLINE(CreateFileDeviceMgr) {
    static void Callback(sead::FileDeviceMgr *thisPtr) {

        Orig(thisPtr);

        thisPtr->mMountedSd = nn::fs::MountSdCardForDebug("sd").isSuccess();

        sead::NinSDFileDevice *sdFileDevice = new sead::NinSDFileDevice();

        thisPtr->mount(sdFileDevice);
    }
};

HOOK_DEFINE_TRAMPOLINE(RedirectFileDevice) {
    static sead::FileDevice *
    Callback(sead::FileDeviceMgr *thisPtr, sead::SafeString &path, sead::BufferedSafeString *pathNoDrive) {

        sead::FixedSafeString<32> driveName;
        sead::FileDevice *device;

        // Logger::log("Path: %s\n", path.cstr());

        if (!sead::Path::getDriveName(&driveName, path)) {

            device = thisPtr->findDevice("sd");

            if (!(device && device->isExistFile(path))) {

                device = thisPtr->getDefaultFileDevice();

                if (!device) {
                    Logger::log("drive name not found and default file device is null\n");
                    return nullptr;
                }

            } else {
                Logger::log("Found File on SD! Path: %s\n", path.cstr());
            }

        } else
            device = thisPtr->findDevice(driveName);

        if (!device)
            return nullptr;

        if (pathNoDrive != nullptr)
            sead::Path::getPathExceptDrive(pathNoDrive, path);

        return device;
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderLoadArc) {
    static sead::ArchiveRes *
    Callback(al::FileLoader *thisPtr, sead::SafeString &path, const char *ext, sead::FileDevice *device) {

        // Logger::log("Path: %s\n", path.cstr());

        sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

        if (sdFileDevice && sdFileDevice->isExistFile(path)) {

            Logger::log("Found File on SD! Path: %s\n", path.cstr());

            device = sdFileDevice;
        }

        return Orig(thisPtr, path, ext, device);
    }
};

sead::FileDevice *tryFindNewDevice(sead::SafeString &path, sead::FileDevice *orig) {
    sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

    if (sdFileDevice && sdFileDevice->isExistFile(path))
        return sdFileDevice;

    return orig;
}

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistFile) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistArchive) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};

HOOK_DEFINE_TRAMPOLINE(CappyKillHook){
    static void Callback(void* thisPtr, al::SensorMsg* msg, al::HitSensor* source, al::HitSensor* target){
        rs::isMsgPlayerCapCatch(msg);
            isCatch = true;
        Orig(thisPtr, msg, source, target);
    }
};

HOOK_DEFINE_TRAMPOLINE(DeathHook){static void Callback(void* acc){deathCounter++; flyingFrames = 0; Orig(acc);}};

extern "C" void exl_main(void *x0, void *x1) {
    /* Setup hooking enviroment. */
    exl::hook::Initialize();

    nn::os::SetUserExceptionHandler(exception_handler, nullptr, 0, nullptr);
    installExceptionStub();

    Logger::instance().init(LOGGER_IP, 3080);

    runCodePatches();


    DeathHook::InstallAtSymbol("_ZN16GameDataFunction10killPlayerE20GameDataHolderWriter");
    RedirectFileDevice::InstallAtOffset(0x76CFE0);
    FileLoaderLoadArc::InstallAtOffset(0xA5EF64);
    CreateFileDeviceMgr::InstallAtOffset(0x76C8D4);
    FileLoaderIsExistFile::InstallAtSymbol(
            "_ZNK2al10FileLoader11isExistFileERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");
    FileLoaderIsExistArchive::InstallAtSymbol(
            "_ZNK2al10FileLoader14isExistArchiveERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");

    // Sead Debugging Overriding

    ReplaceSeadPrint::InstallAtOffset(0xB59E28);
    InputHook::InstallAtSymbol("_ZN2al12getLeftStickEi");
    //Input Hooks
    /*
    InputHook::InstallAtSymbol("_ZN2al21isPadHoldUpRightStickEi");
    InputHook::InstallAtSymbol("_ZN2al23isPadHoldDownRightStickEi");
    InputHook::InstallAtSymbol("_ZN2al24isPadHoldRightRightStickEi");
    InputHook::InstallAtSymbol("_ZN2al23isPadHoldLeftRightStickEi");
    InputHook::InstallAtSymbol("_ZN2al19isPadHoldRightStickEi");
*/
    //InputHook::InstallAtSymbol("_ZN2al21isPadHoldUpRightStickEi");


    // General Hooks
    CappyKillHook::InstallAtSymbol("_ZN7HackCap10receiveMsgEPKN2al9SensorMsgEPNS0_9HitSensorES5_");

    ControlHook::InstallAtSymbol("_ZN10StageScene7controlEv");
    SceneInit::InstallAtOffset(0x4C8DD0);

    // ImGui Hooks
#if IMGUI_ENABLED
    nvnImGui::InstallHooks();

    nvnImGui::addDrawFunc(drawDebugWindow);
#endif

}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}
