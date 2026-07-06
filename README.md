# RanbiClient

DDNet 游戏客户端的修改版，基于 TClient 分支开发。

## 项目由来

RanbiClient fork 自 TClient，TClient 是 DDNet 的一个修改版客户端。本项目在 TClient 的基础上进行二次开发和定制。

上游项目链接:

- DDNet: https://github.com/ddnet/ddnet
- TClient: https://github.com/TaterClient/TClient

## 免责声明

本项目对原版客户端进行了修改，使用时请注意:

- 部分修改可能不符合 DDNet 官方的游戏规则
- 在某些服务器上使用修改版客户端可能导致封禁
- 请自行承担使用风险
- 本项目不保证无 bug，欢迎反馈问题

## 功能特性

RanbiClient 在 TClient 基础上进行的定制修改，勾选表示已完成:

**原版修复**：

- [x] 修复原版特定情况下切换分身时非预期出锤

**分身修改**：

- [x] 切换分身时保留准星位置

**渲染修改**：

- [x] 表情滞后渲染（不被背景覆盖）
- [x] 辅助线滞后渲染（覆盖在透明方块或水上方）

**GUI 修改**：

- [x] 功能菜单

**表情修改**：

- [x] 静默聊天（Tee 右上角不显示聊天图标）

**计分板修改**：

- [x] 计分板使用爱心标记朋友
- [x] 游戏内分数查询，显示在计分板
- [ ] 计分板快速更改为对应玩家的名称

**Nameplates 修改**：

- [x] Nameplates 显示玩家的分数
- [x] Nameplates 显示x坐标
- [x] Nameplates 显示是否完成该地图
- [x] Nameplates 显示是否开启Dummy Copy
- [x] Nameplates 显示是否开启Hammer Fly
- [x] Nameplates 显示是否开启卡键（Dummy Reset On Switch）
- [ ] 指定半径以及角度范围内的玩家隐藏其 Nameplates

**聊天框修改**：

- [x] 聊天框上下键跳过相同的文本
- [x] 聊天框显示玩家的分数

**Ranbi 其他功能**：

- [ ] 标记武器的角度
- [x] 显示当前坐标是否满足跳过三格水
- [x] 碰撞更换 SKIN
- [ ] 切换最近使用的武器
- [x] 右下角显示所旁观玩家的皮肤名称
- [ ] 获取不在视野内的玩家信息
