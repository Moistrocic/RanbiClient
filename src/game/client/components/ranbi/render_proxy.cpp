#include "render_proxy.h"

#include <algorithm>

void CRenderProxy::Clear()
{
	m_vProxies.clear();
}

void CRenderProxy::AddProxy(std::function<void()> Func, RenderProxyTrigger Trigger, int Order)
{
	if(Order <= -1)
	{
		for(const auto &Proxy : m_vProxies)
		{
			if(Proxy.GetProxyTrigger() == Trigger)
				Order = Proxy.GetRenderOrder() + 1;
			else if(Order > -1)
				break;
		}
	}
	if(Order <= -1)
		Order = 0;
	m_vProxies.emplace_back(std::move(Func), Trigger, Order);
}

void CRenderProxy::ProxyTrigger(RenderProxyTrigger Trigger)
{
	std::stable_sort(m_vProxies.begin(), m_vProxies.end(),
		[](const CRenderProxyContainer &A, const CRenderProxyContainer &B) {
			return A.GetRenderOrder() < B.GetRenderOrder();
		});

	for(const auto &Proxy : m_vProxies)
	{
		if(Proxy.GetProxyTrigger() == Trigger)
			Proxy.OnRender();
	}
}

void CRenderProxy::OnRender()
{
	Clear();
}
