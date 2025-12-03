/**
 * Cosmore
 * Copyright (c) 2020-2024 Scott Smitelli and contributors
 *
 * Based on COSMO{1..3}.EXE distributed with "Cosmo's Cosmic Adventure"
 * Copyright (c) 1992 Apogee Software, Ltd.
 *
 * This source code is licensed under the MIT license found in the LICENSE file
 * in the root directory of this source tree.
 */

/*****************************************************************************
 *                         COSMORE MAIN "GLUE" HEADER                        *
 *                                                                           *
 * The apparent intention of the author(s) of the original game was to have  *
 * every C function and variable in a single massive file. This is because   *
 * discrete C files compile into discrete OBJ files, and function calls that *
 * cross an OBJ file boundary become "far" calls which reload the            *
 * processor's CS register and take more clock cycles to execute. There is   *
 * too much code to compile as a single unit under DOS, so there are two     *
 * C files: GAME1 and GAME2.                                                 *
 *                                                                           *
 * This header file is literally the glue that holds these two files         *
 * together, declaring every include, typedef, variable, and function that   *
 * the pieces share.                                                         *
 *****************************************************************************/

#ifndef GLUE_H
#define GLUE_H

/* Replace the in-game status bar with a snapshot of some global variables */
#define DEBUG_BAR

/*
Enable this to prevent falling out of the map with god mode enabled. When
standing on the safety net, the jump command will allow long-jumping out.
*/
/*#define SAFETY_NET*/

/* Enable this to add vanity text inside the game */
/*#define VANITY*/

#define GAME_VERSION "1.20"

#include <alloc.h>  /* for coreleft() only */
#include <conio.h>
#include <dos.h>
#include <io.h>  /* for filelength() only */
#include <mem.h>  /* for movmem() only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Don't typedef here, or bools can't use registers and get forced on stack. */
enum {false = 0, true};

typedef unsigned char byte;
typedef unsigned int  word;
typedef unsigned long dword;
typedef word bool;
typedef byte bbool;  /* bool in a byte */
typedef void interrupt (*InterruptFunction)(void);

#define BYTE_MAX 0xffU
#define WORD_MAX 0xffffU

#include "actor.h"
#include "def.h"
#include "graphics.h"
#include "lowlevel.h"
#include "music.h"
#include "player.h"
#include "scancode.h"
#include "sound.h"
#include "sprite.h"

#if EPISODE == 1
#   include "episode1.h"
#elif EPISODE == 2
#   include "episode2.h"
#elif EPISODE == 3
#   include "episode3.h"
#else
#   error "EPISODE must be defined to 1, 2, or 3"
#endif  /* EPISODE == ... */

typedef char HighScoreName[16];
typedef void (*ActorTickFunction)(word);
typedef void (*DrawFunction)(byte *, word, word);

typedef struct {
    word sprite, type, frame, x, y;
    bool forceactive, stayactive, acrophile, weighted;
    word westfree, eastfree, data1, data2, data3, data4, data5;
    bool dead;
    word falltime;
    byte hurtcooldown;
    ActorTickFunction tickfunc;
} Actor;

typedef struct {
    bool alive;
    word sprite, numframes, x, y, dir, numtimes;
} Decoration;

typedef struct {
    word age, x, y;
} Explosion;

typedef struct {
    word x, y, dir, stepcount, height, stepmax, delayleft;
} Fountain;

typedef struct {
    word side, x, y;
    word junk;  /* unused; required to keep struct padding correct */
} Light;

typedef struct {
    word length, datahead;
} Music;

typedef struct {
    word x, y;

    /* This must begin at word offset 2; see LoadMapData(), MovePlatforms(). */
    word mapstash[5];
} Platform;

typedef struct {
    word sprite, x, y, frame, age, xmode;
    bool bounced;
} Shard;

typedef struct {
    word actor, x, y, age;
} Spawner;

typedef union {byte *b; word *w;} Map;

#include "meta.h"

/*
Prototypes for "private" functions where strictly required.

NOTE: It would be reasonable to declare these `static` since they are not used
in any other compilation unit. Doing so would assemble all references to these
functions as relative near calls:
    0E            push cs
    E88401        call 0x242e
The original game did not do that; all of these get absolute far calls instead:
    9AE0049510    call 0x1095:0x4e0
To remain faithful to the original game, these functions use non-`static`
external linkage even though nothing depends on that.
*/
void DrawCartoon(byte, word, word);
word GetMapTile(word, word);
void SetMapTile(word, word, word);
void NewActor(word, word, word);
void NewShard(word, word, word, word);
void NewExplosion(word, word);
bool IsNearExplosion(word, word, word, word);
void NewSpawner(word, word, word);
void NewDecoration(word, word, word, word, word, word);
void HurtPlayer(word);
void HurtPlayer2(void);
void NewPounceDecoration(word, word);
void DestroyBarrel(word);
char *JoinPath(char *, char *);
bbool LoadGameState(char);
void InitializeLevel(word);
void InitializeEpisode(void);

/*
Inline functions.
*/
#define MAP_CELL_ADDR(x, y)   (mapData.w + ((y) << mapYPower) + x)
#define MAP_CELL_DATA(x, y)   (*(mapData.w + (x) + ((y) << mapYPower)))
#define SET_PLAYER_DIZZY()    { queuePlayerDizzy = true; }
#define TILE_BLOCK_SOUTH(val) (*(tileAttributeData + ((val) / 8)) & 0x01)
#define TILE_BLOCK_NORTH(val) (*(tileAttributeData + ((val) / 8)) & 0x02)
#define TILE_BLOCK_WEST(val)  (*(tileAttributeData + ((val) / 8)) & 0x04)
#define TILE_BLOCK_EAST(val)  (*(tileAttributeData + ((val) / 8)) & 0x08)
#define TILE_SLIPPERY(val)    (*(tileAttributeData + ((val) / 8)) & 0x10)
#define TILE_IN_FRONT(val)    (*(tileAttributeData + ((val) / 8)) & 0x20)
#define TILE_SLOPED(val)      (*(tileAttributeData + ((val) / 8)) & 0x40)
#define TILE_CAN_CLING(val)   (*(tileAttributeData + ((val) / 8)) & 0x80)

/* Duplicate of MAP_CELL_DATA() that takes a shift expression to add to `x`. */
#define MAP_CELL_DATA_SHIFTED(x, y, shift_expr) (*(mapData.w + (x) + ((y) << mapYPower) + shift_expr))

/*****************************************************************************
 * GAME1.C                                                                   *
 *****************************************************************************/

void DrawTextLine(word, word, char *);
void DrawFullscreenImage(word);
void StartSound(word);
void PCSpeakerService(void);
void ShowStarBonus(void);

extern word last_height;
extern word last_width;
void DrawSprite(word, word, word, word, word);
void DrawMapRegion(void);

word GameRand(void);
word TestSpriteMove(word, word, word, word, word);
void AdjustActorMove(word, word);
word GetMapTile(word, word);
bool IsSpriteVisible(word, word, word, word);
void SetMapTile4(word, word, word, word, word, word);
void SetMapTileRepeat(word, word, word, word);
bool IsTouchingPlayer(word, word, word, word);
void MoveAndDrawActors(void);

extern bbool mask_init;

void ValidateSystem(void);
void NextLevel(void);

void DrawLights(void);
void MoveAndDrawShards(void);
void MoveAndDrawSpawners(void);
void MoveAndDrawDecorations(void);
void DrawExplosions(void);
void DrawRandomEffects(void);
void DrawFountains(void);
void MoveFountains(void);
void MovePlatforms(void);
byte ProcessGameInputHelper(word, byte);
void AnimatePalette(void);
void DrawFullscreenText(char *);
byte PromptRestoreGame(void);
byte ShowHelpMenu(void);
bbool ReadDemoFrame(void);
bbool WriteDemoFrame(void);
bbool PromptLevelWarp(void);
bool CanExplode(word, word, word, word);

void NewMapActorAtIndex(word, word, word, word);
void CopyTilesToEGA(byte *, word, word);
void InitializeBackdropTable(void);

/*****************************************************************************
 * GAME2.C                                                                   *
 *****************************************************************************/

typedef char KeyName[6];

typedef struct {
    word junk;  /* in IDLIBC.C/ControlJoystick(), this is movement direction */
    bool button1, button2;
} JoystickState;

extern word yOffsetTable[];
extern bbool isAdLibPresent;

void StartAdLib(void);
void StopAdLib(void);
void WaitHard(word);
void WaitSoft(word);
void FadeWhiteCustom(word);
void FadeOutCustom(word);
void FadeIn(void);
void FadeOut(void);
byte WaitSpinner(word, word);
void ClearScreen(void);
JoystickState ReadJoystickState(word);
word UnfoldTextFrame(int, int, int, char *, char *);
void ReadAndEchoText(word, word, char *, word);
void DrawNumberFlushRight(word, word, dword);
void AddScore(dword);
void UpdateStars(void);
void UpdateBombs(void);
void UpdateHealth(void);
void ShowHighScoreTable(void);
void CheckHighScoreAndShow(void);
void ShowOrderingInformation(void);
void ShowStory(void);
void StartGameMusic(word);
void StartMenuMusic(word);
void StopMusic(void);
void DrawScancodeCharacter(word, word, byte);
void WrapBackdropVertical(byte *, byte *, byte *);
void WrapBackdropHorizontal(byte *, byte *);
void ShowHintsAndKeys(word);
void ShowInstructions(void);
void ShowPublisherBBS(void);
#ifdef FOREIGN_ORDERS
void ShowForeignOrders(void);
#endif  /* FOREIGN_ORDERS */
void ShowRestoreGameError(void);
void ShowCopyright(void);
void ShowAlteredFileError(void);
void ShowRescuedDNMessage(void);
void ShowE1CliffhangerMessage(word);
bbool PromptQuitConfirm(void);
void ToggleSound(void);
void ToggleMusic(void);
void ShowPauseMessage(void);
void ToggleGodMode(void);
void ShowMemoryUsage(void);
void ShowGameRedefineMenu(void);
void LoadConfigurationData(char *);
void SaveConfigurationData(char *);
void ShowEnding(void);
void ShowHintGlobeMessage(word);
void ShowCheatMessage(void);
void AddScoreForSprite(word);
void DrawStaticGameScreen(void);
void DrawMainMenu(void);
void ShowBombHint(void);
void ShowPounceHint(void);
void ShowLevelIntro(word);
void ShowHealthHint(void);

bool SetMusicState(bool);

/*****************************************************************************
 * GAME3.C                                                                   *
 *****************************************************************************/

void ClearScreen2(void);
void ClearAreaGray(word, word, word, word);
void DrawNumber(dword, word, word);
int LenNumber(dword);

word SpriteHeight(word, word);
word SpriteWidth(word, word);
word SpriteFrames(word);
word NextUniqueSprite(word);
typedef word (*SizeFunction)(word, word);
word SpriteSize(word, SizeFunction);

void GFXTest(void);
word DebugPage(word, word, bool);
word DebugRow(word, word, word, bool, bool);
void DebugSprite(word, word, word, bool);
void DebugSpriteBG(word, word, word);
byte DebugKeyPause();

void GenDrawTile(word, word, word);

void LevelTest(void);
void LevelLoad(word);
void TileView(void);
extern bbool level_edit;
extern bbool edit_actors;

typedef struct {
    word x, y;
    bbool active;
    word tile;
} Cursor;

extern word scrollH;

bbool cursor_move(Cursor *, word, word, word, word);

/*****************************************************************************
 * actor.C                                                                   *
 *****************************************************************************/

bbool NewActorAtIndex(word, word, word, word);

/*****************************************************************************
 * ctrlflow.C                                                                *
 *****************************************************************************/

void InnerMain(int, char *[]);
void ExitClean(void);

/*****************************************************************************
 * player.C                                                                  *
 *****************************************************************************/

void DrawPlayer(byte, word, word, word);
void ClearPlayerPush(void);
void SetPlayerPush(word, word, word, word, bool, bool);
void ClearPlayerDizzy(void);
bbool ProcessAndDrawPlayer(void);
word TestPlayerMove(word, word, word);

void MovePlayerPlatform(word, word, word, word);
bool TryPounce(int);
void ProcessPlayerDizzy(void);
void MovePlayer(void);
void MovePlayerScooter(void);
bool InteractPlayer(word, word, word, word, word);

/*****************************************************************************
 * input.C                                                                   *
 *****************************************************************************/

byte ProcessGameInput(byte);
byte WaitForAnyKey(void);
bbool IsAnyKeyDown(void);
void interrupt KeyboardInterruptService(void);

/*****************************************************************************
 * groupf.C                                                                  *
 *****************************************************************************/

void Startup(void);
FILE *GroupEntryFp(char *);
void LoadCartoonData(char *);
void LoadMaskedTileData(char *);
void LoadTileAttributeData(char *);
void LoadMapData(word);
void LoadBackdropData(char *, byte *);
void LoadDemoData(void);
void SaveDemoData(void);
Music *LoadMusicData(word, Music *);

/*****************************************************************************
 * prompt.C                                                                  *
 *****************************************************************************/

void JustPrompt(void);
void void2(void);
void mid_box(char*, char*, char*, char*);

/*****************************************************************************
 * event.C                                                                   *
 *****************************************************************************/

void init_lv_events(word);
void launch(word, word);

/* maybe UI.C, miscobjs.C, audio.C, world.C */

#endif  /* GLUE_H */
