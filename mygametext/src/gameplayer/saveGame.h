#pragma once

#include "gameMap.h"
#include <raylib.h>
#include <vector>

struct SavedEnemy
{
	float x = 0.0f, y = 0.0f;
	int   hp = 30;
	int   type = 0;
	int   boss = 0;      // 1 = 首领
};

struct SavedCoin
{
	float x = 0.0f, y = 0.0f;
	float bob = 0.0f;
};

// 存档快照：地图 + 玩家 + 迷雾 + 敌人 + 金币
struct SaveSnapshot
{
	int     floor = 1;        // 全局层数
	int     act = 1;          // 第几章（4 = 最终决战）
	int     floorInAct = 1;   // 章内第几层
	int     kills = 0;
	int     coins = 0;
	int     hp = 100;
	Vector2 playerPos = { 1.5f, 1.5f };

	std::vector<unsigned char> explored;   // 每格 0/1，w*h
	std::vector<SavedEnemy>    enemies;
	std::vector<SavedCoin>     coinsOnFloor;
};

// 写档：magic + version + 状态 + 地图 + 迷雾 + 敌人 + 金币（带校验和、原子替换）
bool saveGame(GameMap& map, const SaveSnapshot& s, const char* path);

// 读档：失败（文件不存在 / 版本不对 / 校验和不符）返回 false，map 保持不变
bool loadGame(GameMap& map, SaveSnapshot& s, const char* path);
