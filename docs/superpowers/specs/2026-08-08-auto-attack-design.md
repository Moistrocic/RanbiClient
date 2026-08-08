# 自动攻击（Auto Attack）设计文档

日期：2026-08-08
状态：已批准

## 概述

在 Ranbi 设置菜单中添加"按一定频率攻击"功能：开启后，当前控制角色以可配置的间隔自动进行攻击（周期性注入开火输入）。

## 需求

- 设置菜单（Ranbi → Settings）提供开关与攻击间隔调节
- 攻击间隔可调，范围 50–1000 毫秒
- 作用于当前控制角色（跟随 `g_Config.m_ClDummy`，本体/分身切换后自动跟随）
- 开启期间接管开火输入，关闭后立即恢复手动控制

## 配置项（`src/engine/shared/config_variables_ranbi.h`）

| 变量 | 脚本名 | 默认 | 范围 | 说明 |
|---|---|---|---|---|
| `RcAutoAttack` | `rc_auto_attack` | 0 | 0–1 | 自动攻击开关 |
| `RcAutoAttackInterval` | `rc_auto_attack_interval` | 200 | 50–1000 | 攻击间隔（毫秒） |

## 实现

### 1. 攻击注入（`src/game/client/components/ranbi/ranbi_client.cpp` `OnUpdate`）

- 开关关闭或本地角色无效（`m_aLocalIds[Dummy]` 越界、`m_aClients[LocalId].m_Active` 为假、观战 `m_Snap.m_SpecInfo.m_Active`）时跳过注入
- 时间状态：`m_aAttackNextPressTime`（下次按下时刻）、`m_aAttackPressEndTime`（本次按下结束时刻），按 Dummy 索引
- 逻辑（每帧 `OnUpdate`）：
  1. 若 `time_get() >= m_aAttackPressEndTime`，将 `m_aInputData[Dummy].m_Fire` 置 0（松开）
  2. 若 `time_get() >= m_aAttackNextPressTime`，将 `m_Fire` 置 1，并计划下一次按下：`NextPressTime = now + time_freq() * Interval / 1000`，`PressEndTime = now + time_freq() * 40 / 1000`（按下持续 40ms，确保跨越至少一个 25Hz 输入发送周期）
- 开关从关到开时重置两个时间戳，避免开启瞬间立即连发
- 注入位置安全：`m_RanbiClient`（gameclient.h:245）的 `OnUpdate` 在 `m_Controls`（gameclient.h:190）之后执行，注入值会在下一个输入周期被发送（与 `weapons.cpp` 修改 `m_aInputData[...].m_WantedWeapon` 同一模式）

### 2. 设置菜单（`src/game/client/components/ranbi/menus_ranbi.cpp` `RenderRanbiSettings`）

- 新增 "Auto attack" 区块：`DoButton_CheckBoxAutoVMarginAndSet` 开关 + `Ui()->DoScrollbarOption` 间隔滑块（50–1000ms，步长与现有滑块一致）

## 边界与错误处理

- 本地角色无效/观战：跳过注入（不发送开火）
- 间隔为 0 或非法值：由配置范围（50–1000）保证
- 关闭开关：时间戳复位，`m_Fire` 不再被注入，手动输入不受影响
- 与 `m_ClDummyCopyMoves`（分身复制移动）并存时，自动攻击注入的是当前控制角色的输入，副本逻辑按既有机制处理

## 测试

- 编译通过（`-Werror`）
- 手动验证：开启后角色按设定间隔攻击；关闭后停止；切换本体/分身自动跟随；观战时无注入
