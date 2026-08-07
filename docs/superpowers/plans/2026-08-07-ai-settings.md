# AI Settings（AI 设置）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RanbiClient 添加 AI 接口功能：新增"AI设置"菜单 tab（基本设置：Base url/Model/Token 文本框；其他：自动 AI 回复开关），实现他人 @ 自己时自动调用 OpenAI 兼容接口回复（区分本体/分身、5 秒节流、回复限 80 汉字/128 字母）。

**Architecture:** 新建独立组件 `CAiClient`（仿现有 Ranbi 组件模式）：`OnMessage` 接收聊天消息 → 严格三段式解析（区分大小写）→ 节流/单槽检查 → `HttpPostJson` 请求 `<base_url>/chat/completions` → `OnUpdate` 主线程轮询结果 → 加权截断 → `SendPackMsg(0/1)` 指定连接回复。菜单新增 `RANBI_TAB_AI` tab。

**Tech Stack:** C++ / CMake / Ninja / MinGW gcc 14.2.0（`-Werror`）；HTTP 用项目自带 `CHttpRequest`（src/engine/shared/http.h）+ `json-parser`（src/engine/shared/json.h）

**Spec:** `docs/superpowers/specs/2026-08-07-ai-settings-design.md`（已批准）

## Global Constraints

- 不改 Server 相关文件、不改核心文件（`controls.cpp`、`menus.cpp`、`gameclient.cpp` 除注册行外）；只改：`config_variables_ranbi.h`、新建 `ai_client.cpp/h`、`gameclient.h`（include+成员）、`gameclient.cpp`（仅 m_vpAll 注册一行）、`CMakeLists.txt`（仅文件列表）、`menus_ranbi.cpp`、`simplified_chinese.txt`
- 代码不加注释，唯一允许的注释是 `// RANBICLIENT m_RcAiAutoReply` 形式的绑定变量标记
- 多平台兼容；时间戳用 `int64_t`（`time_get()`/`time_freq()`）
- 名字比较**区分大小写**（`str_comp`/`str_comp_n`，不用 nocase 版本）
- 所有 shell 命令在 worktree 目录执行（`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings`，执行前按 using-git-worktrees 流程创建）；若 `export PATH` 前缀被环境拦截，直接裸 `cmake --build build/Debug -j 8`（构建文件里编译器是绝对路径，等效）
- 编译：`cmake --build build/Debug -j 8`（8 核，-Werror 零警告）
- 提交：`git commit --no-gpg-sign`（本机 gpg 签名不可用）
- 编辑/提交一律用绝对路径指向 worktree，避免误改主仓库同名文件
- 本 fork API 已知差异（勿重蹈 auto_fire 覆辙）：`IConsole::ExecuteLine` 需 ClientId 参数（本计划不用它）；`CLineInput` 无 `SetFloat`（本计划用 `CLineInput(pStr, Size)` 直接绑定 config 缓冲，不涉及 SetFloat）

---

### Task 1: 配置变量

**Files:**
- Modify: `src/engine/shared/config_variables_ranbi.h`（文件末尾追加）

**Interfaces:**
- Produces: `g_Config.m_RcAiBaseUrl`（char[256]）、`g_Config.m_RcAiModel`（char[128]）、`g_Config.m_RcAiToken`（char[256]）、`g_Config.m_RcAiAutoReply`（int 0/1）

- [ ] **Step 1: 在文件末尾追加配置变量**

在 `src/engine/shared/config_variables_ranbi.h` 最后一行之后追加：

```cpp

MACRO_CONFIG_STR(RcAiBaseUrl, rc_ai_base_url, 256, "https://api.openai.com/v1", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcAiModel, rc_ai_model, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(RcAiToken, rc_ai_token, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiAutoReply, rc_ai_auto_reply, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

（与现有 MACRO_CONFIG_STR 条目格式一致，参照 `RcShowEnableSkipThreeTilesInfoPositionLeftText` 条目）

- [ ] **Step 2: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告（-Werror）

- [ ] **Step 3: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings
git add src/engine/shared/config_variables_ranbi.h
git commit --no-gpg-sign -m "Add AI settings config variables"
```

---

### Task 2: CAiClient 组件与注册

**Files:**
- Create: `src/game/client/components/ranbi/ai_client.h`
- Create: `src/game/client/components/ranbi/ai_client.cpp`
- Modify: `src/game/client/gameclient.h`（include 区 + 成员区）
- Modify: `src/game/client/gameclient.cpp`（仅 m_vpAll 注册一行）
- Modify: `CMakeLists.txt`（GAME_CLIENT_SRC 列表，components/ranbi/ 段约 2600 行）

**Interfaces:**
- Consumes: `g_Config.m_RcAiBaseUrl` / `m_RcAiModel` / `m_RcAiToken` / `m_RcAiAutoReply`（Task 1）
- Produces: 类 `CAiClient : public CComponent`（`OnMessage`/`OnUpdate`/`OnReset`）；`GameClient()->m_AiClient`

- [ ] **Step 1: 创建 `ai_client.h`**

```cpp
#ifndef GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H

#include <cstdint>
#include <memory>

#include <engine/client/enums.h>

#include <game/client/component.h>

class CHttpRequest;

class CAiClient : public CComponent
{
	int64_t m_aNextReplyTime[NUM_DUMMIES];
	std::shared_ptr<CHttpRequest> m_pRequest;
	int m_ReplyDummy;
	int m_ReplyTeam;

	bool ParseMention(const char *pText, int &Dummy, const char **ppText);
	static void TruncateReply(char *pText, int Size);
	void SendRequest(const char *pText, int Dummy, int Team);
	void HandleResponse();

public:
	CAiClient();

	int Sizeof() const override { return sizeof(*this); }

	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnUpdate() override;
	void OnReset() override;
};

#endif
```

- [ ] **Step 2: 创建 `ai_client.cpp`**

```cpp
#include "ai_client.h"

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h>

CAiClient::CAiClient()
{
	OnReset();
}

void CAiClient::OnReset()
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		m_aNextReplyTime[Dummy] = 0;
	m_pRequest = nullptr;
	m_ReplyDummy = 0;
	m_ReplyTeam = 0;
}

// 严格三段式解析：名字1: 名字2: 文本；名字2 == 本体名或分身名（区分大小写）
bool CAiClient::ParseMention(const char *pText, int &Dummy, const char **ppText)
{
	const char *pColon1 = str_find(pText, ": ");
	if(!pColon1)
		return false;
	const char *pColon2 = str_find(pColon1 + 2, ": ");
	if(!pColon2)
		return false;

	for(int D = 0; D < NUM_DUMMIES; D++)
	{
		const int LocalId = GameClient()->m_aLocalIds[D];
		if(LocalId < 0 || LocalId >= MAX_CLIENTS)
			continue;
		const char *pName = GameClient()->m_aClients[LocalId].m_aName;
		const int NameLen = pColon2 - (pColon1 + 2);
		if((int)str_length(pName) == NameLen && str_comp_n(pColon1 + 2, pName, NameLen) == 0)
		{
			Dummy = D;
			*ppText = pColon2 + 2;
			return true;
		}
	}
	return false;
}

// 加权截断：汉字(U+4E00-U+9FFF)权重16，其他10，总权重上限1280（=128*10）
void CAiClient::TruncateReply(char *pText, int Size)
{
	int Total = 0;
	const char *p = pText;
	while(*p)
	{
		const char *pNext = p;
		const int Ch = str_utf8_decode(&pNext);
		if(Ch == -1)
			break;
		Total += (Ch >= 0x4E00 && Ch <= 0x9FFF) ? 16 : 10;
		if(Total > 1280)
			break;
		p = pNext;
	}
	if(p - pText >= Size)
		p = pText + Size - 1;
	((char *)p)[0] = '\0';
}

void CAiClient::SendRequest(const char *pText, int Dummy, int Team)
{
	// RANBICLIENT m_RcAiBaseUrl / m_RcAiModel / m_RcAiToken
	char aModel[512];
	char aText[1024];
	EscapeJson(aModel, sizeof(aModel), g_Config.m_RcAiModel);
	EscapeJson(aText, sizeof(aText), pText);
	const char *pSystem = "You are an in-game chat assistant. Reply as briefly as possible. Keep every reply within 80 Chinese characters or 128 English letters (including punctuation); longer replies get truncated.";
	char aBody[4096];
	str_format(aBody, sizeof(aBody),
		"{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}]}",
		aModel, pSystem, aText);

	const int UrlLen = str_length(g_Config.m_RcAiBaseUrl);
	char aUrl[512];
	if(UrlLen > 0 && g_Config.m_RcAiBaseUrl[UrlLen - 1] == '/')
		str_format(aUrl, sizeof(aUrl), "%schat/completions", g_Config.m_RcAiBaseUrl);
	else
		str_format(aUrl, sizeof(aUrl), "%s/chat/completions", g_Config.m_RcAiBaseUrl);

	char aToken[512];
	str_format(aToken, sizeof(aToken), "Bearer %s", g_Config.m_RcAiToken);

	m_pRequest = HttpPostJson(aUrl, aBody);
	m_pRequest->HeaderString("Authorization", aToken);
	m_pRequest->Timeout(CTimeout{10000, 60000, 500, 5});
	Http()->Run(m_pRequest);
	m_ReplyDummy = Dummy;
	m_ReplyTeam = Team;
	m_aNextReplyTime[Dummy] = time_get() + time_freq() * 5;
}

void CAiClient::HandleResponse()
{
	if(m_pRequest->State() != EHttpState::DONE)
	{
		m_pRequest = nullptr;
		return;
	}
	if(m_pRequest->StatusCode() < 200 || m_pRequest->StatusCode() >= 300)
	{
		m_pRequest = nullptr;
		return;
	}
	json_value *pObj = m_pRequest->ResultJson();
	if(!pObj)
	{
		m_pRequest = nullptr;
		return;
	}
	const json_value *pChoices = json_object_get(pObj, "choices");
	const json_value *pFirst = json_array_length(pChoices) > 0 ? json_array_get(pChoices, 0) : nullptr;
	const json_value *pMessage = pFirst ? json_object_get(pFirst, "message") : nullptr;
	const json_value *pContent = pMessage ? json_object_get(pMessage, "content") : nullptr;
	const char *pReply = pContent && pContent->type == json_string ? json_string_get(pContent) : nullptr;
	if(pReply)
	{
		char aReply[512];
		str_copy(aReply, pReply, sizeof(aReply));
		TruncateReply(aReply, sizeof(aReply));
		CNetMsg_Cl_Say Msg;
		Msg.m_Team = m_ReplyTeam;
		Msg.m_pMessage = aReply;
		Client()->SendPackMsg(m_ReplyDummy, &Msg, MSGFLAG_VITAL);
	}
	json_value_free(pObj);
	m_pRequest = nullptr;
}

void CAiClient::OnMessage(int MsgType, void *pRawMsg)
{
	// RANBICLIENT m_RcAiAutoReply
	if(MsgType != NETMSGTYPE_SV_CHAT || !g_Config.m_RcAiAutoReply)
		return;
	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(pMsg->m_ClientId < 0)
		return;
	if(pMsg->m_ClientId == GameClient()->m_aLocalIds[0] || pMsg->m_ClientId == GameClient()->m_aLocalIds[1])
		return; // 自己的消息不触发

	int Dummy;
	const char *pText;
	if(!ParseMention(pMsg->m_pMessage, Dummy, &pText))
		return;
	if(time_get() < m_aNextReplyTime[Dummy])
		return;
	if(m_pRequest)
		return;

	SendRequest(pText, Dummy, pMsg->m_Team < 0 ? 0 : pMsg->m_Team);
}

void CAiClient::OnUpdate()
{
	if(!m_pRequest)
		return;
	if(!m_pRequest->Done())
		return;
	HandleResponse();
}
```

- [ ] **Step 3: 修改 `gameclient.h`**

在 include 区（`#include "components/ranbi/points.h"` 之前，保持字母序）插入：

```cpp
#include "components/ranbi/ai_client.h"
```

在成员区（`CRanbiClient m_RanbiClient;` 之前，保持字母序）插入：

```cpp
	CAiClient m_AiClient;
```

- [ ] **Step 4: 修改 `gameclient.cpp` 注册组件**

在 `m_vpAll` 列表中 `&m_RanbiClient, // RanbiClient` 之后插入一行：

```cpp
					      &m_AiClient, // RanbiClient
```

- [ ] **Step 5: 修改 `CMakeLists.txt` 文件列表**

在 `components/ranbi/menus_ranbi.cpp` 之前（约 2600 行，保持字母序）插入两行：

```
    components/ranbi/ai_client.cpp
    components/ranbi/ai_client.h
```

- [ ] **Step 6: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告。若报 `MAX_CLIENTS` 未定义，确认 `ai_client.h` include 了 `<engine/client/enums.h>`（NUM_DUMMIES 来源）；若报 `json_array_length`/`json_object_get` 等未声明，确认 `ai_client.cpp` include 了 `<engine/shared/json.h>`；若报 `CTimeout` 未定义，确认 include 了 `<engine/shared/http.h>`

- [ ] **Step 7: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings
git add src/game/client/components/ranbi/ai_client.h src/game/client/components/ranbi/ai_client.cpp src/game/client/gameclient.h src/game/client/gameclient.cpp CMakeLists.txt
git commit --no-gpg-sign -m "Add AI client component"
```

---

### Task 3: 菜单 tab 与翻译

**Files:**
- Modify: `src/game/client/components/ranbi/menus_ranbi.cpp`
- Modify: `data/ranbi/languages/simplified_chinese.txt`

**Interfaces:**
- Consumes: `g_Config.m_RcAiBaseUrl` / `m_RcAiModel` / `m_RcAiToken` / `m_RcAiAutoReply`（Task 1）；`CAiClient`（Task 2，已注册）
- Produces: 菜单新增 "AI Settings" tab（RANBI_TAB_AI = 4），含 Basic Settings 分区（3 个文本框）与 Other 分区（开关）

- [ ] **Step 1: 修改 tab 常量与分发**

`menus_ranbi.cpp` 第 15-19 行的 tab 常量区改为：

```cpp
static constexpr int RANBI_TAB_SETTINGS = 0;
static constexpr int RANBI_TAB_WEAPONS_SETTINGS = 1;
static constexpr int RANBI_TAB_DDNET_MORE = 2;
static constexpr int RANBI_TAB_INFO = 3;
static constexpr int RANBI_TAB_AI = 4;
static constexpr int NUMBER_OF_RANBI_TABS = 5;
```

`RenderRanbi` 函数中 tab 名数组（约 670 行）改为：

```cpp
	const char *apTabNames[NUMBER_OF_RANBI_TABS] = {
		RCLocalize("Settings"),
		RCLocalize("Weapons Settings"),
		RCLocalize("More DDNet"),
		RCLocalize("Info"),
		RCLocalize("AI Settings")};
```

分发处（`else if(s_CurTab == RANBI_TAB_INFO)` 分支之后）追加：

```cpp
	else if(s_CurTab == RANBI_TAB_AI)
		RenderRanbiAI(MainView);
```

- [ ] **Step 2: 新增 `RenderRanbiAI` 函数**

在 `RenderRanbiInfo` 函数定义之后、`RenderRanbi` 之前插入（骨架参照 `RenderRanbiSettings` 的滚动区域模式，menus_ranbi.cpp:35-70）：

```cpp
void CMenus::RenderRanbiAI(CUIRect MainView)
{
	CUIRect Column, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;

	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = s_MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 10.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column = MainView;
	Column.VSplitLeft(s_MarginSmall, nullptr, &Column);
	Column.VSplitRight(s_MarginSmall, &Column, nullptr);

	// Basic Settings
	Column.HSplitTop(s_Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Basic Settings"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	CLineInput s_BaseUrlInput(g_Config.m_RcAiBaseUrl, sizeof(g_Config.m_RcAiBaseUrl));
	CLineInput s_ModelInput(g_Config.m_RcAiModel, sizeof(g_Config.m_RcAiModel));
	CLineInput s_TokenInput(g_Config.m_RcAiToken, sizeof(g_Config.m_RcAiToken));

	const char *apLabels[3] = {RCLocalize("Base url"), RCLocalize("Model"), RCLocalize("Token")};
	CLineInput *apInputs[3] = {&s_BaseUrlInput, &s_ModelInput, &s_TokenInput};
	for(int i = 0; i < 3; i++)
	{
		Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
		Button.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, apLabels[i], s_FontSize, TEXTALIGN_ML);
		Ui()->DoEditBox(apInputs[i], &Button, s_EditBoxFontSize);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// Other
	Column.HSplitTop(s_MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(s_HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, RCLocalize("Other"), s_HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(s_MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAiAutoReply, RCLocalize("Auto AI reply"), &g_Config.m_RcAiAutoReply, &Column, s_LineSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = Column.y + s_MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}
```

- [ ] **Step 3: 翻译文件追加条目**

在 `data/ranbi/languages/simplified_chinese.txt` 末尾追加（条目之间空一行）：

```
AI Settings
== AI设置

Basic Settings
== 基本设置

Base url
== Base url

Model
== 模型

Token
== Token

Other
== 其他

Auto AI reply
== 自动 AI 回复
```

- [ ] **Step 4: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告（`CLineInput` 构造、`DoEditBox`、`RCLocalize` 均已有 include 链）

- [ ] **Step 5: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings
git add src/game/client/components/ranbi/menus_ranbi.cpp data/ranbi/languages/simplified_chinese.txt
git commit --no-gpg-sign -m "Add AI settings menu tab and translations"
```

---

### Task 4: 格式化与最终验证

**Files:**
- All files from Tasks 1-3

- [ ] **Step 1: 运行项目格式化脚本**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings && python scripts/fix_style.py`
预期：脚本输出格式化后的文件列表；若它格式化到本功能之外的文件，用 `git checkout -- <文件>` 还原那些文件，只保留本功能文件（config_variables_ranbi.h、ai_client.h/cpp、gameclient.h/cpp、CMakeLists.txt、menus_ranbi.cpp、simplified_chinese.txt）的改动

- [ ] **Step 2: 最终全量编译**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 3: 检查改动范围**

运行：`git status` 与 `git diff --stat`
预期：仅包含本计划的 8 个文件，无其他文件被改动

- [ ] **Step 4: 提交格式化改动（如有）**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings
git add -u
git commit --no-gpg-sign -m "Format AI settings changes"
```

- [ ] **Step 5: 手动实测清单（由用户执行）**

1. AI Settings tab 显示正常，三个文本框可编辑并保存（重启客户端后仍在）
2. 配置真实 OpenAI 兼容接口（base url/model/token）后，他人 @ 自己本体名 → 自动回复发出且频道正确
3. 他人 @ 自己分身名 → 分身说话（分身连接开启时）
4. 5 秒内重复被 @ → 不重复请求/发送
5. 空文本 @（`X: 我的名字: `）→ 仍触发
6. 大小写不符的名字（`x: 我的名字: hi` 名字1 大小写不同或 名字2 大小写不同）→ 不触发
7. AI 回复超长时被截断（≤ 128 权重：80 汉字或 128 字母）
8. 接口不可用/超时 → 静默无回复，客户端不崩溃；之后新 @ 可正常触发
