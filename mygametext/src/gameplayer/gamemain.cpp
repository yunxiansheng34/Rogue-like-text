#include "gamemain.h"

#include "assetManager.h"
#include "dungeon.h"
#include "blocks.h"

#include <raylib.h>
#include <imgui.h>

#include "saveGame.h"
#include "settings.h"
#include "sfx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------- 全局数据
AssetManager assetManager;

namespace
{
	constexpr int   kPlayerMaxHp = 100;
	constexpr float kPlayerSpeed = 4.2f;     // 每秒移动多少格
	constexpr float kPlayerHalf = 0.34f;     // 碰撞半宽（格）
	constexpr float kPlayerDraw = 1.25f;     // 贴图绘制边长（格）
	constexpr float kAttackRange = 1.7f;
	constexpr float kAttackCd = 0.35f;
	constexpr int   kAttackDamage = 25;
	constexpr float kEnemySpeed = 1.9f;
	constexpr float kEnemyTouchDist = 0.75f;
	constexpr float kEnemyDamage = 8.0f;
	constexpr float kInvulnTime = 0.9f;

	struct Player
	{
		Vector2 pos = { 1.5f, 1.5f };
		int hp = kPlayerMaxHp;
		int coins = 0;
		int dir = 0;            // 0=下 1=侧 2=上
		bool flip = false;      // 侧面时是否水平翻转
		float anim = 0.0f;
		float atkCd = 0.0f;
		float invuln = 0.0f;
		float atkFlash = 0.0f;
		bool moving = false;
	};

	struct Enemy
	{
		Vector2 pos = {};
		int hp = 30;
		int maxHp = 30;
		int type = 0;            // 0~3 → e1~e4
		bool boss = false;
		float anim = 0.0f;
		float hurt = 0.0f;
	};

	struct Coin
	{
		Vector2 pos = {};
		float bob = 0.0f;
	};

	const char* kSavePath = RESOURCES_PATH "saves/save1.bin";
	const char* kSettingsPath = RESOURCES_PATH "saves/settings.txt";

	GameSettings gSettings;
	bool gSettingsOpen = false;

	GameMap gMap;
	Camera2D gCam = {};

	// 视野：每格两个状态——可见 / 探索过
	std::vector<unsigned char> gVisible;
	std::vector<unsigned char> gExplored;
	int lastPX = -999, lastPY = -999;
	Player gPlayer;
	std::vector<Enemy> gEnemies;
	std::vector<Coin> gCoins;

	// 章节结构：3 章 × 3 层，第 3 层是章节 Boss；第 4 章 = 最终 Boss
	constexpr int kFloorsPerAct = 3;
	constexpr int kActs = 3;

	int gAct = 1;            // 1..3 = 普通章节，4 = 最终 Boss
	int gFloorInAct = 1;     // 1..3
	bool gVictory = false;
	float gBossHpMax = 0.0f; // 只用于 HUD 血条

	int gFloor = 1;          // 全局层数（存档 / 难度曲线用）
	int gKills = 0;
	bool gDead = false;
	float gMsgTimer = 0.0f;
	const char* gMsg = "";

	// ---------------------------------------------------------------- 工具
	bool solidTile(int tx, int ty)
	{
		Block* b = gMap.getBlockSafe(tx, ty);
		if (!b) { return true; }                     // 地图外当墙
		return b->type == Block::stoneBricks;
	}

	// Bresenham 视线：中间被墙挡住就看不见
	bool lineOfSight(int x0, int y0, int x1, int y1)
	{
		int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
		int sx = (x0 < x1) ? 1 : -1;
		int sy = (y0 < y1) ? 1 : -1;
		int err = dx - dy;

		while (!(x0 == x1 && y0 == y1))
		{
			int e2 = 2 * err;
			if (e2 > -dy) { err -= dy; x0 += sx; }
			if (e2 < dx) { err += dx; y0 += sy; }

			if (x0 == x1 && y0 == y1) { break; }
			if (solidTile(x0, y0)) { return false; }
		}
		return true;
	}

	// 重算视野（只在玩家换格时调用）
	void refreshFOV(bool force = false)
	{
		const int n = gMap.w * gMap.h;
		if (n <= 0) { return; }
		if ((int)gVisible.size() != n) { gVisible.assign(n, 0); gExplored.assign(n, 0); }

		int px = (int)floorf(gPlayer.pos.x);
		int py = (int)floorf(gPlayer.pos.y);
		if (!force && px == lastPX && py == lastPY) { return; }
		lastPX = px;
		lastPY = py;

		std::fill(gVisible.begin(), gVisible.end(), (unsigned char)0);

		const int R = 8;
		for (int dy = -R; dy <= R; dy++)
		{
			for (int dx = -R; dx <= R; dx++)
			{
				if (dx * dx + dy * dy > R * R) { continue; }
				int tx = px + dx, ty = py + dy;
				if (tx < 0 || ty < 0 || tx >= gMap.w || ty >= gMap.h) { continue; }
				if (!lineOfSight(px, py, tx, ty)) { continue; }

				int idx = ty * gMap.w + tx;
				gVisible[idx] = 1;
				gExplored[idx] = 1;
			}
		}
	}

	bool blockedAt(Vector2 p, float half)
	{
		return solidTile((int)floorf(p.x - half), (int)floorf(p.y - half)) ||
		       solidTile((int)floorf(p.x + half), (int)floorf(p.y - half)) ||
		       solidTile((int)floorf(p.x - half), (int)floorf(p.y + half)) ||
		       solidTile((int)floorf(p.x + half), (int)floorf(p.y + half));
	}

	float dist2(Vector2 a, Vector2 b)
	{
		float dx = a.x - b.x, dy = a.y - b.y;
		return dx * dx + dy * dy;
	}

	void setMsg(const char* m, float t = 2.0f)
	{
		// 一定要复制：TextFormat 返回的是 raylib 的静态缓冲区，
		// 之后任何 DrawFPS/TextFormat 调用都会把它的内容冲掉。
		static char sBuf[160];
		snprintf(sBuf, sizeof(sBuf), "%s", m);
		gMsg = sBuf;
		gMsgTimer = t;
	}

	void spawnEntities(int act, int floorInAct);   // 前置声明（doLoad 会用）
	void enterFloor(int act, int floorInAct);
	bool isBossFloor(int floorInAct) { return floorInAct >= kFloorsPerAct; }

	// ---------------------------------------------------------------- 存 / 读档
	void doSave(bool silent = false)
	{
		SaveSnapshot snap;
		snap.floor = gFloor;
		snap.act = gAct;
		snap.floorInAct = gFloorInAct;
		snap.kills = gKills;
		snap.coins = gPlayer.coins;
		snap.hp = gPlayer.hp;
		snap.playerPos = gPlayer.pos;
		snap.explored = gExplored;

		snap.enemies.clear();
		snap.enemies.reserve(gEnemies.size());
		for (const auto& e : gEnemies)
		{
			SavedEnemy se;
			se.x = e.pos.x; se.y = e.pos.y;
			se.hp = e.hp;
			se.type = e.type;
			se.boss = e.boss ? 1 : 0;
			snap.enemies.push_back(se);
		}

		snap.coinsOnFloor.clear();
		snap.coinsOnFloor.reserve(gCoins.size());
		for (const auto& c : gCoins)
		{
			SavedCoin sc;
			sc.x = c.pos.x; sc.y = c.pos.y; sc.bob = c.bob;
			snap.coinsOnFloor.push_back(sc);
		}

		if (saveGame(gMap, snap, kSavePath))
		{
			if (!silent) { setMsg("已保存", 1.2f); }
		}
		else
		{
			setMsg("保存失败", 1.5f);
		}
	}

	void doLoad(bool onInit = false)
	{
		SaveSnapshot snap;
		if (!loadGame(gMap, snap, kSavePath))
		{
			if (!onInit) { setMsg("没有存档或存档损坏", 1.8f); }
			return;
		}

		gFloor = snap.floor;
		gAct = (snap.act >= 1) ? snap.act : 1;
		gFloorInAct = (snap.floorInAct >= 1) ? snap.floorInAct : 1;
		gKills = snap.kills;
		gPlayer.pos = snap.playerPos;
		gPlayer.hp = snap.hp;
		gPlayer.coins = snap.coins;
		gPlayer.invuln = 0.0f;
		gDead = false;

		int n = gMap.w * gMap.h;
		gVisible.assign(n, 0);
		gExplored.assign(n, 0);
		if ((int)snap.explored.size() == n) { gExplored = snap.explored; }

		// 敌人与金币从存档还原
		gEnemies.clear();
		gCoins.clear();

		for (const auto& se : snap.enemies)
		{
			Enemy e;
			e.pos = { se.x, se.y };
			e.hp = se.hp;
			e.maxHp = se.hp;
			e.type = se.type;
			e.boss = (se.boss != 0);
			if (e.boss) { gBossHpMax = (float)e.hp; }
			gEnemies.push_back(e);
		}
		for (const auto& sc : snap.coinsOnFloor)
		{
			Coin c;
			c.pos = { sc.x, sc.y };
			c.bob = sc.bob;
			gCoins.push_back(c);
		}

		// 空档（比如刚生成的层还没存）兜底
		if (gEnemies.empty() && gCoins.empty()) { spawnEntities(gAct, gFloorInAct); }

		lastPX = -999;
		lastPY = -999;
		refreshFOV(true);

		gCam.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
		gCam.zoom = gSettings.zoom;
		gCam.target = gPlayer.pos;

		setMsg(onInit ? "读取存档，继续冒险" : "已读取存档", 2.0f);
	}

	// ---------------------------------------------------------------- 铺敌人与金币
	void spawnEntities(int act, int floorInAct)
	{
		gEnemies.clear();
		gCoins.clear();
		gBossHpMax = 0.0f;

		Vector2 spawn = gPlayer.pos;
		bool bossFloor = isBossFloor(floorInAct);

		// ---- 普通杂兵：章节越深越多越硬 ----
		int enemyCount = (bossFloor ? 1 : 2) + act + floorInAct;
		for (int i = 0; i < enemyCount; i++)
		{
			int tx = 1, ty = 1;
			bool placed = false;
			for (int tries = 0; tries < 300; tries++)
			{
				tx = 1 + rand() % (gMap.w - 2);
				ty = 1 + rand() % (gMap.h - 2);
				Block* b = gMap.getBlockSafe(tx, ty);
				if (!b || b->type != Block::air) { continue; }
				Vector2 p{ tx + 0.5f, ty + 0.5f };
				if (dist2(p, spawn) < 36.0f) { continue; }
				placed = true;
				break;
			}
			if (!placed) { continue; }

			Enemy e;
			e.pos = { tx + 0.5f, ty + 0.5f };
			e.type = (act - 1 + i) % 4;
			e.maxHp = 24 + act * 12 + floorInAct * 6;
			e.hp = e.maxHp;
			gEnemies.push_back(e);
		}

		// ---- 章节 Boss：第 3 层 & 最终层 ----
		if (bossFloor)
		{
			Enemy b;
			b.boss = true;
			b.type = (act <= 3) ? (act - 1) : 3;
			b.maxHp = (act <= 3) ? (260 + act * 140) : 1600;
			b.hp = b.maxHp;
			b.pos = { spawn.x, spawn.y - 6.0f };   // 落在玩家上方，进画面

			Block* bb = gMap.getBlockSafe((int)b.pos.x, (int)b.pos.y);
			if (!bb || bb->type != Block::air) { b.pos = spawn; b.pos.y -= 3.0f; }

			gBossHpMax = (float)b.maxHp;
			gEnemies.push_back(b);
		}

		// ---- 金币 ----
		int coinCount = bossFloor ? 6 : 40;
		for (int i = 0; i < coinCount; i++)
		{
			int tx = 1 + rand() % (gMap.w - 2);
			int ty = 1 + rand() % (gMap.h - 2);
			Block* b = gMap.getBlockSafe(tx, ty);
			if (!b || b->type != Block::air) { continue; }
			gCoins.push_back(Coin{ { tx + 0.5f, ty + 0.5f }, (float)(rand() % 100) / 100.0f });
		}
	}

	// ---------------------------------------------------------------- 进入某一层
	void enterFloor(int act, int floorInAct)
	{
		gAct = act;
		gFloorInAct = floorInAct;
		gFloor = (act - 1) * kFloorsPerAct + floorInAct;

		bool bossFloor = isBossFloor(floorInAct);

		Vector2 spawn{};
		generateDungeon(gMap, spawn, bossFloor);

		// 只重置位置与血量，金币 / 击杀 / 章节进度保留
		int keepCoins = gPlayer.coins;
		gPlayer = Player{};
		gPlayer.coins = keepCoins;
		gPlayer.pos = spawn;
		gPlayer.hp = kPlayerMaxHp;

		spawnEntities(act, floorInAct);

		int fogCount = gMap.w * gMap.h;
		gVisible.assign(fogCount, 0);
		gExplored.assign(fogCount, 0);
		lastPX = -999;
		lastPY = -999;
		refreshFOV(true);

		gCam.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
		gCam.zoom = bossFloor ? (gSettings.zoom * 0.62f) : gSettings.zoom;
		gCam.rotation = 0.0f;

		if (act > kActs)
		{
			setMsg("最终决战 —— 击败远古之影", 2.5f);
		}
		else if (bossFloor)
		{
			setMsg(TextFormat("第 %d 章 · 首领层", act), 2.5f);
		}
		else
		{
			setMsg(TextFormat("第 %d 章 · 第 %d 层", act, floorInAct), 2.0f);
		}
	}

	// 打完 Boss：进下一章 / 通关
	void onBossDefeated()
	{
		if (gAct > kActs)
		{
			gVictory = true;
			setMsg("通关！", 5.0f);
			return;
		}

		if (gAct == kActs)
		{
			enterFloor(kActs + 1, 1);      // 最终 Boss
			return;
		}

		enterFloor(gAct + 1, 1);
		setMsg(TextFormat("进入第 %d 章", gAct), 2.5f);
	}

	// ---------------------------------------------------------------- 绘制
	void drawTiles()
	{
		int x0 = (int)floorf(gCam.target.x - GetScreenWidth() / (2.0f * gCam.zoom)) - 1;
		int x1 = (int)ceilf(gCam.target.x + GetScreenWidth() / (2.0f * gCam.zoom)) + 1;
		int y0 = (int)floorf(gCam.target.y - GetScreenHeight() / (2.0f * gCam.zoom)) - 1;
		int y1 = (int)ceilf(gCam.target.y + GetScreenHeight() / (2.0f * gCam.zoom)) + 1;

		x0 = (int)fmaxf(0.0f, (float)x0);
		y0 = (int)fmaxf(0.0f, (float)y0);
		x1 = (int)fminf((float)gMap.w - 1, (float)x1);
		y1 = (int)fminf((float)gMap.h - 1, (float)y1);

		Rectangle one{ 0.0f, 0.0f, 16.0f, 16.0f };

		for (int y = y0; y <= y1; y++)
		{
			for (int x = x0; x <= x1; x++)
			{
				int idx = y * gMap.w + x;
				if (idx < 0 || idx >= (int)gExplored.size()) { continue; }
				if (!gExplored[idx]) { continue; }          // 没探索过：不画（保持黑）
				bool vis = (gVisible[idx] != 0);

				Block& b = gMap.getBlocUnsafe(x, y);
				bool isWall = (b.type == Block::stoneBricks);

				// 按章节选贴图：1 章 floor1/wall1，2 章 floor2/wall2，3 章 floor3/wall3，最终层 floor4
				Texture2D texFloor = assetManager.floor;
				Texture2D texWall = assetManager.wall;
				if (gAct == 2) { texFloor = assetManager.floor2; texWall = assetManager.wall2; }
				else if (gAct == 3) { texFloor = assetManager.floor3; texWall = assetManager.wall3; }
				else if (gAct > 3) { texFloor = assetManager.floor4; texWall = assetManager.wall4; }

				// 可见 = 正常；探索过但看不见 = 压暗
				Color tint;
				if (isWall) { tint = vis ? Color{ 120, 90, 90, 255 } : Color{ 40, 30, 32, 255 }; }
				else        { tint = vis ? Color{ 210, 200, 190, 255 } : Color{ 74, 70, 66, 255 }; }

				DrawTexturePro(isWall ? texWall : texFloor,
				               one,
				               { (float)x, (float)y, 1.0f, 1.0f },
				               { 0.0f, 0.0f }, 0.0f, tint);

				if (!vis) { continue; }                     // 记忆里的门/箱子不画细节

				if (b.type == Block::door)
				{
					DrawTexturePro(assetManager.trapdoor,
					               { 0.0f, 0.0f, 33.0f, 32.0f },
					               { (float)x + 0.02f, (float)y + 0.25f, 0.96f, 0.75f },
					               { 0.0f, 0.0f }, 0.0f, WHITE);
				}
				else if (b.type == Block::woodenChest)
				{
					DrawTexturePro(assetManager.chest,
					               { 0.0f, 0.0f, 32.0f, 24.0f },
					               { (float)x + 0.12f, (float)y + 0.35f, 0.76f, 0.58f },
					               { 0.0f, 0.0f }, 0.0f, WHITE);
				}
			}
		}
	}

	void drawPlayer()
	{
		int dir = gPlayer.dir;
		bool walking = gPlayer.moving;
		Texture2D tex = walking ? assetManager.playerWalk[dir] : assetManager.playerIdle[dir];

		int frames = walking ? 6 : 4;
		int frame = (int)(gPlayer.anim * 10.0f) % frames;

		Rectangle src{ (float)(frame * 32), 0.0f, 32.0f, 32.0f };
		if (gPlayer.flip) { src.width = -32.0f; }

		// 受击闪烁
		Color tint = (gPlayer.invuln > 0.0f && ((int)(gPlayer.invuln * 20) % 2 == 0)) ? RED : WHITE;

		DrawTexturePro(tex, src,
		               { gPlayer.pos.x - kPlayerDraw / 2, gPlayer.pos.y - kPlayerDraw * 0.75f,
		                 kPlayerDraw, kPlayerDraw },
		               { 0.0f, 0.0f }, 0.0f, tint);

		// 挥剑：朝向一个淡色扇形（用两个圆近似）
		if (gPlayer.atkFlash > 0.0f)
		{
			Vector2 fwd = { 0.0f, 0.0f };
			if (gPlayer.dir == 0) { fwd = { 0.0f, 1.0f }; }
			else if (gPlayer.dir == 2) { fwd = { 0.0f, -1.0f }; }
			else { fwd = { gPlayer.flip ? -1.0f : 1.0f, 0.0f }; }

			Vector2 c = { gPlayer.pos.x + fwd.x * 0.9f, gPlayer.pos.y + fwd.y * 0.9f };
			DrawCircleV(c, 0.55f, Fade(WHITE, 0.35f));
			DrawCircleV(c, 0.32f, Fade(YELLOW, 0.5f));
		}
	}

	void drawEnemies()
	{
		for (auto& e : gEnemies)
		{
			int et = (int)floorf(e.pos.y) * gMap.w + (int)floorf(e.pos.x);
			if (et < 0 || et >= (int)gVisible.size() || !gVisible[et]) { continue; }

			Texture2D tex = assetManager.enemyWalk[e.type];
			int frame = (int)(e.anim * 8.0f) % 6;
			Rectangle src{ (float)(frame * 32), 0.0f, 32.0f, 32.0f };

			Color tint = (e.hurt > 0.0f) ? RED : WHITE;
			if (e.boss) { tint = (e.hurt > 0.0f) ? Color{ 255, 160, 160, 255 } : Color{ 255, 210, 210, 255 }; }

			float size = e.boss ? 2.6f : 1.0f;
			DrawTexturePro(tex, src,
			               { e.pos.x - size * 0.5f, e.pos.y - size * 0.7f, size, size },
			               { 0.0f, 0.0f }, 0.0f, tint);
		}
	}

	void drawCoins()
	{
		for (auto& c : gCoins)
		{
			int ci = (int)floorf(c.pos.y) * gMap.w + (int)floorf(c.pos.x);
			if (ci < 0 || ci >= (int)gVisible.size() || !gVisible[ci]) { continue; }

			float bob = sinf(c.bob * 6.0f) * 0.06f;
			DrawTexturePro(assetManager.coin,
			               { 0.0f, 0.0f, 10.0f, 10.0f },
			               { c.pos.x - 0.15f, c.pos.y - 0.15f + bob, 0.3f, 0.3f },
			               { 0.0f, 0.0f }, 0.0f, WHITE);
		}
	}

	// ---------------------------------------------------------------- 逻辑
	void movePlayer(float dt)
	{
		Vector2 dir{ 0.0f, 0.0f };
		if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { dir.x -= 1.0f; }
		if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { dir.x += 1.0f; }
		if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    { dir.y -= 1.0f; }
		if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { dir.y += 1.0f; }

		float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
		gPlayer.moving = (len > 0.0f);

		if (len > 0.0f)
		{
			dir.x /= len;
			dir.y /= len;

			// 朝向：优先上下，其次左右
			if (fabsf(dir.x) > fabsf(dir.y))
			{
				gPlayer.dir = 1;
				gPlayer.flip = (dir.x < 0.0f);
			}
			else
			{
				gPlayer.dir = (dir.y > 0.0f) ? 0 : 2;
				gPlayer.flip = false;
			}
			gPlayer.anim += dt;

			// 分轴移动，避免贴墙卡死
			Vector2 next = gPlayer.pos;
			next.x += dir.x * kPlayerSpeed * dt;
			if (!blockedAt(next, kPlayerHalf)) { gPlayer.pos.x = next.x; }

			next = gPlayer.pos;
			next.y += dir.y * kPlayerSpeed * dt;
			if (!blockedAt(next, kPlayerHalf)) { gPlayer.pos.y = next.y; }
		}
		else
		{
			gPlayer.anim += dt * 0.6f;
		}
	}

	void tryAttack()
	{
		if (gPlayer.atkCd > 0.0f) { return; }

		if (!(IsKeyPressed(KEY_J) || IsKeyPressed(KEY_SPACE))) { return; }

		gPlayer.atkCd = kAttackCd;
		gPlayer.atkFlash = 0.12f;
		sfxPlay(SFX_SWING, gSettings.volume);

		Vector2 fwd{ 0.0f, 0.0f };
		if (gPlayer.dir == 0) { fwd = { 0.0f, 1.0f }; }
		else if (gPlayer.dir == 2) { fwd = { 0.0f, -1.0f }; }
		else { fwd = { gPlayer.flip ? -1.0f : 1.0f, 0.0f }; }

		bool hitAny = false;
		for (auto& e : gEnemies)
		{
			Vector2 d{ e.pos.x - gPlayer.pos.x, e.pos.y - gPlayer.pos.y };
			float dist = sqrtf(d.x * d.x + d.y * d.y);
			if (dist > kAttackRange) { continue; }

			// 必须在面朝方向
			float dot = (dist > 0.001f) ? (d.x * fwd.x + d.y * fwd.y) / dist : 1.0f;
			if (dot < 0.25f) { continue; }

			e.hp -= kAttackDamage;
			e.hurt = 0.15f;
			e.pos.x += fwd.x * 0.35f;
			e.pos.y += fwd.y * 0.35f;
			if (!hitAny) { sfxPlay(SFX_HIT, gSettings.volume); }
			hitAny = true;
		}

		if (hitAny) { setMsg("命中！", 0.6f); }
	}

	void updateEnemies(float dt)
	{
		for (size_t i = 0; i < gEnemies.size();)
		{
			Enemy& e = gEnemies[i];
			e.anim += dt;
			if (e.hurt > 0.0f) { e.hurt -= dt; }

			Vector2 d{ gPlayer.pos.x - e.pos.x, gPlayer.pos.y - e.pos.y };
			float dist = sqrtf(d.x * d.x + d.y * d.y);

			if (dist < 9.0f && dist > 0.001f)
			{
				Vector2 step{ d.x / dist * kEnemySpeed * dt, d.y / dist * kEnemySpeed * dt };
				Vector2 np{ e.pos.x + step.x, e.pos.y + step.y };
				if (!blockedAt(np, 0.3f)) { e.pos = np; }
				else if (!blockedAt({ e.pos.x + step.x, e.pos.y }, 0.3f)) { e.pos.x += step.x; }
				else if (!blockedAt({ e.pos.x, e.pos.y + step.y }, 0.3f)) { e.pos.y += step.y; }
			}

			// 贴身伤害
			if (dist < kEnemyTouchDist && gPlayer.invuln <= 0.0f && !gDead)
			{
				gPlayer.hp -= (int)kEnemyDamage;
				gPlayer.invuln = kInvulnTime;
				if (gPlayer.hp <= 0)
				{
					gPlayer.hp = 0;
					gDead = true;
					sfxPlay(SFX_DEATH, gSettings.volume);
				}
			}

			if (e.hp <= 0)
			{
				sfxPlay(SFX_DEATH, gSettings.volume * 0.6f);
				gKills++;
				gPlayer.coins += e.boss ? 30 : 2;

				bool wasBoss = e.boss;
				gEnemies.erase(gEnemies.begin() + (long)i);

				if (wasBoss)
				{
					onBossDefeated();
					return;              // 地图已换，别再遍历旧容器
				}
				continue;
			}
			i++;
		}
	}

	void updatePickups()
	{
		for (size_t i = 0; i < gCoins.size();)
		{
			gCoins[i].bob += GetFrameTime();
			if (dist2(gCoins[i].pos, gPlayer.pos) < 0.36f)
			{
				gPlayer.coins++;
				sfxPlay(SFX_PICKUP, gSettings.volume);
				gCoins.erase(gCoins.begin() + (long)i);
				continue;
			}
			i++;
		}
	}

	void interact()
	{
		if (!IsKeyPressed(KEY_E)) { return; }

		// 先看脚下/身边有没有活板门或宝箱
		int px = (int)floorf(gPlayer.pos.x);
		int py = (int)floorf(gPlayer.pos.y);

		for (int dy = -1; dy <= 1; dy++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				Block* b = gMap.getBlockSafe(px + dx, py + dy);
				if (!b) { continue; }

				float cx = px + dx + 0.5f, cy = py + dy + 0.5f;
				if (dist2({ cx, cy }, gPlayer.pos) > 1.2f) { continue; }

				if (b->type == Block::door)
				{
					sfxPlay(SFX_DESCEND, gSettings.volume);
					enterFloor(gAct, gFloorInAct + 1);
					return;
				}
				if (b->type == Block::woodenChest)
				{
					b->type = Block::air;
					gPlayer.coins += 6;
					sfxPlay(SFX_PICKUP, gSettings.volume);
					setMsg("宝箱 +6 金币", 1.5f);
					return;
				}
			}
		}
	}

	void drawHUD()
	{
		ImGuiIO& io = ImGui::GetIO();
		const float W = io.DisplaySize.x;
		const float H = io.DisplaySize.y;
		char buf[96];

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(W, H));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		ImGui::Begin("##hud", nullptr,
		             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs |
		             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImFont* font = ImGui::GetFont();

		// ---------------- 左上：状态面板 ----------------
		dl->AddRectFilled({ 16, 16 }, { 306, 108 }, IM_COL32(9, 11, 18, 210), 10.0f);
		dl->AddRect({ 16, 16 }, { 306, 108 }, IM_COL32(80, 88, 122, 255), 10.0f, 0, 2.0f);

		// 血条
		float frac = (float)gPlayer.hp / (float)kPlayerMaxHp;
		if (frac < 0.0f) { frac = 0.0f; }
		const float barL = 36.0f, barR = 286.0f, barT = 34.0f, barB = 58.0f;
		dl->AddRectFilled({ barL, barT }, { barR, barB }, IM_COL32(44, 18, 22, 255), 5.0f);
		dl->AddRectFilled({ barL, barT }, { barL + (barR - barL) * frac, barB },
		                  frac > 0.35f ? IM_COL32(206, 62, 62, 255) : IM_COL32(232, 128, 44, 255), 5.0f);
		dl->AddRect({ barL, barT }, { barR, barB }, IM_COL32(122, 62, 62, 255), 5.0f);

		snprintf(buf, sizeof(buf), "HP  %d / %d", gPlayer.hp, kPlayerMaxHp);
		ImVec2 ts = ImGui::CalcTextSize(buf);
		dl->AddText({ barL + ((barR - barL) - ts.x) * 0.5f, barT + 1.0f },
		            IM_COL32(248, 242, 236, 255), buf);

		// 金币 + 击杀 + 敌人
		if (assetManager.coin.id != 0)
		{
			dl->AddImage((ImTextureID)(intptr_t)assetManager.coin.id,
			             ImVec2(36, 68), ImVec2(60, 92));
		}
		snprintf(buf, sizeof(buf), "%d", gPlayer.coins);
		dl->AddText({ 68, 70 }, IM_COL32(250, 214, 110, 255), buf);

		snprintf(buf, sizeof(buf), "击杀 %d", gKills);
		dl->AddText({ 140, 70 }, IM_COL32(198, 204, 218, 255), buf);

		snprintf(buf, sizeof(buf), "敌人 %d", (int)gEnemies.size());
		dl->AddText({ 226, 70 }, IM_COL32(198, 204, 218, 255), buf);

		// ---------------- 顶部居中：层数 ----------------
		if (gAct > kActs) { snprintf(buf, sizeof(buf), "最终决战"); }
		else { snprintf(buf, sizeof(buf), "第 %d 章 · 第 %d 层", gAct, gFloorInAct); }
		ts = ImGui::CalcTextSize(buf);
		dl->AddText(font, 26.0f, ImVec2((W - ts.x * 26.0f / ts.y * 0.72f) * 0.5f + 1, 17),
		            IM_COL32(0, 0, 0, 190), buf);
		dl->AddText(font, 26.0f, ImVec2((W - ts.x * 26.0f / ts.y * 0.72f) * 0.5f, 16),
		            IM_COL32(246, 236, 210, 255), buf);

		// ---------------- 顶部：Boss 血条 ----------------
		{
			float bossFrac = -1.0f;
			for (const auto& e : gEnemies)
			{
				if (e.boss) { bossFrac = (e.maxHp > 0) ? (float)e.hp / (float)e.maxHp : 0.0f; break; }
			}

			if (bossFrac >= 0.0f)
			{
				float bw = 520.0f;
				float bx = (W - bw) * 0.5f;
				float by = 66.0f;

				if (bossFrac < 0.0f) { bossFrac = 0.0f; }

				dl->AddRectFilled({ bx - 4, by - 4 }, { bx + bw + 4, by + 20 }, IM_COL32(0, 0, 0, 190), 7.0f);
				dl->AddRectFilled({ bx, by }, { bx + bw, by + 16 }, IM_COL32(46, 16, 26, 255), 6.0f);
				dl->AddRectFilled({ bx, by }, { bx + bw * bossFrac, by + 16 }, IM_COL32(198, 48, 118, 255), 6.0f);
				dl->AddRect({ bx, by }, { bx + bw, by + 16 }, IM_COL32(150, 90, 120, 255), 6.0f);

				const char* nm = (gAct > kActs) ? "远古之影" : "章节首领";
				ImVec2 ns = ImGui::CalcTextSize(nm);
				dl->AddText({ (W - ns.x) * 0.5f, by - 22.0f }, IM_COL32(250, 180, 205, 255), nm);
			}
		}

		// ---------------- 右上：小地图 ----------------
		if (gSettings.showMinimap)
		{
			const float cell = 3.0f;
			const float mw = gMap.w * cell;
			const float mh = gMap.h * cell;
			ImVec2 m0(W - mw - 26.0f, 16.0f);
			ImVec2 m1(m0.x + mw + 10.0f, m0.y + mh + 10.0f);

			dl->AddRectFilled(m0, m1, IM_COL32(8, 9, 14, 225), 8.0f);
			dl->AddRect(m0, m1, IM_COL32(80, 88, 122, 255), 8.0f, 0, 2.0f);

			ImVec2 o(m0.x + 5.0f, m0.y + 5.0f);

			for (int y = 0; y < gMap.h; y++)
			{
				for (int x = 0; x < gMap.w; x++)
				{
					int idx = y * gMap.w + x;
					if (idx >= (int)gExplored.size() || !gExplored[idx]) { continue; }

					bool wall = (gMap.mapData[idx].type == Block::stoneBricks);
					bool vis = (gVisible[idx] != 0);

					ImU32 col;
					if (wall) { col = vis ? IM_COL32(118, 122, 150, 255) : IM_COL32(44, 45, 58, 255); }
					else      { col = vis ? IM_COL32(198, 194, 178, 255) : IM_COL32(82, 82, 92, 255); }

					ImVec2 a(o.x + x * cell, o.y + y * cell);
					dl->AddRectFilled(a, ImVec2(a.x + cell - 0.6f, a.y + cell - 0.6f), col);

					if (gMap.mapData[idx].type == Block::door)
					{
						dl->AddRectFilled(a, ImVec2(a.x + cell, a.y + cell), IM_COL32(240, 200, 60, 255));
					}
				}
			}

			for (const auto& e : gEnemies)
			{
				int ei = (int)floorf(e.pos.y) * gMap.w + (int)floorf(e.pos.x);
				if (ei < 0 || ei >= (int)gVisible.size() || !gVisible[ei]) { continue; }
				ImVec2 a(o.x + floorf(e.pos.x) * cell, o.y + floorf(e.pos.y) * cell);
				dl->AddRectFilled(a, ImVec2(a.x + cell, a.y + cell), IM_COL32(226, 70, 70, 255));
			}

			{
				ImVec2 a(o.x + floorf(gPlayer.pos.x) * cell - 1.0f,
				         o.y + floorf(gPlayer.pos.y) * cell - 1.0f);
				dl->AddRectFilled(a, ImVec2(a.x + cell + 2.0f, a.y + cell + 2.0f),
				                  IM_COL32(90, 232, 132, 255));
			}
		}

		// ---------------- 底部提示 ----------------
		ImU32 dim = IM_COL32(170, 175, 190, 120);
		dl->AddText({ 18, H - 84 }, dim, "WASD 移动    J/空格 攻击    E 开箱/下楼");
		dl->AddText({ 18, H - 64 }, dim, "F5 保存   F9 读档   F8 新游戏   F10 设置");

		// ---------------- 消息提示 ----------------
		if (gMsgTimer > 0.0f)
		{
			float a = (gMsgTimer > 1.0f) ? 1.0f : gMsgTimer;
			ts = ImGui::CalcTextSize(gMsg);
			ImVec2 t0((W - ts.x) * 0.5f - 16.0f, H - 150.0f);
			ImVec2 t1((W + ts.x) * 0.5f + 16.0f, H - 150.0f + ts.y + 12.0f);
			dl->AddRectFilled(t0, t1, IM_COL32(0, 0, 0, (int)(170 * a)), 8.0f);
			dl->AddText({ t0.x + 16.0f, t0.y + 6.0f }, IM_COL32(255, 240, 205, (int)(255 * a)), gMsg);
		}

		// ---------------- 通关覆盖层 ----------------
		if (gVictory)
		{
			dl->AddRectFilled({ 0, 0 }, { W, H }, IM_COL32(6, 8, 14, 205));

			const char* title = "通  关  !";
			ts = ImGui::CalcTextSize(title);
			float big = 52.0f;
			float tw = ts.x * big / ts.y;
			dl->AddText(font, big, ImVec2((W - tw) * 0.5f + 2, H * 0.28f + 2),
			            IM_COL32(0, 0, 0, 220), title);
			dl->AddText(font, big, ImVec2((W - tw) * 0.5f, H * 0.28f),
			            IM_COL32(250, 210, 110, 255), title);

			snprintf(buf, sizeof(buf), "击败远古之影，击杀 %d，金币 %d", gKills, gPlayer.coins);
			ts = ImGui::CalcTextSize(buf);
			dl->AddText({ (W - ts.x) * 0.5f, H * 0.42f }, IM_COL32(230, 230, 236, 255), buf);
		}

		// ---------------- 死亡覆盖层 ----------------
		if (gDead)
		{
			dl->AddRectFilled({ 0, 0 }, { W, H }, IM_COL32(6, 6, 10, 195));

			const char* title = "你  死  了";
			ts = ImGui::CalcTextSize(title);
			float big = 44.0f;
			float tw = ts.x * big / ts.y;
			dl->AddText(font, big, ImVec2((W - tw) * 0.5f + 2, H * 0.30f + 2),
			            IM_COL32(0, 0, 0, 220), title);
			dl->AddText(font, big, ImVec2((W - tw) * 0.5f, H * 0.30f),
			            IM_COL32(232, 72, 72, 255), title);

			snprintf(buf, sizeof(buf), "到达第 %d 层    击杀 %d    金币 %d", gFloor, gKills, gPlayer.coins);
			ts = ImGui::CalcTextSize(buf);
			dl->AddText({ (W - ts.x) * 0.5f, H * 0.42f }, IM_COL32(226, 226, 232, 255), buf);
		}

		ImGui::End();
		ImGui::PopStyleColor();

		// ---------------- 设置窗口（F10）----------------
		if (gSettingsOpen)
		{
			ImGui::SetNextWindowPos(ImVec2(20, 330), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
			ImGui::Begin("设置  (F10)", &gSettingsOpen);
			if (ImGui::SliderFloat("缩放", &gSettings.zoom, 24.0f, 96.0f)) { gCam.zoom = gSettings.zoom; }
			ImGui::SliderFloat("音量", &gSettings.volume, 0.0f, 1.0f);
			ImGui::Checkbox("小地图", &gSettings.showMinimap);
			ImGui::Checkbox("显示 FPS", &gSettings.showFps);
			ImGui::Separator();
			if (ImGui::Button("应用并保存", ImVec2(-1, 30)))
			{
				gCam.zoom = gSettings.zoom;
				settingsSave(gSettings, kSettingsPath);
				setMsg("设置已保存", 1.2f);
			}
			ImGui::End();
		}

		// ---------------- 死亡 / 通关按钮 ----------------
		if (gDead || gVictory)
		{
			ImGui::SetNextWindowPos(ImVec2(W * 0.5f - 120.0f, H * 0.52f));
			ImGui::SetNextWindowSize(ImVec2(240, 0));
			ImGui::Begin("##deadbtn", nullptr,
			             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			             ImGuiWindowFlags_AlwaysAutoResize);
			if (ImGui::Button("重新开始  (R)", ImVec2(-1, 42)))
			{
				gKills = 0;
				gDead = false;
				gVictory = false;
				gPlayer.coins = 0;
				enterFloor(1, 1);
			}
			ImGui::End();
		}
	}
}

// ---------------------------------------------------------------- 生命周期
bool initGame()
{
	assetManager.loadAll();

	settingsLoad(gSettings, kSettingsPath);
	InitAudioDevice();
	sfxInit();

	gKills = 0;
	gDead = false;
	gVictory = false;
	enterFloor(1, 1);

	// 有存档就接着玩
	doLoad(true);
	return true;
}

bool updateGame()
{
	float dt = GetFrameTime();
	if (dt > 1.0f / 20.0f) { dt = 1.0f / 20.0f; }

	if (gMsgTimer > 0.0f) { gMsgTimer -= dt; }

	if (gDead)
	{
		if (IsKeyPressed(KEY_R))
		{
			gKills = 0;
			gDead = false;
			gVictory = false;
			gPlayer.coins = 0;
			enterFloor(1, 1);
		}
	}
	else
	{
		if (IsKeyPressed(KEY_F5)) { doSave(); }
		if (IsKeyPressed(KEY_F9)) { doLoad(); }
		if (IsKeyPressed(KEY_F8))
		{
			gKills = 0;
			gDead = false;
			gVictory = false;
			gPlayer.coins = 0;
			enterFloor(1, 1);
			setMsg("新游戏", 1.5f);
		}

		gPlayer.atkCd -= dt;
		gPlayer.invuln -= dt;
		gPlayer.atkFlash -= dt;

		movePlayer(dt);
		refreshFOV();
		tryAttack();
		updateEnemies(dt);
		updatePickups();
		interact();
	}

	// 相机平滑跟随
	gCam.target.x += (gPlayer.pos.x - gCam.target.x) * fminf(1.0f, dt * 12.0f);
	gCam.target.y += (gPlayer.pos.y - gCam.target.y) * fminf(1.0f, dt * 12.0f);

	// ---- 绘制世界 ----
	BeginMode2D(gCam);
	drawTiles();
	drawCoins();
	drawEnemies();
	drawPlayer();
	EndMode2D();

	// ---- HUD ----
	drawHUD();

	if (IsKeyPressed(KEY_F10)) { gSettingsOpen = !gSettingsOpen; }
	if (gSettings.showFps) { DrawFPS(10, GetScreenHeight() - 30); }

	return true;
}

void closeGame()
{
	if (!gDead) { doSave(true); }
	settingsSave(gSettings, kSettingsPath);
	sfxShutdown();
	if (IsAudioDeviceReady()) { CloseAudioDevice(); }
	assetManager.unloadAll();
}

void loadlevel() {}
void closelevel() {}
