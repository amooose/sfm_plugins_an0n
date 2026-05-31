#include <windows.h>
#include <cstdint>
#include <cstring>
#include "pluginCleanup.h"
#include "plugin.h"
#include "patch.h"
#include <TrampolineHook.h>
#include "safetyhook.hpp"
#include <string>
#include "config.h"

// Plugin definitions
class CPlugin_ScriptUpload : public IServerPluginCallbacks
{
    bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;
    void Unload(void) override {}
    void Pause(void) override {}
    void UnPause(void) override {}
    const char* GetPluginDescription(void) override { return "Optifilm Patch"; }
    void LevelInit(char const* pMapName) override {}
    void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override {}
    void GameFrame(bool simulating) override {}
    void LevelShutdown(void) override {}
    void ClientActive(edict_t* pEntity) override {}
    void ClientFullyConnect(edict_t* pEntity) override {}
    void ClientDisconnect(edict_t* pEntity) override {}
    void ClientPutInServer(edict_t* pEntity, char const* playername) override {}
    void SetCommandClient(int index) override {}
    void ClientSettingsChanged(edict_t* pEdict) override {}
    PLUGIN_RESULT ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) override { return PLUGIN_CONTINUE; }
    PLUGIN_RESULT ClientCommand(edict_t* pEntity, const CCommand& args) override { return PLUGIN_CONTINUE; }
    PLUGIN_RESULT NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) override { return PLUGIN_CONTINUE; }
    void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, int eStatus, const char* pCvarName, const char* pCvarValue) override {}
    void OnEdictAllocated(edict_t* edict) override {}
    void OnEdictFreed(const edict_t* edict)  override {}
};
const char* PLUGIN_VER = "v1";
static CPlugin_ScriptUpload g_PluginScriptUpload;
IServerPluginCallbacks* g_pPlugin = &g_PluginScriptUpload;


static uintptr_t ifm_base = (uintptr_t)GetModuleHandleW(L"ifm.dll");
static uintptr_t mat_base = (uintptr_t)GetModuleHandleW(L"materialsystem.dll");
static uintptr_t engine_base = (uintptr_t)GetModuleHandleW(L"engine.dll");
HMODULE hQtGui = GetModuleHandleA("qtgui4.dll");
HMODULE hQtCore = GetModuleHandleA("QtCore4.dll");
typedef uint32_t _DWORD;


// ------------------ Handle throttling the FK lag on IK Rigs ------------------
SafetyHookMid g_tlthrottle_hook{};
int tcount = 0;
bool allowAfterViewer = false;
int allowCount = 0;
void TimeAndLogsThrottle(safetyhook::Context& ctx)
{
    if (tcount % 64 == 0 || allowAfterViewer) {
        tcount = 0;
        allowCount++;
        if (allowCount > 200) {
            allowAfterViewer = false;
            allowCount = 0;
        }
    }
    else {
        ctx.eip += 2;
    }
    tcount++;
}

void InstallTimeAndLogsThrottle()
{
    g_tlthrottle_hook = safetyhook::create_mid(
        reinterpret_cast<void*>(ifm_base + 0x304d66),
        TimeAndLogsThrottle
    );
    OutputDebugStringA("[SFM] Installed TimeAndLogsThrottle hook\n");
}
// -----------------------------------------------------------------------------




// ------------------ Handle throttling curve recalculation when moving keyframes in the graph editor ------------------
static LARGE_INTEGER g_freq = {};
static LARGE_INTEGER g_last = {};
static bool g_timerInit = false;

static __forceinline bool ShouldRun()
{
    if (!g_timerInit)
    {
        QueryPerformanceFrequency(&g_freq);
        QueryPerformanceCounter(&g_last);
        g_timerInit = true;
        return true;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    const double dt = double(now.QuadPart - g_last.QuadPart) / double(g_freq.QuadPart);

    const double target = 1.0 / 60.0;

    if (dt < target)
        return false;

    g_last = now;
    return true;
}

using fnMoveKeyframes = int(__fastcall*)(int _this, void* edx, int a2, float a3,
    float a4, float a5, int a6, char a7, bool a8, char a9, char a10);
static fnMoveKeyframes oMoveKeyframes = nullptr;
static TrampolineHook g_MoveKeyframesHook;

static int __fastcall hkMoveKeyframes(int _this, void* edx, int a2, float a3,
    float a4, float a5, int a6, char a7, bool a8, char a9, char a10)
{
    if (!ShouldRun())
        return 0;

    return oMoveKeyframes(_this, edx, a2, a3, a4, a5, a6, a7, a8, a9, a10);
}

bool InstallMoveKeyframesHook()
{
    if (!g_MoveKeyframesHook.Install(ifm_base + 0x00C7850, (void*)&hkMoveKeyframes, 6))
        return false;
    oMoveKeyframes = g_MoveKeyframesHook.GetOriginal<fnMoveKeyframes>();
    OutputDebugStringA("[SFM] Installed g_MoveKeyframesHook hook\n");
    return true;
}
// -----------------------------------------------------------------------------




// ------------------ Handle calling vs.materialsystem.g_pMaterialSystem.UncacheUnusedMaterials() on Model viewer closing ------------------
//Execute uncache method
using UncacheUnusedMaterials = int(__thiscall*)(void* _this, char a2);
void executeUncache() {
    auto fn = reinterpret_cast<UncacheUnusedMaterials>((uintptr_t)mat_base + 0x11950);
    void* matsystemPtr = (void*)(mat_base + 0x105F88);
    int result = fn(matsystemPtr, 0);
}

typedef uint32_t* (__thiscall* fnQPaintDevice_MDLSelector)( uint32_t* _this, char a2);
fnQPaintDevice_MDLSelector oQPaintDevice_MDLSelector = nullptr;
uint32_t* __fastcall hkQPaintDevice_MDLSelector(uint32_t* _this, void* edx, char a2)
{
    allowAfterViewer = true;
    executeUncache();
    return oQPaintDevice_MDLSelector(_this, a2);
}

//Hook onto the model viewer dialog closing to trigger uncache
static TrampolineHook g_QPaintDeviceHook;
bool InstallQPaintDeviceHook()
{
    uintptr_t addr = ifm_base + 0x125270;

    if (!g_QPaintDeviceHook.Install(
        addr,
        (void*)&hkQPaintDevice_MDLSelector,
        6))
    {
        return false;
    }

    oQPaintDevice_MDLSelector =
        g_QPaintDeviceHook.GetOriginal<fnQPaintDevice_MDLSelector>();

    OutputDebugStringA("[SFM] Installed QPaintDevice hook\n");
    
    return true;
}
// -----------------------------------------------------------------------------


// ------------------ Handle calling vs.materialsystem.g_pMaterialSystem.UncacheUnusedMaterials() on .BSP Load closing ------------------
typedef uint32_t* (__thiscall* fnQPaintDevice_CAssetPickerFrame)(uint32_t* _this);
fnQPaintDevice_CAssetPickerFrame oQPaintDevice_CAssetPickerFrame = nullptr;
uint32_t* __fastcall hkQPaintDevice_CAssetPickerFrame(uint32_t* _this, void* edx)
{
    allowAfterViewer = true;
    OutputDebugStringA("[SFM] BSP Asset uncache\n");
    executeUncache();
    return oQPaintDevice_CAssetPickerFrame(_this);
}

static TrampolineHook g_QPaintDeviceHook_Asset;
bool InstallQPaintDeviceHookAsset()
{
    uintptr_t addr = ifm_base + 0x126850;

    if (!g_QPaintDeviceHook_Asset.Install(
        addr,
        (void*)&hkQPaintDevice_CAssetPickerFrame,
        6))
    {
        return false;
    }

    oQPaintDevice_CAssetPickerFrame =
        g_QPaintDeviceHook_Asset.GetOriginal<fnQPaintDevice_CAssetPickerFrame>();

    OutputDebugStringA("[SFM] Installed QPaintDevice_Asset hook\n");

    return true;
}
// -----------------------------------------------------------------------------

const char* GetClassName(void* pWidget)
{
    if (pWidget)
    {
        void** vtable = *(void***)pWidget;
        typedef void* (__thiscall* fnMetaObject)(void*);
        fnMetaObject getMetaObject = (fnMetaObject)vtable[0];
        void* pMetaObject = getMetaObject(pWidget);
        if (pMetaObject)
        {
            const char* className = *(const char**)((uintptr_t)pMetaObject + 4);
            return className;
        }
    }
    return nullptr;
}

// ------------------ Handle getting CQLowerTimestripWidget and hide it + resize parent ------------------
typedef void(__fastcall* fnQWidget_update)(void* ecx, void* edx);
typedef void(__thiscall* fnSetMaxHeight)(void* pWidget, int height);
typedef void* (__thiscall* fnQObject_parent)(void* pThis);
fnQWidget_update oQWidget_update = nullptr;
TrampolineHook g_QWidgetUpdateHook;
void* g_pTimestripWidgetLowerTimeline = nullptr;
void* g_pTimestripWidgetLowerMotion = nullptr;
void* g_pTimestripWidgetLowerGraph = nullptr;
typedef void(__thiscall* fnQWidget_hide)(void* ecx);
bool lowerHidden = false;
void __fastcall hkQWidget_update(void* pWidget, void* edx)
{
    if (pWidget && !lowerHidden)
    {
        void** vtable = *(void***)pWidget;
        typedef void* (__thiscall* fnMetaObject)(void*);
        fnMetaObject getMetaObject = (fnMetaObject)vtable[0];
        void* pMetaObject = getMetaObject(pWidget);

        if (pMetaObject)
        {
            const char* className = *(const char**)((uintptr_t)pMetaObject + 4);
            if (className && strcmp(className, "CQLowerTimestripWidget") == 0)
            {
                void* pParent = nullptr;
                fnQObject_parent QObject_parent =
                    (fnQObject_parent)GetProcAddress(hQtCore, "?parent@QObject@@QBEPAV1@XZ");
                if (strcmp(GetClassName(QObject_parent(pWidget)), "CQGraphEditorWindow") == 0) {
                    g_pTimestripWidgetLowerGraph = pWidget;
                }
                if (strcmp(GetClassName(QObject_parent(pWidget)), "CQTimelineEditor") == 0) {
                    g_pTimestripWidgetLowerTimeline = pWidget;
                }
                
            }

            if (className && strcmp(className, "CQMotionEditorLowerTimestripWidget") == 0)
            {
                g_pTimestripWidgetLowerMotion = pWidget;
            }


            if (g_pTimestripWidgetLowerTimeline && g_pTimestripWidgetLowerMotion && g_pTimestripWidgetLowerGraph) {

                static fnQWidget_hide QWidget_hide = nullptr;
                static fnSetMaxHeight QWidget_maxHeight = nullptr;
                    
                QWidget_hide = (fnQWidget_hide)GetProcAddress(hQtGui, "?hide@QWidget@@QAEXXZ");
                QWidget_maxHeight = (fnSetMaxHeight)GetProcAddress(hQtGui, "?setMaximumHeight@QWidget@@QAEXH@Z");
                    

                if (QWidget_hide && QWidget_maxHeight)
                {
                    QWidget_hide(g_pTimestripWidgetLowerTimeline);
                    //QWidget_maxHeight(g_pTimestripWidgetLowerTimeline,0);
                    QWidget_hide(g_pTimestripWidgetLowerMotion);
                    //QWidget_maxHeight(g_pTimestripWidgetLowerMotion,0);
                    QWidget_hide(g_pTimestripWidgetLowerGraph);
                    //QWidget_maxHeight(g_pTimestripWidgetLowerGraph,0);
                    g_pTimestripWidgetLowerTimeline = nullptr;
                    lowerHidden = true;
                       
                }
            }
            
        }
    }

    if (oQWidget_update) {
        oQWidget_update(pWidget, edx);
        if (lowerHidden) {
            g_QWidgetUpdateHook.Remove();
            OutputDebugStringA("[SFM] Removed lower timeline and hook\n");
        }
    }
}

bool InstallQWidgetUpdateHook()
{
    uintptr_t addr = (uintptr_t)GetProcAddress(hQtGui, "?update@QWidget@@QAEXXZ");
    if (!g_QWidgetUpdateHook.Install(addr, (void*)&hkQWidget_update, 6))
    {
        OutputDebugStringA("[SFM] Failed to install QWidget::update hook.\n");
        return false;
    }
    oQWidget_update = g_QWidgetUpdateHook.GetOriginal<fnQWidget_update>();
    return true;
}
// -----------------------------------------------------------------------------

bool CPlugin_ScriptUpload::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory)
{
    LOG("Optifilm Patch loading...\n");
    if (ifm_base == NULL)
    {
        LOG("Failed to find ifm\n");
        return false;
    }

    cleanupPlugins(std::string(PLUGIN_NAME));
    Config cfg;
    cfg.Load("optifilm.cfg");

    ifm_base = (uintptr_t)GetModuleHandleW(L"ifm.dll");

    if (cfg.GetBool("opt1_memclear")) {
        InstallQPaintDeviceHook();
        InstallQPaintDeviceHookAsset();
    }
    if (cfg.GetBool("opt2_noKeyLag")) {
        InstallMoveKeyframesHook();
    }
    if (cfg.GetBool("opt3_reduceFKLag")) {
        InstallTimeAndLogsThrottle();
    }
    
    if (cfg.GetBool("opt4_timelineFPSBoost")) {
        InstallQWidgetUpdateHook();
        // Patch horrible inefficient Timeline numbers centering method out, makes it right aligned but who cares
        //Literally saves 10-20 frames
        BYTE patch[] = { 0x90, 0x90, 0x90, 0x90, 0x90,
                     0x90, 0x90, 0x90, 0x90, 0x90,
                     0x90, 0x90, 0x90, 0x90, 0x90,
                     0x90, 0x90 };
        Patch::WriteBytes((void*)(ifm_base + 0x19BCAB), patch, sizeof(patch));
    }

    if (cfg.GetBool("opt5_enginePump2ms")) {
        //Engine-Pump 20ms wait --> 2ms
        double delay = 0.002;
        Patch::WriteBytes((void*)(engine_base + 0x361470), &delay, sizeof(double));
        OutputDebugStringA("[SFM] Reduced Engine Pump wait to 2ms\n");
    }
    return true;
}
