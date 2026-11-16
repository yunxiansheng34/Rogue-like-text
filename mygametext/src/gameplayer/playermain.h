#pragma once

// 游戏主体的生命周期（对应课程 7-游戏架构 的三段式）
bool initGame();     // 初始化：加载资源、创建玩家等
bool updateGame();   // 每帧更新 + 绘制；返回 false 表示要求退出
void closeGame();    // 释放资源
