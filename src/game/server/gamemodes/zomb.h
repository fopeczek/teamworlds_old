// LordSk
#pragma once
#include <game/server/gamecontroller.h>
#include <engine/console.h>
#include <game/gamecore.h>
#include <stdint.h>

#define ZOMB_VERSION "(3.2)"

#define MAX_SURVIVORS 4
#define MAX_ZOMBS (16 - MAX_SURVIVORS)
#define MAX_MAP_SIZE (1024 * 1024)

typedef uint8_t u8;
typedef int32_t i32;
typedef uint32_t u32;
typedef float f32;
typedef double f64;

class IStorage;

class CGameControllerZOMB : public IGameController
{
	enum {
		MAX_WAVES = 64,
		MAX_SPAWN_QUEUE = 1024,
		MAX_LASERS = 64,
		MAX_PROJECTILES = 256,
		MAX_TEE_SPAWN_POINTS = 32,
		MAX_FLAG_SPAWN_POINTS = 16,
	};

	friend class CPlayer;
	IStorage* m_pStorage;
	i32 m_Tick;
	u32 m_Seed;

	CCharacterCore m_ZombCharCore[MAX_ZOMBS];
	CNetObj_PlayerInput m_ZombInput[MAX_ZOMBS];
	i32 m_ZombActiveWeapon[MAX_ZOMBS];
	i32 m_ZombAttackTick[MAX_ZOMBS];

	bool m_ZombAlive[MAX_ZOMBS]; // bad design, but whatever
	i32 m_ZombHealth[MAX_ZOMBS];
	u8 m_ZombType[MAX_ZOMBS];
	u8 m_ZombBuff[MAX_ZOMBS];

	i32 m_ZombSurvTarget[MAX_ZOMBS];
	vec2 m_ZombDestination[MAX_ZOMBS];

	i32 m_ZombPathFindClock[MAX_ZOMBS];
	i32 m_ZombAttackClock[MAX_ZOMBS];
	i32 m_ZombJumpClock[MAX_ZOMBS];
	i32 m_ZombAirJumps[MAX_ZOMBS];
	i32 m_ZombEnrageClock[MAX_ZOMBS];
	i32 m_ZombHookClock[MAX_ZOMBS];
	i32 m_ZombHookGrabClock[MAX_ZOMBS];
	i32 m_ZombExplodeClock[MAX_ZOMBS];
	i32 m_ZombHealClock[MAX_ZOMBS];

	i32 m_ZombChargeClock[MAX_ZOMBS];
	i32 m_ZombChargingClock[MAX_ZOMBS];
	vec2 m_ZombChargeVel[MAX_ZOMBS];
	bool m_ZombChargeHit[MAX_ZOMBS][MAX_CLIENTS];

	vec2 m_ZombLastShotDir[MAX_ZOMBS];

	i32 m_ZombEyes[MAX_ZOMBS];
	i32 m_ZombEyesClock[MAX_ZOMBS];

	i32 m_ZombInvisClock[MAX_ZOMBS];
	bool m_ZombInvisible[MAX_ZOMBS];
	u8 m_ZombGotHitByNinja[MAX_SURVIVORS][MAX_ZOMBS];

	char m_MapName[128];
	u8 m_Map[MAX_MAP_SIZE];
	i32 m_MapWidth;
	i32 m_MapHeight;
	u32 m_MapCrc;

	vec2 m_ZombSpawnPoint[MAX_TEE_SPAWN_POINTS];
	vec2 m_SurvSpawnPoint[MAX_TEE_SPAWN_POINTS];
	u32 m_ZombSpawnPointCount;
	u32 m_SurvSpawnPointCount;

	u32 m_ZombGameState;
	u32 m_ZombLastGameState;

	// waves
	struct SpawnCmd {
		u8 type;
		u8 isElite;
		u8 isEnraged;
	};

	SpawnCmd m_WaveData[MAX_WAVES][MAX_SPAWN_QUEUE];
	u32 m_WaveSpawnCount[MAX_WAVES];
	u32 m_WaveEnrageTime[MAX_WAVES];
	u32 m_WaveCount;
	u32 m_CurrentWave;
	u32 m_SpawnCmdID;
	i32 m_SpawnClock;
	i32 m_WaveWaitClock;

	// revive ctf
	u32 m_BlueFlagSpawnCount;
	u32 m_RedFlagSpawnCount;
	vec2 m_BlueFlagSpawn[MAX_FLAG_SPAWN_POINTS];
	vec2 m_RedFlagSpawn[MAX_FLAG_SPAWN_POINTS];
	vec2 m_RedFlagPos;
	vec2 m_RedFlagVel;
	vec2 m_BlueFlagPos;
	i32 m_RedFlagCarrier;
	bool m_IsReviveCtfActive;
	bool m_CanPlayersRespawn;

	i32 m_RestartClock;

	SpawnCmd m_SurvQueue[MAX_SPAWN_QUEUE];
	i32 m_SurvQueueCount;
	i32 m_SurvivalStartTick;
	i32 m_SurvWaveInterval;
	i32 m_SurvMaxTime;
	i32 m_SurvDifficulty;

	// lasers
	struct Laser {
		vec2 from, to;
		i32 tick;
		u32 id;
	};

	Laser m_LaserList[MAX_LASERS];
	u32 m_LaserCount;
	u32 m_LaserID;

	// projectiles
	struct Projectile {
		vec2 startPos;
		vec2 dir;
		i32 startTick, lifespan;
		i32 type, dmg, ownerCID;
		f32 curvature, speed;
		u32 id;
	};

	Projectile m_ProjectileList[MAX_PROJECTILES];
	i32 m_ProjectileCount;
	i32 m_ProjectileID;

#ifdef CONF_DEBUG
	ivec2 m_DbgPath[256];
	i32 m_DbgPathLen;

	struct DbgLine {
		ivec2 start, end;
	};

	DbgLine m_DbgLines[256];
	i32 m_DbgLinesCount;

	void DebugPathAddPoint(ivec2 p);
	void DebugLine(ivec2 s, ivec2 e);
#endif

	void SpawnZombie(i32 zid, u8 type, u8 isElite, u32 enrageTime);
	void KillZombie(i32 zid, i32 killerCID);
	void SwingHammer(i32 zid, u32 dmg, f32 knockback);

	inline bool InMapBounds(const ivec2& pos);
	inline bool IsBlockedOrOob(const ivec2& pos);
	inline bool IsBlocked(const ivec2& pos);
	bool JumpStraight(const ivec2& start, const ivec2& dir, const ivec2& goal,
					  i32* out_pJumps);
	bool JumpDiagonal(const ivec2& start, const ivec2& dir, const ivec2& goal,
					  i32* out_pJumps);
	vec2 PathFind(vec2 start, vec2 end);

	void SendZombieInfos(i32 zid, i32 CID);
	void SendSurvivorStatus(i32 SurvCID, i32 CID, i32 Status);

	void HandleMovement(u32 zid, const vec2& targetPos, bool targetLOS);
	void HandleHook(u32 zid, f32 targetDist, bool targetLOS);
	void HandleBoomer(u32 zid, f32 targetDist, bool targetLOS);
	void HandleTank(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void HandleBull(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void HandleMudge(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void HandleHunter(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void HandleDominant(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void HandleBerserker(u32 zid);
	void HandleWartule(u32 zid, const vec2& targetPos, f32 targetDist, bool targetLOS);
	void TickZombies();

	static void ConZombStart(IConsole::IResult *pResult, void *pUserData);
	void StartZombGame(u32 startingWave = 0);
	void WaveGameWon();
	void SurvGameWon();
	void GameLost(bool AllowRestart = true);
	void GameCleanUp();
	void ChatMessage(const char* msg, int CID = -1);
	void BroadcastMessage(const char* msg, int CID = -1);
	void AnnounceWave(u32 waveID);
	void AnnounceWaveCountdown(u32 waveID, f32 SecondCountdown);
	void TickWaveGame();

	void ActivateReviveCtf();
	void ReviveSurvivors();
	void TickReviveCtf();

	bool LoadWaveFile(const char* path);
	bool ParseWaveFile(const char* pBuff);
	void LoadDefaultWaves();
    static void ConZombLoadWaveFile(IConsole::IResult *pResult, void *pUserData);

	void CreateLaser(vec2 from, vec2 to);
	void CreateProjectile(vec2 pos, vec2 dir, i32 type, i32 dmg, i32 owner, i32 lifespan);
	void TickProjectiles();
	void CreateZombExplosion(vec2 pos, f32 inner, f32 outer, f32 force, i32 dmg, i32 ownerCID);
	void CreatePlayerExplosion(vec2 Pos, i32 Dmg, i32 OwnerCID, i32 Weapon);

	void ChangeEyes(i32 zid, i32 type, f32 time);

	static void ConZombStartSurv(IConsole::IResult *pResult, void *pUserData);
	void StartZombSurv(i32 seed = -1);
	void TickSurvivalGame();

	i32 IntersectCharacterCore(vec2 Pos0, vec2 Pos1, float Radius, vec2& NewPos, CCharacterCore *pNotThis);

	bool IsEverySurvivorDead() const;

public:
	CGameControllerZOMB(class CGameContext *pGameServer, class IStorage* pStorage);
	void Tick();
	void Snap(i32 SnappingClientID);
	void OnPlayerConnect(CPlayer *pPlayer);
	bool IsFriendlyFire(int ClientID1, int ClientID2) const;
	bool OnEntity(int Index, vec2 Pos);
	bool HasEnoughPlayers() const;
	bool CanSpawn(int Team, vec2 *pPos) const;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	bool CanChangeTeam(CPlayer *pPlayer, int JoinTeam) const;
	void OnReset();

	void ZombTakeDmg(i32 CID, vec2 Force, i32 Dmg, i32 From, i32 Weapon);
	void ZombStopHooking(i32 zid);
	void PlayerFireShotgun(i32 CID, vec2 Pos, vec2 Direction);
	i32 PlayerTryHitHammer(i32 CID, vec2 pos, vec2 direction);
	bool PlayerTryHitLaser(i32 CID, vec2 start, vec2 end, vec2& at);
	bool PlayerProjectileTick(i32 ownerCID, vec2 prevPos, vec2 curPos, i32 weapon, vec2 dir, bool doDestroy);
	void PlayerNinjaStartDashing(i32 SurvCID);
	void PlayerNinjaHit(i32 SurvCID, vec2 Center, float Radius);
	bool IsPlayerNinjaDashing(i32 SurvCID) const;

	void PlayerActivateDeadSpectate(class CPlayer* player);
};

// == "ZOMB"
#define IsControllerZomb(GameServer) (*((uint32_t*)GameServer->GameType()) == 0x424D4F5A)

/*
#define IsControllerZomb(GameServer) (GameServer->GameType()[0] == 'Z' && \
	GameServer->GameType()[1] == 'O' &&\
	GameServer->GameType()[2] == 'M' &&\
	GameServer->GameType()[3] == 'B')
*/

inline bool CharIsSurvivor(i32 CID) {
	return (CID >= 0 && CID < MAX_SURVIVORS);
}

inline bool CharIsZombie(i32 CID) {
	return (CID >= MAX_SURVIVORS);
}
