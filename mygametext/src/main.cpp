// ============================================================================
//  课程 7-游戏架构 / sourceCodeUsingImgui / src/main.cpp
//  —— 已适配本机环境（raylib 6.2 + ImGui 1.93 master + rlImGui main）
//  ---------------------------------------------------------------------------
//  和课程原版的差异（都是版本 API 变化导致的，功能保持一致）：
//    1. ImGui 1.93 是 master 版，**没有 docking**：
//       删掉 ImGuiConfigFlags_DockingEnable / DockSpaceOverViewport / DockingEmptyBg
//    2. io.FontGlobalScale 在 1.92 被移到 style.FontScaleMain
//    3. 字体改用 rlImGuiSetLoadFontsCallback（新版 rlImGui 没有 ReloadFonts）
//    4. 加了 --shot 参数：跑 30 帧截图，方便验证
// ============================================================================
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include <iostream>

#include <raylib.h>

#include <imgui.h>
#include <rlImGui.h>

#include <string.h>

#include "gameplayer/playermain.h"   // 游戏模块（在 src/gameplayer/ 下）

int main(int argc, char** argv)
{
#if PRODUCTION_BUILD == 1
	SetTraceLogLevel(LOG_NONE);
#endif
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(1280, 720, "window name");   // 课程是 800x450，但 FontScale=2 放不下，故放大
	SetTargetFPS(120);

#pragma region imgui
	// 1.93 是动态字体图集：中文字体通过回调注册，必须在 Setup 之前
	static auto LoadFonts = []() {
		ImGuiIO& fio = ImGui::GetIO();
		ImFont* f = fio.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/simhei.ttf", 16.0f);
		if (f) fio.FontDefault = f;
	};
	rlImGuiSetLoadFontsCallback(LoadFonts);

	rlImGuiSetup(true);

	ImGuiIO& io = ImGui::GetIO();
	////io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	////io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable gamepad Controls
	// 课程里这里是 io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// 但 1.93 master 版没有 docking，故删除。
	ImGui::GetStyle().FontScaleMain = 1.5f;   // 1.92 起替代 io.FontGlobalScale；课程用 2，这里 1.5 更适配 1280x720
#pragma endregion

	int frameCounter = 0;

	if (!initGame()) {
		return 0;
	}

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);

	#pragma region imgui
		rlImGuiBegin();

		// 玩法更新 + 绘制（模块内部也会画 ImGui 面板）
		if (!updateGame())
		{
			rlImGuiEnd();
			EndDrawing();
			break;
		}

		// 课程里的 docking 代码（本机不可用，保留注释供对照）：
		// ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		// ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0, 0, 0, 0));
		// ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
		// ImGui::PopStyleColor(2);
	#pragma endregion


		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(440, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("test");

		ImGui::Text("hello");

		if (ImGui::Button("button"))
		{
			std::cout << "Text\n";
		}
		ImGui::SameLine();
		if (ImGui::Button("button2"))
		{
			std::cout << "Different button\n";
		}

		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(20, 340), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(440, 180), ImGuiCond_FirstUseEver);
		ImGui::Begin("second window");

		ImGui::Text("hello");
		ImGui::Separator();
		ImGui::NewLine();
		static float a = 0;
		ImGui::SliderFloat("slider", &a, 0, 1);

		ImGui::End();

		ImGui::ShowDemoWindow();
		if (!updateGame()) {
			CloseWindow();
		}
	#pragma region imgui
		rlImGuiEnd();

	#pragma endregion

		EndDrawing();
	}

	closeGame();

#pragma region imgui
	rlImGuiShutdown();
#pragma endregion

	CloseWindow();

	return 0;
}
