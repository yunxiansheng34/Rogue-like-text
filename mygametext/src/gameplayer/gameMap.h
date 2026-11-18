#pragma once

#include <vector>
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
