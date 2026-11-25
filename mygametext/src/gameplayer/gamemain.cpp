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
		int type = 0;            // 0~3 → e1~e4
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

	int gFloor = 1;
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
		gMsg = m;
		gMsgTimer = t;
	}

	void spawnEntities(int floorIndex);   // 前置声明（doLoad 会用）

	// ---------------------------------------------------------------- 存 / 读档
	void doSave(bool silent = false)
	{
		SaveSnapshot snap;
		snap.floor = gFloor;
		snap.kills = gKills;
		snap.coins = gPlayer.coins;
		snap.hp = gPlayer.hp;
		snap.playerPos = gPlayer.pos;
		snap.explored = gExplored;

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

		// 敌人与金币不入档，按当前层重新铺
		spawnEntities(gFloor);

		lastPX = -999;
		lastPY = -999;
		refreshFOV(true);

		gCam.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
		gCam.zoom = gSettings.zoom;
		gCam.target = gPlayer.pos;

		setMsg(onInit ? "读取存档，继续冒险" : "已读取存档", 2.0f);
	}

	// ---------------------------------------------------------------- 铺敌人与金币
	void spawnEntities(int floorIndex)
	{
		gEnemies.clear();
		gCoins.clear();

		Vector2 spawn = gPlayer.pos;

		int enemyCount = 3 + floorIndex;
		for (int i = 0; i < enemyCount; i++)
		{
			int tx = 1, ty = 1;
			for (int tries = 0; tries < 300; tries++)
			{
				tx = 1 + rand() % (gMap.w - 2);
				ty = 1 + rand() % (gMap.h - 2);
				Block* b = gMap.getBlockSafe(tx, ty);
				if (!b || b->type != Block::air) { continue; }
				Vector2 p{ tx + 0.5f, ty + 0.5f };
				if (dist2(p, spawn) < 36.0f) { continue; }
				break;
			}

			Enemy e;
			e.pos = { tx + 0.5f, ty + 0.5f };
			e.type = i % 4;
			e.hp = 30 + (floorIndex - 1) * 8;
			gEnemies.push_back(e);
		}

		for (int i = 0; i < 40; i++)
		{
			int tx = 1 + rand() % (gMap.w - 2);
			int ty = 1 + rand() % (gMap.h - 2);
			Block* b = gMap.getBlockSafe(tx, ty);
			if (!b || b->type != Block::air) { continue; }
			gCoins.push_back(Coin{ { tx + 0.5f, ty + 0.5f }, (float)(rand() % 100) / 100.0f });
		}
	}

	// ---------------------------------------------------------------- 生成一层
	void resetFloor(int floorIndex)
	{
		Vector2 spawn{};
		generateDungeon(gMap, spawn);

		gPlayer = Player{};
		gPlayer.pos = spawn;
		gPlayer.hp = kPlayerMaxHp;

		spawnEntities(floorIndex);

		int fogCount = gMap.w * gMap.h;
		gVisible.assign(fogCount, 0);
		gExplored.assign(fogCount, 0);
		lastPX = -999; lastPY = -999;
		refreshFOV(true);

		// 敌人：越深越多，每种史莱姆轮换
		int enemyCount = 0;
		for (int i = 0; i < enemyCount; i++)
		{
			int tx = 0, ty = 0;
			for (int tries = 0; tries < 200; tries++)
			{
				tx = 1 + rand() % (gMap.w - 2);
				ty = 1 + rand() % (gMap.h - 2);
				Block* b = gMap.getBlockSafe(tx, ty);
				if (!b || b->type != Block::air) { continue; }
				Vector2 p{ tx + 0.5f, ty + 0.5f };
				if (dist2(p, spawn) < 36.0f) { continue; }   // 出生点附近不放
				break;
			}

			Enemy e;
			e.pos = { tx + 0.5f, ty + 0.5f };
			e.type = i % 4;
			e.hp = 30 + (floorIndex - 1) * 8;
			gEnemies.push_back(e);
		}

		// 金币：每个房间 2 枚
		for (int i = 0; i < 40; i++)
		{
			int tx = 1 + rand() % (gMap.w - 2);
			int ty = 1 + rand() % (gMap.h - 2);
			Block* b = gMap.getBlockSafe(tx, ty);
			if (!b || b->type != Block::air) { continue; }
			gCoins.push_back(Coin{ { tx + 0.5f, ty + 0.5f }, (float)(rand() % 100) / 100.0f });
		}

		gCam.offset = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
		gCam.zoom = gSettings.zoom;
		gCam.rotation = 0.0f;

		setMsg(TextFormat("第 %d 层", floorIndex), 2.0f);
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

				// 可见 = 正常；探索过但看不见 = 压暗
				Color tint;
				if (isWall) { tint = vis ? Color{ 62, 64, 82, 255 } : Color{ 26, 27, 35, 255 }; }
				else        { tint = vis ? Color{ 208, 203, 188, 255 } : Color{ 74, 74, 70, 255 }; }

				DrawTexturePro(isWall ? assetManager.wall : assetManager.floor,
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

			DrawTexturePro(tex, src,
			               { e.pos.x - 0.5f, e.pos.y - 0.7f, 1.0f, 1.0f },
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
				gPlayer.coins += 2;
				gEnemies.erase(gEnemies.begin() + (long)i);
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
					gFloor++;
					resetFloor(gFloor);
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
		ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Always);   // 0 = 高度自适应
		ImGui::Begin("Dungeon Echo");
		ImGui::Text("第 %d 层", gFloor);
		ImGui::Text("HP %d / %d", gPlayer.hp, kPlayerMaxHp);
		ImGui::ProgressBar((float)gPlayer.hp / (float)kPlayerMaxHp, ImVec2(-1, 12), "");
		ImGui::Text("金币 %d    击杀 %d    敌人 %d", gPlayer.coins, gKills, (int)gEnemies.size());
		ImGui::Separator();
		ImGui::TextDisabled("WASD 移动    J/空格 攻击");
		ImGui::TextDisabled("E 宝箱/活板门   F5 存  F9 读  F8 新  F10 设置");
		if (gMsgTimer > 0.0f)
		{
			ImGui::Separator();
			ImGui::TextUnformatted(gMsg);
		}
		ImGui::End();

		// ---------------- 小地图 ----------------
		if (gSettings.showMinimap)
		{
		ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() - 250.0f, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(230, 175), ImGuiCond_FirstUseEver);
		ImGui::Begin("小地图");
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 o = ImGui::GetCursorScreenPos();
			float cell = 3.0f;

			for (int y = 0; y < gMap.h; y++)
			{
				for (int x = 0; x < gMap.w; x++)
				{
					int idx = y * gMap.w + x;
					if (idx >= (int)gExplored.size() || !gExplored[idx]) { continue; }

					bool wall = (gMap.mapData[idx].type == Block::stoneBricks);
					bool vis = (gVisible[idx] != 0);

					ImU32 col;
					if (wall) { col = vis ? IM_COL32(120, 122, 150, 255) : IM_COL32(45, 46, 58, 255); }
					else      { col = vis ? IM_COL32(196, 192, 176, 255) : IM_COL32(80, 80, 88, 255); }

					dl->AddRectFilled(ImVec2(o.x + x * cell, o.y + y * cell),
					                  ImVec2(o.x + x * cell + cell - 0.6f, o.y + y * cell + cell - 0.6f), col);

					if (gMap.mapData[idx].type == Block::door && gExplored[idx])
					{
						dl->AddRectFilled(ImVec2(o.x + x * cell, o.y + y * cell),
						                  ImVec2(o.x + x * cell + cell, o.y + y * cell + cell),
						                  IM_COL32(240, 200, 60, 255));
					}
				}
			}

			// 可见的敌人
			for (auto& e : gEnemies)
			{
				int ei = (int)floorf(e.pos.y) * gMap.w + (int)floorf(e.pos.x);
				if (ei < 0 || ei >= (int)gVisible.size() || !gVisible[ei]) { continue; }
				dl->AddRectFilled(ImVec2(o.x + floorf(e.pos.x) * cell + 0.5f, o.y + floorf(e.pos.y) * cell + 0.5f),
				                  ImVec2(o.x + floorf(e.pos.x) * cell + cell, o.y + floorf(e.pos.y) * cell + cell),
				                  IM_COL32(230, 70, 70, 255));
			}

			// 玩家
			dl->AddRectFilled(ImVec2(o.x + floorf(gPlayer.pos.x) * cell - 0.5f,
			                         o.y + floorf(gPlayer.pos.y) * cell - 0.5f),
			                  ImVec2(o.x + floorf(gPlayer.pos.x) * cell + cell + 0.5f,
			                         o.y + floorf(gPlayer.pos.y) * cell + cell + 0.5f),
			                  IM_COL32(90, 230, 130, 255));

			ImGui::Dummy(ImVec2(gMap.w * cell, gMap.h * cell));
		}
		ImGui::End();
		}   // showMinimap

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

		if (gDead)
		{
			ImGui::SetNextWindowSize(ImVec2(320, 140), ImGuiCond_Always);
			ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2.0f - 160,
			                               GetScreenHeight() / 2.0f - 70), ImGuiCond_Always);
			ImGui::Begin("你死了", nullptr,
			             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			             ImGuiWindowFlags_NoCollapse);
			ImGui::Text("到达第 %d 层，击杀 %d，金币 %d", gFloor, gKills, gPlayer.coins);
			if (ImGui::Button("重新开始 (R)", ImVec2(-1, 34)))
			{
				gFloor = 1;
				gKills = 0;
				gDead = false;
				resetFloor(1);
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

	gFloor = 1;
	gKills = 0;
	gDead = false;
	resetFloor(1);

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
			gFloor = 1;
			gKills = 0;
			gDead = false;
			resetFloor(1);
		}
	}
	else
	{
		if (IsKeyPressed(KEY_F5)) { doSave(); }
		if (IsKeyPressed(KEY_F9)) { doLoad(); }
		if (IsKeyPressed(KEY_F8))
		{
			gFloor = 1;
			gKills = 0;
			gDead = false;
			resetFloor(1);
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
