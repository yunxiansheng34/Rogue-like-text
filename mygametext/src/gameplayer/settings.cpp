#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

void settingsLoad(GameSettings& s, const char* path)
{
	std::ifstream f(path);
	if (!f.is_open()) { return; }        // 没有设置文件就用默认值

	std::string line;
	while (std::getline(f, line))
	{
		auto eq = line.find('=');
		if (eq == std::string::npos) { continue; }

		std::string key = line.substr(0, eq);
		std::string val = line.substr(eq + 1);

		auto trim = [](std::string& t)
		{
			while (!t.empty() && (t.back() == '\r' || t.back() == ' ' || t.back() == '\t')) { t.pop_back(); }
			size_t i = 0;
			while (i < t.size() && (t[i] == ' ' || t[i] == '\t')) { i++; }
			t.erase(0, i);
		};
		trim(key);
		trim(val);

		if (key == "zoom") { s.zoom = (float)atof(val.c_str()); }
		else if (key == "volume") { s.volume = (float)atof(val.c_str()); }
		else if (key == "minimap") { s.showMinimap = (val != "0"); }
		else if (key == "fps") { s.showFps = (val != "0"); }
		else if (key == "fullscreen") { s.fullscreen = (val != "0"); }
	}

	// 夹一下，防止手改文件写坏
	if (s.zoom < 24.0f) { s.zoom = 24.0f; }
	if (s.zoom > 96.0f) { s.zoom = 96.0f; }
	if (s.volume < 0.0f) { s.volume = 0.0f; }
	if (s.volume > 1.0f) { s.volume = 1.0f; }
}

bool settingsSave(const GameSettings& s, const char* path)
{
	std::ofstream f(path, std::ios::trunc);
	if (!f.is_open()) { return false; }

	f << "# Dungeon Echo settings\n";
	f << "zoom=" << s.zoom << "\n";
	f << "volume=" << s.volume << "\n";
	f << "minimap=" << (s.showMinimap ? 1 : 0) << "\n";
	f << "fps=" << (s.showFps ? 1 : 0) << "\n";
	f << "fullscreen=" << (s.fullscreen ? 1 : 0) << "\n";

	f.close();
	return true;
}
