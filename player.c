#include "glue.h"

word playerHealth, playerHealthCells, playerBombs;
word playerX, playerY;
word scrollX, scrollY;
word playerFaceDir;
word playerBaseFrame = PLAYER_BASE_WEST;
word playerFrame = PLAYER_WALK_1;
word playerPushForceFrame;
byte playerClingDir;
bool canPlayerCling, isPlayerNearHintGlobe, isPlayerNearTransporter;

bbool sawAutoHintGlobe;
bool sawJumpPadBubble, sawMonumentBubble, sawScooterBubble,
    sawTransporterBubble, sawPipeBubble, sawBossBubble, sawPusherRobotBubble,
    sawBearTrapBubble, sawMysteryWallBubble, sawTulipLauncherBubble,
    sawHamburgerBubble, sawHurtBubble = false;
bool usedCheatCode, sawBombHint, sawHealthHint;
word pounceHintState;

word playerRecoilLeft = 0;
bool isPlayerLongJumping, isPlayerRecoiling = false;
bool isPlayerSlidingEast, isPlayerSlidingWest;
bbool isPlayerFalling;
int playerFallTime;
byte playerJumpTime;
word playerPushDir, playerPushMaxTime, playerPushTime, playerPushSpeed;
bbool isPlayerPushAbortable;
bool isPlayerPushed, isPlayerPushBlockable;
bool queuePlayerDizzy;
word playerDizzyLeft;
word scooterMounted;
bool isPounceReady;
bbool isPlayerInPipe;

bool isGodMode = false;
bbool isPlayerInvincible;
word playerHurtCooldown, playerDeadTime;
byte playerFallDeadTime = 0;

/*
Converts the `recoil` argument into a global `playerRecoilLeft` value, as long
as the player's jump/fall state is correct for a pounce. Returns true if the
recoil was imparted, or false otherwise. This function is called *a lot*, but
it rarely acts because `isPounceReady` is usually false (i.e. the player is not
lined up against an actor).

The outer `else if` handles the case where the player pounces on more than one
actor in the same game tick. In such cases `lastrecoil` and `playerRecoilLeft`
will be very close, and we don't have to re-test nearly as many variables.
*/
bool TryPounce(int recoil)
{
    static word lastrecoil;

    if (playerDeadTime != 0 || playerDizzyLeft != 0) return false;

    if ((
        /* 2nd `isPlayerRecoiling` test is pointless */
        !isPlayerRecoiling || (isPlayerRecoiling && playerRecoilLeft < 2)
    ) && (
        (isPlayerFalling && playerFallTime >= 0) || playerJumpTime > 6
    ) && isPounceReady) {
        lastrecoil = playerRecoilLeft = recoil + 1;
        isPlayerRecoiling = true;

        ClearPlayerDizzy();

        if (recoil > 18) {
            isPlayerLongJumping = true;
        } else {
            isPlayerLongJumping = false;
        }

        pounceHintState = POUNCE_HINT_SEEN;

        if (recoil == 7) {
            pounceStreak++;
            if (pounceStreak == 10) {
                pounceStreak = 0;
                NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
            }
        } else {
            pounceStreak = 0;
        }

        return true;

    } else if (lastrecoil - 2 < playerRecoilLeft && isPounceReady && isPlayerRecoiling) {
        ClearPlayerDizzy();

        if (playerRecoilLeft > 18) {
            isPlayerLongJumping = true;
        } else {
            isPlayerLongJumping = false;
        }

        pounceHintState = POUNCE_HINT_SEEN;

        return true;
    }

    return false;
}

/*
Cause the player pain, deduct health, and determine if the player becomes dead.
*/
void HurtPlayer(void)
{
    if (
        playerDeadTime != 0 || isGodMode || blockActionCmds || activeTransporter != 0 ||
        isPlayerInvincible || isPlayerInPipe || playerHurtCooldown != 0
    ) return;

    playerClingDir = DIR4_NONE;

    if (!sawHurtBubble) {
        sawHurtBubble = true;

        NewActor(ACT_SPEECH_OUCH, playerX - 1, playerY - 5);

        if (pounceHintState == POUNCE_HINT_UNSEEN) {
            pounceHintState = POUNCE_HINT_QUEUED;
        }
    }

    playerHealth--;

    if (playerHealth == 0) {
        playerDeadTime = 1;
        scooterMounted = 0;
    } else {
        UpdateHealth();
        playerHurtCooldown = 44;
        StartSound(SND_PLAYER_HURT);
    }
}

/*
Handle movement of the player when standing on a moving platform/fountain.

There's no real need for x_dir/y_dir to be split up; might be cruft.
*/
void MovePlayerPlatform(word x_west, word x_east, word x_dir, word y_dir)
{
    register word offset;
    register word playerx2;

    if (scooterMounted != 0) return;

    offset = *playerInfoData;  /* read frame 0 regardless of player's real frame */
    playerx2 = *(playerInfoData + offset + 1) + playerX - 1;

    if (
        playerClingDir != DIR4_NONE &&
        TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE
    ) {
        playerClingDir = DIR4_NONE;
    }

    if (
        (playerX  < x_west || playerX  > x_east) &&
        (playerx2 < x_west || playerx2 > x_east)
    ) return;

    playerX += dir8X[x_dir];
    playerY += dir8Y[y_dir];

    if ((cmdNorth || cmdSouth) && !cmdWest && !cmdEast) {
        if (cmdNorth && scrollY > 0 && playerY - scrollY < scrollH - 1) {
            scrollY--;
        }

        if (cmdSouth && (
            scrollY + 4 < playerY || (dir8Y[y_dir] == 1 && scrollY + 3 < playerY)
        )) {
            scrollY++;
        }
    }

    if (playerY - scrollY > scrollH - 1) {
        scrollY++;
    } else if (playerY - scrollY < 3) {
        scrollY--;
    }

    if (playerX - scrollX > SCROLLW - 15 && mapWidth - SCROLLW > scrollX) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }

    if (dir8Y[y_dir] == 1 && playerY - scrollY > scrollH - 4) {
        scrollY++;
    }

    if (dir8Y[y_dir] == -1 && playerY - scrollY < 3) {
        scrollY--;
    }
}

/*
Can the player move to x,y considering the direction, and how?

NOTE: `dir` does not adjust the x,y values. Therefore, the passed x,y should
always reflect the location the player wants to move into, and *not* the
location where they currently are.
*/
word TestPlayerMove(word dir, word x_origin, word y_origin)
{
    word *mapcell;
    word i;

    isPlayerSlidingEast = false;
    isPlayerSlidingWest = false;

    switch (dir) {
    case DIR4_NORTH:
        if (playerY - 3 == 0 || playerY - 2 == 0) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x_origin, y_origin - 4);

        for (i = 0; i < 3; i++) {
            if (TILE_BLOCK_NORTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_SOUTH:
#ifdef SAFETY_NET
        if (isGodMode && maxScrollY + scrollH <= y_origin) {
            if (cmdJump && !cmdJumpLatch) {
                cmdJumpLatch = true;
                isPlayerRecoiling = true;
                isPlayerLongJumping = true;
                playerFallTime = 1;
                playerRecoilLeft = 20;

                StartSound(SND_PUSH_PLAYER);
            }

            return MOVE_BLOCKED;
        }
#else
        if (maxScrollY + scrollH == playerY) return MOVE_FREE;
#endif  /* SAFETY_NET */

        mapcell = MAP_CELL_ADDR(x_origin, y_origin);

        if (
            !TILE_BLOCK_SOUTH(*mapcell) &&
            TILE_SLOPED(*mapcell) &&
            TILE_SLIPPERY(*mapcell)
        ) isPlayerSlidingEast = true;

        if (
            !TILE_BLOCK_SOUTH(*(mapcell + 2)) &&
            TILE_SLOPED(*(mapcell + 2)) &&
            TILE_SLIPPERY(*(mapcell + 2))
        ) isPlayerSlidingWest = true;

        for (i = 0; i < 3; i++) {
            if (TILE_SLOPED(*(mapcell + i))) {
                pounceStreak = 0;
                return MOVE_SLOPED;
            }

            if (TILE_BLOCK_SOUTH(*(mapcell + i))) {
                pounceStreak = 0;
                return MOVE_BLOCKED;
            }
        }

        break;

    case DIR4_WEST:
        mapcell = MAP_CELL_ADDR(x_origin, y_origin);
        canPlayerCling = TILE_CAN_CLING(*(mapcell - (mapWidth * 2)));

        for (i = 0; i < 5; i++) {
            if (TILE_BLOCK_WEST(*mapcell)) return MOVE_BLOCKED;

            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_WEST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            mapcell -= mapWidth;
        }

        break;

    case DIR4_EAST:
        mapcell = MAP_CELL_ADDR(x_origin + 2, y_origin);
        canPlayerCling = TILE_CAN_CLING(*(mapcell - (mapWidth * 2)));

        for (i = 0; i < 5; i++) {
            if (TILE_BLOCK_EAST(*mapcell)) return MOVE_BLOCKED;

            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_EAST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            mapcell -= mapWidth;
        }

        break;
    }

    return MOVE_FREE;
}

/*
Is the passed sprite frame at x,y touching the player's sprite?
*/
bool IsTouchingPlayer(word sprite_type, word frame, word x_origin, word y_origin)
{
    register word height, width;
    word offset;

    if (playerDeadTime != 0) return false;

    offset = *(actorInfoData + sprite_type) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    if (x_origin > mapWidth && x_origin <= WORD_MAX && sprite_type == SPR_EXPLOSION) {
        /* Handle explosions with negative X; discussed in IsIntersecting() */
        width = x_origin + width;
        x_origin = 0;
    }

    if ((
        (playerX <= x_origin && playerX + 3 > x_origin) ||
        (playerX >= x_origin && x_origin + width > playerX)
    ) && (
        (y_origin - height < playerY && playerY <= y_origin) ||
        (playerY - 4 <= y_origin && y_origin <= playerY)  /* extra <= */
    )) return true;

    /* Need this constant return so the jumps compile like the original did */
    return false;
}

/*
Cancel any active push the player may be experiencing.
*/
void ClearPlayerPush(void)
{
    isPlayerPushed = false;
    playerPushDir = DIR8_NONE;
    playerPushMaxTime = 0;
    playerPushTime = 0;
    playerPushSpeed = 0;
    playerPushForceFrame = 0;

    isPlayerRecoiling = false;
    playerRecoilLeft = 0;

    isPlayerPushAbortable = false;

    isPlayerFalling = true;
    playerFallTime = 0;
}

/*
Push the player in a direction, for a maximum number of frames, at a certain
speed. The player sprite can be overridden, and the push can be configured to
pass through walls. The ability to "jump out" of a push is available, but this
is never used in the game.
*/
void SetPlayerPush(
    word dir, word max_time, word speed, word force_frame, bool abortable,
    bool blockable
) {
    playerPushDir = dir;
    playerPushMaxTime = max_time;
    playerPushTime = 0;
    playerPushSpeed = speed;
    playerPushForceFrame = force_frame;
    isPlayerPushAbortable = abortable;
    isPlayerPushed = true;

    scooterMounted = 0;

    isPlayerPushBlockable = blockable;

    isPlayerRecoiling = false;
    playerRecoilLeft = 0;

    ClearPlayerDizzy();
}

/*
Push the player for one frame. Stop if the player hits the edge of the map or a
wall (if enabled), or if the push expires.
*/
static void MovePlayerPush(void)
{
    word i;
    bool blocked = false;

    if (!isPlayerPushed) return;

    if (cmdJump && isPlayerPushAbortable) {
        isPlayerPushed = false;
        return;
    }

    for (i = 0; i < playerPushSpeed; i++) {
        if (
            playerX + dir8X[playerPushDir] > 0 &&
            playerX + dir8X[playerPushDir] + 2 < mapWidth
        ) {
            playerX += dir8X[playerPushDir];
        }

        playerY += dir8Y[playerPushDir];

        if (
            scrollX + dir8X[playerPushDir] > 0 &&
            scrollX + dir8X[playerPushDir] < mapWidth - (SCROLLW - 1)
        ) {
            scrollX += dir8X[playerPushDir];
        }

        if (scrollY + dir8Y[playerPushDir] > 2) {
            scrollY += dir8Y[playerPushDir];
        }

        if (isPlayerPushBlockable && (
            TestPlayerMove(DIR4_WEST,  playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_EAST,  playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE
        )) {
            blocked = true;
            break;
        }
    }

    if (blocked) {
        /* Rather than precompute, move into the wall then back up one step. */
        playerX -= dir8X[playerPushDir];
        playerY -= dir8Y[playerPushDir];
        scrollX -= dir8X[playerPushDir];
        scrollY -= dir8Y[playerPushDir];

        ClearPlayerPush();
    } else {
        playerPushTime++;
        if (playerPushTime >= playerPushMaxTime) {
            ClearPlayerPush();
        }
    }
}

/*
Handle player movement and bomb placement while the player is riding a scooter.
*/
void MovePlayerScooter(void)
{
    static word bombcooldown = 0;

    ClearPlayerDizzy();

    isPounceReady = false;
    playerRecoilLeft = 0;
    isPlayerFalling = false;

    if (playerDeadTime != 0) return;

    if (scooterMounted > 1) {
        cmdNorth = true;
        scooterMounted--;
    } else if (cmdJump) {
        cmdJumpLatch = true;
        scooterMounted = 0;
        isPlayerFalling = true;
        playerFallTime = 1;
        isPlayerRecoiling = false;
        isPounceReady = true;
        TryPounce(9);  /* used for side effects */
        playerRecoilLeft -= 2;
        StartSound(SND_PLAYER_JUMP);
        return;
    }

    if (cmdWest && !cmdEast) {
        if (playerBaseFrame == PLAYER_BASE_WEST) {
            playerX--;
        }

        playerBaseFrame = PLAYER_BASE_WEST;
        playerFrame = PLAYER_STAND;

        if (playerX < 1) playerX++;

        if (
            TestPlayerMove(DIR4_WEST, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_WEST, playerX, playerY + 1) != MOVE_FREE
        ) {
            playerX++;
        }

        if (playerX % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX + 3, playerY + 1, DIR8_EAST, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    }

    if (cmdEast && !cmdWest) {
        if (playerBaseFrame != PLAYER_BASE_WEST) {
            playerX++;
        }

        playerBaseFrame = PLAYER_BASE_EAST;
        playerFrame = PLAYER_STAND;

        if (mapWidth - 4 < playerX) playerX--;

        if (
            TestPlayerMove(DIR4_EAST, playerX, playerY) != MOVE_FREE ||
            TestPlayerMove(DIR4_EAST, playerX, playerY + 1) != MOVE_FREE
        ) {
            playerX--;
        }

        if (playerX % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX - 1, playerY + 1, DIR8_WEST, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    }

    if (cmdNorth && !cmdSouth) {
        playerFrame = PLAYER_LOOK_NORTH;

        if (playerY > 4) playerY--;

        if (TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE) {
            playerY++;
        }

        if (playerY % 2 != 0) {
            NewDecoration(SPR_SCOOTER_EXHAUST, 4, playerX + 1, playerY + 1, DIR8_SOUTH, 1);
            StartSound(SND_SCOOTER_PUTT);
        }
    } else if (cmdSouth && !cmdNorth) {
        playerFrame = PLAYER_LOOK_SOUTH;

        if (maxScrollY + 17 > playerY) playerY++;

        if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
            playerY--;
        }
    } else {
        playerFrame = PLAYER_STAND;
    }

    if (!cmdBomb) {
        bombcooldown = 0;
    }

    if (cmdBomb && bombcooldown == 0) {
        bombcooldown = 1;
        playerFrame = PLAYER_CROUCH;
    }

    if (bombcooldown != 0 && bombcooldown != 2) {
        playerFrame = PLAYER_CROUCH;

        if (bombcooldown != 0) {  /* redundant; outer condition makes this always true */
            register bool nearblocked, farblocked;
            bombcooldown = 2;

            if (playerBaseFrame == PLAYER_BASE_WEST) {
                nearblocked = TILE_BLOCK_WEST(GetMapTile(playerX - 1, playerY - 2));
                farblocked  = TILE_BLOCK_WEST(GetMapTile(playerX - 2, playerY - 2));
                if (!nearblocked && !farblocked && playerBombs > 0) {
                    NewActor(ACT_BOMB_ARMED, playerX - 2, playerY - 2);
decrement:
                    playerBombs--;
                    UpdateBombs();
                    StartSound(SND_PLACE_BOMB);
                } else {
                    StartSound(SND_NO_BOMBS);
                }
            } else {
                nearblocked = TILE_BLOCK_EAST(GetMapTile(playerX + 3, playerY - 2));
                farblocked  = TILE_BLOCK_EAST(GetMapTile(playerX + 4, playerY - 2));
                if (!nearblocked && !farblocked && playerBombs > 0) {
                    NewActor(ACT_BOMB_ARMED, playerX + 3, playerY - 2);
                    /* HACK: For the life of me, I can't get the compiler's
                    deduplicator to preserve the target branch without this. */
                    goto decrement;
                } else {
                    StartSound(SND_NO_BOMBS);
                }
            }
        }
    } else {
        cmdBomb = false;
    }

    if (playerY - scrollY > scrollH - 4) {
        scrollY++;
    } else {
        if (playerRecoilLeft > 10 && playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }

        if (playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
    }

    if (playerX - scrollX > SCROLLW - 15 && mapWidth - SCROLLW > scrollX) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }
}

/*
Reset all variables for the "player dizzy/shaking head" immobilization.
*/
void ClearPlayerDizzy(void)
{
    queuePlayerDizzy = false;
    playerDizzyLeft = 0;
}

/*
If the player has a head-shake queued up, perform it here.
*/
void ProcessPlayerDizzy(void)
{
    static word shakeframes[] = {
        PLAYER_SHAKE_1, PLAYER_SHAKE_2, PLAYER_SHAKE_3, PLAYER_SHAKE_2,
        PLAYER_SHAKE_1, PLAYER_SHAKE_2, PLAYER_SHAKE_3, PLAYER_SHAKE_2,
        PLAYER_SHAKE_1
    };

    if (playerClingDir != DIR4_NONE) ClearPlayerDizzy();

    if (queuePlayerDizzy && TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
        queuePlayerDizzy = false;
        playerDizzyLeft = 8;
        StartSound(SND_PLAYER_LAND);
    }

    if (playerDizzyLeft != 0) {
        playerFrame = shakeframes[playerDizzyLeft];
        playerDizzyLeft--;
        isPlayerFalling = false;

        if (playerDizzyLeft > 8) {
            /* There's no clear way how this can ever happen */
            ClearPlayerDizzy();
        }
    }
}

/*
Draw the player sprite, as well as any reactions to external factors. Also
responsible for determining if the player fell off the map, and drawing the
different player death animations.

Returns true if the level needs to restart due to player death, and false during
normal gameplay.
*/
bbool ProcessAndDrawPlayer(void)
{
    static byte speechframe = 0;

    if (maxScrollY + scrollH + 3 < playerY && playerDeadTime == 0) {
        playerFallDeadTime = 1;
        playerDeadTime = 1;

        if (maxScrollY + scrollH + 4 == playerY) {
            playerY++;
        }

        speechframe++;
        if (speechframe == 5) speechframe = 0;
    }

    if (playerFallDeadTime != 0) {
        playerFallDeadTime++;

        if (playerFallDeadTime == 2) {
            StartSound(SND_PLAYER_HURT);
        }

        for (; playerFallDeadTime < 12; playerFallDeadTime++) {
            WaitHard(2);
        }

        if (playerFallDeadTime == 13) {
            StartSound(SND_PLAYER_DEATH);
        }

        if (playerFallDeadTime > 12 && playerFallDeadTime < 19) {
            DrawSprite(
                SPR_SPEECH_MULTI, speechframe,
                playerX - 1, (playerY - playerFallDeadTime) + 13, DRAW_MODE_IN_FRONT
            );
        }

        if (playerFallDeadTime > 18) {
            DrawSprite(
                SPR_SPEECH_MULTI, speechframe,
                playerX - 1, playerY - 6, DRAW_MODE_IN_FRONT
            );
        }

        if (playerFallDeadTime > 30) {
            LoadGameState('T');
            InitializeLevel(levelNum);
            playerFallDeadTime = 0;  /* InitializeMapGlobals() already did this */
            return true;
        }

    } else if (playerDeadTime == 0) {
        if (playerHurtCooldown == 44) {
            DrawPlayer(playerBaseFrame + PLAYER_PAIN, playerX, playerY, DRAW_MODE_WHITE);
        } else if (playerHurtCooldown > 40) {
            DrawPlayer(playerBaseFrame + PLAYER_PAIN, playerX, playerY, DRAW_MODE_NORMAL);
        }

        if (playerHurtCooldown != 0) playerHurtCooldown--;

        if (playerHurtCooldown < 41) {
            if (!isPlayerPushed) {
                DrawPlayer(playerBaseFrame + playerFrame, playerX, playerY, DRAW_MODE_NORMAL);
            } else {
                DrawPlayer(playerPushForceFrame, playerX, playerY, DRAW_MODE_NORMAL);
            }
        }

    } else if (playerDeadTime < 10) {
        if (playerDeadTime == 1) {
            StartSound(SND_PLAYER_HURT);
        }

        playerDeadTime++;
        DrawPlayer(PLAYER_DEAD_1 + (playerDeadTime % 2), playerX - 1, playerY, DRAW_MODE_IN_FRONT);

    } else if (playerDeadTime > 9) {
        if (scrollY > 0 && playerDeadTime < 12) {
            scrollY--;
        }

        if (playerDeadTime == 10) {
            StartSound(SND_PLAYER_DEATH);
        }

        playerY--;
        playerDeadTime++;
        DrawPlayer(PLAYER_DEAD_1 + (playerDeadTime % 2), playerX - 1, playerY, DRAW_MODE_IN_FRONT);

        if (playerDeadTime > 36) {
            LoadGameState('T');
            InitializeLevel(levelNum);
            return true;
        }
    }

    return false;
}

/*
This is the hairiest function in the entire game. Best of luck to you.
*/
void MovePlayer(void)
{
    static word idlecount = 0;
    static int jumptable[] = {-2, -1, -1, -1, -1, -1, -1, 0, 0, 0};
    static word movecount = 0;
    static word bombcooldown = 0;
    static word playerBombDir;
    word horizmove;
    register word southmove = 0;
    register bool clingslip = false;

    canPlayerCling = false;

    if (
        playerDeadTime != 0 || activeTransporter != 0 || scooterMounted != 0 ||
        playerDizzyLeft != 0 || blockActionCmds
    ) return;

    movecount++;

    MovePlayerPush();

    if (isPlayerPushed) {
        playerClingDir = DIR4_NONE;
        return;
    }

    if (playerClingDir != DIR4_NONE) {
        word clingtarget;

        if (playerClingDir == DIR4_WEST) {
            clingtarget = GetMapTile(playerX - 1, playerY - 2);
        } else {
            clingtarget = GetMapTile(playerX + 3, playerY - 2);
        }

        if (TILE_SLIPPERY(clingtarget) && TILE_CAN_CLING(clingtarget)) {
            if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) != MOVE_FREE) {
                playerClingDir = DIR4_NONE;
            } else {
                playerY++;
                clingslip = true;

                if (playerClingDir == DIR4_WEST) {
                    clingtarget = GetMapTile(playerX - 1, playerY - 2);
                } else {
                    clingtarget = GetMapTile(playerX + 3, playerY - 2);
                }

                if (!TILE_SLIPPERY(clingtarget) && !TILE_CAN_CLING(clingtarget)) {
                    playerClingDir = DIR4_NONE;
                    clingslip = false;
                }
            }
        } else if (!TILE_CAN_CLING(clingtarget)) {
            playerClingDir = DIR4_NONE;
        }
    }

    if (playerClingDir == DIR4_NONE) {
        if (!cmdBomb) {
            bombcooldown = 0;
        }

        if (cmdBomb && bombcooldown == 0) {
            bombcooldown = 2;
        }

        if (bombcooldown != 0 && bombcooldown != 1) {
            bombcooldown--;
            if (bombcooldown == 1) {
                bool nearblocked, farblocked;
                if (playerBaseFrame == PLAYER_BASE_WEST) {
                    nearblocked = TILE_BLOCK_WEST(GetMapTile(playerX - 1, playerY - 2));
                    farblocked  = TILE_BLOCK_WEST(GetMapTile(playerX - 2, playerY - 2));
                    if (playerBombs == 0 && !sawBombHint) {
                        sawBombHint = true;
                        ShowBombHint();
                    } else if (!nearblocked && !farblocked && playerBombs > 0) {
                        NewActor(ACT_BOMB_ARMED, playerX - 2, playerY - 2);
                        playerBombs--;
                        UpdateBombs();
                        StartSound(SND_PLACE_BOMB);
                    } else {
                        StartSound(SND_NO_BOMBS);
                    }
                } else {
                    nearblocked = TILE_BLOCK_EAST(GetMapTile(playerX + 3, playerY - 2));
                    farblocked  = TILE_BLOCK_EAST(GetMapTile(playerX + 4, playerY - 2));
                    if (playerBombs == 0 && !sawBombHint) {
                        sawBombHint = true;
                        ShowBombHint();
                    }
                    /* INCONSISTENCY: This is not fused to the previous `if`, while
                    west is. This affects bomb sound after hint is dismissed. */
                    if (!nearblocked && !farblocked && playerBombs > 0) {
                        NewActor(ACT_BOMB_ARMED, playerX + 3, playerY - 2);
                        playerBombs--;
                        UpdateBombs();
                        StartSound(SND_PLACE_BOMB);
                    } else {
                        StartSound(SND_NO_BOMBS);
                    }
                }
            }
        } else {
            cmdBomb = false;
        }
    }

    if (
        playerJumpTime == 0 && cmdBomb && !isPlayerFalling && playerClingDir == DIR4_NONE &&
        (!cmdJump || cmdJumpLatch)
    ) {
        if (cmdWest) {
            playerFaceDir = DIR4_WEST;
            playerBombDir = DIR4_WEST;
            playerBaseFrame = PLAYER_BASE_WEST;
        } else if (cmdEast) {
            playerFaceDir = DIR4_EAST;
            playerBombDir = DIR4_EAST;
            playerBaseFrame = PLAYER_BASE_EAST;
        } else if (playerFaceDir == DIR4_WEST) {
            playerBombDir = DIR4_WEST;
        } else if (playerFaceDir == DIR4_EAST) {
            playerBombDir = DIR4_EAST;
        }
    } else {
        playerBombDir = DIR4_NONE;
        TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);  /* used for side effects */
        if (!isPlayerSlidingEast || !isPlayerSlidingWest) {
            if (isPlayerSlidingWest) {
                if (playerClingDir == DIR4_NONE) {
                    playerX--;
                }
                if (
                    TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE &&
                    playerClingDir == DIR4_NONE
                ) {
                    playerY++;
                }
                if (playerY - scrollY > scrollH - 4) {
                    scrollY++;
                }
                if (playerX - scrollX < 12 && scrollX > 0) {
                    scrollX--;
                }
                playerClingDir = DIR4_NONE;
            }
            if (isPlayerSlidingEast) {
                if (playerClingDir == DIR4_NONE) {
                    playerX++;
                }
                if (
                    TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE &&
                    playerClingDir == DIR4_NONE
                ) {
                    playerY++;
                }
                if (playerY - scrollY > scrollH - 4) {
                    scrollY++;
                }
                if (playerX - scrollX > SCROLLW - 15 && mapWidth - SCROLLW > scrollX) {
                    scrollX++;
                }
                playerClingDir = DIR4_NONE;
            }
        }
        if (cmdWest && playerClingDir == DIR4_NONE && !cmdEast) {
            southmove = TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);
            if (playerFaceDir == DIR4_WEST) {
                playerX--;
            } else {
                playerFaceDir = DIR4_WEST;
            }
            playerBaseFrame = PLAYER_BASE_WEST;
            if (playerX < 1) {
                playerX++;
            } else {
                horizmove = TestPlayerMove(DIR4_WEST, playerX, playerY);
                if (horizmove == MOVE_BLOCKED) {
                    playerX++;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE && canPlayerCling) {
                        playerClingDir = DIR4_WEST;
                        isPlayerRecoiling = false;
                        playerRecoilLeft = 0;
                        StartSound(SND_PLAYER_CLING);
                        isPlayerFalling = false;
                        playerJumpTime = 0;
                        playerFallTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                    }
                }
            }
            if (horizmove == MOVE_SLOPED) {  /* WHOA uninitialized */
                playerY--;
            } else if (
                southmove == MOVE_SLOPED &&
                TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE
            ) {
                isPlayerFalling = false;
                playerJumpTime = 0;
                playerY++;
            }
        }
        if (cmdEast && playerClingDir == DIR4_NONE && !cmdWest) {
            southmove = TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);
            if (playerFaceDir == DIR4_EAST) {
                playerX++;
            } else {
                playerFaceDir = DIR4_EAST;
            }
            playerBaseFrame = PLAYER_BASE_EAST;
            if (mapWidth - 4 < playerX) {
                playerX--;
            } else {
                horizmove = TestPlayerMove(DIR4_EAST, playerX, playerY);
                if (horizmove == MOVE_BLOCKED) {
                    playerX--;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE && canPlayerCling) {
                        playerClingDir = DIR4_EAST;
                        isPlayerRecoiling = false;
                        playerRecoilLeft = 0;
                        StartSound(SND_PLAYER_CLING);
                        playerJumpTime = 0;
                        isPlayerFalling = false;
                        playerFallTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                    }
                }
            }
            if (horizmove == MOVE_SLOPED) {  /* WHOA uninitialized */
                playerY--;
            } else if (
                southmove == MOVE_SLOPED &&
                TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1) == MOVE_FREE
            ) {
                isPlayerFalling = false;
                playerFallTime = 0;
                playerY++;
            }
        }
        if (playerClingDir != DIR4_NONE && cmdJumpLatch && !cmdJump) {
            cmdJumpLatch = false;
        }
        if (
            playerRecoilLeft != 0 ||
            (cmdJump && !isPlayerFalling && !cmdJumpLatch) ||
            (playerClingDir != DIR4_NONE && cmdJump && !cmdJumpLatch)
        ) {
            bool newjump;

            if (isPlayerRecoiling && playerRecoilLeft > 0) {
                playerRecoilLeft--;
                if (playerRecoilLeft < 10) {
                    isPlayerLongJumping = false;
                }
                if (playerRecoilLeft > 1) {
                    playerY--;
                }
                if (playerRecoilLeft > 13) {
                    playerRecoilLeft--;
                    if (TestPlayerMove(DIR4_NORTH, playerX, playerY) == MOVE_FREE) {
                        playerY--;
                    } else {
                        isPlayerLongJumping = false;
                    }
                }
                newjump = false;
                if (playerRecoilLeft == 0) {
                    playerJumpTime = 0;
                    isPlayerRecoiling = false;
                    playerFallTime = 0;
                    isPlayerLongJumping = false;
                    cmdJumpLatch = true;
                }
            } else {
                if (playerClingDir == DIR4_WEST) {
                    if (cmdWest) {
                        playerClingDir = DIR4_NONE;
                    } else if (cmdEast) {
                        playerBaseFrame = PLAYER_BASE_EAST;
                    }
                }
                if (playerClingDir == DIR4_EAST) {
                    if (cmdEast) {
                        playerClingDir = DIR4_NONE;
                    } else if (cmdWest) {
                        playerBaseFrame = PLAYER_BASE_WEST;
                    }
                }
                if (playerClingDir == DIR4_NONE) {
                    playerY += jumptable[playerJumpTime];
                }
                if (playerJumpTime == 0 && TestPlayerMove(DIR4_NORTH, playerX, playerY + 1) != MOVE_FREE) {
                    playerY++;
                }
                isPlayerRecoiling = false;
                newjump = true;
            }

            playerClingDir = DIR4_NONE;

            if (TestPlayerMove(DIR4_NORTH, playerX, playerY) != MOVE_FREE) {
                if (playerJumpTime > 0 || isPlayerRecoiling) {
                    StartSound(SND_PLAYER_HIT_HEAD);
                }
                playerRecoilLeft = 0;
                isPlayerRecoiling = false;
                if (TestPlayerMove(DIR4_NORTH, playerX, playerY + 1) != MOVE_FREE) {
                    playerY++;
                }
                playerY++;
                isPlayerFalling = true;
                if (cmdJump) {
                    cmdJumpLatch = true;
                }
                playerFallTime = 0;
                isPlayerLongJumping = false;
            } else if (newjump && playerJumpTime == 0) {
                StartSound(SND_PLAYER_JUMP);
            }
            if (!isPlayerRecoiling && playerJumpTime++ > 6) {
                isPlayerFalling = true;
                if (cmdJump) {
                    cmdJumpLatch = true;
                }
                playerFallTime = 0;
            }
        }
        if (playerClingDir == DIR4_NONE) {
            if (isPlayerFalling && cmdJump) {
                cmdJumpLatch = true;
            }
            if ((!cmdJump || cmdJumpLatch) && !isPlayerFalling) {
                isPlayerFalling = true;
                playerFallTime = 0;
            }
            if (isPlayerFalling && !isPlayerRecoiling) {
                playerY++;
                if (TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE) {
                    if (playerFallTime != 0) {
                        StartSound(SND_PLAYER_LAND);
                    }
                    isPlayerFalling = false;
                    playerY--;
                    playerJumpTime = 0;
                    if (cmdJump) {
                        cmdJumpLatch = true;
                    } else {
                        cmdJumpLatch = false;
                    }
                    playerFallTime = 0;
                }
                if (playerFallTime > 3) {
                    playerY++;
                    scrollY++;
                    if (TestPlayerMove(DIR4_SOUTH, playerX, playerY) != MOVE_FREE) {
                        StartSound(SND_PLAYER_LAND);
                        isPlayerFalling = false;
                        playerY--;
                        scrollY--;
                        playerJumpTime = 0;
                        if (cmdJump) {
                            cmdJumpLatch = true;
                        } else {
                            cmdJumpLatch = false;
                        }
                        playerFallTime = 0;
                    }
                }
                if (playerFallTime < 25) playerFallTime++;
            }
            if (isPlayerFalling && playerFallTime == 1 && !isPlayerRecoiling) {
                playerY--;
            }
        }
    }

    if (playerBombDir != DIR4_NONE) {
        idlecount = 0;
        playerFrame = PLAYER_CROUCH;
    } else if ((cmdNorth || cmdSouth) && !cmdWest && !cmdEast && !isPlayerFalling && !cmdJump) {
        idlecount = 0;
        if (cmdNorth && !isPlayerNearTransporter && !isPlayerNearHintGlobe) {
            if (scrollY > 0 && playerY - scrollY < scrollH - 1) {
                scrollY--;
            }
            if (clingslip) {
                scrollY++;
            }
            if (playerClingDir != DIR4_NONE) {
                playerFrame = PLAYER_CLING_NORTH;
            } else {
                playerFrame = PLAYER_LOOK_NORTH;
            }
        } else if (cmdSouth) {
            if (scrollY + 3 < playerY) {
                scrollY++;
                if ((clingslip || isPlayerSlidingEast || isPlayerSlidingWest) && scrollY + 3 < playerY) {
                    scrollY++;
                }
            }
            if (playerClingDir != DIR4_NONE) {
                playerFrame = PLAYER_CLING_SOUTH;
            } else {
                playerFrame = PLAYER_LOOK_SOUTH;
            }
        }
        return;
    } else if (playerClingDir == DIR4_WEST) {
        idlecount = 0;
        if (cmdEast) {
            playerFrame = PLAYER_CLING_OPPOSITE;
        } else {
            playerFrame = PLAYER_CLING;
        }
    } else if (playerClingDir == DIR4_EAST) {
        idlecount = 0;
        if (cmdWest) {
            playerFrame = PLAYER_CLING_OPPOSITE;
        } else {
            playerFrame = PLAYER_CLING;
        }
    } else if ((isPlayerFalling && !isPlayerRecoiling) || (playerJumpTime > 6 && !isPlayerFalling)) {
        idlecount = 0;
        if (!isPlayerRecoiling && !isPlayerFalling && playerJumpTime > 6) {
            playerFrame = PLAYER_FALL;
        } else if (playerFallTime >= 10 && playerFallTime < 25) {
            playerFrame = PLAYER_FALL_LONG;
        } else if (playerFallTime == 25) {
            playerFrame = PLAYER_FALL_SEVERE;
            SET_PLAYER_DIZZY();
        } else if (!isPlayerFalling) {
            playerFrame = PLAYER_JUMP;
        } else {
            playerFrame = PLAYER_FALL;
        }
    } else if ((cmdJump && !cmdJumpLatch) || isPlayerRecoiling) {
        idlecount = 0;
        playerFrame = PLAYER_JUMP;
        if (isPlayerRecoiling && isPlayerLongJumping) {
            playerFrame = PLAYER_JUMP_LONG;
        }
        if (playerRecoilLeft < 3 && isPlayerRecoiling) {
            playerFrame = PLAYER_FALL;
        }
    } else if (cmdWest == cmdEast) {
        byte rnd = random(50);
        playerFrame = PLAYER_STAND;
        if (!cmdWest && !cmdEast && !isPlayerFalling) {
            idlecount++;
            if (idlecount > 100 && idlecount < 110) {
                playerFrame = PLAYER_LOOK_NORTH;
            } else if (idlecount > 139 && idlecount < 150) {
                playerFrame = PLAYER_LOOK_SOUTH;
            } else if (idlecount == 180) {
                playerFrame = PLAYER_SHAKE_1;
            } else if (idlecount == 181) {
                playerFrame = PLAYER_SHAKE_2;
            } else if (idlecount == 182) {
                playerFrame = PLAYER_SHAKE_3;
            } else if (idlecount == 183) {
                playerFrame = PLAYER_SHAKE_2;
            } else if (idlecount == 184) {
                playerFrame = PLAYER_SHAKE_1;
            } else if (idlecount == 185) {
                idlecount = 0;
            }
        }
        if (
            playerFrame != PLAYER_LOOK_NORTH &&
            playerFrame != PLAYER_LOOK_SOUTH &&
            (rnd == 0 || rnd == 31)
        ) {
            playerFrame = PLAYER_STAND_BLINK;
        }
    } else if (!isPlayerFalling) {
        idlecount = 0;
        if (movecount % 2 != 0) {
            if (playerFrame % 2 != 0) {
                StartSound(SND_PLAYER_FOOTSTEP);
            }
            playerFrame++;
        }
        if (playerFrame > PLAYER_WALK_4) playerFrame = PLAYER_WALK_1;
    }
    if (playerY - scrollY > scrollH - 4) {
        scrollY++;
    }
    if (clingslip && playerY - scrollY > scrollH - 4) {
        scrollY++;
    } else {
        if (playerRecoilLeft > 10 && playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
        if (playerY - scrollY < 7 && scrollY > 0) {
            scrollY--;
        }
    }
    if (playerX - scrollX > SCROLLW - 15 && mapWidth - SCROLLW > scrollX && mapYPower > 5) {
        scrollX++;
    } else if (playerX - scrollX < 12 && scrollX > 0) {
        scrollX--;
    }
}

/*
Draw the player sprite frame at {x,y}_origin with the requested mode.
*/
void DrawPlayer(byte frame, word x_origin, word y_origin, word mode)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;
    DrawFunction drawfn;

    EGA_MODE_DEFAULT();

    /* NOTE: No default draw function. An unhandled `mode` will crash! */
    switch (mode) {
    case DRAW_MODE_NORMAL:
    case DRAW_MODE_IN_FRONT:
    case DRAW_MODE_ABSOLUTE:
        drawfn = DrawSpriteTile;
        break;
    case DRAW_MODE_WHITE:
        drawfn = DrawSpriteTileWhite;
        break;
    case DRAW_MODE_TRANSLUCENT:
        /* never used in this game */
        drawfn = DrawSpriteTileTranslucent;
        break;
    }

    if (mode != DRAW_MODE_ABSOLUTE && (
        playerPushForceFrame == PLAYER_HIDDEN ||
        activeTransporter != 0 ||
        playerHurtCooldown % 2 != 0 ||
        blockActionCmds
    )) return;

    offset = *playerInfoData + (frame * 4);
    height = *(playerInfoData + offset);
    width = *(playerInfoData + offset + 1);

    y = (y_origin - height) + 1;
    src = playerTileData + *(playerInfoData + offset + 2);

    /* `mode` would go to ax if this was a switch, which doesn't happen */
    if (mode == DRAW_MODE_IN_FRONT) goto infront;
    if (mode == DRAW_MODE_ABSOLUTE) goto absolute;

    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + scrollH > y &&
            !TILE_IN_FRONT(MAP_CELL_DATA(x, y))
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

absolute:
    for (;;) {
        DrawSpriteTile(src, x, y);  /* could've been drawfn */

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

infront:
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + scrollH > y
        ) {
            drawfn(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == y_origin) break;
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }
}

/*
Handle interactions between the player and the passed actor.

This handles the player pouncing on and damaging actors, actors touching and
damaging/affecting the player, and the player picking up prizes. Returns true if
the actor was destroyed/picked up or if this actor needs to be drawn in a non-
standard way.

It's not clear what benefit there is to passing sprite/frame/x/y instead of
reading it from the actor itself. Both methods are used interchangeably.
*/
bool InteractPlayer(word index, word sprite_type, word frame, word x, word y)
{
    Actor *act = actors + index;
    register word height;
    word width;
    register word offset;

    if (!IsSpriteVisible(sprite_type, frame, x, y)) return true;

    offset = *(actorInfoData + sprite_type) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    isPounceReady = false;
    if (sprite_type == SPR_BOSS) {
        height = 7;

        if (
            (y - height) + 5 >= playerY && y - height <= playerY &&
            playerX + 2 >= x && x + width - 1 >= playerX
        ) {
            isPounceReady = true;
        }
    } else if (
        (playerFallTime > 3 ? 1 : 0) + (y - height) + 1 >= playerY && y - height <= playerY &&
        playerX + 2 >= x && x + width - 1 >= playerX &&
        scooterMounted == 0
    ) {
        isPounceReady = true;
    }

    switch (sprite_type) {
    case SPR_JUMP_PAD:
        if (act->data5 != 0) break;  /* ceiling-mounted */

        if (act->hurtcooldown == 0 && TryPounce(40)) {
            StartSound(SND_PLAYER_POUNCE);
            if (!sawJumpPadBubble) {
                sawJumpPadBubble = true;
                NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            }
            act->data1 = 3;
        }
        return false;

    case SPR_JUMP_PAD_ROBOT:
        if (act->hurtcooldown == 0 && TryPounce(20)) {
            StartSound(SND_JUMP_PAD_ROBOT);
            act->data1 = 3;
        }
        return false;

    case SPR_CABBAGE:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            act->hurtcooldown = 5;
            StartSound(SND_PLAYER_POUNCE);
            nextDrawMode = DRAW_MODE_WHITE;
            act->data1--;
            if (act->data1 == 0) {
                act->dead = true;
                AddScoreForSprite(SPR_CABBAGE);
                NewPounceDecoration(act->x, act->y);
                return true;
            }
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BASKET:
    case SPR_BARREL:
        if (act->hurtcooldown == 0 && TryPounce(5)) {
            DestroyBarrel(index);
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
            return true;
        }
        return false;

    case SPR_GHOST:
    case SPR_MOON:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            act->hurtcooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            act->data5--;
            nextDrawMode = DRAW_MODE_WHITE;
            if (act->data5 == 0) {
                act->dead = true;
                if (sprite_type == SPR_GHOST) {
                    NewActor(ACT_BABY_GHOST, act->x, act->y);
                }
                NewPounceDecoration(act->x - 1, act->y + 1);
                AddScoreForSprite(SPR_GHOST);
                return true;
            }
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BABY_GHOST:
    case SPR_SUCTION_WALKER:
    case SPR_BIRD:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            StartSound(SND_PLAYER_POUNCE);
            act->dead = true;
            NewPounceDecoration(act->x, act->y);
            AddScoreForSprite(act->sprite);
            return true;
        } else if (IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_BABY_GHOST_EGG:
    case SPR_74:  /* probably for ACT_BABY_GHOST_EGG_PROX; never happens */
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            StartSound(SND_BGHOST_EGG_CRACK);
            if (act->data2 == 0) {
                act->data2 = 10;
            } else {
                act->data2 = 1;
            }
        }
        return false;

    case SPR_PARACHUTE_BALL:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            StartSound(SND_PLAYER_POUNCE);
            act->data3 = 0;
            act->hurtcooldown = 3;
            act->data5--;
            if (act->data1 != 0 || act->falltime != 0) {
                act->data5 = 0;
            }
            if (act->data5 == 0) {
                NewPounceDecoration(act->x, act->y);
                act->dead = true;
                if (act->data1 > 0) {
                    AddScore(3200);
                    NewActor(ACT_SCORE_EFFECT_3200, act->x, act->y);
                } else if (act->falltime != 0) {
                    AddScore(12800);
                    NewActor(ACT_SCORE_EFFECT_12800, act->x, act->y);
                } else {
                    AddScore(800);
                }
            } else {
                nextDrawMode = DRAW_MODE_WHITE;
                if (act->data1 == 0) {
                    act->data2 = 0;
                    act->data1 = (GameRand() % 2) + 1;
                }
            }
            return false;
        }
        /* Can't maintain parity with the original using an `else if` here. */
        if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_RED_JUMPER:
        if (act->hurtcooldown == 0 && TryPounce(15)) {
            StartSound(SND_PLAYER_POUNCE);
            act->hurtcooldown = 6;
            act->data5--;
            if (act->data5 == 0) {
                NewActor(ACT_STAR_FLOAT, act->x, act->y);
                NewPounceDecoration(act->x, act->y);
                act->dead = true;
                return true;
            }
            nextDrawMode = DRAW_MODE_WHITE;
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_SPITTING_TURRET:
    case SPR_RED_CHOMPER:
    case SPR_PUSHER_ROBOT:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            act->hurtcooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            nextDrawMode = DRAW_MODE_WHITE;
            if (sprite_type != SPR_RED_CHOMPER) {
                act->data5--;
            }
            if (act->data5 == 0 || sprite_type == SPR_RED_CHOMPER) {
                act->dead = true;
                AddScoreForSprite(act->sprite);
                NewPounceDecoration(act->x, act->y);
                return true;
            }
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_PINK_WORM:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            AddScoreForSprite(SPR_PINK_WORM);
            StartSound(SND_PLAYER_POUNCE);
            NewPounceDecoration(act->x, act->y);
            act->dead = true;
            NewActor(ACT_PINK_WORM_SLIME, act->x, act->y);
            return true;
        }
        return false;

    case SPR_SENTRY_ROBOT:
        if (
            ((!areLightsActive && hasLightSwitch) || (areLightsActive && !hasLightSwitch)) &&
            act->hurtcooldown == 0 && TryPounce(15)
        ) {
            act->hurtcooldown = 3;
            StartSound(SND_PLAYER_POUNCE);
            if (act->data1 != DIR2_WEST) {
                act->frame = 7;
            } else {
                act->frame = 8;
            }
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_DRAGONFLY:
    case SPR_IVY_PLANT:
        if (act->hurtcooldown == 0 && TryPounce(7)) {
            pounceStreak = 0;
            StartSound(SND_PLAYER_POUNCE);
            act->hurtcooldown = 5;
        } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
            HurtPlayer();
        }
        return false;

    case SPR_ROCKET:
        if (act->x == playerX && act->hurtcooldown == 0 && TryPounce(5)) {
            StartSound(SND_PLAYER_POUNCE);
        }
        return false;

    case SPR_TULIP_LAUNCHER:
        if (act->eastfree != 0) {
            act->eastfree--;
            if (act->eastfree == 0) {
                isPlayerFalling = true;
                isPounceReady = true;
                act->hurtcooldown == 0 && TryPounce(20);
                StartSound(SND_PLAYER_POUNCE);
                blockMovementCmds = false;
                blockActionCmds = false;
                playerFallTime = 0;
                act->westfree = 1;
                act->data2 = 0;
                act->data1 = 1;
                playerY -= 2;
                if (!sawTulipLauncherBubble) {
                    sawTulipLauncherBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
            }
        } else if (
            act->westfree == 0 && act->x + 1 <= playerX && act->x + 5 >= playerX + 2 &&
            (act->y - 1 == playerY || act->y - 2 == playerY) && isPlayerFalling
        ) {
            act->eastfree = 20;
            isPounceReady = false;
            playerRecoilLeft = 0;
            isPlayerFalling = false;
            blockMovementCmds = true;
            blockActionCmds = true;
            act->westfree = 1;
            act->data2 = 0;
            act->data1 = 1;
            StartSound(SND_TULIP_INGEST);
        }
        return false;

    case SPR_BOSS:
#ifdef HARDER_BOSS
#    define D5_VALUE 20  /* why isn't this 18, like in ActBoss()? */
#else
#    define D5_VALUE 12
#endif  /* HARDER_BOSS */

        if (
            act->eastfree == 0
#ifdef HAS_ACT_BOSS
            && act->data5 != D5_VALUE
#endif  /* HAS_ACT_BOSS */
        ) {
            if (act->hurtcooldown == 0 && TryPounce(7)) {
                StartSound(SND_PLAYER_POUNCE);
                act->data5++;
                act->westfree = 10;
                act->hurtcooldown = 7;
                if (act->data1 != 2) {
                    act->data1 = 2;
                    act->data2 = 31;
                    act->data3 = 0;
                    act->data4 = 1;
                    act->weighted = false;
                    act->falltime = 0;
                }
                if (act->data5 == 4) {
                    NewShard(SPR_BOSS, 1, act->x, act->y - 4);
                    StartSound(SND_BOSS_DAMAGE);
                }
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            } else if (act->hurtcooldown == 0 && IsTouchingPlayer(sprite_type, frame, x, y)) {
                HurtPlayer();
            }
        }
        return true;
#undef D5_VALUE
    }

    if (!IsTouchingPlayer(sprite_type, frame, x, y)) return false;

    switch (sprite_type) {
    case SPR_STAR:
        NewDecoration(SPR_SPARKLE_LONG, 8, x, y, DIR8_NONE, 1);
        gameStars++;
        act->dead = true;
        StartSound(SND_BIG_PRIZE);
        AddScoreForSprite(sprite_type);
        NewActor(ACT_SCORE_EFFECT_200, x, y);
        UpdateStars();
        return true;

    case SPR_ARROW_PISTON_W:
    case SPR_ARROW_PISTON_E:
    case SPR_FIREBALL:
    case SPR_SAW_BLADE:
    case SPR_SPEAR:
    case SPR_FLYING_WISP:
    case SPR_TWO_TONS_CRUSHER:
    case SPR_JUMPING_BULLET:
    case SPR_STONE_HEAD_CRUSHER:
    case SPR_PYRAMID:
    case SPR_PROJECTILE:
    case SPR_SHARP_ROBOT_FLOOR:
    case SPR_SHARP_ROBOT_CEIL:
    case SPR_SPARK:
    case SPR_SMALL_FLAME:
    case SPR_6:  /* probably for ACT_FIREBALL_E; never happens */
    case SPR_48:  /* " " " ACT_PYRAMID_CEIL " " " */
    case SPR_50:  /* " " " ACT_PYRAMID_FLOOR " " " */
        HurtPlayer();
        if (act->sprite == SPR_PROJECTILE) {
            act->dead = true;
        }
        return false;

    case SPR_FLAME_PULSE_W:
    case SPR_FLAME_PULSE_E:
        if (act->frame > 1) {
            HurtPlayer();
        }
        return false;

    case SPR_GREEN_SLIME:
    case SPR_RED_SLIME:
        if (act->data5 != 0) {
            act->y = act->data2;
            act->data4 = 0;
            if (act->y > playerY - 4 || act->frame == 6) {
                HurtPlayer();
            }
            act->frame = 0;
            return false;
        }
        /* Can't maintain parity with the original using an `else if` here. */
        if (act->y > playerY - 4) {
            HurtPlayer();
        }
        return false;

    case SPR_CLAM_PLANT:
    case SPR_84:  /* probably for ACT_CLAM_PLANT_CEIL; never happens */
        if (act->frame != 0) {
            HurtPlayer();
        }
        return false;

    case SPR_HEAD_SWITCH_BLUE:
    case SPR_HEAD_SWITCH_RED:
    case SPR_HEAD_SWITCH_GREEN:
    case SPR_HEAD_SWITCH_YELLOW:
        if (act->frame == 0) {
            act->y--;
            act->frame = 1;
        }
        return false;

    case SPR_SPIKES_FLOOR:
    case SPR_SPIKES_FLOOR_RECIP:
    case SPR_SPIKES_E:
    case SPR_SPIKES_E_RECIP:
    case SPR_SPIKES_W:
        if (act->frame > 1) return true;
        HurtPlayer();
        return false;

    case SPR_POWER_UP:
        act->dead = true;
        StartSound(SND_BIG_PRIZE);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        if (!sawHealthHint) {
            sawHealthHint = true;
            ShowHealthHint();
        }
        if (playerHealth <= playerHealthCells) {
            playerHealth++;
            UpdateHealth();
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
        } else {
            AddScore(12800);
            NewActor(ACT_SCORE_EFFECT_12800, act->x, act->y);
        }
        return true;

    case SPR_GRN_TOMATO:
    case SPR_RED_TOMATO:
    case SPR_YEL_PEAR:
    case SPR_ONION:
        act->dead = true;
        AddScore(200);
        NewActor(ACT_SCORE_EFFECT_200, x, y);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        StartSound(SND_PRIZE);
        return true;

    case SPR_GRAPES:
    case SPR_DANCING_MUSHROOM:
    case SPR_BOTTLE_DRINK:
    case SPR_GRN_GOURD:
    case SPR_BLU_SPHERES:
    case SPR_POD:
    case SPR_PEA_PILE:
    case SPR_LUMPY_FRUIT:
    case SPR_HORN:
    case SPR_RED_BERRIES:
    case SPR_YEL_FRUIT_VINE:
    case SPR_HEADDRESS:
    case SPR_ROOT:
    case SPR_REDGRN_BERRIES:
    case SPR_RED_GOURD:
    case SPR_BANANAS:
    case SPR_RED_LEAFY:
    case SPR_BRN_PEAR:
    case SPR_CANDY_CORN:
        act->dead = true;
        if (
            sprite_type == SPR_YEL_FRUIT_VINE || sprite_type == SPR_BANANAS ||
            sprite_type == SPR_GRAPES || sprite_type == SPR_RED_BERRIES
        ) {
            AddScore(800);
            NewActor(ACT_SCORE_EFFECT_800, x, y);
        } else {
            AddScore(400);
            NewActor(ACT_SCORE_EFFECT_400, x, y);
        }
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        StartSound(SND_PRIZE);
        return true;

    case SPR_HAMBURGER:
        act->dead = true;
        AddScore(12800);
        NewActor(ACT_SCORE_EFFECT_12800, x, y);
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        StartSound(SND_PRIZE);
        if (playerHealthCells < 5) playerHealthCells++;
        if (!sawHamburgerBubble) {
            NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            sawHamburgerBubble = true;
        }
        UpdateHealth();
        return true;

    case SPR_EXIT_SIGN:
        winLevel = true;
        return false;

    case SPR_HEART_PLANT:
        act->data1 = 1;
        HurtPlayer();
        return false;

    case SPR_BOMB_IDLE:
        if (playerBombs <= 8) {
            act->dead = true;
            playerBombs++;
            sawBombHint = true;
            AddScore(100);
            NewActor(ACT_SCORE_EFFECT_100, act->x, act->y);
            UpdateBombs();
            NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
            StartSound(SND_PRIZE);
            return true;
        }
        return false;

    case SPR_FOOT_SWITCH_KNOB:
        if (act->data1 < 4 && act->data4 == 0) {
            isPlayerFalling = true;
            ClearPlayerDizzy();
            TryPounce(3);  /* used for side effects */
            act->data1++;
            if (act->data2 == 0) {
                act->data3 = 64;
                act->data2 = 1;
            } else {
                act->data3 = 0;
            }
            act->data4 = 1;
        }
        return false;

    case SPR_ROAMER_SLUG:
        {  /* for scope */
            word i = GameRand() % 4;
            if (act->hurtcooldown == 0) {
                word gifts[] = {ACT_RED_GOURD, ACT_RED_TOMATO, ACT_CLR_DIAMOND, ACT_GRN_EMERALD};
                act->hurtcooldown = 10;
                if (TryPounce(7)) {
                    StartSound(SND_PLAYER_POUNCE);
                } else {
                    playerClingDir = DIR4_NONE;
                }
                NewSpawner(gifts[i], act->x, act->y + 1);
                StartSound(SND_ROAMER_GIFT);
                nextDrawMode = DRAW_MODE_WHITE;
                act->data2--;
                if (act->data2 == 0) {
                    act->dead = true;
                    NewPounceDecoration(act->x - 1, act->y + 1);
                }
            }
        }
        return false;

    case SPR_PIPE_CORNER_N:
    case SPR_PIPE_CORNER_S:
    case SPR_PIPE_CORNER_W:
    case SPR_PIPE_CORNER_E:
        if (isPlayerInPipe) {
            switch (sprite_type) {
            case SPR_PIPE_CORNER_N:
                SetPlayerPush(DIR8_NORTH, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_S:
                SetPlayerPush(DIR8_SOUTH, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_W:
                SetPlayerPush(DIR8_WEST, 100, 2, PLAYER_HIDDEN, false, false);
                break;

            case SPR_PIPE_CORNER_E:
                SetPlayerPush(DIR8_EAST, 100, 2, PLAYER_HIDDEN, false, false);
                break;
            }
            StartSound(SND_PIPE_CORNER_HIT);
        }
        return true;

    case SPR_PIPE_END:
        if (act->data2 == 0 && (act->y + 3 == playerY || act->y + 2 == playerY)) {
            if (isPlayerPushed) {
                playerX = act->x;
                SET_PLAYER_DIZZY();
                isPlayerInPipe = false;
                ClearPlayerPush();
                if (!sawPipeBubble) {
                    sawPipeBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
            }
        } else if (
            (!isPlayerFalling || isPlayerRecoiling) && (cmdJump || isPlayerRecoiling) &&
            act->x == playerX && (act->y + 3 == playerY || act->y + 2 == playerY)
        ) {
            isPlayerInPipe = true;
        }
        return false;

    case SPR_TRANSPORTER:
        if (transporterTimeLeft == 0) {
            if (act->x <= playerX && act->x + 4 >= playerX + 2 && act->y == playerY) {
                if (cmdNorth) {
                    activeTransporter = act->data5;
                    transporterTimeLeft = 15;
                    isPlayerFalling = false;
                }
                isPlayerNearTransporter = true;
            } else {
                isPlayerNearTransporter = false;
            }
        }
        return true;

    case SPR_SPIKES_FLOOR_BENT:
    case SPR_SPIT_WALL_PLANT_E:
    case SPR_SPIT_WALL_PLANT_W:
    case SPR_PINK_WORM_SLIME:
    case SPR_THRUSTER_JET:
        HurtPlayer();
        return false;

    case SPR_SCOOTER:
        if (isPlayerFalling && (act->y == playerY || act->y + 1 == playerY)) {
            scooterMounted = 4;
            StartSound(SND_PLAYER_LAND);
            ClearPlayerPush();
            isPlayerFalling = false;
            playerFallTime = 0;
            isPlayerRecoiling = false;
            isPounceReady = false;
            playerRecoilLeft = 0;
            pounceStreak = 0;
            if (!sawScooterBubble) {
                sawScooterBubble = true;
                NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
            }
        }
        return false;

    case SPR_EXIT_MONSTER_W:
        if (act->data4 != 0) {
            act->data4--;
            if (act->data4 == 0) {
                winLevel = true;
                act->frame = 0;
                return false;
            }
            act->frame = 0;
        } else if (act->data1 != 0 && act->y == playerY && act->x <= playerX) {
            act->frame = 0;
            act->data5 = 0;
            act->data4 = 5;
            blockActionCmds = true;
            blockMovementCmds = true;
            StartSound(SND_EXIT_MONSTER_INGEST);
        }
        return true;

    case SPR_ROTATING_ORNAMENT:
    case SPR_GRN_EMERALD:
    case SPR_CLR_DIAMOND:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        AddScore(3200);
        NewActor(ACT_SCORE_EFFECT_3200, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_BLU_CRYSTAL:
    case SPR_RED_CRYSTAL:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        AddScore(1600);
        NewActor(ACT_SCORE_EFFECT_1600, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_CYA_DIAMOND:
    case SPR_RED_DIAMOND:
    case SPR_GRY_OCTAHEDRON:
    case SPR_BLU_EMERALD:
    case SPR_HEADPHONES:
        act->dead = true;
        NewDecoration(SPR_SPARKLE_SHORT, 4, act->x, act->y, DIR8_NONE, 3);
        AddScore(800);
        NewActor(ACT_SCORE_EFFECT_800, x, y);
        StartSound(SND_PRIZE);
        return true;

    case SPR_BEAR_TRAP:
        if (act->data2 == 0 && act->x == playerX && act->y == playerY) {
            act->data2 = 1;
            blockMovementCmds = true;
            if (!sawBearTrapBubble) {
                sawBearTrapBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }
            return false;
        }
        /* FALL THROUGH! */

    case SPR_EXIT_PLANT:
        if (
            act->frame == 0 && act->x < playerX && act->x + 5 > playerX &&
            act->y - 2 > playerY && act->y - 5 < playerY && isPlayerFalling
        ) {
            act->data5 = 1;
            blockMovementCmds = true;
            blockActionCmds = true;
            act->frame = 1;
            StartSound(SND_EXIT_MONSTER_INGEST);
        }
        return false;

    case SPR_INVINCIBILITY_CUBE:
        act->dead = true;
        NewActor(ACT_INVINCIBILITY_BUBB, playerX - 1, playerY + 1);
        NewDecoration(SPR_SPARKLE_LONG, 8, x, y, DIR8_NONE, 1);
        /* BUG: score effect is spawned, but no score given */
        NewActor(ACT_SCORE_EFFECT_12800, x, y);
        StartSound(SND_BIG_PRIZE);
        return true;

    case SPR_MONUMENT:
        if (!sawMonumentBubble) {
            sawMonumentBubble = true;
            NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
        }
        if (act->x == playerX + 2) {
            SetPlayerPush(DIR8_WEST, 5, 2, PLAYER_BASE_EAST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
        } else if (act->x + 2 == playerX) {
            SetPlayerPush(DIR8_EAST, 5, 2, PLAYER_BASE_WEST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
        }
        return false;

    case SPR_JUMP_PAD:  /* ceiling-mounted only */
        if (
            act->data5 != 0 && act->hurtcooldown == 0 && scooterMounted == 0 &&
            (!isPlayerFalling || isPlayerRecoiling)
        ) {
            act->hurtcooldown = 2;
            StartSound(SND_PLAYER_POUNCE);
            act->data1 = 3;
            playerRecoilLeft = 0;
            isPlayerRecoiling = false;
            isPlayerFalling = true;
            playerFallTime = 4;
            playerJumpTime = 0;
        }
        return false;

#ifdef HAS_ACT_EXIT_MONSTER_N
    case SPR_EXIT_MONSTER_N:
        blockActionCmds = true;
        blockMovementCmds = true;
        act->data1++;
        if (act->frame != 0) {
            winLevel = true;
        } else if (act->data1 == 3) {
            act->frame++;
        }
        if (act->data1 > 1) {
            playerY = act->y;
            playerY = act->y;  /* why this again and never X? */
            isPlayerFalling = false;
        }
        return false;
#endif  /* HAS_ACT_EXIT_MONSTER_N */
    }

    return false;
}