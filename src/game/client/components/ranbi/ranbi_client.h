#ifndef GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H

#include <engine/console.h>

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

class CRanbiClient : public CComponent
{
public:
	CRanbiClient();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;

	std::vector<SWeaponInfo> m_vWeapons;

private:
	static void ConAddWeaponAngle(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);
};

#endif
