# Auto fish 区域与自动购买鱼饵设计文档

日期：2026-08-08
状态：已批准

## 概述

将 Ranbi 设置菜单中的 "Auto attack" 区域标题改名为 "Auto fish"（区域内容不变），并在区域内新增"自动购买鱼饵"功能：开启后按可配置间隔检查服务器投票选项中是否有"购买鱼饵"选项（模糊匹配），有则触发该投票（等效于在"发起投票"界面双击该选项）。

## 需求

- 区域标题 "Auto attack" → "Auto fish"；区域内原有控件（Auto attack 开关、Attack interval (ms) 滑块 50–1000ms）名称与参数不变
- 新增"自动购买鱼饵"：独立开关 + 间隔设置（10–60 秒，默认 30 秒）
- 触发方式：模糊匹配投票选项中的"购买鱼饵"，命中则模拟双击触发（调用 `CallvoteOption`，即双击链路的最终动作）

## 配置项（`src/engine/shared/config_variables_ranbi.h`）

| 变量 | 脚本名 | 默认 | 范围 | 说明 |
|---|---|---|---|---|
| `RcAutoBuyBait` | `rc_auto_buy_bait` | 0 | 0–1 | 自动购买鱼饵开关 |
| `RcAutoBuyBaitInterval` | `rc_auto_buy_bait_interval` | 30 | 10–60 | 检查间隔（秒） |

## 实现

### 1. 自动购买鱼饵逻辑（`src/game/client/components/ranbi/ranbi_client.cpp` `OnUpdate`）

- 新增成员 `int64_t m_BuyBaitNextCheckTime`（构造函数/`OnReset` 清零）
- `OnUpdate` 中（Auto attack 逻辑之后）：
  1. 开关关闭 → 复位 `m_BuyBaitNextCheckTime = 0`，跳过
  2. 开关开启且本地角色有效（`m_aLocalIds` 范围检查 + `m_Active`）：
     - `m_BuyBaitNextCheckTime == 0` 时初始化为 `Now`（避免开启瞬间立即触发）
     - `Now >= m_BuyBaitNextCheckTime` 时执行检查：遍历 `GameClient()->m_Voting.FirstOption()` 的 `CVoteOptionClient` 链表（带下标计数），用 `str_find_nocase(选项描述, "购买鱼饵")` 模糊匹配；命中则 `GameClient()->m_Voting.CallvoteOption(索引, "", false)` 并停止遍历；无论是否命中，`m_BuyBaitNextCheckTime = Now + time_freq() * interval(秒)`
  3. 选项列表为空（无选项）时自然跳过（遍历不命中），不报错

### 2. 设置菜单（`src/game/client/components/ranbi/menus_ranbi.cpp` `RenderRanbiSettings`）

- "Auto attack" 区块标题改为 `RCLocalize("Auto fish")`
- 区块内 Auto attack 开关与 Attack interval 滑块保持不变
- 新增：`DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAutoBuyBait, RCLocalize("Auto buy bait"), ...)` + `Ui()->DoScrollbarOption(&g_Config.m_RcAutoBuyBaitInterval, ..., RCLocalize("Buy bait interval (s)"), 10, 60)`

## 边界与错误处理

- 选项列表未就绪/为空：静默跳过（与双击前列表为空的行为一致）
- 服务器拒绝重复投票：由服务器处理，客户端不预判（与手动双击行为一致）
- 与 Auto attack 完全独立：独立开关、独立间隔、互不干扰
- 观战/角色无效时仍可触发投票？——投票发起不依赖角色状态（发起投票界面在任何时候可用），但为与 Auto attack 行为一致，仅在角色有效时执行检查
- 开关关闭立即复位定时器，重新开启从当前时刻起算

## 测试

- 编译通过（`-Werror`）
- 手动验证：开启后每间隔向含"购买鱼饵"选项的服务器发起 option 投票；无该选项时不触发；关闭后停止
