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

// Auto fish 功能组件：自动攻击、自动购买鱼饵、光标锁定、控制台信息检测
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
	// 自动购买鱼饵（rc_auto_buy_bait，由控制台消息驱动，见 OnMessage 规则 3/4）
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
	int64_t m_DebugLaserNextPrintTime;
	// 左键模拟与自动钓鱼控制（rc_auto_fishing）
	bool m_FireInjected = false;
	bool m_FireRepressPending = false; // FireRepress 进行中（松开保持阶段）
	int64_t m_FireRepressReleaseUntil = 0; // 松开保持到的时刻（须覆盖服务器 40ms tick）
	void FireHold();
	void FireRelease();
	void FireRepress();
	void UpdateAutoFishing();
	// 自动钓鱼状态机（控制台消息驱动）
	bool m_FishingActive = false; // 鱼上钩后激活收线控制
	bool m_CastActive = false; // 出钩后等待"已抛竿"，超时每秒重试
	int64_t m_CastNextTime = 0;
	int m_CastRetryCount = 0; // 当前出钩周期内的重试次数（成功抛竿后清零）
	int64_t m_TotalFishCoins = 0; // 累计钓获币值
	void CastRod();
	void BuyBaitOnce();
	void CheckAbnormalStop();
};

#endif
