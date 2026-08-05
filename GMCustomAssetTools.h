#ifndef TOOLS
#define TOOLS
#include "GMGeneralLibrary.h"
#include <cmath>
#include <iostream>
#include <vector>
#include "GMFuncShorthands.h"

static std::unordered_map<std::string, RValue> custom_talksounds{};
static std::unordered_map<std::string, RValue> custom_assets{};
static bool assets_loaded = false;

namespace cat{
    typedef void (*var_getter)(void* self, void* other, RValue* result, int array_index);

    inline RValue get_builtin_var(uintptr_t getter_rva){
        RValue out{};
        ((var_getter)(g_base + getter_rva))(nullptr, nullptr, &out, 0);  // result -> r8
        return out;
    }

    inline const char* game_save_id(){
        RValue r = get_builtin_var(0x6ad80);
        return (r.kind==RV_STRING && r.ptr) ? ((RefString*)r.ptr)->chars : "";
    }

    inline const char* working_directory(){
        const char* p = *(const char**)(g_base + 0xd8ab10);
        return p ? p : "";
    }

    inline void sound_loop_set(RValue& sound){
        gm_call(GM_AUDIO_SOUND_LOOP_START, sound, 0);
        gm_call(GM_AUDIO_SOUND_LOOP_END, sound, gm_call(GM_AUDIO_SOUND_LENGTH, sound).real-1);
    }

    inline RValue getGuardDirectory(){
        return gml::string_concat(game_save_id(), "addons\\", gml::guard_name(), "\\");
    }

    inline RValue get_talksound_dir(){
        return gm_call(GM_STRING_CONCAT, getGuardDirectory(), "talkSounds");
    }

    inline RValue get_textbox(){
        return gm_call(GM_INSTANCE_FIND, GM_O_TEXTBOX, gm_call(GM_INSTANCE_NUMBER, GM_O_TEXTBOX).real-1);
    }

    inline RValue get_cursor(){
        return gm_call(GM_INSTANCE_FIND, GM_O_CAMERA_3D_MAZE, gm_call(GM_INSTANCE_NUMBER, GM_O_CAMERA_3D_MAZE).real-1);
    }

    inline RValue get_cutscene(){
        return gm_call(GM_INSTANCE_FIND, GM_O_MIDNIGHT_CUTSCENE, gm_call(GM_INSTANCE_NUMBER, GM_O_MIDNIGHT_CUTSCENE).real-1);
    }

    inline RValue get_current_character(){
        RValue textbox = get_textbox();
        RValue cur_page = gml::variable_instance_get(textbox, "TextPage");
        RValue char_index = rv_from(std::ceil(gml::variable_instance_get(textbox, "TextDrawChar").real));
        RValue text_array = gml::variable_instance_get(textbox, "Text"); 
        RValue cur_text = gml::array_get(text_array, cur_page);
        return gml::string_char_at(cur_text, char_index);
    }

    inline RValue get_current_speaker(){
        RValue textbox = get_textbox();
        RValue speaker_array = gml::variable_instance_get(textbox, "CharacterSpeaking");
        RValue cur_page = gml::variable_instance_get(textbox, "TextPage");
        return gml::array_get(speaker_array, cur_page);
    }

    inline RValue generate_talksound_directory(const RValue& name){
        return gm_call(GM_STRING_CONCAT, "\\talksounds\\", name, ".ogg");
    }

    inline RValue get_current_song(){
        RValue raw_object_id = gm_call(GM_INSTANCE_NUMBER, GM_O_LOOPINGMUSICPLAYER);
        RValue music_player = gm_call(GM_INSTANCE_FIND, GM_O_LOOPINGMUSICPLAYER, raw_object_id.real - 1.0);
        return gml::variable_instance_get(music_player, "MusicPlaying");
    }

    inline RValue generate_sub_asset_folder(const RValue& filepath){
        return gm_call(GM_STRING_CONCAT, "\\custom_assets\\", filepath);
    }

    inline RValue real_from_string(const RValue& string){
        RValue string_digits{gm_call(GM_STRING_DIGITS, string)};
        if(rv_equal(string_digits, r_string(""))){
            return r_string("NAN");
        }
        return gm_call(GM_REAL, string_digits);
    }

    inline RValue get_file_list(const RValue& directory){
        RValue folder_list = gml::array_create();
        RValue file_first = gm_call(GM_FILE_FIND_FIRST, gml::string_concat(directory, "\\*"), 16);
        std::cout << "Getting file list...\n";
        while(!rv_equal(gml::string_length(file_first), r_real(0))){
            RValue index = real_from_string(file_first);
            if(rv_equal(index, r_string("NAN"))){
                gml::array_push(folder_list, file_first);
            }else{
                gml::array_set(folder_list, real_from_string(file_first), file_first);
            }
            gml::show_debug_message(file_first);
            
            file_first = gm_call(GM_FILE_FIND_NEXT);
        }
        gm_call(GM_FILE_FIND_CLOSE);
        return folder_list;
    }

    inline std::vector<RValue> gml_to_cpp_array(const RValue& array){
        std::cout << "Starting to convert array...\n";
        int array_size = gml::array_length(array).real;
        std::cout << "Got array length...\n";
        std::vector<RValue> cpp_array{};
        std::cout << "beginning for loop\n";
        for(int index{}; index < array_size; index++){
            std::cout << "Checking if current index is null...\n";
            if(!rv_equal(gml::array_get(array, index), r_real(0))){
                std::cout << "adding to array...\n";
                gml::show_debug_message(gml::array_get(array, index));
                cpp_array.push_back(gml::array_get(array, index));
            }
        }
        return cpp_array;
    }

    inline std::vector<RValue> gml_to_cpp_array_full(const RValue& array){
        std::cout << "Starting to convert array...\n";
        int array_size = gml::array_length(array).real;
        std::cout << "Got array length...\n";
        std::vector<RValue> cpp_array{};
        std::cout << "beginning for loop\n";
        for(int index{}; index < array_size; index++){
            std::cout << "Checking if current index is null...\n";
            std::cout << "adding to array...\n";
            gml::show_debug_message(gml::array_get(array, index));
            cpp_array.push_back(gml::array_get(array, index));
        }
        return cpp_array;
    }

    inline RValue get_asset_folder(){
        return gm_call(GM_STRING_CONCAT, getGuardDirectory(), "custom_assets");
    }

    inline RValue get_asset_path(const RValue& asset_name){
        return gm_call(GM_STRING_CONCAT, get_asset_folder(), "\\", asset_name);
    }

    inline RValue string_last_substr(const RValue& string, const RValue& start_loc){
        RValue length = gml::string_length(string);
        return gm_call(GM_STRING_COPY, string, start_loc.real+1, length.real - start_loc.real);
    }

    //For determining file type
    enum {
        IMAGE,
        SOUND,
        FONT,
        FOLDER,
        JSON,
        INVALID
    };

    inline int get_file_extension(const RValue& file_name){
        RValue extension_loc = gml::string_pos(".", file_name);
        
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

    inline RValue drop_extension(const RValue& fileName){
        RValue extension_location = gml::string_pos(".", fileName);
        return gm_call(GM_STRING_COPY, fileName, 1, extension_location.real-1);
    }

    inline RValue get_json_contents(const RValue& json){
        if(gm_call(GM_FILE_EXISTS, json).real == 0){
            return gm_call(GM_NULL_OBJECT);
        }

        RValue _buffer = gm_call(GM_BUFFER_LOAD, json);
        RValue json_string = gm_call(GM_BUFFER_READ, _buffer, 11);
        gm_call(GM_BUFFER_DELETE, _buffer);
        return gm_call(GM_JSON_PARSE, json_string);
    }

    inline RValue get_alw_data(){
        RValue current_data = get_json_contents(gml::string_concat(getGuardDirectory(), "alw_character_data.json"));

        if(gm_call(GM_STRUCT_EXISTS, current_data, "bb_questions").real == 0){
            gm_call(GM_STRUCT_SET, current_data, "bb_questions", gm_call(GM_NEW_ARRAY));
        }

        if(gm_call(GM_STRUCT_EXISTS, current_data, "backgrounds").real == 0){
            gm_call(GM_STRUCT_SET, current_data, "backgrounds", gm_call(GM_NULL_OBJECT));
        }

        if(gm_call(GM_STRUCT_EXISTS, current_data, "replacement_themes").real == 0){
            gm_call(GM_STRUCT_SET, current_data, "replacement_themes", gm_call(GM_NULL_OBJECT));
        }

        return current_data;
    }

    inline RValue get_sub_dir(const RValue& folder_name, const RValue& file_name){
        return gml::string_concat(get_asset_folder(), "\\", folder_name, "\\", file_name);
    }

    inline bool is_custom_font(const RValue& font){
        for(const auto& [key, value] : custom_assets){
            if (rv_equal(font, value)) {
                return true;
            }
        }
        return false;
    }

    inline RValue sprite_from_folder(const RValue& folder_name){
        RValue folder_directory = gml::string_concat(getGuardDirectory(), "custom_assets\\", folder_name);
        std::cout << "Now attempting to convert gml array to cpp array...\n";
        std::vector<RValue> file_names = gml_to_cpp_array(get_file_list(folder_directory));
        int index{0};
        int starting_index{0};

        bool is_font = false;
        RValue font_key{};
        RValue x_offset = r_real(0);
        RValue y_offset = r_real(0);

        std::cout << "trying to read json...\n";
        if(get_file_extension(file_names[index]) == JSON){
            RValue json = get_json_contents(get_sub_dir(folder_name, file_names[starting_index]));
            x_offset = gml::variable_struct_get(json, "x_offset");
            y_offset = gml::variable_struct_get(json, "y_offset");
            is_font = gml::variable_struct_get(json, "is_font").real == 1.0 ? true : false;
            if(is_font){
                font_key = gml::variable_struct_get(json, "font_key");
            }
            starting_index++;
        }
        std::cout << "Adding sprites together...\n";
        RValue return_sprite = gml::sprite_add(get_sub_dir(folder_name, file_names.at(starting_index)),1, 0, 0, x_offset, y_offset);
        for(RValue cur_file_name: file_names){
            if(index > starting_index){
                RValue adding_sprite = gml::sprite_add(get_sub_dir(folder_name, file_names.at(index)),1, 0, 0, x_offset, y_offset);
                gml::sprite_merge(return_sprite, adding_sprite);
                gml::sprite_delete(adding_sprite);
            }
            index++;
        }
        if(is_font){
            RValue old_sprite = return_sprite;
            gml::font_add_enable_aa(0);
            return_sprite = gml::sprite_to_font(return_sprite, font_key, 0, 1);
            std::string sprite_name = gm_string(folder_name) + "_font";
            custom_assets.insert({sprite_name, old_sprite});
        }
        return return_sprite;
    }

    inline void process_custom_asset(const RValue& asset_path){
        switch (get_file_extension(asset_path)) {
            case FOLDER:{
                custom_assets.insert({gm_string(asset_path), sprite_from_folder(asset_path)});
                break;
            }
            case IMAGE:{
                custom_assets.insert({gm_string(drop_extension(asset_path)), gml::sprite_add(get_asset_path(asset_path), 1, 0, 0, 0, 0)});
                break;
            }
            case SOUND:{
                RValue sound_asset = gml::import_sound(gml::variable_global_get("guard"), generate_sub_asset_folder(asset_path));
                sound_loop_set(sound_asset);
                custom_assets.insert({gm_string(drop_extension(asset_path)), sound_asset});
                break;
            }
            case FONT:{
                gml::font_add_enable_aa(0);
                custom_assets.insert({gm_string(drop_extension(asset_path)), gml::font_add(get_asset_path(asset_path), 24, 0, 0, 32, 128)});
                break;
            }
            case JSON:{
                RValue gm_struct = get_json_contents(get_asset_path(asset_path));
                RValue target_file = gml::variable_struct_get(gm_struct, "target_file");
            }
        }
    }

    inline void change_font(const RValue& first_char_loc, const RValue& last_char_loc, const RValue font){
        RValue cur_page = gml::variable_instance_get(get_textbox(), "TextPage");
        RValue text_font = gml::variable_instance_get(get_textbox(), "TextFont");
        RValue array = gml::variable_instance_get(get_textbox(), "line_break_num");
        gml::array_set(array, cur_page, 0);
        int first_char = first_char_loc.real;
        int last_char = last_char_loc.real;
        gml::variable_instance_set(get_textbox(), "TextSetup", 0);
        for(int index{first_char}; index <= last_char; index++){
            RValue current_text_array = gml::array_get(text_font, index);
            gml::array_set(current_text_array, cur_page, font);
        }
    }

    inline void re_init(){
        if(!assets_loaded){
            assets_loaded = true;
            gml::variable_global_set("frickmod_alive", 1.0);           // check this global to confirm main-thread writes
            RValue talksound_list = get_file_list(get_talksound_dir());
            RValue talksound_count = gml::array_length(talksound_list);
            std::cout << "Loading custom talksounds...\n";
            for(int ts_index{}; ts_index < talksound_count.real; ts_index++){
                RValue cur_talksound_file = drop_extension(gml::array_get(talksound_list, ts_index));
                std::cout << "Dropped extension\n";
                custom_talksounds.insert({gm_string(cur_talksound_file), gml::import_sound(gml::variable_global_get("guard"),generate_talksound_directory(cur_talksound_file))});
            }
            std::cout << "Loading custom assets...\n";
            RValue asset_list = get_file_list(get_asset_folder());
            int custom_asset_count = gml::array_length(asset_list).real;
            for(int as_index{}; as_index < custom_asset_count; as_index++){
                RValue cur_asset_file = gml::array_get(asset_list, as_index);
                process_custom_asset(cur_asset_file);
            }
            std::cout << "Custom assets finished!";
        }
    }

    inline void manual_fade(const RValue& volume, const RValue& time){
        gm_call(GM_AUDIO_SOUND_GAIN, get_current_song(), volume, time);
    }

    inline void cleanUp(){
        if(assets_loaded){
            for (const auto& [key, value] : custom_talksounds){
                gml::audio_destroy_stream(value);
            }
            for(const auto& [key, value] : custom_assets){
                int asset_type = gml::asset_get_type(value).real;
                switch (asset_type) {
                    case 1:{
                        gml::sprite_delete(value);
                        break;
                    }
                    case 2:{
                        gml::audio_destroy_stream(value);
                        break;
                    }
                    case 6:{
                        gml::font_delete(value);
                        break;
                    }
                }
            }
            custom_assets.clear();
            custom_talksounds.clear();
            assets_loaded = false;
        }
    }

    inline gml_fn get_original(int id){
        for (auto& [name, hook] : override_functions)
            if (hook.id == id) return hook.original;
        return nullptr;
    }

    inline gml_fn get_original_str(std::string id){
        if (override_functions.count(id) > 0) {
            return override_functions.at(id).original;
        }
        else{
            gml::show_debug_message("Warning: ", id.c_str(), " is not registered!");
        }
        return nullptr;
    }

    inline void replace_asset(const RValue& original_file_name, const RValue& new_file_name){
        gm_call(GM_FILE_DELETE, gml::string_concat(getGuardDirectory(), original_file_name));
        gm_call(GM_FILE_COPY, gml::string_concat(get_asset_folder(), r_string("\\"), new_file_name), gml::string_concat(getGuardDirectory(), original_file_name));
        
    }

    inline RValue get_all_dialouge_overrides(){
        RValue directory = gml::string_concat(getGuardDirectory(), "dialogue");
        gml::show_debug_message("Getting files...");
        gml::show_debug_message(directory);
        RValue folder_list = gml::array_create();
        RValue file_first = gm_call(GM_FILE_FIND_FIRST, gml::string_concat(directory, "\\*"), 16);
        std::cout << "Getting file list...\n";
        while(!rv_equal(gml::string_length(file_first), r_real(0))){
            gml::array_push(folder_list, drop_extension(file_first));
            file_first = gm_call(GM_FILE_FIND_NEXT);
            gml::show_debug_message(file_first);
        }
        gm_call(GM_FILE_FIND_CLOSE);
        return folder_list;
    }

    inline bool dialouge_exists(RValue dia_name){
        return gm_call(GM_ARRAY_CONTAINS, get_all_dialouge_overrides(), dia_name).real != 0 ? true : false;
    }

    inline void ask_question(RValue index, void* self){
        RValue char_data = get_alw_data();
        RValue questions = gml::variable_struct_get(char_data, "bb_questions");
        questions = gml::array_get(questions, index);
        questions = gml::array_get(questions, gm_call(GM_IRANDOM_RANGE, 0 , gml::array_length(questions).real-1));
        questions = gm_call(GM_ARRAY_SHUFFLE, questions);

        std::vector<RValue> cpp_questions = gml_to_cpp_array(questions);
        for (RValue answer: cpp_questions) {
            if(gml::array_length(answer).real == 3){
                gm_call_special_self("add_page", self, gml::array_get(answer, 0), gml::array_get(answer, 1));
            }else{
                gm_call_special_self("add_option", self, gml::array_get(answer, 0), gml::array_get(answer, 1));
            }
        }
    }

    inline void do_line(RValue line, void*& self, void*& other, RValue dia_name){
        gml::show_debug_message(line);
        if(rv_equal(gml::array_get(line, 0), r_string("_turn_camera"))){
            gm_call_special_self("add_function", self, gml::asset_get_index("anon@17679@game_text_TextboxScripts"));
            return;
        }
        if(rv_equal(gml::array_get(line, 0), r_string("_replace_text"))){
            gm_call_special_self("replace_text", self, dia_name,  gml::array_get(line, 1));
            return;
        }
        if(rv_equal(gml::array_get(line, 0), r_string("_replace_text_ext"))){
            gm_call_special_self("replace_text", self, gml::array_get(line, 1),  gml::array_get(line, 2));
            return;
        }
        if(rv_equal(gml::array_get(line, 0), r_string("_bb_ask"))){
            ask_question(gml::array_get(line, 1), self);
            return;
        }
        if(rv_equal(gml::array_get(line, 0), r_string("_scan_array"))){
            gml::variable_global_set("array_result", gml::array_get(gml::variable_global_get(gml::array_get(line, 1)), gml::array_get(line, 2)));
            return;
        }
        //anon@17679@game_text_TextboxScripts
        //gm_call_sp(GM_FB_HANDLE_LINE, self, line, gm_call(GM_NULL_OBJECT));
        gm_call_special_self("handle_line", self, line, gm_call(GM_NULL_OBJECT));
    }

    inline void play_dialouge(RValue dialouge_name, void*& self, void*& other){
        RValue dialouge_path = gml::string_concat(getGuardDirectory(), "dialogue\\", dialouge_name, ".json");
        gml::show_debug_message(dialouge_path);
        
        RValue dialouge_struct = get_json_contents(dialouge_path);
        if(!gm_call(GM_STRUCT_EXISTS, dialouge_struct, "animatronic_id").real){
            gm_call(GM_STRUCT_SET, dialouge_struct, "animatronic_id", -1);
        }
        int anim_ID = gml::variable_struct_get(dialouge_struct, "animatronic_id").real;

        std::vector<RValue> dialouge_tree = gml_to_cpp_array(gm_call(GM_STRUCT_GET, dialouge_struct, "DIALOGUE"));
        std::cout << "Converted Array\n";
        if(gml::array_length(gml::variable_global_get("SalvagesCurrent")).real >= gml::variable_global_get("SalvageQuota").real &&
            anim_ID > -1 && (
                anim_ID != 20 &&
                anim_ID != 33 &&
                anim_ID != 34 &&
                anim_ID != 35
            )){
                gm_call_special_self("add_page", self, "Your quota's already been met! No point in salvaging this one.", "");
        }else{
            if(gm_call(GM_ARRAY_CONTAINS, gml::variable_global_get("AllTimeSalvages"), anim_ID).real == 1){
                gm_call_special_self("add_page", self, "You appear to already have an animatronic of this type!", "");
            }
            else{
                for(RValue line: dialouge_tree){
                    do_line(line, self, other, dialouge_name);
                }
                if(anim_ID > -1){
                    gm_call_special_self("play_selection_voiceline", self, anim_ID);
                    gm_call_special_self("salvage_options", self, anim_ID);
                }
            }

        }
    }




}
#endif