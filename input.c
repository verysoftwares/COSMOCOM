#include "glue.h"

/*
Read the state of the keyboard/joystick for the next iteration of the game loop.

Returns a result byte that indicates if the game should end or if a level change
is needed.
*/
byte ProcessGameInput(byte demo_state)
{
    if (demo_state != DEMO_STATE_PLAY) {
        if (
            isKeyDown[SCANCODE_TAB] && isKeyDown[SCANCODE_F12] &&
            isKeyDown[SCANCODE_KP_DOT]  /* Del */
        ) {
            isDebugMode = !isDebugMode;
            StartSound(SND_PAUSE_GAME);
            WaitHard(90);
        }

        if (isKeyDown[SCANCODE_F10] && isDebugMode) {
            if (isKeyDown[SCANCODE_G]) {
                ToggleGodMode();
            }

            if (isKeyDown[SCANCODE_W]) {
                if (PromptLevelWarp()) return GAME_INPUT_RESTART;
            }

            if (isKeyDown[SCANCODE_P]) {
                StartSound(SND_PAUSE_GAME);
                while (isKeyDown[SCANCODE_P])
                    ;  /* VOID */
                while (!isKeyDown[SCANCODE_P])
                    ;  /* VOID */
                while (isKeyDown[SCANCODE_P])
                    ;  /* VOID */
            }

            if (isKeyDown[SCANCODE_M]) {
                ShowMemoryUsage();
            }

            if (isKeyDown[SCANCODE_E] && isKeyDown[SCANCODE_N] && isKeyDown[SCANCODE_D]) {
                winGame = true;
            }
        }

        if (
            isKeyDown[SCANCODE_C] && isKeyDown[SCANCODE_0] && isKeyDown[SCANCODE_F10] &&
            !usedCheatCode
        ) {
            StartSound(SND_PAUSE_GAME);
            usedCheatCode = true;
            ShowCheatMessage();
            playerHealthCells = 5;
            playerBombs = 9;
            sawBombHint = true;
            playerHealth = 6;
            UpdateBombs();
            UpdateHealth();
        }

        if (isKeyDown[SCANCODE_S]) {
            ToggleSound();
        } else if (isKeyDown[SCANCODE_M]) {
            ToggleMusic();
        } else if (isKeyDown[SCANCODE_ESC] || isKeyDown[SCANCODE_Q]) {
            if (PromptQuitConfirm()) return GAME_INPUT_QUIT;
        } else if (isKeyDown[SCANCODE_F1]) {
            byte result = ShowHelpMenu();
            if (result == HELP_MENU_RESTART) {
                return GAME_INPUT_RESTART;
            } else if (result == HELP_MENU_QUIT) {
                if (PromptQuitConfirm()) return GAME_INPUT_QUIT;
            }
        } else if (isKeyDown[SCANCODE_P]) {
            StartSound(SND_PAUSE_GAME);
            ShowPauseMessage();
        }
    } else if ((inportb(0x0060) & 0x80) == 0) {
        return GAME_INPUT_QUIT;
    }

    if (demo_state != DEMO_STATE_PLAY) {
        if (!isJoystickReady) {
            cmdWest  = isKeyDown[scancodeWest] >> blockMovementCmds;
            cmdEast  = isKeyDown[scancodeEast] >> blockMovementCmds;
            cmdJump  = isKeyDown[scancodeJump] >> blockMovementCmds;
            cmdNorth = isKeyDown[scancodeNorth];
            cmdSouth = isKeyDown[scancodeSouth];
            cmdBomb  = isKeyDown[scancodeBomb];
        } else {
            /* Returned state is not important; all global cmds get set. */
            ReadJoystickState(JOYSTICK_A);
        }

        if (blockActionCmds) {
            cmdNorth = cmdSouth = cmdBomb = false;
        }

        if (demo_state == DEMO_STATE_RECORD) {
            if (WriteDemoFrame()) return GAME_INPUT_QUIT;
        }
    } else if (ReadDemoFrame()) {
        return GAME_INPUT_QUIT;
    }

    return GAME_INPUT_CONTINUE;
}

/*
Respond to keyboard controller interrupts and update the global key states.
*/
void interrupt KeyboardInterruptService(void)
{
    lastScancode = inportb(0x0060);

    outportb(0x0061, inportb(0x0061) | 0x80);
    outportb(0x0061, inportb(0x0061) & ~0x80);

    if (lastScancode != SCANCODE_EXTENDED) {
        if ((lastScancode & 0x80) != 0) {
            isKeyDown[lastScancode & 0x7f] = false;
        } else {
            isKeyDown[lastScancode] = true;
        }
    }

    /* emergency exit for hardlocks */
    if (isKeyDown[SCANCODE_F12]) {
        StartSound(SND_BOSS_DAMAGE);
        ExitClean();
        return;
    }

    if (isKeyDown[SCANCODE_ALT] && isKeyDown[SCANCODE_C] && isDebugMode) {
        savedInt9();
    } else {
        outportb(0x0020, 0x20);
    }
}

/*
Wait indefinitely for any key to be pressed and released, then return the
scancode of that key.
*/
byte WaitForAnyKey(void)
{
    lastScancode = SCANCODE_NULL;  /* will get modified by the keyboard interrupt service */

    while ((lastScancode & 0x80) == 0)
        ;  /* VOID */

    return lastScancode & ~0x80;
}

/*
Return true if any key is currently pressed, regardless of which key it is.
*/
bbool IsAnyKeyDown(void)
{
    return !(inportb(0x0060) & 0x80);
}