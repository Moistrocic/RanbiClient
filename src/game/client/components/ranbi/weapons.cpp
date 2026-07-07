#include "weapons.h"

#include <engine/shared/config.h>

#include <game/client/components/ranbi/ranbi_client.h>
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

void CWeapons::OnRender()
{
	if(!g_Config.m_RcShowWeaponsAngle)
		return;

	const auto &vWeapons = GameClient()->m_RanbiClient.m_vWeapons;
	if(vWeapons.empty())
		return;

	const int CurId = GameClient()->m_Snap.m_SpecInfo.m_Active &&
					  GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW ?
				  GameClient()->m_Snap.m_SpecInfo.m_SpectatorId :
				  GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(CurId < 0 || CurId >= MAX_CLIENTS)
		return;

	const CGameClient::CClientData &Client = GameClient()->m_aClients[CurId];
	if(!Client.m_Active)
		return;

	int ActiveWeapon = Client.m_Predicted.m_ActiveWeapon;
	vec2 Pos = GameClient()->m_Camera.m_Center;
	float AimRad;
	if(GameClient()->m_Snap.m_aCharacters[CurId].m_HasExtendedDisplayInfo)
	{
		const CNetObj_DDNetCharacter &Ext = GameClient()->m_Snap.m_aCharacters[CurId].m_ExtendedData;
		AimRad = std::atan2((float)Ext.m_TargetY, (float)Ext.m_TargetX);
	}
	else
	{
		AimRad = Client.m_Predicted.m_Angle / 256.0f * 2.0f * pi;
	}
	if(AimRad < 0.0f)
		AimRad += 2.0f * pi;
	float AimDeg = AimRad * 180.0f / pi;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();

	for(const SWeaponInfo &Weapon : vWeapons)
	{
		if(Weapon.m_WeaponID != ActiveWeapon)
			continue;

		bool Matched = false;
		if(Weapon.m_AngleEnd >= Weapon.m_AngleStart)
			Matched = AimDeg >= Weapon.m_AngleStart && AimDeg <= Weapon.m_AngleEnd;
		else
			Matched = AimDeg >= Weapon.m_AngleStart || AimDeg <= Weapon.m_AngleEnd;

		float StartRad = Weapon.m_AngleStart * pi / 180.0f;
		float EndRad = Weapon.m_AngleEnd * pi / 180.0f;
		if(EndRad < StartRad)
			EndRad += 2.0f * pi;

		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(Weapon.m_Color));
		Graphics()->SetColor(Col.r, Col.g, Col.b, Matched ? 0.8f : 0.5f);

		const float InnerRadius = (float)g_Config.m_RcWeaponsAngleInnerRadius;
		const float OuterRadius = (float)g_Config.m_RcWeaponsAngleRadius;
		const int Segments = 32;
		const float AngleStep = (EndRad - StartRad) / (float)Segments;

		std::vector<IGraphics::CFreeformItem> vItems;
		vItems.reserve(Segments);

		for(int i = 0; i < Segments; i++)
		{
			float A1 = StartRad + AngleStep * (float)i;
			float A2 = StartRad + AngleStep * (float)(i + 1);
			vec2 Inner1 = Pos + vec2(std::cos(A1), std::sin(A1)) * InnerRadius;
			vec2 Outer1 = Pos + vec2(std::cos(A1), std::sin(A1)) * OuterRadius;
			vec2 Inner2 = Pos + vec2(std::cos(A2), std::sin(A2)) * InnerRadius;
			vec2 Outer2 = Pos + vec2(std::cos(A2), std::sin(A2)) * OuterRadius;

			vItems.emplace_back(Inner1.x, Inner1.y, Outer1.x, Outer1.y, Inner2.x, Inner2.y, Outer2.x, Outer2.y);
		}

		Graphics()->QuadsDrawFreeform(vItems.data(), vItems.size());

		if(Matched && Weapon.m_aNote[0] != '\0')
		{
			Graphics()->QuadsEnd();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

			vec2 TextPos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy] + vec2(12.0f, 12.0f);
			float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
			Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
			Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y);

			float FontSize = (float)g_Config.m_RcWeaponsAngleFontSize;
			float TextW = TextRender()->TextWidth(FontSize, Weapon.m_aNote);

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			float Pad = 3.0f;
			Graphics()->SetColor(1.0f - Col.r, 1.0f - Col.g, 1.0f - Col.b, 0.5f);
			IGraphics::CFreeformItem BgItem(
				TextPos.x - TextW * 0.5f - Pad, TextPos.y - Pad,
				TextPos.x + TextW * 0.5f + Pad, TextPos.y - Pad,
				TextPos.x - TextW * 0.5f - Pad, TextPos.y + FontSize + Pad,
				TextPos.x + TextW * 0.5f + Pad, TextPos.y + FontSize + Pad);
			Graphics()->QuadsDrawFreeform(&BgItem, 1);
			Graphics()->QuadsEnd();

			TextRender()->TextColor(Col);
			TextRender()->Text(TextPos.x - TextW * 0.5f, TextPos.y, FontSize, Weapon.m_aNote);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
		}
	}

	Graphics()->QuadsEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
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
