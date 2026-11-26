#include "dungeon.h"
#include "blocks.h"
#include "random_stuff.h"

#include <vector>
#include <algorithm>
#include <cmath>

namespace
{
	struct Room
	{
		int x, y, w, h;
		int cx() const { return x + w / 2; }
		int cy() const { return y + h / 2; }
	};

	bool overlaps(const Room& a, const Room& b)
	{
		return !(a.x + a.w + 1 < b.x || b.x + b.w + 1 < a.x ||
		         a.y + a.h + 1 < b.y || b.y + b.h + 1 < a.y);
	}

	void carveRect(GameMap& map, int x, int y, int w, int h)
	{
		for (int j = y; j < y + h; j++)
			for (int i = x; i < x + w; i++)
			{
				Block* b = map.getBlockSafe(i, j);
				if (b) { b->type = Block::air; }
			}
	}

	void carveCorridor(GameMap& map, int x1, int y1, int x2, int y2)
	{
		int x = x1, y = y1;
		while (x != x2)
		{
			Block* b = map.getBlockSafe(x, y);
			if (b) { b->type = Block::air; }
			x += (x2 > x) ? 1 : -1;
		}
		while (y != y2)
		{
			Block* b = map.getBlockSafe(x, y);
			if (b) { b->type = Block::air; }
			y += (y2 > y) ? 1 : -1;
		}
		Block* b = map.getBlockSafe(x2, y2);
		if (b) { b->type = Block::air; }
	}
}

void generateDungeon(GameMap& map, Vector2& spawnPos, bool bossFloor)
{
	const int W = 64;
	const int H = 40;

	map.create(W, H);

	// 1) 全实心墙起步（后面再挖房间和走廊，保证不会漏边）
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			map.getBlocUnsafe(x, y).type = Block::stoneBricks;

	// ---- Boss 层：一个大竞技场，四周留墙 ----
	if (bossFloor)
	{
		const int rw = 34, rh = 18;
		int rx = (W - rw) / 2;
		int ry = (H - rh) / 2;
		carveRect(map, rx, ry, rw, rh);

		// 四角留一点装饰柱
		for (int i = 0; i < 4; i++)
		{
			map.getBlocUnsafe(rx + 4 + i * 8, ry + 4).type = Block::stoneBricks;
			map.getBlocUnsafe(rx + 4 + i * 8, ry + rh - 5).type = Block::stoneBricks;
		}

		spawnPos = { (float)(rx + rw / 2) + 0.5f, (float)(ry + rh - 3) + 0.5f };
		return;
	}

	std::ranlux24_base rng((unsigned)std::random_device{}());

	// 2) 随机放房间，互不重叠
	std::vector<Room> rooms;
	for (int tries = 0; tries < 200 && (int)rooms.size() < 9; tries++)
	{
		int w = getRandomInt(rng, 7, 13);
		int h = getRandomInt(rng, 5, 9);
		int x = getRandomInt(rng, 1, W - w - 2);
		int y = getRandomInt(rng, 1, H - h - 2);

		Room r{ x, y, w, h };
		bool bad = false;
		for (auto& o : rooms) { if (overlaps(r, o)) { bad = true; break; } }
		if (bad) { continue; }

		rooms.push_back(r);
		carveRect(map, x, y, w, h);
	}

	// 3) 用 L 形走廊按顺序连通
	for (size_t i = 1; i < rooms.size(); i++)
		carveCorridor(map, rooms[i - 1].cx(), rooms[i - 1].cy(), rooms[i].cx(), rooms[i].cy());

	// 4) 出生点 = 第一个房间中心；出口 = 离出生点最远的房间
	spawnPos = { (float)rooms[0].cx() + 0.5f, (float)rooms[0].cy() + 0.5f };

	size_t far = 0;
	float bestDist = -1.0f;
	for (size_t i = 0; i < rooms.size(); i++)
	{
		float dx = (float)(rooms[i].cx() - rooms[0].cx());
		float dy = (float)(rooms[i].cy() - rooms[0].cy());
		float d = dx * dx + dy * dy;
		if (d > bestDist) { bestDist = d; far = i; }
	}
	map.getBlocUnsafe(rooms[far].cx(), rooms[far].cy()).type = Block::door;

	// 5) 除出生房和出口房外，随机放宝箱
	for (size_t i = 1; i < rooms.size(); i++)
	{
		if (i == far) { continue; }
		if (!getRandomChance(rng, 0.6f)) { continue; }
		map.getBlocUnsafe(rooms[i].x + 1, rooms[i].y + 1).type = Block::woodenChest;
	}
}
