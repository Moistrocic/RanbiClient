#include "auto_fish.h"

#include <base/dbg.h>
#include <base/system.h>

#include <engine/storage.h>

#include <game/client/components/ranbi/render_proxy.h>
#include <game/client/gameclient.h>

#include <stdarg.h>
#include <stdlib.h>

CAutoFish::CAutoFish()
{
	OnReset();
}

void CAutoFish::OnReset()
{
	m_LastAutoFishingActive = false;
	ResetFishing();
}

void CAutoFish::ResetFishing()
{
	// 光标锁定
	m_LockAimActive = false;

	// 自动钓鱼状态机与统计
	m_FishState = EFishState::Locked;
	m_Targets = SAutoFishTargets{};
	m_FishData = SAutoFishData{};
	m_BaitBuyDelayUntil = 0;

	// 左键控制
	m_FireHoldStartTime = 0;
	m_FireHoldDuration = 0;

	// 异常处理
	m_StateChangeTime = 0;
	m_AbnormalStartTime = 0;
	m_CastNextTime = 0;
	m_CastRetryCount = 0;
}

// 状态名（用于 echo 与屏幕显示）
const char *CAutoFish::FishStateName(EFishState State)
{
	switch(State)
	{
	case EFishState::Locked:
		return "锁定";
	case EFishState::Idle:
		return "空闲";
	case EFishState::CastWait:
		return "抛竿";
	case EFishState::Reel:
		return "收竿";
	}
	return "未知";
}

// 通过游戏内聊天窗口（echo）向玩家呈现自动钓鱼信息
void CAutoFish::EchoFish(const char *pFormat, ...)
{
	char aBuf[512];
	va_list ap;
	va_start(ap, pFormat);
	str_format_v(aBuf, sizeof(aBuf), pFormat, ap);
	va_end(ap);

	char aLine[576];
	str_format(aLine, sizeof(aLine), "[自动钓鱼] %s", aBuf);
	GameClient()->m_Chat.Echo(aLine);
}

// 异常日志：带时间戳追加写入用户配置目录 ranbi/auto_fish_error/error.log
void CAutoFish::LogFishError(const char *pFormat, ...)
{
	Storage()->CreateFolder("ranbi", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ranbi/auto_fish_error", IStorage::TYPE_SAVE);

	char aContent[512];
	va_list ap;
	va_start(ap, pFormat);
	str_format_v(aContent, sizeof(aContent), pFormat, ap);
	va_end(ap);

	char aTime[64];
	str_timestamp_format(aTime, sizeof(aTime), TimestampFormat::SPACE);

	char aLine[640];
	str_format(aLine, sizeof(aLine), "[%s] %s\n", aTime, aContent);

	IOHANDLE File = Storage()->OpenFile("ranbi/auto_fish_error/error.log", IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;
	io_write(File, aLine, str_length(aLine));
	io_close(File);
}

// 状态切换：只更新当前状态与状态变化时刻，具体逻辑由 OnUpdate 按状态和时间执行
void CAutoFish::SetFishState(EFishState NewState, const char *pReason, bool Normal)
{
	if(m_FishState == NewState)
		return;
	const EFishState OldState = m_FishState;
	m_FishState = NewState;
	m_StateChangeTime = time_get();

	// 运行计时：仅累计非锁定（运行中）状态的时长，锁定（未运行）期间不计时
	if(NewState == EFishState::Locked)
	{
		if(m_FishData.m_WorkingStartTime != 0)
		{
			m_FishData.m_WorkTotal += time_get() - m_FishData.m_WorkingStartTime;
			m_FishData.m_WorkingStartTime = 0;
		}
	}
	else if(m_FishData.m_WorkingStartTime == 0)
		m_FishData.m_WorkingStartTime = time_get();

	EchoFish("状态：%s → %s（%s，%s）", FishStateName(OldState), FishStateName(NewState), Normal ? "正常" : "异常", pReason);
}

void CAutoFish::OnUpdate()
{
	// AUTO FISH m_RcLockAim：锁定光标瞄准的地图位置
	// 实测公式：地图距离 = 光标距离(屏幕单位) × zoom；视觉格数 = 光标距离 × zoom / 32
	// 开启时由玩家坐标+光标推算锁定方块；玩家移动后反推光标角度与光标距离；
	// 光标距离受限(最大鼠标距离)时保持角度；脱离视野时放大视野保证可见，进入视野时恢复
	// 状态同步：配置开启时记录玩家坐标，推算光标地图坐标并锁定到方块中心
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	const bool IsPlayerActive = LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_Active &&
				    !GameClient()->m_Snap.m_SpecInfo.m_Active;
	if(g_Config.m_RcLockAim && IsPlayerActive)
	{
		if(!m_LockAimActive)
		{
			// 初始坐标获取
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
		else
		{
			// 开始锁定
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
	else if(!g_Config.m_RcLockAim && m_LockAimActive)
	{
		GameClient()->m_Camera.m_UserZoomTarget = m_LockAimSavedZoom;
		m_LockAimActive = false;
	}

	// 自动钓鱼开关
	if(!g_Config.m_RcAutoFishing)
	{
		if(m_LastAutoFishingActive)
			m_LastAutoFishingActive = false;
		return;
	}
	// 自动钓鱼初始化

	if(!m_LastAutoFishingActive)
	{
		m_LastAutoFishingActive = true;
		ResetFishing();
		SetFishState(EFishState::Locked, "自动钓鱼开启，初始进入锁定状态，手动抛竿开始", true);
	}

	// 左键控制
	if(m_FishState != EFishState::Locked && m_FishState != EFishState::Reel)
	{
		if(time_get() > m_FireHoldStartTime && time_get() < m_FireHoldStartTime + m_FireHoldDuration)
			FireHold();
		else
			FireRelease();
	}

	switch(m_FishState)
	{
	case EFishState::Locked:
	{
		// 不做任何事
		break;
	}
	case EFishState::Idle:
	{
		// 持续等待 1 分钟无变化则打破死循环：重置空闲状态
		if(time_get() - m_StateChangeTime > time_freq() * 60)
		{
			m_FishData.m_AbnormalCount++;
			LogFishError("空闲状态超过 1 分钟无变化，重置空闲状态");
			SetFishState(EFishState::Idle, "等待 1 分钟无变化", false);
			break;
		}
		if(m_CastRetryCount == 0)
		{
			EchoFish("出杆");
			FireRelease();
			FireHold(920, 80);
			m_CastNextTime = time_get() + time_freq() * 1;
			m_CastRetryCount++;
			break;
		}
		if(time_get() < m_CastNextTime)
			break;
		if(m_CastRetryCount < 7200LL)
		{
			EchoFish("出杆重试（%lld 次）", m_CastRetryCount);
			if(m_CastRetryCount % 5 == 0)
				LogFishError("出杆重试（%lld 次）", m_CastRetryCount);
			if(g_Config.m_RcAutoBuyBait)
			{
				m_BaitBuyDelayUntil = 0;
				BuyBaitOnce();
			}
			FireRelease();
			FireHold(920, 80);
			m_CastNextTime = time_get() + time_freq() * 1;
			m_CastRetryCount++;
		}
		else
		{
			LogFishError("严重错误：出杆重试 7200 次依旧未成功！");
			SetFishState(EFishState::Locked, "严重错误：出杆重试 7200 次依旧未成功！", false);
		}
		break;
	}
	case EFishState::CastWait:
	{
		// 状态更新初始化
		if(m_CastRetryCount > 0)
		{
			m_BaitBuyDelayUntil = time_get() + time_freq() * 2;
			m_CastNextTime = 0;
			m_CastRetryCount = 0;
		}

		// 持续等待 1 分钟无变化则打破死循环：回到空闲重新执行流程
		if(time_get() - m_StateChangeTime > time_freq() * 60)
		{
			m_FishData.m_AbnormalCount++;
			LogFishError("出杆等待超过 1 分钟无变化，回到空闲重新执行流程");
			SetFishState(EFishState::Idle, "等待 1 分钟无变化", false);
			break;
		}
		// 进入出杆等待 2 秒后购买一次鱼饵
		if(m_BaitBuyDelayUntil != 0 && time_get() >= m_BaitBuyDelayUntil)
		{
			m_BaitBuyDelayUntil = 0;
			if(g_Config.m_RcAutoBuyBait)
				BuyBaitOnce();
		}
		break;
	}
	case EFishState::Reel:
	{
		// 持续等待 1 分钟无变化则打破死循环：回到空闲重新执行流程
		if(time_get() - m_StateChangeTime > time_freq() * 60)
		{
			m_FishData.m_AbnormalCount++;
			LogFishError("收杆处理超过 1 分钟无变化，回到空闲重新执行流程");
			SetFishState(EFishState::Idle, "等待 1 分钟无变化", false);
			break;
		}
		// 收杆处理：激活钓鱼区域检测 + 收线控制
		UpdateRegionTargets();
		ControlProgress();
		break;
	}
	}

	// 异常状态检查（玩家与准星距离超 20 格时进入空闲锁定状态）
	CheckAbnormalStop();
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
			// 注：pickup 为点实体，无方向/长度属性，无法做垂直判断（钓鱼进度实体，依赖区域过滤）
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
			// 解冻激光必须垂直（x 相同）且 y 方向长度至少 1 格，排除非垂直干扰
			if(std::fabs(To.x - From.x) > 1.0f || std::fabs(To.y - From.y) < 32.0f)
				break;
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
				// 霰弹枪激光必须垂直（x 相同）且 y 方向长度至少 1 格，排除非垂直干扰
				if(std::fabs(To.x - From.x) > 1.0f || std::fabs(To.y - From.y) < 32.0f)
					break;
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
}

void CAutoFish::FireHold(int64_t duration_ms, int64_t start_delay_ms)
{
	m_FireHoldStartTime = time_get() + start_delay_ms * time_freq() / 1000;
	m_FireHoldDuration = duration_ms * time_freq() / 1000;
}

// 2. 模拟左键松开
void CAutoFish::FireRelease()
{
	GameClient()->m_Controls.m_aInputData[g_Config.m_ClDummy].m_Fire = 0;
}

// 收杆处理：用左键模拟把武士刀稳定在解冻激光与霰弹枪激光的中间位置
// 按住左键 → 武士刀向右；松开 → 向左回落。允许小幅度波动，边界强制回拉
void CAutoFish::ControlProgress()
{
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
		FireHold(1000, 0);
		return;
	}
	if(m_Targets.m_KatanaX >= MaxX - BoundaryMargin)
	{
		FireRelease();
		return;
	}

	// 中间控制：偏左按住向右拉，偏右松开向左回落，死区内保持当前状态
	if(m_Targets.m_KatanaX < Mid - Tolerance)
		FireHold(1000, 0);
	else if(m_Targets.m_KatanaX > Mid + Tolerance)
		FireRelease();
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
			EchoFish("购买鱼饵：已触发投票选项 %d", OptionId);
			break;
		}
	}
	if(!Found)
		EchoFish("购买鱼饵：未找到购买鱼饵的投票选项（共 %d 个）", OptionId);
}

// 异常状态检查：玩家与准星在地图坐标距离超过 20 格时进入空闲锁定状态，等待手动抛竿解锁
void CAutoFish::CheckAbnormalStop()
{
	if(!g_Config.m_RcAutoFishStopOutOfRange || !g_Config.m_RcAutoFishing || m_FishState == EFishState::Locked)
		return;
	// 旁观状态不执行检测
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
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
		// 持续超距 0.5 秒才判定异常（避免旁观切换/光标残留等瞬时误判）
		if(m_AbnormalStartTime == 0)
			m_AbnormalStartTime = time_get();
		else if(time_get() - m_AbnormalStartTime > time_freq() / 2)
		{
			// 不关闭自动钓鱼，进入空闲锁定状态：等待玩家手动抛竿后解锁
			m_AbnormalStartTime = 0;
			FireRelease();
			m_FishData.m_AbnormalCount++;
			SetFishState(EFishState::Locked, "准星超距", false);
			EchoFish("准星超距（%.1f 格 > 20 格）持续 0.5 秒，已锁定，等待手动抛竿后解锁", PlayerCursorDist / 32.0f);
			LogFishError("准星超距（%.1f 格 > 20 格）持续 0.5 秒，功能已锁定，等待手动抛竿解锁", PlayerCursorDist / 32.0f);
		}
	}
	else
	{
		m_AbnormalStartTime = 0;
	}
}

// 信息渲染
void CAutoFish::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(!g_Config.m_RcAutoFishing) // 仅在自动钓鱼开启时显示
		return;

	// 保存当前屏幕映射（此时为世界/相机坐标系），绘制完 UI 后还原，避免污染后续组件（名字板/冰冻条/伤害指示等）
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);

	const float ScreenW = Graphics()->ScreenWidth();
	const float ScreenH = Graphics()->ScreenHeight();
	Graphics()->MapScreen(0, 0, ScreenW, ScreenH);

	char aStatus[64];
	ColorRGBA StatusColor;
	bool IsLocked = m_FishState == EFishState::Locked;
	str_format(aStatus, sizeof(aStatus), "%s（%s）", IsLocked ? "未运行" : "运行中", FishStateName(m_FishState));
	if(IsLocked)
		StatusColor = ColorRGBA(1.0f, 0.4f, 0.3f, 1.0f);
	else
		StatusColor = ColorRGBA(0.4f, 1.0f, 0.4f, 1.0f);

	const int64_t WorkTicks = m_FishData.m_WorkTotal + (m_FishData.m_WorkingStartTime != 0 ? time_get() - m_FishData.m_WorkingStartTime : 0);
	const int64_t WorkSec = WorkTicks / time_freq();
	const int AvgPerFish = m_FishData.m_Count > 0 ? (int)(m_FishData.m_Price / m_FishData.m_Count) : 0;
	const int IncomePerMin = m_FishData.m_Count > 0 && WorkSec > 0 ? (int)(m_FishData.m_Price * 60 / WorkSec) : 0;

	char aTime[64];
	str_format(aTime, sizeof(aTime), "%d分%d秒", (int)(WorkSec / 60), (int)(WorkSec % 60));

	char aLines[6][128];
	str_format(aLines[0], sizeof(aLines[0]), "自动钓鱼：%s", aStatus);
	str_format(aLines[1], sizeof(aLines[1]), "鱼获：%d 条，累计 %lld 币", m_FishData.m_Count, m_FishData.m_Price);
	str_format(aLines[2], sizeof(aLines[2]), "时长：%s", aTime);
	str_format(aLines[3], sizeof(aLines[3]), "均价：%d 币/条", AvgPerFish);
	str_format(aLines[4], sizeof(aLines[4]), "收入：%d 币/分", IncomePerMin);
	str_format(aLines[5], sizeof(aLines[5]), "异常：%d 次", m_FishData.m_AbnormalCount);

	constexpr float FontSize = 36.0f;
	constexpr float LineHeight = 48.0f;
	constexpr float Padding = 18.0f;
	float MaxW = 0.0f;
	for(int i = 0; i < 6; i++)
		MaxW = maximum(MaxW, TextRender()->TextWidth(FontSize, aLines[i]));
	const float BoxW = MaxW + Padding * 2;
	const float BoxH = LineHeight * 6 + Padding * 2;
	const float X = ScreenW - BoxW - 10.0f;
	const float Y = ScreenH - BoxH - 10.0f;

	Graphics()->TextureClear();
	Graphics()->DrawRect(X, Y, BoxW, BoxH, ColorRGBA(0.0f, 0.0f, 0.0f, 0.5f), IGraphics::CORNER_ALL, 5.0f);

	float TextY = Y + Padding;
	TextRender()->TextColor(StatusColor);
	TextRender()->Text(X + Padding, TextY, FontSize, aLines[0]);
	TextY += LineHeight;
	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	for(int i = 1; i < 6; i++)
	{
		TextRender()->Text(X + Padding, TextY, FontSize, aLines[i]);
		TextY += LineHeight;
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	// 还原屏幕映射，保证后续组件在世界/相机坐标系下正常渲染
	Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

// 控制台消息监控：NETMSGTYPE_SV_CHAT 的类别映射与控制台输出（chat.cpp FChatMsgCheckAndPrint）保持一致
void CAutoFish::OnMessage(int Msg, void *pRawMsg)
{
	// 自动钓鱼开关关闭时不激活流程
	if(!g_Config.m_RcAutoFishing)
		return;

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

	// 状态更新：仅更新当前状态与状态变化时刻，具体逻辑由 OnUpdate 按状态和时间执行
	// 已抛竿 → 出杆等待（锁定状态下玩家手动抛竿自动解锁）
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 已抛竿"))
	{
		const bool IsLocked = m_FishState == EFishState::Locked;
		SetFishState(EFishState::CastWait, IsLocked ? "已手动抛竿" : "已自动抛竿", true);
	}
	// 鱼上钩了 → 收杆处理
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 鱼上钩了！"))
	{
		SetFishState(EFishState::Reel, "鱼上钩了", true);
	}
	// 钓到鱼 → 空闲（累计统计并展示鱼种/价格/累计价值）
	{
		char aFish[64];
		int Price = 0;
		if(ConsoleTriggerCheck("chat/server", "[钓鱼] 钓到%s x1，价值 %d 币", aFish, &Price))
		{
			m_FishData.m_Count++;
			m_FishData.m_Price += Price;
			SetFishState(EFishState::Idle, "钓到鱼了", true);
			EchoFish("钓到 %s x1，价值 %d 币，累计 %d 条 %lld 币", aFish, Price, m_FishData.m_Count, m_FishData.m_Price);
		}
	}
	// 鱼脱钩逃跑 / 收竿太慢 → 空闲
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 张力进入红色区域，鱼脱钩逃跑了。"))
	{
		m_FishData.m_AbnormalCount++;
		LogFishError("鱼脱钩逃跑，钓鱼失败一次，不影响功能正常运行");
		SetFishState(EFishState::Idle, "鱼脱钩逃跑", true);
	}
	else if(ConsoleTriggerCheck("chat/server", "[钓鱼] 收竿太慢，鱼跑掉了"))
	{
		m_FishData.m_AbnormalCount++;
		LogFishError("收竿太慢，钓鱼失败一次，不影响功能正常运行");
		SetFishState(EFishState::Idle, "收竿太慢", true);
	}

	// 玩家主动取消抛竿 → 空闲并锁定，等待玩家手动抛竿解锁
	if(ConsoleTriggerCheck("chat/server", "[钓鱼] 已取消抛竿"))
	{
		SetFishState(EFishState::Locked, "已取消抛竿", true);
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
