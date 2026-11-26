#include "saveGame.h"
#include "blocks.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

namespace
{
	const char kMagic[4] = { 'R', 'G', 'S', 'V' };
	const int  kVersion = 5;          // 5 = 增加章节进度与 Boss 标记

	template <typename T>
	void writePod(std::ofstream& f, const T& v) { f.write((const char*)&v, sizeof(T)); }

	template <typename T>
	bool readPod(std::ifstream& f, T& v)
	{
		f.read((char*)&v, sizeof(T));
		return (bool)f;
	}

	std::uint32_t fnv1a(const void* data, size_t len, std::uint32_t h = 2166136261u)
	{
		const unsigned char* p = (const unsigned char*)data;
		for (size_t i = 0; i < len; i++)
		{
			h ^= p[i];
			h *= 16777619u;
		}
		return h;
	}
}

bool saveGame(GameMap& map, const SaveSnapshot& s, const char* path)
{
	if (map.w <= 0 || map.h <= 0) { return false; }

	std::string tmp = std::string(path) + ".tmp";

	std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
	if (!f.is_open()) { return false; }

	std::uint32_t sum = 2166136261u;
	auto writeChecked = [&](const void* p, size_t n)
	{
		f.write((const char*)p, n);
		sum = fnv1a(p, n, sum);
	};

	f.write(kMagic, sizeof(kMagic));
	writePod(f, kVersion);

	writeChecked(&s.floor, sizeof(s.floor));
	writeChecked(&s.act, sizeof(s.act));
	writeChecked(&s.floorInAct, sizeof(s.floorInAct));
	writeChecked(&s.kills, sizeof(s.kills));
	writeChecked(&s.coins, sizeof(s.coins));
	writeChecked(&s.hp, sizeof(s.hp));
	writeChecked(&s.playerPos, sizeof(s.playerPos));

	int w = map.w, h = map.h;
	writeChecked(&w, sizeof(w));
	writeChecked(&h, sizeof(h));

	for (int i = 0; i < w * h; i++)
	{
		std::uint16_t t = map.mapData[i].type;
		writeChecked(&t, sizeof(t));
	}

	for (int i = 0; i < w * h; i++)
	{
		unsigned char e = (i < (int)s.explored.size()) ? s.explored[i] : (unsigned char)0;
		writeChecked(&e, sizeof(e));
	}

	// 敌人
	int enemyCount = (int)s.enemies.size();
	writeChecked(&enemyCount, sizeof(enemyCount));
	for (const auto& e : s.enemies)
	{
		writeChecked(&e.x, sizeof(e.x));
		writeChecked(&e.y, sizeof(e.y));
		writeChecked(&e.hp, sizeof(e.hp));
		writeChecked(&e.type, sizeof(e.type));
		writeChecked(&e.boss, sizeof(e.boss));
	}

	// 金币
	int coinCount = (int)s.coinsOnFloor.size();
	writeChecked(&coinCount, sizeof(coinCount));
	for (const auto& c : s.coinsOnFloor)
	{
		writeChecked(&c.x, sizeof(c.x));
		writeChecked(&c.y, sizeof(c.y));
		writeChecked(&c.bob, sizeof(c.bob));
	}

	writePod(f, sum);
	f.close();

	if (!f) { std::remove(tmp.c_str()); return false; }

	std::string bak = std::string(path) + ".bak";
	std::remove(bak.c_str());
	std::rename(path, bak.c_str());

	if (std::rename(tmp.c_str(), path) != 0) { return false; }

	return true;
}

bool loadGame(GameMap& map, SaveSnapshot& s, const char* path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f.is_open()) { return false; }

	char magic[4] = {};
	f.read(magic, sizeof(magic));
	if (!f || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) { return false; }

	int version = 0;
	if (!readPod(f, version) || version != kVersion) { return false; }

	std::uint32_t sum = 2166136261u;
	auto readChecked = [&](void* p, size_t n)
	{
		f.read((char*)p, n);
		if (f) { sum = fnv1a(p, n, sum); }
		return (bool)f;
	};

	int floor = 1, kills = 0, coins = 0, hp = 100;
	Vector2 pos{ 1.5f, 1.5f };
	int w = 0, h = 0;

	int act = 1, floorInAct = 1;
	if (!readChecked(&floor, sizeof(floor)) ||
	    !readChecked(&act, sizeof(act)) || !readChecked(&floorInAct, sizeof(floorInAct)) ||
	    !readChecked(&kills, sizeof(kills)) ||
	    !readChecked(&coins, sizeof(coins)) || !readChecked(&hp, sizeof(hp)) ||
	    !readChecked(&pos, sizeof(pos)) ||
	    !readChecked(&w, sizeof(w)) || !readChecked(&h, sizeof(h)))
	{
		return false;
	}

	if (w <= 0 || h <= 0 || w > 4096 || h > 4096) { return false; }

	std::vector<Block> blocks((size_t)w * h);
	for (int i = 0; i < w * h; i++)
	{
		std::uint16_t t = 0;
		if (!readChecked(&t, sizeof(t))) { return false; }
		Block b;
		b.type = t;
		b.sanitize();
		blocks[i] = b;
	}

	std::vector<unsigned char> explored((size_t)w * h, 0);
	if (!readChecked(explored.data(), explored.size())) { return false; }

	int enemyCount = 0;
	if (!readChecked(&enemyCount, sizeof(enemyCount))) { return false; }
	if (enemyCount < 0 || enemyCount > 100000) { return false; }

	std::vector<SavedEnemy> enemies;
	enemies.reserve((size_t)enemyCount);
	for (int i = 0; i < enemyCount; i++)
	{
		SavedEnemy e;
		if (!readChecked(&e.x, sizeof(e.x)) || !readChecked(&e.y, sizeof(e.y)) ||
		    !readChecked(&e.hp, sizeof(e.hp)) || !readChecked(&e.type, sizeof(e.type)) ||
		    !readChecked(&e.boss, sizeof(e.boss)))
		{
			return false;
		}
		enemies.push_back(e);
	}

	int coinCount = 0;
	if (!readChecked(&coinCount, sizeof(coinCount))) { return false; }
	if (coinCount < 0 || coinCount > 100000) { return false; }

	std::vector<SavedCoin> coinsOnFloor;
	coinsOnFloor.reserve((size_t)coinCount);
	for (int i = 0; i < coinCount; i++)
	{
		SavedCoin c;
		if (!readChecked(&c.x, sizeof(c.x)) || !readChecked(&c.y, sizeof(c.y)) ||
		    !readChecked(&c.bob, sizeof(c.bob)))
		{
			return false;
		}
		coinsOnFloor.push_back(c);
	}

	std::uint32_t fileSum = 0;
	if (!readPod(f, fileSum)) { return false; }
	if (fileSum != sum) { return false; }

	// 校验通过才动内存
	map.w = w;
	map.h = h;
	map.mapData.swap(blocks);

	s.floor = floor;
	s.act = act;
	s.floorInAct = floorInAct;
	s.kills = kills;
	s.coins = coins;
	s.hp = hp;
	s.playerPos = pos;
	s.explored.swap(explored);
	s.enemies.swap(enemies);
	s.coinsOnFloor.swap(coinsOnFloor);

	f.close();
	return true;
}
