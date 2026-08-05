#include "GMFuncShorthands.h"
#include "GMGeneralLibrary.h"
#include "GMFunctionOverrides.h"
#include <cstdint>
#include <iostream>
#include <synchapi.h>
#include <unordered_map>
#include <utility>
#include <vector>
#include <windows.h>
#include <winsock.h>
#include "MinHook.h"
// Only uncomment the above if I start overriding ASM functions
uint8_t* g_base = nullptr;
uint8_t* table  = nullptr;
std::unordered_map<std::string, function_hook> override_functions;
RValue original_upgrade_func{};
bool mod_active{false};


typedef int (*populate_t)(void);
static populate_t g_orig_populate = nullptr;

typedef long long (*func_dispatch)(long long, int, long long, long long, long long);
static func_dispatch dispatcher = nullptr;

static bool slot_ready(int id){
    uint64_t s = *(uint64_t*)(table + (size_t)id * 0x18);
    return s >= 0x140000000ULL && s < 0x150000000ULL;   // real module pointer only
}

static bool register_overrides(const std::vector<std::pair<int, gml_fn>>& functions){
    for (std::pair<int, gml_fn> func : functions){
        if (!*(const char**)(table + (size_t)func.first * 0x18) || (uintptr_t)*(const char**)(table + (size_t)func.first * 0x18) < 0x10000) {
            fprintf(stderr, "id %d name not ready (%p), retrying\n", func.first, (void*)*(const char**)(table + (size_t)func.first * 0x18));
            fflush(stderr);
            return false;
        }
        uint8_t* entry = table + (size_t)func.first * 0x18;
        printf("entry bytes: ");
        for (int i = 0; i < 0x18; i++) printf("%02x ", entry[i]);
        printf("\n"); fflush(stdout);
        std::cout << "Registering override...\n";
        std::cout << "Getting function name...\n";
        std::string func_name = *(const char**)(table + (size_t)func.first * 0x18);
        std::cout << "Getting original function...\n";
        gml_fn original_function = *(gml_fn*)(table + (size_t)func.first * 0x18 + 8);
        std::cout << "Creating hook...\n";
        function_hook hook = {func.first, original_function, func.second};
        std::cout << "Registering hook to map...\n";
        override_functions.insert({func_name, hook});
    }
    return true;
}

static bool hookshot(){
    if (!table) return false;
    DWORD old;
    for (const auto& [name, hook] : override_functions) {
        void** slot = (void**)(table + (size_t)hook.id * 0x18 + 8);
        if(*slot != (void*)hook.override){
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
            std::cout << "Hooking function " << name << "... \n";
            *slot = (void*)hook.override;
            VirtualProtect(slot, sizeof(void*), old, &old);
        }else{
        }
    }
    return true;
}


static void hk_populate(void){
    std::cout << "She be pupu my lating till I fucking CRASH (out)\n";
    register_overrides({
        {GM_AUDIO_PLAY_SOUND, talksound_override},
        {GM_ASSET_GET_INDEX, custom_asset_patch},
        {GM_INSTANCE_DESTROY, custom_func_loader},
        {GM_WINDOW_SET_CURSOR, set_salvage_var},
        {GM_METHOD, func_override},
        {GM_WIN8_LIVETILE_TILE_NOTIFICATION, new_upgrades},
        {GM_ARRAY_CONTAINS_EXT, get_dialouge},
        {GM_SHOW_DEBUG_MESSAGE, override_background},
        {GM_VAR_INSTANCE_GET, super_conditions}
        
    });
    hookshot();
}

int override_datawin(){
    int return_val = g_orig_populate();
    table = *(uint8_t**)(g_base + FUNC_TABLE_RVA);
    hk_populate();
    mod_active = true;
    
    if(gm_call(GM_FILE_EXISTS, "new_quota.alw").real != 0){
        RValue buffer = gm_call(GM_BUFFER_LOAD, "new_quota.alw");
        new_quota = gm_call(GM_REAL, gm_call(GM_BUFFER_READ, buffer, 11)).real;
        gm_call(GM_BUFFER_DELETE, buffer);
    }

    if(gm_call(GM_FILE_EXISTS, "mechanics_off.alw").real != 0){
        disable_override = true;
        gml::variable_global_set("mechanics_off", 1);
    }

    return return_val;
}

long long override_winfunc(long long param_1, int param_2, long long param_3, long long param_4, long long param_5){
    long long result = dispatcher(param_1, param_2, param_3, param_4, param_5);
    std::cout << "First arg: " << param_1 << "\n" <<
        "Second arg: " << param_2 << "\n" <<
        "Third arg: " << param_3 << "\n" <<
        "Fourth arg: " << param_4 << "\n" <<
        "Fifth arg: " << param_5 << "\n" <<
        "Result: " << result << "\n";
    return result; 
}

static DWORD WINAPI gm_worker(LPVOID){
    g_base = (uint8_t*)GetModuleHandleW(NULL);
    MH_Initialize();
    MH_CreateHook((void*)(g_base + 0x113a70), (void*)override_datawin, (void**)&g_orig_populate);
    MH_EnableHook((void*)(g_base + 0x113a70));

    //MH_CreateHook((void*)(g_base + 0x2cb960), (void*)override_winfunc, (void**)&dispatcher);
    //MH_EnableHook((void*)(g_base + 0x2cb960));

    while (!mod_active) {
        Sleep(50);
    }

    for(;;) {
        Sleep(100);
        hookshot();
    };
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID){
    if (reason == DLL_PROCESS_ATTACH){ 
        std::cout << "Starting thread...\n";
        DisableThreadLibraryCalls(h); 
        CreateThread(NULL,0,gm_worker,NULL,0,NULL); 
    }
    return TRUE;
}

extern "C" __declspec(dllexport) double Init(void){ 

    return 0.0; 

}
