#ifndef GAME_CLIENT_COMPONENTS_RANBI_AUTO_FISH_H
#define GAME_CLIENT_COMPONENTS_RANBI_AUTO_FISH_H

#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <generated/protocol7.h>

#include <game/client/component.h>

#include <regex>
#include <string>
#include <vector>

// Auto fish 功能组件：自动抛竿、自动购买鱼饵、光标锁定、控制台信息检测
class CAutoFish : public CComponent
{
public:
	CAutoFish();
	int Sizeof() const override { return sizeof(*this); }

	void OnUpdate() override;
	void OnReset() override;
	void OnRender() override;
	void OnMessage(int Msg, void *pRawMsg) override;

private:
	// 自动钓鱼状态机：空闲（自动抛竿/重试）→ 出杆等待（等鱼上钩）→ 收杆处理（收线控制）
	enum class EFishState : char
	{
		Idle,
		CastWait,
		Reel,
	};
	EFishState m_FishState = EFishState::Idle;
	int64_t m_StateChangeTime = 0; // 最近一次状态变化时刻（用于 1 分钟无变化超时）
	bool m_Locked = false; // 出杆长期失败或玩家取消抛竿后锁定，等待玩家手动抛竿（"已抛竿"消息）解锁
	int64_t m_LockTime = 0; // 锁定时刻（仅取消抛竿时记录；锁定后 1 秒内的"已抛竿"视为取消前残留确认，忽略并保持锁定）
	bool m_LastAutoFishing = false; // 上次 rc_auto_fishing 开关状态（检测开关边沿）
	int64_t m_CastNextTime = 0; // 空闲状态的下一次出杆时刻（0 = 待首次出杆）
	int m_CastRetryCount = 0; // 普通重试计数（2 秒一次，满 5 次进入长重试）
	bool m_CastLongRetry = false; // 长重试模式（30 秒一次）
	int m_CastLongRetryCount = 0; // 长重试计数（满 5 次锁定功能）
	int64_t m_BaitBuyDelayUntil = 0; // 出杆等待状态进入 2 秒后的买饵时刻
	int64_t m_TotalFishCoins = 0; // 累计钓获币值
	int m_TotalFishCount = 0; // 累计钓获条数
	int64_t m_WorkingStartTime = 0; // 本次开启自动钓鱼的时刻（统计工作时长）
	int m_AbnormalCount = 0; // 异常次数（出杆失败锁定 + 状态 1 分钟无变化超时）
	void SetFishState(EFishState NewState, const char *pReason, bool Normal);
	void ResetFishing();
	void EchoFish(const char *pFormat, ...);
	static const char *FishStateName(EFishState State);
	void RenderStatusHud();
	// 自动购买鱼饵（rc_auto_buy_bait，由控制台消息驱动）
	// 光标锁定（rc_lock_aim）
	bool m_LockAimActive;
	vec2 m_LockAimTarget;
	float m_LockAimSavedZoom;
	// 控制台信息检测（OnMessage 记录当前消息，ConsoleTriggerCheck 按类别+模式匹配并返回捕获值，仿 scanf 格式符）
	enum class EConsoleTriggerType : char	{
		String,
		Char,
		Short,
		Int,
		Long,
		LongLong,
		UShort,
		UInt,
		ULong,
		ULongLong,
		SizeT,
		Float,
		Double,
		LongDouble,
	};
	std::string m_CurrentChatCategory;
	std::string m_CurrentChatText;
	std::regex m_ConsoleTriggerRegex;
	std::string m_ConsoleTriggerPatternCache;
	std::vector<EConsoleTriggerType> m_vConsoleTriggerFormats;
	std::vector<int> m_vConsoleTriggerBases;
	bool ConsoleTriggerCheck(const char *pCategory, const char *pPattern, ...);
	bool ConsoleTriggerBuildRegex(const char *pPattern);
	// 玩家上方 15x7 区域内的钓鱼目标检测（武士刀/解冻激光/霰弹枪激光的 x 坐标、体力条范围）
	struct SAutoFishTargets
	{
		bool m_Valid = false;
		bool m_HasKatana = false;
		float m_KatanaX = 0.0f;
		bool m_HasUnfreeze = false;
		float m_UnfreezeX = 0.0f;
		bool m_HasShotgun = false;
		float m_ShotgunX = 0.0f;
		bool m_HasStamina = false;
		float m_StaminaFromX = 0.0f;
		float m_StaminaToX = 0.0f;
	} m_Targets;
	void UpdateRegionTargets();
	// 左键模拟与自动钓鱼控制（rc_auto_fishing）
	bool m_FireInjected = false;
	bool m_FireRepressPending = false; // FireRepress 进行中（松开保持阶段）
	int64_t m_FireRepressReleaseUntil = 0; // 松开保持到的时刻（须覆盖服务器 40ms tick）
	void FireHold();
	void FireRelease();
	void FireRepress();
	void UpdateAutoFishing();
	void BuyBaitOnce();
	// 异常状态检查（准星超距）
	int64_t m_AbnormalStartTime = 0; // 准星超距持续计时（持续 0.5 秒才判定异常）
	void CheckAbnormalStop();
};

#endif
