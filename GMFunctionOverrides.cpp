#include <iostream>
#include <utility>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define __USE_MINGW_ANSI_STDIO 1
#include "GMGeneralLibrary.h"
#include "GMCustomAssetTools.h"
#include "GMFuncShorthands.h"
#include <string>
#include <unordered_map>
#include <winnls.h>
#include <windows.h>
#define DEBUG
#define DIALOUGE_OFFSET 168

std::unordered_map<std::string, RValue> placed_sprites{};

int new_quota = -1;
bool disable_override = false;

static bool readable(void* p) {
    return !IsBadReadPtr(p, sizeof(RValue));
}

//Depricated
void func_override(RValue* result, void* self, void* other, int argc, RValue* args){
    if(args[1].real == GM_FB_GET_UPGRADES){
        std::cout << "Found it!\n";
        RValue new_args[]{args[0], r_real(GM_WIN8_LIVETILE_TILE_NOTIFICATION)};
        cat::get_original_str("method")(result, self, other, argc, new_args);
        return;
    }
    cat::get_original_str("method")(result, self, other, argc, args);
}

//Depricated
void new_upgrades(RValue* result, void* self, void* other, int argc, RValue* args){
    std::cout << "Upgrades!\n";
    auto f = gm_impl(GM_SCRIPT_EXECUTE);
    RValue new_args[]{r_real(GM_FB_GET_UPGRADES), args[0]};
    f(result, self, other, argc + 1, new_args);
}

void custom_func_loader(RValue* result, void* self, void* other, int argc, RValue* args){
    if(argc >= 1 && args[0].kind == RV_STRING){
        return;
    }else {
        cat::get_original_str("instance_destroy")(result, self, other, argc, args);
    }
}

void set_salvage_var(RValue* result, void* self, void* other, int argc, RValue* args){
    if(new_quota > 0 && !disable_override){
        gml::variable_global_set("SalvageQuota", new_quota);

    }
    cat::get_original_str("window_set_cursor")(result, self, other, argc, args);
}

void talksound_override(RValue* result, void* self, void* other, int argc, RValue* args){
    if (args[0].kind == RV_UNDEF) {
        cat::get_original_str("audio_play_sound")(result, self, other, argc, args);
    }
    RValue music_name = gm_call(GM_AUDIO_GET_NAME, args[0]);
    RValue replcement_themes = gml::variable_struct_get(cat::get_alw_data(), "replacement_themes");
    if(music_name.kind != RV_UNDEF && replcement_themes.kind != RV_UNDEF && gm_call(GM_STRUCT_EXISTS, replcement_themes, music_name).real){
        RValue new_args[]{gml::asset_get_index(gml::variable_struct_get(replcement_themes, music_name)), args[1],  args[2], args[3], args[4], args[5]};
        cat::get_original_str("audio_play_sound")(result, self, other, argc, new_args);
        return;
    }
    if (rv_equal(cat::get_textbox(), r_real(-4))) {
        cat::get_original_str("audio_play_sound")(result, self, other, argc, args);
        return;
    }
    std::string cur_speaker = gm_string(cat::get_current_speaker());
    if(rv_equal(gml::variable_instance_get(cat::get_textbox(), r_string("Talksound")), args[0])){
        if (
            rv_equal(cat::get_current_character(), r_string(".")) || 
            rv_equal(cat::get_current_character(), r_string(",")) ||
            rv_equal(cat::get_current_character(), r_string("!")) ||
            rv_equal(cat::get_current_character(), r_string("?"))) {
                cat::get_original_str("audio_play_sound")(result, self, other, argc, args);
        }
        else if(custom_talksounds.count(cur_speaker) > 0 && argc == 6){
            RValue new_args[]{custom_talksounds.at(cur_speaker), args[1], args[2], args[3], args[4], args[5]};
            cat::get_original_str("audio_play_sound")(result, self, other, argc, new_args);
        }
        else{
            cat::get_original_str("audio_play_sound")(result, self, other, argc, args);
        }
    }else{
        cat::get_original_str("audio_play_sound")(result, self, other, argc, args);
    }
}

static void draw_asset(const RValue& asset_name, const std::string& asset_index, const RValue& x, const RValue& y){
    RValue new_sprite = gm_call(GM_INSTANCE_CREATE_DEPTH, x, y, gml::variable_instance_get(cat::get_textbox(), "depth").real-1, 244);
    gml::variable_instance_set(new_sprite, "sprite_index", gml::asset_get_index(asset_name));
    placed_sprites.insert({asset_index, new_sprite});
}

RValue eval_expression(const RValue& expression, void* self);

static std::pair<double, double> get_left_right(const RValue& expression, void* self){
    RValue left = eval_expression(gml::variable_struct_get(expression, "left"), self);
    RValue right = eval_expression(gml::variable_struct_get(expression, "right"), self);
    return std::pair<double, double>{left.real, right.real};
}

RValue eval_expression(const RValue& expression, void* self){
    gml::show_debug_message("Running...");
    gml::show_debug_message(expression);
    if(!(gm_string(gml::typeOf(expression)) == "struct" && gm_call(GM_STRUCT_EXISTS, expression, "alw_type").real != 0)){
        return expression;
    }
    std::cout << "Testing\n";
    RValue expression_type = gml::variable_struct_get(expression, "alw_type");
    if(expression_type.kind == RV_REAL){
        std::vector<RValue> func_args = cat::gml_to_cpp_array_full(gml::variable_struct_get(expression, "args"));
        RValue processed_args = gm_call(GM_NEW_ARRAY);
        for(RValue argument: func_args){
            std::cout << argument.real;
            gml::show_debug_message(argument);
            gml::array_push(processed_args, eval_expression(argument, self));
            gml::show_debug_message(processed_args);
        }
        return gm_call_sp(GM_SCRIPT_EXECUTE_EXT, self, expression_type, processed_args);
    }
    if(expression_type.kind == RV_STRING){
        std::string expression_type_string = gm_string(expression_type);

        if(expression_type_string.length() == 1){
            char exp_type_char = expression_type_string.at(0);
            switch (exp_type_char) {
                case '+':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first + numbers.second);
                }
                case '-':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first - numbers.second);
                }
                case '*':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first * numbers.second);
                }
                case '/':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first / numbers.second);
                }
                case '%':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first / numbers.second);
                }
                case '>':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first > numbers.second);
                }
                case '<':{
                    std::pair numbers = get_left_right(expression, self);
                    return r_real(numbers.first > numbers.second);
                }
                case '!':{
                    RValue condition = gml::variable_struct_get(expression, "value");
                    return r_real(!(eval_expression(condition, self).real));
                }
            }
        }

        if(expression_type_string == ">="){
            std::pair numbers = get_left_right(expression, self);
            return r_real(numbers.first >= numbers.second);
        }
        if(expression_type_string == "<="){
            std::pair numbers = get_left_right(expression, self);
            return r_real(numbers.first <= numbers.second);
        }
        if(expression_type_string == "=="){
            std::pair numbers = get_left_right(expression, self);
            return r_real(numbers.first == numbers.second);
        }
        if(expression_type_string == "&&"){
            std::pair numbers = get_left_right(expression, self);
            return r_real(numbers.first && numbers.second);
        }
        if(expression_type_string == "||"){
            std::pair numbers = get_left_right(expression, self);
            return r_real(numbers.first || numbers.second);
        }

        RValue data_func = gm_call(GM_ASSET_GET_INDEX, expression_type);
        std::vector<RValue> func_args = cat::gml_to_cpp_array_full(gml::variable_struct_get(expression, "args"));
        RValue processed_args = gm_call(GM_NEW_ARRAY);
        for(RValue argument: func_args){
            gml::array_push(processed_args, eval_expression(argument, self));
        }
        return gm_call_sp(GM_SCRIPT_EXECUTE_EXT, self, data_func, processed_args);
    }
    return expression;

}

void super_conditions(RValue* result, void* self, void* other, int argc, RValue* args){
    if (args[1].kind == RV_STRUCT_OR_METHOD) {
        RValue condition_args = args[1];
        *result = eval_expression(args[1], self);
    }else{
        cat::get_original_str("variable_instance_get")(result, self, other, argc, args);
    }
}

void get_dialouge(RValue* result, void* self, void* other, int argc, RValue* args){
    if(rv_equal(args[0], gml::variable_global_get("SalvagesCurrent")) && !rv_equal(cat::get_cursor(), r_real(-4))){
        gml::show_debug_message("Current text: ");
        RValue* cur_dialouge = (RValue*)((uint8_t*)args + DIALOUGE_OFFSET);
        std::string cur_dia_string = gm_string(*cur_dialouge);
        gml::show_debug_message(cur_dia_string);
        if( cat::dialouge_exists(r_string(cur_dia_string.c_str()))){
            
            cat::play_dialouge(r_string(cur_dia_string.c_str()), self, other);
            *cur_dialouge = r_string("farse");
            gml::show_debug_message(*cur_dialouge);
        }

    }

    cat::get_original_str("array_contains_ext")(result, self, other, argc, args);
}

void override_background(RValue* result, void* self, void* other, int argc, RValue* args){
    cat::get_original_str("show_debug_message")(result, self, other, argc, args);
    if(!rv_equal(cat::get_cutscene(), r_real(-4))){
        RValue self_struct = {.ptr = self, .flags = 0, .kind = RV_STRUCT_OR_METHOD};
        std::cout << "Showing debug message...\n";

        if(readable(self) && gm_call(GM_STRUCT_EXISTS, self_struct, "Conversation").real){
            RValue backgrounds = gml::variable_struct_get(cat::get_alw_data(), "backgrounds");

            if(self_struct.kind != RV_UNDEF && gm_call(GM_STRUCT_EXISTS, backgrounds, args[0]).real){
                RValue new_background = gml::variable_struct_get(backgrounds, args[0]);
                if(new_background.kind == RV_ARRAY && rv_equal(gml::variable_global_get(gml::array_get(new_background, 1)), gml::array_get(new_background, 2))){
                    gml::variable_instance_set(cat::get_cutscene(), "sprite_index", gml::asset_get_index(gml::array_get(new_background, 0)));
                }else if (new_background.kind == RV_STRING){
                    gml::variable_instance_set(cat::get_cutscene(), "sprite_index", gml::asset_get_index(new_background));
                }

                
            }
        }
    }

}

void custom_asset_patch(RValue* result, void* self, void* other, int argc, RValue* args){
    cat::re_init();
    if (argc >= 1 && args[0].kind == RV_STRING){
        std::string asset_name = gm_string(args[0]);
        auto it = custom_assets.find(asset_name);
        if (it != custom_assets.end()){
            *result = it->second;
            return;
        }
    }
    if (argc >= 1 && args[0].kind == RV_ARRAY) {
        std::string func_name = gm_string(gml::array_get(args[0], 0));

        if (func_name == "font") {
            RValue font_asset = gm_call(GM_ASSET_GET_INDEX, gml::array_get(args[0], 3));
            cat::change_font(gml::array_get(args[0], 1), gml::array_get(args[0], 2), font_asset);
        }

        if (func_name == "font_reset") {
            RValue font_asset = gml::variable_global_get("FontHanddrawn");
            cat::change_font(r_real(0), r_real(99), font_asset);
        }

        if (func_name == "init"){
            cat::re_init();
        }

        if(func_name == "fadeMusic"){
            RValue target_vol = gml::array_get(args[0], 1);
            RValue time = gml::array_get(args[0], 2);
            cat::manual_fade(target_vol, time);
        }

        if(func_name == "replace_asset"){
            RValue target_file = gml::array_get(args[0], 1);
            RValue new_file = gml::array_get(args[0], 2);
            cat::replace_asset(target_file, new_file);
        }

        if(func_name == "set_global"){
            RValue target_global = gml::array_get(args[0], 1);
            RValue new_value = gml::array_get(args[0], 2);
            gml::variable_global_set(target_global, new_value);
        }

        if(func_name == "draw_sprite"){
            std::cout << "Warning: Function not finished yet!\n";
        }

        if(func_name == "set_quota"){
            new_quota = gml::array_get(args[0], 1).real;
            RValue buffer = gm_call(GM_BUFFER_CREATE, gm_call(GM_STRING_BYTE_LENGTH, gm_call(GM_STRING, new_quota)).real+1, 0, 1);
            gm_call(GM_BUFFER_WRITE, buffer, 11, gm_call(GM_STRING, new_quota));
            gm_call(GM_BUFFER_SAVE, buffer, "new_quota.alw");
            gm_call(GM_BUFFER_DELETE, buffer);
        }

        if(func_name == "alw_func"){
            eval_expression(gml::array_get(args[0], 1), self);
        }

        *result = gml::array_get(args[0], 0);
        return;
    }
    cat::get_original_str("asset_get_index")(result, self, other, argc, args);
}

