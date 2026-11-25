#pragma once

#include "gameMap.h"
#include <raylib.h>
#include <vector>

// 存档快照：地图 + 玩家状态 + 已探索区域
struct SaveSnapshot
{
	int     floor = 1;
	int     kills = 0;
	int     coins = 0;
	int     hp = 100;
	Vector2 playerPos = { 1.5f, 1.5f };
	std::vector<unsigned char> explored;   // 每格 0/1，w*h
};

// 写档：magic + version + 状态 + 地图 + 迷雾
bool saveGame(GameMap& map, const SaveSnapshot& s, const char* path);

// 读档：失败（文件不存在 / 版本不对 / 数据损坏）返回 false，map 保持不变
bool loadGame(GameMap& map, SaveSnapshot& s, const char* path);
