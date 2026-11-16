# mygametext —— 课程 7-游戏架构 的 main + ImGui

## 源码结构

```
mygametext/
  CMakeLists.txt
  CMakePresets.json
  mygametext/
    CMakeLists.txt          自动收集 src/*.cpp
    src/
      main.cpp              课程 7-游戏架构 的 main（已适配本机）
      gameplayer/
        playermain.cpp      玩法模块：initGame / updateGame / closeGame
        playermain.h
  ThirdParty/raylib-6.2/    ← 不纳入 Git（见下）
```

## 依赖准备（克隆后必做）

仓库里**不包含** `ThirdParty/`（raylib 源码 188 MB），第一次拉下来要自己放：

1. 下载 raylib 6.2 源码（Source code zip）：
   `https://github.com/raysan5/raylib/releases/tag/6.2`
2. 解压后改成这个路径：

```
mygametext/ThirdParty/raylib-6.2/CMakeLists.txt      ← 这个文件必须存在
```

3. ImGui 与 rlImGui 也不在仓库里，按 `mygametext/CMakeLists.txt` 顶部的
   `IMGUI_DIR` / `RLIMGUI_DIR` 变量准备：

| 变量 | 需要的目录 | 获取方式 |
|---|---|---|
| `IMGUI_DIR` | `D:/libs/imgui-1.93.0` | `https://github.com/ocornut/imgui/releases` 下 1.93.0 源码 |
| `RLIMGUI_DIR` | `D:/libs/rlImGui-main` | `https://github.com/raylib-extras/rlImGui` 的 main 分支 |

> 也可以把这两个变量改成相对路径，改成项目内 `thirdparty/`，再用 `-D` 覆盖。

## main.cpp 相对课程原版改了什么

课程用的是 raylib 5.0 + ImGui 1.89(docking) + 老 rlImGui，本机是
raylib 6.2 + **ImGui 1.93 master（无 docking）** + rlImGui main，所以改了三处：

| 课程原版 | 本机改法 | 原因 |
|---|---|---|
| `io.ConfigFlags \|= ImGuiConfigFlags_DockingEnable;` | **删除** | 1.93 master 版没有 docking |
| `DockSpaceOverViewport(...)` + `ImGuiCol_DockingEmptyBg` | **删除**（注释保留） | 同上 |
| `io.FontGlobalScale = 2;` | `ImGui::GetStyle().FontScaleMain = 1.5f;` | 1.92 起该字段迁移到 style |
| — | 加 `rlImGuiSetLoadFontsCallback` 加载中文字体 | 新版 rlImGui 没有 ReloadFonts |
| `InitWindow(800, 450, ...)` | `1280×720` + 给两个窗口 SetNextWindowPos/Size | 字体放大后 800×450 放不下，窗口会重叠 |

功能完全保留：两个 ImGui 窗口、button/button2 打印到控制台、slider、ShowDemoWindow。

## 编译运行

```bat
cmake --preset x64-release
cmake --build --preset x64-release
out\build\x64-release\mygametext\Release\mygametext.exe
```

VS：打开文件夹 → 配置 `x64-release` → 启动项 `mygametext.exe` → F5

## 小提示

- 程序会在 exe 旁边生成 **`imgui.ini`** 记住窗口位置。想恢复默认布局就删掉它。
- `--shot` 参数：跑 30 帧后截图到 `mygametext_shot.png`。
- 新增 `.cpp` 放 `src/` 下即可，CMake 会自动收集（含 `src/gameplayer/` 子目录）。

## 想用 docking

需要换成 ImGui 的 **docking 分支**（`git clone -b docking`），
然后 `#define IMGUI_HAS_DOCK` 并加回 `DockSpaceOverViewport`。

## 备份

`_backup_课程main前/`（原来的 mygametext.cpp/h）、`_backup_CMakeLists_当前可用/`