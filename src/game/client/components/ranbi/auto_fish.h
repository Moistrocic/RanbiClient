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
	void ResetFishing();
	// 自动钓鱼控制
	bool m_LastAutoFishingActive = false;

	// 自动钓鱼状态机：空闲（自动抛竿/重试）→ 出杆等待（等鱼上钩）→ 收杆处理（收线控制）
	enum class EFishState : char
	{
		Locked,
		Idle,
		CastWait,
		Reel,
	};
	EFishState m_FishState = EFishState::Locked;
	void SetFishState(EFishState NewState, const char *pReason, bool Normal);
	static const char *FishStateName(EFishState State);

	void EchoFish(const char *pFormat, ...);
	void LogFishError(const char *pFormat, ...);

	// 光标锁定（rc_lock_aim）
	bool m_LockAimActive = false;
	vec2 m_LockAimTarget;
	float m_LockAimSavedZoom;

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

	// 数据记录
	struct SAutoFishData
	{
		int64_t m_WorkingStartTime = 0; // 当前运行段的开始时刻（锁定/未运行时为 0）
		int64_t m_WorkTotal = 0; // 累计运行时长（tick，不含锁定期间）
		int m_Count = 0;
		int64_t m_Price = 0;
		int m_AbnormalCount = 0;

	} m_FishData;

	// 左键模拟
	int64_t m_FireHoldStartTime = 0;
	int64_t m_FireHoldDuration = 0;
	void FireHold();
	void FireHold(int64_t druation_ms, int64_t start_delay_ms);
	void FireRelease();
	// 自动控制钓鱼进度条
	void ControlProgress();
	// 购买鱼饵
	int64_t m_BaitBuyDelayUntil = 0;
	void BuyBaitOnce();

	// 异常状态检查
	int64_t m_StateChangeTime = 0;
	int64_t m_AbnormalStartTime = 0; // 准星超距持续计时（持续 0.5 秒才判定异常）
	int64_t m_CastNextTime = 0;
	int64_t m_CastRetryCount = 0;
	void CheckAbnormalStop();

	// 控制台信息检测（OnMessage 记录当前消息，ConsoleTriggerCheck 按类别+模式匹配并返回捕获值，仿 scanf 格式符）
	enum class EConsoleTriggerType : char
	{
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
};

#endif
