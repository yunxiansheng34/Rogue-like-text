#pragma once

#include <vector>
#include <raylib.h>
#include "blocks.h"

struct GameMap
{
	int w = 0;
	int h = 0;
	std::vector<Block> mapData;

	void create(int width, int height);
	Block& getBlocUnsafe(int x, int y);     // 越界不做检查
	Block* getBlockSafe(int x, int y);      // 越界返回 nullptr
};
// 注意：这里不能写成 }GameMap;
// 那样会额外声明一个叫 GameMap 的变量，之后 "GameMap xxx;" 就会报
// E0757「变量 GameMap 不是类型名」

// ---------------------------------------------------------------- 图集取块
// 纹理图集里每种方块占 tileW x tileH 一格，横向排列。
//   type  —— 方块类型（blocks.h 里的枚举值，0 = air）
//   row   —— 取图集的第几行（0 起）
//   tileW —— 单块宽度（本项目素材是 32）
//   tileH —— 单块高度（本项目素材是 32）
// 返回可直接喂给 DrawTexturePro 的源矩形。
Rectangle getTextureAtlas(int type, int row, int tileW, int tileH);
