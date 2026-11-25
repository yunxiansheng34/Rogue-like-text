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
	const int  kVersion = 3;          // 3 = 地图 + 玩家 + 迷雾 + 校验和

	template <typename T>
	void writePod(std::ofstream& f, const T& v) { f.write((const char*)&v, sizeof(T)); }

	template <typename T>
	bool readPod(std::ifstream& f, T& v)
	{
		f.read((char*)&v, sizeof(T));
		return (bool)f;
	}

	// FNV-1a，用来检测存档被截断/被外部改坏
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

	// 先写临时文件，成功后再替换正式档：中途崩溃也不会毁掉老档
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

	writePod(f, sum);          // 校验和自己不参与计算
	f.close();

	if (!f) { std::remove(tmp.c_str()); return false; }

	// 旧档留一份 .bak，再让新档上位（同一分区 rename 是原子的）
	std::string bak = std::string(path) + ".bak";
	std::remove(bak.c_str());
	std::rename(path, bak.c_str());          // 老档不存在时失败，忽略即可

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

	if (!readChecked(&floor, sizeof(floor)) || !readChecked(&kills, sizeof(kills)) ||
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

	std::uint32_t fileSum = 0;
	if (!readPod(f, fileSum)) { return false; }

	if (fileSum != sum) { return false; }    // 数据被改坏，拒绝加载

	// 校验通过才动内存
	map.w = w;
	map.h = h;
	map.mapData.swap(blocks);

	s.floor = floor;
	s.kills = kills;
	s.coins = coins;
	s.hp = hp;
	s.playerPos = pos;
	s.explored.swap(explored);

	f.close();
	return true;
}
