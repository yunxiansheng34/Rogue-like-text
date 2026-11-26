#include "assetManager.h"

#include <string>

// RESOURCES_PATH 由 CMake 注入（Debug = 绝对路径，Release = ./resources/）
#ifndef RESOURCES_PATH
#define RESOURCES_PATH "./resources/"
#endif

static Texture2D loadTex(const char* rel)
{
	std::string path = std::string(RESOURCES_PATH) + rel;
	return LoadTexture(path.c_str());
}

Rectangle frameRect(int index, int frameSize)
{
	return Rectangle{ (float)(index * frameSize), 0.0f, (float)frameSize, (float)frameSize };
}

void AssetManager::loadAll()
{
	playerIdle[0] = loadTex("rogue/player/D_Idle.png");
	playerIdle[1] = loadTex("rogue/player/S_Idle.png");
	playerIdle[2] = loadTex("rogue/player/U_Idle.png");

	playerWalk[0] = loadTex("rogue/player/D_Walk.png");
	playerWalk[1] = loadTex("rogue/player/S_Walk.png");
	playerWalk[2] = loadTex("rogue/player/U_Walk.png");

	enemyIdle[0] = loadTex("rogue/e1/D_Idle.png");
	enemyIdle[1] = loadTex("rogue/e2/D_Idle.png");
	enemyIdle[2] = loadTex("rogue/e3/D_Idle.png");
	enemyIdle[3] = loadTex("rogue/e4/D_Idle.png");

	enemyWalk[0] = loadTex("rogue/e1/D_Walk.png");
	enemyWalk[1] = loadTex("rogue/e2/D_Walk.png");
	enemyWalk[2] = loadTex("rogue/e3/D_Walk.png");
	enemyWalk[3] = loadTex("rogue/e4/D_Walk.png");

	floor   = loadTex("rogue/tiles/floor1.png");
	wall    = loadTex("rogue/tiles/wall1.png");
	floor2  = loadTex("rogue/tiles/floor2.png");
	wall2   = loadTex("rogue/tiles/wall2.png");
	floor3  = loadTex("rogue/tiles/floor3.png");
	wall3   = loadTex("rogue/tiles/wall3.png");
	floor4  = loadTex("rogue/tiles/floor4.png");
	wall4   = loadTex("rogue/tiles/wall3.png");
	trapdoor = loadTex("rogue/objects/trapdoor.png");
	chest   = loadTex("rogue/objects/chest.png");
	coin    = loadTex("rogue/ui/icon_coin.png");
	shadow  = loadTex("rogue/fx/shadow.png");
}

void AssetManager::unloadAll()
{
	for (int i = 0; i < 3; i++)
	{
		UnloadTexture(playerIdle[i]);
		UnloadTexture(playerWalk[i]);
	}
	for (int i = 0; i < 4; i++)
	{
		UnloadTexture(enemyIdle[i]);
		UnloadTexture(enemyWalk[i]);
	}
	UnloadTexture(floor);
	UnloadTexture(wall);
	UnloadTexture(floor2);
	UnloadTexture(wall2);
	UnloadTexture(floor3);
	UnloadTexture(wall3);
	UnloadTexture(floor4);
	UnloadTexture(wall4);
	UnloadTexture(trapdoor);
	UnloadTexture(chest);
	UnloadTexture(coin);
	UnloadTexture(shadow);
}
