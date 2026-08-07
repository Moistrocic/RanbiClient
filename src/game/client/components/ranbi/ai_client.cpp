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

// 解析被 @ 消息：一段式 "我的名字: 内容" 或三段式 "名字1: 我的名字: 内容"；名字区分大小写
bool CAiClient::ParseMention(const char *pText, int &Dummy, const char **ppText)
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

		// 一段式：消息以 "我的名字:" 或 "我的名字: " 开头（空文本 @ 无尾随空格）
		if(str_comp_num(pText, pName, NameLen) == 0 && pText[NameLen] == ':')
		{
			const char *pAfter = pText + NameLen + 1;
			if(pAfter[0] == '\0' || pAfter[0] == ' ')
			{
				if(pAfter[0] == ' ')
					pAfter++;
				Dummy = D;
				*ppText = pAfter;
				return true;
			}
		}

		// 三段式兼容：消息含 "名字1: 我的名字: "
		const char *pColon1 = str_find(pText, ": ");
		if(pColon1)
		{
			const char *pColon2 = str_find(pColon1 + 2, ": ");
			if(pColon2)
			{
				const int SegmentLen = pColon2 - (pColon1 + 2);
				if(SegmentLen == NameLen && str_comp_num(pColon1 + 2, pName, NameLen) == 0)
				{
					Dummy = D;
					*ppText = pColon2 + 2;
					return true;
				}
			}
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
	char aSystem[1024];
	EscapeJson(aSystem, sizeof(aSystem), "你是DDNet这款游戏的玩家，你需要回复其他玩家跟你的谈话，且谈话可能为空，可能仅是打招呼。每次回复长度保证在80汉字或128字母内，过长的回复会被截断");
	char aBody[4096];
	str_format(aBody, sizeof(aBody),
		"{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"%s\"},{\"role\":\"user\",\"content\":\"%s\"}]}",
		aModel, aSystem, aText);

	const int UrlLen = str_length(g_Config.m_RcAiBaseUrl);
	char aUrl[512];
	if(UrlLen > 0 && g_Config.m_RcAiBaseUrl[UrlLen - 1] == '/')
		str_format(aUrl, sizeof(aUrl), "%schat/completions", g_Config.m_RcAiBaseUrl);
	else
		str_format(aUrl, sizeof(aUrl), "%s/chat/completions", g_Config.m_RcAiBaseUrl);

	char aToken[512];
	str_format(aToken, sizeof(aToken), "Bearer %s", g_Config.m_RcAiToken);

	dbg_msg("ranbi_ai", "send request: url=%s model=%s", aUrl, g_Config.m_RcAiModel);
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
		dbg_msg("ranbi_ai", "response: state=%d (not DONE, dropped)", (int)m_pRequest->State());
		m_pRequest = nullptr;
		return;
	}
	if(m_pRequest->StatusCode() < 200 || m_pRequest->StatusCode() >= 300)
	{
		dbg_msg("ranbi_ai", "response: http status=%d (dropped)", m_pRequest->StatusCode());
		m_pRequest = nullptr;
		return;
	}
	json_value *pObj = m_pRequest->ResultJson();
	if(!pObj)
	{
		dbg_msg("ranbi_ai", "response: invalid json (dropped)");
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
		dbg_msg("ranbi_ai", "response: reply=\"%s\" (dummy=%d team=%d)", aReply, m_ReplyDummy, m_ReplyTeam);
		CNetMsg_Cl_Say Msg;
		Msg.m_Team = m_ReplyTeam;
		Msg.m_pMessage = aReply;
		Client()->SendPackMsg(m_ReplyDummy, &Msg, MSGFLAG_VITAL);
	}
	else
	{
		dbg_msg("ranbi_ai", "response: no content in choices[0].message (dropped)");
	}
	json_value_free(pObj);
	m_pRequest = nullptr;
}

void CAiClient::OnMessage(int MsgType, void *pRawMsg)
{
	// RANBICLIENT m_RcAiAutoReply
	if(MsgType != NETMSGTYPE_SV_CHAT)
		return;
	CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	dbg_msg("ranbi_ai", "chat: client=%d team=%d auto_reply=%d msg=\"%s\"", pMsg->m_ClientId, pMsg->m_Team, g_Config.m_RcAiAutoReply, pMsg->m_pMessage);
	if(!g_Config.m_RcAiAutoReply)
	{
		dbg_msg("ranbi_ai", "  skip: auto reply disabled");
		return;
	}
	if(pMsg->m_ClientId < 0)
	{
		dbg_msg("ranbi_ai", "  skip: server message");
		return;
	}
	if(pMsg->m_ClientId == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && pMsg->m_ClientId == GameClient()->m_aLocalIds[1]))
	{
		dbg_msg("ranbi_ai", "  skip: own message");
		return;
	}

	int Dummy;
	const char *pText;
	if(!ParseMention(pMsg->m_pMessage, Dummy, &pText))
	{
		dbg_msg("ranbi_ai", "  skip: not a mention (expected \"myname: text\" or \"name: myname: text\")");
		return;
	}
	dbg_msg("ranbi_ai", "  mention matched: dummy=%d text=\"%s\"", Dummy, pText);
	if(time_get() < m_aNextReplyTime[Dummy])
	{
		dbg_msg("ranbi_ai", "  skip: throttled, %d s remaining", (int)((m_aNextReplyTime[Dummy] - time_get()) / time_freq()));
		return;
	}
	if(m_pRequest)
	{
		dbg_msg("ranbi_ai", "  skip: request already in flight");
		return;
	}

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
