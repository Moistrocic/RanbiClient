# 自定义系统提示词 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** AI 设置菜单新增自定义系统提示词选项（rc_ai_system_prompt），留空回退内置默认。

**Architecture:** 配置变量 + `ai_client.cpp` SendRequest 的 aSystem 拼装改为"配置非空用配置、为空用默认" + Basic Settings 区域文本框（数组扩为 4 项）。

**Tech Stack:** C++ / CMake / Ninja / MinGW gcc 14.2.0（-Werror）

**Spec:** `docs/superpowers/specs/2026-08-08-ai-system-prompt-design.md`（已批准）

## Global Constraints

- 只改：`config_variables_ranbi.h`、`ai_client.cpp`、`menus_ranbi.cpp`、`simplified_chinese.txt`
- 代码不加注释（仅允许 `// RANBICLIENT m_RcAiSystemPrompt` 标记）
- 提交信息用**中文**；提交用 `git commit --no-gpg-sign`
- 编译：`cmake --build build/Debug -j 8`（-Werror 零警告）；**构建前确认游戏未运行**（DDNet.exe 运行中链接失败）
- 所有命令在 worktree 执行（`cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt`）
- 编辑/提交用绝对路径指向 worktree
- 格式化：`python scripts/fix_style.py`（clang-format 在 `C:\Users\fu\AppData\Roaming\Python\Python313\Scripts`），跑完还原 3 个预期外文件（nameplates.cpp/players.cpp/moving_tiles.h）

---

### Task 1: 配置变量与组件拼装

**Files:**
- Modify: `src/engine/shared/config_variables_ranbi.h`（末尾追加）
- Modify: `src/game/client/components/ranbi/ai_client.cpp`（SendRequest 的 aSystem 拼装）

- [ ] **Step 1: 追加配置变量**

在 `config_variables_ranbi.h` 最后一行之后追加：

```cpp

MACRO_CONFIG_STR(RcAiSystemPrompt, rc_ai_system_prompt, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

- [ ] **Step 2: 修改 SendRequest 的 aSystem 拼装**

在 `ai_client.cpp` 的 `SendRequest` 中，把：

```cpp
	char aSystem[9000];
	str_copy(aSystem, "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断", sizeof(aSystem));
```

替换为：

```cpp
	// RANBICLIENT m_RcAiSystemPrompt
	char aSystem[9000];
	const char *pPrompt = g_Config.m_RcAiSystemPrompt[0] != '\0' ? g_Config.m_RcAiSystemPrompt : "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断";
	str_copy(aSystem, pPrompt, sizeof(aSystem));
```

- [ ] **Step 3: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 4: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt
git add src/engine/shared/config_variables_ranbi.h src/game/client/components/ranbi/ai_client.cpp
git commit --no-gpg-sign -m "添加自定义系统提示词配置与回退逻辑"
```

---

### Task 2: 菜单文本框与翻译

**Files:**
- Modify: `src/game/client/components/ranbi/menus_ranbi.cpp`（RenderRanbiAI 的 Basic Settings 区域）
- Modify: `data/ranbi/languages/simplified_chinese.txt`（末尾追加）

- [ ] **Step 1: Basic Settings 数组扩为 4 项**

在 `menus_ranbi.cpp` 的 `RenderRanbiAI` 中，把：

```cpp
	static CLineInput s_BaseUrlInput(g_Config.m_RcAiBaseUrl, sizeof(g_Config.m_RcAiBaseUrl));
	static CLineInput s_ModelInput(g_Config.m_RcAiModel, sizeof(g_Config.m_RcAiModel));
	static CLineInput s_TokenInput(g_Config.m_RcAiToken, sizeof(g_Config.m_RcAiToken));

	const char *apLabels[3] = {RCLocalize("Base url"), RCLocalize("Model"), RCLocalize("Token")};
	CLineInput *apInputs[3] = {&s_BaseUrlInput, &s_ModelInput, &s_TokenInput};
	for(int i = 0; i < 3; i++)
```

替换为：

```cpp
	static CLineInput s_BaseUrlInput(g_Config.m_RcAiBaseUrl, sizeof(g_Config.m_RcAiBaseUrl));
	static CLineInput s_ModelInput(g_Config.m_RcAiModel, sizeof(g_Config.m_RcAiModel));
	static CLineInput s_TokenInput(g_Config.m_RcAiToken, sizeof(g_Config.m_RcAiToken));
	static CLineInput s_SystemPromptInput(g_Config.m_RcAiSystemPrompt, sizeof(g_Config.m_RcAiSystemPrompt));

	const char *apLabels[4] = {RCLocalize("Base url"), RCLocalize("Model"), RCLocalize("Token"), RCLocalize("System prompt")};
	CLineInput *apInputs[4] = {&s_BaseUrlInput, &s_ModelInput, &s_TokenInput, &s_SystemPromptInput};
	for(int i = 0; i < 4; i++)
```

（`static CLineInput` 绑定 config 缓冲，与现有 3 个输入框同模式；其余循环体不变）

- [ ] **Step 2: 翻译文件追加条目**

在 `data/ranbi/languages/simplified_chinese.txt` 末尾追加：

```
System prompt
== 系统提示词
```

- [ ] **Step 3: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 4: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt
git add src/game/client/components/ranbi/menus_ranbi.cpp data/ranbi/languages/simplified_chinese.txt
git commit --no-gpg-sign -m "添加自定义系统提示词菜单文本框与翻译"
```

---

### Task 3: 格式化与最终验证

- [ ] **Step 1: 格式化**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt && export PATH="/c/Users/fu/AppData/Roaming/Python/Python313/Scripts:$PATH" && python scripts/fix_style.py`
跑完**必须还原 3 个预期外文件**：`git checkout -- src/game/client/components/nameplates.cpp src/game/client/components/players.cpp src/game/client/components/tclient/moving_tiles.h`（若被改动）

- [ ] **Step 2: 全量编译**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 3: 检查改动范围**

运行：`git status` 与 `git diff --stat`
预期：仅包含本计划的 4 个文件，无其他文件被改动

- [ ] **Step 4: 提交格式化改动（如有）**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/system-prompt
git add -u
git commit --no-gpg-sign -m "格式化自定义系统提示词改动"
```

- [ ] **Step 5: 手动实测清单（由用户执行）**

1. System prompt 文本框可编辑保存（重启保留）
2. 留空 → 回复行为与之前一致（默认提示词）
3. 填写自定义（如"你是毒舌玩家"）→ 回复风格变化
4. 自定义提示词为空字符串（清空）→ 回退默认
