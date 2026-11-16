# ---------------------------------------------------------------------------
# mygametext 的可执行目标配置
#   1. raylib 用工程内源码（ThirdParty/raylib-6.2）
#   2. ImGui 1.93 用 D:\libs\imgui-1.93.0
#   3. 后端用 rlImGui main 分支（支持 1.93；内部处理输入，走 rlgl 绘制）
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 自动收集本项目下所有 .cpp（含子目录）
#   - 以后在 mygametext/ 或 mygametext/src/ 里新增 .cpp，不需要改这个文件
#   - CONFIGURE_DEPENDS 让 CMake 在文件增删后自动重新配置
# ---------------------------------------------------------------------------
# 源码统一放在 src/ 下（可在 src/ 里再建子目录）
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if (NOT MY_SOURCES)
    message(FATAL_ERROR "在 ${CMAKE_CURRENT_SOURCE_DIR}/src 下没找到任何 .cpp 文件")
endif()

message(STATUS "mygametext 源文件: ${MY_SOURCES}")

add_executable (mygametext ${MY_SOURCES})
set_property(TARGET mygametext PROPERTY CXX_STANDARD 17)

# ---------------- raylib（工程内源码） ----------------
set(BUILD_EXAMPLES OFF CACHE BOOL "Don't build raylib examples" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "Don't build raylib games"    FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib as static"   FORCE)

add_subdirectory("${CMAKE_SOURCE_DIR}/ThirdParty/raylib-6.2"
                 "${CMAKE_BINARY_DIR}/thirdparty/raylib")

# ---------------- ImGui 1.93 核心 ----------------
set(IMGUI_DIR "D:/libs/imgui-1.93.0" CACHE PATH "imgui 源码目录")
if (NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "找不到 imgui 源码：${IMGUI_DIR}")
endif()

add_library(imgui_core STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp")

target_include_directories(imgui_core PUBLIC "${IMGUI_DIR}")
if (MSVC)
    target_compile_options(imgui_core PRIVATE /utf-8)
endif()

# ---------------- rlImGui 后端 ----------------
set(RLIMGUI_DIR "D:/libs/rlImGui-main" CACHE PATH "rlImGui 目录")
if (NOT EXISTS "${RLIMGUI_DIR}/rlImGui.cpp")
    message(FATAL_ERROR "找不到 rlImGui：${RLIMGUI_DIR}")
endif()

add_library(rlimgui STATIC "${RLIMGUI_DIR}/rlImGui.cpp")
target_include_directories(rlimgui PUBLIC "${RLIMGUI_DIR}")
target_link_libraries(rlimgui PUBLIC imgui_core raylib)
if (MSVC)
    target_compile_options(rlimgui PRIVATE /utf-8)
endif()

# ---------------- 链接 ----------------
# 让 src/ 里的头文件可以直接 #include "xxx.h"
target_include_directories(mygametext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gameplayer")

target_link_libraries(mygametext PRIVATE raylib rlimgui)

if (MSVC)
    target_compile_options(mygametext PRIVATE /utf-8)
    target_compile_definitions(mygametext PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# TODO: 如有需要，请添加测试并安装目标。
