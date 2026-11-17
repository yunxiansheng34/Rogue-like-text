
#include "playermain.h"
#include <iostream>
#include <raylib.h>
#include <imgui.h>
#include <stdio.h>
#include <cmath>
#include <asserts.h>
struct GameData {
	float postionX = 100;
	float postionY = 100;
	RenderTexture renderingSystem;
}GameData;

struct GameData& getGameData() { return GameData; }

void renderStuff()
{
	auto& GameData = getGameData();
}


void renderStuff(struct GameData& gameData) {/***/ };

void renderStuff(RenderTexture &renderingSystem) {/***/ };

bool initGame()
{
	bool thingsAreOk = false;
	permaAssertComment(thingsAreOk==true,"error, thing are not ok :/");
	return true;
}

bool updateGame()
{
	float deltaTime = GetFrameTime();
	if (deltaTime > 1.f / 5.f) { deltaTime = 1.f / 5.f; }
	Color c;
	c.r = 255;
	c.g = 0;
	c.b = 200;
	c.a = 255;
	if (IsKeyDown(KEY_W) ){GameData.postionY -= 200*deltaTime;}
	if(IsKeyDown(KEY_A)){GameData.postionX -= 200*deltaTime;}
	if(IsKeyDown(KEY_D)){GameData.postionX += 200.f*deltaTime;}
	if (IsKeyDown(KEY_S)) { GameData.postionY += 200.f*deltaTime; }
	DrawRectangle(GameData.postionX, GameData.postionY, 50, 50, c);
	return true;
}

void closeGame()
{
	
}
void closelevel() {
	GameData = {};
}
void loadlevel() {
	GameData = {};
}