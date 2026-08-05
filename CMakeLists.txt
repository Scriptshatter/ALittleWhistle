cmake_minimum_required(VERSION 3.10.0)
project(aLittleWhistle VERSION 0.1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aLittleWhistle SHARED
    GMFunctionDispatch.cpp
    GMFunctionOverrides.cpp
    minhook/src/buffer.c
    minhook/src/hook.c
    minhook/src/trampoline.c
    minhook/src/hde/hde64.c)

target_include_directories(aLittleWhistle PRIVATE minhook/include)
set_property(TARGET aLittleWhistle PROPERTY CXX_STANDARD 17)

target_compile_options(aLittleWhistle PRIVATE -O2)
target_link_options(aLittleWhistle PRIVATE -static -s)
set_target_properties(aLittleWhistle PROPERTIES PREFIX "" SUFFIX ".dll")