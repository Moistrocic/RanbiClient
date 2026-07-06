#ifndef GAME_CLIENT_COMPONENTS_RANBI_RENDER_PROXY_H
#define GAME_CLIENT_COMPONENTS_RANBI_RENDER_PROXY_H

#include <game/client/component.h>

#include <functional>
#include <vector>

enum class RenderProxyTrigger
{
	OutlinesBefore = 0,
	OutlinesAfter,
	NamePlatesBefore,
	NamePlatesAfter,
	HudBefore,
	HudAfter,
};

class CRenderProxy : public CComponent
{
private:
	class CRenderProxyContainer
	{
	private:
		std::function<void()> m_RenderFunc;
		RenderProxyTrigger m_ProxyTrigger;
		int m_RenderOrder;

	public:
		CRenderProxyContainer(std::function<void()> Func, RenderProxyTrigger Trigger, int Order) :
			m_RenderFunc(std::move(Func)), m_ProxyTrigger(Trigger), m_RenderOrder(Order) {}

		RenderProxyTrigger GetProxyTrigger() const { return m_ProxyTrigger; }
		int GetRenderOrder() const { return m_RenderOrder; }
		void OnRender() const { m_RenderFunc(); }
	};

	std::vector<CRenderProxyContainer> m_vProxies;

public:
	int Sizeof() const override { return sizeof(*this); }

	void Clear();

	void AddProxy(std::function<void()> Func, RenderProxyTrigger Trigger, int Order = -1);
	void ProxyTrigger(RenderProxyTrigger Trigger);
	void OnRender() override;
};

#endif
