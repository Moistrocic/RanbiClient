#ifndef GAME_CLIENT_COMPONENTS_RANBI_POINTS_H
#define GAME_CLIENT_COMPONENTS_RANBI_POINTS_H

#include <engine/shared/http.h>
#include <engine/shared/json.h>

#include <game/client/component.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class CPoints : public CComponent
{
	std::vector<std::pair<std::string, std::shared_ptr<CHttpRequest>>> m_vPointsRequests;
	std::unordered_map<std::string, int> m_PointsCache;
	std::mutex m_PointsRequestsMutex;
	std::mutex m_PointsMutex;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;
	void ClearPoints();

	void AddRequest(const std::string &Name, const std::shared_ptr<CHttpRequest> &Request);
	void FinishRequest(const std::string &Name);

	int GetPoints(const std::string &Name);
	void RemovePoints(const std::string &Name);

private:
	std::string EncodePlayerName(const std::string &Name);
	void ParseJson(const json_value *pObj, const std::string &TargetName);
	void Run(const std::string &Name, std::shared_ptr<CHttpRequest> pPointsRequest);
};

#endif
