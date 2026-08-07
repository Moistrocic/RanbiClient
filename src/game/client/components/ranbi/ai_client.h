#ifndef GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_AI_CLIENT_H

#include <engine/client/enums.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>

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
