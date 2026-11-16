// ============================================================================
//  gameplayer/playermain.cpp
//  ---------------------------------------------------------------------------
//  课程 7-游戏架构 的玩法模块骨架：initGame / updateGame / closeGame
//  主循环只负责窗口和 ImGui，具体玩法都在这里，方便以后拆分实体、系统。
// ============================================================================
#include "playermain.h"

#include <raylib.h>
#include <imgui.h>

#include <cmath>

// ---------------------------------------------------------------- 玩家数据
struct Player
{
	Vector2 pos;
	float speed;
	float radius;
	Color color;
};

static Player gPlayer;
static int gScore = 0;
static bool gQuit = false;

bool initGame()
{
	gPlayer.pos = Vector2{ 640.0f, 400.0f };
	gPlayer.speed = 320.0f;
	gPlayer.radius = 24.0f;
	gPlayer.color = Color{ 90, 200, 120, 255 };

	gScore = 0;
	gQuit = false;

	return true;   // 返回 false 时主程序会直接退出
}

bool updateGame()
{
	const float dt = GetFrameTime();

	// ---- 输入 ----
	if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) gPlayer.pos.x += gPlayer.speed * dt;
	if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  gPlayer.pos.x -= gPlayer.speed * dt;
	if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    gPlayer.pos.y -= gPlayer.speed * dt;
	if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  gPlayer.pos.y += gPlayer.speed * dt;

	// 简单边界限制
	const float w = (float)GetScreenWidth();
	const float h = (float)GetScreenHeight();
	if (gPlayer.pos.x < gPlayer.radius) gPlayer.pos.x = gPlayer.radius;
	if (gPlayer.pos.x > w - gPlayer.radius) gPlayer.pos.x = w - gPlayer.radius;
	if (gPlayer.pos.y < gPlayer.radius) gPlayer.pos.y = gPlayer.radius;
	if (gPlayer.pos.y > h - gPlayer.radius) gPlayer.pos.y = h - gPlayer.radius;

	// 左键点一下加分（演示"玩法逻辑在模块里"）
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		float dx = GetMouseX() - gPlayer.pos.x;
		float dy = GetMouseY() - gPlayer.pos.y;
		if (dx * dx + dy * dy < (gPlayer.radius + 10) * (gPlayer.radius + 10))
			gScore++;
	}

	// ---- 绘制 ----
	DrawCircleV(gPlayer.pos, gPlayer.radius + 6, Fade(gPlayer.color, 0.25f));
	DrawCircleV(gPlayer.pos, gPlayer.radius, gPlayer.color);

	// ---- ImGui 调试面板（主循环里 rlImGuiBegin 已经开好帧）----
	ImGui::SetNextWindowPos(ImVec2(20, 545), ImGuiCond_FirstUseEver);   // 放左下角，避开课程的两个窗口
	ImGui::SetNextWindowSize(ImVec2(440, 150), ImGuiCond_FirstUseEver);
	ImGui::Begin("gameplayer");
	ImGui::Text("FPS: %d", GetFPS());
	ImGui::Text("pos: (%.0f, %.0f)", gPlayer.pos.x, gPlayer.pos.y);
	ImGui::Text("score: %d", gScore);
	ImGui::SliderFloat("speed", &gPlayer.speed, 60.0f, 900.0f);
	ImGui::Checkbox("quit", &gQuit);
	ImGui::TextDisabled("WASD 移动，点小球加分");
	ImGui::End();

	return !gQuit;
}

void closeGame()
{
	// 有资源（纹理/音效）在这里 Unload
}
