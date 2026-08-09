#ifndef GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H

#include <engine/client/enums.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

class CHttpRequest;

class CAiClient : public CComponent
{
	struct CContextEntry
	{
		char m_aSpeaker[16];
		char m_aText[2048];
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
	std::vector<std::string> m_ReplyQueue;

	bool ParseMention(const char *pText, int &Dummy);
	static void TruncateReply(char *pText, int Size);
	static void SplitSentences(const char *pText, std::vector<std::string> &Sentences);
	int LoadKnowledge(const char *pQuestion, char *aKbBuf, int KbBufSize);
	void SendRequest(const char *pText, const char *pSpeaker, int Dummy, int Team);
	void HandleResponse();

public:
	CAiClient();

	int Sizeof() const override { return sizeof(*this); }

	void OnInit() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnUpdate() override;
	void OnReset() override;
};

#endif
