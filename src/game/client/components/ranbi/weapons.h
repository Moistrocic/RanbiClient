#ifndef GAME_CLIENT_COMPONENTS_RANBI_WEAPONS_H
#define GAME_CLIENT_COMPONENTS_RANBI_WEAPONS_H

#include <engine/client/enums.h>
#include <engine/console.h>

#include <game/client/component.h>

class CWeapons : public CComponent
{
	int m_aRecentWeapon[NUM_DUMMIES];
	int m_aLastActiveWeapon[NUM_DUMMIES];

	static void ConSwitchRecentWeapon(IConsole::IResult *pResult, void *pUserData);

public:
	CWeapons();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnReset() override;
};

#endif
