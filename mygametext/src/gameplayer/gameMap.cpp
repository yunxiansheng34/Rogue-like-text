#include "gameMap.h"

#include "asserts.h"

void GameMap::create(int width, int height)
{
	*this = {};

	if (width <= 0 || height <= 0) { return; }

	mapData.assign(static_cast<size_t>(width) * static_cast<size_t>(height), Block{});

	w = width;
	h = height;
}

Block& GameMap::getBlocUnsafe(int x, int y)
{
	permaAssertCommentDevelopement(
		mapData.size() == static_cast<size_t>(w) * static_cast<size_t>(h),
		"Map data not initialized");

	// 原来这里写成了 (x >= 0 && y >= 0 && x, w && y < h)，是逗号表达式的误用
	permaAssertCommentDevelopement(
		x >= 0 && y >= 0 && x < w && y < h,
		"getBlocUnsafe out of bounds error");

	return mapData[static_cast<size_t>(x) + static_cast<size_t>(y) * w];
}

Block* GameMap::getBlockSafe(int x, int y)
{
	permaAssertCommentDevelopement(
		mapData.size() == static_cast<size_t>(w) * static_cast<size_t>(h),
		"Map data not initialized");

	if (x < 0 || y < 0 || x >= w || y >= h) { return nullptr; }

	// 原来写的是 &GameData[...]，GameData 根本不存在，应该是 mapData
	return &mapData[static_cast<size_t>(x) + static_cast<size_t>(y) * w];
}

// ---------------------------------------------------------------- 图集取块
Rectangle getTextureAtlas(int type, int row, int tileW, int tileH)
{
	return Rectangle{
		(float)(type * tileW),
		(float)(row * tileH),
		(float)tileW,
		(float)tileH
	};
}
