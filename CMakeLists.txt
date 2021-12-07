cmake_minimum_required(VERSION 2.8)

#Needed for old Qt Creator versions, so header files are shown in project tree
#FILE(GLOB_RECURSE LibFiles "*.h")
#add_custom_target(headers SOURCES ${LibFiles})

project(libpartikel LANGUAGES C)

set (CMAKE_C_STANDARD 11)

set (CMAKE_C_FLAGS_INIT           "-Wall -std=c11")
set (CMAKE_C_FLAGS_DEBUG          "-g")
set (CMAKE_C_FLAGS_RELEASE        "-O2 -DNDEBUG")

add_executable(demo "demo.c")
add_executable(editor "editor.c")

target_link_libraries(demo ${RAYLIB_LIBRARY} m)
target_link_libraries(editor ${RAYLIB_LIBRARY} m)

target_include_directories(demo PUBLIC ${RAYLIB_INCLUDE})
target_include_directories(editor PUBLIC ${RAYLIB_INCLUDE})

if (APPLE)
  target_link_libraries(demo "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
  target_link_libraries(editor "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
endif (APPLE)