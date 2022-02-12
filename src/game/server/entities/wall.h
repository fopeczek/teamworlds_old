/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once

#include <game/server/entity.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>

class CWall : public CEntity
{
public:
	CWall(CGameWorld *pGameWorld, int Owner);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
    void StartWallEdit(vec2 Dir);
    void EndWallEdit();
    bool Created;
    int m_Owner;

    void Die(int Killer, int Weapon=-1);
    void TakeDamage(int Dmg, int From, int Weapon=-1);

protected:
    bool HitCharacter();
    void CheckForBullets();
private:
    const int m_deconstruct_range = 40;

    CPlayer *pPlayer;
	vec2 m_From;
    vec2 m_Dir;
	int m_EvalTick;
    bool m_Done;
    float m_Delay_fac;
    float m_Health;
};
