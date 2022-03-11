// LordSk
#include "zomb.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "engine/map.h"
#include <engine/server.h>
#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/storage.h>
#include <game/server/entity.h>
#include <game/server/player.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>
#include <generated/server_data.h>
#include <game/server/entities/projectile.h>

static char msgBuff__[256];
#ifdef CONF_DEBUG
#define dbg_zomb_msg(...)\
	memset(msgBuff__, 0, sizeof(msgBuff__));\
	str_format(msgBuff__, sizeof(msgBuff__), ##__VA_ARGS__);\
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "zomb", msgBuff__);
#else
#define dbg_zomb_msg(...)
#endif

#define std_zomb_msg(...)\
	memset(msgBuff__, 0, sizeof(msgBuff__));\
	str_format(msgBuff__, sizeof(msgBuff__), ##__VA_ARGS__);\
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "zomb", msgBuff__);

#define MULTILINE(a) #a

#define ZombCID(id) (id + MAX_SURVIVORS)
#define NO_TARGET -1
#define SecondsToTick(sec) (i32)(sec * SERVER_TICK_SPEED)

#define SPAWN_INTERVAL (SecondsToTick(0.5f))
#define SPAWN_INTERVAL_SURV (SecondsToTick(5.f))
#define WAVE_WAIT_TIME (SecondsToTick(10))

#define DEFAULT_ENRAGE_TIME 180

#define BOOMER_EXPLOSION_INNER_RADIUS 150.f
#define BOOMER_EXPLOSION_OUTER_RADIUS 300.f

#define BULL_CHARGE_CD (SecondsToTick(2.75f))
#define BULL_CHARGE_SPEED 1500.f
#define BULL_KNOCKBACK_FORCE 30.f

#define MUDGE_PULL_STR 1.1f
#define TANK_PULL_STR 1.0f

#define HUNTER_CHARGE_CD (SecondsToTick(1.5f))
#define HUNTER_CHARGE_SPEED 2700.f
#define HUNTER_CHARGE_DMG 3

#define BERSERKER_ENRAGE_RADIUS 300.f
#define WARTULE_GRENADE_DMG 2

#define ARMOR_DMG_REDUCTION 0.25f
#define HEALING_INTERVAL (SecondsToTick(0.5f))
#define HEALING_AMOUNT 1
#define ELITE_DMG_MULTIPLIER 1.5f

#define AURA_RADIUS 600.0f

#define SURVIVAL_MAX_TIME_EASY (SecondsToTick(600))
#define SURVIVAL_MAX_TIME_NORMAL (SecondsToTick(300))
#define SURVIVAL_MAX_TIME_HARD (SecondsToTick(150))
#define SURVIVAL_START_WAVE_INTERVAL_EASY (SecondsToTick(8))
#define SURVIVAL_START_WAVE_INTERVAL_NORMAL (SecondsToTick(4))
#define SURVIVAL_START_WAVE_INTERVAL_HARD (SecondsToTick(2))
#define SURVIVAL_ENRAGE_TIME 60

static const char* g_SurvDiffString[] = {
	"Easy",
	"Normal",
	"Hard"
};

enum {
	SKINPART_COUNT = SKINPART_EYES+1
};

enum {
	ZTYPE_BASIC = 0,
	ZTYPE_TANK,
	ZTYPE_BOOMER,
	ZTYPE_BULL,
	ZTYPE_MUDGE,
	ZTYPE_HUNTER,
	ZTYPE_DOMINANT,
	ZTYPE_BERSERKER,
	ZTYPE_WARTULE,

	ZTYPE_MAX,
};

enum {
	SURV_STATUS_OK=0,
	SURV_STATUS_LOW_HEALTH
};

static const char* g_ZombName[] = {
	"zombie",
	"Tank",
	"Boomer",
	"Bull",
	"Mudge",
	"Hunter",
	"Dominant",
	"Berserker",
	"Wartule"
};

static const u32 g_ZombMaxHealth[] = {
	7, // ZTYPE_BASIC
	35, // ZTYPE_TANK
	12, // ZTYPE_BOOMER
	20, // ZTYPE_BULL
	8, // ZTYPE_MUDGE
	7, // ZTYPE_HUNTER
	15, // ZTYPE_DOMINANT
	15, // ZTYPE_BERSERKER
	15 // ZTYPE_WARTULE
};

static const f32 g_ZombAttackSpeed[] = {
	1.5f, // ZTYPE_BASIC
	1.5f, // ZTYPE_TANK
	0.0001f, // ZTYPE_BOOMER
	0.5f, // ZTYPE_BULL
	2.0f, // ZTYPE_MUDGE
	2.0f, // ZTYPE_HUNTER
	0.6f, // ZTYPE_DOMINANT
	3.0f, // ZTYPE_BERSERKER
	2.0f // ZTYPE_WARTULE
};

static const i32 g_ZombAttackDmg[] = {
	1, // ZTYPE_BASIC
	4, // ZTYPE_TANK
	10, // ZTYPE_BOOMER
	4, // ZTYPE_BULL
	1, // ZTYPE_MUDGE
	1, // ZTYPE_HUNTER
	2, // ZTYPE_DOMINANT
	1, // ZTYPE_BERSERKER
	1, // ZTYPE_WARTULE
};

static const f32 g_ZombKnockbackMultiplier[] = {
	2.f, // ZTYPE_BASIC
	0.f, // ZTYPE_TANK
	0.f, // ZTYPE_BOOMER
	0.5f, // ZTYPE_BULL
	0.f, // ZTYPE_MUDGE
	1.5f, // ZTYPE_HUNTER
	1.f, // ZTYPE_DOMINANT
	0.5f, // ZTYPE_BERSERKER
	1.f, // ZTYPE_WARTULE
};

static const f32 g_ZombHookRange[] = {
	200.f, // ZTYPE_BASIC
	350.f, // ZTYPE_TANK
	300.f, // ZTYPE_BOOMER
	10.f,  // ZTYPE_BULL
	800.f, // ZTYPE_MUDGE
	10.f,  // ZTYPE_HUNTER
	300.f, // ZTYPE_DOMINANT
	300.f, // ZTYPE_BERSERKER
	300.f, // ZTYPE_WARTULE
};

static const i32 g_ZombGrabLimit[] = {
	SecondsToTick(0.2f), // ZTYPE_BASIC
	SecondsToTick(2.5f), // ZTYPE_TANK
	SecondsToTick(1.0f), // ZTYPE_BOOMER
	SecondsToTick(1.f), // ZTYPE_BULL
	SecondsToTick(30.f), // ZTYPE_MUDGE
	SecondsToTick(0.5f), // ZTYPE_HUNTER
	SecondsToTick(0.0f), // ZTYPE_DOMINANT
	SecondsToTick(0.3f), // ZTYPE_BERSERKER
	SecondsToTick(0.0f), // ZTYPE_WARTULE
};

static const i32 g_ZombHookCD[] = {
	SecondsToTick(2.f), // ZTYPE_BASIC
	SecondsToTick(2.f), // ZTYPE_TANK
	SecondsToTick(0.5f), // ZTYPE_BOOMER
	SecondsToTick(3.f), // ZTYPE_BULL
	SecondsToTick(5.f), // ZTYPE_MUDGE
	SecondsToTick(2.f), // ZTYPE_HUNTER
	SecondsToTick(999.f), // ZTYPE_DOMINANT
	SecondsToTick(1.5f), // ZTYPE_BERSERKER
	SecondsToTick(999.f), // ZTYPE_WARTULE
};

static const f32 g_ZombSpawnChance[] = {
	0.0f,   // ZTYPE_BASIC (unused)
	5.0f,   // ZTYPE_TANK
	35.0f,  // ZTYPE_BOOMER
	30.0f,  // ZTYPE_BULL
	10.0f,  // ZTYPE_MUDGE
	30.0f,  // ZTYPE_HUNTER
	20.0f,  // ZTYPE_DOMINANT
	10.0f,  // ZTYPE_BERSERKER
	20.0f,  // ZTYPE_WARTULE
};

static f32 g_ZombSpawnChanceTotal = 0.0f;

static const i32 g_WeaponDmgOnZomb[] = {
	3,  //WEAPON_HAMMER
	2,  //WEAPON_GUN
	1,  //WEAPON_SHOTGUN
	5,  //WEAPON_GRENADE
	7,  //WEAPON_LASER
	10, //WEAPON_NINJA
};

static const f32 g_WeaponForceOnZomb[] = {
	20.0f,  //WEAPON_HAMMER
	0,      //WEAPON_GUN
	0,      //WEAPON_SHOTGUN
	15.0f,  //WEAPON_GRENADE
	0,      //WEAPON_LASER
	0,      //WEAPON_NINJA
};

enum {
	BUFF_ENRAGED = 1,
	BUFF_HEALING = (1 << 1),
	BUFF_ARMORED = (1 << 2),
	BUFF_ELITE = (1 << 3)
};

enum {
	ZOMBIE_GROUNDJUMP = 1,
	ZOMBIE_AIRJUMP,
	ZOMBIE_BIGJUMP
};

enum {
	ZSTATE_NONE = 0,
	ZSTATE_WAVE_GAME,
	ZSTATE_SURV_GAME,
};

inline u32 xorshift32(u32* seed)
{
	u32 x = *seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*seed = x;
	return x;
}

inline f64 randf01(u32* seed)
{
	return (f64)xorshift32(seed)/0xffffffff;
}

inline i32 randi(u32* seed, i32 vmin, i32 vmax)
{
	dbg_assert(vmax > vmin, "vmax must be > vmin");
	return vmin + (xorshift32(seed) % (vmax - vmin + 1));
}

void CGameControllerZOMB::SpawnZombie(i32 zid, u8 type, u8 isElite, u32 enrageTime)
{
	const bool isEnraged = (enrageTime == 0 || type == ZTYPE_BERSERKER);
	// do we need to update clients
	bool needSendInfos = false;
	if(m_ZombType[zid] != type ||
	   (m_ZombBuff[zid]&BUFF_ELITE) != isElite ||
	   (m_ZombBuff[zid]&BUFF_ENRAGED) != isEnraged) {
		needSendInfos = true;
	}

	m_ZombAlive[zid] = true;
	m_ZombType[zid] = type;
	m_ZombHealth[zid] = g_ZombMaxHealth[type];
	m_ZombBuff[zid] = 0;
	m_ZombActiveWeapon[zid] = WEAPON_HAMMER;

	if(isElite) {
		m_ZombHealth[zid] *= 2;
		m_ZombBuff[zid] |= BUFF_ELITE;
	}

	if(isEnraged) {
		m_ZombBuff[zid] |= BUFF_ENRAGED;
	}

	vec2 spawnPos = m_ZombSpawnPoint[(zid + m_Seed)%m_ZombSpawnPointCount];
	m_ZombCharCore[zid].Reset();
	m_ZombCharCore[zid].m_Pos = spawnPos + vec2(randf01(&m_Seed) * 2.0f, randf01(&m_Seed) * 2.0f);
	GameServer()->m_World.m_Core.m_apCharacters[ZombCID(zid)] = &m_ZombCharCore[zid];

	m_ZombInput[zid] = CNetObj_PlayerInput();
	m_ZombSurvTarget[zid] = NO_TARGET;
	m_ZombJumpClock[zid] = 0;
	m_ZombAttackClock[zid] = SecondsToTick(1.f / g_ZombAttackSpeed[type]);
	m_ZombHookClock[zid] = 0;
	m_ZombEnrageClock[zid] = SecondsToTick(enrageTime);
	m_ZombExplodeClock[zid] = -1;
	m_ZombHookGrabClock[zid] = 0;
	m_ZombChargeClock[zid] = 0;
	m_ZombChargingClock[zid] = -1;
	m_ZombHealClock[zid] = 0;
	m_ZombLastShotDir[zid] = vec2(-1, 0);
	m_ZombEyesClock[zid] = -1;
	m_ZombInvisClock[zid] = 0;
	m_ZombInvisible[zid] = false;

	if(needSendInfos) {
		for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
			if(GameServer()->m_apPlayers[i] && Server()->ClientIngame(i)) {
				SendZombieInfos(zid, i);
			}
		}
	}

	GameServer()->CreatePlayerSpawn(spawnPos, Server()->MainMapID);
}

void CGameControllerZOMB::KillZombie(i32 zid, i32 killerCID)
{
	m_ZombAlive[zid] = false;
	GameServer()->m_World.m_Core.m_apCharacters[ZombCID(zid)] = 0;
	GameServer()->CreateSound(m_ZombCharCore[zid].m_Pos, SOUND_PLAYER_DIE, -1, Server()->MainMapID);
	GameServer()->CreateSound(m_ZombCharCore[zid].m_Pos,
		m_ZombType[zid] == ZTYPE_BASIC ? SOUND_PLAYER_PAIN_SHORT : SOUND_PLAYER_PAIN_LONG, -1, Server()->MainMapID);
	GameServer()->CreateDeath(m_ZombCharCore[zid].m_Pos, ZombCID(zid), Server()->MainMapID);
	// TODO: send a kill message?

	if(m_RedFlagCarrier == ZombCID(zid)) {
		m_RedFlagCarrier = -1;
	}
}

void CGameControllerZOMB::SwingHammer(i32 zid, u32 dmg, f32 knockback)
{
	const vec2& zpos = m_ZombCharCore[zid].m_Pos;
	GameServer()->CreateSound(zpos, SOUND_HAMMER_FIRE, -1, Server()->MainMapID);

	vec2 hitDir = normalize(vec2(m_ZombInput[zid].m_TargetX, m_ZombInput[zid].m_TargetY));
	vec2 hitPos = zpos + hitDir * 21.f; // NOTE: different reach per zombie type?

	CCharacter *apEnts[MAX_CLIENTS];

	// NOTE: same here for radius
	i32 count = GameServer()->m_World.FindEntities(hitPos, 14.f, (CEntity**)apEnts,
					MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);

	for(i32 i = 0; i < count; ++i) {
		CCharacter *pTarget = apEnts[i];
		const vec2& targetPos = pTarget->GetPos();

		if(GameServer()->Collision(Server()->MainMapID)->IntersectLine(hitPos, targetPos, NULL, NULL)) {
			continue;
		}

		// set his velocity to fast upward (for now)
		if(length(targetPos-hitPos) > 0.0f) {
			GameServer()->CreateHammerHit(targetPos-normalize(targetPos-hitPos)*14.f, Server()->MainMapID);
		}
		else {
			GameServer()->CreateHammerHit(hitPos, Server()->MainMapID);
		}

		vec2 kbDir;
		if(length(targetPos - zpos) > 0.0f) {
			kbDir = normalize(targetPos - zpos);
		}
		else {
			kbDir = vec2(0.f, -1.f);
		}

		vec2 kbForce = vec2(0.f, -1.f) + normalize(kbDir + vec2(0.f, -1.1f)) * knockback;

		pTarget->TakeDamage(kbForce, kbDir, dmg, ZombCID(zid), WEAPON_HAMMER);
	}

	m_ZombAttackTick[zid] = m_Tick;
}

struct Node
{
	ivec2 pos;
	f32 g, f;
	Node* pParent;

	Node() {};
	Node(ivec2 pos_, f32 g_, f32 h_, Node* pParent_):
		pos(pos_),
		g(g_),
		f(g_ + h_),
		pParent(pParent_)
	{}
};

inline Node* addToList_(Node** pNodeList, u32* pCount, Node* pNode)
{
	dbg_assert((*pCount)+1 < MAX_MAP_SIZE, "list is full");\
	pNodeList[(*pCount)++] = pNode;
	return pNodeList[(*pCount)-1];
}

#define addToList(list, node) addToList_(list, &list##Count, (node))

inline Node* addNode_(Node* pNodeList, u32* pCount, Node node)
{
	dbg_assert((*pCount)+1 < MAX_MAP_SIZE, "list is full");\
	pNodeList[(*pCount)++] = node;
	return &pNodeList[(*pCount)-1];
}

#define addNode(node) addNode_(nodeList, &nodeListCount, (node))

inline Node* searchList_(Node* pNodeList, u32 count, ivec2 pos)
{
	for(u32 i = 0; i < count; ++i) {
		if(pNodeList[i].pos == pos) {
			return &pNodeList[i];
		}
	}
	return 0;
}

#define searchList(list, pos) searchList_(list, list##Count, pos)

static i32 compareNodePtr(const void* a, const void* b)
{
	const Node* na = *(const Node**)a;
	const Node* nb = *(const Node**)b;
	if(na->f < nb->f) return -1;
	if(na->f > nb->f) return 1;
	return 0;
}

inline bool CGameControllerZOMB::InMapBounds(const ivec2& pos)
{
	return (pos.x >= 0 && pos.x < (i32)m_MapWidth &&
			pos.y >= 0 && pos.y < (i32)m_MapHeight);
}

inline bool CGameControllerZOMB::IsBlockedOrOob(const ivec2& pos)
{
	return (!InMapBounds(pos) || m_Map[pos.y * m_MapWidth + pos.x] != 0);
}

inline bool CGameControllerZOMB::IsBlocked(const ivec2& pos)
{
	return m_Map[pos.y * m_MapWidth + pos.x] != 0;
}

static Node* openList[MAX_MAP_SIZE];
static Node* closedList[MAX_MAP_SIZE];
static u32 openListCount = 0;
static u32 closedListCount = 0;
static Node nodeList[MAX_MAP_SIZE];
static u32 nodeListCount = 0;

bool CGameControllerZOMB::JumpStraight(const ivec2& start, const ivec2& dir, const ivec2& goal,
									   i32* out_pJumps)
{
	ivec2 jumpPos = start;
	ivec2 forcedCheckDir[2]; // places to check for walls

	if(dir.x != 0) {
		forcedCheckDir[0] = ivec2(0, 1);
		forcedCheckDir[1] = ivec2(0, -1);
	}
	else {
		forcedCheckDir[0] = ivec2(1, 0);
		forcedCheckDir[1] = ivec2(-1, 0);
	}

	*out_pJumps = 0;
	while(1) {
		if(jumpPos == goal) {
			return true;
		}

		// hit a wall, diregard jump
		if(!InMapBounds(jumpPos) || IsBlockedOrOob(jumpPos)) {
			return false;
		}

		// forced neighours check
		ivec2 wallPos = jumpPos + forcedCheckDir[0];
		ivec2 freePos = jumpPos + forcedCheckDir[0] + dir;
		if(IsBlockedOrOob(wallPos) && !IsBlockedOrOob(freePos)) {
			return true;
		}

		wallPos = jumpPos + forcedCheckDir[1];
		freePos = jumpPos + forcedCheckDir[1] + dir;
		if(IsBlockedOrOob(wallPos) && !IsBlockedOrOob(freePos)) {
			return true;
		}

		jumpPos += dir;
		++(*out_pJumps);
	}

	return false;
}

bool CGameControllerZOMB::JumpDiagonal(const ivec2& start, const ivec2& dir, const ivec2& goal,
									   i32* out_pJumps)
{
	ivec2 jumpPos = start;
	ivec2 forcedCheckDir[2]; // places to check for walls
	forcedCheckDir[0] = ivec2(-dir.x, 0);
	forcedCheckDir[1] = ivec2(0, -dir.y);

	ivec2 straigthDirs[] = {
		ivec2(dir.x, 0),
		ivec2(0, dir.y)
	};

	*out_pJumps = 0;
	while(1) {
		if(jumpPos == goal) {
			return true;
		}

		// check veritcal/horizontal
		for(u32 i = 0; i < 2; ++i) {
			i32 j;
			if(JumpStraight(jumpPos, straigthDirs[i], goal, &j)) {
				return true;
			}
		}

		// hit a wall, diregard jump
		if(!InMapBounds(jumpPos) || IsBlockedOrOob(jumpPos)) {
			return false;
		}

		// forced neighours check
		ivec2 wallPos = jumpPos + forcedCheckDir[0];
		ivec2 freePos = jumpPos + forcedCheckDir[0] + ivec2(0, dir.y);
		if(IsBlockedOrOob(wallPos) && !IsBlockedOrOob(freePos)) {
			return true;
		}

		wallPos = jumpPos + forcedCheckDir[1];
		freePos = jumpPos + forcedCheckDir[1] + ivec2(dir.x, 0);
		if(IsBlockedOrOob(wallPos) && !IsBlockedOrOob(freePos)) {
			return true;
		}

		jumpPos += dir;
		++(*out_pJumps);
	}

	return false;
}

vec2 CGameControllerZOMB::PathFind(vec2 start, vec2 end)
{
	ivec2 mStart(start.x / 32, start.y / 32);
	ivec2 mEnd(end.x / 32, end.y / 32);

	if(mStart == mEnd) {
		return end;
	}

	if(!InMapBounds(mEnd)) {
		mEnd.x = clamp(mEnd.x, 0, m_MapWidth-1);
		mEnd.y = clamp(mEnd.y, 0, m_MapHeight-1);
	}

	if(IsBlockedOrOob(mStart) || IsBlocked(mEnd)) {
		// dbg_zomb_msg("Error: PathFind() start and end point must be clear.");
		return end;
	}

	bool pathFound = false;
	openListCount = 0;
	closedListCount = 0;
	nodeListCount = 0;

#ifdef CONF_DEBUG
	//m_DbgPathLen = 0;
	//m_DbgLinesCount = 0;
	u32 maxIterations = Config()->m_DbgPathFindIterations;
#endif

	Node* pFirst = addNode(Node(mStart, 0, 0, NULL));
	addToList(openList, pFirst);

	u32 iterations = 0;
	while(openListCount > 0 && !pathFound) {
		qsort(openList, openListCount, sizeof(Node*), compareNodePtr);

		// pop first node from open list and add it to closed list
		Node* pCurrent = openList[0];
		openList[0] = openList[openListCount-1];
		--openListCount;
		addToList(closedList, pCurrent);

		// neighbours
		ivec2 nbDir[8];
		i32 nbCount = 0;
		const ivec2& cp = pCurrent->pos;

		// first node (TODO: move this out of the loop)
		if(!pCurrent->pParent) {
			for(i32 x = -1; x < 2; ++x) {
				for(i32 y = -1; y < 2; ++y) {
					ivec2 dir(x, y);
					ivec2 succPos = cp + dir;
					if((x == 0 && y == 0) ||
					   !InMapBounds(succPos) ||
					   IsBlocked(succPos)) {
						continue;
					}
					nbDir[nbCount++] = dir;
				}
			}
		}
		else {
			ivec2 fromDir(clamp(cp.x - pCurrent->pParent->pos.x, -1, 1),
						  clamp(cp.y - pCurrent->pParent->pos.y, -1, 1));



			// straight (add fromDir + adjacent diags)
			if(fromDir.x == 0 || fromDir.y == 0) {
				if(!IsBlockedOrOob(cp + fromDir)) {
					nbDir[nbCount++] = fromDir;

					if(fromDir.x == 0) {
						ivec2 diagR(1, fromDir.y);
						if(!IsBlockedOrOob(cp + diagR)) {
							nbDir[nbCount++] = diagR;
						}
						ivec2 diagL(-1, fromDir.y);
						if(!IsBlockedOrOob(cp + diagL)) {
							nbDir[nbCount++] = diagL;
						}
					}
					else if(fromDir.y == 0) {
						ivec2 diagB(fromDir.x, 1);
						if(!IsBlockedOrOob(cp + diagB)) {
							nbDir[nbCount++] = diagB;
						}
						ivec2 diagT(fromDir.x, -1);
						if(!IsBlockedOrOob(cp + diagT)) {
							nbDir[nbCount++] = diagT;
						}
					}
				}
			}
			// diagonal (add diag + adjacent straight dirs + forced neighbours)
			else {
				ivec2 stX(fromDir.x, 0);
				ivec2 stY(0, fromDir.y);

				if(!IsBlockedOrOob(cp + fromDir) &&
				   (!IsBlockedOrOob(cp + stX) || !IsBlockedOrOob(cp + stY))) {
					nbDir[nbCount++] = fromDir;
				}
				if(!IsBlockedOrOob(cp + stX)) {
					nbDir[nbCount++] = stX;
				}
				if(!IsBlockedOrOob(cp + stY)) {
					nbDir[nbCount++] = stY;
				}

				// forced neighbours
				ivec2 fn1(-stX.x, stY.y);
				if(IsBlockedOrOob(cp - stX) && !IsBlockedOrOob(cp + stY) && !IsBlockedOrOob(cp + fn1)) {
					nbDir[nbCount++] = fn1;
				}
				ivec2 fn2(stX.x, -stY.y);
				if(IsBlockedOrOob(cp - stY) && !IsBlockedOrOob(cp + stX) && !IsBlockedOrOob(cp + fn2)) {
					nbDir[nbCount++] = fn2;
				}
			}
		}

		// jump
		for(i32 n = 0; n < nbCount; ++n) {
			const ivec2& succDir = nbDir[n];
			ivec2 succPos = pCurrent->pos + succDir;

			if(succDir.x == 0 || succDir.y == 0) {
				i32 jumps;
				if(JumpStraight(succPos, succDir, mEnd, &jumps)) {
					ivec2 jumpPos = succPos + succDir * jumps;
					f32 g = pCurrent->g + jumps;
					f32 h = fabs(mEnd.x - jumpPos.x) + fabs(mEnd.y - jumpPos.y);

					if(jumpPos == mEnd) {
						addToList(closedList, addNode(Node(jumpPos, 0, 0, pCurrent)));
						pathFound = true;
						break;
					}
					else {
						Node* pSearchNode = searchList(nodeList, jumpPos);
						if(pSearchNode) {
							f32 f = g + h;
							if(f < pSearchNode->f) {
								pSearchNode->f = f;
								pSearchNode->g = g;
								pSearchNode->pParent = pCurrent;
							}
						}
						else {
							Node* pJumpNode = addNode(Node(jumpPos, g, h, pCurrent));
							addToList(openList, pJumpNode);
						}
					}
				}
			}
			else {
				i32 jumps = 0;
				if(JumpDiagonal(succPos, succDir, mEnd, &jumps)) {
					ivec2 jumpPos = succPos + succDir * jumps;
					f32 g = pCurrent->g + jumps * 2.f;
					f32 h = fabs(mEnd.x - jumpPos.x) + fabs(mEnd.y - jumpPos.y);

					if(jumpPos == mEnd) {
						addToList(closedList, addNode(Node(jumpPos, 0, 0, pCurrent)));
						pathFound = true;
						break;
					}
					else {
						Node* pSearchNode = searchList(nodeList, jumpPos);
						if(pSearchNode) {
							f32 f = g + h;
							if(f < pSearchNode->f) {
								pSearchNode->f = f;
								pSearchNode->g = g;
								pSearchNode->pParent = pCurrent;
							}
						}
						else {
							Node* pJumpNode = addNode(Node(jumpPos, g, h, pCurrent));
							addToList(openList, pJumpNode);
						}
					}
				}
			}
		}

		// TODO: remove this safeguard
		++iterations;
#ifdef CONF_DEBUG
		if(iterations > maxIterations) {
			break;
		}
#endif
	}

	//m_DbgLinesCount = 0;
	Node* pCur = closedList[closedListCount-1];
	ivec2 next = pCur->pos;
	while(pCur && pCur->pParent) {
		//DebugLine(next, pCur->pos);
		next = pCur->pos;
		pCur = pCur->pParent;
	}

	return vec2(next.x * 32.f + 16.f, next.y * 32.f + 16.f);
}

inline i32 PackColor(i32 hue, i32 sat, i32 lgt, i32 alpha = 255)
{
	return ((alpha << 24) + (hue << 16) + (sat << 8) + lgt);
}

void CGameControllerZOMB::SendZombieInfos(i32 zid, i32 CID)
{
	// send zombie client informations
	u32 zombCID = ZombCID(zid);

	// send drop first
	CNetMsg_Sv_ClientDrop Msg;
	Msg.m_ClientID = zombCID;
	Msg.m_pReason = "";
	Msg.m_Silent = 1;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, CID);

	// then update
	CNetMsg_Sv_ClientInfo nci;
	nci.m_ClientID = zombCID;
	nci.m_Local = 0;
	nci.m_Team = 0;
	nci.m_pClan = "";
	nci.m_Country = 0;
	nci.m_Silent = 1;
	nci.m_apSkinPartNames[SKINPART_DECORATION] = "standard";
	nci.m_aUseCustomColors[SKINPART_DECORATION] = 0;
	nci.m_aSkinPartColors[SKINPART_DECORATION] = 0;

	// hands and feets color
	i32 brown = PackColor(28, 77, 13);
	i32 red = PackColor(0, 255, 40);
	i32 yellow = PackColor(37, 255, 70);
	i32 handFeetColor = brown;

	nci.m_apSkinPartNames[SKINPART_EYES] = "zombie_eyes";
	nci.m_aUseCustomColors[SKINPART_EYES] = 0;
	nci.m_aSkinPartColors[SKINPART_EYES] = 0;

	if(m_ZombBuff[zid]&BUFF_ELITE) {
		nci.m_apSkinPartNames[SKINPART_EYES] = "zombie_eyes_elite";
	}

	nci.m_aUseCustomColors[SKINPART_MARKING] = 0;
	nci.m_aSkinPartColors[SKINPART_MARKING] = 0;

	if(m_ZombBuff[zid]&BUFF_ENRAGED && m_ZombType[zid] != ZTYPE_BERSERKER) {
		nci.m_apSkinPartNames[SKINPART_MARKING] = "enraged";
	}
	else {
		nci.m_apSkinPartNames[SKINPART_MARKING] = "standard";
	}

	nci.m_aUseCustomColors[SKINPART_BODY] = 0;
	nci.m_aSkinPartColors[SKINPART_BODY] = 0;

	if(m_ZombBuff[zid]&BUFF_ENRAGED && m_ZombType[zid] != ZTYPE_BERSERKER) {
		nci.m_aUseCustomColors[SKINPART_BODY] = 1;
		nci.m_aSkinPartColors[SKINPART_BODY] = red;
	}

	static char s_aName[64];
	if(m_ZombBuff[zid]&BUFF_ELITE) {
		str_format(s_aName, sizeof(s_aName), "Elite %s", g_ZombName[m_ZombType[zid]]);
		nci.m_pName = s_aName;
	}
	else {
		nci.m_pName = g_ZombName[m_ZombType[zid]];
	}

	switch(m_ZombType[zid]) {
		case ZTYPE_BASIC:
			nci.m_apSkinPartNames[SKINPART_BODY] = zid%2 ? "zombie1" : "zombie2";
			break;

		case ZTYPE_TANK:
			nci.m_apSkinPartNames[SKINPART_BODY] = "tank";
			handFeetColor = PackColor(9, 145, 108); // pinkish feet
			break;

		case ZTYPE_BOOMER:
			nci.m_apSkinPartNames[SKINPART_BODY] = "boomer";
			break;

		case ZTYPE_BULL:
			nci.m_apSkinPartNames[SKINPART_BODY] = "bull";
			break;

		case ZTYPE_MUDGE:
			nci.m_apSkinPartNames[SKINPART_BODY] = "mudge";
			break;

		case ZTYPE_HUNTER:
			nci.m_apSkinPartNames[SKINPART_BODY] = "hunter";
			handFeetColor = PackColor(0, 0, 15); // grey
			break;

		case ZTYPE_DOMINANT:
			nci.m_apSkinPartNames[SKINPART_BODY] = "dominant";
			break;

		case ZTYPE_BERSERKER:
			nci.m_apSkinPartNames[SKINPART_BODY] = "berserker";
			handFeetColor = red;
			break;

		case ZTYPE_WARTULE:
			nci.m_apSkinPartNames[SKINPART_BODY] = "wartule";
			break;

		default: break;
	}

	if(m_ZombBuff[zid]&BUFF_ENRAGED) {
		handFeetColor = red;
	}
	if(m_ZombBuff[zid]&BUFF_ELITE) {
		handFeetColor = yellow;
	}

	nci.m_apSkinPartNames[SKINPART_HANDS] = "standard";
	nci.m_aUseCustomColors[SKINPART_HANDS] = 1;
	nci.m_aSkinPartColors[SKINPART_HANDS] = handFeetColor;
	nci.m_apSkinPartNames[SKINPART_FEET] = "standard";
	nci.m_aUseCustomColors[SKINPART_FEET] = 1;
	nci.m_aSkinPartColors[SKINPART_FEET] = handFeetColor;

	//dbg_zomb_msg("zombInfo (zombCID=%d CID=%d)", zombCID, CID);
	Server()->SendPackMsg(&nci, MSGFLAG_VITAL|MSGFLAG_NORECORD, CID);
}

void CGameControllerZOMB::SendSurvivorStatus(i32 SurvCID, i32 CID, i32 Status)
{
	CPlayer** apPlayers = GameServer()->m_apPlayers;
	if(SurvCID == CID || !apPlayers[CID]) return;

	// send drop first
	CNetMsg_Sv_ClientDrop Msg;
	Msg.m_ClientID = SurvCID;
	Msg.m_pReason = "";
	Msg.m_Silent = 1;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, CID);

	// then update
	CNetMsg_Sv_ClientInfo nci;
	nci.m_ClientID = SurvCID;
	nci.m_Local = 0;
	nci.m_Team = 0;
	nci.m_pName = Server()->ClientName(SurvCID);
	nci.m_pClan = Server()->ClientClan(SurvCID);
	nci.m_Country = Server()->ClientCountry(SurvCID);
	nci.m_Silent = 1;

	for(i32 i = 0; i < SKINPART_COUNT; i++)
	{
		nci.m_apSkinPartNames[i] = apPlayers[SurvCID]->m_TeeInfos.m_aaSkinPartNames[i];
	}

	if(Status == SURV_STATUS_LOW_HEALTH)
	{
		const i32 red = PackColor(0, 255, 64);
		for(i32 i = 0; i < SKINPART_COUNT; i++)
		{
			nci.m_aUseCustomColors[i] = 1;
			nci.m_aSkinPartColors[i] = red;
		}
	}
	else
	{
		for(i32 i = 0; i < SKINPART_COUNT; i++)
		{
			nci.m_aUseCustomColors[i] = apPlayers[SurvCID]->m_TeeInfos.m_aUseCustomColors[i];
			nci.m_aSkinPartColors[i] = apPlayers[SurvCID]->m_TeeInfos.m_aSkinPartColors[i];
		}
	}

	dbg_zomb_msg("sending survivorInfo (SurvCID=%d CID=%d Status=%d)", SurvCID, CID, Status);
	Server()->SendPackMsg(&nci, MSGFLAG_VITAL|MSGFLAG_NORECORD, CID);
}

void CGameControllerZOMB::HandleMovement(u32 zid, const vec2& targetPos, bool targetLOS)
{
	const vec2& pos = m_ZombCharCore[zid].m_Pos;
	CNetObj_PlayerInput& input = m_ZombInput[zid];

	vec2& dest = m_ZombDestination[zid];
	if(m_ZombPathFindClock[zid] <= 0) {
		if(targetLOS) {
			dest = targetPos;
		}
		else {
			dest = PathFind(pos, targetPos);
		}
		m_ZombDestination[zid] = dest;
		m_ZombPathFindClock[zid] = 2;
	}

	//DebugLine(ivec2(pos.x/32, pos.y/32), ivec2(dest.x/32, dest.y/32));

	input.m_Direction = -1;
	if(pos.x < dest.x) {
		input.m_Direction = 1;
	}

	// grounded?
	f32 phyzSize = 28.f;
	bool grounded = (GameServer()->Collision(Server()->MainMapID)->CheckPoint(pos.x+phyzSize/2, pos.y+phyzSize/2+5) ||
					 GameServer()->Collision(Server()->MainMapID)->CheckPoint(pos.x-phyzSize/2, pos.y+phyzSize/2+5));
	for(u32 i = 0; i < MAX_CLIENTS; ++i) {
		CCharacterCore* pCore = GameServer()->m_World.m_Core.m_apCharacters[i];
		if(!pCore) {
			continue;
		}

		// on top of another character
		if(pos.y < pCore->m_Pos.y &&
		   fabs(pos.x - pCore->m_Pos.x) < phyzSize &&
		   fabs(pos.y - pCore->m_Pos.y) < (phyzSize*1.5f)) {
			grounded = true;
			break;
		}
	}

	if(grounded) {
		m_ZombAirJumps[zid] = 1;
	}

	const bool destLOS = (GameServer()->Collision(Server()->MainMapID)->IntersectLine(pos, dest, 0, 0) == 0);

	// jump
	input.m_Jump = 0;
	f32 xDist = fabs(dest.x - pos.x);
	f32 yDist = fabs(dest.y - pos.y);

	// don't excessively jump when target does
	if(destLOS) {
		if((yDist < 20.f && xDist > 40.f) || xDist > 150.0f) {
			m_ZombJumpClock[zid] = 1;
		}
	}

	if(m_ZombJumpClock[zid] <= 0 && dest.y < pos.y) {
		if(yDist > 300.f) {
			input.m_Jump = ZOMBIE_BIGJUMP;
			m_ZombJumpClock[zid] = SecondsToTick(0.9f);
		}
		else if(yDist > 0.f) {
			if(grounded) {
				input.m_Jump = ZOMBIE_GROUNDJUMP;
				m_ZombJumpClock[zid] = SecondsToTick(0.5f);
			}
			else if(m_ZombAirJumps[zid] > 0) {
				input.m_Jump = ZOMBIE_AIRJUMP;
				m_ZombJumpClock[zid] = SecondsToTick(0.5f);
				--m_ZombAirJumps[zid];
			}
		}
	}
}

void CGameControllerZOMB::HandleHook(u32 zid, f32 targetDist, bool targetLOS)
{
	CNetObj_PlayerInput& input = m_ZombInput[zid];

	if(m_ZombHookClock[zid] <= 0 && targetDist < g_ZombHookRange[m_ZombType[zid]] && targetLOS) {
		input.m_Hook = 1;
	}

	if(m_ZombCharCore[zid].m_HookState == HOOK_GRABBED &&
	   m_ZombCharCore[zid].m_HookedPlayer != m_ZombSurvTarget[zid]) {
		input.m_Hook = 0;
	}

	if(m_ZombCharCore[zid].m_HookState == HOOK_RETRACTED) {
		input.m_Hook = 0;
	}

	if(m_ZombCharCore[zid].m_HookState == HOOK_FLYING) {
		input.m_Hook = 1;
	}

	// keep grabbing target
	if(m_ZombCharCore[zid].m_HookState == HOOK_GRABBED &&
	   m_ZombCharCore[zid].m_HookedPlayer == m_ZombSurvTarget[zid]) {
		++m_ZombHookGrabClock[zid];
		i32 limit = g_ZombGrabLimit[m_ZombType[zid]];
		if(m_ZombBuff[zid]&BUFF_ENRAGED) {
			limit *= 2;
		}

		if(m_ZombHookGrabClock[zid] >= limit) {
			input.m_Hook = 0;
			m_ZombHookGrabClock[zid] = 0;
		}
		else {
			input.m_Hook = 1;
			m_ZombCharCore[zid].m_HookTick = 0;
		}
	}

	if(m_ZombInput[zid].m_Hook == 0) {
		m_ZombHookGrabClock[zid] = 0;
	}

	if(m_ZombCharCore[zid].m_HookState == HOOK_GRABBED) {
		m_ZombHookClock[zid] = g_ZombHookCD[m_ZombType[zid]];
	}
}

void CGameControllerZOMB::HandleBoomer(u32 zid, f32 targetDist, bool targetLOS)
{
	i32 zombCID = ZombCID(zid);
	const vec2& pos = m_ZombCharCore[zid].m_Pos;

	if(m_ZombExplodeClock[zid] > 0)
		--m_ZombExplodeClock[zid];

	// BOOM
	if(m_ZombExplodeClock[zid] == 0) {
		KillZombie(zid, zombCID);
		i32 dmg = g_ZombAttackDmg[m_ZombType[zid]];
		if(m_ZombBuff[zid]&BUFF_ELITE) {
			dmg *= 2;
		}

		// explosion
		CreateZombExplosion(pos, BOOMER_EXPLOSION_INNER_RADIUS, BOOMER_EXPLOSION_OUTER_RADIUS, 30.f, dmg,
							zombCID);
		// amplify sound a bit
		GameServer()->CreateSound(pos, SOUND_GRENADE_EXPLODE, -1, Server()->MainMapID);
		GameServer()->CreateSound(pos, SOUND_GRENADE_EXPLODE, -1, Server()->MainMapID);
		GameServer()->CreateSound(pos, SOUND_GRENADE_EXPLODE, -1, Server()->MainMapID);
	}

	// start the fuse
	if(m_ZombExplodeClock[zid] < 0 && targetDist < BOOMER_EXPLOSION_INNER_RADIUS && targetLOS) {
		m_ZombExplodeClock[zid] = SecondsToTick(0.35f);
		m_ZombInput[zid].m_Hook = 0; // stop hooking to give the survivor a chance to escape
		m_ZombHookClock[zid] = SecondsToTick(1.f);
		// send some love
		GameServer()->SendEmoticon(zombCID, 2);
		ChangeEyes(zid, EMOTE_HAPPY, 3.f);
	}
}

void CGameControllerZOMB::HandleTank(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	if(m_ZombCharCore[zid].m_HookState == HOOK_GRABBED && targetDist > 80.0f)
	{
		float GrabProgress = m_ZombHookGrabClock[zid]/(float)g_ZombGrabLimit[ZTYPE_TANK];
		float s = fabs(sinf(GrabProgress * pi * 1.0f));

		if(s >= 0.6f) {
			m_ZombDestination[zid] = m_ZombCharCore[zid].m_Pos - (targetPos - m_ZombCharCore[zid].m_Pos);
			m_ZombPathFindClock[zid] = 2;
			ChangeEyes(zid, EMOTE_ANGRY, 0.5f);

			vec2 pullDir = normalize(m_ZombCharCore[zid].m_Pos - targetPos);
			CCharacterCore* pTargetCore = GameServer()->m_World.m_Core.m_apCharacters[m_ZombSurvTarget[zid]];
			if(pTargetCore) {
				pTargetCore->m_Vel += pullDir * TANK_PULL_STR;
			}
		}
		else {
			ChangeEyes(zid, EMOTE_NORMAL, 1.f);
		}
	}
}

void CGameControllerZOMB::HandleBull(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	const vec2& pos = m_ZombCharCore[zid].m_Pos;
	i32 zombCID = ZombCID(zid);

	// CHARRRGE
	if(m_ZombChargeClock[zid] <= 0 && targetDist > 150.f && targetLOS && targetDist < 500.f) {
		m_ZombChargeClock[zid] = BULL_CHARGE_CD;
		f32 chargeDist = targetDist * 1.5f; // overcharge a bit
		m_ZombChargingClock[zid] = SecondsToTick(chargeDist/BULL_CHARGE_SPEED);
		m_ZombChargeVel[zid] = normalize(targetPos - pos) * (BULL_CHARGE_SPEED/SERVER_TICK_SPEED);

		m_ZombAttackTick[zid] = m_Tick; // ninja attack tick

		GameServer()->CreateSound(pos, SOUND_PLAYER_SKID, -1, Server()->MainMapID);
		GameServer()->SendEmoticon(zombCID, 1); // exclamation
		mem_zero(m_ZombChargeHit[zid], sizeof(bool) * MAX_CLIENTS);
	}
	else if(targetLOS && targetDist <= 150.f) {
		// back up a bit
		m_ZombDestination[zid] = m_ZombCharCore[zid].m_Pos - (targetPos - m_ZombCharCore[zid].m_Pos);
		m_ZombPathFindClock[zid] = SecondsToTick(0.25f);
	}

	if(m_ZombChargingClock[zid] > 0) {
		m_ZombActiveWeapon[zid] = WEAPON_NINJA;
		m_ZombChargeClock[zid] = BULL_CHARGE_CD; // don't count cd while charging

		// look where it is charging
		m_ZombInput[zid].m_TargetX = m_ZombChargeVel[zid].x * 10.f;
		m_ZombInput[zid].m_TargetY = m_ZombChargeVel[zid].y * 10.f;
		m_ZombInput[zid].m_Hook = 0;

		i32 dmg = g_ZombAttackDmg[m_ZombType[zid]];
		if(m_ZombBuff[zid]&BUFF_ELITE) {
			dmg *= ELITE_DMG_MULTIPLIER;
		}

		f32 checkRadius = 100.f;
		f32 chargeSpeed = length(m_ZombChargeVel[zid]);
		u32 checkCount = maximum(1, (i32)(chargeSpeed / checkRadius));

		// as it is moving quite fast, check several times along path if needed
		for(u32 c = 0; c < checkCount; ++c) {
			vec2 checkPos = pos + ((m_ZombChargeVel[zid] / (f32)checkCount) * (f32)c);

			CCharacter *apEnts[MAX_SURVIVORS];
			i32 count = GameServer()->m_World.FindEntities(checkPos, checkRadius,
														   (CEntity**)apEnts, MAX_SURVIVORS,
														   CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);
			for(i32 s = 0; s < count; ++s) {
				i32 cid = apEnts[s]->GetPlayer()->GetCID();

				// hit only once during charge
				if(m_ZombChargeHit[zid][cid]) {
					continue;
				}
				m_ZombChargeHit[zid][cid] = true;

				vec2 n = normalize(m_ZombChargeVel[zid]);
				n.y -= 0.5f;

				GameServer()->CreateHammerHit(apEnts[s]->GetPos(), Server()->MainMapID);
				apEnts[s]->TakeDamage(n * BULL_KNOCKBACK_FORCE, n, dmg, zombCID, WEAPON_HAMMER);
				CCharacterCore* pCore = GameServer()->m_World.m_Core.m_apCharacters[cid];
				if(pCore) {
					pCore->m_HookState = HOOK_RETRACTED;
				}
			}

			// knockback zombies
			for(u32 z = 0; z < MAX_ZOMBS; ++z) {
				if(!m_ZombAlive[z] || z == zid) continue;

				// hit only once during charge
				i32 cid = ZombCID(z);
				if(m_ZombChargeHit[zid][cid]) {
					continue;
				}
				m_ZombChargeHit[zid][cid] = true;


				vec2 d = m_ZombCharCore[z].m_Pos - checkPos;
				f32 l = length(d);
				if(l > checkRadius) {
					continue;
				}
				vec2 n = normalize(d);

				m_ZombCharCore[z].m_Vel += n * BULL_KNOCKBACK_FORCE *
						g_ZombKnockbackMultiplier[m_ZombType[z]];
				m_ZombHookClock[z] = SecondsToTick(1);
				m_ZombInput[z].m_Hook = 0;
			}

			// it hit something, reset attack time so it doesn't attack when
			// bumping with charge
			if(count > 0) {
				m_ZombAttackClock[zid] = SecondsToTick(1.f / g_ZombAttackSpeed[m_ZombType[zid]]);
				m_ZombInput[zid].m_Hook = 0;
				m_ZombHookClock[zid] = SecondsToTick(0.25f);
				m_ZombChargingClock[zid] = 1; // stop on hit
			}
		}
	}
	else {
		m_ZombActiveWeapon[zid] = WEAPON_HAMMER;
	}
}

void CGameControllerZOMB::HandleMudge(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	if(targetLOS && m_ZombHookClock[zid] <= 0 && targetDist < g_ZombHookRange[ZTYPE_MUDGE] && m_ZombCharCore[zid].m_HookState != HOOK_GRABBED) {
		m_ZombCharCore[zid].m_HookState = HOOK_GRABBED;
		m_ZombCharCore[zid].m_HookedPlayer = m_ZombSurvTarget[zid];
		GameServer()->CreateSound(targetPos, SOUND_HOOK_ATTACH_PLAYER, -1, Server()->MainMapID);
		m_ZombInput[zid].m_Hook = 1;
	}

	// stop moving once it grabs onto someone
	// and pulls a bit stronger
	if(m_ZombCharCore[zid].m_HookState == HOOK_GRABBED) {
		m_ZombInput[zid].m_Jump = 0;
		m_ZombInput[zid].m_Direction = 0;
		vec2 pullDir = normalize(m_ZombCharCore[zid].m_Pos - targetPos);
		CCharacterCore* pTargetCore = GameServer()->m_World.m_Core.m_apCharacters[m_ZombSurvTarget[zid]];
		if(pTargetCore) {
			pTargetCore->m_Vel += pullDir * MUDGE_PULL_STR;
		}
	}
}

void CGameControllerZOMB::HandleHunter(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	const vec2& pos = m_ZombCharCore[zid].m_Pos;
	i32 zombCID = ZombCID(zid);

	// CHARRRGE
	if(m_ZombChargeClock[zid] <= 0 && targetDist < 500.f && targetLOS) {
		m_ZombChargeClock[zid] = HUNTER_CHARGE_CD;
		f32 chargeDist = targetDist * 1.5f; // overcharge a bit
		m_ZombChargingClock[zid] = SecondsToTick(chargeDist/HUNTER_CHARGE_SPEED);
		m_ZombChargeVel[zid] = normalize(targetPos - pos) * (HUNTER_CHARGE_SPEED/SERVER_TICK_SPEED);

		m_ZombAttackTick[zid] = m_Tick; // ninja attack tick

		GameServer()->CreateSound(pos, SOUND_NINJA_FIRE, -1, Server()->MainMapID);
		//GameServer()->SendEmoticon(zombCID, 10);
		mem_zero(m_ZombChargeHit[zid], sizeof(bool) * MAX_CLIENTS);
		m_ZombInvisClock[zid] = SecondsToTick(2.0f);
	}

	if(m_ZombChargingClock[zid] > 0) {
		m_ZombActiveWeapon[zid] = WEAPON_NINJA;
		m_ZombChargeClock[zid] = HUNTER_CHARGE_CD; // don't count cd while charging

		f32 checkRadius = 28.f * 2.f;
		f32 chargeSpeed = length(m_ZombChargeVel[zid]);
		u32 checkCount = maximum(1, (i32)(chargeSpeed / checkRadius));

		// as it is moving quite fast, check several times along path if needed
		for(u32 c = 0; c < checkCount; ++c) {
			vec2 checkPos = pos + ((m_ZombChargeVel[zid] / (f32)checkCount) * (f32)c);

			CCharacter *apEnts[MAX_SURVIVORS];
			i32 count = GameServer()->m_World.FindEntities(checkPos, checkRadius,
														   (CEntity**)apEnts, MAX_SURVIVORS,
														   CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);
			for(i32 s = 0; s < count; ++s) {
				i32 cid = apEnts[s]->GetPlayer()->GetCID();

				// hit only once during charge
				if(m_ZombChargeHit[zid][cid]) {
					continue;
				}
				m_ZombChargeHit[zid][cid] = true;

				GameServer()->CreateHammerHit(apEnts[s]->GetPos(), Server()->MainMapID);
				apEnts[s]->TakeDamage(vec2(0, 0), m_ZombChargeVel[zid], HUNTER_CHARGE_DMG, zombCID,
									  WEAPON_NINJA);
			}

			// it hit something, reset attack time so it doesn't attack when
			// hitting with charge
			if(count > 0) {
				m_ZombAttackClock[zid] = SecondsToTick(1.f / g_ZombAttackSpeed[m_ZombType[zid]]);
				GameServer()->CreateSound(pos, SOUND_NINJA_HIT, -1, Server()->MainMapID);
			}
		}
	}
	else {
		m_ZombActiveWeapon[zid] = WEAPON_HAMMER;
	}

	if(targetDist < 100.0f) {
		m_ZombInvisClock[zid] = SecondsToTick(1.0f);
	}

	m_ZombInvisClock[zid]--;
	m_ZombInvisible[zid] = m_ZombInvisClock[zid] <= 0;
}

void CGameControllerZOMB::HandleDominant(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	m_ZombInput[zid].m_Hook = 0; // dominants don't hook
	m_ZombActiveWeapon[zid] = WEAPON_LASER;

	// pew pew
	if(targetDist > 56.f && targetLOS && targetDist < 400.f) {
		if(m_ZombAttackClock[zid] <= 0) {
			GameServer()->CreateSound(m_ZombCharCore[zid].m_Pos, SOUND_LASER_FIRE, -1, Server()->MainMapID);
			CreateLaser(m_ZombCharCore[zid].m_Pos, targetPos);

			i32 dmg = g_ZombAttackDmg[m_ZombType[zid]];
			if(m_ZombBuff[zid]&BUFF_ELITE) {
				dmg *= ELITE_DMG_MULTIPLIER;
			}

			GameServer()->GetPlayerChar(m_ZombSurvTarget[zid])->TakeDamage(vec2(0,0), vec2(0,1),
				dmg, ZombCID(zid), WEAPON_LASER);

			m_ZombAttackClock[zid] = SecondsToTick(1.f / g_ZombAttackSpeed[m_ZombType[zid]]);
		}
	}
}

void CGameControllerZOMB::HandleBerserker(u32 zid)
{
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(i == zid || !m_ZombAlive[i] || m_ZombBuff[i]&BUFF_ENRAGED) {
			continue;
		}

		// enrage nearby zombies
		if(distance(m_ZombCharCore[zid].m_Pos, m_ZombCharCore[i].m_Pos) < BERSERKER_ENRAGE_RADIUS) {
			m_ZombEnrageClock[i] = 1;
		}
	}
}

void CGameControllerZOMB::HandleWartule(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS)
{
	m_ZombInput[zid].m_Hook = 0; // wartules don't hook
	m_ZombActiveWeapon[zid] = WEAPON_GRENADE;

	if(targetDist < 350.f && targetLOS) {
		// flee! :s (for 1 s)
		m_ZombDestination[zid].x = m_ZombCharCore[zid].m_Pos.x - (targetPos.x - m_ZombCharCore[zid].m_Pos.x);
		m_ZombPathFindClock[zid] = SecondsToTick(1.f);
		ChangeEyes(zid, EMOTE_PAIN, 1.f);
	}

	// throw grenades at people
	if(targetDist > 80.f && targetLOS) {
		if(m_ZombAttackClock[zid] <= 0 &&
		   (m_ZombPathFindClock[zid] > SecondsToTick(0.25f) || targetDist < 400.f)) { // when feeing and when close enough
			i32 dmg = WARTULE_GRENADE_DMG;
			if(m_ZombBuff[zid]&BUFF_ELITE) {
				dmg *= ELITE_DMG_MULTIPLIER;
			}

			f32 shootAngle = angle(targetPos - m_ZombCharCore[zid].m_Pos);
			shootAngle += randi(&m_Seed, -15, 15) * pi / 180.f;
			vec2 shootDir = direction(shootAngle);

			GameServer()->CreateSound(m_ZombCharCore[zid].m_Pos, SOUND_GRENADE_FIRE, -1, Server()->MainMapID);
			CreateProjectile(m_ZombCharCore[zid].m_Pos, shootDir,
							 WEAPON_GRENADE, dmg, ZombCID(zid), SecondsToTick(0.3f));

			m_ZombLastShotDir[zid] = shootDir;
			m_ZombAttackClock[zid] = SecondsToTick(1.f / g_ZombAttackSpeed[m_ZombType[zid]]);
		}

		m_ZombInput[zid].m_TargetX = m_ZombLastShotDir[zid].x * 10.f;
		m_ZombInput[zid].m_TargetY = m_ZombLastShotDir[zid].y * 10.f;
	}
	else {
		m_ZombLastShotDir[zid] = targetPos - m_ZombCharCore[zid].m_Pos;
	}
}

void CGameControllerZOMB::TickZombies()
{
	/*m_DbgLinesCount = 0;
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		ivec2 Start(m_ZombCharCore[i].m_Pos.x/32, m_ZombCharCore[i].m_Pos.y/32);
		ivec2 End(m_ZombDestination[i].x/32, m_ZombDestination[i].y/32);
		DebugLine(Start, End);
	}*/

	// apply auras (could be optimized)
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		// remove auras
		m_ZombBuff[i] &= ~BUFF_HEALING;
		m_ZombBuff[i] &= ~BUFF_ARMORED;

		for(u32 CheckID = 0; CheckID < MAX_ZOMBS; ++CheckID) {
			if(i == CheckID) continue;

			if(m_ZombAlive[CheckID] &&
			   distance(m_ZombCharCore[i].m_Pos, m_ZombCharCore[CheckID].m_Pos) < AURA_RADIUS) {
				if(m_ZombType[CheckID] == ZTYPE_DOMINANT) {
					m_ZombBuff[i] |= BUFF_ARMORED;
				}
				else if(m_ZombType[CheckID] == ZTYPE_WARTULE) {
					m_ZombBuff[i] |= BUFF_HEALING;
				}
			}
		}
	}

	static i32 ZombStuckTicks[MAX_ZOMBS] = {0};
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(!m_ZombAlive[i]) {
			ZombStuckTicks[i] = 0;
			continue;
		}

		if(length(m_ZombCharCore[i].m_Vel) < 0.001f)
			ZombStuckTicks[i]++;
		else
			ZombStuckTicks[i] = 0;
	}

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(!m_ZombAlive[i]) continue;

		// clocks
		--m_ZombPathFindClock[i];
		--m_ZombJumpClock[i];
		--m_ZombAttackClock[i];
		--m_ZombEnrageClock[i];
		--m_ZombHookClock[i];
		--m_ZombChargeClock[i];
		--m_ZombHealClock[i];
		--m_ZombEyesClock[i];

		// enrage
		if(m_ZombEnrageClock[i] <= 0 && !(m_ZombBuff[i]&BUFF_ENRAGED)) {
			m_ZombBuff[i] |= BUFF_ENRAGED;
			for(u32 s = 0; s < MAX_SURVIVORS; ++s) {
				if(GameServer()->m_apPlayers[s] && Server()->ClientIngame(s)) {
					SendZombieInfos(i, s);
				}
			}
		}

		// double if enraged
		if(m_ZombBuff[i]&BUFF_ENRAGED) {
			--m_ZombAttackClock[i];
			--m_ZombHookClock[i];
			--m_ZombChargeClock[i];
		}

		// healing
		if(m_ZombBuff[i]&BUFF_HEALING && m_ZombHealClock[i] <= 0) {
			m_ZombHealClock[i] = HEALING_INTERVAL;
			m_ZombHealth[i] += HEALING_AMOUNT;
			i32 maxHealth = g_ZombMaxHealth[m_ZombType[i]];
			if(m_ZombBuff[i]&BUFF_ELITE) {
				maxHealth *= 2;
			}
			if(m_ZombHealth[i] > maxHealth) {
				m_ZombHealth[i] = maxHealth;
			}
		}

		vec2 pos = m_ZombCharCore[i].m_Pos;

		// find closest target
		f32 closestDist = -1.f;
		i32 survTargetID = NO_TARGET;
		for(u32 s = 0; s < MAX_SURVIVORS; ++s) {
			CCharacter* pSurvChar = GameServer()->GetPlayerChar(s);
			if(pSurvChar) {
				f32 dist = length(pos - pSurvChar->GetPos());
				if(closestDist < 0.f || dist < closestDist) {
					closestDist = dist;
					survTargetID = s;
				}
			}
		}

		// don't "hit" ninja dashing players
		if(survTargetID != NO_TARGET && IsPlayerNinjaDashing(survTargetID)) {
			survTargetID = NO_TARGET;
		}

		m_ZombSurvTarget[i] = survTargetID;

		if(m_ZombSurvTarget[i] == NO_TARGET) {
			continue;
		}


		vec2 targetPos = GameServer()->GetPlayerChar(m_ZombSurvTarget[i])->GetPos();
		f32 targetDist = distance(pos, targetPos);
		bool targetLOS = (GameServer()->Collision(Server()->MainMapID)->IntersectLine(pos, targetPos, 0, 0) == 0);

		HandleMovement(i, targetPos, targetLOS);

		if(ZombStuckTicks[i] > 15) {
			m_ZombInput[i].m_Jump = ZOMBIE_GROUNDJUMP;
		}

		m_ZombInput[i].m_TargetX = targetPos.x - pos.x;
		m_ZombInput[i].m_TargetY = targetPos.y - pos.y;

		// attack!
		if(m_ZombAttackClock[i] <= 0 && targetDist < 56.f) {
			i32 dmg = g_ZombAttackDmg[m_ZombType[i]];

			if(m_ZombBuff[i]&BUFF_ELITE) {
				dmg *= ELITE_DMG_MULTIPLIER;
			}

			SwingHammer(i, dmg, 2.f);
			m_ZombAttackClock[i] = SecondsToTick(1.f / g_ZombAttackSpeed[m_ZombType[i]]);
		}

		HandleHook(i, targetDist, targetLOS);

		// special behaviors
		switch(m_ZombType[i]) {
			case ZTYPE_BOOMER:
				HandleBoomer(i, targetDist, targetLOS);
				break;

			case ZTYPE_TANK:
				HandleTank(i, targetPos, targetDist, targetLOS);
				break;

			case ZTYPE_BULL:
				HandleBull(i, targetPos, targetDist, targetLOS);
				break;

			case ZTYPE_MUDGE:
				HandleMudge(i, targetPos, targetDist, targetLOS);
				break;

			case ZTYPE_HUNTER:
				HandleHunter(i, targetPos, targetDist, targetLOS);
				break;

			case ZTYPE_DOMINANT:
				HandleDominant(i, targetPos, targetDist, targetLOS);
				break;

			case ZTYPE_BERSERKER:
				HandleBerserker(i);
				break;

			case ZTYPE_WARTULE:
				HandleWartule(i, targetPos, targetDist, targetLOS);
				break;

			default: break;
		}
	}

	// core actual move
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(!m_ZombAlive[i]) continue;

		// smooth out small movement
		int SmoothTrigger = 0;
		const i32 PosTx = (i32)(m_ZombCharCore[i].m_Pos.x / 32);
		const i32 PosTy = (i32)(m_ZombCharCore[i].m_Pos.y / 32);
		const i32 DestTx = (i32)(m_ZombDestination[i].x / 32);
		const i32 DestTy = (i32)(m_ZombDestination[i].y / 32);
		if((m_Tick%SERVER_TICK_SPEED != 0) && abs(DestTy - PosTy) > 1 && PosTx == DestTx &&
		   fabs(m_ZombCharCore[i].m_Pos.x - m_ZombDestination[i].x) < 4) {
			m_ZombInput[i].m_Direction = 0;
			//SmoothTrigger = COREEVENTFLAG_AIR_JUMP; // TODO: remove
		}

		if(m_ZombChargingClock[i] > 0) {
			--m_ZombChargingClock[i];

			// reset input except from firing
			i32 doFire = m_ZombInput[i].m_Fire;
			m_ZombInput[i] = CNetObj_PlayerInput();
			m_ZombInput[i].m_Fire = doFire;

			// don't move naturally
			m_ZombCharCore[i].m_Vel = vec2(0, 0);

			vec2 chargeVel = m_ZombChargeVel[i];
			GameServer()->Collision(Server()->MainMapID)->MoveBox(&m_ZombCharCore[i].m_Pos,
											   &chargeVel,
											   vec2(28.f, 28.f),
											   0.f);
		}

		m_ZombCharCore[i].m_Input = m_ZombInput[i];

		// handle jump
		m_ZombCharCore[i].m_Jumped = 0;
		i32 jumpTriggers = 0;
		if(m_ZombCharCore[i].m_Input.m_Jump) {
			f32 jumpStr = 14.f;
			if(m_ZombCharCore[i].m_Input.m_Jump == ZOMBIE_BIGJUMP) {
				jumpStr = 22.f;
				//jumpTriggers |= COREEVENTFLAG_AIR_JUMP;
				m_ZombCharCore[i].m_Jumped |= 3;
			}
			else if(m_ZombCharCore[i].m_Input.m_Jump == ZOMBIE_AIRJUMP) {
				//jumpTriggers |= COREEVENTFLAG_AIR_JUMP;
				m_ZombCharCore[i].m_Jumped |= 3;
			}
			else {
				jumpTriggers |= COREEVENTFLAG_GROUND_JUMP;
				m_ZombCharCore[i].m_Jumped |= 1;
			}

			m_ZombCharCore[i].m_Vel.y = -jumpStr;
			m_ZombCharCore[i].m_Input.m_Jump = 0;
		}

		m_ZombCharCore[i].Tick(true);
		m_ZombCharCore[i].m_TriggeredEvents |= jumpTriggers|SmoothTrigger;
		m_ZombCharCore[i].Move();

		// predict charge
		if(m_ZombChargingClock[i] > 0) {
			m_ZombCharCore[i].m_Vel = m_ZombChargeVel[i];
		}
	}

	// check if character cores overlap
	const bool PlayerCollision = GameServer()->m_World.m_Core.m_Tuning.m_PlayerCollision;
	CCharacterCore** apCharacters = GameServer()->m_World.m_Core.m_apCharacters;
	u32 Seed = m_Tick;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		CCharacterCore *pCharCore = apCharacters[i];
		if(!pCharCore)
			continue;

		for(int j = 0; j < MAX_CLIENTS; j++)
		{

			CCharacterCore *pCharCore2 = apCharacters[j];
			if(!pCharCore2 || pCharCore == pCharCore2)
				continue;

			// handle player <-> player collision
			float Distance = distance(pCharCore->m_Pos, pCharCore2->m_Pos);
			vec2 Dir = normalize(pCharCore->m_Pos - pCharCore2->m_Pos);

			if(PlayerCollision && Distance < 0.0001f)
			{
				float a = randf01(&Seed) * 2 * pi;
				vec2 Dir(cosf(a), sinf(a));
				pCharCore->m_Vel += Dir*10;
				pCharCore2->m_Vel -= Dir*10;
			}
		}
	}
}

void CGameControllerZOMB::ConZombStart(IConsole::IResult* pResult, void* pUserData)
{
	CGameControllerZOMB *pThis = (CGameControllerZOMB *)pUserData;

	i32 startingWave = 0;
	if(pResult->NumArguments()) {
		startingWave = clamp(pResult->GetInteger(0), 0, (int)(pThis->m_WaveCount)-1);
	}

	pThis->StartZombGame(startingWave);
}

void CGameControllerZOMB::StartZombGame(u32 startingWave)
{
	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		if(GameServer()->m_apPlayers[i]) {
			GameServer()->m_apPlayers[i]->m_Score = 0;
		}
	}

	m_CurrentWave = startingWave-1; // will be increased to one after wait time
	m_SpawnCmdID = 0;
	m_SpawnClock = 0;
	m_WaveWaitClock = WAVE_WAIT_TIME;
	AnnounceWave(startingWave);
	ChatMessage(">> 10s to setup.");
	DoWarmup(0);
	m_ZombGameState = ZSTATE_WAVE_GAME;
	m_CanPlayersRespawn = true;

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(m_ZombAlive[i]) {
			KillZombie(i, -1);
		}
	}

	m_Seed = time(NULL);
	m_RestartClock = -1;
}

void CGameControllerZOMB::WaveGameWon()
{
	ChatMessage(">> You won, good job!");
	BroadcastMessage("^090You won, good job!");
	EndMatch();
	GameCleanUp();
}

void CGameControllerZOMB::GameLost(bool AllowRestart)
{
	ChatMessage(">> You LOST.");

	if(m_ZombGameState == ZSTATE_WAVE_GAME) {
		char msgBuff[256];
		str_format(msgBuff, sizeof(msgBuff), ">> You survived until Wave %d, good job!",
				   m_CurrentWave+1);
		ChatMessage(msgBuff);
		str_format(msgBuff, sizeof(msgBuff), "You survived until ^090Wave %d^999, good job!",
				   m_CurrentWave+1);
		BroadcastMessage(msgBuff);
	}

	if(m_ZombGameState == ZSTATE_SURV_GAME) {
		i32 t = (m_Tick - m_GameStartTick)/SERVER_TICK_SPEED;
		i32 min = t / 60;
		i32 sec = t % 60;
		char msgBuff[256];
		str_format(msgBuff, sizeof(msgBuff), ">> You survived %d min %d sec, good job! [%s difficulty]",
				   min, sec, g_SurvDiffString[m_SurvDifficulty]);
		ChatMessage(msgBuff);

		const char* aSurvDiffColor[] = {
			"090",
			"990",
			"900"
		};
		str_format(msgBuff, sizeof(msgBuff), "You survived ^090%d min %d sec^999, good job! ^%s[%s difficulty]",
				   min, sec, aSurvDiffColor[m_SurvDifficulty],
				   g_SurvDiffString[m_SurvDifficulty]);
		BroadcastMessage(msgBuff);
	}

	EndMatch();

	m_ZombLastGameState = m_ZombGameState;

	GameCleanUp();

	if(AllowRestart && Config()->m_SvZombAutoRestart > 0) {
		m_RestartClock = SecondsToTick(clamp(Config()->m_SvZombAutoRestart, 10, 600));
		char restartMsg[128];
		str_format(restartMsg, sizeof(restartMsg), "Game restarting in %ds...", Config()->m_SvZombAutoRestart);
		ChatMessage(restartMsg);
	}
}

void CGameControllerZOMB::GameCleanUp()
{
	m_ZombGameState = ZSTATE_NONE;
	m_IsReviveCtfActive = false;

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(m_ZombAlive[i]) {
			KillZombie(i, -1);
		}
	}

	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		if(GameServer()->m_apPlayers[i]) {
			GameServer()->m_apPlayers[i]->m_RespawnDisabled = false;
			GameServer()->m_apPlayers[i]->m_DeadSpecMode = false;
		}
	}

	m_LaserCount = 0;
	m_ProjectileCount = 0;
}

void CGameControllerZOMB::ChatMessage(const char* msg, int CID)
{
	CNetMsg_Sv_Chat chatMsg;
	chatMsg.m_Mode = CHAT_ALL;
	chatMsg.m_ClientID = -1;
	chatMsg.m_TargetID = -1;
	chatMsg.m_pMessage = msg;
	Server()->SendPackMsg(&chatMsg, MSGFLAG_VITAL, CID);
}

void CGameControllerZOMB::BroadcastMessage(const char* msg, int CID)
{
	CNetMsg_Sv_Broadcast Broadcast;
	Broadcast.m_pMessage = msg;
	Server()->SendPackMsg(&Broadcast, MSGFLAG_VITAL, CID);
}

void CGameControllerZOMB::AnnounceWave(u32 waveID)
{
	CNetMsg_Sv_Broadcast chatMsg;
	char msgBuff[256];
	str_format(msgBuff, sizeof(msgBuff), "WAVE ^980%d ^999(%d zombies)", waveID + 1,
			   m_WaveSpawnCount[waveID]);
	chatMsg.m_pMessage = msgBuff;
	Server()->SendPackMsg(&chatMsg, MSGFLAG_VITAL, -1);
}

void CGameControllerZOMB::AnnounceWaveCountdown(u32 waveID, f32 SecondCountdown)
{
	CNetMsg_Sv_Broadcast chatMsg;
	char msgBuff[256];
	str_format(msgBuff, sizeof(msgBuff), "%.1f\nWAVE ^980%d ^999(%d zombies)", SecondCountdown, waveID + 1,
			   m_WaveSpawnCount[waveID]);
	chatMsg.m_pMessage = msgBuff;
	Server()->SendPackMsg(&chatMsg, MSGFLAG_VITAL, -1);
}

void CGameControllerZOMB::TickWaveGame()
{
	// wait in between waves
	if(m_WaveWaitClock > 0) {
		--m_WaveWaitClock;
		if(m_WaveWaitClock < (WAVE_WAIT_TIME/2+1))
			AnnounceWaveCountdown(m_CurrentWave + 1, m_WaveWaitClock/(f32)SERVER_TICK_SPEED);

		// wave start
		if(m_WaveWaitClock == 0) {
			++m_CurrentWave;
			AnnounceWave(m_CurrentWave);
			m_SpawnClock = SPAWN_INTERVAL;
			m_WaveWaitClock = -1;
		}
	}
	else if(m_SpawnClock > 0) {
		--m_SpawnClock;

		if(m_SpawnClock == 0 && m_SpawnCmdID < m_WaveSpawnCount[m_CurrentWave]) {
			for(u32 i = 0; i < MAX_ZOMBS; ++i) {
				if(m_ZombAlive[i]) continue;
				u32 cmdID = m_SpawnCmdID++;
				SpawnCmd cmd = m_WaveData[m_CurrentWave][cmdID];
				const u8 type = cmd.type;
				bool isElite = cmd.isElite;
				bool isEnraged = cmd.isEnraged;
				SpawnZombie(i, type, isElite, isEnraged? 0:m_WaveEnrageTime[m_CurrentWave]);
				break;
			}

			m_SpawnClock = SPAWN_INTERVAL / m_ZombSpawnPointCount;
		}
	}

	// end of the wave
	if(m_WaveWaitClock == -1 && m_SpawnCmdID == m_WaveSpawnCount[m_CurrentWave]) {
		bool areZombsDead = true;
		for(u32 i = 0; i < MAX_ZOMBS; ++i) {
			if(m_ZombAlive[i]) {
				areZombsDead = false;
				break;
			}
		}

		if(areZombsDead) {
			ReviveSurvivors();
			m_WaveWaitClock = WAVE_WAIT_TIME;
			m_SpawnCmdID = 0;

			if(m_CurrentWave == (m_WaveCount-1)) {
				WaveGameWon();
			}
			else {
				char aBuff[128];
				str_format(aBuff, sizeof(aBuff), ">> Wave %d complete.", m_CurrentWave + 1);
				ChatMessage(aBuff);
				ChatMessage(">> 10s until next wave.");
				AnnounceWave(m_CurrentWave + 1);
			}
		}
	}
}

void CGameControllerZOMB::ActivateReviveCtf()
{
	if(m_RedFlagSpawnCount < 1 || m_BlueFlagSpawnCount < 1)
	{
		std_zomb_msg("WARNING: put red & blue flags in your map for revive ctf to be activated!");

		if(IsEverySurvivorDead()) {
			GameLost();
		}
		return;
	}

	m_RedFlagPos = m_RedFlagSpawn[m_Tick%m_RedFlagSpawnCount];
	m_BlueFlagPos = m_BlueFlagSpawn[m_Tick%m_BlueFlagSpawnCount];
	m_RedFlagCarrier = -1;
	m_RedFlagVel = vec2(0, 0);
	GameServer()->SendGameMsg(GAMEMSG_CTF_GRAB, 0, -1);
	m_IsReviveCtfActive = true;
	m_CanPlayersRespawn = false;

	// if there are more than 2 people playing,
	// spawn flag on a random zombie
	u32 survCount = 0;
	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		if(GameServer()->m_apPlayers[i]) {
			++survCount;
		}
	}

	u32 choices[MAX_ZOMBS];
	u32 choiceCount = 0;
	if(survCount > 2) {
		for(u32 i = 0; i < MAX_ZOMBS; ++i) {
			if(m_ZombAlive[i]) {
				choices[choiceCount++] = i;
			}
		}

		if(choiceCount > 0) {
			u32 chosen = choices[m_Tick%choiceCount];
			m_RedFlagCarrier = ZombCID(chosen);
			m_RedFlagPos = m_ZombCharCore[chosen].m_Pos;
		}
	}
}

void CGameControllerZOMB::ReviveSurvivors()
{
	m_CanPlayersRespawn = true;

	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		CPlayer* pPlayer = GameServer()->m_apPlayers[i];
		if(pPlayer) {
			CCharacter* pChar = pPlayer->m_pCharacter;
			if(pChar && !pChar->IsAlive()) { // yes this can happen somehow...
				delete pPlayer->m_pCharacter;
				pPlayer->m_pCharacter = 0;
			}

			if(!pPlayer->m_pCharacter)
			{
				pPlayer->m_RespawnDisabled = false;
				pPlayer->m_DeadSpecMode = false;
				pPlayer->TryRespawn();
			}
		}
	}

	m_CanPlayersRespawn = false;
	m_IsReviveCtfActive = false;
}

enum {
	TK_INVALID = 0,
	TK_OPEN_BRACE,
	TK_CLOSE_BRACE,
	TK_COLON,
	TK_SEMICOLON,
	TK_UNDERSCORE,
	TK_COMMENT,
	TK_IDENTIFIER,
	TK_NUMBER,
	TK_EOS
};

struct Token
{
	const char* at;
	u32 length;
	u8 type;
};

struct Cursor
{
	const char* at;
	u32 line;
};

inline bool IsWhiteSpace(char c)
{
	return (c == ' ' ||
			c == '\n' ||
			c == '\t' ||
			c == '\r');
}

inline bool IsAlpha(char c)
{
	return ((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z'));
}

inline bool IsNumber(char c)
{
	return (c >= '0' && c <= '9');
}

static Token GetToken(Cursor* pCursor)
{
	while(*pCursor->at && IsWhiteSpace(*pCursor->at)) {
		if(*pCursor->at == '\n') {
			++pCursor->line;
		}
		++pCursor->at;
	}

	Token token;
	token.at = pCursor->at;
	token.length = 1;
	token.type = TK_INVALID;

	switch(*pCursor->at) {
		case '{': token.type = TK_OPEN_BRACE; break;
		case '}': token.type = TK_CLOSE_BRACE; break;
		case ':': token.type = TK_COLON; break;
		case ';': token.type = TK_SEMICOLON; break;
		case '_': token.type = TK_UNDERSCORE; break;
		case 0: token.type = TK_EOS; break;

		case '#': {
			token.type = TK_COMMENT;
			const char* ccur = pCursor->at+1;
			while(*ccur != 0 && *ccur != '\n') {
				++token.length;
				++ccur;
			}
		} break;

		default: {
			if(IsAlpha(*pCursor->at)) {
				token.type = TK_IDENTIFIER;
				const char* ccur = pCursor->at+1;
				while(*ccur != 0 && IsAlpha(*ccur)) {
					++token.length;
					++ccur;
				}
			}
			else if(IsNumber(*pCursor->at)) {
				token.type = TK_NUMBER;
				const char* ccur = pCursor->at+1;
				while(*ccur != 0 && IsNumber(*ccur)) {
					++token.length;
					++ccur;
				}
			}
		} break;
	}

	pCursor->at += token.length;
	return token;
}

static bool ParseZombDecl(Cursor* pCursor, u32* out_pCount, bool* out_pIsElite, bool* out_pIsEnraged)
{
	Token token = GetToken(pCursor);

	if(token.type == TK_UNDERSCORE) {
		token = GetToken(pCursor);
		// _elite
		if(token.type == TK_IDENTIFIER && token.length == 5
		   && str_comp_nocase_num(token.at, "elite", 5) == 0) {
			*out_pIsElite = true;
			token = GetToken(pCursor); // :
		}
		// _enraged
		else if(token.type == TK_IDENTIFIER && token.length == 7
		   && str_comp_nocase_num(token.at, "enraged", 7) == 0) {
			*out_pIsEnraged = true;
			token = GetToken(pCursor); // :
		}
		else {
			return false;
		}
	}

	// _enraged
	if(token.type == TK_UNDERSCORE) {
		token = GetToken(pCursor);
		if(token.type == TK_IDENTIFIER && token.length == 7
		   && str_comp_nocase_num(token.at, "enraged", 7) == 0) {
			*out_pIsEnraged = true;
			token = GetToken(pCursor); // :
		}
		else {
			return false;
		}
	}

	if(token.type != TK_COLON) {
		return false;
	}

	token = GetToken(pCursor);
	if(token.type != TK_NUMBER) {
		return false;
	}

	// parse count
	char numBuff[8];
	memcpy(numBuff, token.at, token.length);
	numBuff[token.length] = 0;
	*out_pCount = str_toint(numBuff);

	token = GetToken(pCursor);
	if(token.type != TK_SEMICOLON) {
		return false;
	}

	return true;
}

static bool ParseEnrageDecl(Cursor* pCursor, u32* out_pTime)
{
	Token token = GetToken(pCursor);

	if(token.type != TK_COLON) {
		return false;
	}

	token = GetToken(pCursor);
	if(token.type != TK_NUMBER) {
		return false;
	}

	// parse time
	char numBuff[8];
	memcpy(numBuff, token.at, token.length);
	numBuff[token.length] = 0;
	*out_pTime = str_toint(numBuff);

	token = GetToken(pCursor);
	if(token.type != TK_SEMICOLON) {
		return false;
	}

	return true;
}

bool CGameControllerZOMB::LoadWaveFile(const char* path)
{
	IOHANDLE WaveFile = m_pStorage->OpenFile(path, IOFLAG_READ, IStorage::TYPE_ALL);
	if(WaveFile)
	{
		u32 fileSize = io_length(WaveFile);
		char* fileContents = (char*)mem_alloc(fileSize+1);
		io_read(WaveFile, fileContents, fileSize);
		io_close(WaveFile);
		fileContents[fileSize] = 0;

		if(!ParseWaveFile(fileContents)) {
			mem_free(fileContents);
			return false;
		}

		mem_free(fileContents);
		return true;
	}

	std_zomb_msg("Error: could not open '%s'", path);

	return false;
}

bool CGameControllerZOMB::ParseWaveFile(const char* pBuff)
{
	SpawnCmd waveData[MAX_WAVES][MAX_SPAWN_QUEUE];
	u32 waveSpawnCount[MAX_WAVES];
	u32 waveEnrageTime[MAX_WAVES];
	u32 waveCount = 0;

	bool parsing = true;
	Cursor cursor;
	cursor.at = pBuff;
	cursor.line = 1;

	bool insideWave = false;
	i32 waveId = -1;

	while(parsing) {
		Token token = GetToken(&cursor);

		switch(token.type) {
			case TK_OPEN_BRACE: {
				//dbg_zomb_msg("{");
				if(!insideWave && waveCount < MAX_WAVES) {
					waveId = waveCount++;
					waveEnrageTime[waveId] = DEFAULT_ENRAGE_TIME;
					waveSpawnCount[waveId] = 0;
					insideWave = true;
				}
				else {
					std_zomb_msg("Error: wave parsing error: { not closed (line: %d)", cursor.line);
					return false;
				}
			} break;

			case TK_CLOSE_BRACE: {
				//dbg_zomb_msg("}");
				if(insideWave) {
					if(waveSpawnCount[waveId] == 0) {
						std_zomb_msg("Error: wave parsing error: wave %d is empty (line: %d)", waveId, cursor.line);
						return false;
					}

					insideWave = false;
				}
				else {
					std_zomb_msg("Error: wave parsing error: } not open (line: %d)", cursor.line);
					return false;
				}
			} break;

			case TK_IDENTIFIER: {
				//dbg_zomb_msg("%.*s", token.length, token.at);
				if(insideWave) {
					i32 zombType = -1;
					for(i32 i = 0; i < ZTYPE_MAX; ++i) {
						if(strlen(g_ZombName[i]) == token.length &&
						   str_comp_nocase_num(token.at, g_ZombName[i], token.length) == 0) {
							zombType = i;
							break;
						}
					}

					if(zombType != -1) {
						u32 count = 0;
						bool isElite = false;
						bool isEnraged = false;
						if(ParseZombDecl(&cursor, &count, &isElite, &isEnraged)) {
							if(waveSpawnCount[waveId]+count > MAX_SPAWN_QUEUE)
							{
								std_zomb_msg("Error: wave parsing error: near %.*s (line: %d), %d exceeds max number of zombies (%d)",
											 token.length, token.at, cursor.line, waveSpawnCount[waveId]+count, MAX_SPAWN_QUEUE);
							}

							// add to wave
							for(u32 c = 0; c < count; ++c) {
								SpawnCmd Cmd = {(u8)zombType, isElite, isEnraged};
								waveData[waveId][waveSpawnCount[waveId]++] = Cmd;
							}
						}
						else {
							std_zomb_msg("Error: wave parsing error: near %.*s (format is type[_elite][_enraged]: count;) (line: %d)",
										 token.length, token.at, cursor.line);
							return false;
						}
					}
					else if(token.length == 6 &&
							str_comp_nocase_num(token.at, "enrage", token.length) == 0) {
						u32 enrageTime = 0;
						if(ParseEnrageDecl(&cursor, &enrageTime)) {
							waveEnrageTime[waveId] = enrageTime;
						}
						else {
							std_zomb_msg("Error: wave parsing error: near %.*s (format is enrage: time;) (line: %d)",
										 token.length, token.at, cursor.line);
							return false;
						}

					}
					else {
						std_zomb_msg("Error: wave parsing error: near %.*s (unknown identifer) (line: %d)",
									 token.length, token.at, cursor.line);
						return false;
					}
				}
				else {
					std_zomb_msg("Error: wave parsing error: identifier outside wave block (line: %d)", cursor.line);
					return false;
				}
			} break;

			case TK_EOS: {
				parsing = false;
			} break;
		}
	}

	if(waveCount == 0) {
		std_zomb_msg("Error: wave parsing error: no waves declared");
		return false;
	}

	memcpy(m_WaveData, waveData, sizeof(m_WaveData));
	memcpy(m_WaveSpawnCount, waveSpawnCount, sizeof(m_WaveSpawnCount));
	memcpy(m_WaveEnrageTime, waveEnrageTime, sizeof(m_WaveEnrageTime));
	m_WaveCount = waveCount;

	return true;
}

void CGameControllerZOMB::LoadDefaultWaves()
{
	static const char* defaultWavesFile = MULTILINE(
	{
		zombie: 10;
		wartule: 1;
		zombie: 5;
	}

	{
		zombie: 5;
		boomer: 1;
		mudge: 1;
		zombie: 5;
	}

	{
		zombie: 4;
		bull: 1;
		hunter: 1;
		zombie: 6;
	}

	{
		zombie: 10;
		tank: 1;
	}

	{
		zombie: 5;
		boomer: 2;
		bull: 1;
		zombie: 5;
		mudge: 1;
		zombie: 5;
	}

	{
		hunter: 2;
		zombie: 10;
		berserker: 1;
		zombie: 10;
	}

	{
		bull: 3;
		zombie: 12;
		mudge: 1;
		wartule: 1;
		zombie: 12;
	}

	{
		zombie: 10;
		dominant: 1;
		hunter: 3;
		boomer_elite: 1;
		zombie: 30;
	}

	{
		zombie_elite: 2;
		boomer: 1;
		wartule: 1;
		bull: 2;
		zombie: 20;
	}

	{
		tank: 1;
		zombie_elite: 10;
		berserker: 1;
		zombie: 5;
		dominant: 1;
		zombie: 15;
	}

	{
		zombie_elite: 5;
		bull_elite: 1;
		boomer_elite: 1;
		wartule_enraged: 1;
		zombie: 20;
	});

	bool Result = ParseWaveFile(defaultWavesFile);
	dbg_assert(Result, "Error parsing default waves");
}

void CGameControllerZOMB::ConZombLoadWaveFile(IConsole::IResult* pResult, void* pUserData)
{
//	CGameControllerZOMB *pThis = (CGameControllerZOMB *)pUserData;
//
//	if(pResult->NumArguments()) {
//		const char* rStr = pResult->GetString(0);
//		if(pThis->LoadWaveFile(rStr)) {
//			pThis->ChatMessage("-- New waves successfully loaded.");
//			memcpy(Config()->m_SvZombWaveFile, rStr, minimum(256, (i32)strlen(rStr)));
//		}
//	}
}

void CGameControllerZOMB::CreateLaser(vec2 from, vec2 to)
{
	u32 id = m_LaserCount++;
	m_LaserList[id].to = to;
	m_LaserList[id].from = from;
	m_LaserList[id].tick = m_Tick;
	m_LaserList[id].id = m_LaserID++;
}

void CGameControllerZOMB::CreateProjectile(vec2 pos, vec2 dir, i32 type, i32 dmg, i32 owner, i32 lifespan)
{
	i32 id = m_ProjectileCount++;
	m_ProjectileList[id].id = m_ProjectileID++;
	m_ProjectileList[id].startPos = pos;
	m_ProjectileList[id].startTick = m_Tick;
	m_ProjectileList[id].lifespan = lifespan;
	m_ProjectileList[id].dir = dir;
	m_ProjectileList[id].type = type;
	m_ProjectileList[id].dmg = dmg;
	m_ProjectileList[id].ownerCID = owner;

	switch(type)
	{
		case WEAPON_GRENADE:
			m_ProjectileList[id].curvature = GameServer()->Tuning()->m_GrenadeCurvature;
			m_ProjectileList[id].speed = GameServer()->Tuning()->m_GrenadeSpeed;
			break;

		case WEAPON_SHOTGUN:
			m_ProjectileList[id].curvature = GameServer()->Tuning()->m_ShotgunCurvature;
			m_ProjectileList[id].speed = GameServer()->Tuning()->m_ShotgunSpeed;
			break;

		case WEAPON_GUN:
			m_ProjectileList[id].curvature = GameServer()->Tuning()->m_GunCurvature;
			m_ProjectileList[id].speed = GameServer()->Tuning()->m_GunSpeed;
			break;
	}
}

void CGameControllerZOMB::TickProjectiles()
{
	for(i32 i = 0; i < m_ProjectileCount; ++i) {
		f32 pt = (m_Tick - m_ProjectileList[i].startTick - 1)/(f32)SERVER_TICK_SPEED;
		f32 ct = (m_Tick - m_ProjectileList[i].startTick)/(f32)SERVER_TICK_SPEED;
		vec2 prevPos = CalcPos(m_ProjectileList[i].startPos, m_ProjectileList[i].dir,
							   m_ProjectileList[i].curvature, m_ProjectileList[i].speed, pt);
		vec2 curPos = CalcPos(m_ProjectileList[i].startPos, m_ProjectileList[i].dir,
							  m_ProjectileList[i].curvature, m_ProjectileList[i].speed, ct);

		if(m_ProjectileList[i].lifespan-- <= 0) {
			if(m_ProjectileList[i].type == WEAPON_GRENADE) {
				CreateZombExplosion(curPos, 48.f, 135.f, 12.f, m_ProjectileList[i].dmg,
								m_ProjectileList[i].ownerCID);
			}

			// delete
			if(m_ProjectileCount > 1) {
				m_ProjectileList[i] = m_ProjectileList[m_ProjectileCount - 1];
				i--;
			}
			--m_ProjectileCount;
			continue;
		}

		bool collided = (GameServer()->Collision(Server()->MainMapID)->IntersectLine(prevPos, curPos, &curPos, 0) != 0);

		// out of bounds
		i32 rx = round_to_int(curPos.x) / 32;
		i32 ry = round_to_int(curPos.y) / 32;
		if(rx < -200 || rx >= (i32)m_MapWidth+200 ||
		   ry < -200 || ry >= (i32)m_MapHeight+200) {

			// delete
			if(m_ProjectileCount > 1) {
				m_ProjectileList[i] = m_ProjectileList[m_ProjectileCount - 1];
				i--;
			}
			--m_ProjectileCount;
			continue;
		}


		if(collided && m_ProjectileList[i].type == WEAPON_GRENADE) {
			CreateZombExplosion(curPos, 48.f, 135.f, 12.f, m_ProjectileList[i].dmg,
							m_ProjectileList[i].ownerCID);
			// delete
			if(m_ProjectileCount > 1) {
				m_ProjectileList[i] = m_ProjectileList[m_ProjectileCount - 1];
				i--;
			}
			--m_ProjectileCount;
			continue;
		}


		CCharacter* pTargetChr = GameServer()->m_World.IntersectCharacter(prevPos, curPos, 6.0f, curPos, 0);
		if(pTargetChr) {
			if(m_ProjectileList[i].type == WEAPON_GRENADE) {
				CreateZombExplosion(curPos, 48.f, 135.f, 12.f, m_ProjectileList[i].dmg,
								m_ProjectileList[i].ownerCID);

			}
			// delete
			if(m_ProjectileCount > 1) {
				m_ProjectileList[i] = m_ProjectileList[m_ProjectileCount - 1];
				i--;
			}
			--m_ProjectileCount;
			continue;
		}
	}

	// TODO: this can happen somehow, fix it someday
	if(m_ProjectileCount < 0)
		m_ProjectileCount = 0;
}

void CGameControllerZOMB::CreateZombExplosion(vec2 pos, f32 inner, f32 outer, f32 force, i32 dmg, i32 ownerCID)
{
	GameServer()->CreateExplosion(pos, ownerCID, 0, 0, Server()->MainMapID);
	GameServer()->CreateSound(pos, SOUND_GRENADE_EXPLODE, -1, Server()->MainMapID);

	CCharacter *apEnts[MAX_SURVIVORS];
	i32 count = GameServer()->m_World.FindEntities(pos, outer, (CEntity**)apEnts, MAX_SURVIVORS,
												   CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);

	f32 radiusDiff = outer - inner;

	for(i32 s = 0; s < count; ++s) {
		vec2 d = apEnts[s]->GetPos() - pos;
		vec2 n = normalize(d);
		f32 l = length(d);
		f32 factor = 0.2f;
		if(l < inner) {
			factor = 1.f;
		}
		else {
			l -= inner;
			factor = maximum(0.2f, l / radiusDiff);
		}

		apEnts[s]->TakeDamage(n * force * factor, n, (i32)(dmg * factor),
				ownerCID, WEAPON_GRENADE);

		u32 cid = apEnts[s]->GetPlayer()->GetCID();
		CCharacterCore* pCore = GameServer()->m_World.m_Core.m_apCharacters[cid];
		if(pCore) {
			pCore->m_HookState = HOOK_RETRACTED;
		}
	}

	// knockback zombies
	for(u32 z = 0; z < MAX_ZOMBS; ++z) {
		if(!m_ZombAlive[z]) continue;

		vec2 d = m_ZombCharCore[z].m_Pos - pos;
		f32 l = length(d);
		if(l > outer) {
			continue;
		}

		vec2 n = normalize(d);

		f32 factor = 0.2f;
		if(l < inner) {
			factor = 1.f;
		}
		else {
			l -= inner;
			factor = maximum(0.2f, l / radiusDiff);
		}

		m_ZombCharCore[z].m_Vel += n * force * factor *
				g_ZombKnockbackMultiplier[m_ZombType[z]];
		m_ZombHookClock[z] = SecondsToTick(1);
		m_ZombInput[z].m_Hook = 0;
	}
}

void CGameControllerZOMB::CreatePlayerExplosion(vec2 Pos, i32 Dmg, i32 OwnerCID, i32 Weapon)
{
	GameServer()->CreateExplosion(Pos, OwnerCID, WEAPON_GRENADE,
								  g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, Server()->MainMapID);
	GameServer()->CreateSound(Pos, SOUND_GRENADE_EXPLODE, -1, Server()->MainMapID);

	float Radius = g_pData->m_Explosion.m_Radius;
	float InnerRadius = 48.0f;
	float MaxForce = g_pData->m_Explosion.m_MaxForce;

	for(int i = 0; i < MAX_CLIENTS; ++i) {
		CCharacterCore* pCore = GameServer()->m_World.m_Core.m_apCharacters[i];

		if(pCore && CharIsZombie(i) &&
		   distance(pCore->m_Pos, Pos) < (Radius + CCharacter::ms_PhysSize)) {
			vec2 Diff = pCore->m_Pos - Pos;
			vec2 Force(0, MaxForce);
			float l = length(Diff);
			if(l)
				Force = normalize(Diff) * MaxForce;
			float Factor = 1 - clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);

			ZombTakeDmg(i, Force * Factor, (int)(Factor * Dmg), OwnerCID, Weapon);
		}
	}
}

void CGameControllerZOMB::ChangeEyes(i32 zid, i32 type, f32 time)
{
	m_ZombEyes[zid] = type;
	m_ZombEyesClock[zid] = SecondsToTick(time);
}

void CGameControllerZOMB::ConZombStartSurv(IConsole::IResult* pResult, void* pUserData)
{
	CGameControllerZOMB *pThis = (CGameControllerZOMB *)pUserData;
	pThis->StartZombSurv(pResult->NumArguments() > 0 ? pResult->GetInteger(0) : -1);
}

void CGameControllerZOMB::StartZombSurv(i32 seed)
{
	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		if(GameServer()->m_apPlayers[i]) {
			GameServer()->m_apPlayers[i]->m_Score = 0;
		}
	}

	m_SurvDifficulty = Config()->m_SvZombSurvivalDifficulty;
	switch(m_SurvDifficulty)
	{
		case 2:
			m_SurvWaveInterval = SURVIVAL_START_WAVE_INTERVAL_HARD;
			m_SurvMaxTime = SURVIVAL_MAX_TIME_HARD;
			break;

		case 1:
			m_SurvWaveInterval = SURVIVAL_START_WAVE_INTERVAL_NORMAL;
			m_SurvMaxTime = SURVIVAL_MAX_TIME_NORMAL;
			break;

		case 0:
		default:
			m_SurvWaveInterval = SURVIVAL_START_WAVE_INTERVAL_EASY;
			m_SurvMaxTime = SURVIVAL_MAX_TIME_EASY;
			break;
	}

	m_SurvQueueCount = 0;
	m_SpawnClock = SecondsToTick(10); // 10s to setup
	m_SurvivalStartTick = m_Tick;

	char aMsg[128];
	str_format(aMsg, sizeof(aMsg), ">> Survive! (%s)", g_SurvDiffString[m_SurvDifficulty]);
	ChatMessage(aMsg);
	str_format(aMsg, sizeof(aMsg), "^900Survive! (%s)", g_SurvDiffString[m_SurvDifficulty]);
	BroadcastMessage(aMsg);
	ChatMessage(">> 10s to setup.");
	DoWarmup(0);
	m_ZombGameState = ZSTATE_SURV_GAME;
	m_CanPlayersRespawn = true;

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(m_ZombAlive[i]) {
			KillZombie(i, -1);
		}
	}

	if(seed == -1) {
		m_Seed = time(NULL);
	}
	else {
		m_Seed = seed;
	}

	m_RestartClock = -1;
}

void CGameControllerZOMB::TickSurvivalGame()
{
	const i32 POCKET_COUNT = MAX_ZOMBS / 3;

	const f64 progress = minimum(1.0, (m_Tick - m_SurvivalStartTick)/(f64)m_SurvMaxTime);
	dbg_assert(progress >= 0.0 && progress <= 1.0, "");

	if(m_SurvQueueCount == 0) {
		const i32 specialMaxCount = maximum(1, (i32)(progress * POCKET_COUNT));
		i32 specialsToSpawn = 0;

		if(specialMaxCount > 0) {
			specialsToSpawn = randi(&m_Seed, specialMaxCount/2, specialMaxCount);

			dbg_zomb_msg("specialMaxCount=%d specialsToSpawn=%d", specialMaxCount, specialsToSpawn);

			for(i32 s = 0; s < specialsToSpawn; s++) {
				f32 n = randf01(&m_Seed);
				dbg_assert(n >= 0.0f && n <= 1.0f, "");
				n *= g_ZombSpawnChanceTotal;

				// select roulette
				f32 r = 0.0f;
				u8 ztype = ZTYPE_BASIC;
				for(i32 i = 1; i < ZTYPE_MAX; i++) {
					r += g_ZombSpawnChance[i];
					if(n < r) {
						ztype = i;
						break;
					}
				}
				dbg_assert(ztype != ZTYPE_BASIC, "Should not happen");
				u8 elite = randf01(&m_Seed) < maximum(0.05, maximum(progress-0.5, 0.0) * 2.0);
				u8 enraged = randf01(&m_Seed) < maximum(0.05, progress * 0.2); // 5% -> 20%

				dbg_zomb_msg("#%d type=%d elite=%d enraged=%d", s, ztype, elite, enraged);
				SpawnCmd Cmd = {ztype, elite, enraged};
				m_SurvQueue[m_SurvQueueCount++] = Cmd;
			}
		}

		const i32 basicToSpawn = POCKET_COUNT - specialsToSpawn;
		for(i32 s = 0; s < basicToSpawn; s++) {
			u8 elite = randf01(&m_Seed) < maximum(0.025, maximum(progress-0.5, 0.0) * 2.0);
			u8 enraged = randf01(&m_Seed) < maximum(0.1, progress * 0.4); // 10% -> 40%
			SpawnCmd Cmd = {ZTYPE_BASIC, elite, enraged};
			m_SurvQueue[m_SurvQueueCount++] = Cmd;
		}

		dbg_zomb_msg("progress=%g basic=%d", progress, basicToSpawn);
	}

	if(m_SpawnClock > 0) {
		--m_SpawnClock;
	}

	i32 spots = 0;
	for(i32 i = 0; i < MAX_ZOMBS; ++i) {
		if(m_ZombAlive[i]) continue;
		spots++;
	}

	if(m_SpawnClock == 0 && spots >= POCKET_COUNT) {
		dbg_zomb_msg("spawning spots=%d", spots);
		spots = minimum(POCKET_COUNT, spots);

		for(i32 i = 0; i < MAX_ZOMBS && spots > 0; ++i) {
			if(m_ZombAlive[i]) continue;
			u8 type = m_SurvQueue[0].type;
			bool isElite = m_SurvQueue[0].isElite;
			bool isEnraged = m_SurvQueue[0].isEnraged;
			SpawnZombie(i, type, isElite, isEnraged? 0:SURVIVAL_ENRAGE_TIME);
			memmove(m_SurvQueue, m_SurvQueue+1, m_SurvQueueCount * sizeof(SpawnCmd));
			--m_SurvQueueCount;
			--spots;
		}

		m_SpawnClock = mix(m_SurvWaveInterval, 0, progress * progress);
	}
}

void CGameControllerZOMB::TickReviveCtf()
{
	if(IsEverySurvivorDead()) {
		GameLost();
		return;
	}

	if(m_RedFlagCarrier == -1) {
		m_RedFlagVel.y += GameServer()->Tuning()->m_Gravity;
		GameServer()->Collision(Server()->MainMapID)->MoveBox(&m_RedFlagPos,
										   &m_RedFlagVel,
										   vec2(28.f, 28.f),
										   0.f);

		CCharacter *apEnts[MAX_SURVIVORS];
		i32 count = GameServer()->m_World.FindEntities(m_RedFlagPos, 56.f,
													   (CEntity**)apEnts, MAX_SURVIVORS,
													   CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);

		for(i32 i = 0; i < count; ++i) {
			m_RedFlagCarrier = apEnts[i]->GetPlayer()->GetCID();
			m_RedFlagVel = vec2(0, 0);
			m_RedFlagPos = apEnts[i]->GetPos();
			GameServer()->SendGameMsg(GAMEMSG_CTF_GRAB, 1, -1);
			break;
		}

	}
	else {
		if(CharIsSurvivor(m_RedFlagCarrier)) {
			m_RedFlagPos = GameServer()->GetPlayerChar(m_RedFlagCarrier)->GetPos();
		}
		else {
			m_RedFlagPos = m_ZombCharCore[m_RedFlagCarrier - MAX_SURVIVORS].m_Pos;
		}

		m_RedFlagVel = vec2(0, 0);
	}

	// capture
	if(distance(m_RedFlagPos, m_BlueFlagPos) < 100.f && CharIsSurvivor(m_RedFlagCarrier)) {
		GameServer()->CreateSound(m_RedFlagPos, SOUND_CTF_CAPTURE, -1, Server()->MainMapID);
		GameServer()->SendGameMsg(GAMEMSG_CTF_CAPTURE, 0, m_RedFlagCarrier, 0 /*time*/, -1); //TODO: add time
		ReviveSurvivors();
	}
}

#ifdef CONF_DEBUG
void CGameControllerZOMB::DebugPathAddPoint(ivec2 p)
{
	if(m_DbgPathLen >= 256) return;
	m_DbgPath[m_DbgPathLen++] = p;
}

void CGameControllerZOMB::DebugLine(ivec2 s, ivec2 e)
{
	if(m_DbgLinesCount >= 256) m_DbgLinesCount = 0;
	u32 id = m_DbgLinesCount++;
	m_DbgLines[id].start = s;
	m_DbgLines[id].end = e;
}
#endif

CGameControllerZOMB::CGameControllerZOMB(CGameContext *pGameServer, IStorage* pStorage)
: IGameController(pGameServer)
{
	m_pStorage = pStorage;
	m_pGameType = "ZOMB";
	m_ZombSpawnPointCount = 0;
	m_SurvSpawnPointCount = 0;
	m_Seed = 1337;
	m_GameFlags = 0;

	m_GameInfo.m_ScoreLimit = 0;
	Config()->m_SvWarmup = 0;
    Config()->m_SvScorelimit = 0;
    Config()->m_SvPlayerSlots = MAX_SURVIVORS;

	// get map info
	memcpy(m_MapName, Config()->m_SvMap, sizeof(Config()->m_SvMap));
	mem_zero(m_Map, sizeof(m_Map));
	m_MapCrc = reinterpret_cast<IEngineMap*>(GameServer()->m_vLayers[Server()->MainMapID].Map())->Crc();
	CMapItemLayerTilemap* pGameLayer = GameServer()->m_vLayers[Server()->MainMapID].GameLayer();
	m_MapWidth = pGameLayer->m_Width;
	m_MapHeight = pGameLayer->m_Height;
	CTile* pTiles = (CTile*)(GameServer()->m_vLayers[Server()->MainMapID].Map()->GetData(pGameLayer->m_Data));
	int count = m_MapWidth * m_MapHeight;
	dbg_assert(count <= MAX_MAP_SIZE, "map too big");
	if(count > MAX_MAP_SIZE) {
		std_zomb_msg("WARNING: map too big (%d > %d)", count, MAX_MAP_SIZE);
	}

	for(int i = 0; i < count && i < MAX_MAP_SIZE; ++i) {
		const int Index = pTiles[i].m_Index;
		if(Index <= 128 && Index&CCollision::COLFLAG_SOLID) {
			m_Map[i] = 1;
		}
	}

	// init zombies
	mem_zero(m_ZombAlive, sizeof(m_ZombAlive));
	mem_zero(m_ZombAttackTick, sizeof(m_ZombAttackTick));
	mem_zero(m_ZombPathFindClock, sizeof(m_ZombPathFindClock));

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		m_ZombCharCore[i].Init(&GameServer()->m_World.m_Core, GameServer()->Collision(Server()->MainMapID));
		m_ZombType[i] = ZTYPE_BASIC;
	}

	GameServer()->Console()->Register("zomb_start", "?i", CFGFLAG_SERVER, ConZombStart,
									  this, "Start a ZOMB game");
	GameServer()->Console()->Register("zomb_load", "s", CFGFLAG_SERVER, ConZombLoadWaveFile,
									  this, "Load a ZOMB wave file");
	GameServer()->Console()->Register("zomb_surv", "?i", CFGFLAG_SERVER, ConZombStartSurv,
									  this, "Start a ZOMB survival game");

	m_ZombGameState = ZSTATE_NONE;
	m_ZombLastGameState = ZSTATE_WAVE_GAME;
	m_RestartClock = -1;

	m_RedFlagSpawnCount = 0;
	m_BlueFlagSpawnCount = 0;
	m_IsReviveCtfActive = false;
	m_CanPlayersRespawn = true;

	if(Config()->m_SvZombWaveFile[0] && LoadWaveFile(Config()->m_SvZombWaveFile)) {
		std_zomb_msg("'%s' wave file loaded (%d waves).",
                     Config()->m_SvZombWaveFile, m_WaveCount);
	}
	else {
		std_zomb_msg("loading default waves.");
		LoadDefaultWaves();
	}

	GameServer()->Console()->ExecuteLine("add_vote \"Start WAVE gamemode\" zomb_start");
	GameServer()->Console()->ExecuteLine("add_vote \"Start SURVIVAL gamemode\" zomb_surv");

	m_LaserCount = 0;
	m_LaserID = 512;
	m_ProjectileCount = 0;
	m_ProjectileID = 512;

#ifdef CONF_DEBUG
	m_DbgPathLen = 0;
	m_DbgLinesCount = 0;
#endif

	g_ZombSpawnChanceTotal = 0.0f;
	for(i32 i = 0; i < ZTYPE_MAX; i++) {
		g_ZombSpawnChanceTotal += g_ZombSpawnChance[i];
	}
}

void CGameControllerZOMB::Tick()
{
	m_Tick = Server()->Tick();
	IGameController::Tick();

	// move all valid players to the correct team
	for(u32 i = 0; i < MAX_SURVIVORS; ++i) {
		CPlayer* pPlayer = GameServer()->m_apPlayers[i];
		if(pPlayer && pPlayer->GetTeam() != TEAM_RED) {
			DoTeamChange(pPlayer, TEAM_RED, false);
			if(m_ZombGameState != ZSTATE_NONE)
				pPlayer->m_RespawnDisabled = true;
		}
	}

	if(m_RestartClock > 0) {
		--m_RestartClock;
		if(m_RestartClock < SecondsToTick(5))
		{
			char aBuff[64];
			str_format(aBuff, sizeof(aBuff), "Auto-restarting in %ds", m_RestartClock/SERVER_TICK_SPEED);
			BroadcastMessage(aBuff);
		}
		if(m_RestartClock == 0) {
			if(m_ZombLastGameState == ZSTATE_WAVE_GAME) {
				StartZombGame(0);
			}
			else if(m_ZombLastGameState == ZSTATE_SURV_GAME) {
				StartZombSurv(-1);
			}
			return;
		}
	}

	if(GameServer()->m_World.m_Paused) {
		return;
	}

	// lasers
	for(i32 i = 0; i < (i32)m_LaserCount; ++i) {
		if(m_Tick > (m_LaserList[i].tick + SecondsToTick(0.4f))) {
			if(m_LaserCount > 1) {
				m_LaserList[i] = m_LaserList[m_LaserCount - 1];
			}
			--m_LaserCount;
			--i;
		}
	}

	TickProjectiles();

	if(m_ZombGameState == ZSTATE_WAVE_GAME) {
		TickWaveGame();
	}
	else if(m_ZombGameState == ZSTATE_SURV_GAME) {
		TickSurvivalGame();
	}

	TickZombies();

	if(m_ZombGameState != ZSTATE_NONE)
	{
		bool everyoneDisconnected = true;
		for(u32 i = 0; i < MAX_SURVIVORS && everyoneDisconnected; ++i) {
			if(GameServer()->m_apPlayers[i]) {
				everyoneDisconnected = false;
			}
		}

		if(everyoneDisconnected) {
			m_RestartClock = -1;
			GameLost(false);
			return;
		}
	}

	if(m_IsReviveCtfActive && m_ZombGameState != ZSTATE_NONE) {
		TickReviveCtf();
	}

	static i32 aLastSurvDanger[MAX_SURVIVORS] = {0};
	for(i32 i = 0; i < MAX_SURVIVORS; i++)
	{
		const CCharacter* pChar = GameServer()->GetPlayerChar(i);
		if(pChar && pChar->IsAlive())
		{
			const f32 HealthDanger = (pChar->m_Health * 3 + pChar->m_Armor) / 4.0f;
			if(aLastSurvDanger[i] == 0 && HealthDanger < 5)
			{
				aLastSurvDanger[i] = 1;
				for(i32 s = 0; s < MAX_SURVIVORS; s++)
				{
					SendSurvivorStatus(i, s, SURV_STATUS_LOW_HEALTH);
				}
			}
			else if(aLastSurvDanger[i] == 1 && HealthDanger >= 5)
			{
				aLastSurvDanger[i] = 0;
				for(i32 s = 0; s < MAX_SURVIVORS; s++)
				{
					SendSurvivorStatus(i, s, SURV_STATUS_OK);
				}
			}
		}
	}
}


inline bool NetworkClipped(vec2 viewPos, vec2 checkPos)
{
	f32 dx = viewPos.x-checkPos.x;
	f32 dy = viewPos.y-checkPos.y;

	if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f ||
	   distance(viewPos, checkPos) > 1100.0f) {
		return true;
	}
	return false;
}

void CGameControllerZOMB::Snap(i32 SnappingClientID)
{
	if(!GameServer()->m_apPlayers[SnappingClientID] ||
	   !CharIsSurvivor(SnappingClientID)) {
		return;
	}

	IGameController::Snap(SnappingClientID);

	const vec2& viewPos = GameServer()->m_apPlayers[SnappingClientID]->m_ViewPos;

	// send zombie player and character infos
	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		if(!m_ZombAlive[i] || m_ZombInvisible[i]) {
			continue;
		}

		u32 zombCID = ZombCID(i);

		CNetObj_PlayerInfo *pPlayerInfo = (CNetObj_PlayerInfo *)Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO,
												zombCID, sizeof(CNetObj_PlayerInfo));
		if(!pPlayerInfo) {
			dbg_zomb_msg("Error: failed to SnapNewItem(NETOBJTYPE_PLAYERINFO)");
			return;
		}

		pPlayerInfo->m_PlayerFlags = PLAYERFLAG_READY;
		pPlayerInfo->m_Latency = -zombCID;
		pPlayerInfo->m_Score = -zombCID;

		if(NetworkClipped(viewPos, m_ZombCharCore[i].m_Pos)) {
			continue;
		}

		CNetObj_Character *pCharacter = (CNetObj_Character *)Server()->SnapNewItem(NETOBJTYPE_CHARACTER,
													zombCID, sizeof(CNetObj_Character));
		if(!pCharacter) {
			dbg_zomb_msg("Error: failed to SnapNewItem(NETOBJTYPE_CHARACTER)");
			return;
		}

		pCharacter->m_Tick = m_Tick;

		// eyes
		pCharacter->m_Emote = EMOTE_NORMAL;
		if(m_ZombBuff[i]&BUFF_ENRAGED) {
			pCharacter->m_Emote = EMOTE_SURPRISE;
		}
		// eye emote
		if(m_ZombEyesClock[i] > 0) {
			pCharacter->m_Emote = m_ZombEyes[i];
		}

		pCharacter->m_TriggeredEvents = m_ZombCharCore[i].m_TriggeredEvents;
		pCharacter->m_Weapon = m_ZombActiveWeapon[i];
		pCharacter->m_AttackTick = m_ZombAttackTick[i];

		m_ZombCharCore[i].Write(pCharacter);

		i32 xOffset = 0.f;
		if(m_ZombBuff[i]&BUFF_HEALING && m_ZombBuff[i]&BUFF_ARMORED) {
			xOffset = 16.f;
		}

		// buffs
		if(m_ZombBuff[i]&BUFF_HEALING) {
			CNetObj_Pickup *pPickup = (CNetObj_Pickup *)Server()->SnapNewItem(NETOBJTYPE_PICKUP,
											16256+zombCID, sizeof(CNetObj_Pickup));
			if(!pPickup)
				return;

			pPickup->m_X = m_ZombCharCore[i].m_Pos.x - xOffset;
			pPickup->m_Y = m_ZombCharCore[i].m_Pos.y - 72.f;
			pPickup->m_Type = PICKUP_HEALTH;
		}

		if(m_ZombBuff[i]&BUFF_ARMORED) {
			CNetObj_Pickup *pPickup = (CNetObj_Pickup *)Server()->SnapNewItem(NETOBJTYPE_PICKUP,
											16512+zombCID, sizeof(CNetObj_Pickup));
			if(!pPickup)
				return;

			pPickup->m_X = m_ZombCharCore[i].m_Pos.x + xOffset;
			pPickup->m_Y = m_ZombCharCore[i].m_Pos.y - 72.f;
			pPickup->m_Type = PICKUP_ARMOR;
		}
	}

	// lasers
	for(u32 i = 0; i < m_LaserCount; ++i) {
		if(NetworkClipped(viewPos, m_LaserList[i].from)) {
			continue;
		}

		CNetObj_Laser *pLaser = (CNetObj_Laser *)Server()->SnapNewItem(NETOBJTYPE_LASER, m_LaserList[i].id,
																	   sizeof(CNetObj_Laser));
		if(!pLaser)
			return;

		pLaser->m_X = m_LaserList[i].to.x;
		pLaser->m_Y = m_LaserList[i].to.y;
		pLaser->m_FromX = m_LaserList[i].from.x;
		pLaser->m_FromY = m_LaserList[i].from.y;
		pLaser->m_StartTick = m_LaserList[i].tick;
	}

	// projectiles
	for(i32 i = 0; i < m_ProjectileCount; ++i) {
		f32 ct = (m_Tick - m_ProjectileList[i].startTick) / (f32)SERVER_TICK_SPEED;
		vec2 projPos = CalcPos(m_ProjectileList[i].startPos,
							   m_ProjectileList[i].dir,
							   m_ProjectileList[i].curvature,
							   m_ProjectileList[i].speed,
							   ct);

		if(NetworkClipped(viewPos, projPos)) {
			continue;
		}

		CNetObj_Projectile *pProj = (CNetObj_Projectile *)Server()->SnapNewItem(NETOBJTYPE_PROJECTILE,
												m_ProjectileList[i].id, sizeof(CNetObj_Projectile));
		if(pProj) {
			pProj->m_X = m_ProjectileList[i].startPos.x;
			pProj->m_Y = m_ProjectileList[i].startPos.y;
			pProj->m_VelX = m_ProjectileList[i].dir.x * 100.0f;
			pProj->m_VelY = m_ProjectileList[i].dir.y * 100.0f;
			pProj->m_StartTick = m_ProjectileList[i].startTick;
			pProj->m_Type = m_ProjectileList[i].type;
		}
	}

	// revive ctf
	if(m_IsReviveCtfActive) {
		CNetObj_Flag *pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG,
																	TEAM_RED, sizeof(CNetObj_Flag));
		if(pFlag) {
			pFlag->m_X = m_RedFlagPos.x;
			pFlag->m_Y = m_RedFlagPos.y;
			pFlag->m_Team = TEAM_RED;
		}

		pFlag = (CNetObj_Flag *)Server()->SnapNewItem(NETOBJTYPE_FLAG,
													  TEAM_BLUE, sizeof(CNetObj_Flag));
		if(pFlag) {
			pFlag->m_X = m_BlueFlagPos.x;
			pFlag->m_Y = m_BlueFlagPos.y;
			pFlag->m_Team = TEAM_BLUE;
		}

		CNetObj_GameDataFlag *pGameDataFlag = (CNetObj_GameDataFlag *)
				Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag));
		if(pGameDataFlag) {
			pGameDataFlag->m_FlagCarrierBlue = FLAG_ATSTAND;
			if(m_RedFlagCarrier == -1) {
				pGameDataFlag->m_FlagCarrierRed = FLAG_ATSTAND;
			}
			else {
				pGameDataFlag->m_FlagCarrierRed = m_RedFlagCarrier;
			}
		}
	}

#ifdef CONF_DEBUG
	i32 tick = Server()->Tick();
	i32 laserID = 16000;

	// debug path
	for(i32 i = 1; i < m_DbgPathLen; ++i) {
		CNetObj_Laser *pObj = (CNetObj_Laser*)Server()->SnapNewItem(NETOBJTYPE_LASER,
									 laserID++, sizeof(CNetObj_Laser));
		if(!pObj)
			return;

		pObj->m_X = m_DbgPath[i-1].x * 32 + 16;
		pObj->m_Y = m_DbgPath[i-1].y * 32 + 16;
		pObj->m_FromX = m_DbgPath[i].x * 32 + 16;
		pObj->m_FromY = m_DbgPath[i].y * 32 + 16;
		pObj->m_StartTick = tick;
	}

	// debug lines
	for(i32 i = 0; i < m_DbgLinesCount; ++i) {
		CNetObj_Laser *pObj = (CNetObj_Laser*)Server()->SnapNewItem(NETOBJTYPE_LASER,
									 laserID++, sizeof(CNetObj_Laser));
		if(!pObj)
			return;

		pObj->m_X = m_DbgLines[i].start.x * 32 + 16;
		pObj->m_Y = m_DbgLines[i].start.y * 32 + 16;
		pObj->m_FromX = m_DbgLines[i].end.x * 32 + 16;
		pObj->m_FromY = m_DbgLines[i].end.y * 32 + 16;
		pObj->m_StartTick = tick;
	}

#if 0
	for(i32 y = 0; y < m_MapHeight; y++)
	{
		for(i32 x = 0; x < m_MapWidth; x++)
		{
			const int id = y * m_MapWidth + x;
			const vec2 Pos(x*32, y*32);
			if(!m_Map[id] || NetworkClipped(viewPos, Pos))
				continue;

			CNetObj_Pickup *pPickup = (CNetObj_Pickup *)Server()->SnapNewItem(NETOBJTYPE_PICKUP,
											1024+id, sizeof(CNetObj_Pickup));
			if(!pPickup)
				return;

			pPickup->m_X = Pos.x;
			pPickup->m_Y = Pos.y;
			pPickup->m_Type = PICKUP_ARMOR;
		}
	}
#endif

#endif
}

void CGameControllerZOMB::OnPlayerConnect(CPlayer* pPlayer)
{
	if(m_ZombGameState != ZSTATE_NONE) {
		PlayerActivateDeadSpectate(pPlayer);
	}
	else if(m_RestartClock < 0 && Config()->m_SvZombAutoStart > 0) {
		m_RestartClock = SecondsToTick(clamp(Config()->m_SvZombAutoStart, 10, 600));
	}

	IGameController::OnPlayerConnect(pPlayer);

	for(u32 i = 0; i < MAX_ZOMBS; ++i) {
		SendZombieInfos(i, pPlayer->GetCID());
	}

	const char* pWelcomeChatMessage = "Call a vote to start the game! Download the zombie skins here: bit.ly/zomb_skins";
	const char* pWelcomeBcMessage = "Call a vote to start the game!\\nDownload the zombie skins here: ^039bit.ly/zomb_skins";
	ChatMessage(pWelcomeChatMessage, pPlayer->GetCID());
	BroadcastMessage(pWelcomeBcMessage, pPlayer->GetCID());
}

bool CGameControllerZOMB::IsFriendlyFire(int ClientID1, int ClientID2) const
{
	if(ClientID1 == ClientID2) {
		return false;
	}

	// both survivors
	if(CharIsSurvivor(ClientID1) && CharIsSurvivor(ClientID2)) {
		return true;
	}

	// both zombies
	if(CharIsZombie(ClientID1) && CharIsZombie(ClientID2)) {
		return true;
	}

	return false;
}

bool CGameControllerZOMB::OnEntity(int Index, vec2 Pos)
{
	bool r = IGameController::OnEntity(Index, Pos, Server()->MainMapID);

	if(Index == ENTITY_SPAWN_BLUE || Index == ENTITY_SPAWN) {
		if(m_ZombSpawnPointCount < MAX_TEE_SPAWN_POINTS)
			m_ZombSpawnPoint[m_ZombSpawnPointCount++] = Pos;
	}
	if(Index == ENTITY_SPAWN_RED || Index == ENTITY_SPAWN) {
		if(m_SurvSpawnPointCount < MAX_TEE_SPAWN_POINTS)
			m_SurvSpawnPoint[m_SurvSpawnPointCount++] = Pos;
	}

	if(Index == ENTITY_FLAGSTAND_RED) {
		if(m_RedFlagSpawnCount < MAX_FLAG_SPAWN_POINTS)
			m_RedFlagSpawn[m_RedFlagSpawnCount++] = Pos;
	}
	else if(Index == ENTITY_FLAGSTAND_BLUE) {
		if(m_RedFlagSpawnCount < MAX_FLAG_SPAWN_POINTS)
			m_BlueFlagSpawn[m_BlueFlagSpawnCount++] = Pos;
	}

	return r;
}

bool CGameControllerZOMB::HasEnoughPlayers() const
{
	// get rid of that annoying warmup message
	return true;
}

bool CGameControllerZOMB::CanSpawn(int Team, vec2* pPos) const
{
	if(Team == TEAM_SPECTATORS) {
		return false;
	}

	if(!m_CanPlayersRespawn && m_ZombGameState != ZSTATE_NONE) {
		return false;
	}

	u32 chosen = (m_Tick%m_SurvSpawnPointCount);
	CCharacter *aEnts[MAX_SURVIVORS];
	for(u32 i = 0; i < m_SurvSpawnPointCount; ++i) {
		i32 count = GameServer()->m_World.FindEntities(m_SurvSpawnPoint[i], 28.f, (CEntity**)aEnts,
													   MAX_SURVIVORS, CGameWorld::ENTTYPE_CHARACTER, Server()->MainMapID);
		if(count == 0) {
			chosen = i;
			break;
		}
	}

	*pPos = m_SurvSpawnPoint[chosen];
	return true;
}

int CGameControllerZOMB::OnCharacterDeath(CCharacter* pVictim, CPlayer* pKiller, int Weapon)
{
	if(m_ZombGameState != ZSTATE_NONE) {
		PlayerActivateDeadSpectate(pVictim->GetPlayer());
	}

	if(!m_IsReviveCtfActive && m_ZombGameState != ZSTATE_NONE) {
		ActivateReviveCtf();
	}

	if(m_RedFlagCarrier == pVictim->GetPlayer()->GetCID()) {
		m_RedFlagCarrier = -1;
		GameServer()->SendGameMsg(GAMEMSG_CTF_DROP, -1);
	}

	return IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
}

bool CGameControllerZOMB::CanChangeTeam(CPlayer* pPlayer, int JoinTeam) const
{
	if(m_ZombGameState != ZSTATE_NONE) {
		return false;
	}
	return true;
}

void CGameControllerZOMB::OnReset()
{
	IGameController::OnReset();
}

void CGameControllerZOMB::ZombTakeDmg(i32 CID, vec2 Force, i32 Dmg, i32 From, i32 Weapon)
{
	u32 zid = CID - MAX_SURVIVORS; // TODO: remove

	// don't take damage from other zombies
	if(!m_ZombAlive[zid] || CharIsZombie(From) || Dmg <= 0) {
		return;
	}

	if(m_ZombBuff[zid]&BUFF_ARMORED) {
		Dmg *= (1.f - ARMOR_DMG_REDUCTION);
		if(Dmg < 1) {
			if(randf01(&m_Seed) < ARMOR_DMG_REDUCTION) {
				return;
			}
			Dmg = 1;
		}
	}

	// teammates can make the zombie stop hooking
	if(From >= 0 && From != m_ZombSurvTarget[zid])
		ZombStopHooking(zid);

	// do damage Hit sound
	if(From >= 0 && GameServer()->m_apPlayers[From]) {
		int Mask = CmaskOne(From);
		for(int i = 0; i < MAX_CLIENTS; i++) {
			if(GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||  GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
				GameServer()->m_apPlayers[i]->GetSpectatorID() == From)
				Mask |= CmaskOne(i);
		}
		GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask, Server()->MainMapID);
	}

	GameServer()->CreateDamage(m_ZombCharCore[zid].m_Pos, ZombCID(zid), vec2(0,1), Dmg, 0, false, Server()->MainMapID);

	m_ZombHealth[zid] -= Dmg;
	if(m_ZombHealth[zid] <= 0) {
		KillZombie(zid, -1);
		GameServer()->m_apPlayers[From]->m_Score++;
	}

	m_ZombCharCore[zid].m_Vel += Force * g_ZombKnockbackMultiplier[m_ZombType[zid]];

	ChangeEyes(zid, EMOTE_PAIN, 1.f);

	// reveal on damage
	if(m_ZombType[zid] == ZTYPE_HUNTER) {
		m_ZombInvisClock[zid] = SecondsToTick(1.0f);
	}
}

void CGameControllerZOMB::ZombStopHooking(i32 zid)
{
	if(m_ZombCharCore[zid].m_HookState >= HOOK_FLYING)
	{
		m_ZombCharCore[zid].m_HookState = HOOK_RETRACT_START;
		m_ZombInput[zid].m_Hook = 0;
		m_ZombHookClock[zid] = g_ZombHookCD[m_ZombType[zid]];
	}
}

void CGameControllerZOMB::PlayerFireShotgun(i32 CID, vec2 Pos, vec2 Direction)
{
	const vec2 ProjStartPos = Pos + Direction *28.f*0.75f;
	const i32 ShotSpread = 3;

	for(i32 i = -ShotSpread; i <= ShotSpread; ++i) {
		float a = angle(Direction);
		a += 0.08f * i;
		float v = 1-(absolute(i)/(float)ShotSpread);
		float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);

		new CProjectile(&GameServer()->m_World, WEAPON_SHOTGUN,
			CID,
			ProjStartPos,
			vec2(cosf(a), sinf(a))*Speed,
			(int)(Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
			1, false, 0, -1, WEAPON_SHOTGUN, Server()->MainMapID);
	}

	GameServer()->CreateSound(Pos, SOUND_SHOTGUN_FIRE, -1, Server()->MainMapID);
}

i32 CGameControllerZOMB::PlayerTryHitHammer(i32 CID, vec2 pos, vec2 direction)
{
	i32 hits = 0;
	const vec2 hitAreaCenter = pos;
	const f32 hitAreaRadius = CCharacter::ms_PhysSize * 3.0;

	for(int i = 0; i < MAX_ZOMBS; ++i) {
		if(!m_ZombAlive[i]) continue;
		const vec2& zombPos = m_ZombCharCore[i].m_Pos;

		if(distance(zombPos, hitAreaCenter) < hitAreaRadius) {
			GameServer()->CreateHammerHit(zombPos, Server()->MainMapID);

			vec2 dir;
			if(length(zombPos - pos) > 0.0f) {
				dir = normalize(zombPos - pos);
			}
			else {
				dir = vec2(0.f, -1.f);
			}

			i32 zid = ZombCID(i);
			ZombTakeDmg(ZombCID(i), dir * g_WeaponForceOnZomb[WEAPON_HAMMER],
						g_WeaponDmgOnZomb[WEAPON_HAMMER],
						CID, WEAPON_HAMMER);
			ZombStopHooking(zid);
			hits++;
		}
	}

	return hits;
}

i32 CGameControllerZOMB::IntersectCharacterCore(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos,
												CCharacterCore *pNotThis)
{
   float ClosestLen = distance(Pos0, Pos1) * 100.0f;
   int closest = -1;

   for(int i = 0; i < MAX_CLIENTS; ++i) {
	   CCharacterCore* p = GameServer()->m_World.m_Core.m_apCharacters[i];
	   if(!p || p == pNotThis)
		   continue;

	   vec2 IntersectPos = closest_point_on_line(Pos0, Pos1, p->m_Pos);
	   float Len = distance(p->m_Pos, IntersectPos);
	   if(Len < 28.f + Radius) {
		   Len = distance(Pos0, IntersectPos);
		   if(Len < ClosestLen) {
			   NewPos = IntersectPos;
			   ClosestLen = Len;
			   closest = i;
		   }
	   }
   }

   return closest;
}

bool CGameControllerZOMB::IsEverySurvivorDead() const
{
	bool everyoneDead = true;
	for(u32 i = 0; i < MAX_SURVIVORS && everyoneDead; ++i) {
		if(GameServer()->GetPlayerChar(i)) {
			everyoneDead = false;
		}
	}

	return everyoneDead;
}

bool CGameControllerZOMB::PlayerTryHitLaser(i32 CID, vec2 start, vec2 end, vec2& at)
{
	int hitCID = IntersectCharacterCore(start, end, 0.f, at,
					GameServer()->m_World.m_Core.m_apCharacters[CID]);
	if(hitCID == -1) {
		return false;
	}

	if(CharIsZombie(hitCID)) {
		ZombTakeDmg(hitCID, vec2(0.f, 0.f),
					g_WeaponDmgOnZomb[WEAPON_LASER],
					CID, WEAPON_LASER);
		return true;
	}

	return false;
}

bool CGameControllerZOMB::PlayerProjectileTick(i32 ownerCID, vec2 prevPos, vec2 curPos, i32 weapon,
											   vec2 dir, bool doDestroy)
{
	int hitCID = IntersectCharacterCore(prevPos, curPos, 6.0f, curPos,
										GameServer()->m_World.m_Core.m_apCharacters[ownerCID]);

	int rx = round_to_int(curPos.x) / 32;
	int ry = round_to_int(curPos.y) / 32;
	bool clipped = (rx < -200 || rx >= GameServer()->Collision(Server()->MainMapID)->GetWidth()+200)
			|| (ry < -200 || ry >= GameServer()->Collision(Server()->MainMapID)->GetHeight()+200);

	if(hitCID != -1 || doDestroy || clipped) {
		if(weapon == WEAPON_GRENADE) {
			CreatePlayerExplosion(curPos, g_WeaponDmgOnZomb[weapon], ownerCID, weapon);
		}
		else if(hitCID != -1 && CharIsZombie(hitCID)) {
			ZombTakeDmg(hitCID, dir * g_WeaponForceOnZomb[weapon], g_WeaponDmgOnZomb[weapon],
						ownerCID, weapon);
		}

		return true;
	}
	return false;
}

void CGameControllerZOMB::PlayerNinjaStartDashing(i32 SurvCID)
{
	dbg_assert(SurvCID >= 0 && SurvCID < MAX_SURVIVORS, "SurvCID is invalid");
	mem_zero(m_ZombGotHitByNinja[SurvCID], sizeof(m_ZombGotHitByNinja[SurvCID]));
}

void CGameControllerZOMB::PlayerNinjaHit(i32 SurvCID, vec2 Center, float Radius)
{
	dbg_assert(SurvCID >= 0 && SurvCID < MAX_SURVIVORS, "SurvCID is invalid");
	for(i32 i = 0; i < MAX_ZOMBS; i++)
	{
		if(!m_ZombAlive[i] || m_ZombGotHitByNinja[SurvCID][i]) continue;

		if(distance(Center, m_ZombCharCore[i].m_Pos) < Radius) {
			m_ZombGotHitByNinja[SurvCID][i] = 1;
			GameServer()->CreateSound(m_ZombCharCore[i].m_Pos, SOUND_NINJA_HIT, -1, Server()->MainMapID);
			ZombTakeDmg(ZombCID(i), vec2(0,0), g_WeaponDmgOnZomb[WEAPON_NINJA], SurvCID, WEAPON_NINJA);
		}
	}
}

bool CGameControllerZOMB::IsPlayerNinjaDashing(i32 SurvCID) const
{
	CCharacter* pChar = GameServer()->GetPlayerChar(SurvCID);
	return pChar && pChar->m_ActiveWeapon == WEAPON_NINJA && pChar->m_Ninja.m_CurrentMoveTime > 0;
}

void CGameControllerZOMB::PlayerActivateDeadSpectate(CPlayer* player)
{
	player->m_RespawnDisabled = true;

	for(i32 i = 0; i < MAX_SURVIVORS; i++) {
		CCharacter* chara = GameServer()->GetPlayerChar(i);
		if(chara && chara->IsAlive()) {
			player->SetSpectatorID(SPEC_PLAYER, i);
			break;
		}
	}
}
