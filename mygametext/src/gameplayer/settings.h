#pragma once

// 游戏设置（文本 key=value 存盘，对应课程 20-设置 / 22-基于文本的数据保存）
struct GameSettings
{
	float zoom = 56.0f;      // 相机缩放，32 ~ 96
	float volume = 0.7f;     // 音效音量，0 ~ 1
	bool  showMinimap = true;
	bool  showFps = true;
	bool  fullscreen = false;
};

void settingsLoad(GameSettings& s, const char* path);
bool settingsSave(const GameSettings& s, const char* path);
