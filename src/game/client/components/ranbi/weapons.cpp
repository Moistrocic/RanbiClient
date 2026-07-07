#include "weapons.h"

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/gamecore.h>

CWeapons::CWeapons()
{
	OnReset();
}

void CWeapons::OnReset()
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		m_aRecentWeapon[Dummy] = -1;
		m_aLastActiveWeapon[Dummy] = -1;
	}
}

void CWeapons::OnConsoleInit()
{
	Console()->Register("+switch_recent_weapon", "", CFGFLAG_CLIENT, ConSwitchRecentWeapon, this, "Switch to most recently used weapon");
}

void CWeapons::OnUpdate()
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId < 0 || LocalId >= MAX_CLIENTS)
			continue;

		const CGameClient::CClientData &Client = GameClient()->m_aClients[LocalId];
		if(!Client.m_Active)
			continue;

		int CurrentWeapon = Client.m_Predicted.m_ActiveWeapon;
		if(CurrentWeapon < 0 || CurrentWeapon >= NUM_WEAPONS)
			continue;

		if(CurrentWeapon != m_aLastActiveWeapon[Dummy])
		{
			if(m_aLastActiveWeapon[Dummy] >= 0 && m_aLastActiveWeapon[Dummy] < NUM_WEAPONS &&
				Client.m_Predicted.m_aWeapons[m_aLastActiveWeapon[Dummy]].m_Got)
			{
				m_aRecentWeapon[Dummy] = m_aLastActiveWeapon[Dummy];
			}
			m_aLastActiveWeapon[Dummy] = CurrentWeapon;
		}
	}
}

void CWeapons::ConSwitchRecentWeapon(IConsole::IResult *pResult, void *pUserData)
{
	CWeapons *pThis = (CWeapons *)pUserData;
	if(!pResult->GetInteger(0))
		return;

	int Dummy = g_Config.m_ClDummy;
	int RecentWeapon = pThis->m_aRecentWeapon[Dummy];
	if(RecentWeapon < 0 || RecentWeapon >= NUM_WEAPONS)
		return;

	int LocalId = pThis->GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;

	const CGameClient::CClientData &Client = pThis->GameClient()->m_aClients[LocalId];
	if(!Client.m_Active)
		return;

	if(Client.m_Predicted.m_aWeapons[RecentWeapon].m_Got)
		pThis->GameClient()->m_Controls.m_aInputData[Dummy].m_WantedWeapon = RecentWeapon + 1;
}
