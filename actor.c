#include "glue.h"

/*
Create the specified actor at the current nextActorIndex.
*/
static void ConstructActor(
    word sprite_type, word x_origin, word y_origin, bool force_active,
    bool stay_active, bool weighted, bool acrophile, ActorTickFunction tick_func,
    word data1, word data2, word data3, word data4, word data5
) {
    Actor *act;

    if (data2 == SPR_BARREL_SHARDS || data2 == SPR_BASKET_SHARDS) {
        numBarrels++;
    }

    act = actors + nextActorIndex;

    act->sprite = sprite_type;
    act->type = nextActorType;
    act->frame = 0;
    act->x = x_origin;
    act->y = y_origin;
    act->forceactive = force_active;
    act->stayactive = stay_active;
    act->weighted = weighted;
    act->acrophile = acrophile;
    act->dead = false;
    act->tickfunc = tick_func;
    act->westfree = 0;
    act->eastfree = 0;
    act->falltime = 0;
    act->data1 = data1;
    act->data2 = data2;
    act->data3 = data3;
    act->data4 = data4;
    act->data5 = data5;
    act->hurtcooldown = 0;
}

/*
Handle all common per-frame tasks for one actor.

Tasks include application of gravity, killing actors that fall off the map,
wakeup/cooldown management, interactions between the actor and the player/
explosions, calling the actor tick function, and typical-case sprite drawing.
*/
static void ProcessActor(word index)
{
    Actor *act = actors + index;

    if (level_edit && !edit_actors) return;

    if (act->dead) return;

    if (act->y > maxScrollY + scrollH + 3) {
        act->dead = true;
        return;
    }

    nextDrawMode = DRAW_MODE_NORMAL;

    if (act->hurtcooldown != 0) act->hurtcooldown--;

    if (IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        if (act->stayactive) {
            act->forceactive = true;
        }
    } else if (!act->forceactive) {
        return;
    } else {
        nextDrawMode = DRAW_MODE_HIDDEN;
    }

    if (act->weighted && !level_edit) {
        /*
        BUG: Each TestSpriteMove() call uses zero for the frame number, not the
        actual frame in view. For walking actors with variable-width sprite
        frames (specifically the pink worms) this makes them choose to walk off
        ledges in the west direction but not do so in the opposite direction.
        */
        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y) != MOVE_FREE) {
            act->y--;
            act->falltime = 0;
        }

        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) == MOVE_FREE) {
            if (act->falltime < 5) act->falltime++;

            if (act->falltime > 1 && act->falltime < 6) {
                act->y++;
            }

            if (act->falltime == 5) {
                if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
                    act->falltime = 0;
                } else {
                    act->y++;
                }
            }
        } else {
            act->falltime = 0;
        }
    }

    if (IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        nextDrawMode = DRAW_MODE_NORMAL;
    }

    if (!level_edit) {
        act->tickfunc(index);

        if (
            IsNearExplosion(act->sprite, act->frame, act->x, act->y) &&
            CanExplode(act->sprite, act->frame, act->x, act->y)
        ) {
            act->dead = true;
            return;
        }

        if (InteractPlayer(index, act->sprite, act->frame, act->x, act->y)) return;
    }

    if (nextDrawMode != DRAW_MODE_HIDDEN) {
        DrawSprite(act->sprite, act->frame, act->x, act->y, nextDrawMode);
        if (level_edit) {
            DrawNumber(index, act->x-scrollX+1, act->y-scrollY+2);
        }
    }
}

/*
Reset per-frame global actor variables, and process each actor in turn.
*/
void MoveAndDrawActors(void)
{
    word i;

    isPlayerNearHintGlobe = false;

    for (i = 0; i < numActors; i++) {
        ProcessActor(i);
    }

    if (mysteryWallTime != 0) mysteryWallTime = 0;
}
/*
Ensure that the numbered actor moved to a valid place, and adjust if not.

Actors modified by this function move left/right while negotiating slopes. The
actor should have already been (blindly) moved into the desired location, then
this function will check to see if the actor is in a good place and adjust or
revert the move if not.

Sets westfree to 0 if westward movement was blocked, and 1 if the move was valid
as-is. eastfree is changed the same way for eastward movement.

NOTE: Because this uses slightly different criteria than ProcessActor() when
determining if actors are standing on the ground at a given position, it's
possible for an actor to be permitted to walk somewhere by this code, only to be
detected as falling by ProcessActor() later. See the comment about pink worms in
the aforementioned function.
*/
static void AdjustActorMove(word index, word dir)
{
    Actor *act = actors + index;
    word offset;
    word width;
    word result = 0;

    offset = *(actorInfoData + act->sprite);
    width = *(actorInfoData + offset + 1);

    if (dir == DIR4_WEST) {
        result = TestSpriteMove(DIR4_WEST, act->sprite, act->frame, act->x, act->y);
        act->westfree = !result;

        if (act->westfree == 0 && result != MOVE_SLOPED) {
            act->x++;
        } else if (result == MOVE_SLOPED) {
            act->westfree = 1;
            act->y--;
            return;  /* shouldn't need this; only here for jump target parity */
        } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, act->frame, act->x, act->y + 1) > MOVE_FREE) {
            act->westfree = 1;
        } else if (
            TILE_SLOPED(GetMapTile(act->x + width, act->y + 1)) &&
            TILE_SLOPED(GetMapTile(act->x + width - 1, act->y + 2))
        ) {
            if (!TILE_BLOCK_SOUTH(GetMapTile(act->x + width - 1, act->y + 1))) {
                act->westfree = 1;
                if (!TILE_SLOPED(GetMapTile(act->x + width - 1, act->y + 1))) {
                    act->y++;
                }
            }
        } else if (act->westfree == 0) {
            act->x++;
        } else if (
            !act->acrophile &&
            TestSpriteMove(DIR4_WEST, act->sprite, act->frame, act->x, act->y + 1) == MOVE_FREE &&
            !TILE_SLOPED(GetMapTile(act->x + width - 1, act->y + 1))
        ) {
            act->x++;
            act->westfree = 0;
        }
    } else {  /* DIR4_EAST */
        result = TestSpriteMove(DIR4_EAST, act->sprite, act->frame, act->x, act->y);
        act->eastfree = !result;

        if (act->eastfree == 0 && result != MOVE_SLOPED) {
            act->x--;
        } else if (result == MOVE_SLOPED) {
            act->eastfree = 1;
            act->y--;
            return;  /* shouldn't need this; only here for jump target parity */
        } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, act->frame, act->x, act->y + 1) > MOVE_FREE) {
            act->eastfree = 1;
        } else if (
            TILE_SLOPED(GetMapTile(act->x - 1, act->y + 1)) &&
            TILE_SLOPED(GetMapTile(act->x, act->y + 2))
        ) {
            if (!TILE_BLOCK_SOUTH(GetMapTile(act->x, act->y + 1))) {
                act->eastfree = 1;
                if (!TILE_SLOPED(GetMapTile(act->x, act->y + 1))) {
                    act->y++;
                }
            }
        } else if (act->eastfree == 0) {
            act->x--;
        } else if (
            !act->acrophile &&
            TestSpriteMove(DIR4_EAST, act->sprite, act->frame, act->x, act->y + 1) == MOVE_FREE &&
            !TILE_SLOPED(GetMapTile(act->x, act->y + 1))
        ) {
            act->x--;
            act->eastfree = 0;
        }
    }
}

/*
Handle one frame of foot switch movement.
*/
static void ActFootSwitch(word index)
{
    Actor *act = actors + index;

    /*
    This function is used for a variety of functionless actors, and in those
    cases it behaves as a no-op.
    */
    if (act->sprite != SPR_FOOT_SWITCH_KNOB) return;

    /* extra data var */
    if (act->westfree == 0) {
        act->westfree = 1;
        /*
        BUG: TILE_SWITCH_BLOCK_* have a horizontal white line on the top edge
        which is not appropriate for the design of the topmost switch position.
        When a switch is pounced for the first time, this is visible for one
        frame until this function runs and adjusts it to TILE_SWITCH_FREE_*N.
        */
        SetMapTile4(
            TILE_SWITCH_BLOCK_1, TILE_SWITCH_BLOCK_2, TILE_SWITCH_BLOCK_3,
            TILE_SWITCH_BLOCK_4, act->x, act->y
        );
    }

    if (act->data4 != 0) {
        act->data4 = 0;
        /* data3 is 64 on first pounce, for TILE_SWITCH_FREE_1N. 0 after. */
        SetMapTile4(
            (TILE_SWITCH_FREE_1L - act->data3),
            (TILE_SWITCH_FREE_1L - act->data3) + 8,
            (TILE_SWITCH_FREE_1L - act->data3) + 16,
            (TILE_SWITCH_FREE_1L - act->data3) + 24,
            act->x, act->y
        );
        act->y++;
        SetMapTile4(
            TILE_SWITCH_BLOCK_1, TILE_SWITCH_BLOCK_2, TILE_SWITCH_BLOCK_3,
            TILE_SWITCH_BLOCK_4, act->x, act->y
        );

        if (act->data1 == 4) {
            StartSound(SND_FOOT_SWITCH_ON);

            switch (act->data5) {
            case ACT_SWITCH_PLATFORMS:
                arePlatformsActive = true;
                break;

            case ACT_SWITCH_MYSTERY_WALL:
                mysteryWallTime = 4;  /* no significance; could've been bool */
                if (!sawMysteryWallBubble) {
                    sawMysteryWallBubble = true;
                    NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
                }
                break;

            case ACT_SWITCH_LIGHTS:
                areLightsActive = true;
                break;

            case ACT_SWITCH_FORCE_FIELD:
                areForceFieldsActive = false;
                break;
            }
        } else {
            StartSound(SND_FOOT_SWITCH_MOVE);
        }
    }

    if (
        act->data1 < 4 && act->data4 == 0 &&
        IsNearExplosion(SPR_FOOT_SWITCH_KNOB, 0, act->x, act->y)
    ) {
        act->data1++;

        if (act->data2 == 0) {
            act->data3 = 64;
            act->data2 = 1;
        } else {
            act->data3 = 0;
        }

        act->data4 = 1;
    }
}

/*
Handle one frame of horizontal saw blade/sharp robot movement.
*/
static void ActHorizontalMover(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;

    if (act->sprite == SPR_SAW_BLADE) {
        act->data3 = 1;

        if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
            StartSound(SND_SAW_BLADE_MOVE);
        }
    }

    if (act->data4 != 0) act->data4--;

    if (act->data3 == 0) return;

    if (act->data4 == 0) {
        if (act->data2 != DIR2_WEST) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->eastfree == 0) {
                act->data2 = DIR2_WEST;
                act->data4 = act->data1;
            }
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->westfree == 0) {
                act->data2 = DIR2_EAST;
                act->data4 = act->data1;
            }
        }
    }

    act->frame++;
    if (act->frame > act->data5) act->frame = 0;
}

/*
Handle one frame of floor/ceiling jump pad movement.
*/
static void ActJumpPad(word index)
{
    Actor *act = actors + index;

    if (act->data1 > 0) {
        act->frame = 1;
        act->data1--;
    } else {
        act->frame = 0;
    }

    if (act->data5 != 0) {
        nextDrawMode = DRAW_MODE_FLIPPED;

        if (act->frame == 0) {
            act->y = act->data3;
        } else {
            act->y = act->data4;
        }
    }
}

/*
Handle one frame of arrow piston movement.
*/
static void ActArrowPiston(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 31) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (
        (act->data1 == 29 || act->data1 == 26) &&
        IsSpriteVisible(act->sprite, 0, act->x, act->y)
    ) {
        StartSound(SND_SPIKES_MOVE);
    }

    if (act->data5 == DIR2_WEST) {
        if (act->data1 > 28) {
            act->x++;
        } else if (act->data1 > 25) {
            act->x--;
        }
    } else {
        if (act->data1 > 28) {
            act->x--;
        } else if (act->data1 > 25) {
            act->x++;
        }
    }
}

/*
Handle one frame of fireball movement.

NOTE: This actor consists only of the fireball, which spends its idle frames
hidden behind map tiles. The "launchers" are solid map tiles.
*/
static void ActFireball(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 29) {
        StartSound(SND_FIREBALL_LAUNCH);
    }

    if (act->data1 < 30) {
        act->data1++;
    } else {
        if (act->data5 == DIR2_WEST) {
            act->x--;
            act->westfree = !TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x, act->y);
            if (act->westfree == 0) {
                act->data1 = 0;
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTH, 1);
                act->x = act->data2;
                act->y = act->data3;
                StartSound(SND_BIG_OBJECT_HIT);
            }
        } else {
            act->x++;
            act->eastfree = !TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x, act->y);
            if (act->eastfree == 0) {
                act->data1 = 0;
                NewDecoration(SPR_SMOKE, 6, act->x - 2, act->y, DIR8_NORTH, 1);
                act->x = act->data2;
                act->y = act->data3;
                StartSound(SND_BIG_OBJECT_HIT);
            }
        }
    }

    if (!IsSpriteVisible(act->sprite, act->frame, act->x, act->y)) {
        act->data1 = 0;
        act->x = act->data2;
        act->y = act->data3;
    }

    act->frame = !act->frame;
}

/*
Look for any door(s) linked to the passed switch, and unlock them if needed.
*/
static void UpdateDoors(word door_sprite, Actor *act_switch)
{
    word i, y;

    for (i = 0; i < numActors; i++) {
        Actor *door = actors + i;

        if (door->sprite != door_sprite) continue;

        if (act_switch->data1 == 2) {
            door->dead = true;
            StartSound(SND_DOOR_UNLOCK);

            NewDecoration(door_sprite, 1, door->x, door->y, DIR8_SOUTH, 5);
        } else if (act_switch->data1 == 1) {
            for (y = 0; y < 5; y++) {
                SetMapTile(*((word *)&door->data1 + y), door->x + 1, door->y - y);
            }
        }
    }
}

/*
Handle one frame of head switch movement.
*/
static void ActHeadSwitch(word index)
{
    Actor *act = actors + index;

    if (act->frame == 1) {
        if (act->data1 < 3) act->data1++;

        UpdateDoors(act->data5, act);
    }
}

/*
Handle first-time initialization of a door, then become a no-op.
*/
static void ActDoor(word index)
{
    word y;
    Actor *act = actors + index;

    /* extra data var */
    if (act->westfree != 0) return;

    act->westfree = 1;

    for (y = 0; y < 5; y++) {
        *((word *)&act->data1 + y) = GetMapTile(act->x + 1, act->y - y);

        SetMapTile(TILE_DOOR_BLOCK, act->x + 1, act->y - y);
    }
}

/*
Handle one frame of jump pad robot movement.
*/
static void ActJumpPadRobot(word index)
{
    Actor *act = actors + index;

    if (act->data1 > 0) {
        act->frame = 2;
        act->data1--;
    } else {
        act->frame = !act->frame;

        if (act->data2 != DIR2_WEST) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->eastfree == 0) {
                act->data2 = DIR2_WEST;
            }
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->westfree == 0) {
                act->data2 = DIR2_EAST;
            }
        }
    }

    if (!IsSpriteVisible(SPR_JUMP_PAD_ROBOT, 2, act->x, act->y)) {
        act->frame = 0;
    }
}

/*
Handle one frame of reciprocating spike movement.
*/
static void ActReciprocatingSpikes(word index)
{
    Actor *act = actors + index;

    act->data2++;
    if (act->data2 == 20) act->data2 = 0;

    if (act->frame == 0 && act->data2 == 0) {
        act->data1 = 0;
        StartSound(SND_SPIKES_MOVE);

    } else if (act->frame == 2 && act->data2 == 0) {
        act->data1 = 1;
        StartSound(SND_SPIKES_MOVE);
        nextDrawMode = DRAW_MODE_HIDDEN;

    } else if (act->data1 != 0) {
        if (act->frame > 0) act->frame--;

    } else if (act->frame < 2) {
        act->frame++;
    }

    if (act->frame == 2) {
        nextDrawMode = DRAW_MODE_HIDDEN;
    }
}

/*
Handle one frame of vertical saw blade movement.
*/
static void ActVerticalMover(word index)
{
    Actor *act = actors + index;

    act->frame = !act->frame;

    if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
        StartSound(SND_SAW_BLADE_MOVE);
    }

    if (act->data1 != DIR2_SOUTH) {
        if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
            act->data1 = DIR2_SOUTH;
        } else {
            act->y--;
        }
    } else {
        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->data1 = DIR2_NORTH;
        } else {
            act->y++;
        }
    }
}

/*
Handle one frame of armed bomb movement.
*/
static void ActBombArmed(word index)
{
    Actor *act = actors + index;

    if (act->frame == 3) {
        act->data2++;
        act->data1++;
        if (act->data1 % 2 != 0 && act->frame == 3) {
            nextDrawMode = DRAW_MODE_WHITE;
        }

        if (act->data2 == 10) {
            act->dead = true;
            NewPounceDecoration(act->x - 2, act->y + 2);
            nextDrawMode = DRAW_MODE_HIDDEN;
            NewExplosion(act->x - 2, act->y);
            if (act->data1 % 2 != 0 && act->frame == 3) {
                DrawSprite(SPR_BOMB_ARMED, act->frame, act->x, act->y, DRAW_MODE_WHITE);
            }
        }
    } else {
        act->data1++;
        if (act->data1 == 5) {
            act->data1 = 0;
            act->frame++;
        }
    }

    if (TestSpriteMove(DIR4_SOUTH, SPR_BOMB_ARMED, 0, act->x, act->y) != MOVE_FREE) {
        act->y--;
    }
}

/*
Handle one frame of barrel movement.
*/
static void ActBarrel(word index)
{
    Actor *act = actors + index;

    if (IsNearExplosion(SPR_BARREL, 0, act->x, act->y)) {
        DestroyBarrel(index);
        AddScore(1600);
        NewActor(ACT_SCORE_EFFECT_1600, act->x, act->y);
    }
}

/*
Handle one frame of cabbage movement.
*/
static void ActCabbage(word index)
{
    Actor *act = actors + index;

    if (
        act->data2 == 10 && act->data3 == 3 &&
        TestSpriteMove(DIR4_SOUTH, SPR_CABBAGE, 0, act->x, act->y + 1) == MOVE_FREE
    ) {
        if (act->data4 != 0) {
            act->frame = 3;
        } else {
            act->frame = 1;
        }

    } else if (
        act->data2 < 10 &&
        TestSpriteMove(DIR4_SOUTH, SPR_CABBAGE, 0, act->x, act->y + 1) != MOVE_FREE
    ) {
        act->data2++;
        if (act->x > playerX) {
            act->data4 = act->frame = 0;
        } else {
            act->data4 = act->frame = 2;
        }

    } else if (act->data3 < 3) {
        static signed char yjump[] = {-1, -1, 0};
        act->y += yjump[act->data3];

        if (act->data4 != 0) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
        } else {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
        }

        act->data3++;
        if (act->data4 != 0) {
            act->frame = 3;
        } else {
            act->frame = 1;
        }

    } else {
        act->data2 = 0;
        act->data3 = 0;
        if (act->x > playerX) {
            act->data4 = act->frame = 0;
        } else {
            act->data4 = act->frame = 2;
        }
    }
}

/*
Handle one frame of reciprocating spear movement.
*/
static void ActReciprocatingSpear(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 30) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (act->data1 > 22) {
        act->y--;
    } else if (act->data1 > 14) {
        act->y++;
    }
}

/*
Handle one frame of ged/green slime movement.

The repeat rate of dripping slime actors is directly related to how far they
have to travel before falling off the bottom of the screen.
*/
static void ActRedGreenSlime(word index)
{
    static word throbframes[] = {0, 1, 2, 3, 2, 1, 0};
    Actor *act = actors + index;

    if (act->data5 != 0) {  /* throb and drip */
        if (act->data4 == 0) {
            act->frame = throbframes[act->data3 % 6];
            act->data3++;

            if (act->data3 == 15) {
                act->data4 = 1;
                act->data3 = 0;
                act->frame = 4;
                if (IsSpriteVisible(SPR_GREEN_SLIME, 6, act->x, act->data2)) {
                    StartSound(SND_DRIP);
                }
            }

        } else if (act->frame < 6) {
            act->frame++;

        } else {
            act->y++;
            if (!IsSpriteVisible(SPR_GREEN_SLIME, 6, act->x, act->y)) {
                act->y = act->data2;
                act->data4 = 0;
                act->frame = 0;
            }
        }

    } else {  /* throb */
        act->frame = throbframes[act->data3];
        act->data3++;

        if (act->data3 == 6) {
            act->data3 = 0;
        }
    }
}

/*
Handle one frame of flying wisp movement.
*/
static void ActFlyingWisp(word index)
{
    Actor *act = actors + index;

    act->frame = !act->frame;

    if (act->data1 < 63) {
        act->data1++;
    } else {
        act->data1 = 0;
    }

    if (act->data1 > 50) {
        act->y += 2;

        if (act->data1 < 55) {
            act->y--;
        }

        nextDrawMode = DRAW_MODE_FLIPPED;

    } else if (act->data1 > 34) {
        if (act->data1 < 47) {
            act->y--;
        }

        if (act->data1 < 45) {
            act->y--;
        }
    }
}

/*
Handle one frame of "Two Tons" crusher movement.
*/
static void ActTwoTonsCrusher(word index)
{
    Actor *act = actors + index;

    if (act->data1 < 20) {
        act->data1++;
    }

    if (act->data1 == 19) {
        act->data2 = 1;
    }

    if (act->data2 == 1) {
        if (act->frame < 3) {
            act->frame++;

            switch (act->frame) {
            case 1:
                act->data3 = 1;
                break;
            case 2:
                act->data3 = 2;
                break;
            case 3:
                act->data3 = 4;
                break;
            }

            act->y += act->data3;

        } else {
            act->data2 = 2;

            if (IsSpriteVisible(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3)) {
                StartSound(SND_OBJECT_HIT);
            }
        }
    }

    if (act->data2 == 2) {
        if (act->frame > 0) {
            act->frame--;

            switch (act->frame) {
            case 0:
                act->data3 = 1;
                break;
            case 1:
                act->data3 = 2;
                break;
            case 2:
                act->data3 = 4;
                break;
            }

            act->y -= act->data3;

        } else {
            act->data2 = 0;
            act->data1 = 0;
            act->data3 = 0;
        }
    }

    if (IsTouchingPlayer(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3)) {
        HurtPlayer(index);
    }

    DrawSprite(SPR_TWO_TONS_CRUSHER, 4, act->x - 1, act->y + 3, DRAW_MODE_NORMAL);
}

/*
Handle one frame of jumping bullet movement.
*/
static void ActJumpingBullet(word index)
{
    static int yjump[] = {-2, -2, -2, -2, -1, -1, -1, 0, 0, 1, 1, 1, 2, 2, 2, 2};
    Actor *act = actors + index;

    if (act->data2 == DIR2_WEST) {
        act->x--;
    } else {
        act->x++;
    }

    act->y += yjump[act->data3];
    act->data3++;

    if (act->data3 == 16) {
        act->data2 = !act->data2;  /* flip between west and east */

        if (IsSpriteVisible(SPR_JUMPING_BULLET, 0, act->x, act->y)) {
            StartSound(SND_OBJECT_HIT);
        }

        act->data3 = 0;
    }
}

/*
Handle one frame of stone head crusher movement.
*/
static void ActStoneHeadCrusher(word index)
{
    Actor *act = actors + index;

    act->data4 = !act->data4;

    if (act->data1 == 0) {
        if (act->y < playerY && act->x <= playerX + 6 && act->x + 7 > playerX) {
            act->data1 = 1;
            act->data2 = act->y;
            act->frame = 1;
        } else {
            act->frame = 0;
        }

    } else if (act->data1 == 1) {
        act->frame = 1;
        act->y++;

        if (TestSpriteMove(DIR4_SOUTH, SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y) != MOVE_FREE) {
            act->data1 = 2;

            if (IsSpriteVisible(SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y)) {
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTHEAST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            }

            act->y--;

        } else {
            act->y++;

            if (TestSpriteMove(DIR4_SOUTH, SPR_STONE_HEAD_CRUSHER, 0, act->x, act->y) != MOVE_FREE) {
                act->data1 = 2;

                /* Possible BUG: Missing IsSpriteVisible() causes offscreen sound? */
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y, DIR8_NORTHEAST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);

                act->y--;
            }
        }

    } else if (act->data1 == 2) {
        act->frame = 0;

        if (act->y == act->data2) {
            act->data1 = 0;
        } else if (act->data4 != 0) {
            act->y--;
        }
    }
}

/*
Handle one frame of pyramid movement.
*/
static void ActPyramid(word index)
{
    Actor *act = actors + index;

    if (act->data5 != 0) {  /* floor mounted */
        nextDrawMode = DRAW_MODE_FLIPPED;

    } else if (act->data1 == 0) {
        if (act->y < playerY && act->x <= playerX + 6 && act->x + 5 > playerX) {
            act->data1 = 1;
            act->weighted = true;
        }

    } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
        act->dead = true;
        NewDecoration(SPR_SMOKE, 6, act->x, act->y, DIR8_NORTH, 3);
        StartSound(SND_BIG_OBJECT_HIT);
        nextDrawMode = DRAW_MODE_HIDDEN;
    }

    if (!act->dead) {
        /* BUG? Non-falling pyramids use a different function; don't propagate explosions */
        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            act->data2 = 3;
        }

        if (act->data2 != 0) {
            act->data2--;

            if (act->data2 == 0) {
                NewExplosion(act->x - 1, act->y + 1);
                act->dead = true;
                AddScore(200);
                NewShard(act->sprite, 0, act->x, act->y);
            }
        }
    }
}

/*
Handle one frame of ghost movement.
*/
static void ActGhost(word index)
{
    Actor *act = actors + index;

    act->data4++;
    if (act->data4 % 3 == 0) {
        act->data1++;
    }

    if (act->data1 == 4) act->data1 = 0;

    if (playerBaseFrame == PLAYER_BASE_WEST) {
        if (act->x > playerX + 2 && playerClingDir == DIR4_WEST && cmdEast) {
            act->frame = (random(35) == 0 ? 4 : 0) + 2;
        } else if (act->x > playerX) {
            act->frame = act->data1 % 2;
            if (act->data1 == 0) {
                act->x--;
                if (act->y < playerY) {
                    act->y++;
                } else if (act->y > playerY) {
                    act->y--;
                }
            }
        } else {
            act->frame = (random(35) == 0 ? 2 : 0) + 5;
        }
    } else {
        if (act->x < playerX && playerClingDir == DIR4_EAST && cmdWest) {
            act->frame = (random(35) == 0 ? 2 : 0) + 5;
        } else if (act->x < playerX) {
            act->frame = (act->data1 % 2) + 3;
            if (act->data1 == 0) {
                act->x++;
                if (act->y < playerY) {
                    act->y++;
                } else if (act->y > playerY) {
                    act->y--;
                }
            }
        } else {
            act->frame = (random(35) == 0 ? 4 : 0) + 2;
        }
    }
}

/*
Handle one frame of moon movement.
*/
static void ActMoon(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;

    if (act->data3 == 0) {
        act->data2++;

        if (act->x < playerX) {
            act->frame = (act->data2 % 2) + 2;
        } else {
            act->frame = act->data2 % 2;
        }
    }
}

/*
Handle one frame of heart plant movement.
*/
static void ActHeartPlant(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0 && act->y > playerY && act->x == playerX) {
        act->data1 = 1;
    }

    if (act->data1 == 1) {
        act->data2++;

        if (act->data2 == 2) {
            act->data2 = 0;
            act->frame++;

            if (act->frame == 3) {
                act->data1 = 0;
                act->frame = 0;
            }

            if (act->frame == 1) {
                act->x--;
                StartSound(SND_PLANT_MOUTH_OPEN);
            }

            if (act->frame == 2) {
                act->x++;
            }
        }
    }
}

/*
Handle one frame of idle bomb movement.
*/
static void ActBombIdle(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 2) {
        NewExplosion(act->x - 2, act->y);
        act->dead = true;

    } else {
        if (act->data1 != 0) act->data1++;

        if (act->data1 == 0 && IsNearExplosion(SPR_BOMB_IDLE, 0, act->x, act->y)) {
            act->data1 = 1;
        }
    }
}

/*
Set map tile at x,y to `value`.
*/
void SetMapTile(word value, word x, word y)
{
    MAP_CELL_DATA(x, y) = value;
}

/*
Handle one frame of mystery wall movement.
*/
static void ActMysteryWall(word index)
{
    Actor *act = actors + index;

    if (mysteryWallTime != 0) {
        act->data1 = 1;
        act->forceactive = true;
    }

    if (act->data1 == 0) return;

    if (act->data1 % 2 != 0) {
        SetMapTile(TILE_MYSTERY_BLOCK_NW, act->x,     act->y - 1);
        SetMapTile(TILE_MYSTERY_BLOCK_NE, act->x + 1, act->y - 1);
        SetMapTile(TILE_MYSTERY_BLOCK_SW, act->x,     act->y);
        SetMapTile(TILE_MYSTERY_BLOCK_SE, act->x + 1, act->y);
    }

    if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
        if (act->data1 % 2 == 0) {
            SetMapTile(TILE_MYSTERY_BLOCK_SW, act->x,     act->y - 1);
            SetMapTile(TILE_MYSTERY_BLOCK_SE, act->x + 1, act->y - 1);
        }

        act->dead = true;

    } else {
        if (act->data1 % 2 == 0) {
            NewDecoration(SPR_SPARKLE_SHORT, 4, act->x - 1, act->y - 1, DIR8_NONE, 1);
        }

        act->data1++;
        act->y--;
    }
}

/*
Handle one frame of baby ghost movement.
*/
static void ActBabyGhost(word index)
{
    Actor *act = actors + index;

    if (act->data4 != 0) {
        act->data4--;

    } else if (act->data1 == DIR2_SOUTH) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_BABY_GHOST, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->weighted = false;
            act->data1 = DIR2_NORTH;
            act->data4 = 3;
            act->data2 = 4;
            act->frame = 1;
            act->data3 = 1;

            if (IsSpriteVisible(SPR_BABY_GHOST, 0, act->x, act->y)) {
                StartSound(SND_BABY_GHOST_LAND);
            }

        } else if (act->data5 == 0) {
            act->frame = 1;
            if (act->data3 == 0) {
                act->data4++;
            }

        } else {
            act->data5--;
        }

    } else if (act->data1 == DIR2_NORTH) {
        act->y--;
        act->frame = 0;

        if (act->data2 == 4 && IsSpriteVisible(SPR_BABY_GHOST, 0, act->x, act->y)) {
            StartSound(SND_BABY_GHOST_JUMP);
        }

        act->data2--;
        if (act->data2 == 0) {
            act->data1 = DIR2_SOUTH;
            act->data5 = 3;
            act->weighted = true;
        }
    }
}

/*
Handle one frame of projectile movement.

Projectiles use a unique DIRP_* system that is incompatible with DIR8_*.
*/
static void ActProjectile(word index)
{
    Actor *act = actors + index;

    if (!IsSpriteVisible(SPR_PROJECTILE, 0, act->x, act->y)) {
        act->dead = true;
        return;
    }

    if (act->data1 == 0) {
        act->data1 = 1;
        StartSound(SND_PROJECTILE_LAUNCH);
    }

    act->frame = !act->frame;

    switch (act->data5) {
    case DIRP_WEST:
        act->x--;
        break;

    case DIRP_SOUTHWEST:
        act->x--;
        act->y++;
        break;

    case DIRP_SOUTH:
        act->y++;
        break;

    case DIRP_SOUTHEAST:
        act->x++;
        act->y++;
        break;

    case DIRP_EAST:
        act->x++;
        break;
    }
}

/*
Handle one frame of roamer slug movement.
*/
static void ActRoamerSlug(word index)
{
    Actor *act = actors + index;

    if (act->data5 == 0) {
        switch (act->data1) {
        case DIR4_NORTH:
            if (TestSpriteMove(DIR4_NORTH, SPR_ROAMER_SLUG, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->y--;
            }
            act->data3 = 0;
            break;

        case DIR4_SOUTH:
            if (TestSpriteMove(DIR4_SOUTH, SPR_ROAMER_SLUG, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->y++;
            }
            act->data3 = 4;
            break;

        case DIR4_WEST:
            if (TestSpriteMove(DIR4_WEST, SPR_ROAMER_SLUG, 0, act->x - 1, act->y) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->x--;
            }
            act->data3 = 6;
            break;

        case DIR4_EAST:
            if (TestSpriteMove(DIR4_EAST, SPR_ROAMER_SLUG, 0, act->x + 1, act->y) != MOVE_FREE) {
                act->data5 = 1;
            } else {
                act->x++;
            }
            act->data3 = 2;
            break;
        }

    } else {
        word newdir = GameRand() % 4;

        if (
            newdir == DIR4_NORTH &&
            TestSpriteMove(DIR4_NORTH, SPR_ROAMER_SLUG, 0, act->x, act->y - 1) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_NORTH;
        }

        if (
            newdir == DIR4_SOUTH &&
            TestSpriteMove(DIR4_SOUTH, SPR_ROAMER_SLUG, 0, act->x, act->y + 1) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_SOUTH;
        }

        if (
            newdir == DIR4_WEST &&
            TestSpriteMove(DIR4_WEST, SPR_ROAMER_SLUG, 0, act->x - 1, act->y) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_WEST;
        }

        if (
            newdir == DIR4_EAST &&
            TestSpriteMove(DIR4_EAST, SPR_ROAMER_SLUG, 0, act->x + 1, act->y) == MOVE_FREE
        ) {
            act->data5 = 0;
            act->data1 = DIR4_EAST;
        }
    }

    act->data4 = !act->data4;
    act->frame = act->data3 + act->data4;
}

/*
Pipe corner actors do not perform any meaningful action.
*/
static void ActPipeCorner(word index)
{
    (void) index;

    nextDrawMode = DRAW_MODE_HIDDEN;
}

/*
Handle one frame of baby ghost egg movement.
*/
static void ActBabyGhostEgg(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->frame = 2;
    } else if (GameRand() % 70 == 0 && act->data3 == 0) {
        act->data3 = 2;
    } else {
        act->frame = 0;
    }

    if (act->data3 != 0) {
        act->data3--;
        act->frame = 1;
    }

    if (
        act->data5 == 0 && act->data1 == 0 &&
        act->y <= playerY && act->x - 6 < playerX && act->x + 4 > playerX
    ) {
        act->data1 = 1;
        act->data2 = 20;
        StartSound(SND_BGHOST_EGG_CRACK);
    }

    if (act->data2 > 1) {
        act->data2--;
    } else if (act->data2 == 1) {
        act->dead = true;
        nextDrawMode = DRAW_MODE_HIDDEN;

        NewActor(ACT_BABY_GHOST, act->x, act->y);
        NewDecoration(SPR_BGHOST_EGG_SHARD_1, 1, act->x,     act->y - 1, DIR8_NORTHWEST, 5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_2, 1, act->x + 1, act->y - 1, DIR8_NORTHEAST, 5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_3, 1, act->x,     act->y,     DIR8_EAST,      5);
        NewDecoration(SPR_BGHOST_EGG_SHARD_4, 1, act->x + 1, act->y,     DIR8_WEST,      5);
        StartSound(SND_BGHOST_EGG_HATCH);
    }
}

/*
Handle one frame of sharp robot movement.
*/
static void ActSharpRobot(word index)
{
    Actor *act = actors + index;

    act->data3 = !act->data3;
    if (act->data3 == 0) return;

    if (act->data4 != 0) {
        act->data4--;

    } else if (act->data2 == DIR2_EAST) {
        if (
            TestSpriteMove(DIR4_EAST, SPR_SHARP_ROBOT_CEIL, 0, act->x + 1, act->y) != MOVE_FREE ||
            TestSpriteMove(DIR4_EAST, SPR_SHARP_ROBOT_CEIL, 0, act->x + 1, act->y - 1) == MOVE_FREE
        ) {
            act->data4 = 4;
            act->data2 = DIR2_WEST;
        } else {
            act->x++;
        }

    } else {
        if (
            TestSpriteMove(DIR4_WEST, SPR_SHARP_ROBOT_CEIL, 0, act->x - 1, act->y) != MOVE_FREE ||
            TestSpriteMove(DIR4_WEST, SPR_SHARP_ROBOT_CEIL, 0, act->x - 1, act->y - 1) == MOVE_FREE
        ) {
            act->data4 = 4;
            act->data2 = DIR2_EAST;
        } else {
            act->x--;
        }
    }

    act->frame = !act->frame;
}

/*
Handle one frame of clam plant movement.
*/
static void ActClamPlant(word index)
{
    Actor *act = actors + index;

    nextDrawMode = act->data5;

    if (act->data2 == 1) {
        act->frame++;
        if (act->frame == 1) {
            StartSound(SND_PLANT_MOUTH_OPEN);
        }

        if (act->frame == 4) {
            act->data2 = 2;
        }

    } else if (act->data2 == 2) {
        act->frame--;
        if (act->frame == 1) {
            act->data2 = 0;
            act->data1 = 1;
        }

    } else {
        if (act->data1 < 16) {
            act->data1++;
        } else {
            act->data1 = 0;
        }

        if (act->data1 == 0) {
            act->data2 = 1;
        } else {
            act->frame = 0;
        }
    }
}

/*
Handle one frame of parachute ball movement.

NOTE: This uses a direction system that is incompatible with DIR2_*.
*/
static void ActParachuteBall(word index)
{
    Actor *act = actors + index;

    if (act->falltime != 0) {
        act->data1 = 0;
        act->data2 = 20;

        if (act->falltime < 2) {
            act->frame = 1;  /* no purpose */
        } else if (act->falltime >= 2 && act->falltime <= 4) {
            DrawSprite(SPR_PARACHUTE_BALL, 8, act->x, act->y - 2, DRAW_MODE_NORMAL);
        } else {
            act->y--;
            DrawSprite(SPR_PARACHUTE_BALL, 9, act->x, act->y - 2, DRAW_MODE_NORMAL);
        }

        act->frame = 10;

        return;
    }

    if (act->data1 == 0) {
        static byte idleframes[] = {
            2, 2, 2, 0, 3, 3, 3, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 3, 3, 3, 0, 1, 1, 0, 1, 1, 1
        };
        act->data2++;
        act->frame = idleframes[act->data2];

        if (act->data2 == 26) {
            act->data2 = 0;

            if (act->y == playerY || GameRand() % 2 == 0) {
                if (act->x >= playerX + 2) {
                    act->data1 = 1;  /* W */
                    act->data2 = 0;
                    act->frame = 2;
                    act->data3 = 6;
                } else if (act->x + 2 <= playerX) {
                    act->data1 = 2;  /* E */
                    act->data2 = 0;
                    act->frame = 3;
                    act->data3 = 6;
                }
            }
        }
    }

    if (act->data3 != 0) {
        act->data3--;

    } else if (act->data1 == 1) {  /* W */
        act->x--;
        AdjustActorMove(index, DIR4_WEST);
        if (act->westfree == 0) {
            act->data1 = 0;
            act->data2 = 0;
            act->frame = 0;
        } else {
            static byte frames[] = {7, 6, 5, 4};
            act->frame = frames[act->data2 % 4];
            act->data2++;
            if (act->data2 == 16) {
                act->data1 = 0;
                act->data2 = 0;
            }
        }

    } else if (act->data1 == 2) {  /* E */
        act->x++;
        AdjustActorMove(index, DIR4_EAST);
        if (act->eastfree == 0) {
            act->data1 = 0;
            act->data2 = 0;
            act->frame = 0;
        } else {
            static byte frames[] = {4, 5, 6, 7};
            act->frame = frames[act->data2 % 4];
            act->data2++;
            if (act->data2 == 12) {
                act->data1 = 0;
                act->data2 = 0;
            }
        }
    }
}

/*
Handle one frame of beam robot movement.

NOTE: This uses a direction system that is incompatible with DIR2_*.
*/
static void ActBeamRobot(word index)
{
    static word beamframe = 0;
    Actor *act = actors + index;
    int i;

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (act->data2 != 0) {
        for (i = 0; act->data2 > (word)i; i += 4) {
            NewExplosion(act->x, act->y - i);
            NewActor(ACT_STAR_FLOAT, act->x, act->y - i);
        }

        act->dead = true;

        return;
    }

    act->data5 = !act->data5;
    act->data4++;

    if (act->data1 != 0) {  /* != E */
        if (act->data4 % 2 != 0) {
            act->x--;
        }

        AdjustActorMove(index, DIR4_WEST);
        if (act->westfree == 0) {
            act->data1 = 0;  /* E */
        }
    } else {
        if (act->data4 % 2 != 0) {
            act->x++;
        }

        AdjustActorMove(index, DIR4_EAST);
        if (act->eastfree == 0) {
            act->data1 = 1;  /* W */
        }
    }

    DrawSprite(SPR_BEAM_ROBOT, act->data5, act->x, act->y, DRAW_MODE_NORMAL);
    if (IsTouchingPlayer(SPR_BEAM_ROBOT, 0, act->x, act->y)) {
        HurtPlayer(index);
    }

    beamframe++;

    for (i = 2; i < 21; i++) {
        if (TestSpriteMove(DIR4_NORTH, SPR_BEAM_ROBOT, 2, act->x + 1, act->y - i) != MOVE_FREE) break;

        DrawSprite(SPR_BEAM_ROBOT, (beamframe % 4) + 4, act->x + 1, act->y - i, DRAW_MODE_NORMAL);

        if (IsTouchingPlayer(SPR_BEAM_ROBOT, 4, act->x + 1, act->y - i)) {
            HurtPlayer(index);
        }
    }
    /* value left in i is used below! */

    DrawSprite(SPR_BEAM_ROBOT, act->data5 + 2, act->x + 1, (act->y - i) + 1, DRAW_MODE_NORMAL);

    if (IsTouchingPlayer(SPR_BEAM_ROBOT, 0, act->x, act->y + 1)) {
        HurtPlayer(index);
    }

    if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        act->data2 = i;
    }
}

/*
Handle one frame of splitting platform movement.
*/
static void ActSplittingPlatform(word index)
{
    Actor *act = actors + index;

    /* extra data var */
    act->westfree++;

    if (act->data1 == 0) {
        act->data1 = 1;
        SetMapTileRepeat(TILE_BLUE_PLATFORM, 4, act->x, act->y - 1);

    } else if (act->data1 == 1 && act->y - 2 == playerY) {
        if (
            (act->x <= playerX && act->x + 3 >= playerX) ||
            (act->x <= playerX + 2 && act->x + 3 >= playerX + 2)
        ) {
            act->data1 = 2;
            act->data2 = 0;
            ClearPlayerDizzy();
        }

    } else if (act->data1 == 2) {
        if (act->westfree % 2 != 0) {
            act->data2++;
        }

        if (act->data2 == 5) {
            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 1);
        }

        if (act->data2 >= 5 && act->data2 < 8) {
            nextDrawMode = DRAW_MODE_HIDDEN;
            DrawSprite(SPR_SPLITTING_PLATFORM, 1, act->x - (act->data2 - 5), act->y, DRAW_MODE_NORMAL);
            DrawSprite(SPR_SPLITTING_PLATFORM, 2, act->x + act->data2 - 3, act->y, DRAW_MODE_NORMAL);
        }

        if (act->data2 == 7) {
            act->data1 = 3;
            act->data2 = 0;
        }
    }

    if (act->data1 == 3) {
        nextDrawMode = DRAW_MODE_HIDDEN;
        DrawSprite(SPR_SPLITTING_PLATFORM, 1, act->x + act->data2 - 2, act->y, DRAW_MODE_NORMAL);
        DrawSprite(SPR_SPLITTING_PLATFORM, 2, act->x + 4 - act->data2, act->y, DRAW_MODE_NORMAL);

        if (act->westfree % 2 != 0) {
            act->data2++;
        }

        if (act->data2 == 3) {
            nextDrawMode = DRAW_MODE_NORMAL;
            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 1);
            act->data1 = 0;
        }
    }
}

/*
Handle one frame of spark movment.

NOTE: This uses a direction system that is incompatible with DIR4_*.
*/
static void ActSpark(word index)
{
    Actor *act = actors + index;

    act->data5++;
    act->frame = !act->frame;

    if (act->data5 % 2 != 0) return;

    if (act->data1 == 0) {  /* W */
        act->x--;

        if (TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x - 1, act->y) != MOVE_FREE) {
            act->data1 = 2;  /* N */
        } else if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->data1 = 3;  /* S */
        }

    } else if (act->data1 == 1) {  /* E */
        act->x++;

        if (TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x + 1, act->y) != MOVE_FREE) {
            act->data1 = 3;  /* S */
        } else if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->data1 = 2;  /* N */
        }

    } else if (act->data1 == 2) {  /* N */
        act->y--;

        if (TestSpriteMove(DIR4_NORTH, act->sprite, 0, act->x, act->y - 1) != MOVE_FREE) {
            act->data1 = 1;  /* E */
        } else if (TestSpriteMove(DIR4_WEST, act->sprite, 0, act->x - 1, act->y) == MOVE_FREE) {
            act->data1 = 0;  /* W */
        }

    } else if (act->data1 == 3) {  /* S */
        act->y++;

        if (TestSpriteMove(DIR4_SOUTH, act->sprite, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->data1 = 0;  /* W */
        } else if (TestSpriteMove(DIR4_EAST, act->sprite, 0, act->x + 1, act->y) == MOVE_FREE) {
            act->data1 = 1;  /* E */
        }
    }
}

/*
Handle one frame of eye plant movement.
*/
static void ActEyePlant(word index)
{
    Actor *act = actors + index;

    nextDrawMode = act->data5;

    act->data2 = random(40);
    if (act->data2 > 37) {
        act->data2 = 3;
    } else {
        act->data2 = 0;
    }

    if (act->x - 2 > playerX) {
        act->frame = act->data2;
    } else if (act->x + 1 < playerX) {
        act->frame = act->data2 + 2;
    } else {
        act->frame = act->data2 + 1;
    }
}

/*
Handle one frame of red jumper movement.
*/
static void ActRedJumper(word index)
{
    static int jumptable[] = {
        /* even elements = y change; odd elements = actor frame */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, -2, 2,
        -2, 2, -2, 2, -2, 2, -1, 2, -1, 2, -1, 2, 0, 2, 0, 2, 1, 1, 1, 1, 1, 1
    };
    Actor *act = actors + index;

#ifdef HAS_ACT_RED_JUMPER
    int yjump;

    if (act->data2 < 5) {
        if (act->x > playerX) {
            act->data1 = 0;
        } else {
            act->data1 = 3;
        }

    } else if (act->data2 == 14 && IsSpriteVisible(SPR_RED_JUMPER, 0, act->x, act->y)) {
        StartSound(SND_RED_JUMPER_JUMP);

    } else if (act->data2 > 16 && act->data2 < 39) {
        if (
            act->data1 == 0 &&
            TestSpriteMove(DIR4_WEST, SPR_RED_JUMPER, 0, act->x - 1, act->y) == MOVE_FREE
        ) {
            act->x--;
        } else if (
            act->data1 == 3 &&
            TestSpriteMove(DIR4_EAST, SPR_RED_JUMPER, 0, act->x + 1, act->y) == MOVE_FREE
        ) {
            act->x++;
        }
    }

    if (act->data2 > 39) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y + 1) == MOVE_FREE &&
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, ++act->y + 1) == MOVE_FREE
        ) {
            act->y++;
            act->frame = act->data1 + jumptable[act->data2 + 1];

            if (act->data2 < 39) {  /* never occurs */
                act->data2 += 2;
            }
        } else {
            act->data2 = 0;

            if (IsSpriteVisible(SPR_RED_JUMPER, 0, act->x, act->y)) {
                StartSound(SND_RED_JUMPER_LAND);
            }
        }

        return;
    }

    yjump = jumptable[act->data2];

    if (yjump == -1) {
        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }
    }

    if (yjump == -2) {
        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }

        if (TestSpriteMove(DIR4_NORTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE) {
            act->y--;
        } else {
            act->data2 = 34;
        }
    }

    if (yjump == 1) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->y++;
        }
    }

    if (yjump == 2) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, act->y - 1) == MOVE_FREE &&
            TestSpriteMove(DIR4_SOUTH, SPR_RED_JUMPER, 0, act->x, ++act->y - 1) == MOVE_FREE
        ) {
            act->y++;
        } else {
            act->data2 = 0;

            return;
        }
    }

    act->frame = act->data1 + jumptable[act->data2 + 1];

    if (act->data2 < 39) act->data2 += 2;

#else
    (void) jumptable, (void) act;
#endif  /* HAS_ACT_RED_JUMPER */
}

/*
Handle one frame of boss movement.
*/
static void ActBoss(word index)
{
    static int yjump[] = {2, 2, 1, 0, -1, -2, -2, -2, -2, -1, 0, 1, 2, 2};
    Actor *act = actors + index;

#ifdef HAS_ACT_BOSS
#   ifdef HARDER_BOSS
#       define WIN_VAR winGame
#       define D5_VALUE 18
#   else
#       define WIN_VAR winLevel
#       define D5_VALUE 12
#   endif  /* HARDER_BOSS */

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (!sawBossBubble) {
        sawBossBubble = true;
        NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
        StopMusic();
        StartGameMusic(MUSIC_BOSS);
    }

    /* extra data var */
    if (act->eastfree > 0) {
        act->eastfree--;
        if (act->eastfree < 40) {
            act->y--;
        }

        act->weighted = false;
        act->falltime = 0;

        if (
            act->eastfree == 1 ||
#ifndef HARDER_BOSS
            act->y == 0 ||
#endif  /* HARDER_BOSS */
            (!IsSpriteVisible(SPR_BOSS, 0, act->x, act->y) && act->eastfree < 30)
        ) {
            WIN_VAR = true;
            AddScore(100000L);
        }

        if (act->eastfree < 40 && act->eastfree != 0 && act->eastfree % 3 == 0) {
            NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            StartSound(SND_BOSS_MOVE);
        }

        if (act->eastfree % 2 != 0) {
            DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAW_MODE_WHITE);
            DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAW_MODE_WHITE);

            if (act->eastfree > 39) {
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);
            }

        } else {
            DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAW_MODE_NORMAL);
            DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAW_MODE_NORMAL);
        }

        return;
    }

    if (act->data5 == D5_VALUE) {
        if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) == MOVE_FREE) {
            act->y++;
            if (act->y % 2 != 0) {
                DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAW_MODE_WHITE);
                DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAW_MODE_WHITE);
            } else {
                DrawSprite(SPR_BOSS, 0, act->x, act->y,     DRAW_MODE_NORMAL);
                DrawSprite(SPR_BOSS, 5, act->x, act->y - 4, DRAW_MODE_NORMAL);
            }
        }

        if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE) {
            act->eastfree = 80;
        }

        return;
    }

    /* extra data var */
    if (act->westfree != 0) {
        word frame = act->data5 > 3 ? 5 : 1;

        act->westfree--;
        if (act->westfree % 2 != 0) {
            DrawSprite(SPR_BOSS, 0,     act->x, act->y,     DRAW_MODE_WHITE);
            DrawSprite(SPR_BOSS, frame, act->x, act->y - 4, DRAW_MODE_WHITE);
        } else {
            DrawSprite(SPR_BOSS, 0,     act->x, act->y,     DRAW_MODE_NORMAL);
            DrawSprite(SPR_BOSS, frame, act->x, act->y - 4, DRAW_MODE_NORMAL);
        }
    }

    if (act->data1 == 0) {
        act->y -= 2;

        act->data2++;
        if (act->data2 == 6) {
            act->data1 = 1;
        }

    } else if (act->data1 == 1) {
        if (act->data2 != 0) {
            act->data2--;
        } else {
            act->data1 = 2;
        }

    } else if (act->data1 == 2) {
        if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + yjump[act->data3 % 14]) != MOVE_FREE
            && yjump[act->data3 % 14] == 2
        ) {
            act->y -= 2;
        }

        if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + yjump[act->data3 % 14]) != MOVE_FREE
            && yjump[act->data3 % 14] == 1
        ) {
            act->y--;
        } else {
            act->y += yjump[act->data3 % 14];
        }

        act->data3++;
        if (act->data3 % 14 == 1) {
            StartSound(SND_BOSS_MOVE);
        }

        act->data2++;
        if (act->data2 > 30 && act->data2 < 201) {
#ifdef HARDER_BOSS
            if (act->data2 > 100 && act->data2 < 104 && act->data2 % 2 != 0) {
                NewSpawner(ACT_PARACHUTE_BALL, act->x + 2, act->y - 5);
                StartSound(SND_BOSS_LAUNCH);
            }
#endif  /* HARDER_BOSS */

            if (act->data4 != 0) {
                if (TestSpriteMove(DIR4_EAST, SPR_BOSS, 0, act->x + 1, act->y) != MOVE_FREE) {
                    act->data4 = 0;
                    StartSound(SND_OBJECT_HIT);
                    NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y - 2, DIR8_SOUTH, 1);
                } else {
                    act->x++;
                }

            } else if (TestSpriteMove(DIR4_WEST, SPR_BOSS, 0, act->x - 1, act->y) == MOVE_FREE) {
                act->x--;

            } else {
                act->data4 = 1;
                StartSound(SND_OBJECT_HIT);
                NewDecoration(SPR_SMOKE, 6, act->x, act->y - 2, DIR8_SOUTH, 1);
            }

        } else if (act->data2 > 199) {
            act->data1 = 3;
            act->data2 = 0;
            act->data3 = 8;
        }

    } else if (act->data1 == 3) {
        act->data2++;

        if (act->data3 < 6) {
            act->data3++;
            act->y -= 2;

        } else if (act->data2 < 102) {
            act->weighted = true;

            if (TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data3 = 0;
                act->weighted = false;
                act->falltime = 0;
                StartSound(SND_SMASH);
                NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);

            } else if (act->x + 1 > playerX) {
                if (TestSpriteMove(DIR4_WEST, SPR_BOSS, 0, act->x - 1, act->y) == MOVE_FREE) {
                    act->x--;
                }

            } else if (
                act->x + 3 < playerX &&
                TestSpriteMove(DIR4_EAST, SPR_BOSS, 0, act->x + 1, act->y) == MOVE_FREE
            ) {
                act->x++;
            }

        } else if (
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y + 1) != MOVE_FREE ||
            TestSpriteMove(DIR4_SOUTH, SPR_BOSS, 0, act->x, act->y) != MOVE_FREE
        ) {
            act->data1 = 4;
            act->data2 = 0;
            act->data3 = 0;
            act->weighted = false;
            act->falltime = 0;
            StartSound(SND_OBJECT_HIT);
            NewDecoration(SPR_SMOKE, 6, act->x,     act->y, DIR8_NORTHWEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y, DIR8_NORTHEAST, 1);

        } else {
            act->y++;
        }

    } else if (act->data1 == 4) {
        act->weighted = false;
        act->falltime = 0;
        act->y--;

        act->data2++;
        if (act->data2 == 6) {
            act->data1 = 2;
            act->data3 = 0;
            act->data2 = 0;
        }
    }

    if (act->westfree == 0) {
        DrawSprite(SPR_BOSS, 0, act->x, act->y, 0);

        if (act->data5 < 4) {
            DrawSprite(SPR_BOSS, 1, act->x,     act->y - 4, DRAW_MODE_NORMAL);
        } else if (act->x + 1 > playerX) {
            DrawSprite(SPR_BOSS, 2, act->x + 1, act->y - 4, DRAW_MODE_NORMAL);
        } else if (act->x + 2 < playerX) {
            DrawSprite(SPR_BOSS, 4, act->x + 1, act->y - 4, DRAW_MODE_NORMAL);
        } else {
            DrawSprite(SPR_BOSS, 3, act->x + 1, act->y - 4, DRAW_MODE_NORMAL);
        }
    }

#   undef WIN_VAR
#   undef D5_VALUE
#else
    (void) yjump, (void) act;
#endif  /* HAS_ACT_BOSS */
}

/*
Handle one frame of pipe end movement.
*/
static void ActPipeEnd(word index)
{
    Actor *act = actors + index;

    if (act->data2 == 0) return;

    act->data1++;

    act->data3++;
    if (act->data3 % 2 != 0) {
        act->frame = 4;
    } else {
        act->frame = 0;
    }

    if (act->data1 == 4) act->data1 = 1;

    DrawSprite(SPR_PIPE_END, act->data1, act->x, act->y + 3, DRAW_MODE_NORMAL);
}

/*
Return true if there is a close-enough surface above/below this suction walker.
*/
static bbool CanSuctionWalkerFlip(word index, word dir)
{
    Actor *act = actors + index;
    word y;

    if (GameRand() % 2 == 0) return false;

    if (dir == DIR4_NORTH) {
        for (y = 0; y < 15; y++) {
            if (
                /* BUG: This should probably be testing north, not west. */
                TILE_BLOCK_WEST(GetMapTile(act->x,     (act->y - y) - 4)) &&
                TILE_BLOCK_WEST(GetMapTile(act->x + 2, (act->y - y) - 4))
            ) {
                return true;
            }
        }
    } else if (dir == DIR4_SOUTH) {
        for (y = 0; y < 15; y++) {
            if (
                TILE_BLOCK_SOUTH(GetMapTile(act->x,     act->y + y)) &&
                TILE_BLOCK_SOUTH(GetMapTile(act->x + 2, act->y + y))
            ) {
                return true;
            }
        }
    }

    return false;
}

/*
Handle one frame of suction walker movement.

NOTE: The _BLOCK_WEST checks should almost certainly be _BLOCK_NORTH instead.
*/
static void ActSuctionWalker(word index)
{
    Actor *act = actors + index;
    word move, ledge;

    act->data4 = !act->data4;

    if (act->data1 == DIR2_WEST) {
        if (act->data2 == 0) {  /* facing west, floor */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3;
            }

            move = TestSpriteMove(DIR4_WEST, SPR_SUCTION_WALKER, 0, act->x - 1, act->y);
            ledge = !TILE_BLOCK_SOUTH(GetMapTile(act->x - 1, act->y + 1));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_NORTH)) {
                    act->data2 = 2;
                    act->frame = 9;
                } else {
                    act->data1 = DIR2_EAST;
                    act->data2 = 0;
                }
            } else if (act->data4 != 0) {
                act->x--;
            }

        } else if (act->data2 == 1) {  /* facing west, ceiling */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 4;
            }

            move = TestSpriteMove(DIR4_WEST, SPR_SUCTION_WALKER, 0, act->x - 1, act->y);
            ledge = !TILE_BLOCK_WEST(GetMapTile(act->x - 1, act->y - 4));
            if (move == MOVE_SLOPED && act->data4 != 0) {
                act->y--;
                act->x--;
            } else if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_SOUTH)) {
                    act->data2 = 3;
                    act->frame = 9;
                } else {
                    act->data1 = DIR2_EAST;
                    act->data2 = 1;
                }
            } else if (act->data4 != 0) {
                act->x--;
            }

        } else if (act->data2 == 2) {  /* facing west, rising */
            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

        } else if (act->data2 == 3) {  /* facing west, falling */
            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }

            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }
        }

    } else if (act->data1 == DIR2_EAST) {
        if (act->data2 == 0) {  /* facing east, floor */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 2;
            }

            move = TestSpriteMove(DIR4_EAST, SPR_SUCTION_WALKER, 0, act->x + 1, act->y);
            ledge = !TILE_BLOCK_SOUTH(GetMapTile(act->x + 3, act->y + 1));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_NORTH)) {
                    act->data2 = 2;
                    act->frame = 8;
                } else {
                    act->data1 = DIR2_WEST;
                    act->data2 = 0;
                }
            } else if (act->data4 != 0) {
                act->x++;
            }

        } else if (act->data2 == 1) {  /* facing east, ceiling */
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 6;
            }

            move = TestSpriteMove(DIR4_EAST, SPR_SUCTION_WALKER, 0, act->x + 1, act->y);
            ledge = !TILE_BLOCK_WEST(GetMapTile(act->x + 3, act->y - 4));
            if (move != MOVE_FREE || ledge != 0 || GameRand() % 50 == 0) {
                if (CanSuctionWalkerFlip(index, DIR4_SOUTH)) {
                    act->data2 = 3;
                    act->frame = 8;
                } else {
                    act->data1 = DIR2_WEST;
                    act->data2 = 1;
                }
            } else if (act->data4 != 0) {
                act->x++;
            }

        } else if (act->data2 == 2) {  /* facing east, rising */
            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

            if (TestSpriteMove(DIR4_NORTH, SPR_SUCTION_WALKER, 0, act->x, act->y - 1) != MOVE_FREE) {
                act->data2 = 1;
            } else {
                act->y--;
            }

        } else if (act->data2 == 3) {  /* facing east, falling */
            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }

            if (TestSpriteMove(DIR4_SOUTH, SPR_SUCTION_WALKER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->data2 = 0;
            } else {
                act->y++;
            }
        }
    }
}

/*
Handle one frame of transporter movement.
*/
static void ActTransporter(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_HIDDEN;

    /* SPR_TRANSPORTER_COPY draws as SPR_TRANSPORTER; inconsistent for parity */
    if (transporterTimeLeft != 0 && random(2U) != 0) {
        DrawSprite(SPR_TRANSPORTER_COPY, 0, act->x, act->y, DRAW_MODE_WHITE);
    } else {
        DrawSprite(SPR_TRANSPORTER_COPY, 0, act->x, act->y, DRAW_MODE_NORMAL);
    }

    if (GameRand() % 2 != 0) {
        DrawSprite(SPR_TRANSPORTER_COPY, random(2U) + 1, act->x, act->y, DRAW_MODE_NORMAL);
    }

    if (transporterTimeLeft == 15) {
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX - 1, playerY,     DIR8_NONE, 1);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX + 1, playerY,     DIR8_NONE, 1);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX - 1, playerY - 3, DIR8_NONE, 2);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX,     playerY - 2, DIR8_NONE, 3);
        NewDecoration(SPR_SPARKLE_SHORT, 4, playerX + 1, playerY - 3, DIR8_NONE, 3);

        StartSound(SND_TRANSPORTER_ON);
    }

    if (transporterTimeLeft > 1) {
        transporterTimeLeft--;
    } else if (activeTransporter == 3) {
        winLevel = true;
    } else if (activeTransporter != 0 && act->data5 != activeTransporter && act->data5 != 3) {
        playerX = act->x + 1;
        playerY = act->y;

        if ((int) (playerX - 14) < 0) {
            scrollX = 0;
        } else if (playerX - 14 > mapWidth - SCROLLW) {
            scrollX = mapWidth - SCROLLW;
        } else {
            scrollX = playerX - 14;
        }

        if ((int) (playerY - 12) < 0) {
            scrollY = 0;
        } else if (playerY - 12 > maxScrollY) {
            scrollY = maxScrollY;
        } else {
            scrollY = playerY - 12;
        }

        activeTransporter = 0;
        transporterTimeLeft = 0;
        isPlayerRecoiling = false;

        if (!sawTransporterBubble) {
            sawTransporterBubble = true;
            NewActor(ACT_SPEECH_WHOA, playerX - 1, playerY - 5);
        }
    }
}

/*
Handle one frame of spitting wall plant movement.
*/
static void ActSpittingWallPlant(word index)
{
    Actor *act = actors + index;

    act->data4++;

    if (act->data4 == 50) {
        act->data4 = 0;
        act->frame = 0;
    }

    if (act->data4 == 42) {
        act->frame = 1;
    }

    if (act->data4 == 45) {
        act->frame = 2;

        if (act->data5 == DIR4_WEST) {
            NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
        } else {
            NewActor(ACT_PROJECTILE_E, act->x + 4, act->y - 1);
        }
    }
}

/*
Handle one frame of spitting turret movement.
*/
static void ActSpittingTurret(word index)
{
    Actor *act = actors + index;

    act->data2--;
    if (act->data2 == 0) {
        act->data1++;
        act->data2 = 3;

        if (act->data1 != 3) {
            switch (++act->frame) {
            case 2:
                NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
                break;

            case 5:
                NewActor(ACT_PROJECTILE_SW, act->x - 1, act->y + 1);
                break;

            case 8:
                NewActor(ACT_PROJECTILE_S, act->x + 1, act->y + 1);
                break;

            case 11:
                NewActor(ACT_PROJECTILE_SE, act->x + 5, act->y + 1);
                break;

            case 14:
                NewActor(ACT_PROJECTILE_E, act->x + 5, act->y - 1);
                break;
            }
        }
    }

    if (act->data1 == 0) {
        /* below: a bunch of bare `else` would've sufficed */
        if (act->y >= playerY - 2) {
            if (act->x + 1 > playerX) {
                act->frame = 0;  /* west */
                act->x = act->data3;
            } else if (act->x + 2 <= playerX) {
                act->frame = 12;  /* east */
                act->x = act->data3 + 1;
            }
        } else if (act->y < playerY - 2) {
            if (act->x - 2 > playerX) {
                act->frame = 3;  /* southwest */
                act->x = act->data3;
            } else if (act->x + 3 < playerX) {
                act->frame = 9;  /* southeast */
                act->x = act->data3 + 1;
            } else if (act->x - 2 < playerX && act->x + 3 >= playerX) {
                act->frame = 6;  /* south */
                act->x = act->data3 + 1;
            }

            /* less-or-equal in the previous `else if` would've avoided this */
            if (act->x - 2 == playerX) {
                act->frame = 6;  /* south */
                act->x = act->data3 + 1;
            }
        }
    }

    if (act->data1 == 3) {
        act->data2 = 27;
        act->data1 = 0;
    }

    if (act->frame > 14) {
        act->frame = 14;
    }
}

/*
Handle one frame of scooter movement.
*/
static void ActScooter(word index)
{
    Actor *act = actors + index;

    act->frame++;
    act->frame &= 3;

    if (scooterMounted != 0) {
        act->x = playerX;
        act->y = playerY + 1;

    } else {
        act->data2++;
        if (act->data2 % 10 == 0) {
            if (TestSpriteMove(DIR4_SOUTH, SPR_SCOOTER, 0, act->x, act->y + 1) != MOVE_FREE) {
                act->y--;
            } else {
                act->y++;

                if (TestSpriteMove(DIR4_SOUTH, SPR_SCOOTER, 0, act->x, act->y + 1) != MOVE_FREE) {
                    act->y--;
                }
            }
        }
    }
}

/*
Handle one frame of red chomper movement.
*/
static void ActRedChomper(word index)
{
    Actor *act = actors + index;

    act->data4 = !act->data4;

    if (GameRand() % 95 == 0) {
        act->data5 = 10;
    } else if (GameRand() % 100 == 0) {
        act->data5 = 11;
    }

    if (act->data5 < 11 && act->data5 != 0) {
        act->data5--;

        if (act->data5 > 8) {
            act->frame = 6;
        } else if (act->data5 == 8) {
            act->frame = 5;
        } else {
            act->data2 = !act->data2;
            act->frame = act->data2 + 6;
        }

        if (act->data5 == 0 && GameRand() % 2 != 0) {
            if (act->x >= playerX) {
                act->data1 = DIR2_WEST;
            } else {
                act->data1 = DIR2_EAST;
            }
        }

    } else if (act->data5 > 10) {
        if (act->data1 == DIR2_WEST) {
            static word searchframes[] = {8, 9, 10, 10, 9, 8};
            act->frame = searchframes[act->data5 - 11];
        } else {
            static word searchframes[] = {10, 9, 8, 8, 9, 10};
            act->frame = searchframes[act->data5 - 11];
        }

        act->data5++;
        if (act->data5 == 17) {
            act->data5 = 0;
        }

    } else {
        if (act->data1 == DIR2_WEST) {
            if (act->data4 != 0) {
                act->frame = !act->frame;
                act->x--;
                AdjustActorMove(index, DIR4_WEST);
                if (act->westfree == 0) {
                    act->data1 = DIR2_EAST;
                    act->frame = 4;
                }
            }
        } else {
            if (act->data4 != 0) {
                act->data3 = !act->data3;
                act->frame = act->data3 + 2;
                act->x++;
                AdjustActorMove(index, DIR4_EAST);
                if (act->eastfree == 0) {
                    act->data1 = DIR2_WEST;
                    act->frame = 4;
                }
            }
        }
    }
}

/*
Handle one frame of force field movement.
*/
static void ActForceField(word index)
{
    Actor *act = actors + index;

    act->data1 = 0;

    act->data4++;
    if (act->data4 == 3) {
        act->data4 = 0;
    }

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (!areForceFieldsActive) {
        act->dead = true;
        return;
    }

    if (act->data5 == 0) {
        for (;; act->data1++) {
            if (IsTouchingPlayer(act->sprite, 0, act->x, act->y - act->data1)) {
                HurtPlayer(index);
                break;
            }

            if (TILE_BLOCK_NORTH(GetMapTile(act->x, act->y - act->data1))) break;

            DrawSprite(act->sprite, act->data4, act->x, act->y - act->data1, DRAW_MODE_NORMAL);
        }

    } else {
        for (;; act->data1++) {
            if (IsTouchingPlayer(act->sprite, 0, act->x + act->data1, act->y)) {
                HurtPlayer(index);
                break;
            }

            if (TILE_BLOCK_EAST(GetMapTile(act->x + act->data1, act->y))) break;

            DrawSprite(act->sprite, act->data4, act->x + act->data1, act->y, DRAW_MODE_NORMAL);
        }
    }
}

/*
Handle one frame of pink worm movement.
*/
static void ActPinkWorm(word index)
{
    Actor *act = actors + index;

    if (act->data5 == 0) {  /* always true? */
        act->data4 = !act->data4;
        if (act->data4 != 0) return;
    }

    if (random(40) > 37 && act->data3 == 0 && act->data2 == 0) {
        act->data3 = 4;
    }

    if (act->data3 != 0) {
        act->data3--;

        if (act->data3 == 2) {
            if (act->data1 == DIR2_WEST) {
                act->frame = 2;
            } else if (act->data2 == 0) {
                act->frame = 5;
            }
        } else if (act->data1 == DIR2_WEST) {
            act->frame = 0;
        } else {
            act->frame = 3;
        }

    } else if (act->data1 == DIR2_WEST) {
        act->frame = !act->frame;
        if (act->frame != 0) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->westfree == 0) {
                act->data1 = DIR2_EAST;
            }
        }

    } else {
        act->data2 = !act->data2;
        if (act->data2 == 0) {
            act->x++;
            act->frame = 1;
            AdjustActorMove(index, DIR4_EAST);
            if (act->eastfree == 0) {
                act->data1 = DIR2_WEST;
            }
        }
        act->frame = act->data2 + 3;
    }
}

/*
Handle one frame of hint globe movement.
*/
static void ActHintGlobe(word index)
{
    static byte orbframes[] = {0, 4, 5, 6, 5, 4};
    Actor *act = actors + index;

    act->data4 = !act->data4;
    if (act->data4 != 0) {
        act->data3++;
    }

    DrawSprite(SPR_HINT_GLOBE, orbframes[act->data3 % 6], act->x, act->y - 2, DRAW_MODE_NORMAL);

    act->data2++;
    if (act->data2 == 4) {
        act->data2 = 1;
    }

    DrawSprite(SPR_HINT_GLOBE, act->data2, act->x, act->y, DRAW_MODE_NORMAL);

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (IsTouchingPlayer(SPR_HINT_GLOBE, 0, act->x, act->y - 2)) {
        isPlayerNearHintGlobe = true;

        if (demoState != DEMO_STATE_NONE) {
            sawAutoHintGlobe = true;
        }

        if ((cmdNorth && scooterMounted == 0) || !sawAutoHintGlobe) {
            StartSound(SND_HINT_DIALOG_ALERT);
            ShowHintGlobeMessage(act->data5);
        }

        sawAutoHintGlobe = true;
    }
}

/*
Handle one frame of pusher robot movement.
*/
static void ActPusherRobot(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_TRANSLUCENT;
    if (act->data5 == 1) {
        nextDrawMode = DRAW_MODE_NORMAL;
    }

    if (act->data2 != 0) {
        act->data2--;
        nextDrawMode = DRAW_MODE_NORMAL;
        return;
    }

    if (act->data4 != 0) {
        act->data4--;
    }

    act->data3 = !act->data3;

    if (act->data1 == DIR2_WEST) {
        if (act->y == playerY && act->x - 3 == playerX && act->data4 == 0) {
            act->frame = 2;
            act->data2 = 8;
            SetPlayerPush(DIR8_WEST, 5, 2, PLAYER_BASE_EAST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
            playerBaseFrame = PLAYER_BASE_EAST;

            act->data4 = 3;
            nextDrawMode = DRAW_MODE_NORMAL;
            if (!sawPusherRobotBubble) {
                sawPusherRobotBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }

        } else if (act->data3 != 0) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->westfree == 0) {
                act->data1 = DIR2_EAST;
                act->frame = (act->x % 2) + 3;
            } else {
                act->frame = !act->frame;
            }
        }

    } else {
        if (act->y == playerY && act->x + 4 == playerX && act->data4 == 0) {
            act->frame = 5;
            act->data2 = 8;
            SetPlayerPush(DIR8_EAST, 5, 2, PLAYER_BASE_WEST + PLAYER_PUSHED, false, true);
            StartSound(SND_PUSH_PLAYER);
            playerBaseFrame = PLAYER_BASE_WEST;

            act->data4 = 3;
            nextDrawMode = DRAW_MODE_NORMAL;
            if (!sawPusherRobotBubble) {
                sawPusherRobotBubble = true;
                NewActor(ACT_SPEECH_UMPH, playerX - 1, playerY - 5);
            }

        } else if (act->data3 != 0) {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->eastfree == 0) {
                act->frame = !act->frame;
                act->data1 = DIR2_WEST;
            } else {
                act->frame = (act->x % 2) + 3;
            }
        }
    }
}

/*
Handle one frame of sentry robot movement.
*/
static void ActSentryRobot(word index)
{
    Actor *act = actors + index;

    if (act->hurtcooldown != 0) return;

    act->data3 = !act->data3;
    if (act->data3 != 0) return;

    if (areLightsActive && GameRand() % 50 > 48 && act->data4 == 0) {
        act->data4 = 10;
    }

    if (act->data4 != 0) {
        act->data2 = !act->data2;
        act->data4--;

        if (act->data4 == 1) {
            if (act->x + 1 > playerX) {
                act->data1 = DIR2_WEST;
            } else {
                act->data1 = DIR2_EAST;
            }

            if (act->data1 != DIR2_WEST) {
                NewActor(ACT_PROJECTILE_E, act->x + 3, act->y - 1);
            } else {
                NewActor(ACT_PROJECTILE_W, act->x - 1, act->y - 1);
            }
        }

        if (act->data1 != DIR2_WEST) {
            if (act->data2 != 0) {
                act->frame = 5;
            } else {
                act->frame = 0;
            }
        } else {
            if (act->data2 != 0) {
                act->frame = 6;
            } else {
                act->frame = 2;
            }
        }
    } else {
        if (act->data1 == DIR2_WEST) {
            act->x--;
            AdjustActorMove(index, DIR4_WEST);
            if (act->westfree == 0) {
                act->data1 = DIR2_EAST;
                act->frame = 4;
            } else {
                act->data2 = !act->data2;
                act->frame = act->data2 + 2;
            }
        } else {
            act->x++;
            AdjustActorMove(index, DIR4_EAST);
            if (act->eastfree == 0) {
                act->data1 = DIR2_WEST;
                act->frame = 4;
            } else {
                act->frame = !act->frame;
            }
        }
    }
}

/*
Handle one frame of pink worm slime movement.
*/
static void ActPinkWormSlime(word index)
{
    Actor *act = actors + index;

    if (act->data5 != 0) {
        act->data5--;
    } else {
        if (act->frame == 8) {
            act->frame = 1;
        }

        act->frame++;
    }
}

/*
Handle one frame of dragonfly movement.
*/
static void ActDragonfly(word index)
{
    Actor *act = actors + index;

    if (act->data1 != DIR2_WEST) {
        if (TestSpriteMove(DIR4_EAST, SPR_DRAGONFLY, 0, act->x + 1, act->y) != MOVE_FREE) {
            act->data1 = DIR2_WEST;
        } else {
            act->x++;
            act->data2 = !act->data2;
            act->frame = act->data2 + 2;
        }
    } else {
        if (TestSpriteMove(DIR4_WEST, SPR_DRAGONFLY, 0, act->x - 1, act->y) != MOVE_FREE) {
            act->data1 = DIR2_EAST;
        } else {
            act->x--;
            act->frame = !act->frame;
        }
    }
}

/*
Handle one frame of worm crate movmenet.
*/
static void ActWormCrate(word index)
{
    Actor *act = actors + index;

    if (act->data4 == 0) {
        SetMapTileRepeat(TILE_STRIPED_PLATFORM, 4, act->x, act->y - 2);
        act->data4 = 1;

    } else if (TestSpriteMove(DIR4_SOUTH, SPR_WORM_CRATE, 0, act->x, act->y + 1) == MOVE_FREE) {
        SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 2);
        act->y++;
        if (TestSpriteMove(DIR4_SOUTH, SPR_WORM_CRATE, 0, act->x, act->y + 1) != MOVE_FREE) {
            SetMapTileRepeat(TILE_STRIPED_PLATFORM, 4, act->x, act->y - 2);
        }

    } else if (IsSpriteVisible(SPR_WORM_CRATE, 0, act->x, act->y)) {
        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            act->data5 = 1;
            /* extra data var */
            act->eastfree = WORM_CRATE_EXPLODE;
        }

        if (act->data5 != 0) {
            act->data5--;
        } else {
            act->dead = true;
            if (act->eastfree == WORM_CRATE_EXPLODE) {
                NewExplosion(act->x - 1, act->y - 1);
            }

            SetMapTileRepeat(TILE_EMPTY, 4, act->x, act->y - 2);
            NewActor(ACT_PINK_WORM, act->x, act->y);
            nextDrawMode = DRAW_MODE_WHITE;
            NewShard(SPR_WORM_CRATE_SHARDS, 0, act->x - 1, act->y + 3);
            NewShard(SPR_WORM_CRATE_SHARDS, 1, act->x,     act->y - 1);
            NewShard(SPR_WORM_CRATE_SHARDS, 2, act->x + 1, act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 3, act->x,     act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 4, act->x + 3, act->y + 2);
            NewShard(SPR_WORM_CRATE_SHARDS, 5, act->x,     act->y);
            NewShard(SPR_WORM_CRATE_SHARDS, 6, act->x + 5, act->y + 5);
            StartSound(SND_DESTROY_SOLID);
        }
    }
}

/*
Handle one frame of satellite movement.
*/
static void ActSatellite(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->data2--;

        if (act->data2 != 0) {
            if (act->data2 % 2 != 0) {
                nextDrawMode = DRAW_MODE_WHITE;
            }

            return;
        }
    }

    if (IsNearExplosion(SPR_SATELLITE, 0, act->x, act->y)) {
        if (act->data1 == 0) {
            act->data1 = 1;
            act->data2 = 15;
        } else {
            act->dead = true;
            nextDrawMode = DRAW_MODE_WHITE;
            StartSound(SND_DESTROY_SATELLITE);

            for (act->data1 = 1; act->data1 < 9; act->data1++) {
                NewDecoration(SPR_SMOKE, 6, act->x + 3, act->y - 3, act->data1, 3);
            }

            NewPounceDecoration(act->x, act->y + 5);
            NewShard(SPR_SATELLITE_SHARDS, 0, act->x,     act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 1, act->x + 1, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 2, act->x + 7, act->y + 2);
            NewShard(SPR_SATELLITE_SHARDS, 3, act->x + 3, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 4, act->x - 1, act->y - 8);
            NewShard(SPR_SATELLITE_SHARDS, 5, act->x + 2, act->y + 3);
            NewShard(SPR_SATELLITE_SHARDS, 6, act->x + 6, act->y - 2);
            NewShard(SPR_SATELLITE_SHARDS, 7, act->x - 4, act->y + 1);
            NewSpawner(ACT_HAMBURGER, act->x + 4, act->y);
        }
    }
}

/*
Handle one frame of ivy plant movement.
*/
static void ActIvyPlant(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        act->y++;
        act->data4++;

        if (act->data4 == 7) {
            act->data2 = 0;
            act->data3 = 0;
            act->data1 = 12;
        }

    } else if (act->data3 < act->data1) {
        act->data3++;

    } else {
        act->data5 = !act->data5;

        act->frame++;
        if (act->frame == 4) act->frame = 0;

        if (act->data4 != 0) {
            if (act->data4 == 7) {
                StartSound(SND_IVY_PLANT_RISE);
            }

            act->data4--;
            act->y--;
        }

        if (IsNearExplosion(SPR_IVY_PLANT, 0, act->x, act->y)) {
            act->data2 = 1;
        }
    }
}

/*
Handle one frame of exit monster (facing west) movement.
*/
static void ActExitMonsterWest(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0) {
        act->data2++;
    }

    if (act->data2 == 10) {
        act->data1 = 1;
        act->data2 = 11;
        act->frame = 1;
        act->data5 = 1;
        StartSound(SND_EXIT_MONSTER_OPEN);
    }

    if (act->frame != 0) {
        static byte tongueframes[] = {2, 3, 4, 3};
        DrawSprite(
            SPR_EXIT_MONSTER_W, tongueframes[act->data3 % 4],
            act->x + 6 - act->data5, act->y - 3, DRAW_MODE_NORMAL
        );
        act->data3++;
    }

    if (!IsSpriteVisible(SPR_EXIT_MONSTER_W, 1, act->x, act->y)) {
        act->frame = 0;
        act->data2 = 0;
        act->data1 = 0;
        act->data5 = 0;
    }

    nextDrawMode = DRAW_MODE_HIDDEN;

    DrawSprite(act->sprite, 1, act->x, act->y, DRAW_MODE_NORMAL);

    if (act->data5 != 0 && act->data5 < 4) {
        act->data5++;
    }

    DrawSprite(act->sprite, 0, act->x, (act->y - 1) - act->data5, DRAW_MODE_NORMAL);
}

/*
Handle interaction between the player and a vertical exit line.
*/
static void ActExitLineVertical(word index)
{
    Actor *act = actors + index;

    if (act->x <= playerX + 3) {
        winLevel = true;
    }

    nextDrawMode = DRAW_MODE_HIDDEN;
}

/*
Handle interaction between the player and a horizontal exit line.
*/
static void ActExitLineHorizontal(word index)
{
    Actor *act = actors + index;

    if (act->y <= playerY && act->data1 == 0) {
        winLevel = true;
    } else if (act->y >= playerY && act->data1 != 0) {  /* player could be dead! */
        winGame = true;
    }

    nextDrawMode = DRAW_MODE_HIDDEN;
}

/*
Handle one frame of small flame movement.
*/
static void ActSmallFlame(word index)
{
    Actor *act = actors + index;

    act->frame++;
    if (act->frame == 6) act->frame = 0;
}

/*
Handle one frame of prize movement.
*/
static void ActPrize(word index)
{
    Actor *act = actors + index;

    if (act->data1 != 0) {
        nextDrawMode = DRAW_MODE_FLIPPED;
    }

    if (act->data4 == 0) {
        act->frame++;
    } else {
        act->data3 = !act->data3;

        if (act->data3 != 0) {
            act->frame++;
        }
    }

    if (act->frame == act->data5) act->frame = 0;

    if (act->data5 == 1 && act->sprite != SPR_THRUSTER_JET && act->data4 == 0 && random(64U) == 0) {
        NewDecoration(
            SPR_SPARKLE_LONG, 8,
            random(act->data1) + act->x, random(act->data2) + act->y, DIR8_NONE, 1
        );
    }
}

/*
Handle one frame of bear trap movement.
*/
static void ActBearTrap(word index)
{
    Actor *act = actors + index;

    if (act->data2 != 0) {
        static byte frames[] = {
            0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 0
        };

        if (act->data3 == 1) {
            StartSound(SND_BEAR_TRAP_CLOSE);
        }

        act->frame = frames[act->data3];

        act->data3++;
        if (act->data3 >= 24) {
            blockMovementCmds = false;
        }

        if (act->data3 == 27) {
            act->data3 = 0;
            act->data2 = 0;
            blockMovementCmds = false;
        }

        if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
            AddScore(250);
            NewShard(act->sprite, act->frame, act->x, act->y);
            act->dead = true;
            blockMovementCmds = false;
        }

    } else if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        AddScore(250);
        NewShard(act->sprite, act->frame, act->x, act->y);
        act->dead = true;
    }
}

/*
Handle one frame of falling floor movement.
*/
static void ActFallingFloor(word index)
{
    Actor *act = actors + index;

    if (TestSpriteMove(DIR4_SOUTH, SPR_FALLING_FLOOR, 0, act->x, act->y + 1) != MOVE_FREE) {
        act->dead = true;
        NewShard(SPR_FALLING_FLOOR, 1, act->x, act->y);
        NewShard(SPR_FALLING_FLOOR, 2, act->x, act->y);
        StartSound(SND_DESTROY_SOLID);
        nextDrawMode = DRAW_MODE_WHITE;

    } else {
        if (act->data1 == 0) {
            /* extra data vars */
            act->westfree = GetMapTile(act->x,     act->y - 1);
            act->eastfree = GetMapTile(act->x + 1, act->y - 1);

            SetMapTile(TILE_STRIPED_PLATFORM, act->x,     act->y - 1);
            SetMapTile(TILE_STRIPED_PLATFORM, act->x + 1, act->y - 1);
            act->data1 = 1;
        }

        if (act->y - 2 == playerY && act->x <= playerX + 2 && act->x + 1 >= playerX) {
            act->data2 = 7;
        }

        if (act->data2 != 0) {
            act->data2--;

            if (act->data2 == 0) {
                act->weighted = true;

                SetMapTile(act->westfree, act->x,     act->y - 1);
                SetMapTile(act->eastfree, act->x + 1, act->y - 1);
            }
        }
    }
}

/*
Handle interaction between the player and the Episode 1 end sequence lines.
*/
static void ActEpisode1End(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (act->data2 == 0 && act->y <= playerY && act->y >= playerY - 4) {
        ShowE1CliffhangerMessage(act->data1);
        act->data2 = 1;
    }
}

/*
Handle one frame of score effect movement.
*/
static void ActScoreEffect(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_HIDDEN;

    act->data1++;
    act->frame = !act->frame;

    if (act->data1 > 31) {
        static signed char xmoves[] = {-2, -1, 0, 1, 2, 2, 1, 0, -1, -2};
        act->y--;
        act->x += xmoves[(act->data1 - 32) % 10];
    }

    if (act->data1 < 4) {
        act->y--;
    }

    if (
        act->data1 == 100 ||
        !IsSpriteVisible(act->sprite, act->frame, act->x, act->y)
    ) {
        act->dead = true;
        nextDrawMode = DRAW_MODE_HIDDEN;
    }

    DrawSprite(act->sprite, act->frame, act->x, act->y, DRAW_MODE_IN_FRONT);
}

/*
Handle one frame of exit plant movement.
*/
static void ActExitPlant(word index)
{
    Actor *act = actors + index;
    byte tongueframes[] = {5, 6, 7, 8};
    byte swallowframes[] = {1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 1, 1, 1, 1, 1, 1};

    if (act->data3 != 0) {
        act->data3--;
        act->frame = 1;
        if (act->data3 != 0) return;
        act->frame = 0;
    }

    if (act->frame == 0 && act->data5 == 0) {
        DrawSprite(SPR_EXIT_PLANT, tongueframes[act->data1 % 4], act->x + 2, act->y - 3, DRAW_MODE_NORMAL);
        act->data1++;
    }

    if (act->data5 != 0) {
        act->frame = swallowframes[act->data5 - 1];

        if (act->data5 == 16) {
            winLevel = true;
        } else {
            act->data5++;
        }
    }

    if (!IsSpriteVisible(SPR_EXIT_PLANT, 1, act->x, act->y)) {
        act->data3 = 30;
        act->data5 = 0;
        act->frame = 1;
    }
}

/*
Handle one frame of bird movement.
*/
static void ActBird(word index)
{
    Actor *act = actors + index;

    if (act->data1 == 0) {
        if (act->x + 1 > playerX) {
            if (random(10) == 0) {
                act->data2 = 1;
            } else {
                act->data2 = 0;
            }
        } else {
            if (random(10) == 0) {
                act->data2 = 5;
            } else {
                act->data2 = 4;
            }
        }

        act->frame = act->data2;

        act->data3++;
        if (act->data3 == 30) {
            act->data1 = 1;
            act->data3 = 0;
        }

    } else if (act->data1 == 1) {
        act->data3++;

        if (act->data3 == 20) {
            act->data3 = 0;
            act->data1 = 2;

            if (act->x + 1 > playerX) {
                act->data4 = DIR2_WEST;
            } else {
                act->data4 = DIR2_EAST;
            }
        } else if (act->data3 % 2 != 0 && act->data3 < 10) {
            act->y--;
        }

        if (act->x + 1 > playerX) {
            act->frame = (act->data3 % 2) + 2;
        } else {
            act->frame = (act->data3 % 2) + 6;
        }

    } else if (act->data1 == 2) {
        static signed char yjump[] = {2, 2, 2, 1, 1, 1, 0, 0, 0, -1, -1, -1, -2, -2, -2};

        act->data3++;

        if (act->data4 == DIR2_WEST) {
            act->frame = (act->data3 % 2) + 2;
            act->x--;
        } else {
            act->frame = (act->data3 % 2) + 6;
            act->x++;
        }

        act->y += yjump[act->data3 - 1];

        if (act->data3 == 15) {
            act->data1 = 1;
            act->data3 = 10;
        }
    }
}

/*
Handle one frame of rocket movement.
*/
static void ActRocket(word index)
{
    Actor *act = actors + index;

    if (act->data1 != 0) {
        act->data1--;

        if (act->data1 < 30) {
            if (act->data1 % 2 != 0) {
                NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_NORTHWEST, 1);
            } else {
                NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y + 1, DIR8_NORTHEAST, 1);
            }
        }

        return;
    }

    if (act->data2 != 0) {
        if (act->data2 > 7) {
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_WEST, 1);
            NewDecoration(SPR_SMOKE, 6, act->x + 1, act->y + 1, DIR8_EAST, 1);
            StartSound(SND_ROCKET_BURN);
        }

        if (act->data2 > 1) {
            act->data2--;
        }

        if (act->data2 < 10) {
            if (TestSpriteMove(DIR4_NORTH, SPR_ROCKET, 0, act->x, act->y - 1) == MOVE_FREE) {
                act->y--;
            } else {
                act->data5 = 1;
            }

            if (IsSpriteVisible(act->sprite, 0, act->x, act->y)) {
                StartSound(SND_ROCKET_BURN);
            }
        }

        if (act->data2 < 5) {
            if (TestSpriteMove(DIR4_NORTH, SPR_ROCKET, 0, act->x, act->y - 1) == MOVE_FREE) {
                act->y--;
            } else {
                act->data5 = 1;
            }

            act->data4 = !act->data4;
            DrawSprite(SPR_ROCKET, act->data4 + 4, act->x, act->y + 6, DRAW_MODE_NORMAL);
            if (IsTouchingPlayer(SPR_ROCKET, 4, act->x, act->y + 6)) {
                HurtPlayer(index);
            }

            if (act->data4 != 0) {
                NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
            }
        }

        if (act->x == playerX && act->y - 7 <= playerY && act->y - 4 >= playerY) {
            playerRecoilLeft = 16;
            isPlayerRecoiling = true;
            ClearPlayerDizzy();
            isPlayerLongJumping = false;
            if (act->y - 7 == playerY) playerY++;
            if (act->y - 6 == playerY) playerY++;
            if (act->y - 4 == playerY) playerY--;
        }

        if (act->data2 > 4 && act->data2 % 2 != 0) {
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 2, DIR8_SOUTH, 1);
        }
    }

    if (act->data5 != 0) {
        act->dead = true;
        NewShard(SPR_ROCKET, 1, act->x,     act->y);
        NewShard(SPR_ROCKET, 2, act->x + 1, act->y);
        NewShard(SPR_ROCKET, 3, act->x + 2, act->y);
        NewExplosion(act->x - 4, act->y);
        NewExplosion(act->x + 1, act->y);
        nextDrawMode = DRAW_MODE_WHITE;
    }
}

/*
Handle one frame of pedestal movement.
*/
static void ActPedestal(word index)
{
    Actor *act = actors + index;
    word i;

    nextDrawMode = DRAW_MODE_HIDDEN;

    for (i = 0; act->data1 > i; i++) {
        DrawSprite(SPR_PEDESTAL, 1, act->x, act->y - i, DRAW_MODE_NORMAL);
    }

    DrawSprite(SPR_PEDESTAL, 0, act->x - 2, act->y - i, DRAW_MODE_NORMAL);
    SetMapTileRepeat(TILE_INVISIBLE_PLATFORM, 5, act->x - 2, act->y - i);

    if (act->data2 == 0 && IsNearExplosion(SPR_PEDESTAL, 1, act->x, act->y)) {
        act->data2 = 3;
    }

    if (act->data2 > 1) {
        act->data2--;
    }

    if (act->data2 == 1) {
        act->data2 = 3;

        SetMapTileRepeat(TILE_EMPTY, 5, act->x - 2, act->y - i);

        act->data1--;

        if (act->data1 == 1) {
            act->dead = true;
            NewShard(SPR_PEDESTAL, 0, act->x, act->y);
        } else {
            NewShard(SPR_PEDESTAL, 1, act->x, act->y);
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y + 1, DIR8_NORTH, 1);
        }
    }
}

/*
Handle one frame of invincibility bubble movement.

As long as this actor is alive, it follows the player and gives invincibility.
*/
static void ActInvincibilityBubble(word index)
{
    Actor *act = actors + index;
    byte frames[] = {0, 1, 2, 1};

    isPlayerInvincible = true;

    act->data1++;
    act->frame = frames[act->data1 % 4];

    if (act->data1 > 200 && act->data1 % 2 != 0) {
        nextDrawMode = DRAW_MODE_HIDDEN;
    }

    if (act->data1 == 240) {
        act->dead = true;
        nextDrawMode = DRAW_MODE_HIDDEN;
        isPlayerInvincible = false;
    } else {
        act->x = playerX - 1;
        act->y = playerY + 1;
    }
}

/*
Handle one frame of monument movement.
*/
static void ActMonument(word index)
{
    Actor *act = actors + index;
    int i;

    if (act->data2 != 0) {
        act->dead = true;
        nextDrawMode = DRAW_MODE_HIDDEN;
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 8);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 7);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y - 6);
        NewShard(SPR_MONUMENT, 3, act->x,     act->y);
        NewShard(SPR_MONUMENT, 3, act->x + 1, act->y);
        NewShard(SPR_MONUMENT, 3, act->x + 2, act->y);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTH, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTHEAST, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y,     DIR8_NORTHWEST, 2);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y - 4, DIR8_NORTH, 3);
        AddScore(25600);
        NewActor(ACT_SCORE_EFFECT_12800, act->x - 2, act->y - 9);
        NewActor(ACT_SCORE_EFFECT_12800, act->x + 2, act->y - 9);
        StartSound(SND_DESTROY_SOLID);

        return;
    }

    /* extra data var */
    if (act->westfree == 0) {
        act->westfree = 1;
        for (i = 0; i < 9; i++) {
            SetMapTile(TILE_SWITCH_BLOCK_1, act->x + 1, act->y - i);
        }
    }

    if (act->data1 != 0) {
        act->data1--;
        if (act->data1 % 2 != 0) {
            nextDrawMode = DRAW_MODE_WHITE;
        }
    }

    if (IsNearExplosion(SPR_MONUMENT, 0, act->x, act->y) && act->data1 == 0) {
        act->data1 = 10;
        act->frame++;
        if (act->frame == 3) {
            act->frame = 2;
            act->data2 = 1;
            for (i = 0; i < 9; i++) {
                SetMapTile(TILE_EMPTY, act->x + 1, act->y-i);
            }
        }
    }
}

/*
Handle one frame of tulip launcher movement.
*/
static void ActTulipLauncher(word index)
{
    byte launchframes[] = {0, 2, 1, 0, 1};
    Actor *act = actors + index;

    /* extra data var */
    if (act->eastfree > 0 && act->eastfree < 7) return;

    if (act->data3 != 0) {
        act->data3--;

        if (act->data3 % 2 != 0) {
            nextDrawMode = DRAW_MODE_WHITE;
        }

        return;
    }

    if (IsNearExplosion(act->sprite, act->frame, act->x, act->y)) {
        act->data3 = 15;

        act->data5++;
        if (act->data5 == 2) {
            act->dead = true;
            NewShard(SPR_PARACHUTE_BALL, 0, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 2, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 4, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 9, act->x + 2, act->y - 5);
            NewShard(SPR_PARACHUTE_BALL, 3, act->x + 2, act->y - 5);
            NewShard(act->sprite, act->frame, act->x, act->y);

            return;
        }
    }

    if (act->data2 == 0) {
        act->frame = launchframes[act->data1];

        act->data1++;
        /* extra data ver */
        if (act->data1 == 2 && act->westfree == 0) {
            NewSpawner(ACT_PARACHUTE_BALL, act->x + 2, act->y - 5);
            StartSound(SND_TULIP_LAUNCH);
        }

        if (act->data1 == 5) {
            act->data2 = 100;
            act->data1 = 0;
            act->westfree = 0;
        }
    } else {
        act->frame = 1;
        act->data2--;
    }
}

/*
Handle one frame of frozen D.N. movement.
*/
static void ActFrozenDN(word index)
{
    Actor *act = actors + index;

#ifdef HAS_ACT_FROZEN_DN
    nextDrawMode = DRAW_MODE_HIDDEN;

    if (act->data1 == 0) {
        if (IsNearExplosion(SPR_FROZEN_DN, 0, act->x, act->y)) {
            NewShard(SPR_FROZEN_DN, 6,  act->x,     act->y - 6);
            NewShard(SPR_FROZEN_DN, 7,  act->x + 4, act->y);
            NewShard(SPR_FROZEN_DN, 8,  act->x,     act->y - 5);
            NewShard(SPR_FROZEN_DN, 9,  act->x,     act->y - 4);
            NewShard(SPR_FROZEN_DN, 10, act->x + 5, act->y - 6);
            NewShard(SPR_FROZEN_DN, 11, act->x + 5, act->y - 4);
            StartSound(SND_SMASH);
            act->data1 = 1;
            act->x++;
        } else {
            DrawSprite(SPR_FROZEN_DN, 0, act->x, act->y, DRAW_MODE_NORMAL);
        }

    } else if (act->data1 == 1) {
        act->data2++;
        if (act->data2 % 2 != 0) {
            act->y--;
        }

        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAW_MODE_NORMAL);
        DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAW_MODE_NORMAL);
        NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);

        if (act->data2 == 10) {
            act->data1 = 2;
            act->data2 = 0;
        }

    } else if (act->data1 == 2) {
        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAW_MODE_NORMAL);
        DrawSprite(SPR_FROZEN_DN, 1, act->x, act->y, DRAW_MODE_NORMAL);

        act->data2++;
        if (act->data2 == 30) {
            ShowRescuedDNMessage();

            act->data1 = 3;
            act->data2 = 0;
        }

    } else if (act->data1 == 3) {
        act->data2++;

        DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAW_MODE_NORMAL);

        if (act->data2 < 10) {
            DrawSprite(SPR_FROZEN_DN, 1, act->x, act->y, DRAW_MODE_NORMAL);
        } else {
            DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAW_MODE_NORMAL);
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
        }

        if (act->data2 == 15) {
            act->data1 = 4;
            act->data2 = 0;
        }

    } else if (act->data1 == 4) {
        act->data2++;
        if (act->data2 == 1) {
            NewSpawner(ACT_HAMBURGER, act->x, act->y);
        }

        act->y--;

        if (act->data2 > 50 || !IsSpriteVisible(SPR_FROZEN_DN, 2, act->x, act->y)) {
            act->dead = true;
        } else {
            DrawSprite(SPR_FROZEN_DN, (act->data5++ % 2) + 4, act->x, act->y + 5, DRAW_MODE_NORMAL);
            DrawSprite(SPR_FROZEN_DN, 2, act->x, act->y, DRAW_MODE_NORMAL);
            NewDecoration(SPR_SMOKE, 6, act->x, act->y + 6, DIR8_SOUTH, 1);
            StartSound(SND_ROCKET_BURN);
        }
    }
#else
    (void) act;
#endif  /* HAS_ACT_FROZEN_DN */
}

/*
Handle one frame of flame pulse movement.
*/
static void ActFlamePulse(word index)
{
    Actor *act = actors + index;
    byte frames[] = {0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 1, 0};

    if (act->data1 == 0) {
        act->frame = frames[act->data2];

        if (act->frame == 2) {
            NewDecoration(SPR_SMOKE, 6, act->x - act->data5, act->y - 3, DIR8_NORTH, 1);
            StartSound(SND_FLAME_PULSE);
        }

        act->data2++;
        if (act->data2 == 16) {
            act->data1 = 30;
            act->data2 = 0;
        }
    } else {
        act->data1--;
        nextDrawMode = DRAW_MODE_HIDDEN;
    }
}

/*
Handle one frame of speech bubble movement.
*/
static void ActSpeechBubble(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_HIDDEN;

    if (act->data1 == 0) {
        StartSound(SND_SPEECH_BUBBLE);

        if (act->sprite == SPR_SPEECH_WOW_50K) {
            AddScore(50000L);
        }
    }

    act->data1++;

    if (act->data1 == 20) {
        act->dead = true;
    } else {
        DrawSprite(act->sprite, 0, playerX - 1, playerY - 5, DRAW_MODE_IN_FRONT);
    }
}

/*
Handle one frame of smoke emitter movement.
*/
static void ActSmokeEmitter(word index)
{
    Actor *act = actors + index;

    nextDrawMode = DRAW_MODE_HIDDEN;

    act->data1 = GameRand() % 32;
    if (act->data1 == 0) {
        if (act->data5 != 0) {
            NewDecoration(SPR_SMOKE, 6, act->x - 1, act->y, DIR8_NORTH, 1);
        } else {
            NewDecoration(SPR_SMOKE_LARGE, 6, act->x - 2, act->y, DIR8_NORTH, 1);
        }
    }
}

/*
Create a new actor of the specified type located at x,y.
Relies on the caller providing the correct index into the main actors array.
*/
bbool NewActorAtIndex(word index, word actor_type, word x, word y)
{
    /* This is probably to save having to pass this 240+ times below. */
    nextActorIndex = index;
    nextActorType = actor_type;

#define F false
#define T true

    switch (actor_type) {
    case ACT_BASKET_NULL:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BASKET_NULL, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_STAR_FLOAT:
        ConstructActor(SPR_STAR, x, y, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_JUMP_PAD_FLOOR:
        ConstructActor(SPR_JUMP_PAD, x, y, F, T, T, F, ActJumpPad, 0, 0, 0, 0, 0);
        break;
    case ACT_ARROW_PISTON_W:
        ConstructActor(SPR_ARROW_PISTON_W, x, y, F, T, F, F, ActArrowPiston, 0, 0, 0, 0, DIR2_WEST);
        break;
    case ACT_ARROW_PISTON_E:
        ConstructActor(SPR_ARROW_PISTON_E, x - 4, y, F, T, F, F, ActArrowPiston, 0, 0, 0, 0, DIR2_EAST);
        break;
    case ACT_FIREBALL_W:
        ConstructActor(SPR_FIREBALL, x, y, T, F, F, F, ActFireball, 0, x, y, 0, DIR2_WEST);
        break;
    case ACT_FIREBALL_E:
        ConstructActor(SPR_FIREBALL, x - 1, y, T, F, F, F, ActFireball, 0, x - 1, y, 0, DIR2_EAST);
        break;
    case ACT_HEAD_SWITCH_BLUE:
        ConstructActor(SPR_HEAD_SWITCH_BLUE, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_BLUE);
        break;
    case ACT_DOOR_BLUE:
        ConstructActor(SPR_DOOR_BLUE, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_RED:
        ConstructActor(SPR_HEAD_SWITCH_RED, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_RED);
        break;
    case ACT_DOOR_RED:
        ConstructActor(SPR_DOOR_RED, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_GREEN:
        ConstructActor(SPR_HEAD_SWITCH_GREEN, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_GREEN);
        break;
    case ACT_DOOR_GREEN:
        ConstructActor(SPR_DOOR_GREEN, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_HEAD_SWITCH_YELLOW:
        ConstructActor(SPR_HEAD_SWITCH_YELLOW, x, y + 1, F, F, F, F, ActHeadSwitch, 0, 0, 0, 0, SPR_DOOR_YELLOW);
        break;
    case ACT_DOOR_YELLOW:
        ConstructActor(SPR_DOOR_YELLOW, x, y, F, F, F, F, ActDoor, 0, 0, 0, 0, 0);
        break;
    case ACT_JUMP_PAD_ROBOT:
        ConstructActor(SPR_JUMP_PAD_ROBOT, x, y, T, F, F, F, ActJumpPadRobot, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_SPIKES_FLOOR:
        ConstructActor(SPR_SPIKES_FLOOR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_FLOOR_RECIP:
        ConstructActor(SPR_SPIKES_FLOOR_RECIP, x, y, F, F, F, F, ActReciprocatingSpikes, 1, 0, 0, 0, 0);
        break;
    case ACT_SAW_BLADE_VERT:
        ConstructActor(SPR_SAW_BLADE, x, y, F, T, F, T, ActVerticalMover, 0, 0, 0, 0, 0);
        break;
    case ACT_SAW_BLADE_HORIZ:
        ConstructActor(SPR_SAW_BLADE, x, y, T, F, F, T, ActHorizontalMover, 0, 0, 0, 0, 1);
        break;
    case ACT_BOMB_ARMED:
        ConstructActor(SPR_BOMB_ARMED, x, y, T, F, T, T, ActBombArmed, 0, 0, 0, 0, 0);
        break;
    case ACT_CABBAGE:
        ConstructActor(SPR_CABBAGE, x, y, F, T, T, T, ActCabbage, 1, 0, 0, 0, 0);
        break;
    case ACT_POWER_UP_FLOAT:
        ConstructActor(SPR_POWER_UP, x, y, T, F, T, F, ActPrize, 0, 0, 0, 1, 6);
        break;
    case ACT_BARREL_POWER_UP:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_POWER_UP_FLOAT, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_GRN_TOMATO:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_GRN_TOMATO, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_RED_TOMATO:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_TOMATO, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_YEL_PEAR:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_YEL_PEAR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_ONION:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_ONION, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_JUMP_PAD_FL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, T, ActBarrel, ACT_JUMP_PAD_FLOOR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_TOMATO:
        ConstructActor(SPR_GRN_TOMATO, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_TOMATO:
        ConstructActor(SPR_RED_TOMATO, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_YEL_PEAR:
        ConstructActor(SPR_YEL_PEAR, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_ONION:
        ConstructActor(SPR_ONION, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_SIGN:
        ConstructActor(SPR_EXIT_SIGN, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEAR:
        ConstructActor(SPR_SPEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEAR_RECIP:
        ConstructActor(SPR_SPEAR, x, y, F, F, F, F, ActReciprocatingSpear, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_SLIME_THROB:
        ConstructActor(SPR_GREEN_SLIME, x, y + 1, F, F, F, F, ActRedGreenSlime, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_SLIME_DRIP:
        ConstructActor(SPR_GREEN_SLIME, x, y + 1, F, T, F, F, ActRedGreenSlime, x, y + 1, 0, 0, 1);
        break;
    case ACT_FLYING_WISP:
        ConstructActor(SPR_FLYING_WISP, x, y, T, F, F, F, ActFlyingWisp, 0, 0, 0, 0, 0);
        break;
    case ACT_TWO_TONS_CRUSHER:
        ConstructActor(SPR_TWO_TONS_CRUSHER, x, y, F, T, F, F, ActTwoTonsCrusher, 0, 0, 0, 0, 0);
        break;
    case ACT_JUMPING_BULLET:
        ConstructActor(SPR_JUMPING_BULLET, x, y, F, T, F, F, ActJumpingBullet, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_STONE_HEAD_CRUSHER:
        ConstructActor(SPR_STONE_HEAD_CRUSHER, x, y, F, T, F, F, ActStoneHeadCrusher, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_CEIL:
        ConstructActor(SPR_PYRAMID, x, y + 1, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_FALLING:
        ConstructActor(SPR_PYRAMID, x, y + 1, F, T, F, T, ActPyramid, 0, 0, 0, 0, 0);
        break;
    case ACT_PYRAMID_FLOOR:
        ConstructActor(SPR_PYRAMID, x, y, F, F, F, F, ActPyramid, 0, 0, 0, 0, 1);
        break;
    case ACT_GHOST:
        ConstructActor(SPR_GHOST, x, y, F, T, F, F, ActGhost, 0, 0, 0, 0, 4);
        break;
    case ACT_MOON:
        ConstructActor(SPR_MOON, x, y, F, F, F, T, ActMoon, 0, 0, 0, 0, 4);
        break;
    case ACT_HEART_PLANT:
        ConstructActor(SPR_HEART_PLANT, x, y, F, F, F, F, ActHeartPlant, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_BOMB:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BOMB_IDLE, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BOMB_IDLE:
        ConstructActor(SPR_BOMB_IDLE, x, y, T, F, T, F, ActBombIdle, 0, 0, 0, 0, 0);
        break;
    case ACT_SWITCH_PLATFORMS:
        ConstructActor(SPR_FOOT_SWITCH_KNOB, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_PLATFORMS);
        arePlatformsActive = false;
        break;
    case ACT_SWITCH_MYSTERY_WALL:
        ConstructActor(SPR_FOOT_SWITCH_KNOB, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_MYSTERY_WALL);
        break;
    case ACT_MYSTERY_WALL:
        ConstructActor(SPR_MYSTERY_WALL, x, y, T, F, F, F, ActMysteryWall, 0, 0, 0, 0, 0);
        mysteryWallTime = 0;
        break;
    case ACT_BABY_GHOST:
        ConstructActor(SPR_BABY_GHOST, x, y, F, T, T, F, ActBabyGhost, DIR2_SOUTH, 0, 0, 0, 0);
        break;
    case ACT_PROJECTILE_SW:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTHWEST);
        break;
    case ACT_PROJECTILE_SE:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTHEAST);
        break;
    case ACT_PROJECTILE_S:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, T, ActProjectile, 0, 0, 0, 0, DIRP_SOUTH);
        break;
    case ACT_ROAMER_SLUG:
        ConstructActor(SPR_ROAMER_SLUG, x, y, F, T, F, F, ActRoamerSlug, 0, 3, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_N:
        ConstructActor(SPR_PIPE_CORNER_N, x, y, F, F, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_S:
        ConstructActor(SPR_PIPE_CORNER_S, x, y, F, F, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_W:
        ConstructActor(SPR_PIPE_CORNER_W, x, y, F, T, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_CORNER_E:
        ConstructActor(SPR_PIPE_CORNER_E, x, y, F, T, F, F, ActPipeCorner, 0, 0, 0, 0, 0);
        break;
    case ACT_BABY_GHOST_EGG_PROX:
        ConstructActor(SPR_BABY_GHOST_EGG, x, y, F, F, F, F, ActBabyGhostEgg, 0, 0, 0, 0, 0);
        break;
    case ACT_BABY_GHOST_EGG:
        ConstructActor(SPR_BABY_GHOST_EGG, x, y, F, F, F, F, ActBabyGhostEgg, 0, 0, 0, 0, 1);
        break;
    case ACT_SHARP_ROBOT_FLOOR:
        ConstructActor(SPR_SHARP_ROBOT_FLOOR, x, y, F, T, F, F, ActHorizontalMover, 8, 0, 0, 0, 1);
        break;
    case ACT_SHARP_ROBOT_CEIL:
        ConstructActor(SPR_SHARP_ROBOT_CEIL, x, y + 2, F, T, F, F, ActSharpRobot, 0, DIR2_WEST, 0, 0, 0);
        break;
    case ACT_BASKET_HAMBURGER:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_HAMBURGER, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_HAMBURGER:
        ConstructActor(SPR_HAMBURGER, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_CLAM_PLANT_FLOOR:
        ConstructActor(SPR_CLAM_PLANT, x, y, F, F, F, F, ActClamPlant, 0, 0, 0, 0, DRAW_MODE_NORMAL);
        break;
    case ACT_CLAM_PLANT_CEIL:
        ConstructActor(SPR_CLAM_PLANT, x, y + 2, F, F, F, F, ActClamPlant, 0, 0, 0, 0, DRAW_MODE_FLIPPED);
        break;
    case ACT_GRAPES:
        ConstructActor(SPR_GRAPES, x, y + 2, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PARACHUTE_BALL:
        ConstructActor(SPR_PARACHUTE_BALL, x, y, F, T, T, T, ActParachuteBall, 0, 20, 0, 0, 2);
        break;
    case ACT_SPIKES_E:
        ConstructActor(SPR_SPIKES_E, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_E_RECIP:
        ConstructActor(SPR_SPIKES_E_RECIP, x, y, F, F, F, F, ActReciprocatingSpikes, 1, 0, 0, 0, 0);
        break;
    case ACT_SPIKES_W:
        ConstructActor(SPR_SPIKES_W, x - 3, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BEAM_ROBOT:
        ConstructActor(SPR_BEAM_ROBOT, x, y, T, F, F, F, ActBeamRobot, 0, 0, 0, 0, 0);
        break;
    case ACT_SPLITTING_PLATFORM:
        ConstructActor(SPR_SPLITTING_PLATFORM, x, y, T, F, F, F, ActSplittingPlatform, 0, 0, 0, 0, 0);
        break;
    case ACT_SPARK:
        ConstructActor(SPR_SPARK, x, y, F, T, F, F, ActSpark, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_DANCE_MUSH:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_DANCING_MUSHROOM, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_DANCING_MUSHROOM:
        ConstructActor(SPR_DANCING_MUSHROOM, x, y, T, F, T, F, ActPrize, 0, 0, 0, 1, 2);
        break;
    case ACT_EYE_PLANT_FLOOR:
        ConstructActor(SPR_EYE_PLANT, x, y, F, T, F, F, ActEyePlant, 0, 0, 0, 0, DRAW_MODE_NORMAL);
        if (numEyePlants < 15) numEyePlants++;
        break;
    case ACT_EYE_PLANT_CEIL:
        ConstructActor(SPR_EYE_PLANT, x, y + 1, F, F, F, F, ActEyePlant, 0, 0, 0, 0, DRAW_MODE_FLIPPED);
        break;
    case ACT_BARREL_CABB_HARDER:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CABBAGE_HARDER, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_JUMPER:
        ConstructActor(SPR_RED_JUMPER, x, y, F, T, F, F, ActRedJumper, 0, 0, 0, 0, 7);
        break;
    case ACT_BOSS:
        ConstructActor(SPR_BOSS, x, y, F, T, F, F, ActBoss, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_OUTLET:
        ConstructActor(SPR_PIPE_END, x - 1, y + 2, T, F, F, F, ActPipeEnd, 0, 0, 0, 0, 0);
        break;
    case ACT_PIPE_INLET:
        ConstructActor(SPR_PIPE_END, x - 1, y + 2, F, T, F, F, ActPipeEnd, 0, 1, 0, 0, 0);
        break;
    case ACT_SUCTION_WALKER:
        ConstructActor(SPR_SUCTION_WALKER, x, y, F, T, F, F, ActSuctionWalker, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_TRANSPORTER_1:
        ConstructActor(SPR_TRANSPORTER, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 2);
        break;
    case ACT_TRANSPORTER_2:
        ConstructActor(SPR_TRANSPORTER, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 1);
        break;
    case ACT_PROJECTILE_W:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, F, ActProjectile, 0, 0, 0, 0, DIRP_WEST);
        break;
    case ACT_PROJECTILE_E:
        ConstructActor(SPR_PROJECTILE, x, y, T, F, F, F, ActProjectile, 0, 0, 0, 0, DIRP_EAST);
        break;
    case ACT_SPIT_WALL_PLANT_W:
        ConstructActor(SPR_SPIT_WALL_PLANT_W, x - 3, y, F, F, F, F, ActSpittingWallPlant, 0, 0, 0, 0, DIR4_WEST);
        break;
    case ACT_SPIT_WALL_PLANT_E:
        ConstructActor(SPR_SPIT_WALL_PLANT_E, x, y, F, F, F, F, ActSpittingWallPlant, 0, 0, 0, 0, DIR4_EAST);
        break;
    case ACT_SPITTING_TURRET:
        ConstructActor(SPR_SPITTING_TURRET, x, y, F, T, F, F, ActSpittingTurret, 0, 10, x, 0, 3);
        break;
    case ACT_SCOOTER:
        ConstructActor(SPR_SCOOTER, x, y, F, T, F, F, ActScooter, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_CHOMPER:
        ConstructActor(SPR_RED_CHOMPER, x, y, F, T, T, F, ActRedChomper, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_SWITCH_LIGHTS:
        ConstructActor(SPR_FOOT_SWITCH_KNOB, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_LIGHTS);
        areLightsActive = false;
        hasLightSwitch = true;
        break;
    case ACT_SWITCH_FORCE_FIELD:
        ConstructActor(SPR_FOOT_SWITCH_KNOB, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, ACT_SWITCH_FORCE_FIELD);
        break;
    case ACT_FORCE_FIELD_VERT:
        ConstructActor(SPR_FORCE_FIELD_VERT, x, y, T, F, F, F, ActForceField, 0, 0, 0, 0, 0);
        break;
    case ACT_FORCE_FIELD_HORIZ:
        ConstructActor(SPR_FORCE_FIELD_HORIZ, x, y, T, F, F, F, ActForceField, 0, 0, 0, 0, 1);
        break;
    case ACT_PINK_WORM:
        ConstructActor(SPR_PINK_WORM, x, y, F, T, T, F, ActPinkWorm, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_HINT_GLOBE_0:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 0);
        break;
    case ACT_PUSHER_ROBOT:
        ConstructActor(SPR_PUSHER_ROBOT, x, y, F, T, F, F, ActPusherRobot, DIR2_WEST, 0, 0, 0, 4);
        break;
    case ACT_SENTRY_ROBOT:
        ConstructActor(SPR_SENTRY_ROBOT, x, y, F, T, F, F, ActSentryRobot, DIR2_WEST, 0, 0, 0, 4);
        break;
    case ACT_PINK_WORM_SLIME:
        ConstructActor(SPR_PINK_WORM_SLIME, x, y, F, F, T, F, ActPinkWormSlime, 0, 0, 0, 0, 3);
        break;
    case ACT_DRAGONFLY:
        ConstructActor(SPR_DRAGONFLY, x, y, F, T, F, F, ActDragonfly, DIR2_WEST, 0, 0, 0, 0);
        break;
    case ACT_WORM_CRATE:
        ConstructActor(SPR_WORM_CRATE, x, y, T, F, F, F, ActWormCrate, 0, 0, 0, 0, ((GameRand() % 20) * 5) + 50);
        break;
    case ACT_BOTTLE_DRINK:
        ConstructActor(SPR_BOTTLE_DRINK, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_GRN_GOURD:
        ConstructActor(SPR_GRN_GOURD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BLU_SPHERES:
        ConstructActor(SPR_BLU_SPHERES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_POD:
        ConstructActor(SPR_POD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_PEA_PILE:
        ConstructActor(SPR_PEA_PILE, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_LUMPY_FRUIT:
        ConstructActor(SPR_LUMPY_FRUIT, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HORN:
        ConstructActor(SPR_HORN, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_BERRIES:
        ConstructActor(SPR_RED_BERRIES, x, y + 2, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_BOTL_DRINK:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BOTTLE_DRINK, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_GRN_GOURD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_GRN_GOURD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_BLU_SPHERES:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BLU_SPHERES, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_POD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_POD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_PEA_PILE:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_PEA_PILE, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BASKET_LUMPY_FRUIT:
        ConstructActor(SPR_BASKET, x, y, T, F, F, F, ActBarrel, ACT_LUMPY_FRUIT, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_HORN:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_HORN, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_SATELLITE:
        ConstructActor(SPR_SATELLITE, x, y, F, F, F, F, ActSatellite, 0, 0, 0, 0, 0);
        break;
    case ACT_IVY_PLANT:
        ConstructActor(SPR_IVY_PLANT, x, y + 7, F, T, F, F, ActIvyPlant, 5, 0, 0, 7, 0);
        break;
    case ACT_YEL_FRUIT_VINE:
        ConstructActor(SPR_YEL_FRUIT_VINE, x, y + 2, T, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HEADDRESS:
        ConstructActor(SPR_HEADDRESS, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_HEADDRESS:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_HEADDRESS, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_EXIT_MONSTER_W:
        ConstructActor(SPR_EXIT_MONSTER_W, x - 4, y, F, T, F, F, ActExitMonsterWest, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_LINE_VERT:
        ConstructActor(SPR_150, x, y, T, F, F, F, ActExitLineVertical, 0, 0, 0, 0, 0);
        break;
    case ACT_SMALL_FLAME:
        ConstructActor(SPR_SMALL_FLAME, x, y, F, F, F, F, ActSmallFlame, 0, 0, 0, 0, 0);
        break;
    case ACT_ROTATING_ORNAMENT:
        ConstructActor(SPR_ROTATING_ORNAMENT, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_BLU_CRYSTAL:
        ConstructActor(SPR_BLU_CRYSTAL, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 5);
        break;
    case ACT_RED_CRYSTAL_FLOOR:
        ConstructActor(SPR_RED_CRYSTAL, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 6);
        break;
    case ACT_BARREL_RT_ORNAMENT:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_ROTATING_ORNAMENT, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_BLU_CRYSTAL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BLU_CRYSTAL, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BARREL_RED_CRYSTAL:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_RED_CRYSTAL_FLOOR, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_TOMATO_FLOAT:
        ConstructActor(SPR_GRN_TOMATO, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_TOMATO_FLOAT:
        ConstructActor(SPR_RED_TOMATO, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_YEL_PEAR_FLOAT:
        ConstructActor(SPR_YEL_PEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BEAR_TRAP:
        ConstructActor(SPR_BEAR_TRAP, x, y, F, F, F, F, ActBearTrap, 0, 0, 0, 0, 0);
        break;
    case ACT_FALLING_FLOOR:
        ConstructActor(SPR_FALLING_FLOOR, x, y, F, T, F, F, ActFallingFloor, 0, 0, 0, 0, 0);
        break;
    case ACT_EP1_END_1:
    case ACT_EP1_END_2:
    case ACT_EP1_END_3:
        ConstructActor(SPR_164, x, y, T, F, F, F, ActEpisode1End, actor_type, 0, 0, 0, 0);
        break;
    case ACT_ROOT:
        ConstructActor(SPR_ROOT, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_ROOT:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_ROOT, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_REDGRN_BERRIES:
        ConstructActor(SPR_REDGRN_BERRIES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RG_BERRIES:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_REDGRN_BERRIES, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_GOURD:
        ConstructActor(SPR_RED_GOURD, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RED_GOURD:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_GOURD, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_GRN_EMERALD:
        ConstructActor(SPR_GRN_EMERALD, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 5);
        break;
    case ACT_BARREL_GRN_EMERALD:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_GRN_EMERALD, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_CLR_DIAMOND:
        ConstructActor(SPR_CLR_DIAMOND, x, y, T, F, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_BARREL_CLR_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CLR_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_100:
        ConstructActor(SPR_SCORE_EFFECT_100, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_200:
        ConstructActor(SPR_SCORE_EFFECT_200, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_400:
        ConstructActor(SPR_SCORE_EFFECT_400, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_800:
        ConstructActor(SPR_SCORE_EFFECT_800, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_1600:
        ConstructActor(SPR_SCORE_EFFECT_1600, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_3200:
        ConstructActor(SPR_SCORE_EFFECT_3200, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_6400:
        ConstructActor(SPR_SCORE_EFFECT_6400, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_SCORE_EFFECT_12800:
        ConstructActor(SPR_SCORE_EFFECT_12800, x, y, F, T, F, F, ActScoreEffect, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_PLANT:
        ConstructActor(SPR_EXIT_PLANT, x, y, F, T, F, F, ActExitPlant, 0, 0, 30, 0, 0);
        break;
    case ACT_BIRD:
        ConstructActor(SPR_BIRD, x, y, F, T, F, F, ActBird, 0, 0, 0, DIR2_WEST, 0);
        break;
    case ACT_ROCKET:
        ConstructActor(SPR_ROCKET, x, y, F, T, F, F, ActRocket, 60, 10, 0, 0, 0);
        break;
    case ACT_INVINCIBILITY_CUBE:
        ConstructActor(SPR_INVINCIBILITY_CUBE, x, y, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_PEDESTAL_SMALL:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 13, 0, 0, 0, 0);
        break;
    case ACT_PEDESTAL_MEDIUM:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 19, 0, 0, 0, 0);
        break;
    case ACT_PEDESTAL_LARGE:
        ConstructActor(SPR_PEDESTAL, x, y, T, F, F, F, ActPedestal, 25, 0, 0, 0, 0);
        break;
    case ACT_INVINCIBILITY_BUBB:
        ConstructActor(SPR_INVINCIBILITY_BUBB, x, y, F, F, F, F, ActInvincibilityBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_BARREL_CYA_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_CYA_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_CYA_DIAMOND:
        ConstructActor(SPR_CYA_DIAMOND, x, y, T, F, T, F, ActPrize, 3, 2, 0, 0, 1);
        break;
    case ACT_BARREL_RED_DIAMOND:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_RED_DIAMOND, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_DIAMOND:
        ConstructActor(SPR_RED_DIAMOND, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BARREL_GRY_OCTAHED:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_GRY_OCTAHEDRON, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_GRY_OCTAHEDRON:
        ConstructActor(SPR_GRY_OCTAHEDRON, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BARREL_BLU_EMERALD:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_BLU_EMERALD, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_BLU_EMERALD:
        ConstructActor(SPR_BLU_EMERALD, x, y, T, F, T, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_THRUSTER_JET:
        ConstructActor(SPR_THRUSTER_JET, x, y + 2, F, F, F, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_EXIT_TRANSPORTER:
        ConstructActor(SPR_TRANSPORTER, x, y, T, F, F, F, ActTransporter, 0, 0, 0, 0, 3);
        break;
    case ACT_HINT_GLOBE_1:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 1);
        break;
    case ACT_HINT_GLOBE_2:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 2);
        break;
    case ACT_HINT_GLOBE_3:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 3);
        break;
    case ACT_HINT_GLOBE_4:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 4);
        break;
    case ACT_HINT_GLOBE_5:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 5);
        break;
    case ACT_HINT_GLOBE_6:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 6);
        break;
    case ACT_HINT_GLOBE_7:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 7);
        break;
    case ACT_HINT_GLOBE_8:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 8);
        break;
    case ACT_HINT_GLOBE_9:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 9);
        break;
    case ACT_SPIKES_FLOOR_BENT:
        ConstructActor(SPR_SPIKES_FLOOR_BENT, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_MONUMENT:
        ConstructActor(SPR_MONUMENT, x, y, F, F, F, F, ActMonument, 0, 0, 0, 0, 0);
        break;
    case ACT_CYA_DIAMOND_FLOAT:
        ConstructActor(SPR_CYA_DIAMOND, x, y, F, F, F, F, ActPrize, 3, 2, 0, 0, 1);
        break;
    case ACT_RED_DIAMOND_FLOAT:
        ConstructActor(SPR_RED_DIAMOND, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_GRY_OCTAHED_FLOAT:
        ConstructActor(SPR_GRY_OCTAHEDRON, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_BLU_EMERALD_FLOAT:
        ConstructActor(SPR_BLU_EMERALD, x, y, F, F, F, F, ActPrize, 2, 2, 0, 0, 1);
        break;
    case ACT_TULIP_LAUNCHER:
        ConstructActor(SPR_TULIP_LAUNCHER, x, y, F, F, F, F, ActTulipLauncher, 0, 30, 0, 0, 0);
        break;
    case ACT_JUMP_PAD_CEIL:
        ConstructActor(SPR_JUMP_PAD, x, y, T, F, F, F, ActJumpPad, 0, 0, y + 1, y + 3, 1);
        break;
    case ACT_BARREL_HEADPHONES:
        ConstructActor(SPR_BARREL, x, y, T, F, T, F, ActBarrel, ACT_HEADPHONES, SPR_BARREL_SHARDS, 0, 0, 0);
        break;
    case ACT_HEADPHONES_FLOAT:
        ConstructActor(SPR_HEADPHONES, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_HEADPHONES:
        ConstructActor(SPR_HEADPHONES, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_FROZEN_DN:
        ConstructActor(SPR_FROZEN_DN, x, y, F, F, F, F, ActFrozenDN, 0, 0, 0, 0, 0);
        break;
    case ACT_BANANAS:
        ConstructActor(SPR_BANANAS, x, y + 1, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_RED_LEAFY:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_RED_LEAFY, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_RED_LEAFY_FLOAT:
        ConstructActor(SPR_RED_LEAFY, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_LEAFY:
        ConstructActor(SPR_RED_LEAFY, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_BRN_PEAR:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_BRN_PEAR, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_BRN_PEAR_FLOAT:
        ConstructActor(SPR_BRN_PEAR, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BRN_PEAR:
        ConstructActor(SPR_BRN_PEAR, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_BASKET_CANDY_CORN:
        ConstructActor(SPR_BASKET, x, y, T, F, T, F, ActBarrel, ACT_CANDY_CORN, SPR_BASKET_SHARDS, 0, 0, 0);
        break;
    case ACT_CANDY_CORN_FLOAT:
        ConstructActor(SPR_CANDY_CORN, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_CANDY_CORN:
        ConstructActor(SPR_CANDY_CORN, x, y, T, F, T, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_FLAME_PULSE_W:
        ConstructActor(SPR_FLAME_PULSE_W, x - 1, y, F, F, F, F, ActFlamePulse, 0, 0, 0, 0, 1);
        break;
    case ACT_FLAME_PULSE_E:
        ConstructActor(SPR_FLAME_PULSE_E, x, y, F, F, F, F, ActFlamePulse, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_SLIME_THROB:
        ConstructActor(SPR_RED_SLIME, x, y + 1, F, F, F, F, ActRedGreenSlime, 0, 0, 0, 0, 0);
        break;
    case ACT_RED_SLIME_DRIP:
        ConstructActor(SPR_RED_SLIME, x, y + 1, F, T, F, F, ActRedGreenSlime, x, y + 1, 0, 0, 1);
        break;
    case ACT_HINT_GLOBE_10:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 10);
        break;
    case ACT_HINT_GLOBE_11:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 11);
        break;
    case ACT_HINT_GLOBE_12:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 12);
        break;
    case ACT_HINT_GLOBE_13:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 13);
        break;
    case ACT_HINT_GLOBE_14:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 14);
        break;
    case ACT_HINT_GLOBE_15:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 15);
        break;
    case ACT_SPEECH_OUCH:
        ConstructActor(SPR_SPEECH_OUCH, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_WHOA:
        ConstructActor(SPR_SPEECH_WHOA, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_UMPH:
        ConstructActor(SPR_SPEECH_UMPH, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_SPEECH_WOW_50K:
        ConstructActor(SPR_SPEECH_WOW_50K, x, y, T, F, F, F, ActSpeechBubble, 0, 0, 0, 0, 0);
        break;
    case ACT_EXIT_MONSTER_N:
        ConstructActor(SPR_EXIT_MONSTER_N, x, y, F, F, F, F, ActFootSwitch, 0, 0, 0, 0, 0);
        break;
    case ACT_SMOKE_EMIT_SMALL:
        ConstructActor(SPR_248, x, y, F, F, F, F, ActSmokeEmitter, 0, 0, 0, 0, 1);
        break;
    case ACT_SMOKE_EMIT_LARGE:
        ConstructActor(SPR_249, x, y, F, F, F, F, ActSmokeEmitter, 1, 0, 0, 0, 0);
        break;
    case ACT_EXIT_LINE_HORIZ:
        ConstructActor(SPR_250, x, y, T, F, F, F, ActExitLineHorizontal, 0, 0, 0, 0, 0);
        break;
    case ACT_CABBAGE_HARDER:
        ConstructActor(SPR_CABBAGE, x, y, T, F, T, T, ActCabbage, 2, 0, 0, 0, 0);
        break;
    case ACT_RED_CRYSTAL_CEIL:
        ConstructActor(SPR_RED_CRYSTAL, x, y + 1, F, F, F, F, ActPrize, 1, 0, 0, 0, 6);
        break;
    case ACT_HINT_GLOBE_16:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 16);
        break;
    case ACT_HINT_GLOBE_17:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 17);
        break;
    case ACT_HINT_GLOBE_18:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 18);
        break;
    case ACT_HINT_GLOBE_19:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 19);
        break;
    case ACT_HINT_GLOBE_20:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 20);
        break;
    case ACT_HINT_GLOBE_21:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 21);
        break;
    case ACT_HINT_GLOBE_22:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 22);
        break;
    case ACT_HINT_GLOBE_23:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 23);
        break;
    case ACT_HINT_GLOBE_24:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 24);
        break;
    case ACT_HINT_GLOBE_25:
        ConstructActor(SPR_HINT_GLOBE, x, y, F, F, F, F, ActHintGlobe, 0, 0, 0, 0, 25);
        break;
    case ACT_POWER_UP:
        ConstructActor(SPR_POWER_UP, x, y, F, T, T, F, ActPrize, 0, 0, 0, 1, 6);
        break;
    case ACT_STAR:
        ConstructActor(SPR_STAR, x, y, F, T, T, F, ActPrize, 0, 0, 0, 0, 4);
        break;
    case ACT_EP2_END_LINE:
        ConstructActor(SPR_265, x, y + 3, T, F, F, F, ActExitLineHorizontal, 1, 0, 0, 0, 0);
        break;
    default:
        return false;
    }

#undef F
#undef T

    return true;
}

/*
Add a new actor of the specified type at x,y. This function finds a free slot.
*/
void NewActor(word actor_type, word x_origin, word y_origin)
{
    word i;
    Actor *act;

    for (i = 0; i < numActors; i++) {
        act = actors + i;

        if (!act->dead) continue;

        NewActorAtIndex(i, actor_type, x_origin, y_origin);

        if (actor_type == ACT_PARACHUTE_BALL) {
            act->forceactive = true;
        }

        return;
    }

    if (numActors < MAX_ACTORS - 2) {
        act = actors + numActors;

        NewActorAtIndex(numActors, actor_type, x_origin, y_origin);

        if (actor_type == ACT_PARACHUTE_BALL) {
            act->forceactive = true;
        }

        numActors++;
    }
}