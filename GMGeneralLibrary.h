
#ifndef ADSLIST
#define ADSLIST

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>


//Huge enum containing all currently needed function IDs
enum { 
    GM_INSTANCE_FIND=100,
    GM_INSTANCE_EXISTS=101, 
    GM_INSTANCE_NUMBER=102,
    GM_INSTANCE_NEAREST = 105,
    GM_INSTANCE_CREATE_DEPTH = 109,
    GM_INSTANCE_DESTROY=113,
    GM_ROOM_GOTO = 125,
    GM_METHOD = 160,
    GM_TYPEOF=167,
    GM_ARRAY_LENGTH = 170,
    GM_ARRAY_GET = 174,
    GM_ARRAY_SET = 175,
    GM_ARRAY_CREATE = 183,
    GM_ARRAY_COPY = 184,
    GM_ARRAY_PUSH = 186,
    GM_ARRAY_INSERT = 189,
    GM_ARRAY_SHUFFLE = 192,
    GM_ARRAY_GET_INDEX=198,
    GM_ARRAY_CONTAINS = 199,
    GM_ARRAY_CONTAINS_EXT = 200,
    GM_IRANDOM=220,
    GM_IRANDOM_RANGE = 221,
    GM_REAL=263,
    GM_STRING = 265,
    GM_STRING_LENGTH=272,
    GM_STRING_POS = 273,
    GM_STRING_COPY = 277,
    GM_STRING_CHAR_AT=278,
    GM_STRING_BYTE_LENGTH = 280,
    GM_STRING_DIGITS=289,
    GM_STRING_CONCAT = 305,
    GM_WINDOW_SET_CURSOR = 354,
    GM_DRAW_SET_FONT = 478,
    GM_DRAW_SPRITE = 500,
    GM_FILE_EXISTS = 639,
    GM_FILE_DELETE = 640,
    GM_FILE_COPY = 642,
    GM_FILE_FIND_FIRST = 646,
    GM_FILE_FIND_NEXT = 647,
    GM_FILE_FIND_CLOSE = 648,
    GM_JSON_PARSE = 679,
    GM_SPRITE_ADD = 707,
    GM_SPRITE_DELETE = 713,
    GM_SPRITE_MERGE = 716,
    GM_FONT_ADD_ENABLE_AA = 749,
    GM_FONT_ADD = 751,
    GM_FONT_ADD_SPRITE_EXT = 753,
    GM_FONT_DELETE = 756,
    GM_SCRIPT_EXECUTE = 759,
    GM_SCRIPT_EXECUTE_EXT = 760,
    GM_ASSET_GET_INDEX=835,
    GM_ASSET_GET_TYPE=836,
    GM_SHOW_MESSAGE = 893,
    GM_SHOW_DEBUG_MESSAGE=1151,
    GM_VAR_GLOBAL_EXISTS = 1164,
    GM_VAR_GLOBAL_GET=1165,
    GM_VAR_GLOBAL_SET=1166,
    GM_VAR_INSTANCE_GET=1168,
    GM_VAR_INSTANCE_SET=1169, 
    GM_STRUCT_GET = 1173,
    GM_STRUCT_EXISTS = 1182,
    GM_STRUCT_SET = 1185,
    GM_AUDIO_PLAY_SOUND = 1435,
    GM_AUDIO_SOUND_GAIN = 1460,
    GM_AUDIO_SOUND_LENGTH = 1463,
    GM_AUDIO_GET_NAME = 1472,
    GM_AUDIO_SOUND_LOOP_START=1477,
    GM_AUDIO_SOUND_LOOP_END=1479,
    GM_AUDIO_CREATE_STREAM = 1517,
    GM_AUDIO_DESTROY_STREAM = 1518,
    GM_BUFFER_CREATE = 1653,
    GM_BUFFER_DELETE = 1654,
    GM_BUFFER_WRITE = 1655,
    GM_BUFFER_READ = 1656,
    GM_BUFFER_SAVE = 1660,
    GM_BUFFER_LOAD = 1662,
    GM_GET_TIMER=1819,
    GM_NEW_ARRAY = 2342,
    GM_GLOBAL = 2344,
    GM_NULL_OBJECT = 2361,
    GM_WIN8_LIVETILE_TILE_NOTIFICATION = 2368,
    GM_FB_HANDLE_LINE = 100007,
    GM_FB_IMPORT_SOUND = 100036,
    GM_FB_GET_UPGRADES = 100090,
    GM_FB_GUARD_NICKNAME = 100113,
    GM_FB_GAME_TEXT_MIDNIGHT = 100723,
    GM_FB_ADD_PAGE = 100727,
    GM_FB_ADD_OPTION = 100729,
    GM_FB_ADD_SALVAGE_SHEET = 100734,
    GM_FB_SALVAGE_OPTIONS = 100736,
    GM_FB_GAME_TEXT = 100968,
    GM_FB_PLAY_MUSIC = 100975,
    GM_FB_TEXTBOX_INIT = 103063};

//Enum containing all currently needed object IDs
enum {
    GM_O_TEXTBOX = 62,
    GM_O_CAMERA_3D_MAZE = 107,
    GM_O_MIDNIGHT_CUTSCENE =  119,
    GM_O_INTERACTION_ANIMATRONIC = 336,
    GM_O_LOOPINGMUSICPLAYER = 429
};

//An enum containing keys to different values. I don't believe some of these are normally seen...
//That being said unknown is anything that isn't one of these values.
enum : uint32_t { 
    RV_REAL=0, 
    RV_STRING=1, 
    RV_ARRAY=2, 
    RV_PTR=3, 
    RV_VEC3=4, 
    RV_UNDEF=5,
    RV_STRUCT_OR_METHOD=6, //The typeof() is run and the value is type 6, gamemaker just checks if its callable to determine if it's a struct or method.
    RV_INT32=7, 
    RV_VEC4=8, 
    RV_VEC44=9, 
    RV_INT64=10, 
    RV_ACCESSOR=11,
    RV_NULL=12,
    RV_BOOL=13,
    RV_ITERATOR=14, 
    RV_HANDLE=15 };

//This is where we store the games location in memory
extern uint8_t* g_base;
extern uint8_t* table;

// Functions
// To use these you must first make a template for the function.
// Then, you must make a variable, and assign it to the location in the exe it stored in, plus the offset of where the game is running in memory.
// Then you may run the function through that variable.
static const uintptr_t ALLOC_RVA = 0x749620;   // The allocater function as defined in the EXE itself
typedef void* (*gm_alloc_t)(size_t size, void* a2, int64_t a3, int64_t flag); //The template for the function.

/*
RValues are a dynamic value that's interpreted differently depending on it's kind.
union: The value stored [8 bytes]
flags: A collection of 32 bools stored as a uint32 that hold information about the value [4 bytes, location is +0x08]
kind: How the backend should treat the value [4 bytes, location is +0x0C]
*/
struct RValue {
    union { double real; void* ptr; int32_t i32; int64_t i64; };
    uint32_t flags;
    uint32_t kind;
};

/*
RefStrings are rarely used outside of direct conversions between C/CPP strings and GML strings. They're basically and address and a zip+4 in one for strings.
chars: The address of the string in memory
refcount: How many things are refrencing this string
length: how long the string is in memory
*/
struct RefString { 
    char* chars; 
    int32_t refcount; 
    int32_t length; 
};

//If YOU set EITHER RValues or RefStrings to anything over or under 16 bytes EXACTLY I will PERSONALLY DELIVER you to GAY BABY JAIL.
static_assert(sizeof(RValue) == 16, "Malformed RValue; RValue's size was over 16 bytes.");
static_assert(sizeof(RefString) == 16, "Malformed RefString; RefString's size was over 16 bytes.");



//Functions from and for GMValueHandler.cpp
template <typename> inline constexpr bool rv_always_false = false;

// Makes a new RValue, sets its union (The value) to x, sets it's kind to a real, then returns it. Easy, simple, clean, covergirl.
inline RValue r_real(double x){ 
    RValue r{}; 
    r.real = x; 
    r.kind = RV_REAL; 
    return r; 
}

//First, it registeres an allocator function. Then, it uses the length of the string to make the
//refrence to the string, and the buffer where the string is stored. Then it stores the pointer to the refrence
//in the returned RValue.
inline RValue r_string(const char* s){
    gm_alloc_t alloc = (gm_alloc_t)(g_base + ALLOC_RVA);
    size_t len = strlen(s);
    RefString* rs = (RefString*)alloc(sizeof(RefString), nullptr, 0, 1); // struct, flag 1
    char* buf = (char*)alloc(len + 1, nullptr, 0, 0);               // chars, flag 0
    memcpy(buf, s, len + 1);
    rs->chars = buf; rs->refcount = 1; rs->length = (int32_t)len;
    RValue r{}; r.ptr = rs; r.kind = RV_STRING;
    return r;
}

//Basically just determines how it should interpret the value and what it should return.
inline double rv_to_double(const RValue& r){
    switch (r.kind){
        case RV_INT32: return (double)r.i32;
        case RV_INT64: return (double)r.i64;
        case RV_BOOL:  return r.real != 0.0 ? 1.0 : 0.0;
        default:       return r.real;
    }
}

inline std::string gm_string(const RValue& v){
    return (v.kind==RV_STRING && v.ptr) ? std::string(((RefString*)v.ptr)->chars) : std::string();
}

inline bool rv_is_num(uint32_t k){
    return k==RV_REAL || k==RV_INT32 || k==RV_INT64 || k==RV_BOOL;
}

inline bool rv_equal(const RValue& a, const RValue& b){
    // numeric vs numeric -> compare by value (5.0 == 5 == true, like GM)
    if (rv_is_num(a.kind) && rv_is_num(b.kind))
        return rv_to_double(a) == rv_to_double(b);

    // string vs string -> compare the TEXT (length + bytes), never the pointer
    if (a.kind==RV_STRING && b.kind==RV_STRING){
        RefString* ra = (RefString*)a.ptr; RefString* rb = (RefString*)b.ptr;
        if (!ra || !rb) return ra == rb;
        if (ra->length != rb->length) return false;
        return std::memcmp(ra->chars, rb->chars, (size_t)ra->length) == 0;
    }

    if (a.kind==RV_HANDLE && b.kind==RV_HANDLE)
        return a.i32 == b.i32;

    // array / struct / ptr of the same kind -> GM compares by REFERENCE (pointer)
    if (a.kind==b.kind && (a.kind==RV_ARRAY || a.kind==RV_STRUCT_OR_METHOD || a.kind==RV_PTR))
        return a.ptr == b.ptr;

    if (a.kind==RV_UNDEF && b.kind==RV_UNDEF) return true;   // both undefined
    return false;                                            // different / incompatible kinds
}

//For converting RValues into CPP values
template <typename T>
inline T rv_to(const RValue& v){
    if constexpr (std::is_same_v<T, RValue>)
        return v;                                                    // passthrough
    else if constexpr (std::is_arithmetic_v<T>)                      // int, double, float, bool, ...
        return static_cast<T>(rv_to_double(v));
    else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>)
        return (v.kind == RV_STRING && v.ptr) ? ((RefString*)v.ptr)->chars : "";
    else
        static_assert(rv_always_false<T>, "rv_to: unsupported type");
}

//The opposite of above
template <typename T>
inline RValue rv_from(T x){                                          // by value: string literals decay to const char*
    if constexpr (std::is_same_v<std::decay_t<T>, RValue>)
        return x;
    else if constexpr (std::is_enum_v<std::decay_t<T>>)          // <-- add this
        return r_real((double)static_cast<std::underlying_type_t<std::decay_t<T>>>(x));
    else if constexpr (std::is_arithmetic_v<std::decay_t<T>>)
        return r_real((double)x);
    else if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                       std::is_same_v<std::decay_t<T>, char*>)
        return r_string(x);
    else if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        return r_string(x.c_str());
    else
        static_assert(sizeof(T) == 0, "rv_from: unsupported type");
}







// Data tables
static const uintptr_t FUNC_TABLE_RVA = 0xd99ef8;   // All built-in GML functions. These are grabbed from FUN_140315b80.
typedef void  (*gml_fn)(RValue* result, void* self, void* other, int argc, RValue* args); //This is a template for calling GML functions.

// These value types are for handling override functions
struct function_hook{
    int id;
    gml_fn original;
    gml_fn override;
};

//Contains all override functions.
extern std::unordered_map<std::string, function_hook> override_functions;

//For Function Handling
inline gml_fn gm_impl(int id){
    uint8_t* table = *(uint8_t**)(g_base + FUNC_TABLE_RVA);
    return *(gml_fn*)(table + (size_t)id * 0x18 + 8);
}

//This function first makes sure you're using RValues
//Then it passes in the amount of elements in the array as the arg count
//If the id is over 100000, it basically just tacks on the ID to the start and executes script_execute.
template <typename... GMV>
inline RValue gm_call(int id, GMV&&... xs) {
    if (id < 0) return RValue{};

    constexpr int argc = sizeof...(xs);
    RValue arguments[argc > 0 ? argc : 1] = { rv_from(std::forward<GMV>(xs))... };

    RValue result{};

    if (id >= 100000) {
        RValue exec_args[argc + 1];
        exec_args[0] = r_real(id);
        for (int i = 0; i < argc; ++i) exec_args[i + 1] = arguments[i];
        auto f = gm_impl(GM_SCRIPT_EXECUTE);
        if (!f) return RValue{};                    // <-- don't call null
        f(&result, nullptr, nullptr, argc + 1, exec_args);
    } else {
        auto f = gm_impl(id);
        if (!f) return RValue{};                     // <-- don't call null
        f(&result, nullptr, nullptr, argc, argc ? arguments : nullptr);
    }
    return result;
}

template <typename... GMV>
inline RValue gm_call_sp(int id, void*& self, GMV&&... xs) {
    if (id < 0) return RValue{};

    constexpr int argc = sizeof...(xs);
    RValue arguments[argc > 0 ? argc : 1] = { rv_from(std::forward<GMV>(xs))... };

    RValue result{};

    if (id >= 100000) {
        RValue exec_args[argc + 1];
        exec_args[0] = r_real(id);
        for (int i = 0; i < argc; ++i) exec_args[i + 1] = arguments[i];
        auto f = gm_impl(GM_SCRIPT_EXECUTE);
        if (!f) return RValue{};                    // <-- don't call null
        f(&result, self, nullptr, argc + 1, exec_args);
    } else {
        auto f = gm_impl(id);
        if (!f) return RValue{};                     // <-- don't call null
        f(&result, self, nullptr, argc, argc ? arguments : nullptr);
    }
    return result;
}

template <typename... GMV>
inline RValue gm_call_full(int id, void*& self, void*& other, GMV&&... xs) {
    if (id < 0) return RValue{};

    constexpr int argc = sizeof...(xs);
    RValue arguments[argc > 0 ? argc : 1] = { rv_from(std::forward<GMV>(xs))... };

    RValue result{};

    if (id >= 100000) {
        RValue exec_args[argc + 1];
        exec_args[0] = r_real(id);
        for (int i = 0; i < argc; ++i) exec_args[i + 1] = arguments[i];
        auto f = gm_impl(GM_SCRIPT_EXECUTE);
        if (!f) return RValue{};                    // <-- don't call null
        f(&result, self, other, argc + 1, exec_args);
    } else {
        auto f = gm_impl(id);
        if (!f) return RValue{};                     // <-- don't call null
        f(&result, self, other, argc, argc ? arguments : nullptr);
    }
    return result;
}

template <typename... GMV>
inline RValue gm_call_special(std::string func_id, GMV&&... xs){
    RValue func_index = gm_call(GM_ASSET_GET_INDEX, func_id);
    return gm_call(GM_SCRIPT_EXECUTE, func_index, std::forward<GMV>(xs)...);
}

template <typename... GMV>
inline RValue gm_call_special_self(std::string func_id, void* self, GMV&&... xs){
    RValue func_index = gm_call(GM_ASSET_GET_INDEX, func_id);
    return gm_call_sp(GM_SCRIPT_EXECUTE, self, func_index, std::forward<GMV>(xs)...);
}

#define CUR_FRAME (*(uint8_t**)(g_base + 0xd99f18))
//   Script table + count (DAT_140b7c110 / DAT_140b7c0f0).
#define SCRIPT_TABLE (*(uint8_t***)(g_base + 0xb7c110))
#define SCRIPT_COUNT (*(int*)      (g_base + 0xb7c0f0))
 
// --- identity gate: are we inside a specific script? ---
// NOTE: pass the RUNTIME id (game_text = 100967), NOT the +1 data.win value.
// (Your enum's GM_FB_GAME_TEXT = 100968 is the data.win-space value; subtract 1.)
inline uint8_t* g_target_code_obj = nullptr;
 
inline void cache_target(int runtime_id){
    int idx = runtime_id - 100000;
    if (idx < 0 || idx >= SCRIPT_COUNT) return;
    uint8_t* cs = SCRIPT_TABLE[idx];
    if (cs) g_target_code_obj = *(uint8_t**)(cs + 0x08);   // CScript+0x08 = code obj
}
inline bool in_target(void* ctx){
    return ctx && g_target_code_obj &&
           *(uint8_t**)((uint8_t*)ctx + 0x38) == g_target_code_obj;  // ctx+0x38 = code obj
}
 
// --- the scan ---
// LOG: replace with your DLL's logger (you've used gml::show_debug_message /
// a file sink before). Signature assumed printf-style.
#ifndef LOG
#define LOG(...) /* wire this to your logger */
#endif
 
inline void dump_frame_strings(uint8_t* frame){
    for (int off = -0x40; off <= 0xC0; off += (int)sizeof(RValue)){
        RValue* r = (RValue*)(frame + off);
        if ((r->kind & 0xffffff) != RV_STRING) continue;   // only real strings
 
        RefString* rs = (RefString*)r->ptr;
        if (!rs || !rs->chars) continue;
 
        int len = rs->length; if (len < 0 || len > 128) len = 128;  // clamp bad reads
        LOG("frame%+d (0x%x): len=%d \"%.*s\"", off, off, rs->length, len, rs->chars);
    }
    LOG("---- end frame dump ----");
}

#endif