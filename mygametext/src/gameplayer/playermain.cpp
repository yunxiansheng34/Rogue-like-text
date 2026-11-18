#include "playermain.h"

#include "assetManager.h"      // 资源管理器
#include "gameMap.h"           // 地图
#include "blocks.h"            // Block 类型

#include <raylib.h>
#include <imgui.h>

#include <cmath>

// ---------------------------------------------------------------- 全局数据
// 只在这里定义一次；头文件里已有 struct 定义，重复定义会冲突。
AssetManager assetManager;

struct GameData
{
	GameMap gameMap;
	Camera2D camera;
	RenderTexture2D renderingSystem = {};
};

// 变量名用小写，别和类型名 GameData 撞车
static GameData gameData;

GameData& getGameData() { return gameData; }

// ---------------------------------------------------------------- 占位函数
void renderStuff()
{
	auto& gd = getGameData();
	(void)gd;
}

void renderStuff(GameData& gd) { (void)gd; }

void renderStuff(RenderTexture2D& renderingSystem) { (void)renderingSystem; }

// ---------------------------------------------------------------- 生命周期
bool initGame()
{
	assetManager.loadAll();

	// 建一张 30x10 的地图，并在 (0,0) 放一块泥土
	gameData.gameMap.create(30, 10);
	gameData.gameMap.getBlocUnsafe(0, 0).type = Block::dirt;
	gameData.gameMap.getBlocUnsafe(1, 1).type = Block::boneBricks;
	gameData.gameMap.getBlocUnsafe(2, 2).type = Block::ice;
	gameData.gameMap.getBlocUnsafe(3, 3).type = Block::glass;
	gameData.gameMap.getBlocUnsafe(4, 4).type = Block::goldBlock;
	gameData.camera.target = { 0,0 };
	gameData.camera.rotation = 0.0f;
	gameData.camera.zoom = 100.0f;
	return true;
}

bool updateGame()
{
	float deltaTime = GetFrameTime();
	if (deltaTime > 1.f / 5.f) { deltaTime = 1.f / 5.f; }
	gameData.camera.offset = { GetScreenWidth() / 2.0f,GetScreenHeight() / 2.0f };
	ClearBackground({ 75,75,150,255 });
#pragma region camera movement
	if (IsKeyDown(KEY_LEFT)) { gameData.camera.target.x -= 7.f * deltaTime; }
	if (IsKeyDown(KEY_RIGHT)) { gameData.camera.target.x += 7.f * deltaTime; }
	if (IsKeyDown(KEY_UP)) { gameData.camera.target.y -= 7.f * deltaTime; }
	if (IsKeyDown(KEY_DOWN)) { gameData.camera.target.y += 7.f * deltaTime; }

	BeginMode2D(gameData.camera);

	for (int y = 0;y < gameData.gameMap.h;y++)
	{
		for (int x = 0;x < gameData.gameMap.w;x++)
		{
			auto& b = gameData.gameMap.getBlocUnsafe(x, y);
			if (b.type != Block::air) {
				Rectangle textureUV;
				textureUV.width = 32;
				textureUV.height = 32;
				textureUV.x = b.type * 32;
				textureUV.y = 0;
				DrawTexturePro(
					assetManager.textures,
					textureUV,
					{(float)x,(float)y,1,1},
					{0,0},
					0.0f,
					WHITE
				);
			}
		}
	}
	return true;
}

void closeGame()
{
	UnloadTexture(assetManager.dirt);
}

// ---------------------------------------------------------------- 关卡切换
void loadlevel() { gameData.gameMap = {}; }
void closelevel() { gameData.gameMap = {}; }
