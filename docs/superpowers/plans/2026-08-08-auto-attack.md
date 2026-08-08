# 自动攻击（Auto Attack）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Ranbi 设置菜单中添加自动攻击功能：按可配置间隔（50–1000ms）周期性向当前控制角色注入开火输入。

**Architecture:** 配置变量驱动（`rc_auto_attack` 开关 + `rc_auto_attack_interval` 间隔），`CRanbiClient::OnUpdate` 中按间隔向 `m_aInputData[m_ClDummy].m_Fire` 写入 0/1 序列（按下 40ms 后松开），UI 在 Ranbi Settings tab 新增区块。组件更新顺序保证注入有效：`m_RanbiClient`（gameclient.h:245）在 `m_Controls`（gameclient.h:190）之后更新。

**Tech Stack:** C++ / DDNet 客户端组件框架 / CMake+Ninja 构建（gcc 14.2，`-Werror`）

## Global Constraints

- 仅修改：`src/engine/shared/config_variables_ranbi.h`、`src/game/client/components/ranbi/ranbi_client.h`、`src/game/client/components/ranbi/ranbi_client.cpp`、`src/game/client/components/ranbi/menus_ranbi.cpp`（不修改其他文件）
- 配置变量前缀 `Rc` / 脚本名前缀 `rc_`，`CFGFLAG_CLIENT | CFGFLAG_SAVE`
- 代码风格：tab 缩进、Allman 大括号、`m_` 成员前缀、无新增注释（除非必要）
- 多平台兼容（不引入平台特定 API）
- 编译命令：`cd build/Debug && PATH="/c/Code/Env/gcc-x86_64-14.2.0-win32-msvc/bin:$PATH" ninja DDNet.exe -j8`
- 链接失败若为 `Permission denied`（DDNet.exe 被测试环境实例占用），仅验证 `.obj` 编译成功即可，不杀进程

---

### Task 1: 添加配置变量

**Files:**
- Modify: `src/engine/shared/config_variables_ranbi.h`（文件末尾，`RcAiSystemPrompt` 行之后）

**Interfaces:**
- Produces: `g_Config.m_RcAutoAttack`（int，0/1）、`g_Config.m_RcAutoAttackInterval`（int，50–1000）

- [ ] **Step 1: 添加配置变量**

在 `src/engine/shared/config_variables_ranbi.h` 末尾（`MACRO_CONFIG_STR(RcAiSystemPrompt, ...)` 之后）添加：

```cpp
MACRO_CONFIG_INT(RcAutoAttack, rc_auto_attack, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAutoAttackInterval, rc_auto_attack_interval, 200, 50, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

- [ ] **Step 2: 编译验证**

运行编译命令。Expected: 编译通过（config 头被大量文件包含，可能触发较多重编译）。

- [ ] **Step 3: 提交**

```bash
git add src/engine/shared/config_variables_ranbi.h
git commit -m "添加自动攻击配置变量"
```

---

### Task 2: 攻击注入逻辑

**Files:**
- Modify: `src/game/client/components/ranbi/ranbi_client.h`（private 成员区，`m_aLastSkinChangeTick` 附近）
- Modify: `src/game/client/components/ranbi/ranbi_client.cpp`（构造函数、`OnReset`、`OnUpdate`）

**Interfaces:**
- Consumes: `g_Config.m_RcAutoAttack`、`g_Config.m_RcAutoAttackInterval`（Task 1）
- Produces: 无对外接口（内部状态 `m_aAttackNextPressTime[NUM_DUMMIES]`、`m_aAttackPressEndTime[NUM_DUMMIES]`）

- [ ] **Step 1: 添加成员变量**

`ranbi_client.h` private 区（`int m_aLastSkinChangeTick[NUM_DUMMIES];` 之后）添加：

```cpp
	int64_t m_aAttackNextPressTime[NUM_DUMMIES];
	int64_t m_aAttackPressEndTime[NUM_DUMMIES];
```

`ranbi_client.cpp` 构造函数（`m_aLastSkinChangeTick` 初始化循环内）添加：

```cpp
		m_aAttackNextPressTime[Dummy] = 0;
		m_aAttackPressEndTime[Dummy] = 0;
```

`OnReset()` 中同样添加这两行（`OnReset` 目前只重置 `m_aLastSkinChangeTick`）。

- [ ] **Step 2: 实现注入逻辑**

`ranbi_client.cpp` `OnUpdate()` 函数开头（`// RANBICLIENT m_RcAutoChangeSkin` 之前）插入：

```cpp
	// RANBICLIENT m_RcAutoAttack
	if(g_Config.m_RcAutoAttack)
	{
		const int Dummy = g_Config.m_ClDummy;
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_Active &&
			!GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int64_t Now = time_get();
			if(m_aAttackNextPressTime[Dummy] == 0)
			{
				m_aAttackNextPressTime[Dummy] = Now;
				m_aAttackPressEndTime[Dummy] = 0;
			}
			if(Now >= m_aAttackPressEndTime)
				GameClient()->m_Controls.m_aInputData[Dummy].m_Fire = 0;
			if(Now >= m_aAttackNextPressTime)
			{
				GameClient()->m_Controls.m_aInputData[Dummy].m_Fire = 1;
				m_aAttackNextPressTime[Dummy] = Now + time_freq() * g_Config.m_RcAutoAttackInterval / 1000;
				m_aAttackPressEndTime[Dummy] = Now + time_freq() * 40 / 1000;
			}
		}
		else
		{
			m_aAttackNextPressTime[Dummy] = 0;
			m_aAttackPressEndTime[Dummy] = 0;
		}
	}
	else
	{
		for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		{
			m_aAttackNextPressTime[Dummy] = 0;
			m_aAttackPressEndTime[Dummy] = 0;
		}
	}
```

要点：
- `m_Fire = 1` 按下持续 40ms 后 `m_Fire = 0` 松开；间隔 `m_RcAutoAttackInterval` ms 一个周期
- 角色无效/观战/开关关闭时复位时间戳，重新开启时从当前时刻开始（不会立即连发）
- `time_get()` 与 `time_freq()` 来自 `base/system.h`（ranbi_client.cpp 已 include）

- [ ] **Step 3: 编译验证**

运行编译命令。Expected: `ai_client` 无关，`ranbi_client.cpp.obj` 编译通过、链接成功（若 Permission denied 则仅确认 `.obj` 编译成功）。

- [ ] **Step 4: 提交**

```bash
git add src/game/client/components/ranbi/ranbi_client.h src/game/client/components/ranbi/ranbi_client.cpp
git commit -m "实现自动攻击注入逻辑"
```

---

### Task 3: 设置菜单 UI

**Files:**
- Modify: `src/game/client/components/ranbi/menus_ranbi.cpp`（`RenderRanbiSettings` 函数末尾，`s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;` 结束最后一个区块后、函数返回前）

**Interfaces:**
- Consumes: `g_Config.m_RcAutoAttack`、`g_Config.m_RcAutoAttackInterval`（Task 1）

- [ ] **Step 1: 添加 UI 区块**

在 `RenderRanbiSettings` 末尾（函数内最后一个区块之后）添加：

```cpp
	// Auto attack
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Auto attack"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAutoAttack, RCLocalize("Auto attack"), &g_Config.m_RcAutoAttack, &Column, s_LineSize);

	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcAutoAttackInterval, &g_Config.m_RcAutoAttackInterval, &Button, RCLocalize("Attack interval (ms)"), 50, 1000);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
```

（参考 `RenderRanbiAI` 中 `m_RcAiAutoReply` 开关 + `m_RcAiReplyInterval` 滑块的写法；`Label`/`Button` 变量在 `RenderRanbiSettings` 开头已声明。）

- [ ] **Step 2: 编译验证**

运行编译命令。Expected: 编译通过、链接成功。

- [ ] **Step 3: 提交**

```bash
git add src/game/client/components/ranbi/menus_ranbi.cpp
git commit -m "添加自动攻击设置菜单"
```

---

### Task 4: 完整验证与收尾

**Files:** 无代码改动

- [ ] **Step 1: 完整编译**

运行编译命令。Expected: 全部目标编译链接成功（若链接 Permission denied，确认所有 `.obj` 均已更新且编译无警告）。

- [ ] **Step 2: 代码审查**

检查最终 diff：仅 4 个文件改动；配置变量命名/范围正确；注入逻辑无越界（`m_aLocalIds` 检查、`MAX_CLIENTS` 检查）；无资源泄漏；时间戳类型 `int64_t` 与 `time_get()` 返回值一致。

- [ ] **Step 3: 确认工作区状态**

`git status --porcelain` 应无未提交改动；`git log --oneline -5` 显示 3 个新提交（配置变量 / 注入逻辑 / 设置菜单）。

---

## Self-Review

**1. Spec 覆盖：**
- 配置项（开关 + 间隔 50–1000ms 默认 200）→ Task 1 ✓
- 攻击注入（当前控制角色、按下 40ms、间隔周期）→ Task 2 ✓
- 设置菜单 UI（Settings tab 新区块）→ Task 3 ✓
- 边界（观战/角色无效跳过、关闭复位、开启不立即连发）→ Task 2 实现内 ✓
- 测试（编译验证 + 手动验证说明）→ Task 2/3/4 ✓（项目无单测框架，编译 + review 为验证手段）

**2. 占位符扫描：** 无 TBD/TODO；所有代码步骤含完整代码。

**3. 类型一致性：** `m_aAttackNextPressTime`/`m_aAttackPressEndTime` 均为 `int64_t`，与 `time_get()` 返回值一致；`m_aInputData[Dummy].m_Fire` 为 `int`，赋值 0/1 正确；配置变量名在 Task 1 定义、Task 2/3 使用，拼写一致。
