#include "glue.h"

/*
Run the game loop. This function does not return until the entire game has been
won or the player quits.
*/
static void GameLoop(byte demo_state)
{
    for (;;) {
        while (gameTickCount < 13)
            ;  /* VOID */

        gameTickCount = 0;

        AnimatePalette();

        {  /* for scope */
            word result = ProcessGameInputHelper(activePage, demo_state);
            if (result == GAME_INPUT_QUIT) return;
            if (result == GAME_INPUT_RESTART) continue;
        }

        MovePlayer();

        if (scooterMounted != 0) {
            MovePlayerScooter();
        }

        if (queuePlayerDizzy || playerDizzyLeft != 0) {
            ProcessPlayerDizzy();
        }

        MovePlatforms();
        MoveFountains();
        DrawMapRegion();

        if (ProcessAndDrawPlayer()) continue;

        DrawFountains();
        MoveAndDrawActors();
        MoveAndDrawShards();
        MoveAndDrawSpawners();
        DrawRandomEffects();
        DrawExplosions();
        MoveAndDrawDecorations();
        DrawLights();

        if (demoState != DEMO_STATE_NONE) {
            DrawSprite(SPR_DEMO_OVERLAY, 0, 18, 4, DRAW_MODE_ABSOLUTE);
        }

        if (0) {
        #define BSTR(value) ((value) ? "T" : "F")
        {
            char debugBar[41];
            word x, y, southmove;
            static word spinoff = 0;
            bool ise, isw;

            for (x = 0; x < 40; x++) {
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x, 0);
                for (y = 19; y < 25; y++) {
                    DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, x, y);
                }
            }

            /* Dump variable contents into bars at the top/bottom of the screen */

            sprintf(debugBar,
                "E%uL%02u PX=%04u PY=%04u SX=%04u SY=%04u",
                EPISODE, levelNum, playerX, playerY, scrollX, scrollY
            );
            DrawTextLine(0, 0, debugBar);
            sprintf(debugBar,
                "Score=%07lu Health=%d:%u Bomb=%u Star=%02lu",
                gameScore, playerHealth - 1, playerHealthCells, playerBombs, gameStars
            );
            DrawTextLine(0, 19, debugBar);
            sprintf(debugBar,
                "CJ=%s CJL=%s iR=%s iLJ=%s JT=%u RL=%02u PS=%u",
                BSTR(cmdJump), BSTR(cmdJumpLatch), BSTR(isPlayerRecoiling), BSTR(isPlayerLongJumping),
                playerJumpTime, playerRecoilLeft, pounceStreak
            );
            DrawTextLine(0, 21, debugBar);
            sprintf(debugBar,
                "iF=%s FT=%02d QD=%s DL=%u HC=%02u DT=%02u FDT=%02u",
                BSTR(isPlayerFalling), playerFallTime, BSTR(queuePlayerDizzy),
                playerDizzyLeft, playerHurtCooldown, playerDeadTime, playerFallDeadTime
            );
            DrawTextLine(0, 22, debugBar);
            southmove = TestPlayerMove(DIR4_SOUTH, playerX, playerY + 1);
            ise = isPlayerSlidingEast;
            isw = isPlayerSlidingWest;
            sprintf(debugBar,
                "NSWE=%u%u%u%u iSE=%s iSW=%s bAbM=%s%s cC=%s CD=%u",
                TestPlayerMove(DIR4_NORTH, playerX, playerY - 1),
                southmove,
                TestPlayerMove(DIR4_WEST, playerX - 1, playerY),
                TestPlayerMove(DIR4_EAST, playerX + 1, playerY),
                BSTR(ise), BSTR(isw), BSTR(blockActionCmds), BSTR(blockMovementCmds),
                BSTR(canPlayerCling), playerClingDir
            );
            DrawTextLine(0, 23, debugBar);

            spinoff += 8;
            if (spinoff >= 4 * 8) spinoff = 0;
            EGA_MODE_LATCHED_WRITE();
            DrawSolidTile(TILE_WAIT_SPINNER_1 + spinoff, 39 + (0 * 320));
        }
        /*#undef BSTR*/
        /*#endif*/  /* DEBUG_BAR */
        }

        SelectDrawPage(activePage);
        activePage = !activePage;
        SelectActivePage(activePage);

        if (pounceHintState == POUNCE_HINT_QUEUED) {
            pounceHintState = POUNCE_HINT_SEEN;
            ShowPounceHint();
        }

        if (winLevel) {
            winLevel = false;
            StartSound(SND_WIN_LEVEL);
            NextLevel();
            InitializeLevel(levelNum);
        } else if (winGame) {
            break;
        }
    }

    ShowEnding();
}

/*
Display the main title screen, credits (if the user waits long enough), and
main menu options. Returns a result byte indicating which demo mode the game
loop should run under.
*/
static word titleMus=MUSIC_DRUMS;
static byte TitleLoop(void)
{
#ifdef FOREIGN_ORDERS
#   define YSHIFT 1
#else
#   define YSHIFT 0
#endif  /* FOREIGN_ORDERS */

    word idlecount;
    byte scancode;
    register word junk;  /* only here to tie up the SI register */

    isNewGame = false;

title:
    StartMenuMusic(titleMus);
    DrawFullscreenImage(IMAGE_TITLE);
    idlecount = 0;
    gameTickCount = 0;

    while (!IsAnyKeyDown()) {
        WaitHard(3);
        idlecount++;

        if (idlecount == 152) goto menu;

        if (idlecount == 600) {
            DrawFullscreenImage(IMAGE_CREDITS);
        }

        if (idlecount == 1200) {
            InitializeEpisode();
            return DEMO_STATE_PLAY;
        }
    }

    scancode = WaitForAnyKey();
    if (scancode == SCANCODE_Q || scancode == SCANCODE_ESC) {
        if (PromptQuitConfirm()) ExitClean();
        goto title;
    }

    for (;;) {
menu:        
        DrawMainMenu();

getkey:
        scancode = WaitSpinner(28, 20 + YSHIFT);

        switch (scancode) {
        case SCANCODE_B:
        case SCANCODE_ENTER:
        case SCANCODE_SPACE:
            InitializeEpisode();
            isNewGame = true;
            pounceHintState = POUNCE_HINT_UNSEEN;
            StartSound(SND_NEW_GAME);
            return DEMO_STATE_NONE;
        case SCANCODE_O:
            ShowOrderingInformation();
            break;
        case SCANCODE_I:
            ShowInstructions();
            break;
        case SCANCODE_A:
            ShowPublisherBBS();
            break;
        case SCANCODE_R:
            {  /* for scope */
                byte result = PromptRestoreGame();
                if (result == RESTORE_GAME_SUCCESS) {
                    return DEMO_STATE_NONE;
                } else if (result == RESTORE_GAME_NOT_FOUND) {
                    ShowRestoreGameError();
                }
            }
            break;
        case SCANCODE_S:
            ShowStory();
            break;
        case SCANCODE_F11:
            if (isDebugMode) {
                InitializeEpisode();
                return DEMO_STATE_RECORD;
            }
            break;
        case SCANCODE_D:
            InitializeEpisode();
            return DEMO_STATE_PLAY;
        case SCANCODE_T:
            goto title;
        case SCANCODE_Q:
        case SCANCODE_ESC:
            if (PromptQuitConfirm()) ExitClean();
            break;
        case SCANCODE_C:
            DrawFullscreenImage(IMAGE_CREDITS);
            WaitForAnyKey();
            break;
        case SCANCODE_G:
            ShowGameRedefineMenu();
            break;
#ifdef FOREIGN_ORDERS
        case SCANCODE_F:
            ShowForeignOrders();
            break;
#endif  /* FOREIGN_ORDERS */
        case SCANCODE_H:
            FadeOut();
            ClearScreen();
            ShowHighScoreTable();
            break;
        case SCANCODE_F2:
            StopMusic();
            GFXTest();
            goto title;
        case SCANCODE_F3:
            StopMusic();
            LevelTest();
            goto title;
        default:
            goto getkey;
        }

        DrawFullscreenImage(IMAGE_TITLE);
    }

#undef YSHIFT
#pragma warn -use
}
#pragma warn .use

/*
Exit the program cleanly.

Saves the configuration file, restores the keyboard interrupt handler, graphics,
and AdLib. Removes the temporary save file, displays a text page, and returns to
DOS.
*/
void ExitClean(void)
{
    SaveConfigurationData(JoinPath(writePath, FILENAME_BASE ".CFG"));

    disable();
    setvect(9, savedInt9);
    enable();

    FadeOut();

    textmode(C80);

    /* Silence PC speaker */
    outportb(0x0061, inportb(0x0061) & ~0x02);

    StopAdLib();

    /* BUG: `writePath` is not considered here! */
    remove(FILENAME_BASE ".SVT");

    DrawFullscreenText(EXIT_TEXT_PAGE);

    exit(EXIT_SUCCESS);
}

/*
Main entry point for the game, after the 80286 processor test has passed. This
function never returns; the only way to end the program is for something within
the loop to call ExitClean() or a similar function to request termination.
*/
void InnerMain(int argc, char *argv[])
{
    if (argc == 2) {
        writePath = argv[1];
    } else {
        writePath = "\0";
    }

    Startup();

    for (;;) {
        demoState = TitleLoop();

        InitializeLevel(levelNum);
        mask_init = false;

        if (demoState == DEMO_STATE_PLAY) {
            LoadDemoData();
        }

        isInGame = true;
        GameLoop(demoState);
        isInGame = false;

        StopMusic();

        if (demoState != DEMO_STATE_PLAY && demoState != DEMO_STATE_RECORD) {
            CheckHighScoreAndShow();
        }

        if (demoState == DEMO_STATE_RECORD) {
            SaveDemoData();
        }
    }
}