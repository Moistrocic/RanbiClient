# AI 设置扩展 v2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展 AI 设置功能：词边界 @ 检测、回复前缀、频率/温度/上下文条数配置、上下文 FIFO 队列、Storage 知识库、精简 debug 输出。

**Architecture:** 单组件扩展 `CAiClient`（src/game/client/components/ranbi/ai_client.cpp/h）：OnMessage 中词边界任意位置检测名字 → 节流/单槽检查 → 知识库筛选（Storage ListDirectory + ReadFileStr，TYPE_ALL 用户目录优先）→ 组装请求（system+上下文+当前消息，temperature 配置）→ 主线程轮询响应 → 前缀+截断 → SendPackMsg；上下文 FIFO 存于组件成员。

**Tech Stack:** C++ / CMake / Ninja / MinGW gcc 14.2.0（-Werror）；HTTP 用项目自带 CHttpRequest；JSON 用 json-parser；Storage 用 IStorage（ListDirectory/ReadFileStr）。

**Spec:** `docs/superpowers/specs/2026-08-08-ai-settings-v2-design.md`（已批准）

## Global Constraints

- 不改 Server 相关文件、不改核心文件（`controls.cpp`、`menus.cpp`、`gameclient.cpp` 除注册行外）；只改：`config_variables_ranbi.h`、`ai_client.h`、`ai_client.cpp`、`menus_ranbi.cpp`、`simplified_chinese.txt`
- 代码不加注释，唯一允许的注释是 `// RANBICLIENT m_RcAiAutoReply` 形式的绑定变量标记；计划代码中的中文注释照抄
- 多平台兼容（Windows/Linux）；时间戳 int64_t（time_get/time_freq）
- 名字比较**区分大小写**；词边界 = 前后字符非字母/数字（ASCII）
- 提交信息用**中文**；提交用 `git commit --no-gpg-sign`（本机 gpg 不可用）
- 编译：`cmake --build build/Debug -j 8`（8 核，-Werror 零警告；若 export PATH 前缀被拦截直接裸命令，构建文件里编译器为绝对路径）
- 所有 shell 命令在 worktree 目录执行（`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2`）
- 编辑/提交一律用绝对路径指向 worktree，避免误改主仓库同名文件
- **构建前确认游戏未运行**（DDNet.exe 运行中链接会失败：`collect2.exe: error: ld returned 1 exit status`）
- 格式化：`python scripts/fix_style.py`（clang-format 在 `C:\Users\fu\AppData\Roaming\Python\Python313\Scripts`，先 export PATH）；脚本会误格式化 nameplates.cpp/players.cpp/moving_tiles.h，跑完用 `git checkout --` 还原这 3 个文件

---

### Task 1: 配置变量

**Files:**
- Modify: `src/engine/shared/config_variables_ranbi.h`（文件末尾追加）

**Interfaces:**
- Produces: `g_Config.m_RcAiReplyInterval`（int 1~60，默认 2）、`g_Config.m_RcAiTemperature`（int 0~20，默认 10）、`g_Config.m_RcAiContextCount`（int 0~20，默认 5）

- [ ] **Step 1: 在文件末尾追加配置变量**

在 `src/engine/shared/config_variables_ranbi.h` 最后一行（`MACRO_CONFIG_INT(RcAiAutoReply, ...)`）之后追加：

```cpp

MACRO_CONFIG_INT(RcAiReplyInterval, rc_ai_reply_interval, 2, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiTemperature, rc_ai_temperature, 10, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(RcAiContextCount, rc_ai_context_count, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
```

- [ ] **Step 2: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2 && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 3: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2
git add src/engine/shared/config_variables_ranbi.h
git commit --no-gpg-sign -m "添加AI设置扩展配置变量"
```

---

### Task 2: CAiClient 组件扩展

**Files:**
- Modify: `src/game/client/components/ranbi/ai_client.h`（整体替换）
- Modify: `src/game/client/components/ranbi/ai_client.cpp`（整体替换）

**Interfaces:**
- Consumes: `g_Config.m_RcAiReplyInterval` / `m_RcAiTemperature` / `m_RcAiContextCount`（Task 1）；已有 `g_Config.m_RcAiBaseUrl` / `m_RcAiModel` / `m_RcAiToken` / `m_RcAiAutoReply`
- Produces: 类 `CAiClient : public CComponent`（`OnMessage`/`OnUpdate`/`OnReset`），成员 `m_aNextReplyTime`/`m_pRequest`/`m_ReplyDummy`/`m_ReplyTeam`/`m_Context`/`m_KnowledgeHit`/`m_aPendingSpeaker`/`m_aPendingText`

- [ ] **Step 1: 整体替换 `ai_client.h`**

```cpp
#ifndef GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <engine/client/enums.h>

#include <game/client/component.h>

class CHttpRequest;

class CAiClient : public CComponent
{
	struct CContextEntry
	{
		char m_aSpeaker[16];
		char m_aText[256];
		bool m_IsReply;
	};

	int64_t m_aNextReplyTime[NUM_DUMMIES];
	std::shared_ptr<CHttpRequest> m_pRequest;
	int m_ReplyDummy;
	int m_ReplyTeam;
	std::vector<CContextEntry> m_Context;
	bool m_KnowledgeHit;
	char m_aPendingSpeaker[16];
	char m_aPendingText[256];

	bool ParseMention(const char *pText, int &Dummy);
	static void TruncateReply(char *pText, int Size);
	int LoadKnowledge(const char *pQuestion, char *aKbBuf, int KbBufSize);
	void SendRequest(const char *pText, const char *pSpeaker, int Dummy, int Team);
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

- [ ] **Step 2: 整体替换 `ai_client.cpp`**

```cpp
#include "ai_client.h"

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

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
	m_KnowledgeHit = false;
	m_aPendingSpeaker[0] = '\0';
	m_aPendingText[0] = '\0';
	m_Context.clear();
}

static bool IsWordChar(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// 任意位置出现名字（词边界，区分大小写）即触发
bool CAiClient::ParseMention(const char *pText, int &Dummy)
{
	for(int D = 0; D < NUM_DUMMIES; D++)
	{
		if(D == 1 && !Client()->DummyConnected())
			continue;
		const int LocalId = GameClient()->m_aLocalIds[D];
		if(LocalId < 0 || LocalId >= MAX_CLIENTS)
			continue;
		const char *pName = GameClient()->m_aClients[LocalId].m_aName;
		const int NameLen = str_length(pName);
		if(NameLen == 0)
			continue;

		const char *pFound = pText;
		while((pFound = str_find(pFound, pName)) != nullptr)
		{
			const bool LeftOk = pFound == pText || !IsWordChar(pFound[-1]);
			const bool RightOk = !IsWordChar(pFound[NameLen]);
			if(LeftOk && RightOk)
			{
				Dummy = D;
				return true;
			}
			pFound += NameLen;
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

// 知识库文件列表回调：收集 .txt 文件名（去重由 set 完成）
static int KnowledgeListCallback(const char *pName, int IsDir, int Type, void *pUser)
{
	const int Len = str_length(pName);
	if(IsDir || Len < 4 || str_comp(pName + Len - 4, ".txt") != 0)
		return 0;
	auto *pEntries = static_cast<std::set<std::string> *>(pUser);
	pEntries->emplace(pName);
	return 0;
}

// 命中规则：提问包含文件名（去 .txt，不区分大小写），或提问任一连续4字符片段出现在内容中
static bool KnowledgeHit(const char *pQuestion, const char *pFileName, const char *pContent)
{
	char aName[128];
	str_copy(aName, pFileName, sizeof(aName));
	const int NameLen = str_length(aName);
	if(NameLen > 4)
		aName[NameLen - 4] = '\0';
	if(str_find_nocase(pQuestion, aName))
		return true;

	const char *p = pQuestion;
	while(*p)
	{
		const char *pEnd = p;
		int Chars = 0;
		for(; Chars < 4 && *pEnd; Chars++)
			str_utf8_decode(&pEnd);
		if(Chars < 4)
			break;
		char aFragment[20];
		str_copy(aFragment, p, (int)(pEnd - p) + 1);
		if(str_find(pContent, aFragment))
			return true;
		str_utf8_decode(&p);
	}
	return false;
}

// 枚举并筛选知识库，注入文本追加到 aKbBuf（返回追加长度）
int CAiClient::LoadKnowledge(const char *pQuestion, char *aKbBuf, int KbBufSize)
{
	std::set<std::string> aFiles;
	Storage()->ListDirectory(IStorage::TYPE_ALL, "ranbi/knowledge", KnowledgeListCallback, &aFiles);

	int TotalLen = 0;
	for(const std::string &FileName : aFiles)
	{
		if(TotalLen >= KbBufSize)
			break;
		char aPath[512];
		str_format(aPath, sizeof(aPath), "ranbi/knowledge/%s", FileName.c_str());
		char *pContent = Storage()->ReadFileStr(aPath, IStorage::TYPE_ALL);
		if(!pContent)
			continue;
		if(!KnowledgeHit(pQuestion, FileName.c_str(), pContent))
		{
			free(pContent);
			continue;
		}
		const int ContentLen = str_length(pContent);
		const int UseLen = minimum(ContentLen, 4096);
		const int HeaderLen = str_format(aKbBuf + TotalLen, KbBufSize - TotalLen, "\n[知识库: %s]\n", FileName.c_str());
		if(HeaderLen < 0 || HeaderLen >= KbBufSize - TotalLen)
		{
			free(pContent);
			break;
		}
		TotalLen += HeaderLen;
		const int CopyLen = minimum(UseLen, KbBufSize - TotalLen - 1);
		str_copy(aKbBuf + TotalLen, pContent, CopyLen + 1);
		TotalLen += CopyLen;
		m_KnowledgeHit = true;
		free(pContent);
	}
	return TotalLen;
}

void CAiClient::SendRequest(const char *pText, const char *pSpeaker, int Dummy, int Team)
{
	// RANBICLIENT m_RcAiModel / m_RcAiBaseUrl / m_RcAiToken
	char aModel[512];
	EscapeJson(aModel, sizeof(aModel), g_Config.m_RcAiModel);

	char aSystem[9000];
	str_copy(aSystem, "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断", sizeof(aSystem));
	int SystemLen = str_length(aSystem);
	m_KnowledgeHit = false;
	SystemLen += LoadKnowledge(pText, aSystem + SystemLen, sizeof(aSystem) - SystemLen);

	char aSystemEsc[36000];
	EscapeJson(aSystemEsc, sizeof(aSystemEsc), aSystem);

	const int UrlLen = str_length(g_Config.m_RcAiBaseUrl);
	char aUrl[512];
	if(UrlLen > 0 && g_Config.m_RcAiBaseUrl[UrlLen - 1] == '/')
		str_format(aUrl, sizeof(aUrl), "%schat/completions", g_Config.m_RcAiBaseUrl);
	else
		str_format(aUrl, sizeof(aUrl), "%s/chat/completions", g_Config.m_RcAiBaseUrl);

	char aToken[512];
	str_format(aToken, sizeof(aToken), "Bearer %s", g_Config.m_RcAiToken);

	char aBody[65536];
	int BodyLen = str_format(aBody, sizeof(aBody),
		"{\"model\":\"%s\",\"temperature\":%.1f,\"messages\":[{\"role\":\"system\",\"content\":\"%s\"",
		aModel, g_Config.m_RcAiTemperature / 10.0f, aSystemEsc);

	for(const CContextEntry &Entry : m_Context)
	{
		if(BodyLen + 2000 >= (int)sizeof(aBody))
			break;
		if(Entry.m_IsReply)
		{
			char aEsc[1536];
			EscapeJson(aEsc, sizeof(aEsc), Entry.m_aText);
			BodyLen += str_format(aBody + BodyLen, sizeof(aBody) - BodyLen, ",{\"role\":\"assistant\",\"content\":\"%s\"}", aEsc);
		}
		else
		{
			char aMsg[300];
			str_format(aMsg, sizeof(aMsg), "%s: %s", Entry.m_aSpeaker, Entry.m_aText);
			char aEsc[1536];
			EscapeJson(aEsc, sizeof(aEsc), aMsg);
			BodyLen += str_format(aBody + BodyLen, sizeof(aBody) - BodyLen, ",{\"role\":\"user\",\"content\":\"%s\"}", aEsc);
		}
	}

	char aCurMsg[300];
	str_format(aCurMsg, sizeof(aCurMsg), "%s: %s", pSpeaker, pText);
	char aCurEsc[1536];
	EscapeJson(aCurEsc, sizeof(aCurEsc), aCurMsg);
	BodyLen += str_format(aBody + BodyLen, sizeof(aBody) - BodyLen, ",{\"role\":\"user\",\"content\":\"%s\"}]}", aCurEsc);

	m_pRequest = HttpPostJson(aUrl, aBody);
	m_pRequest->HeaderString("Authorization", aToken);
	m_pRequest->Timeout(CTimeout{10000, 60000, 500, 5});
	Http()->Run(m_pRequest);
	m_ReplyDummy = Dummy;
	m_ReplyTeam = Team;
	m_aNextReplyTime[Dummy] = time_get() + time_freq() * g_Config.m_RcAiReplyInterval;
}

void CAiClient::HandleResponse()
{
	if(m_pRequest->State() != EHttpState::DONE)
	{
		dbg_msg("ranbi_ai", "[AI失败] 请求状态异常(%d)", (int)m_pRequest->State());
		m_pRequest = nullptr;
		return;
	}
	if(m_pRequest->StatusCode() < 200 || m_pRequest->StatusCode() >= 300)
	{
		dbg_msg("ranbi_ai", "[AI失败] HTTP状态码 %d", m_pRequest->StatusCode());
		m_pRequest = nullptr;
		return;
	}
	json_value *pObj = m_pRequest->ResultJson();
	if(!pObj)
	{
		dbg_msg("ranbi_ai", "[AI失败] 响应JSON解析失败");
		m_pRequest = nullptr;
		return;
	}
	const json_value *pChoices = json_object_get(pObj, "choices");
	const json_value *pFirst = json_array_length(pChoices) > 0 ? json_array_get(pChoices, 0) : nullptr;
	const json_value *pMessage = pFirst ? json_object_get(pFirst, "message") : nullptr;
	const json_value *pContent = pMessage ? json_object_get(pMessage, "content") : nullptr;
	const char *pReply = pContent && pContent->type == json_string ? json_string_get(pContent) : nullptr;
	if(!pReply)
	{
		dbg_msg("ranbi_ai", "[AI失败] 响应中无 content 字段");
		json_value_free(pObj);
		m_pRequest = nullptr;
		return;
	}

	char aReply[512];
	str_copy(aReply, pReply, sizeof(aReply));
	TruncateReply(aReply, sizeof(aReply));

	const json_value *pUsage = json_object_get(pObj, "usage");
	const json_value *pTokens = pUsage ? json_object_get(pUsage, "completion_tokens") : nullptr;
	const int Tokens = pTokens && pTokens->type == json_int ? (int)json_int_get(pTokens) : 0;

	char aMessage[540];
	str_format(aMessage, sizeof(aMessage), "%s: %s", m_aPendingSpeaker, aReply);

	int Length = 0;
	for(const char *p = aReply; *p;)
	{
		const char *pNext = p;
		const int Ch = str_utf8_decode(&pNext);
		Length += (Ch >= 0x4E00 && Ch <= 0x9FFF) ? 16 : 10;
		p = pNext;
	}

	if(g_Config.m_RcAiContextCount > 0)
	{
		CContextEntry UserEntry;
		str_copy(UserEntry.m_aSpeaker, m_aPendingSpeaker, sizeof(UserEntry.m_aSpeaker));
		str_copy(UserEntry.m_aText, m_aPendingText, sizeof(UserEntry.m_aText));
		UserEntry.m_IsReply = false;
		m_Context.push_back(UserEntry);

		CContextEntry ReplyEntry;
		ReplyEntry.m_aSpeaker[0] = '\0';
		str_copy(ReplyEntry.m_aText, aReply, sizeof(ReplyEntry.m_aText));
		ReplyEntry.m_IsReply = true;
		m_Context.push_back(ReplyEntry);

		while((int)m_Context.size() > g_Config.m_RcAiContextCount * 2)
			m_Context.erase(m_Context.begin());
	}

	dbg_msg("ranbi_ai", "[AI] 说话人: %s | 回复给: %s | 回复: %s | 知识库: %s | 长度: %d/128 | tokens: %d",
		m_aPendingSpeaker, m_ReplyDummy == 0 ? "本体" : "分身", aReply, m_KnowledgeHit ? "是" : "否", Length / 10, Tokens);

	CNetMsg_Cl_Say Msg;
	Msg.m_Team = m_ReplyTeam;
	Msg.m_pMessage = aMessage;
	Client()->SendPackMsg(m_ReplyDummy, &Msg, MSGFLAG_VITAL);

	json_value_free(pObj);
	m_pRequest = nullptr;
}

void CAiClient::OnMessage(int MsgType, void *pRawMsg)
{
	// RANBICLIENT m_RcAiAutoReply
	if(MsgType != NETMSGTYPE_SV_CHAT)
		return;
	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(!g_Config.m_RcAiAutoReply || pMsg->m_ClientId < 0)
		return;
	if(pMsg->m_ClientId == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && pMsg->m_ClientId == GameClient()->m_aLocalIds[1]))
		return; // 自己的消息不触发

	int Dummy;
	if(!ParseMention(pMsg->m_pMessage, Dummy))
		return;
	if(time_get() < m_aNextReplyTime[Dummy])
		return;
	if(m_pRequest)
		return;

	str_copy(m_aPendingSpeaker, GameClient()->m_aClients[pMsg->m_ClientId].m_aName, sizeof(m_aPendingSpeaker));
	str_copy(m_aPendingText, pMsg->m_pMessage, sizeof(m_aPendingText));
	SendRequest(m_aPendingText, m_aPendingSpeaker, Dummy, pMsg->m_Team < 0 ? 0 : pMsg->m_Team);
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

- [ ] **Step 3: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2 && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告。若报 `EscapeJson`/`str_find_nocase` 未声明，确认 include 了 `<engine/shared/json.h>`（str_find_nocase 在 `<base/system.h>`）；若报 `minimum` 未定义，确认 include 了 `<base/system.h>`；若报 `Storage` 未声明，确认 include 了 `<engine/storage.h>`

- [ ] **Step 4: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2
git add src/game/client/components/ranbi/ai_client.h src/game/client/components/ranbi/ai_client.cpp
git commit --no-gpg-sign -m "扩展AI客户端：词边界检测/上下文队列/知识库/温度配置"
```

---

### Task 3: 菜单滑块与翻译

**Files:**
- Modify: `src/game/client/components/ranbi/menus_ranbi.cpp`（RenderRanbiAI 的 Other 区域）
- Modify: `data/ranbi/languages/simplified_chinese.txt`（末尾追加）

**Interfaces:**
- Consumes: `g_Config.m_RcAiReplyInterval` / `m_RcAiTemperature` / `m_RcAiContextCount`（Task 1）
- Produces: Other 区域 3 个滑块

- [ ] **Step 1: 在 Other 区域追加 3 个滑块**

在 `menus_ranbi.cpp` 的 `RenderRanbiAI` 函数中，`DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_RcAiAutoReply, ...)` 之后、`s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;` 之前插入：

```cpp
	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcAiReplyInterval, &g_Config.m_RcAiReplyInterval, &Button, RCLocalize("Reply interval (seconds)"), 1, 60);

	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcAiTemperature, &g_Config.m_RcAiTemperature, &Button, RCLocalize("Temperature (x0.1)"), 0, 20);

	Column.HSplitTop(s_LineSize + s_MarginExtraSmall, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_RcAiContextCount, &g_Config.m_RcAiContextCount, &Button, RCLocalize("Context count"), 0, 20);
```

（DoScrollbarOption 签名：`(pId, pOption, pRect, pStr, Min, Max, pScale, Flags, pSuffix)`，pId/pOption 都传 `&g_Config.m_RcXxx`，与 menus_ranbi.cpp:119 现有写法一致；滑块自动显示 "标签: 值"）

- [ ] **Step 2: 翻译文件追加条目**

在 `data/ranbi/languages/simplified_chinese.txt` 末尾追加（条目之间空一行）：

```
Reply interval (seconds)
== 回复间隔（秒）

Temperature (x0.1)
== 温度（×0.1）

Context count
== 上下文条数
```

- [ ] **Step 3: 编译验证**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2 && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 4: 提交**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2
git add src/game/client/components/ranbi/menus_ranbi.cpp data/ranbi/languages/simplified_chinese.txt
git commit --no-gpg-sign -m "添加AI设置菜单滑块与翻译"
```

---

### Task 4: 格式化与最终验证

**Files:**
- All files from Tasks 1-3

- [ ] **Step 1: 运行项目格式化脚本**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2 && export PATH="/c/Users/fu/AppData/Roaming/Python/Python313/Scripts:$PATH" && python scripts/fix_style.py`
预期：脚本输出格式化后的文件列表；**必须还原 3 个预期外文件**：`git checkout -- src/game/client/components/nameplates.cpp src/game/client/components/players.cpp src/game/client/components/tclient/moving_tiles.h`（若被改动）

- [ ] **Step 2: 最终全量编译**

运行：`cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2 && cmake --build build/Debug -j 8`
预期：编译成功，无错误无警告

- [ ] **Step 3: 检查改动范围**

运行：`git status` 与 `git diff --stat`
预期：仅包含本计划的 5 个文件（config_variables_ranbi.h、ai_client.h、ai_client.cpp、menus_ranbi.cpp、simplified_chinese.txt），无其他文件被改动

- [ ] **Step 4: 提交格式化改动（如有）**

```bash
cd /c/Code/Projects/RanbiClient/.worktrees/ai-settings-v2
git add -u
git commit --no-gpg-sign -m "格式化AI设置扩展改动"
```

- [ ] **Step 5: 手动实测清单（由用户执行）**

1. 任意位置出现名字触发（词边界：短名字不匹配子串；`Merikaros: ` 空文本、`Merikaros: 666`、`你好 Merikaros` 均触发；`aMerikarosb` 不触发）
2. 回复带 `说话者名: ` 前缀
3. 上下文多轮：AI 引用前几轮对话、能区分说话者；Context count 配置生效；设为 0 后单轮
4. 知识库：在 `C:\Users\<user>\AppData\Roaming\DDNet\ranbi\knowledge\` 放 `测试.txt`，提问含"测试"或文件内容片段 → 注入（debug 知识库: 是）；提问无关 → 否；运行中修改文件下次触发即生效
5. Reply interval 滑块改 1 → 节流 1 秒；改 60 → 60 秒
6. Temperature 滑块生效（请求体含 temperature 字段）
7. debug 输出为单行：`[AI] 说话人: X | 回复给: 本体 | 回复: xxx | 知识库: 是/否 | 长度: n/128 | tokens: n`；失败时 `[AI失败] 原因`
