


#ifndef OVERRIDE
#define OVERRIDE
#include "GMGeneralLibrary.h"
void custom_func_loader(RValue* result, void* self, void* other, int argc, RValue* args);
void talksound_override(RValue* result, void* self, void* other, int argc, RValue* args);
void custom_asset_patch(RValue* result, void* self, void* other, int argc, RValue* args);
void set_salvage_var(RValue* result, void* self, void* other, int argc, RValue* args);
void func_override(RValue* result, void* self, void* other, int argc, RValue* args);
void new_upgrades(RValue* result, void* self, void* other, int argc, RValue* args);
void get_dialouge(RValue* result, void* self, void* other, int argc, RValue* args);
void override_background(RValue* result, void* self, void* other, int argc, RValue* args);
void super_conditions(RValue* result, void* self, void* other, int argc, RValue* args);

extern int new_quota;
extern bool disable_override;



#endif