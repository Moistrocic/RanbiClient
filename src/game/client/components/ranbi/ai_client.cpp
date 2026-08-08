#include "ai_client.h"

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h>

// 默认知识库内容（总结自 DDNet 中文 wiki：https://wiki.ddnet.org/wiki/Main_Page/zh），首次启动时写入用户配置目录
static const char s_aDefaultKnowledge[] =
	"DDNet（DDraceNetwork）是一款免费开源的横向卷轴平台游戏，由Teeworlds的DDRace模组发展而来，核心是独特的合作玩法，支持最多64名玩家一起通关。官方服务器遍布全球，可在官网ddnet.org或Steam（搜索DDraceNetwork）免费下载。\n"
	"\n"
	"【入门】玩家角色叫Tee（毛球/猫球/小人）。基本操作：A/D左右移动，Space跳跃，鼠标左键使用武器，鼠标右键使用钩索，Shift打开表情菜单。默认武器为锤子（可击打解冻他人）和手枪，地图中可收集霰弹枪、榴弹枪、激光枪、武士刀。建议先完成教学服务器，再从简单（Novice）地图开始。\n"
	"\n"
	"【机制】冻结区域会冻结Tee，被其他Tee锤击可解冻；常见地图元素有传送器、加速带、开关层、阻挡器、喷气背包（手枪的特殊能力）、分身Dummy（第二个角色）。常用技巧：锤子飞hf、锤击hh、榴弹飞rf、钩飞、二段跳dj、边缘跳。\n"
	"\n"
	"【模式与难度】主要模式是DDRace合作通关，另有竞速Race、阻碍Block、冻结与捕获FNG、原版Vanilla、感染Infection等。地图类型：简单Novice、中阶Moderate、高阶Brutal、疯狂Insane、传统Oldschool、古典DDmaX、单人Solo、娱乐Fun（无分数），星级越多越难（0-5星）。\n"
	"\n"
	"【积分排名】在官方服务器完成地图即可获得积分与排名，分数=星级×倍数+初始值。默认队伍（team 0）完成得个人排名，/team加入队伍后完成得团队排名。/rank查看当前地图排名，/points查看总分，同一张地图的全球分数只算一次。\n"
	"\n"
	"【常用指令】/team 数字 加入队伍，/lock 锁定队伍，/invite 邀请，/save 存档，/load 密码 读档，/map 地图名 换图。和朋友玩：进空服后输入/team同一数字并/lock。\n"
	"\n"
	"【外观】设置-玩家标签改名称/战队/旗帜，Tee标签改皮肤外观，皮肤可到ddnet.org/skins或skins.tw下载更多。官方会定期举办锦标赛（Tournament），2人组队争夺新图最佳成绩。\n"
	"\n"
	"【常见问题】配置目录：Windows为%appdata%\\DDNet，Linux为~/.local/share/ddnet，macOS为~/Library/Application Support/DDNet，配置文件settings_ddnet.cfg。服务器列表刷不出来可看ddnet.org/status，或直接输IP连接，如chn0.ddnet.org:8308（端口8300是教学服）。\n"
	"\n"
	"【术语】b=请求返回救援，re=重开，rq=怒退，flw=跟随，r1=第一名，t0=默认队伍，hj/hh=锤击解冻；阻碍者Blocker指故意让其他玩家失败的玩家。\n";

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

void CAiClient::OnInit()
{
	// RANBICLIENT 初始化知识库文件夹，首次启动写入默认知识库（已存在则不覆盖）
	Storage()->CreateFolder("ranbi", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ranbi/knowledge", IStorage::TYPE_SAVE);

	const char *pPath = "ranbi/knowledge/ddnet_wiki.txt";
	IOHANDLE ReadHandle = Storage()->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_ALL);
	if(ReadHandle)
	{
		io_close(ReadHandle);
		return;
	}
	IOHANDLE WriteHandle = Storage()->OpenFile(pPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!WriteHandle)
		return;
	const int KnowledgeLen = str_length(s_aDefaultKnowledge);
	if(io_write(WriteHandle, s_aDefaultKnowledge, KnowledgeLen) != (unsigned)KnowledgeLen)
		Storage()->RemoveFile(pPath, IStorage::TYPE_SAVE);
	io_close(WriteHandle);
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

// 命中规则：提问包含文件名（去 .txt，不区分大小写）、提问中的字母数字单词在内容中独立出现，或提问任一连续4字符片段出现在内容中
static bool KnowledgeHit(const char *pQuestion, const char *pFileName, const char *pContent)
{
	char aName[128];
	str_copy(aName, pFileName, sizeof(aName));
	const int NameLen = str_length(aName);
	if(NameLen > 4)
		aName[NameLen - 4] = '\0';
	if(str_find_nocase(pQuestion, aName))
		return true;

	// 提问中的字母数字单词（长度≥2）作为独立词出现在内容中即命中
	const char *p = pQuestion;
	while(*p)
	{
		if(IsWordChar(*p))
		{
			const char *pWord = p;
			while(IsWordChar(*p))
				++p;
			const int WordLen = (int)(p - pWord);
			if(WordLen >= 2 && WordLen < 64)
			{
				char aWord[64];
				str_copy(aWord, pWord, WordLen + 1);
				const char *pFound = pContent;
				while((pFound = str_find_nocase(pFound, aWord)) != nullptr)
				{
					const bool LeftOk = pFound == pContent || !IsWordChar(pFound[-1]);
					const bool RightOk = !IsWordChar(pFound[WordLen]);
					if(LeftOk && RightOk)
						return true;
					pFound += WordLen;
				}
			}
		}
		else
		{
			const char *pNext = p;
			if(str_utf8_decode(&pNext) == -1)
				break;
			p = pNext;
		}
	}

	// 提问任一连续4字符片段出现在内容中
	const char *p2 = pQuestion;
	while(*p2)
	{
		const char *pEnd = p2;
		int Chars = 0;
		for(; Chars < 4 && *pEnd; Chars++)
			str_utf8_decode(&pEnd);
		if(Chars < 4)
			break;
		char aFragment[20];
		str_copy(aFragment, p2, (int)(pEnd - p2) + 1);
		if(str_find(pContent, aFragment))
			return true;
		const char *pNext = p2;
		if(str_utf8_decode(&pNext) == -1)
			break;
		p2 = pNext;
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
		// 单文件上限 4KB，超限跳过（避免主线程读取大文件卡顿）
		IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_ALL);
		if(!File)
			continue;
		const int64_t FileSize = io_length(File);
		io_close(File);
		if(FileSize > 4096)
			continue;
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

	// RANBICLIENT m_RcAiSystemPrompt
	char aSystem[9000];
	const char *pPrompt = g_Config.m_RcAiSystemPrompt[0] != '\0' ? g_Config.m_RcAiSystemPrompt : "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断";
	str_copy(aSystem, pPrompt, sizeof(aSystem));
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
		"{\"model\":\"%s\",\"temperature\":%.1f,\"messages\":[{\"role\":\"system\",\"content\":\"%s\"}",
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
		dbg_msg("ranbi_ai", "[AI失败] 请求异常 state=%d", (int)m_pRequest->State());
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
	const int Tokens = pTokens && pTokens->type == json_integer ? (int)json_int_get(pTokens) : 0;

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
