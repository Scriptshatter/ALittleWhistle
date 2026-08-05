#ifndef SHORTHAND
#define SHORTHAND
#include "GMGeneralLibrary.h"

namespace gml{
    template <typename T, typename F>
    inline void variable_global_set(T name, F value){gm_call(GM_VAR_GLOBAL_SET, name, value);}

    template <typename T>
    inline RValue variable_global_get(T name){ return gm_call(GM_VAR_GLOBAL_GET, name); }

    template <typename... T>
    inline void show_debug_message(T&&... contents){ gm_call(GM_SHOW_DEBUG_MESSAGE, std::forward<T>(contents)...); }

    template <typename... T>
    inline RValue string_concat(T&&... args){ return gm_call(GM_STRING_CONCAT, std::forward<T>(args)...); }

    template <typename T, typename F>
    inline RValue variable_struct_get(T target, F key){
        return gm_call(GM_STRUCT_GET, target, key);
    }

    template <typename T>
    inline RValue typeOf(T value){
        return gm_call(GM_TYPEOF, value);
    }

    template <typename T>
    inline RValue asset_get_index(T name){
        return gm_call(GM_ASSET_GET_INDEX, name);
    }

    inline RValue get_guard_nickname(){
        return gm_call_special("guard_nickname");
    }

    template <typename T, typename F, typename... G>
    inline RValue array_get_index(T array, F value, G&&... args){
        return gm_call(GM_ARRAY_GET_INDEX, array, value, std::forward<G>(args)...);
    }

    template <typename T, typename F>
    inline RValue variable_instance_get(T instance, F var_name){
        return gm_call(GM_VAR_INSTANCE_GET, instance, var_name);
    }

    template <typename T, typename F>
    inline RValue string_char_at(T string, F index){
        return gm_call(GM_STRING_CHAR_AT, string, index);
    }

    template <typename T>
    inline RValue array_length(T array){
        return gm_call(GM_ARRAY_LENGTH, array);
    }

    template <typename... T>
    inline RValue import_sound(T&&... args){
        return gm_call_special("import_sound", std::forward<T>(args)...);
    }

    template <typename T>
    inline RValue audio_create_stream(T filepath){
        return gm_call(GM_AUDIO_CREATE_STREAM, filepath);
    }

    template <typename T>
    inline void audio_destroy_stream(T audio){
        gm_call(GM_AUDIO_DESTROY_STREAM, audio);
    }

    template <typename T>
    inline void game_text(T text_id){
        gm_call_special("game_text", text_id);
    }

    inline RValue array_create(){
        return gm_call(GM_ARRAY_CREATE);
    }

    template <typename T, typename F>
    inline void array_push(T array, F value){
        gm_call(GM_ARRAY_PUSH, array, value);
    }

    template <typename T, typename F, typename G>
    inline void array_insert(T array, F value, G index){
        gm_call(GM_ARRAY_INSERT, array, value, index);
    }

    template <typename T>
    inline RValue string_length(T string){
        return gm_call(GM_STRING_LENGTH, string);
    }

    template <typename T, typename F, typename G, typename H, typename J, typename K>
    inline RValue sprite_add(T fname, F imgnum, G removeback, H smooth, J xorig, K yorig){
        return gm_call(GM_SPRITE_ADD, fname, imgnum, removeback, smooth, xorig, yorig);
    }

    template <typename T, typename F>
    inline void sprite_merge(T ind1, F ind2){
        gm_call(GM_SPRITE_MERGE, ind1, ind2);
    }

    template <typename T>
    inline RValue sprite_delete(T index){
        return gm_call(GM_SPRITE_DELETE, index);
    }

    template <typename T, typename F>
    inline RValue string_pos(T substr, F string){
        return gm_call(GM_STRING_POS, substr, string);
    }

    template <typename T>
    inline void font_delete(T font){
        gm_call(GM_FONT_DELETE, font);
    }

    template <typename T>
    inline void font_add_enable_aa(T enable){
        gm_call(GM_FONT_ADD_ENABLE_AA, enable);
    }

    template <typename T, typename F, typename G, typename H, typename J, typename K>
    inline RValue font_add(T name, F size, G bold, H italic, J first, K last){
        return gm_call(GM_FONT_ADD, name, size, bold, italic, first, last);
    }

    template <typename T, typename F, typename G, typename H>
    inline RValue sprite_to_font(T spr, F string_map, G prop, H sep){
        return gm_call(GM_FONT_ADD_SPRITE_EXT, spr, string_map, prop, sep);
    }

    template <typename T>
    inline RValue asset_get_type(T name_or_ref){
        return gm_call(GM_ASSET_GET_TYPE, name_or_ref);
    }

    template <typename T, typename F, typename G>
    inline void variable_instance_set(T instance_id, F name, G val){
        gm_call(GM_VAR_INSTANCE_SET, instance_id, name, val);
    }

    template <typename T>
    inline void draw_set_font(T font){
        gm_call(GM_DRAW_SET_FONT, font);
    }

    inline RValue guard_name(){
        return gm_call_special("guard_name");
    }

    template <typename T, typename F, typename G>
    inline void array_set(T array, F index, G value){
        gm_call(GM_ARRAY_SET, array, index, value);
    }

    template <typename T, typename F>
    static RValue array_get(T array, F index){
        return gm_call(GM_ARRAY_GET, array, index);
    }
}

#endif