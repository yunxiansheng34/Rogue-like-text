# ---------------------------------------------------------------------------
# mygametext 的可执行目标配置
#   1. raylib 用工程内源码（ThirdParty/raylib-6.2）
#   2. ImGui 1.93 用 D:\libs\imgui-1.93.0
#   3. 后端用 rlImGui main 分支（支持 1.93；内部处理输入，走 rlgl 绘制）
#   4. 资源路径宏 RESOURCES_PATH：来自课件 10-资源管理器 的做法
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 自动收集本项目下所有 .cpp（含子目录）
#   - 以后在 mygametext/ 或 mygametext/src/ 里新增 .cpp，不需要改这个文件
#   - CONFIGURE_DEPENDS 让 CMake 在文件增删后自动重新配置
# ---------------------------------------------------------------------------
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

# ---------------- FastNoiseSIMD（16-4 起用于地形/洞穴噪声） ----------------
add_subdirectory("${CMAKE_SOURCE_DIR}/ThirdParty/FastNoiseSIMD"
                 "${CMAKE_BINARY_DIR}/thirdparty/fastnoise")

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

# ---------------- 生产构建开关 ----------------
# PRODUCTION_BUILD：
#   ON  = 发布版：raylib 日志关掉（main.cpp 的 SetTraceLogLevel），
#                 RESOURCES_PATH 变成相对 exe 的 "./resources/"
#   OFF = 开发版：保留日志，RESOURCES_PATH 用绝对路径，IDE 里直接能跑
option(PRODUCTION_BUILD "ON = 发布版（关闭日志、资源走 exe 同级目录）" OFF)

# HIDE_CONSOLE：
#   ON  = 编译成 GUI 程序（Windows 子系统），运行时不弹控制台黑窗口
#   OFF = 控制台程序，能看 raylib 日志和自己的 std::cout
# 默认跟随 PRODUCTION_BUILD（课件里发布版就是无控制台的）
option(HIDE_CONSOLE "ON = 隐藏控制台窗口（GUI 子系统）" ${PRODUCTION_BUILD})

message(STATUS "PRODUCTION_BUILD = ${PRODUCTION_BUILD}   HIDE_CONSOLE = ${HIDE_CONSOLE}")

# ---------------- 资源路径（课件 10-资源管理器 的做法） ----------------
# 代码里用 RESOURCES_PATH 拼路径，例如：
#     std::string p = std::string(RESOURCES_PATH) + "player/idle.png";
# 注意结尾自带 "/"，拼的时候不要再加。
if (PRODUCTION_BUILD)
    # 发布版：exe 同级的 ./resources/，整个文件夹跟着 exe 一起拷走
    target_compile_definitions(mygametext PRIVATE RESOURCES_PATH="./resources/")
    target_compile_definitions(mygametext PRIVATE PRODUCTION_BUILD=1)
    message(STATUS "RESOURCES_PATH = ./resources/  （相对 exe）")
else()
    # 开发版：用源码目录的绝对路径，在 IDE 里 F5 也能找到资源
    target_compile_definitions(mygametext PRIVATE
        RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
    target_compile_definitions(mygametext PRIVATE PRODUCTION_BUILD=0)
    message(STATUS "RESOURCES_PATH = ${CMAKE_CURRENT_SOURCE_DIR}/resources/  （绝对路径）")
endif()

# ---------------- 链接 ----------------
# 让 src/ 里的头文件可以直接 #include "xxx.h"
target_include_directories(mygametext PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gameplayer"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"      # 课件里的平台层
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer")    # 课件里的游戏层

target_link_libraries(mygametext PRIVATE raylib rlimgui fastNoiseSIMD)

# 隐藏控制台：把子系统从 CONSOLE 改成 WINDOWS
# /ENTRY:mainCRTStartup 是为了继续用 int main()（否则链接器会去找 WinMain）
if (HIDE_CONSOLE AND MSVC)
    target_link_options(mygametext PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "/ENTRY:mainCRTStartup")
endif()

if (MSVC)
    target_compile_options(mygametext PRIVATE /utf-8)
    target_compile_definitions(mygametext PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------- 发布版把 resources/ 拷到 exe 旁边 ----------------
if (PRODUCTION_BUILD)
    add_custom_command(TARGET mygametext POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:mygametext>/resources"
        COMMENT "复制 resources/ 到 exe 目录（发布版）")
endif()

# TODO: 如有需要，请添加测试并安装目标。
