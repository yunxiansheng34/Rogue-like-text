这里放游戏资源（图片、字体、音效、关卡数据）。
代码里统一用 RESOURCES_PATH 拼路径：

    std::string p = std::string(RESOURCES_PATH) + "player/idle.png";

- 开发版（PRODUCTION_BUILD=OFF）：RESOURCES_PATH = 源码目录的绝对路径
- 发布版（PRODUCTION_BUILD=ON） ：RESOURCES_PATH = "./resources/"（exe 同级）

构建时发布版会自动把本目录复制到 exe 旁边。