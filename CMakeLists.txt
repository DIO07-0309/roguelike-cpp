cmake_minimum_required(VERSION 3.16)
project(roguelike_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 强制 UTF-8: MSVC 源文件编码 + 运行期字面量编码
if(MSVC)
    add_compile_options("/utf-8")
    add_compile_options("/Zc:__cplusplus")
endif()

# ---- Raylib (vendored) ----
set(RAYLIB_DIR "${CMAKE_SOURCE_DIR}/vendor/raylib")
set(JSON_DIR   "${CMAKE_SOURCE_DIR}/vendor/json/include")

include_directories("${RAYLIB_DIR}/include")
include_directories("${JSON_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/src/core")
include_directories("${CMAKE_SOURCE_DIR}/src/game")
include_directories("${CMAKE_SOURCE_DIR}/src/game/entities")
include_directories("${CMAKE_SOURCE_DIR}/src/game/systems")
include_directories("${CMAKE_SOURCE_DIR}/src/game/world")
include_directories("${CMAKE_SOURCE_DIR}/src/game/scenes")
include_directories("${CMAKE_SOURCE_DIR}/src/game/rendering")
include_directories("${CMAKE_SOURCE_DIR}/src/game/audio")
include_directories("${CMAKE_SOURCE_DIR}/src/game/save")
include_directories("${CMAKE_SOURCE_DIR}/src/game/tutorial")

# ---- Sources ----
file(GLOB_RECURSE SOURCES src/*.cpp)

# ---- Executable ----
add_executable(${PROJECT_NAME} ${SOURCES})

# MSVC: 使用 raylibdll.lib (导入库) + 复制 raylib.dll
if(MSVC)
    set(RAYLIB_IMPLIB "${RAYLIB_DIR}/lib/raylibdll.lib")
    set(RAYLIB_DLL    "${RAYLIB_DIR}/lib/raylib.dll")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${RAYLIB_IMPLIB}" winmm opengl32
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RAYLIB_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/raylib.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
else()
    # MinGW / Linux / macOS
    set(RAYLIB_LIB "${RAYLIB_DIR}/lib/libraylibdll.a")
    set(RAYLIB_DLL "${RAYLIB_DIR}/lib/raylib.dll")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${RAYLIB_LIB}" winmm gdi32 opengl32
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RAYLIB_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/raylib.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    )
endif()
