#pragma once

#include <raylib.h>

// 地牢类 Rogue-like 用到的素材（craftpix top-down roguelike kit）
struct AssetManager
{
	// 玩家：0 = 朝下(D)，1 = 侧面(S)，2 = 朝上(U)
	Texture2D playerIdle[3] = {};
	Texture2D playerWalk[3] = {};

	// 敌人：e1 ~ e4 四种史莱姆
	Texture2D enemyIdle[4] = {};
	Texture2D enemyWalk[4] = {};

	// 地形与道具
	Texture2D floor = {};      // 第 1 章
	Texture2D wall = {};
	Texture2D floor2 = {};     // 第 2 章
	Texture2D wall2 = {};
	Texture2D floor3 = {};     // 第 3 章
	Texture2D wall3 = {};
	Texture2D floor4 = {};     // 最终 Boss 场
	Texture2D wall4 = {};
	Texture2D trapdoor = {};
	Texture2D chest = {};
	Texture2D coin = {};
	Texture2D shadow = {};

	void loadAll();
	void unloadAll();
};

// 取图集里第 index 帧（每帧 frameSize x frameSize）
Rectangle frameRect(int index, int frameSize);
