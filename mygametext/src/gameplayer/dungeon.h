#pragma once

#include "gameMap.h"
#include <raylib.h>

// 生成一层地牢：房间 + L 形走廊，墙 = stoneBricks，地面 = air
// 出口（活板门）放在离出生点最远的房间中心，标记为 Block::door
// 宝箱标记为 Block::woodenChest，散布在随机房间里
// bossFloor = true 时只生成一个大竞技场，不放假活板门（清怪后自动进下一章）
void generateDungeon(GameMap& map, Vector2& spawnPos, bool bossFloor = false);
