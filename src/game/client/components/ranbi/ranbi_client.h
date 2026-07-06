#ifndef GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <generated/protocol7.h>

#include <game/client/component.h>

#include <vector>

struct SWeaponInfo
{
	int m_WeaponID;
	float m_AngleStart;
	float m_AngleEnd;
	unsigned int m_Color;
	char m_aNote[64];
};

struct SCollisionSkinInfo
{
	int m_ClientId;
	bool m_Valid;

	// 0.6/0.7 protocol skin info
	char m_aSkinName[MAX_SKIN_LENGTH];
	int m_UseCustomColor;
	int m_ColorBody;
	int m_ColorFeet;

	void Reset()
	{
		m_ClientId = -1;
		m_Valid = false;
		m_aSkinName[0] = '\0';
		m_UseCustomColor = 0;
		m_ColorBody = 0;
		m_ColorFeet = 0;
	}
};

class CRanbiClient : public CComponent
{
public:
	CRanbiClient();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnUpdate() override;

	SCollisionSkinInfo m_LastCollisionSkin;

	std::vector<SWeaponInfo> m_vWeapons;

private:
	static void ConAddWeaponAngle(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
};

#endif
