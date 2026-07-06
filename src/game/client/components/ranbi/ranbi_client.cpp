#include "ranbi_client.h"

#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/skins7.h>
#include <game/client/gameclient.h>
#include <game/gamecore.h>

CRanbiClient::CRanbiClient()
{
	OnReset();
}

void CRanbiClient::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::RANBIWEAPONS);

	Console()->Register("add_weapon_angle", "i[weapon_id] f[angle_start] f[angle_end] i[color] ?s[note]", CFGFLAG_CLIENT, ConAddWeaponAngle, this, "Add a weapon angle config");
}

void CRanbiClient::OnUpdate()
{
	// RANBICLIENT m_RcAutoChangeSkin
	if(g_Config.m_RcAutoChangeSkin)
	{
		m_LastCollisionSkin.Reset();

		for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		{
			int LocalId = GameClient()->m_aLocalIds[Dummy];
			if(LocalId < 0 || LocalId >= MAX_CLIENTS)
				continue;

			const CGameClient::CClientData &LocalClient = GameClient()->m_aClients[LocalId];
			if(!LocalClient.m_Active)
				continue;

			vec2 LocalPos = LocalClient.m_Predicted.m_Pos;
			float ClosestDist = CCharacterCore::PhysicalSize() * 1.25f;
			int ClosestId = -1;

			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(i == LocalId)
					continue;

				const CGameClient::CClientData &OtherClient = GameClient()->m_aClients[i];
				if(!OtherClient.m_Active)
					continue;

				float Dist = distance(LocalPos, OtherClient.m_Predicted.m_Pos);
				if(Dist < ClosestDist)
				{
					ClosestDist = Dist;
					ClosestId = i;
				}
			}

			if(ClosestId < 0)
				continue;

			const CGameClient::CClientData &CollidedClient = GameClient()->m_aClients[ClosestId];

			m_LastCollisionSkin.m_ClientId = ClosestId;
			m_LastCollisionSkin.m_Valid = true;

			str_copy(m_LastCollisionSkin.m_aSkinName, CollidedClient.m_aSkinName);
			m_LastCollisionSkin.m_UseCustomColor = CollidedClient.m_UseCustomColor;
			m_LastCollisionSkin.m_ColorBody = CollidedClient.m_ColorBody;
			m_LastCollisionSkin.m_ColorFeet = CollidedClient.m_ColorFeet;

			// RANBICLIENT m_RcAutoChangeSkin: compare and replace skin if different
			bool NeedChange = false;

			if(str_comp(LocalClient.m_aSkinName, CollidedClient.m_aSkinName) != 0)
				NeedChange = true;
			else if(LocalClient.m_UseCustomColor != CollidedClient.m_UseCustomColor)
				NeedChange = true;
			else if(LocalClient.m_ColorBody != CollidedClient.m_ColorBody)
				NeedChange = true;
			else if(LocalClient.m_ColorFeet != CollidedClient.m_ColorFeet)
				NeedChange = true;

			if(NeedChange)
			{
				if(Dummy == 0)
				{
					str_copy(g_Config.m_ClPlayerSkin, CollidedClient.m_aSkinName);
					g_Config.m_ClPlayerUseCustomColor = CollidedClient.m_UseCustomColor;
					g_Config.m_ClPlayerColorBody = CollidedClient.m_ColorBody;
					g_Config.m_ClPlayerColorFeet = CollidedClient.m_ColorFeet;
				}
				else
				{
					str_copy(g_Config.m_ClDummySkin, CollidedClient.m_aSkinName);
					g_Config.m_ClDummyUseCustomColor = CollidedClient.m_UseCustomColor;
					g_Config.m_ClDummyColorBody = CollidedClient.m_ColorBody;
					g_Config.m_ClDummyColorFeet = CollidedClient.m_ColorFeet;
				}

				if(Dummy == 0)
					GameClient()->SendInfo(false);
				else
					GameClient()->SendDummyInfo(false);
			}
			return;
		}
	}
}

void CRanbiClient::ConAddWeaponAngle(IConsole::IResult *pResult, void *pUserData)
{
	CRanbiClient *pThis = (CRanbiClient *)pUserData;
	SWeaponInfo Weapon;
	Weapon.m_WeaponID = pResult->GetInteger(0);
	Weapon.m_AngleStart = pResult->GetFloat(1);
	Weapon.m_AngleEnd = pResult->GetFloat(2);
	Weapon.m_Color = pResult->GetInteger(3);
	str_copy(Weapon.m_aNote, pResult->GetString(4));
	pThis->m_vWeapons.push_back(Weapon);
}

void CRanbiClient::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CRanbiClient *pThis = (CRanbiClient *)pUserData;

	char aBuf[256];
	for(const SWeaponInfo &Weapon : pThis->m_vWeapons)
	{
		str_format(aBuf, sizeof(aBuf), "add_weapon_angle %d %.2f %.2f %d \"%s\"",
			Weapon.m_WeaponID, Weapon.m_AngleStart, Weapon.m_AngleEnd, Weapon.m_Color, Weapon.m_aNote);
		pConfigManager->WriteLine(aBuf, ConfigDomain::RANBIWEAPONS);
	}
}
