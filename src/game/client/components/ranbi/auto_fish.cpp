#include "auto_fish.h"

#include <base/dbg.h>
#include <base/system.h>

#include <game/client/gameclient.h>

#include <stdarg.h>
#include <stdlib.h>

CAutoFish::CAutoFish()
{
	OnReset();
}

void CAutoFish::OnReset()
{
	m_LockAimActive = false;
	m_LockAimTarget = vec2(0, 0);
	m_LockAimSavedZoom = 1.0f;

	m_DebugLaserNextPrintTime = 0;
}

void CAutoFish::OnUpdate()
{
	// FireRepress：先松开并保持足够时长（覆盖服务器 40ms tick），再按下（FireHold）
	if(m_FireRepressPending)
	{
		if(time_get() < m_FireRepressReleaseUntil)
			FireRelease(); // 松开保持中
		else
		{
			m_FireRepressPending = false;
			FireHold(); // 松开足够时长后按下，产生可靠的 0->1 边沿
		}
	}

	// AUTO FISH m_RcAutoBuyBait：不再定时轮询，改为由控制台消息驱动（规则 3/4，见 OnMessage）
	// 菜单开关 rc_auto_buy_bait 作为消息驱动购买的开关

	// 检测玩家上方 15x7 区域内的钓鱼目标（武士刀/解冻激光/霰弹枪激光）
	UpdateRegionTargets();

	// 自动钓鱼控制（用左键模拟把武士刀稳定在解冻激光与霰弹枪激光中间）
	UpdateAutoFishing();

	// 异常状态检查（玩家与准星距离超 20 格时自动终止）
	CheckAbnormalStop();

	// 出钩重试：出钩后未收到"已抛竿"确认，每秒重新出钩（放末尾避免被收线注入覆盖）
	if(m_CastActive && time_get() >= m_CastNextTime)
	{
		m_CastRetryCount++;
		// 成功抛竿前重试超过 10 次：判为异常行为，自动关闭自动钓鱼（受异常终止开关控制）
		if(m_CastRetryCount > 10 && g_Config.m_RcAutoFishStopOutOfRange)
		{
			g_Config.m_RcAutoFishing = 0;
			m_FishingActive = false;
			m_CastActive = false;
			FireRelease();
			dbg_msg("ranbi/autofish", "cast retry >10 times without success, auto fishing stopped");
			return;
		}
		// 重试前同样补一次鱼饵（开启自动买饵时）
		if(g_Config.m_RcAutoBuyBait)
			BuyBaitOnce();
		dbg_msg("ranbi/autofish", "cast retry");
		FireRepress();
		m_CastNextTime = time_get() + time_freq();
	}
}

// 检测玩家上方 15x7 区域内的钓鱼目标：武士刀（POWERUP_NINJA pickup）、解冻激光（旧 LASER）、霰弹枪激光（SHOTGUN）
void CAutoFish::UpdateRegionTargets()
{
	m_Targets = SAutoFishTargets{};

	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_aClients[LocalId].m_Active)
		return;

	const vec2 PlayerPos = GameClient()->m_LocalCharacterPos;
	const int Tx = (int)std::floor(PlayerPos.x / 32.0f);
	const int Ty = (int)std::floor(PlayerPos.y / 32.0f);
	const float MinX = (Tx - 7) * 32.0f;
	const float MaxX = (Tx + 8) * 32.0f;
	const float MinY = (Ty - 7) * 32.0f;
	const float MaxY = Ty * 32.0f;

	const int Num = Client()->SnapNumItems(IClient::SNAP_CURRENT);
	for(int i = 0; i < Num; i++)
	{
		const IClient::CSnapItem Item = Client()->SnapGetItem(IClient::SNAP_CURRENT, i);
		switch(Item.m_Type)
		{
		case NETOBJTYPE_PICKUP:
		case NETOBJTYPE_DDNETPICKUP:
		{
			const CNetObj_Pickup *pPickup = (const CNetObj_Pickup *)Item.m_pData;
			if(pPickup->m_Type != POWERUP_NINJA)
				break;
			const vec2 Pos(pPickup->m_X, pPickup->m_Y);
			if(Pos.x < MinX || Pos.x >= MaxX || Pos.y < MinY || Pos.y >= MaxY)
				break;
			m_Targets.m_KatanaX = Pos.x;
			m_Targets.m_HasKatana = true;
			break;
		}
		case NETOBJTYPE_LASER:
		{
			const CNetObj_Laser *pLaser = (const CNetObj_Laser *)Item.m_pData;
			const vec2 From(pLaser->m_FromX, pLaser->m_FromY);
			const vec2 To(pLaser->m_X, pLaser->m_Y);
			const bool InFrom = From.x >= MinX && From.x < MaxX && From.y >= MinY && From.y < MaxY;
			const bool InTo = To.x >= MinX && To.x < MaxX && To.y >= MinY && To.y < MaxY;
			if(!InFrom && !InTo)
				break;
			m_Targets.m_UnfreezeX = To.x;
			m_Targets.m_HasUnfreeze = true;
			break;
		}
		case NETOBJTYPE_DDNETLASER:
		{
			const CNetObj_DDNetLaser *pLaser = (const CNetObj_DDNetLaser *)Item.m_pData;
			// 过滤其他玩家的激光：钓鱼进度激光为服务器实体（owner=-1）或自己的激光，
			// 其他玩家发射的激光带其玩家 ID（owner>=0 且非本地）
			if(pLaser->m_Owner >= 0 && pLaser->m_Owner != LocalId)
				break;
			const vec2 From(pLaser->m_FromX, pLaser->m_FromY);
			const vec2 To(pLaser->m_ToX, pLaser->m_ToY);
			const bool InFrom = From.x >= MinX && From.x < MaxX && From.y >= MinY && From.y < MaxY;
			const bool InTo = To.x >= MinX && To.x < MaxX && To.y >= MinY && To.y < MaxY;
			if(!InFrom && !InTo)
				break;
			if(pLaser->m_Type == LASERTYPE_SHOTGUN)
			{
				m_Targets.m_ShotgunX = To.x;
				m_Targets.m_HasShotgun = true;
			}
			else if(pLaser->m_Type == LASERTYPE_FREEZE)
			{
				// 体力条：水平激光，to.x 随收线/张力变化
				m_Targets.m_StaminaFromX = From.x;
				m_Targets.m_StaminaToX = To.x;
				m_Targets.m_HasStamina = true;
			}
			break;
		}
		}
	}
	m_Targets.m_Valid = true;
}

// 1. 模拟左键按住（持续，不能松开）
void CAutoFish::FireHold()
{
	GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire = 1;
	m_FireInjected = true;
}

// 2. 模拟左键松开
void CAutoFish::FireRelease()
{
	GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire = 0;
	m_FireInjected = false;
}

// 3. 模拟重新按住左键：先执行 FireRelease（松开并保持 80ms，覆盖服务器 40ms tick），再执行 FireHold（按下并保持）
void CAutoFish::FireRepress()
{
	FireRelease();
	m_FireRepressPending = true;
	m_FireRepressReleaseUntil = time_get() + time_freq() * 80 / 1000;
}

// 自动钓鱼：用左键模拟把武士刀稳定在解冻激光与霰弹枪激光的中间位置
// 按住左键 → 武士刀向右；松开 → 向左回落。允许小幅度波动，边界强制回拉
void CAutoFish::UpdateAutoFishing()
{
	if(!g_Config.m_RcAutoFishing)
		return;
	if(!m_FishingActive) // 收到"鱼上钩了"消息后才激活收线控制
		return;
	if(!m_Targets.m_Valid || !m_Targets.m_HasKatana || !m_Targets.m_HasUnfreeze || !m_Targets.m_HasShotgun)
		return;

	const float MinX = m_Targets.m_UnfreezeX;
	const float MaxX = m_Targets.m_ShotgunX;
	if(MaxX <= MinX)
		return;

	const float Mid = (MinX + MaxX) * 0.5f;
	constexpr float Tolerance = 8.0f; // 中间死区：允许波动范围（1/4 格）
	constexpr float BoundaryMargin = 24.0f; // 边界保护触发距离（3/4 格，吸收松开过冲与鱼挣扎）

	// 边界保护：接近边界时强制反向（不能小于解冻激光或大于霰弹枪激光）
	if(m_Targets.m_KatanaX <= MinX + BoundaryMargin)
	{
		FireHold();
		return;
	}
	if(m_Targets.m_KatanaX >= MaxX - BoundaryMargin)
	{
		FireRelease();
		return;
	}

	// 中间控制：偏左按住向右拉，偏右松开向左回落，死区内保持当前状态
	if(m_Targets.m_KatanaX < Mid - Tolerance)
		FireHold();
	else if(m_Targets.m_KatanaX > Mid + Tolerance)
		FireRelease();
}

// 出钩（抛竿）：执行重新按住左键，并激活每秒重试直到收到"已抛竿"
// 注：首次出杆不购买鱼饵（购买在出杆成功后进行，重试时补买作为保险）
void CAutoFish::CastRod()
{
	m_CastRetryCount = 0;
	FireRepress();
	m_CastActive = true;
	m_CastNextTime = time_get() + time_freq();
}

// 执行一次鱼饵购买：遍历投票选项匹配"购买鱼饵"并触发
void CAutoFish::BuyBaitOnce()
{
	int OptionId = 0;
	bool Found = false;
	for(const CVoteOptionClient *pOption = GameClient()->m_Voting.FirstOption(); pOption; pOption = pOption->m_pNext, OptionId++)
	{
		if(str_find_nocase(pOption->m_aDescription, "购买鱼饵"))
		{
			Found = true;
			GameClient()->m_Voting.CallvoteOption(OptionId, "", false);
			dbg_msg("ranbi/autofish", "buy bait: vote option %d", OptionId);
			break;
		}
	}
	if(!Found)
		dbg_msg("ranbi/autofish", "buy bait: no matching vote option (total %d)", OptionId);
}

// 异常状态检查：玩家与准星在地图坐标距离超过 20 格时自动关闭自动钓鱼
void CAutoFish::CheckAbnormalStop()
{
	if(!g_Config.m_RcAutoFishStopOutOfRange)
		return;
	if(!g_Config.m_RcAutoFishing) // 已关闭则不再检查
		return;

	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS || !GameClient()->m_aClients[LocalId].m_Active)
		return;

	// 准星世界坐标：与 m_RcLockAim 相同的换算（地图距离 = 光标距离 × zoom）
	const vec2 PlayerPos = GameClient()->m_LocalCharacterPos;
	const vec2 MousePos = GameClient()->m_Controls.m_aMousePos[Dummy];
	const float Dist = length(MousePos);
	const float Zoom = GameClient()->m_Camera.m_Zoom;
	const vec2 Dir = Dist > 0.001f ? MousePos / Dist : vec2(1.0f, 0.0f);
	const vec2 CursorPos = PlayerPos + Dir * (Dist * Zoom);
	const float PlayerCursorDist = distance(PlayerPos, CursorPos);
	if(PlayerCursorDist > 20.0f * 32.0f)
	{
		g_Config.m_RcAutoFishing = 0;
		m_FishingActive = false;
		m_CastActive = false;
		FireRelease();
		dbg_msg("ranbi/autofish", "cursor too far (%.0f units > 20 tiles), auto fishing stopped", PlayerCursorDist);
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

	// 规则 1：鱼上钩 → 激活自动钓鱼收线控制，停止出钩重试
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 鱼上钩了！"))
	{
		m_FishingActive = true;
		m_CastActive = false;
	}

	// 规则 2：钓到鱼 → 关闭收线控制、累计币值并重新出钩（续钓）
	{
		char aFish[64];
		int Price = 0;
		if(ConsoleTriggerCheck("chat/server", "[钓鱼] 钓到%s x1，价值 %d 币", aFish, &Price))
		{
			m_FishingActive = false;
			m_TotalFishCoins += Price;
			dbg_msg("ranbi/autofish", "caught '%s' +%d coins, total %lld, recasting", aFish, Price, m_TotalFishCoins);
			CastRod();
		}
	}

	// 规则 3/4/6 已移除：鱼饵购买改为出钩前执行（见 CastRod，受 rc_auto_buy_bait 开关控制）

	// 规则 5：已抛竿 → 停止出钩重试并清零重试计数；出杆成功后购买鱼饵（为下次出杆备饵）
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 已抛竿"))
	{
		m_CastActive = false;
		m_CastRetryCount = 0;
		if(g_Config.m_RcAutoBuyBait)
			BuyBaitOnce();
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
			pRegex = "(-?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)"; // 内层用非捕获组，保证只产生 1 个捕获组
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

		// pRegex 已含捕获括号，直接拼接（不可再包裹，否则产生嵌套捕获组导致 Match 索引错位）
		Escaped += pRegex;
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
