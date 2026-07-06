#ifndef GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H
#define GAME_CLIENT_COMPONENTS_RANBI_RANBI_CLIENT_H

#include <game/client/component.h>

class CRanbiClient : public CComponent
{
public:
	CRanbiClient();
	int Sizeof() const override { return sizeof(*this); }
};

#endif
