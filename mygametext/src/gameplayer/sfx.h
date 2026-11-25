#pragma once

#include <raylib.h>

// 程序化合成音效（不依赖任何音频素材）：挥砍 / 命中 / 拾取 / 下楼 / 死亡
enum GameSfx
{
	SFX_SWING = 0,
	SFX_HIT,
	SFX_PICKUP,
	SFX_DESCEND,
	SFX_DEATH,
	SFX_COUNT
};

void sfxInit();
void sfxShutdown();
void sfxPlay(int id, float volume);
