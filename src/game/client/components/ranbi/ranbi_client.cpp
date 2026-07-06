#include "ranbi_client.h"

#include <base/system.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

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
