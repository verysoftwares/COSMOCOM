#ifndef META_H
#define META_H

/*
Overarching game control variables.
*/
extern bbool isInGame;
extern bool winGame;
extern dword gameScore, gameStars;
extern bbool isNewGame;
extern bool winLevel;

/*
Memory content indicators. Used to determine if the values currently held in
different storage areas is acceptable for re-use, or if they need to be reloaded
from a group entry.
*/
extern bbool isCartoonDataLoaded;
extern word miscDataContents;

/*
Debug mode and demo record/playback variables.
*/
extern byte demoState;
extern word demoDataLength, demoDataPos;
extern bbool isDebugMode;

/*
X any Y move component tables for DIR8_* directions.
*/
extern int dir8X[];
extern int dir8Y[];

/*
Free-running counters. These generally keep counting no matter what's going on
elsewhere in the game.
*/
extern word activePage;
extern word gameTickCount;
extern word randStepCount;
extern dword paletteStepCount;

/*
String storage.
*/
extern char *stnGroupFilename;
extern char *volGroupFilename;
extern char *fullscreenImageNames[];
extern char *backdropNames[];
extern char *mapNames[];
extern char *musicNames[];
extern char *starBonusRanks[];

/*
System variables.
*/
extern dword totalMemFreeBefore, totalMemFreeAfter;
extern InterruptFunction savedInt9;
extern char *writePath;

/*
BSS storage areas. All of these are big ol' fixed-sized arrays.
*/
extern HighScoreName highScoreNames[11];
extern dword highScoreValues[11];
extern byte soundPriority[80 + 1];
extern Platform platforms[MAX_PLATFORMS];
extern Fountain fountains[MAX_FOUNTAINS];
extern Light lights[MAX_LIGHTS];
extern Actor actors[MAX_ACTORS];
extern Shard shards[MAX_SHARDS];
extern Explosion explosions[MAX_EXPLOSIONS];
extern Spawner spawners[MAX_SPAWNERS];
extern Decoration decorations[MAX_DECORATIONS];
/* Holds each decoration's currently displayed frame. Why this isn't in the Decoration struct, who knows. */
extern word decorationFrame[MAX_DECORATIONS];
extern word backdropTable[BACKDROP_WIDTH * BACKDROP_HEIGHT * 4];

/*
Heap storage areas. Space for all of these is allocated on startup.
*/
extern byte *fontTileData, *maskedTileData, *miscData;
extern byte *actorTileData[3], *playerTileData, *tileAttributeData;
extern word *actorInfoData, *playerInfoData, *cartoonInfoData;
extern word *soundData1, *soundData2, *soundData3, *soundDataPtr[80];
extern Map mapData;

/*
Pass-by-global variables. If you see one of these in use, some earlier function
wants to influence the behavior of a subsequently called function.
*/
extern dword lastGroupEntryLength;
extern word nextActorIndex, nextDrawMode;
extern word nextActorType;

/*
Keyboard and joystick variables. Also includes player immobilization and jump
lockout flags.
*/
extern byte lastScancode;
extern bbool isKeyDown[BYTE_MAX];
extern bool isJoystickReady;
extern bbool cmdWest, cmdEast, cmdNorth, cmdSouth, cmdJump, cmdBomb;
extern bbool blockMovementCmds, cmdJumpLatch;
extern bool blockActionCmds;

/*
Customizable options. These are saved and persist across restarts.
*/
extern bool isMusicEnabled, isSoundEnabled;
extern byte scancodeWest, scancodeEast, scancodeNorth, scancodeSouth, scancodeJump, scancodeBomb;

/*
PC speaker sound effect and AdLib music variables.
*/
extern Music *activeMusic;
extern word activeSoundIndex, activeSoundPriority;
extern bool isNewSound, enableSpeaker;

/*
Level/map control and global world variables.
*/
extern word levelNum;
extern word lvlMusicNum;
extern word lvlBdnum;
extern word mapWidth, maxScrollY, mapYPower;  /* y power = map width expressed as 2^n. */
extern bool hasLightSwitch, hasRain, hasHScrollBackdrop, hasVScrollBackdrop;
extern bool areForceFieldsActive, areLightsActive, arePlatformsActive;
extern byte paletteAnimationNum;

/*
Actor (and similar) counts, as well as a few odds and ends for actors that
interact heavily with the outside world.
*/
extern word numActors;
extern word numPlatforms, numFountains, numLights;
extern word numBarrels, numEyePlants, pounceStreak;
extern word mysteryWallTime;
extern word activeTransporter, transporterTimeLeft;

/*
Player position and game interaction variables. Most of these are under direct
or almost-direct control of the player.
*/
extern word playerHealth, playerHealthCells, playerBombs;
extern word playerX, playerY;
extern word scrollX, scrollY;
extern word playerFaceDir;
extern word playerBaseFrame;
extern word playerFrame;
extern word playerPushForceFrame;
extern byte playerClingDir;
extern bool canPlayerCling, isPlayerNearHintGlobe, isPlayerNearTransporter;

/*
Player one-shot variables. Each of these controls the "happens only once"
behavior of something in the game. Some are reset when a new level is started,
others persist for the whole game (and into save files).

In the case of the `saw*Bubble` variables, it appears the author believed that
all of these were receiving an initial value, but only `sawHurtBubble` is
actually initialized here.
*/
extern bbool sawAutoHintGlobe;
extern bool sawJumpPadBubble, sawMonumentBubble, sawScooterBubble,
    sawTransporterBubble, sawPipeBubble, sawBossBubble, sawPusherRobotBubble,
    sawBearTrapBubble, sawMysteryWallBubble, sawTulipLauncherBubble,
    sawHamburgerBubble, sawHurtBubble;
extern bool usedCheatCode, sawBombHint, sawHealthHint;
extern word pounceHintState;

/*
Player speed/movement variables. These control vertical movement in the form of
jumping, pouncing, and falling. Horizontal movement takes the form of east/
west sliding for slippery surfaces. There are also variables for involuntary
pushes and "dizzy" immobilization.
*/
extern word playerRecoilLeft;
extern bool isPlayerLongJumping, isPlayerRecoiling;
extern bool isPlayerSlidingEast, isPlayerSlidingWest;
extern bbool isPlayerFalling;
extern int playerFallTime;
extern byte playerJumpTime;
extern word playerPushDir, playerPushMaxTime, playerPushTime, playerPushSpeed;
extern bbool isPlayerPushAbortable;
extern bool isPlayerPushed, isPlayerPushBlockable;
extern bool queuePlayerDizzy;
extern word playerDizzyLeft;
extern word scooterMounted;  /* Acts like bool, except at moment of mount (decrements 4->1) */
extern bool isPounceReady;
extern bbool isPlayerInPipe;

/*
Player pain and death variables.
*/
extern bool isGodMode;
extern bbool isPlayerInvincible;
extern word playerHurtCooldown, playerDeadTime;
extern byte playerFallDeadTime;

extern bbool emexit; /* whether you can F12 out */
extern word now; /* how manieth dialogue */

#endif /* #ifndef META_H */