/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once

#include <game/server/entity.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "pickup.h"

class CWall : public CEntity
{
public:
	CWall(CGameWorld *pGameWorld, int Owner, int MapID, bool SpiderWeb =false);

	virtual void Reset();
    virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
    void StartWallEdit(vec2 Dir);
    bool EndWallEdit(int ammo);
    void SpiderWeb(vec2 Dir);
    vec2 Clamp_vec(vec2 From, vec2 To, float clamp);
    bool Created;
    int m_Owner;

    void Die(int Killer);
    bool TakeDamage(int Dmg, int From);
    void HammerHit(int Dmg, CPlayer* player);

    void HeIsHealing(CPlayer* player);
protected:
    vec2 Calc_hp_pos(float alpha);
    bool HitCharacter();
    void CheckForBullets();
    void CheckForBulletCollision();
    void UpdateHealthInterface();
private:
    static constexpr float m_laser_range = 800.f;
    static constexpr float m_deconstruct_range = 50.f;
    static constexpr float m_collision_range = 30.f;
    static constexpr float m_hp_interface_delay = 500.f;
    static constexpr float m_hp_interface_space = 50.f;
    static constexpr int m_wall_score = 2;
    static constexpr int m_MAX_Health = 10;
    static constexpr int m_MAX_SpiderWeb_Health = 1;
    static constexpr float m_SpiderWeb_range = 500.f;

    CPlayer *pPlayer;
    vec2 m_From;
    vec2 m_Dir;
    int m_EvalTick;
    int m_HPTick;
    bool m_Done;
    float m_Delay_fac;
    int m_Health;
    bool m_SpiderWeb;

    CPickup *m_Health_Interface[m_MAX_Health];
    CPickup *m_Hud_Interface[3];

    //-----------------------for calculations-------------------------------
    static constexpr float radius = 50.f;

    vec2 midpoint1;
    vec2 midpoint2;
    float theta;
    vec2 versor;
    vec2 diff;
    float total_path;
    float stops[4];
    float cumsum_stops[4];
};
