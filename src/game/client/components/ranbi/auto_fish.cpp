#include "auto_fish.h"

#include <base/dbg.h>
#include <base/system.h>

#include <stdarg.h>
#include <stdlib.h>

#include <game/client/gameclient.h>

CAutoFish::CAutoFish()
{
	OnReset();
}

void CAutoFish::OnReset()
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		m_aAttackNextPressTime[Dummy] = 0;
		m_aAttackPressEndTime[Dummy] = 0;
		m_aAttackFireInjected[Dummy] = false;
	}

	m_BuyBaitNextCheckTime = 0;

	m_LockAimActive = false;
	m_LockAimTarget = vec2(0, 0);
	m_LockAimSavedZoom = 1.0f;
}

void CAutoFish::OnUpdate()
{
	// AUTO FISH m_RcAutoAttack
	if(g_Config.m_RcAutoAttack)
	{
		const int Dummy = g_Config.m_ClDummy;
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_Active &&
			!GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			const int64_t Now = time_get();
			if(m_aAttackNextPressTime[Dummy] == 0)
			{
				m_aAttackNextPressTime[Dummy] = Now;
				m_aAttackPressEndTime[Dummy] = 0;
			}
			if(Now >= m_aAttackPressEndTime[Dummy])
			{
				GameClient()->m_Controls.m_aInputData[Dummy].m_Fire = 0;
				m_aAttackFireInjected[Dummy] = false;
			}
			if(Now >= m_aAttackNextPressTime[Dummy])
			{
				GameClient()->m_Controls.m_aInputData[Dummy].m_Fire = 1;
				m_aAttackFireInjected[Dummy] = true;
				m_aAttackNextPressTime[Dummy] = Now + time_freq() * g_Config.m_RcAutoAttackInterval / 1000;
				m_aAttackPressEndTime[Dummy] = Now + time_freq() * 40 / 1000;
			}
		}
		else
		{
			if(LocalId >= 0 && LocalId < MAX_CLIENTS && m_aAttackFireInjected[Dummy])
			{
				int &Fire = GameClient()->m_Controls.m_aInputData[Dummy].m_Fire;
				if((Fire & 1) != 0)
					Fire++;
				m_aAttackFireInjected[Dummy] = false;
			}
			m_aAttackNextPressTime[Dummy] = 0;
			m_aAttackPressEndTime[Dummy] = 0;
		}
	}
	else
	{
		const int Dummy = g_Config.m_ClDummy;
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId >= 0 && LocalId < MAX_CLIENTS && m_aAttackFireInjected[Dummy])
		{
			int &Fire = GameClient()->m_Controls.m_aInputData[Dummy].m_Fire;
			if((Fire & 1) != 0)
				Fire++;
			m_aAttackFireInjected[Dummy] = false;
		}
		m_aAttackNextPressTime[Dummy] = 0;
		m_aAttackPressEndTime[Dummy] = 0;
	}

	// AUTO FISH m_RcAutoBuyBait
	if(g_Config.m_RcAutoBuyBait)
	{
		const int Dummy = g_Config.m_ClDummy;
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_Active)
		{
			const int64_t Now = time_get();
			if(m_BuyBaitNextCheckTime == 0)
				m_BuyBaitNextCheckTime = Now;
			if(Now >= m_BuyBaitNextCheckTime)
			{
				int OptionId = 0;
				for(const CVoteOptionClient *pOption = GameClient()->m_Voting.FirstOption(); pOption; pOption = pOption->m_pNext, OptionId++)
				{
					if(str_find_nocase(pOption->m_aDescription, "购买鱼饵"))
					{
						GameClient()->m_Voting.CallvoteOption(OptionId, "", false);
						break;
					}
				}
				m_BuyBaitNextCheckTime = Now + time_freq() * g_Config.m_RcAutoBuyBaitInterval;
			}
		}
		else
		{
			m_BuyBaitNextCheckTime = 0;
		}
	}
	else
	{
		m_BuyBaitNextCheckTime = 0;
	}
}

// AUTO FISH m_RcLockAim：锁定光标瞄准的地图位置
// 实测公式：地图距离 = 光标距离(屏幕单位) × zoom；视觉格数 = 光标距离 × zoom / 32
// 开启时由玩家坐标+光标推算锁定方块；玩家移动后反推光标角度与光标距离；
// 光标距离受限(最大鼠标距离)时保持角度；脱离视野时放大视野保证可见，进入视野时恢复
void CAutoFish::OnRender()
{
	// 状态同步：配置开启时记录玩家坐标，推算光标地图坐标并锁定到方块中心
	if(g_Config.m_RcLockAim && !m_LockAimActive)
	{
		const int Dummy = g_Config.m_ClDummy;
		const vec2 PlayerPos = GameClient()->m_LocalCharacterPos;
		const vec2 MousePos = GameClient()->m_Controls.m_aMousePos[Dummy];
		const float Dist = length(MousePos);
		const float Zoom = GameClient()->m_Camera.m_Zoom;
		// 地图距离 = 光标距离 × zoom；光标地图坐标 = 玩家坐标 + 方向 × 地图距离
		const vec2 Dir = Dist > 0.001f ? MousePos / Dist : vec2(1.0f, 0.0f);
		const vec2 CursorPos = PlayerPos + Dir * (Dist * Zoom);
		m_LockAimTarget = vec2((int)std::floor(CursorPos.x / 32.0f) * 32 + 16, (int)std::floor(CursorPos.y / 32.0f) * 32 + 16);
		m_LockAimSavedZoom = GameClient()->m_Camera.m_UserZoomTarget;
		m_LockAimActive = true;
	}
	else if(!g_Config.m_RcLockAim && m_LockAimActive)
	{
		GameClient()->m_Camera.m_UserZoomTarget = m_LockAimSavedZoom;
		m_LockAimActive = false;
	}

	if(m_LockAimActive)
	{
		const int Dummy = g_Config.m_ClDummy;
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		if(LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_Active &&
			!GameClient()->m_Snap.m_SpecInfo.m_Active)
		{
			// 玩家移动：计算玩家-方块距离和方向，反推光标角度与光标距离(屏幕单位)
			const vec2 PlayerPos = GameClient()->m_LocalCharacterPos;
			const vec2 ToTarget = m_LockAimTarget - PlayerPos;
			const float MapDist = length(ToTarget);
			const float Zoom = GameClient()->m_Camera.m_Zoom;
			vec2 MousePos = ToTarget;
			if(MapDist > 0.001f)
			{
				// 光标距离(屏幕) = 地图距离 / zoom；超过最大鼠标距离时角度保持、距离取限制值
				const float MaxDistance = GameClient()->m_Controls.GetMaxMouseDistance();
				const float CursorDist = MapDist / Zoom;
				MousePos = ToTarget / MapDist * minimum(CursorDist, MaxDistance);
			}
			GameClient()->m_Controls.m_aMousePos[Dummy] = MousePos;

			// 视野：zoom 值越大视野越大
			// 距离条件（保证光标能锁定方块）：光标距离 ≤ 最大距离 → zoom ≥ 地图距离 / 最大距离
			// 视野条件（保证方块在屏幕内，留 10% 边距）：zoom ≥ 方块到相机中心距离 / (半高@zoom=1 × 0.9)
			float BaseHalfH;
			{
				float W, H;
				Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), 1.0f, &W, &H);
				BaseHalfH = H;
			}
			const float DistToCenter = length(m_LockAimTarget - GameClient()->m_Camera.m_Center);
			const float NeedZoom = maximum(MapDist / GameClient()->m_Controls.GetMaxMouseDistance(),
				DistToCenter / (BaseHalfH * 0.9f));
			GameClient()->m_Camera.m_UserZoomTarget = maximum(m_LockAimSavedZoom, NeedZoom);
		}
	}
}

// 控制台消息监控：NETMSGTYPE_SV_CHAT 的类别映射与控制台输出（chat.cpp FChatMsgCheckAndPrint）保持一致
void CAutoFish::OnMessage(int Msg, void *pRawMsg)
{
	if(Msg != NETMSGTYPE_SV_CHAT)
		return;

	const CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	const char *pCategory;
	if(pMsg->m_Team >= 2)
		pCategory = "chat/whisper";
	else if(pMsg->m_Team == 1)
		pCategory = "chat/team";
	else if(pMsg->m_ClientId == -1) // CChat::SERVER_MSG（私有 enum，chat.h:86）
		pCategory = "chat/server";
	else if(pMsg->m_ClientId == -2) // CChat::CLIENT_MSG（私有 enum，chat.h:85）
		pCategory = "chat/client";
	else
		pCategory = "chat/all";

	m_CurrentChatCategory = pCategory;
	m_CurrentChatText = pMsg->m_pMessage;

	// 使用示例：检测玩家进入游戏并输出玩家名
	char aName[128];
	if(ConsoleTriggerCheck(pCategory, "%s entered and joined the game", aName))
	{
		dbg_msg("ranbi/dbg", "name:%s", aName);
	}
}

// 模式编译：支持 scanf/printf 格式符
//   %s → 字符串捕获（char*）          %c → 单字符捕获（char*）
//   %d %i → 十进制整数（int*）        %u → 无符号十进制（unsigned int*）
//   %x %X → 十六进制（unsigned int*） %o → 八进制（unsigned int*）
//   %f %e %E %g %G → 浮点（float*）   %% → 字面 %
// 长度修饰符：h/hh（short*）、l（long* / double*）、ll（long long*）、z（size_t*）、L（long double*）
// 其余字符按字面量匹配（模糊匹配，子串命中即可）
bool CAutoFish::ConsoleTriggerBuildRegex(const char *pPattern)
{
	std::string Escaped;
	m_vConsoleTriggerFormats.clear();
	m_vConsoleTriggerBases.clear();
	const char *p = pPattern;
	while(*p)
	{
		if(*p != '%')
		{
			// 占位符之外的字符按正则特殊字符转义，保证字面匹配
			if(strchr("\\^$.[]|()?*+{}", *p) != nullptr)
				Escaped += '\\';
			Escaped += *p;
			p++;
			continue;
		}

		// 长度修饰符：1=short(h/hh) 2=long(l) 3=long long(ll) 4=size_t(z) 5=long double(L)
		const char *pConv = p + 1;
		int LenMod = 0;
		if(pConv[0] == 'h')
		{
			LenMod = 1;
			pConv += (pConv[1] == 'h') ? 2 : 1;
		}
		else if(pConv[0] == 'l')
		{
			LenMod = (pConv[1] == 'l') ? 3 : 2;
			pConv += (pConv[1] == 'l') ? 2 : 1;
		}
		else if(pConv[0] == 'z')
		{
			LenMod = 4;
			pConv++;
		}
		else if(pConv[0] == 'L')
		{
			LenMod = 5;
			pConv++;
		}

		const char Conv = *pConv;
		EConsoleTriggerType Type;
		const char *pRegex = nullptr;
		int Base = 10;

		switch(Conv)
		{
		case 's':
			Type = EConsoleTriggerType::String;
			pRegex = "(.+?)";
			break;
		case 'c':
			Type = EConsoleTriggerType::Char;
			pRegex = "(.)";
			break;
		case 'd':
		case 'i':
			switch(LenMod)
			{
			case 1: Type = EConsoleTriggerType::Short; break;
			case 2: Type = EConsoleTriggerType::Long; break;
			case 3: Type = EConsoleTriggerType::LongLong; break;
			case 4: Type = EConsoleTriggerType::SizeT; break;
			default: Type = EConsoleTriggerType::Int; break;
			}
			pRegex = "(-?[0-9]+)";
			break;
		case 'u':
			switch(LenMod)
			{
			case 1: Type = EConsoleTriggerType::UShort; break;
			case 2: Type = EConsoleTriggerType::ULong; break;
			case 3: Type = EConsoleTriggerType::ULongLong; break;
			case 4: Type = EConsoleTriggerType::SizeT; break;
			default: Type = EConsoleTriggerType::UInt; break;
			}
			pRegex = "([0-9]+)";
			break;
		case 'x':
		case 'X':
			switch(LenMod)
			{
			case 1: Type = EConsoleTriggerType::UShort; break;
			case 2: Type = EConsoleTriggerType::ULong; break;
			case 3: Type = EConsoleTriggerType::ULongLong; break;
			case 4: Type = EConsoleTriggerType::SizeT; break;
			default: Type = EConsoleTriggerType::UInt; break;
			}
			pRegex = "([0-9a-fA-F]+)";
			Base = 16;
			break;
		case 'o':
			switch(LenMod)
			{
			case 1: Type = EConsoleTriggerType::UShort; break;
			case 2: Type = EConsoleTriggerType::ULong; break;
			case 3: Type = EConsoleTriggerType::ULongLong; break;
			case 4: Type = EConsoleTriggerType::SizeT; break;
			default: Type = EConsoleTriggerType::UInt; break;
			}
			pRegex = "([0-7]+)";
			Base = 8;
			break;
		case 'f':
		case 'e':
		case 'E':
		case 'g':
		case 'G':
			switch(LenMod)
			{
			case 2:
			case 3: Type = EConsoleTriggerType::Double; break;
			case 5: Type = EConsoleTriggerType::LongDouble; break;
			default: Type = EConsoleTriggerType::Float; break;
			}
			pRegex = "(-?[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?)";
			break;
		case '%':
			Escaped += '%';
			p = pConv + 1;
			continue;
		default:
			// 未知转换符：% 按字面量，回退重新处理后续字符
			Escaped += '%';
			p++;
			continue;
		}

		Escaped += '(';
		Escaped += pRegex;
		Escaped += ')';
		m_vConsoleTriggerFormats.push_back(Type);
		m_vConsoleTriggerBases.push_back(Base);
		p = pConv + 1;
	}

	try
	{
		m_ConsoleTriggerRegex.assign(Escaped);
	}
	catch(const std::regex_error &)
	{
		return false;
	}
	m_ConsoleTriggerPatternCache = pPattern;
	return true;
}

// 匹配当前消息（OnMessage 记录的类别与文本）：类别一致且模式命中时返回 true，
// 捕获值按模式中格式符的顺序写入变参（%s/%c 对应 char*，%d/%u/%x/%o 对应整数指针，%f/%lf 对应浮点指针，调用方需保证缓冲足够）
bool CAutoFish::ConsoleTriggerCheck(const char *pCategory, const char *pPattern, ...)
{
	if(str_comp(m_CurrentChatCategory.c_str(), pCategory) != 0)
		return false;
	if(m_CurrentChatText.empty())
		return false;
	if(m_ConsoleTriggerPatternCache != pPattern && !ConsoleTriggerBuildRegex(pPattern))
		return false;

	std::cmatch Match;
	if(!std::regex_search(m_CurrentChatText.c_str(), Match, m_ConsoleTriggerRegex))
		return false;

	va_list ap;
	va_start(ap, pPattern);
	for(size_t i = 0; i < m_vConsoleTriggerFormats.size() && i + 1 < Match.size(); i++)
	{
		const std::string &Captured = Match[i + 1].str();
		switch(m_vConsoleTriggerFormats[i])
		{
		case EConsoleTriggerType::String:
		{
			char *pOut = va_arg(ap, char *);
			str_copy(pOut, Captured.c_str(), (int)(Captured.size() + 1));
			break;
		}
		case EConsoleTriggerType::Char:
		{
			char *pOut = va_arg(ap, char *);
			pOut[0] = Captured[0];
			break;
		}
		case EConsoleTriggerType::Short:
		{
			short *pOut = va_arg(ap, short *);
			*pOut = (short)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::Int:
		{
			int *pOut = va_arg(ap, int *);
			*pOut = (int)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::Long:
		{
			long *pOut = va_arg(ap, long *);
			*pOut = (long)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::LongLong:
		{
			long long *pOut = va_arg(ap, long long *);
			*pOut = (long long)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::UShort:
		{
			unsigned short *pOut = va_arg(ap, unsigned short *);
			*pOut = (unsigned short)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::UInt:
		{
			unsigned int *pOut = va_arg(ap, unsigned int *);
			*pOut = (unsigned int)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::ULong:
		{
			unsigned long *pOut = va_arg(ap, unsigned long *);
			*pOut = (unsigned long)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::ULongLong:
		{
			unsigned long long *pOut = va_arg(ap, unsigned long long *);
			*pOut = (unsigned long long)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::SizeT:
		{
			size_t *pOut = va_arg(ap, size_t *);
			*pOut = (size_t)str_toint64_base(Captured.c_str(), m_vConsoleTriggerBases[i]);
			break;
		}
		case EConsoleTriggerType::Float:
		{
			float *pOut = va_arg(ap, float *);
			*pOut = str_tofloat(Captured.c_str());
			break;
		}
		case EConsoleTriggerType::Double:
		{
			double *pOut = va_arg(ap, double *);
			*pOut = strtod(Captured.c_str(), nullptr);
			break;
		}
		case EConsoleTriggerType::LongDouble:
		{
			long double *pOut = va_arg(ap, long double *);
			*pOut = strtold(Captured.c_str(), nullptr);
			break;
		}
		}
	}
	va_end(ap);
	return true;
}
