#include "assetManager.h"

// RESOURCES_PATH 由 CMake 注入（Debug = 绝对路径，Release = ./resources/）。
// 这里加个兜底，只是为了让 IntelliSense 不报「未定义标识符」；真正编译时不会走到。
#ifndef RESOURCES_PATH
#define RESOURCES_PATH "./resources/"
#endif

void AssetManager::loadAll()
{
	dirt = LoadTexture(RESOURCES_PATH "dirt.png");
	textures = LoadTexture(RESOURCES_PATH "textures.png");
}
