// frickmod.cpp — call GM built-ins + read/write globals by name. No source / no data.win edits.
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <winnls.h>
#define WIN32_LEAN_AND_MEAN
#define __USE_MINGW_ANSI_STDIO 1
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include "MinHook.h"
#define DEBUG



bool assets_loaded = false;

// ---- RValue: 16 bytes. value@+0, flags@+8, kind@+0xC. (RV_ prefix dodges wtypes.h's VT_*) ----
enum : uint32_t { RV_REAL=0, RV_STRING=1, RV_ARRAY=2, RV_PTR=3, RV_UNDEF=5,
                  RV_OBJECT=6, RV_INT32=7, RV_INT64=10, RV_BOOL=13, RV_HANDLE=15 };
struct RValue {
    union { double real; void* ptr; int32_t i32; int64_t i64; };
    uint32_t flags;   // +0x08
    uint32_t kind;    // +0x0C
};
static_assert(sizeof(RValue) == 16, "RValue must be 16 bytes");

typedef void (*exec_fn)(int id, void* self, void* other);
static exec_fn g_orig_exec = nullptr;

static inline RValue r_real(double x){ RValue r{}; r.real = x; r.kind = RV_REAL; return r; }
static inline double rv_to_double(const RValue& r){
    switch (r.kind){
        case RV_INT32: return (double)r.i32;
        case RV_INT64: return (double)r.i64;
        case RV_BOOL:  return r.real != 0.0 ? 1.0 : 0.0;
        default:       return r.real;
    }
}

static RValue raw_textbox_id = r_real(62);

// ---- runtime hooks (RVA off the module base) ----
static uint8_t* g_base = nullptr;
static const uintptr_t FUNC_TABLE_RVA = 0xd99ef8;   // .data ptr -> table of 0x18-byte entries
static const uintptr_t ALLOC_RVA      = 0x749620;   // void* alloc(size, _, _, flag)

typedef void  (*gml_fn)(RValue* result, void* self, void* other, int argc, RValue* args);
typedef void* (*gm_alloc_t)(size_t size, void* a2, int64_t a3, int64_t flag);



static inline gml_fn gm_impl(int id){
    uint8_t* table = *(uint8_t**)(g_base + FUNC_TABLE_RVA);
    return *(gml_fn*)(table + (size_t)id * 0x18 + 8);
}
static inline RValue gm_call(int id, RValue* args, int argc){
    RValue result{};                 // zero-init -> kind 0, value 0
    gm_impl(id)(&result, nullptr, nullptr, argc, args);   // self/other null: context-free fns only
    return result;
}

static inline RValue spec_call(gml_fn func, RValue* args, int argc){
    RValue result{};                 // zero-init -> kind 0, value 0
    func(&result, nullptr, nullptr, argc, args);   // self/other null: context-free fns only
    return result;
}

static inline RValue gm_call1_helper(int id, double a){ RValue v[1]={r_real(a)}; return gm_call(id, v, 1); }

// ---- GM string from a C string (replicates chr's inline construction exactly) ----
struct RefString { char* chars; int32_t refcount; int32_t length; };  // 16 bytes

static RValue r_string(const char* s){
    gm_alloc_t alloc = (gm_alloc_t)(g_base + ALLOC_RVA);
    size_t len = strlen(s);
    RefString* rs = (RefString*)alloc(sizeof(RefString), nullptr, 0, 1); // struct, flag 1
    char*      buf = (char*)alloc(len + 1, nullptr, 0, 0);               // chars, flag 0
    memcpy(buf, s, len + 1);
    rs->chars = buf; rs->refcount = 1; rs->length = (int32_t)len;
    RValue r{}; r.ptr = rs; r.kind = RV_STRING;
    return r;
    // NOTE: not freed -> ~tiny leak per call. Safe; freeing risks a double-free vs the runtime.
}

// handy IDs
enum { 
    GM_GET_TIMER=1819, 
    GM_IRANDOM=220, 
    GM_VAR_GLOBAL_GET=1165, 
    GM_VAR_GLOBAL_SET=1166 };

// ---- global accessors by name ----
static void set_global_real(const char* name, double value){
    RValue a[2] = { r_string(name), r_real(value) };
    gm_call(GM_VAR_GLOBAL_SET, a, 2);
}
static void set_global_string(const char* name, const char* value){
    RValue a[2] = { r_string(name), r_string(value) };
    gm_call(GM_VAR_GLOBAL_SET, a, 2);
}
static RValue get_global(const char* name){
    RValue nm = r_string(name);
    return gm_call(GM_VAR_GLOBAL_GET, &nm, 1);
}

static RValue get_global(RValue name){
    return gm_call(GM_VAR_GLOBAL_GET, &name, 1);
}
static double get_global_real(const char* name){ return rv_to_double(get_global(name)); }

// ===== main-thread hook: hijack draw_sprite (id 500) to run logic every frame =====
enum { GM_DRAW_SPRITE = 500 };
static const ULONGLONG HOOK_INTERVAL_MS = 10;   // heartbeat spacing (~1 frame @60fps); tune if needed

static gml_fn g_orig_draw_sprite = nullptr;

static gml_fn g_orig_cleanup = nullptr;

// >>> YOUR per-frame logic. Runs on GM's MAIN thread with valid state.
//     It fires ~every frame, so gate one-shot actions on a state change (see the Night example).


static inline bool rv_is_num(uint32_t k){
    return k==RV_REAL || k==RV_INT32 || k==RV_INT64 || k==RV_BOOL;
}

static bool rv_equal(const RValue& a, const RValue& b){
    // numeric vs numeric -> compare by value (5.0 == 5 == true, like GM)
    if (rv_is_num(a.kind) && rv_is_num(b.kind))
        return rv_to_double(a) == rv_to_double(b);

    // string vs string -> compare the TEXT (length + bytes), never the pointer
    if (a.kind==RV_STRING && b.kind==RV_STRING){
        RefString* ra = (RefString*)a.ptr; RefString* rb = (RefString*)b.ptr;
        if (!ra || !rb) return ra == rb;
        if (ra->length != rb->length) return false;
        return memcmp(ra->chars, rb->chars, (size_t)ra->length) == 0;
    }

    if (a.kind==RV_HANDLE && b.kind==RV_HANDLE)
        return a.i32 == b.i32;          // <-- see note

    // array / struct / ptr of the same kind -> GM compares by REFERENCE (pointer)
    if (a.kind==b.kind && (a.kind==RV_ARRAY || a.kind==RV_OBJECT || a.kind==RV_PTR))
        return a.ptr == b.ptr;

    if (a.kind==RV_UNDEF && b.kind==RV_UNDEF) return true;   // both undefined
    return false;                                            // different / incompatible kinds
}

template <typename> inline constexpr bool rv_always_false = false;  // for the error branch

// ---- RValue -> C++ value.  double d = rv_to<double>(v);  const char* s = rv_to<const char*>(v);
template <typename T>
static T rv_to(const RValue& v){
    if constexpr (std::is_same_v<T, RValue>)
        return v;                                                    // passthrough
    else if constexpr (std::is_arithmetic_v<T>)                      // int, double, float, bool, ...
        return static_cast<T>(rv_to_double(v));
    else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
        return (v.kind == RV_STRING && v.ptr) ? ((RefString*)v.ptr)->chars : "";
    else
        static_assert(rv_always_false<T>, "rv_to: unsupported type");
}
static gml_fn get_base_asset = nullptr;
// ---- C++ value -> RValue.  RValue a = rv_from(5.0);  RValue s = rv_from("hi");
template <typename T>
static RValue rv_from(T x){                                          // by value: string literals decay to const char*
    if constexpr (std::is_same_v<T, RValue>)
        return x;                                                    // passthrough
    else if constexpr (std::is_arithmetic_v<T>)
        return r_real(static_cast<double>(x));
    else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
        return r_string(x);
    else
        static_assert(rv_always_false<T>, "rv_from: unsupported type");
}


enum { GM_VAR_INSTANCE_SET=1169, GM_INSTANCE_FIND=100,
       GM_INSTANCE_NUMBER=102, GM_INSTANCE_EXISTS=101, GM_ASSET_GET_INDEX=835 };

enum EditScope { EDIT_INSTANCE, EDIT_GLOBAL, EDIT_OBJECT };

struct EditTarget {
    EditScope   scope;        // INSTANCE = one instance · GLOBAL = a global · OBJECT = every instance of an object
    const char* varName;      // variable to write
    bool        isUnique;     // INSTANCE only: true -> instanceID is the Nth-created ordinal within objectName;
                              //                false -> instanceID is the raw runtime id
    double      instanceID;   // raw id, or ordinal when isUnique
    const char* objectName;   // required for OBJECT, and for INSTANCE when isUnique
};

struct ValueSource {
    int     functionID;       // builtin id whose return becomes the value
    RValue* args;             // its arguments
    int     argc;
    bool    perInstance;      // OBJECT only: recompute the value for each instance (e.g. per-instance RNG)
};

// --- small resolvers (all context-free, so self=nullptr is fine) ---
static double resolve_object(const char* name){
    RValue a[1] = { r_string(name) };
    return rv_to_double(spec_call(get_base_asset, a, 1));   // -1 if not found
}
static bool inst_exists(double id){
    RValue a[1] = { r_real(id) };
    return rv_to_double(gm_call(GM_INSTANCE_EXISTS, a, 1)) != 0.0;
}
static double inst_find(double obj, double n){
    RValue a[2] = { r_real(obj), r_real(n) };
    return rv_to_double(gm_call(GM_INSTANCE_FIND, a, 2));
}
static double inst_number(double obj){
    RValue a[1] = { r_real(obj) };
    return rv_to_double(gm_call(GM_INSTANCE_NUMBER, a, 1));
}

// Returns: # of instances written (>=0), or  -1 target not found · -2 bad functionID · -3 bad scope.
static int edit_variable(const EditTarget& t, const ValueSource& v){
    if (gm_impl(v.functionID) == nullptr) return -2;          // empty/invalid table slot

    switch (t.scope){
    case EDIT_GLOBAL: {
        RValue a[2] = { r_string(t.varName), gm_call(v.functionID, v.args, v.argc) };
        gm_call(GM_VAR_GLOBAL_SET, a, 2);
        return 1;
    }
    case EDIT_INSTANCE: {
        double id;
        if (t.isUnique){
            if (!t.objectName) return -1;
            double obj = resolve_object(t.objectName);
            if (obj < 0) return -1;
            id = inst_find(obj, t.instanceID);               // instanceID = ordinal
        } else {
            id = t.instanceID;                                // raw runtime id
        }
        if (!inst_exists(id)) return -1;
        RValue a[3] = { r_real(id), r_string(t.varName), gm_call(v.functionID, v.args, v.argc) };
        gm_call(GM_VAR_INSTANCE_SET, a, 3);
        return 1;
    }
    case EDIT_OBJECT: {
        if (!t.objectName) return -1;
        double obj = resolve_object(t.objectName);
        if (obj < 0) return -1;
        int n = (int)inst_number(obj);
        RValue shared{}; bool haveShared = false;
        if (!v.perInstance){ shared = gm_call(v.functionID, v.args, v.argc); haveShared = true; }
        int written = 0;
        for (int i = 0; i < n; i++){
            double id = inst_find(obj, (double)i);
            if (!inst_exists(id)) continue;
            RValue val = haveShared ? shared : gm_call(v.functionID, v.args, v.argc);
            RValue a[3] = { r_real(id), r_string(t.varName), val };
            gm_call(GM_VAR_INSTANCE_SET, a, 3);
            written++;
        }
        return written;
    }
    }
    return -3;
}

enum {
    GM_STRING_CONCAT = 305, 
    GM_OBJECT_TEXTBOX_ID = 62, 
    GM_SCRIPT_EXECUTE = 759,
    GM_ARRAY_CREATE = 183,
    GM_ARRAY_PUSH = 186,
    GM_ARRAY_LENGTH = 170,
    GM_ARRAY_COPY = 184,
    GM_STRUCT_GET = 1173,
    GM_VAR_GLOBAL_EXISTS = 1164,};

static char old_speaker[256] = "";
static bool in_cutscene;

static RValue cur_guard_ID;
static RValue curDialouge;
static RValue realGuardID;
static RValue realGuardList;
static RValue should_cleanup;
static bool first = true;
static char soundNames[64][128];
static int  soundCount = 0;

static RValue newAssetSetter;
static RValue instanceVarSetter;
static RValue audioPlayer;

static void show_gml_message(RValue message_contents){
    #ifdef DEBUG
    RValue dialouge_args[1] = {message_contents};
    RValue as_string = gm_call(265, dialouge_args, 1);
    RValue message_to_show[1] = {as_string};
    gm_call(893, message_to_show, 1);
    #endif
}


static void set_global(const char* target, const RValue& value){
    RValue setter_args[2] = {r_string(target), value};
    gm_call(GM_VAR_GLOBAL_SET, setter_args, 2);
}

static void set_global(const RValue& target, const RValue& value){
    RValue setter_args[2] = {target, value};
    gm_call(GM_VAR_GLOBAL_SET, setter_args, 2);
}

static const char* working_directory(){
    const char* p = *(const char**)(g_base + 0xd8ab10);
    return p ? p : "";
}

static RValue get_struct_value(const RValue& target, const RValue& key){
    RValue getter_args[2] = {target, key};
    return gm_call(GM_STRUCT_GET, getter_args, 2);
}

static RValue get_struct_value(const RValue& target, const char* key){
    RValue getter_args[2] = {target, r_string(key)};
    return gm_call(GM_STRUCT_GET, getter_args, 2);
}

static RValue typeOf(const RValue& value){
    RValue typingArg[1] = {value};
    return gm_call(167, typingArg, 1);
}

typedef void (*var_getter)(void* self, void* other, RValue* result, int array_index);

static RValue get_builtin_var(uintptr_t getter_rva){
    RValue out{};
    ((var_getter)(g_base + getter_rva))(nullptr, nullptr, &out, 0);  // result -> r8
    return out;
}
// getters:  working_directory 0x6ad60 · program_directory 0x6ab00 · temp_directory 0x6adb0

static RValue r_string2(const char* s){
    RValue r{};
    ((void(*)(RValue*, const char*))(g_base + 0x6acb0))(&r, s);
    return r;
}

static const char* game_save_id(){
    RValue r = get_builtin_var(0x6ad80);
    return (r.kind==RV_STRING && r.ptr) ? ((RefString*)r.ptr)->chars : "";
}

static RValue genAssetDirectory(const char* assetType, RValue guard){
    RValue filename = get_struct_value(newAssetSetter, "file_name");
    RValue filepath_maker_args[7] = {r_string(game_save_id()), r_string("addons\\"), guard, r_string("\\new_assets\\"), r_string(assetType), r_string("\\"),filename};
    
    RValue return_value = gm_call(GM_STRING_CONCAT, filepath_maker_args, 7);
    RValue file_check_args[1] = {return_value};

    return return_value;
}

static void sound_loop_set(RValue& sound){
    RValue sound_loop_start_args[]{sound, r_real(0)};
    RValue sound_length_get_args[]{sound};
    RValue sound_loop_end_args[]{sound, r_real(rv_to_double(gm_call(1463, sound_length_get_args, 1))-1)};
    gm_call(1477, sound_loop_start_args, 2);
    gm_call(1479, sound_loop_end_args, 2);
}

static RValue getGMLAsset(const RValue& name){
    RValue asset_index[1] = {name};
    return gm_call(835, asset_index, 1);
}

static void setObjectValue(const RValue& setter_struct){
    RValue object_name = get_struct_value(setter_struct, "object");
    RValue global_name = get_struct_value(setter_struct, "value");
    RValue variable_name = get_struct_value(setter_struct, "variable");

    RValue target_object = getGMLAsset(object_name);
    RValue setter_value = get_global(global_name);

    RValue raw_object_retrieve_args[1] = {target_object};
    RValue raw_object_id = gm_call(GM_INSTANCE_NUMBER, raw_object_retrieve_args, 1);
    RValue raw_instance_id = r_real(rv_to_double(raw_object_id) - 1.0);
    RValue raw_instance_get_args[2] = {target_object, raw_instance_id};
    RValue cur_object_instance_id = gm_call(GM_INSTANCE_FIND, raw_instance_get_args, 2);

    RValue raw_instance_set_args[3] = {cur_object_instance_id, variable_name, setter_value};
    gm_call(GM_VAR_INSTANCE_SET, raw_instance_set_args, 3);
}

static void copy_gm_string(char* dst, const RValue& s, size_t n){
    const char* p = (s.kind==RV_STRING && s.ptr) ? ((RefString*)s.ptr)->chars : "";
    strncpy(dst, p, n-1); dst[n-1] = '\0';
}

static void play_global_sound(const RValue& player_struct){
    RValue global_name = get_struct_value(player_struct, "audio");
    RValue gain = get_struct_value(player_struct, "gain");
    RValue pitch = get_struct_value(player_struct, "pitch");
    RValue priority = get_struct_value(player_struct, "prioity");
    RValue should_loop = get_struct_value(player_struct, "loop");

    RValue audio = get_global(global_name);

    RValue audio_play_args[6] = {audio, priority, should_loop, gain, r_real(0.0), pitch};
    gm_call(1435, audio_play_args, 6);
}

static void play_global_song(const RValue& player_struct){
    RValue global_name = get_struct_value(player_struct, "audio");
    RValue gain = get_struct_value(player_struct, "gain");
    RValue pitch = get_struct_value(player_struct, "pitch");

    RValue song = get_global(global_name);
    RValue get_playsong_args[]{r_string("play_music")};

    RValue song_args[4] = {spec_call(get_base_asset, get_playsong_args, 1), song, gain, pitch};
    gm_call(GM_SCRIPT_EXECUTE, song_args, 4);
}

static void stop_global_sound(const RValue& player_struct){
    RValue global_name = get_struct_value(player_struct, "audio");

    RValue sound = get_global(global_name);

    RValue stop_args[1] = {sound};
    gm_call(1440, stop_args, 1);
}

static void fade_global_music(const RValue& player_struct){
    RValue global_name = get_struct_value(player_struct, "audio");
    RValue volume = get_struct_value(player_struct, "volume");
    RValue time = get_struct_value(player_struct, "time");

    RValue music = get_global(global_name);
    
    RValue music_fade_args[3] = {music, volume, time};
    gm_call(1460, music_fade_args, 3);
}

static RValue get_guard_nickname(){
    RValue gn[1] = { r_string("guard_nickname") };
    RValue args[]{spec_call(get_base_asset, gn, 1)};
    return gm_call(GM_SCRIPT_EXECUTE, args, 1);
}

static RValue getGuardDirectory(){
    RValue filepath_maker_args[7] = {r_string(game_save_id()), r_string("addons\\"), get_guard_nickname(), r_string("\\")};
    
    RValue return_value = gm_call(GM_STRING_CONCAT, filepath_maker_args, 4);
    return return_value;
}

static RValue get_talksound_dir(){
    RValue concat_args[]{getGuardDirectory(), r_string("talkSounds")};
    return gm_call(GM_STRING_CONCAT, concat_args, 2);
}

static RValue get_textbox(){
    RValue raw_textbox_exists_args[1] = {raw_textbox_id};
    RValue raw_object_id = gm_call(GM_INSTANCE_NUMBER, raw_textbox_exists_args, 1);
    RValue raw_instance_id = r_real(rv_to_double(raw_object_id) - 1.0);
    RValue raw_instance_get_args[2] = {raw_textbox_id, raw_instance_id};
    return gm_call(GM_INSTANCE_FIND, raw_instance_get_args, 2);
}

static RValue array_get_index(const RValue& array, int index){
    RValue args[]{array, rv_from(index)};
    return gm_call(174, args, 2);
}

static RValue array_get_index(const RValue& array, const RValue& index){
    RValue args[]{array, index};
    return gm_call(174, args, 2);    
}

static RValue get_instance_var(const RValue& instance, const char* var_name){
    RValue name = r_string(var_name);
    RValue params[]{instance, name};
    return gm_call(1168, params, 2);
}

static RValue string_char_at(const RValue& string, const RValue& index){
    RValue args[]{string, index};
    return gm_call(278, args, 2);
}

static RValue get_current_character(){
    RValue textbox = get_textbox();
    RValue cur_page = get_instance_var(textbox, "TextPage");
    RValue char_index = rv_from(std::ceil(rv_to_double(get_instance_var(textbox, "TextDrawChar"))));
    RValue text_array = get_instance_var(textbox, "Text"); 
    RValue cur_text = array_get_index(text_array, cur_page);
    return string_char_at(cur_text, char_index);
}

static RValue get_current_speaker(){
    RValue textbox = get_textbox();
    RValue speaker_array = get_instance_var(textbox, "CharacterSpeaking");
    RValue cur_page = get_instance_var(textbox, "TextPage");
    return array_get_index(speaker_array, cur_page);
}

static int get_r_array_size(const RValue& array){
    RValue args[]{array};
    return rv_to_double(gm_call(170, args, 1));
}

static RValue generate_talksound_directory(const RValue& name){
    RValue args[]{r_string("\\talksounds\\"), name, r_string(".ogg")};
    return gm_call(GM_STRING_CONCAT, args, 3);
}

static RValue fb3_import_sound(const RValue& filepath){
    
    RValue args[]{r_string("import_sound")};
    RValue other_args[]{spec_call(get_base_asset, args, 1), get_global("Guard"), filepath};
    return gm_call(GM_SCRIPT_EXECUTE, other_args, 3);
}

static RValue get_current_song(){
    RValue raw_textbox_exists_args[1] = {r_real(429)};
    RValue raw_object_id = gm_call(GM_INSTANCE_NUMBER, raw_textbox_exists_args, 1);
    RValue raw_instance_id = r_real(rv_to_double(raw_object_id) - 1.0);
    RValue raw_instance_get_args[2] = {r_real(429), raw_instance_id};
    RValue music_player = gm_call(GM_INSTANCE_FIND, raw_instance_get_args, 2);
    return get_instance_var(music_player, "MusicPlaying");
}

static RValue generate_sub_asset_folder(const RValue& filepath){
    RValue concat_args[]{r_string("\\custom_assets\\"), filepath};
    return gm_call(GM_STRING_CONCAT, concat_args, 2);
}

static RValue create_audio_stream(const RValue& filepath){
    RValue args[]{filepath};
    return gm_call(1517, args, 1);
}

static void destroy_audio_stream(const RValue& audio){
    RValue args[]{audio};
    gm_call(1518, args, 1);
}

static std::string gm_string(const RValue& v){
    return (v.kind==RV_STRING && v.ptr) ? std::string(((RefString*)v.ptr)->chars) : std::string();
}

static void game_text(const RValue& text_id){
    RValue args[]{r_real(100968), text_id};
    gm_call(GM_SCRIPT_EXECUTE, args, 2);
}

static RValue create_array(){
    RValue array_maker_args[]{r_real(0)};
    return gm_call(GM_ARRAY_CREATE, array_maker_args, 0);
}

static void array_push(RValue& array, const RValue& value){
    RValue array_add_args[]{array, value};
    gm_call(GM_ARRAY_PUSH, array_add_args, 2);
}

static void array_insert(RValue& array, const RValue& value, const RValue& index){
    RValue array_insert_args[]{array, index, value};
    gm_call(189, array_insert_args, 3);
}

static RValue real_from_string(const RValue& string){
    RValue string_digits_args[]{string};
    RValue real_args[]{gm_call(289, string_digits_args, 1)};
    if(rv_equal(real_args[0], r_string(""))){
        return r_string("NAN");
    }
    return gm_call(263, real_args, 1);
}

static RValue no_args_call(int func_ID){
    RValue args[]{};
    return gm_call(func_ID, args, 0);
}

static RValue string_length(const RValue& string){
    RValue args[]{string};
    return gm_call(272, args, 1);
}

static RValue get_file_list(const RValue& directory){
    RValue file_first_concat_args[]{directory, r_string("\\*")};
    RValue file_first_args[]{gm_call(GM_STRING_CONCAT, file_first_concat_args, 2), r_real(16)};
    RValue folder_list = create_array();
    RValue file_first = gm_call(646, file_first_args, 2);
    while(!rv_equal(string_length(file_first), r_real(0))){
        RValue index = real_from_string(file_first);
        if(rv_equal(index, r_string("NAN"))){
            array_push(folder_list, file_first);
        }else{
            array_insert(folder_list, file_first, real_from_string(file_first));
        }
        
        file_first = no_args_call(647);
    }
    no_args_call(648);
    return folder_list;
}

static RValue for_each_gml(const RValue& array, RValue (*func)(const RValue&, int)){
    int array_size = get_r_array_size(array);
    RValue return_array = create_array();
    for(int index{}; index < array_size; index++){
        array_push(return_array, func(array_get_index(array, index), index));
    }
    return return_array;
}

//Note: This is not meant to convert the contents of the array, this is just meant to make an array for the purposes of a cleaner for loop.
static std::vector<RValue> gml_to_cpp_array(const RValue& array){
    int array_size = get_r_array_size(array);
    std::vector<RValue> cpp_array{};
    for(int index{}; index < array_size; index++){
        if(!rv_equal(array_get_index(array, index), r_real(0))){
            cpp_array.push_back(array_get_index(array, index));
        }
    }
    return cpp_array;
}

static RValue sprite_add(const RValue& fname){
    
    RValue args[]{fname, r_real(0), r_real(0), r_real(0), r_real(0), r_real(0)};
    return gm_call(707, args, 6);
}

static RValue sprite_add(const RValue& fname, const RValue& x_orig, const RValue& y_orig){
    RValue args[]{fname, r_real(0), r_real(0), r_real(0), y_orig, x_orig};
    return gm_call(707, args, 6);
}


static void sprite_merge(RValue& spr1, const RValue& spr2){
    RValue args[]{spr1, spr2};
    gm_call(716, args, 2);
}

static void sprite_delete(RValue spr){
    RValue args[]{spr};
    gm_call(713, args, 1);
}



static RValue get_asset_folder(){
    RValue concat_args[]{getGuardDirectory(), r_string("custom_assets")};
    return gm_call(GM_STRING_CONCAT, concat_args, 2);
}

static RValue get_asset_path(const RValue& asset_name){
    RValue concat_args[]{get_asset_folder(), r_string("\\"), asset_name};
    return gm_call(GM_STRING_CONCAT, concat_args, 3);
}

enum {
    IMAGE,
    SOUND,
    FONT,
    FOLDER,
    JSON,
    INVALID
};

static RValue string_pos(const RValue& substr, const RValue& string){
    RValue args[]{substr, string};
    return gm_call(273, args, 2);
}

static RValue string_last_substr(const RValue& string, const RValue& start_loc){
    RValue length = string_length(string);
    RValue char_total = r_real( rv_to_double(length)-rv_to_double(start_loc));
    RValue args[]{string, r_real(rv_to_double(start_loc)+1), char_total};
    return gm_call(277, args, 3);
}

static void font_delete(const RValue& font){
    RValue args[]{font};
    gm_call(756, args, 1);
}

static int get_file_extension(const RValue& file_name){
    RValue extension_loc = string_pos(r_string("."), file_name);
    
    if(rv_equal(extension_loc, r_real(0))){
        return FOLDER;
    }
    RValue extension_params = string_last_substr(file_name, extension_loc);
    if(rv_equal(extension_params, r_string("png"))){
        return IMAGE;
    }
    if(rv_equal(extension_params, r_string("ogg"))){
        return SOUND;
    }
    if(rv_equal(extension_params, r_string("ttf")) || rv_equal(extension_params, r_string("otf"))){
        return FONT;
    }
    if(rv_equal(extension_params, r_string("json"))){
        return JSON;
    }

    return INVALID;
}

static RValue drop_extension(const RValue& fileName){
    RValue extension_location = string_pos(r_string("."), fileName);
    RValue string_copy_args[]{fileName, r_real(1), r_real(rv_to_double(extension_location)-1)};
    return gm_call(277, string_copy_args, 3);
}

static RValue add_font(const RValue& filePath){
    RValue args[]{{r_real(0)}};
    gm_call(749, args, 1);
    RValue font_args[]{filePath, r_real(24), r_real(0), r_real(0), r_real(32), r_real(128)};
    return gm_call(751, font_args, 6);
}

static RValue sprite_to_font(const RValue& sprite, const RValue& charOrder){
    RValue args[]{{r_real(0)}};
    gm_call(749, args, 1);
    RValue new_font_args[]{sprite, charOrder, r_real(0), r_real(1)};
    return gm_call(753, new_font_args, 4);
}

static int get_asset_type(const RValue& asset){
    RValue type_getter[]{asset};
    return rv_to_double(gm_call(836, type_getter, 1));
}

static void set_instance_var(const RValue& instance, const char* name, const RValue& var){
    RValue args[]{instance, r_string(name), var};
    gm_call(1169, args, 3);
}



static RValue get_json_contents(const RValue& json){
    RValue buffer_args[]{json};
    RValue _buffer = gm_call(1662, buffer_args, 1);
    RValue read_args[]{_buffer, r_real(11)};
    RValue json_string = gm_call(1656, read_args, 2);
    RValue delete_buffer_args[]{_buffer};
    gm_call(1654, delete_buffer_args, 1);
    RValue return_args[]{json_string};
    return gm_call(679, return_args, 1);
}

static RValue get_sub_dir(const RValue& folder_name, const RValue& file_name){
    RValue concat_args[]{get_asset_folder(), r_string("\\"), folder_name, r_string("\\"), file_name};
    return gm_call(GM_STRING_CONCAT, concat_args, 5);
}

static void draw_set_font(const RValue& font){
    RValue call_args[]{font};
    gm_call(478, call_args, 1);
}

static std::unordered_map<std::string, RValue> custom_talksounds{};
static std::unordered_map<std::string, RValue> custom_assets{};

static bool is_custom_font(const RValue& font){
    for(const auto& [key, value] : custom_assets){
        if (rv_equal(font, value)) {
            return true;
        }
    }
    return false;
}

static RValue sprite_from_folder(const RValue& folder_name){
    RValue concat_args[]{getGuardDirectory(), r_string("custom_assets\\"), folder_name};
    RValue folder_directory = gm_call(GM_STRING_CONCAT, concat_args, 3);
    std::vector<RValue> file_names = gml_to_cpp_array(get_file_list(folder_directory));
    int index{0};
    int starting_index{0};
    RValue x_offset = r_real(0);
    RValue y_offset = r_real(0);
    bool is_font = false;
    RValue font_key{};
    if(get_file_extension(file_names[index]) == JSON){
        RValue json = get_json_contents(get_sub_dir(folder_name, file_names[starting_index]));
        x_offset = get_struct_value(json, "x_offset");
        y_offset = get_struct_value(json, "y_offset");
        is_font = rv_to_double(get_struct_value(json, "is_font")) == 1.0 ? true : false;
        if(is_font){
            font_key = get_struct_value(json, "font_key");
        }
        starting_index++;
    }
    RValue return_sprite = sprite_add(get_sub_dir(folder_name, file_names.at(starting_index)), x_offset, y_offset);
    for(RValue cur_file_name: file_names){
        if(index > starting_index){
            RValue adding_sprite = sprite_add(get_sub_dir(folder_name, file_names.at(index)), x_offset, y_offset);
            sprite_merge(return_sprite, adding_sprite);
            sprite_delete(adding_sprite);
        }
        index++;
    }
    if(is_font){
        RValue bounding_args[]{return_sprite, r_real(0), r_real(0), r_real(31), r_real(31)};
        gm_call(705, bounding_args, 5);

        RValue old_sprite = return_sprite;
        return_sprite = sprite_to_font(return_sprite, font_key);
        std::string sprite_name = gm_string(folder_name) + "_font";
        custom_assets.insert({sprite_name, old_sprite});
    }
    return return_sprite;
}

static void set_array_value(RValue& array, const RValue& index, const RValue& value){
    RValue args[]{array, index, value};
    gm_call(175, args, 3);
}

static RValue get_array_value(const RValue& array, const RValue& index){
    RValue args[]{array, index};
    return gm_call(174, args, 2);
}



static void process_custom_asset(const RValue& asset_path){
    switch (get_file_extension(asset_path)) {
        case FOLDER:{
            custom_assets.insert({gm_string(asset_path), sprite_from_folder(asset_path)});
            break;
        }
        case IMAGE:{
            custom_assets.insert({gm_string(drop_extension(asset_path)), sprite_add(get_asset_path(asset_path))});
            break;
        }
        case SOUND:{
            RValue sound_asset = fb3_import_sound(generate_sub_asset_folder(asset_path));
            sound_loop_set(sound_asset);
            custom_assets.insert({gm_string(drop_extension(asset_path)), sound_asset});
            break;
        }
        case FONT:{
            custom_assets.insert({gm_string(drop_extension(asset_path)), add_font(get_asset_path(asset_path))});
            break;
        }
        case JSON:{
            RValue gm_struct = get_json_contents(get_asset_path(asset_path));
            RValue target_file = get_struct_value(gm_struct, "target_file");
        }
    }
}

static gml_fn play_sound = nullptr;

static gml_fn instance_destroy = nullptr;
static gml_fn string_width = nullptr;


static void frame_update(void* self, void* other){}

static void cleanUp(void* self, void* other, RValue* args){
    for (const auto& [key, value] : custom_talksounds){
        destroy_audio_stream(value);
    }
    for(const auto& [key, value] : custom_assets){
        int asset_type = get_asset_type(value);
        switch (asset_type) {
            case 1:{
                sprite_delete(value);
                break;
            }
            case 2:{
                destroy_audio_stream(value);
                break;
            }
            case 6:{
                font_delete(value);
                break;
            }
        }
    }
    custom_assets.clear();
    custom_talksounds.clear();
}

static void hook_cleanup(RValue* result, void* self, void* other, int argc, RValue* args){
    if(g_orig_cleanup){
        g_orig_cleanup(result, self, other, argc, args);
    }
    if(rv_equal(get_textbox(), r_real(-4))){
        return;
    }
    static ULONGLONG last = 0;
    static bool inLogic = false;                     // re-entrancy guard
    ULONGLONG now = GetTickCount64();
    RValue cur_textbox_instance_id = get_textbox();
    RValue im_dead_inside[]{cur_textbox_instance_id, r_string("CustomTalksound")};
    RValue talkSound = gm_call(1168, im_dead_inside, 2);
    if (!inLogic && now - last >= HOOK_INTERVAL_MS && rv_equal(talkSound, args[0])){
        last = now; 
        inLogic = true;
        cleanUp(self, other, args);                        // self/other = the instance currently drawing (valid CInstance*)
        inLogic = false;
    }
    assets_loaded = false;
}

static void hook_step(RValue* result, void* self, void* other, int argc, RValue* args){
    if(g_orig_draw_sprite){
        g_orig_draw_sprite(result, self, other, argc, args);
    }
    if(rv_equal(get_textbox(), r_real(-4))){
        return;
    }

    static ULONGLONG last = 0;
    static bool inLogic = false;                     // re-entrancy guard
    ULONGLONG now = GetTickCount64();
    if (!inLogic && now - last >= HOOK_INTERVAL_MS){
        last = now; 
        inLogic = true;
        frame_update(self, other);                        // self/other = the instance currently drawing (valid CInstance*)
        inLogic = false;
    }
}

static void custom_func_loader(RValue* result, void* self, void* other, int argc, RValue* args){
    if(argc >= 1 && args[0].kind == RV_STRING){
        return;
    }else {
        instance_destroy(result, self, other, argc, args);
    }
}

static void talksound_override(RValue* result, void* self, void* other, int argc, RValue* args){
    if (rv_equal(get_textbox(), r_real(-4))) {
        play_sound(result, self, other, argc, args);
        return;
    }
    std::string cur_speaker = gm_string(get_current_speaker());
    if(rv_equal(get_instance_var(get_textbox(), "Talksound"), args[0])){
        
        if (
            rv_equal(get_current_character(), r_string(".")) || 
            rv_equal(get_current_character(), r_string(",")) ||
            rv_equal(get_current_character(), r_string("!")) ||
            rv_equal(get_current_character(), r_string("?"))) {
                play_sound(result, self, other, argc, args);
        }
        else if(custom_talksounds.count(cur_speaker) && argc == 6){
            RValue new_args[]{custom_talksounds.at(cur_speaker), args[1], args[2], args[3], args[4], args[5]};
            play_sound(result, self, other, argc, new_args);
        }
        else{
            play_sound(result, self, other, argc, args);
        }
    }else{
        play_sound(result, self, other, argc, args);
    }
}

static void change_font(const RValue& first_char_loc, const RValue& last_char_loc, const RValue font){
    RValue cur_page = get_instance_var(get_textbox(), "TextPage");
    RValue text_font = get_instance_var(get_textbox(), "TextFont");
    RValue array = get_instance_var(get_textbox(), "line_break_num");
    set_array_value(array, cur_page, r_real(0));
    int first_char = rv_to_double(first_char_loc);
    int last_char = rv_to_double(last_char_loc);
    set_instance_var(get_textbox(), "TextSetup", r_real(0));
    
    for(int index{first_char}; index <= last_char; index++){
        RValue current_text_array = array_get_index(text_font, index);
        set_array_value(current_text_array, r_real(rv_to_double(cur_page)), font);
    }
}

static void re_init(){
    if(!assets_loaded){
        set_global_real("frickmod_alive", 1.0);           // check this global to confirm main-thread writes
        RValue talksound_list = get_file_list(get_talksound_dir());
        int talksound_count = get_r_array_size(talksound_list);
        for(int ts_index{}; ts_index < talksound_count; ts_index++){
            RValue cur_talksound_file = drop_extension(array_get_index(talksound_list, ts_index));
            custom_talksounds.insert({gm_string(cur_talksound_file), fb3_import_sound(generate_talksound_directory(cur_talksound_file))});
        }
        RValue asset_list = get_file_list(get_asset_folder());
        int custom_asset_count = get_r_array_size(asset_list);
        for(int as_index{}; as_index < custom_asset_count; as_index++){
            RValue cur_asset_file = array_get_index(asset_list, as_index);
            process_custom_asset(cur_asset_file);
        }
        assets_loaded = true;
    }
}

static void manual_fade(const RValue& volume, const RValue& time){
    RValue args[]{get_current_song(), volume, time};
    gm_call(1460, args, 3);
}

static void replace_asset(const RValue& original_file_name, const RValue& new_file_name){
    RValue concat_one_args[]{getGuardDirectory(), original_file_name};

    RValue concat_two_args[]{get_asset_folder(), r_string("\\"), new_file_name};

    RValue processed_one = gm_call(GM_STRING_CONCAT, concat_one_args, 2);
    RValue processed_two = gm_call(GM_STRING_CONCAT, concat_two_args, 3);

    RValue delete_args[]{processed_one};
    RValue copy_args[]{processed_two, processed_one};

    gm_call(640, delete_args, 1);
    gm_call(642, copy_args, 2);
    
}

static void custom_asset_patch(RValue* result, void* self, void* other, int argc, RValue* args){
    re_init();
    if (argc >= 1 && args[0].kind == RV_STRING){
        std::string asset_name = gm_string(args[0]);
        auto it = custom_assets.find(asset_name);
        if (it != custom_assets.end()){
            *result = it->second;
            return;
        }
    }
    if (argc >= 1 && args[0].kind == RV_ARRAY) {
        std::string func_name = gm_string(array_get_index(args[0], 0));
        if (func_name == "font") {
            RValue font_asset_args[]{array_get_index(args[0], 3)};
            RValue font_asset = gm_call(835, font_asset_args, 1);
            change_font(array_get_index(args[0], 1), array_get_index(args[0], 2), font_asset);
        }

        if (func_name == "font_reset") {
            RValue font_asset = get_global(r_string("FontHanddrawn"));
            change_font(r_real(0), r_real(99), font_asset);
        }

        if (func_name == "init"){
            re_init();
        }

        if(func_name == "fadeMusic"){
            RValue target_vol = array_get_index(args[0], 1);
            RValue time = array_get_index(args[0], 2);
            manual_fade(target_vol, time);
        }

        if(func_name == "replace_asset"){
            RValue target_file = array_get_index(args[0], 1);
            RValue new_file = array_get_index(args[0], 2);
            replace_asset(target_file, new_file);
        }

        *result = array_get_index(args[0], 0);
        return;
    }
    get_base_asset(result, self, other, argc, args);
}

static void sprite_width_correct(RValue* result, void* self, void* other, int argc, RValue* args){
    if(is_custom_font(no_args_call(477))){
    }
    string_width(result, self, other, argc, args);
}

static void exec_hook(int id, void* self, void* other, int argc, RValue* args){
    RValue argss[]{r_real(id)};
    show_gml_message(gm_call(758, argss, 1));
    show_gml_message(r_real(id));
    switch (id){                     // id is the raw func_ids number (100035, etc.)
        case 90:                 // import_sound — EVERY call in the game lands here now
            show_gml_message(r_string("What fun, fun!"));
            g_orig_exec(id, self, other); 
            return;
        // case 100112: /* guard_nickname */ ...
        case 100090:                 // import_sound — EVERY call in the game lands here now
            show_gml_message(r_string("What fun, fun!"));
            g_orig_exec(id, self, other); 
            return;
        // case 100112: /* guard_nickname */ ...
    }
    g_orig_exec(id, self, other);   // not your target -> run the real script
}

static bool hookshot(int ID, uint8_t* table){
    void** slot = (void**)(table + (size_t)ID * 0x18 + 8);
    void* loc = *slot;
    DWORD old;
    if(ID == 1518 && loc != (void*)&hook_cleanup){
        g_orig_cleanup = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&hook_cleanup;
        VirtualProtect(slot, sizeof(void*), old, &old);
    } else if(ID == 1435 && loc != (void*)&talksound_override){
        play_sound = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&talksound_override;
        VirtualProtect(slot, sizeof(void*), old, &old);
    } else if(ID == 835 && loc != (void*)&custom_asset_patch){
        get_base_asset = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&custom_asset_patch;
        VirtualProtect(slot, sizeof(void*), old, &old);
    } else if(ID == 500 && loc != (void*)&hook_step){
        g_orig_draw_sprite = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&hook_step;
        VirtualProtect(slot, sizeof(void*), old, &old);
    } else if(ID == 113 && loc != (void*)&custom_func_loader){
        instance_destroy = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&custom_func_loader;
        VirtualProtect(slot, sizeof(void*), old, &old);
    } else if(ID == 483 && loc != (void*)&sprite_width_correct){
        string_width = (gml_fn)(*slot);
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = (void*)&sprite_width_correct;
        VirtualProtect(slot, sizeof(void*), old, &old);
    }
    

    return true;   // matched-and-installed, OR already-installed -> both are success
}

static bool install_hook(){
    uint8_t* table = *(uint8_t**)(g_base + FUNC_TABLE_RVA);
    if (!table) return false;

    if(!hookshot(1518, table) || 
    !hookshot(GM_DRAW_SPRITE, table) || 
    !hookshot(1435, table) ||
    !hookshot(835, table) ||
    !hookshot(113, table) ||
    !hookshot(483, table)){
        return false;
    }
    

    return true;
}

static void watchdog(){
    Sleep(1);
    uint8_t* table = *(uint8_t**)(g_base + FUNC_TABLE_RVA);
    if (!table) return;
    void** s500  = (void**)(table + (size_t)GM_DRAW_SPRITE * 0x18 + 8);
    void** s1518 = (void**)(table + (size_t)1518 * 0x18 + 8);
    void** s1435 = (void**)(table + (size_t)1435 * 0x18 + 8);
    void** s835 = (void**)(table + (size_t)835 * 0x18 + 8);
    void** s113 = (void**)(table + (size_t)113 * 0x18 + 8);
    void** s483 = (void**)(table + (size_t)483 * 0x18 + 8);
    if (*s500 != (void*)&hook_step || 
    *s1518 != (void*)&hook_cleanup || 
    *s1435 != (void*)&talksound_override ||
    *s835 != (void*)&custom_asset_patch ||
    *s483 != (void*)&sprite_width_correct)
        install_hook();
}

static DWORD WINAPI gm_worker(LPVOID){
    g_base = (uint8_t*)GetModuleHandleW(NULL);
    while (!*(uint8_t**)(g_base + FUNC_TABLE_RVA)) Sleep(50);   // WAIT, don't bail
    install_hook();
    // install on your worker thread, after g_base is set:
    MH_Initialize();
    MH_CreateHook((LPVOID)(g_base + 0x1402b75e0), (LPVOID)&exec_hook, (LPVOID*)&g_orig_exec);
    MH_EnableHook((LPVOID)(g_base + 0x1402b75e0));

    

    for(;;) watchdog();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID){
    if (reason == DLL_PROCESS_ATTACH){ 
        DisableThreadLibraryCalls(h); 
        CreateThread(NULL,0,gm_worker,NULL,0,NULL); 
    }
    return TRUE;
}

extern "C" __declspec(dllexport) double Init(void){ 

    return 0.0; 

}






