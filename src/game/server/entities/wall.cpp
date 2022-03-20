/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <cassert>
#include <numeric>

#include "character.h"
#include "wall.h"
#include "projectile.h"
#include "pickup.h"

CWall::CWall(CGameWorld *pGameWorld, int Owner, int MapID, bool SpiderWeb) :  CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, vec2(0, 0), MapID)
{
    m_Owner = Owner;
    m_EvalTick = 0;
    m_SpiderWebTick=0;
    m_LastHitTick=0;
    m_Hited = 0;
    pPlayer = GameServer()->GetPlayerChar(m_Owner)->GetPlayer();
    Created= false;
    m_Done= false;
    m_Fortified= false;
    m_Delay_fac = 1000.0f;
    m_Health=0;
    m_SpiderWeb=SpiderWeb;
    if (SpiderWeb){
        for (int i = 0; i < m_MAX_SpiderWeb_Health; i++) {
            m_Health_Interface[i] = nullptr;
        }
    }else {
        for (int i = 0; i < m_MAX_Health; i++) {
            m_Health_Interface[i] = nullptr;
        }
    }
    m_Pos=vec2 (0,0);
}


bool CWall::HitCharacter()
{
    if (m_SpiderWeb){
        if (Server()->Tick() > m_LastHitTick + (Server()->TickSpeed() * m_WebHitDelay)) {
            vec2 At;
            CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

            CCharacter *pHit = GameWorld()->IntersectCharacter(m_Pos, m_From, 0.f, At, pOwnerChar);
            if (!pHit)
                return false;
            if (pHit->GetPlayer()->GetTeam() == pPlayer->GetTeam())
                return false;

            pHit->TakeDamage(vec2(0.f, 0.f), normalize(m_Pos - m_From),
                             1,
                             m_Owner, WEAPON_LASER);

            pHit->m_Core.m_Vel.x = clamp(pHit->m_Core.m_Vel.x, -10.f, 10.f);
            pHit->m_Core.m_Vel.y = clamp(pHit->m_Core.m_Vel.y, -10.f, 10.f);

            if (Server()->Tick() > m_LastHitTick+ (Server()->TickSpeed()*10)){
                m_Hited=0;
            }

            ++m_Hited;

            if(m_Hited>=m_WebMaxHits){
                TakeDamage(1, pHit->GetPlayer()->GetCID());
                m_Hited=0;
            }

            m_LastHitTick=Server()->Tick();
            return true;
        }
    }else {
        vec2 At;
        CCharacter *pOwnerChar = GameServer()->GetPlayerChar(m_Owner);

        CCharacter *pHit = GameWorld()->IntersectCharacter(m_Pos, m_From, 0.f, At, pOwnerChar);
        if (!pHit)
            return false;
        if (pHit->GetPlayer()->GetTeam() == pPlayer->GetTeam())
            return false;

        pHit->TakeDamage(vec2(0.f, 0.f), normalize(m_Pos - m_From), g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Damage * 2,
                         m_Owner, WEAPON_LASER);
        TakeDamage(1, pHit->GetPlayer()->GetCID());
        return true;
    }
}

vec2 CWall::CheckForIntersection(vec2 st_pos1, vec2 st_pos2, vec2 sec_pos1, vec2 sec_pos2){
    float a1=(st_pos1.y-st_pos2.y)/(st_pos1.x-st_pos2.x);
    float b1=st_pos1.y-a1*st_pos1.x;
    
    float a2=(sec_pos1.y-sec_pos2.y)/(sec_pos1.x-sec_pos2.x);
    float b2=sec_pos1.y-a1*sec_pos1.x;
    
    float x = (b2-b1)/(a1-a2);
    float y = a1*x+b1;
    
    vec2 res(x, y);

    float st_right_pos;
    float st_left_pos;
    if (st_pos1.x>st_pos2.x){
        st_right_pos=st_pos1.x;
        st_left_pos=st_pos2.x;
    } else{
        st_right_pos=st_pos2.x;
        st_left_pos=st_pos1.x;
    }

    float st_up_pos;
    float st_down_pos;
    if (st_pos1.y>st_pos2.y){
        st_up_pos=st_pos1.y;
        st_down_pos=st_pos2.y;
    } else{
        st_up_pos=st_pos2.y;
        st_down_pos=st_pos1.y;
    }

    float sec_right_pos;
    float sec_left_pos;
    if (sec_pos1.x>sec_pos2.x){
        sec_right_pos=sec_pos1.x;
        sec_left_pos=sec_pos2.x;
    } else{
        sec_right_pos=sec_pos2.x;
        sec_left_pos=sec_pos1.x;
    }

    float sec_up_pos;
    float sec_down_pos;
    if (sec_pos1.y>sec_pos2.y){
        sec_up_pos=sec_pos1.y;
        sec_down_pos=sec_pos2.y;
    } else{
        sec_up_pos=sec_pos2.y;
        sec_down_pos=sec_pos1.y;
    }

    if (x<=st_right_pos and x >= st_left_pos){
        if (y<=st_up_pos and y >= st_down_pos) {
            if (x<=sec_right_pos and x >= sec_left_pos) {
                if (y <= sec_up_pos and y >= sec_down_pos) {
                    return res;
                } else{
                    return vec2 (0,0);
                }
            } else{
                return vec2 (0,0);
            }
        } else{
            return vec2 (0,0);
        }
    } else{
        return vec2 (0,0);
    }
}

void CWall::HeIsHealing(CPlayer* player)
{
    if (!m_SpiderWeb){
        if (distance(player->GetCharacter()->GetPos(), m_Pos) <= player->GetCharacter()->GetProximityRadius()*1.5f or distance(player->GetCharacter()->GetPos(), m_From) <= player->GetCharacter()->GetProximityRadius()*1.5f){
            if (m_Health <m_MAX_Health){
                m_Health += 1;
                m_Health = clamp(m_Health, 0, m_MAX_Health);

                for (int i=0;i<m_Health; i++){
                    if (!m_Health_Interface[i]){
                        m_Health_Interface[i] = new CPickup(GameWorld(), PICKUP_HEALTH, Calc_hp_pos(m_HPTick/m_hp_interface_delay), GetMapID(), false);
                    }
                }
                GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, -1, GetMapID());
            }
        }
    } else if (m_SpiderWeb and m_Fortified){
        if (distance(player->GetCharacter()->GetPos(), m_Pos) <= player->GetCharacter()->GetProximityRadius()*1.5f or distance(player->GetCharacter()->GetPos(), m_From) <= player->GetCharacter()->GetProximityRadius()*1.5f){
            if (m_Health <m_MAX_FortifiedSpiderWeb_Health){
                m_Health += 1;
                m_Health = clamp(m_Health, 0, m_MAX_FortifiedSpiderWeb_Health);

                for (int i=0;i<m_Health; i++){
                    if (!m_Health_Interface[i]){
                        m_Health_Interface[i] = new CPickup(GameWorld(), PICKUP_HEALTH, Calc_hp_pos(m_HPTick/m_hp_interface_delay), GetMapID(), false);
                    }
                }
                GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, -1, GetMapID());
            }
        }
    }
}

bool CWall::EndWallEdit(int ammo){
    m_From = pPlayer->GetCharacter()->GetPos();
    m_From = Clamp_vec(m_Pos, m_From, m_laser_range);

    GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

    if (distance(m_Pos, m_From) >= radius * 2) {
        m_Delay_fac = 10000.0f;

        m_From = pPlayer->GetCharacter()->GetPos();
        m_From = Clamp_vec(m_Pos, m_From, m_laser_range);

        GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

        m_Health = ammo;

        //setup health animation
        vec2 pos;
        if (distance(diff, vec2(0, 0)) > 2 * radius) {
            pos = m_From;
        } else {
            midpoint1 = m_Pos;
            midpoint2 = m_From;
            vec2 vec = midpoint2 - midpoint1;
            theta = std::atan2(vec.x, vec.y) + pi / 2;
            diff = midpoint2 - midpoint1;
            versor = diff / distance(diff, vec2(0, 0));
            diff = midpoint2 - midpoint1 - versor * radius * 2;
            float line_segment_len = distance(diff, vec2(0, 0));
            total_path = radius * pi * 2 + 2 * line_segment_len;
            stops[0] = radius * pi / total_path;
            stops[1] = line_segment_len / total_path;
            stops[2] = radius * pi / total_path;
            stops[3] = line_segment_len / total_path;
            std::partial_sum(&stops[0], &stops[3], &cumsum_stops[0]);
            pos = Calc_hp_pos(m_HPTick / m_hp_interface_delay);
        }
        for (int i = 0; i < m_Health; i++) {
            if (pos != m_From) {
                int HPTick = m_HPTick + i * m_hp_interface_space;
                HPTick %= static_cast<int>(m_hp_interface_delay);
                pos = Calc_hp_pos(HPTick / m_hp_interface_delay);
            }
            m_Health_Interface[i] = new CPickup(GameWorld(), PICKUP_HEALTH, pos, GetMapID(), false);
        }

        //setup hud interface
        if (str_comp(GameServer()->GameType(), "DM") != 0 and str_comp(GameServer()->GameType(), "LMS") != 0) {
            vec2 middle((m_Pos.x + m_From.x) / 2, (m_Pos.y + m_From.y) / 2);
            m_Hud_Interface[0] = new CPickup(GameWorld(), PICKUP_ARMOR, middle, GetMapID(), false,
                                             pPlayer->GetTeam());
            m_Hud_Interface[1] = new CPickup(GameWorld(), PICKUP_ARMOR, m_From, GetMapID(), false,
                                             pPlayer->GetTeam());
            m_Hud_Interface[2] = new CPickup(GameWorld(), PICKUP_ARMOR, m_Pos, GetMapID(), false,
                                             pPlayer->GetTeam());
        }

        m_Done = true;
        return true;
    } else {
        m_From = pPlayer->GetCharacter()->GetPos();
        m_From = Clamp_vec(m_Pos, m_From, m_laser_range);
        return false;
    }
}

void CWall::StartWallEdit(vec2 Dir){
    if (!m_Done) {
        m_Dir = Dir;
        GameWorld()->InsertEntity(this);
        m_From = pPlayer->GetCharacter()->GetPos();
        m_Pos = m_From + m_Dir * m_laser_range;
        GameServer()->Collision(GetMapID())->IntersectLine(m_From, m_Pos, 0x0, &m_Pos);
        m_EvalTick = Server()->Tick();
        Created = true;
    }
}

bool CWall::FirstTryToFortify(vec2 Dir){
    bool b = false;
    CWall* spiderWebs [MAX_PLAYERS*5];
    int manyWebs = GameWorld()->FindEntities(pPlayer->GetCharacter()->GetPos(), m_laser_range, (CEntity**)spiderWebs, MAX_PLAYERS*5, GameWorld()->ENTTYPE_LASER, GetMapID());
    if (manyWebs>0) {
        for (int i = 0; i < manyWebs; i++) {
            if (spiderWebs[i]) {
                if (spiderWebs[i]->m_Done and !spiderWebs[i]->m_Fortified and  pPlayer->GetCharacter()->m_aWeapons[pPlayer->GetCharacter()->m_ActiveWeapon].m_Ammo>=1) {
                    if (distance(spiderWebs[i]->m_From, pPlayer->GetCharacter()->GetPos()) <= m_deconstruct_range or distance(spiderWebs[i]->m_Pos, pPlayer->GetCharacter()->GetPos()) <= m_deconstruct_range) {
                        spiderWebs[i]->SpiderWebFortify();
                        b = true;
                    }
                }
            }
        }
    }
    if (b){
        return true;
    } else{
        return false;
    }
}

bool CWall::SpiderWeb(vec2 Dir){

    if (pPlayer->GetCharacter()->m_aWeapons[pPlayer->GetCharacter()->m_ActiveWeapon].m_Ammo>=1) {
        m_Dir = Dir;
        m_From = pPlayer->GetCharacter()->GetPos();
        m_Pos = m_From + m_Dir * m_laser_range;
        GameServer()->Collision(GetMapID())->IntersectLine(m_From, m_Pos, 0x0, &m_Pos);
        m_Pos = Clamp_vec(m_From, m_Pos, m_SpiderWeb_range);

        if (distance(m_Pos, m_From) >= radius * 2) {
            GameWorld()->InsertEntity(this);
            m_Delay_fac = 10000.0f;

            m_From = pPlayer->GetCharacter()->GetPos();
            m_From = Clamp_vec(m_Pos, m_From, m_laser_range);

            GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

            m_Health = m_MAX_SpiderWeb_Health;

            //setup hud interface
            if (str_comp(GameServer()->GameType(), "DM") != 0 and str_comp(GameServer()->GameType(), "LMS") != 0) {
                vec2 middle((m_Pos.x + m_From.x) / 2, (m_Pos.y + m_From.y) / 2);
                m_Hud_Interface[0] = new CPickup(GameWorld(), PICKUP_ARMOR, middle, GetMapID(), false,
                                                 pPlayer->GetTeam());
                m_Hud_Interface[1] = new CPickup(GameWorld(), PICKUP_ARMOR, m_From, GetMapID(), false,
                                                 pPlayer->GetTeam());
                m_Hud_Interface[2] = new CPickup(GameWorld(), PICKUP_ARMOR, m_Pos, GetMapID(), false,
                                                 pPlayer->GetTeam());
            }

            m_SpiderWebTick = Server()->Tick();
            m_SpiderWeb = true;
            m_Done = true;
            pPlayer->m_Spider_ActiveWebs++;
            pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo -= 1;
            GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, -1, GetMapID());
            return true;
        } else {
            return false;
        }
    } else{
        return false;
    }
}

void CWall::SpiderWebFortify(){
    if (!m_Fortified) {
        if (distance(m_Pos, m_From) >= radius * 2) {

            m_Health = m_MAX_FortifiedSpiderWeb_Health;

            //setup health animation
            vec2 pos;
            if (distance(diff, vec2(0, 0)) > 2 * radius) {
                pos = m_From;
            } else {
                midpoint1 = m_Pos;
                midpoint2 = m_From;
                vec2 vec = midpoint2 - midpoint1;
                theta = std::atan2(vec.x, vec.y) + pi / 2;
                diff = midpoint2 - midpoint1;
                versor = diff / distance(diff, vec2(0, 0));
                diff = midpoint2 - midpoint1 - versor * radius * 2;
                float line_segment_len = distance(diff, vec2(0, 0));
                total_path = radius * pi * 2 + 2 * line_segment_len;
                stops[0] = radius * pi / total_path;
                stops[1] = line_segment_len / total_path;
                stops[2] = radius * pi / total_path;
                stops[3] = line_segment_len / total_path;
                std::partial_sum(&stops[0], &stops[3], &cumsum_stops[0]);
                pos = Calc_hp_pos(m_HPTick / m_hp_interface_delay);
            }
            for (int i = 0; i < m_Health; i++) {
                if (pos != m_From) {
                    int HPTick = m_HPTick + i * m_hp_interface_space;
                    HPTick %= static_cast<int>(m_hp_interface_delay);
                    pos = Calc_hp_pos(HPTick / m_hp_interface_delay);
                }
                m_Health_Interface[i] = new CPickup(GameWorld(), PICKUP_HEALTH, pos, GetMapID(), false);
            }

            pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo -= 1;
            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, -1, GetMapID());
            m_Fortified = true;
        }
    }
}

void CWall::HammerHit(int Dmg, CPlayer* player){
    if (player->GetTeam() != pPlayer->GetTeam()) {
        if (distance(player->GetCharacter()->GetPos(), m_Pos) <= player->GetCharacter()->GetProximityRadius() * 2.f) {
            GameServer()->CreateDamage(m_Pos, m_Owner, player->GetCharacter()->GetPos(), Dmg, 0, false, GetMapID());
            TakeDamage(Dmg, player->GetCID());
            GameServer()->CreateSound(m_Pos, SOUND_HAMMER_HIT, -1, GetMapID());
        } else if (distance(player->GetCharacter()->GetPos(), m_From) <= player->GetCharacter()->GetProximityRadius() * 2.f) {
            GameServer()->CreateDamage(m_From, m_Owner, player->GetCharacter()->GetPos(), Dmg, 0, false, GetMapID());
            TakeDamage(Dmg, player->GetCID());
            GameServer()->CreateSound(m_Pos, SOUND_HAMMER_HIT, -1, GetMapID());
        }
    }
}

bool CWall::TakeDamage(int Dmg, int From) {
    if (Dmg>0){
        m_Health-=Dmg;
    }

    if (!m_SpiderWeb)
        m_Health = clamp(m_Health, 0, m_MAX_Health);
    else if (m_SpiderWeb and !m_Fortified)
        m_Health = clamp(m_Health, 0, m_MAX_SpiderWeb_Health);
    else if (m_SpiderWeb and m_Fortified)
        m_Health = clamp(m_Health, 0, m_MAX_FortifiedSpiderWeb_Health);

    if (!m_SpiderWeb or (m_SpiderWeb and m_Fortified)) {
        for (int i = m_Health + 1; i <= m_MAX_Health; i++) {
            if (m_Health < m_MAX_Health) {
                if (m_Health_Interface[i - 1]) {
                    m_Health_Interface[i - 1]->Destroy();
                    m_Health_Interface[i - 1] = nullptr;
                }
            }
        }
    }

    if (m_Health <= 0) {
        Die(From);
        return true;
    }
    return false;
}

void CWall::Die(int Killer) {
    if (!m_SpiderWeb) {
        if (pPlayer) {
            if (pPlayer->GetCharacter()) {
                if (Killer == -1) {
                    CNetMsg_Sv_Chat chatMsg;
                    chatMsg.m_Mode = CHAT_WHISPER;
                    chatMsg.m_ClientID = m_Owner;
                    chatMsg.m_TargetID = m_Owner;
                    chatMsg.m_pMessage = "Wall was destroyed by zombie";
                    Server()->SendPackMsg(&chatMsg, MSGFLAG_VITAL, m_Owner);
                } else if (Killer == -2) {

                } else {
                    CNetMsg_Sv_Chat chatMsg;
                    chatMsg.m_Mode = CHAT_WHISPER;
                    chatMsg.m_ClientID = m_Owner;
                    chatMsg.m_TargetID = m_Owner;
                    char WallMsg[128];
                    str_format(WallMsg, sizeof(WallMsg), "Wall was destroyed by %s", Server()->ClientName(Killer));
                    chatMsg.m_pMessage = WallMsg;
                    Server()->SendPackMsg(&chatMsg, MSGFLAG_VITAL, m_Owner);
                    GameServer()->m_apPlayers[Killer]->m_Score += m_wall_score;
                }
            }
        }
        new CProjectile(GameWorld(), WEAPON_GRENADE,
                        m_Owner,
                        m_Pos,
                        vec2(0, 0),
                        0,
                        g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE,
                        WEAPON_GRENADE, GetMapID());
        new CProjectile(GameWorld(), WEAPON_GRENADE,
                        m_Owner,
                        m_From,
                        vec2(0, 0),
                        0,
                        g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE,
                        WEAPON_GRENADE, GetMapID());
    }
    Reset();
}

void CWall::CheckForBulletCollision(){
    CProjectile *pAttackBullet = (CProjectile *) GameWorld()->FindFirst(GameWorld()->ENTTYPE_PROJECTILE);
    if (pAttackBullet) {
        for (; pAttackBullet; pAttackBullet = (CProjectile *) pAttackBullet->TypeNext()) {
            vec2 At;
            if (GameWorld()->IntersectBullet(m_Pos, m_From, m_collision_range, At, pAttackBullet)) {

                if (GameServer()->m_apPlayers[pAttackBullet->GetOwner()]->GetTeam() !=
                    GameServer()->m_apPlayers[m_Owner]->GetTeam()) {

                    float Ct = (Server()->Tick() - pAttackBullet->GetStartTick()) / (float) Server()->TickSpeed();

                    if (distance(pAttackBullet->GetPos(Ct), m_Pos) <= m_deconstruct_range) {
                        int Dmg;
                        if (pAttackBullet->GetExposive()) {
                            Dmg = 2;
                        } else {
                            Dmg = 1;
                        }
                        Dmg *= 2;
                        if (TakeDamage(Dmg, pAttackBullet->GetOwner())) {
                            pAttackBullet->Wall_Coll = true;
                            pAttackBullet->Tick();
                            return;
                        }
                        GameServer()->CreateDamage(m_Pos, m_Owner, pAttackBullet->GetPos(Ct), Dmg, 0, false,
                                                   GetMapID());
                    } else if (distance(pAttackBullet->GetPos(Ct), m_From) <= m_deconstruct_range) {
                        int Dmg;
                        if (pAttackBullet->GetExposive()) {
                            Dmg = 2;
                        } else {
                            Dmg = 1;
                        }
                        Dmg *= 2;
                        if (TakeDamage(Dmg, pAttackBullet->GetOwner())) {
                            pAttackBullet->Wall_Coll = true;
                            pAttackBullet->Tick();
                            return;
                        }
                        GameServer()->CreateDamage(m_From, m_Owner, pAttackBullet->GetPos(Ct), Dmg, 0, false,
                                                   GetMapID());
                    } else {
                        int Dmg;
                        if (pAttackBullet->GetExposive()) {
                            Dmg = 2;
                        } else {
                            Dmg = 1;
                        }
                        if (TakeDamage(Dmg, pAttackBullet->GetOwner())) {
                            pAttackBullet->Wall_Coll = true;
                            pAttackBullet->Tick();
                            return;
                        }
                        GameServer()->CreateDamage(At, m_Owner, pAttackBullet->GetPos(Ct), Dmg, 0, false,
                                                   GetMapID());
                    }
                    pAttackBullet->Wall_Coll = true;
                    pAttackBullet->Tick();
                }
                if (pAttackBullet == (CProjectile *) pAttackBullet->TypeNext()) {
                    break;
                }
            }
        }
    }
}

void CWall::CheckForBullets() {
    if (!m_SpiderWeb) {
        CProjectile *pPosBullet = (CProjectile *) GameWorld()->ClosestEntity(m_Pos, m_deconstruct_range,
                                                                             GameWorld()->ENTTYPE_PROJECTILE, this,
                                                                             GetMapID());
        if (pPosBullet) {
            if (pPosBullet->GetOwner() == m_Owner and pPosBullet->GetWeapon() == WEAPON_GUN) {
                pPlayer->GetCharacter()->GiveWeapon(WEAPON_LASER, m_Health);
                pPosBullet->Destroy();
                Reset();
            }
        }

        CProjectile *pFromBullet = (CProjectile *) GameWorld()->ClosestEntity(m_From, m_deconstruct_range,
                                                                              GameWorld()->ENTTYPE_PROJECTILE, this,
                                                                              GetMapID());
        if (pFromBullet) {
            if (pFromBullet->GetOwner() == m_Owner and pFromBullet->GetWeapon() == WEAPON_GUN) {
                pPlayer->GetCharacter()->GiveWeapon(WEAPON_LASER, m_Health);
                pFromBullet->Destroy();
                Reset();
            }
        }
    } else{
        CProjectile *pPosBullet = (CProjectile *) GameWorld()->ClosestEntity(m_Pos, m_deconstruct_range,
                                                                             GameWorld()->ENTTYPE_PROJECTILE, this,
                                                                             GetMapID());

        CWall* otherWalls[MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs];
        int howMany = GameWorld()->FindEntities(m_Pos, m_laser_range, (CEntity **)otherWalls, MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs, GameWorld()->ENTTYPE_LASER, GetMapID());
        if (pPosBullet) {
            if (pPosBullet->GetOwner() == m_Owner and pPosBullet->GetWeapon() == WEAPON_GUN) {
                if (howMany>0) {
                    for (int i = 0; i < howMany; i++) {
                        if (otherWalls[i]) {
                            if (distance(otherWalls[i]->m_Pos, pPlayer->GetCharacter()->GetPos()) <=
                                m_deconstruct_range) {
                                if (pPosBullet->GetOwner() == otherWalls[i]->m_Owner) {
                                    if ((pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo + otherWalls[i]->m_Health)<=10 or otherWalls[i]->m_WaitingToConfirm){
                                        otherWalls[i]->Reset();
                                        pPlayer->GetCharacter()->GiveWeapon(WEAPON_SHOTGUN,
                                                                            pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo +
                                                                            otherWalls[i]->m_Health);
                                    } else{
                                        otherWalls[i]->m_WaitingToConfirm= true;
                                        otherWalls[i]->m_ConfirmTick= Server()->Tick();
                                    }
                                }
                            }
                        }
                    }
                }
                pPosBullet->Destroy();
            }
        }

        CProjectile *pFromBullet = (CProjectile *) GameWorld()->ClosestEntity(m_From, m_deconstruct_range,
                                                                              GameWorld()->ENTTYPE_PROJECTILE, this,
                                                                              GetMapID());

        otherWalls[MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs];
        howMany = GameWorld()->FindEntities(m_From, m_laser_range, (CEntity **)otherWalls, MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs, GameWorld()->ENTTYPE_LASER, GetMapID());
        if (pFromBullet) {
            if (pFromBullet->GetOwner() == m_Owner and pFromBullet->GetWeapon() == WEAPON_GUN) {
                if (howMany>0) {
                    for (int i = 0; i < howMany; i++) {
                        if (otherWalls[i]) {
                            if (distance(otherWalls[i]->m_From, pPlayer->GetCharacter()->GetPos()) <=
                                m_deconstruct_range) {
                                if (pFromBullet->GetOwner() == otherWalls[i]->m_Owner) {
                                    if ((pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo + otherWalls[i]->m_Health)<=10 or otherWalls[i]->m_WaitingToConfirm) {
                                        otherWalls[i]->Reset();
                                        pPlayer->GetCharacter()->GiveWeapon(WEAPON_SHOTGUN,
                                                                            pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo +
                                                                            otherWalls[i]->m_Health);
                                    } else{
                                        otherWalls[i]->m_WaitingToConfirm= true;
                                        otherWalls[i]->m_ConfirmTick= Server()->Tick();
                                    }
                                }
                            }
                        }
                    }
                }
                pFromBullet->Destroy();
            }
        }
    }
}

void CWall::UpdateHealthInterface(){
    m_HPTick++;
    m_HPTick %= static_cast<int>(m_hp_interface_delay);
    for (int i =0; i < m_MAX_Health; i++){
        if (m_Health_Interface[i]){
            int HPTick = m_HPTick + i*m_hp_interface_space;
            HPTick %= static_cast<int>(m_hp_interface_delay);
            m_Health_Interface[i]->SetPos(Calc_hp_pos(HPTick/m_hp_interface_delay));
        }
    }
}

void CWall::Reset()
{
    for (int i = 0; i<m_MAX_Health; i++){
        if (m_Health_Interface[i]) {
            m_Health_Interface[i]->Destroy();
            m_Health_Interface[i] = nullptr;
        }
    }
    for (int i = 0; i<3; i++){
        if(m_Hud_Interface[i]){
            m_Hud_Interface[i]->Destroy();
            m_Hud_Interface[i] = nullptr;
        }
    }
    if (!m_SpiderWeb) {
        pPlayer->m_Engineer_ActiveWalls--;
    } else {
        pPlayer->m_Spider_ActiveWebs--;
    }
    GameWorld()->DestroyEntity(this);
}

vec2 CWall::Calc_hp_pos(float alpha){
    if (alpha < cumsum_stops[0]) {
        alpha = alpha / stops[0];
        vec2 p;
        p.x = radius * sin(theta   + alpha * pi);
        p.y = radius * cos(theta    + alpha * pi);
        vec2 start = midpoint1 + versor * radius;
        return start + p;
    } else if (alpha < cumsum_stops[1]) {
        float a = (alpha - cumsum_stops[0]) / stops[1];

        vec2 start = midpoint1 + versor * radius +
                     vec2(-versor[1], versor[0]) * radius;
        vec2 end = midpoint2 - versor * radius +
                   vec2(-versor[1], versor[0]) * radius;
        vec2 p = start * (1 - a) + end * a;
        return p;
    } else if (alpha < cumsum_stops[2]) {
        alpha = (alpha - cumsum_stops[1]) / stops[2];
        vec2 p;
        p.x = radius * sin(theta - pi  + alpha * pi);
        p.y = radius * cos(theta - pi  + alpha * pi);
        vec2 start = midpoint2 - versor * radius;
        return start + p;
    } else {
        float a = (alpha - cumsum_stops[2]) / stops[3];

        vec2 start = midpoint2 - versor * radius +
                vec2(versor[1], -versor[0]) * radius;
        vec2 end = midpoint1 + versor * radius +
              vec2(versor[1], -versor[0]) * radius;
        vec2 p = start * (1 - a) + end * a;
        return p;
    }
}

vec2 CWall::Clamp_vec(vec2 From, vec2 To, float clamp){
    vec2 diff = To - From;
    float d = maximum(clamp, distance(To, From));
    vec2 clamped_end;
    if (d <= clamp) {
        clamped_end = To;
    } else {
        clamped_end = From + diff * clamp /d;
    }
    return clamped_end;
}

void CWall::Tick() {
    if (Server()->Tick() > m_EvalTick + (Server()->TickSpeed() * GameServer()->Tuning()->m_LaserBounceDelay) / m_Delay_fac){
        if(pPlayer->GetCharacter()) {
            if (pPlayer->GetCharacter()->GetActiveWeapon()!=WEAPON_LASER and !m_Done){
                return;
            }
            if (pPlayer->m_Engineer_Wall_Editing and !m_Done) {
                m_EvalTick = Server()->Tick();

                m_From = pPlayer->GetCharacter()->GetPos();
                m_From = Clamp_vec(m_Pos, m_From, m_laser_range);
                GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

//                CWall* otherWalls[MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs];
//                int howMany = GameWorld()->FindEntities(m_Pos, m_laser_range*2, (CEntity **)otherWalls, MAX_PLAYERS*pPlayer->m_Engineer_MaxActiveWalls + MAX_PLAYERS*pPlayer->m_Spider_MaxActiveWebs, GameWorld()->ENTTYPE_LASER, GetMapID());
//                if (howMany>0){
//                    for(int i=0; i<howMany;++i){
//                        vec2 intersection = CheckForIntersection(m_Pos, m_From, otherWalls[i]->m_Pos, otherWalls[i]->m_From); //Not working TODO fix CheckForIntersection
//                        if (intersection!=vec2(0,0)){
//                            m_From=intersection;
//                        }
//                    }
//                }

                GameServer()->CreateSound(m_From, SOUND_LASER_BOUNCE, -1, GetMapID());
            } else {
                m_EvalTick = Server()->Tick();

                GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

                HitCharacter();
                CheckForBullets();
                CheckForBulletCollision();
                if (!m_SpiderWeb or m_Fortified)
                    UpdateHealthInterface();
            }
            if (m_SpiderWeb and !m_Fortified){
                if(m_SpiderWebTick + (Server()->TickSpeed()*30) < Server()->Tick()) {
                    Reset();
                }
            }
            if (m_WaitingToConfirm){
                if(m_ConfirmTick + (Server()->TickSpeed()*5) < Server()->Tick()) {
                    m_WaitingToConfirm= false;
                }
            }
        } else{
            Die(-2);
        }
    }
}

void CWall::TickPaused()
{
    ++m_EvalTick;
    ++m_SpiderWebTick;
}

void CWall::Snap(int SnappingClient)
{
    if(NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_From))
        return;

    CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(), sizeof(CNetObj_Laser)));
    if(!pObj)
        return;

    pObj->m_X = (int)m_Pos.x;
    pObj->m_Y = (int)m_Pos.y;
    pObj->m_FromX = (int)m_From.x;
    pObj->m_FromY = (int)m_From.y;
    pObj->m_StartTick = m_EvalTick;
}