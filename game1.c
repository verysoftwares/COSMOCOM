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
 *                           COSMORE "GAME1" UNIT                            *
 *                                                                           *
 * This file contains the bulk of the game code. Notable elements that are   *
 * *not* present here include the AdLib service, UI utility functions,       *
 * joystick input, status bar, config/group file functions, and the majority *
 * of the in-game text screens. All of these are in GAME2.                   *
 *                                                                           *
 * This file pushes the TCC compiler to the limits of available memory.      *
 *****************************************************************************/

#include "glue.h"

/* global variables, see meta.h for comments */

bbool isInGame = false;
bool winGame;
dword gameScore, gameStars;
bbool isNewGame;
bool winLevel;

bbool isCartoonDataLoaded = false;
word miscDataContents = IMAGE_NONE;

byte demoState;
word demoDataLength, demoDataPos;
bbool isDebugMode = false;

int dir8X[] = {0,  0,  1,  1,  1,  0, -1, -1, -1};
int dir8Y[] = {0, -1, -1,  0,  1,  1,  1,  0, -1};

word activePage = 0;
word gameTickCount;
word randStepCount;
dword paletteStepCount;

char *stnGroupFilename = FILENAME_BASE ".STN";
char *volGroupFilename = FILENAME_BASE ".VOL";
char *fullscreenImageNames[] = {
    "PRETITLE.MNI", TITLE_SCREEN, "CREDIT.MNI", "BONUS.MNI", END_SCREEN,
    "ONEMOMNT.MNI"
};
char *backdropNames[] = {
    "bdblank.mni", "bdpipe.MNI", "bdredsky.MNI", "bdrocktk.MNI", "bdjungle.MNI",
    "bdstar.MNI", "bdwierd.mni", "bdcave.mni", "bdice.mni", "bdshrum.mni",
    "bdtechms.mni", "bdnewsky.mni", "bdstar2.mni", "bdstar3.mni",
    "bdforest.mni", "bdmountn.mni", "bdguts.mni", "bdbrktec.mni",
    "bdclouds.mni", "bdfutcty.mni", "bdice2.mni", "bdcliff.mni", "bdspooky.mni",
    "bdcrystl.mni", "bdcircut.mni", "bdcircpc.mni"
};
char *mapNames[] = MAP_NAMES;
char *musicNames[] = {
    "mcaves.mni", "mscarry.mni", "mboss.mni", "mrunaway.mni", "mcircus.mni",
    "mtekwrd.mni", "measylev.mni", "mrockit.mni", "mhappy.mni", "mdevo.mni",
    "mdadoda.mni", "mbells.mni", "mdrums.mni", "mbanjo.mni", "measy2.mni",
    "mteck2.mni", "mteck3.mni", "mteck4.mni", "mzztop.mni"
};
char *starBonusRanks[] = {
    "    Not Bad!    ", "    Way Cool    ", "     Groovy     ",
    "    Radical!    ", "     Insane     ", "     Gnarly     ",
    "   Outrageous   ", "   Incredible   ", "    Awesome!    ",
    "   Brilliant!   ", "    Profound    ", "    Towering    ",
    "Rocket Scientist"
};

dword totalMemFreeBefore, totalMemFreeAfter;
InterruptFunction savedInt9;
char *writePath;

HighScoreName highScoreNames[11];
dword highScoreValues[11];
byte soundPriority[80 + 1];
Platform platforms[MAX_PLATFORMS];
Fountain fountains[MAX_FOUNTAINS];
Light lights[MAX_LIGHTS];
Actor actors[MAX_ACTORS];
Shard shards[MAX_SHARDS];
Explosion explosions[MAX_EXPLOSIONS];
Spawner spawners[MAX_SPAWNERS];
Decoration decorations[MAX_DECORATIONS];
word decorationFrame[MAX_DECORATIONS];
word backdropTable[BACKDROP_WIDTH * BACKDROP_HEIGHT * 4];

byte *fontTileData, *maskedTileData, *miscData;
byte *actorTileData[3], *playerTileData, *tileAttributeData;
word *actorInfoData, *playerInfoData, *cartoonInfoData;
word *soundData1, *soundData2, *soundData3, *soundDataPtr[80];
Map mapData;

dword lastGroupEntryLength;
word nextActorIndex, nextDrawMode;

byte lastScancode;
bbool isKeyDown[BYTE_MAX];
bool isJoystickReady;
bbool cmdWest, cmdEast, cmdNorth, cmdSouth, cmdJump, cmdBomb;
bbool blockMovementCmds, cmdJumpLatch;
bool blockActionCmds;

bool isMusicEnabled, isSoundEnabled;
byte scancodeWest, scancodeEast, scancodeNorth, scancodeSouth, scancodeJump, scancodeBomb;

Music *activeMusic;
word activeSoundIndex, activeSoundPriority;
bool isNewSound, enableSpeaker;

word levelNum;
word lvlMusicNum;
word lvlBdnum;
word mapWidth, maxScrollY, mapYPower;
bool hasLightSwitch, hasRain, hasHScrollBackdrop, hasVScrollBackdrop;
bool areForceFieldsActive, areLightsActive, arePlatformsActive;
byte paletteAnimationNum;

word numActors;
word numPlatforms, numFountains, numLights;
word numBarrels, numEyePlants, pounceStreak;
word mysteryWallTime;
word activeTransporter, transporterTimeLeft;

/*
Random number generator for world events.

Unlike rand() or the random() macro, this function returns deterministic results
for consistent demo recording/playback.

The upper bound for return value is *roughly* the perimeter measurement of the
map (in tiles) plus 236. In practice this is around 1,350. The lower bound is
maybe 10 or 20.
*/
word GameRand(void)
{
    static word randtable[] = {
        31,  12,  17,  233, 99,  8,   64,  12,  199, 49,  5,   6,
        143, 1,   35,  46,  52,  5,   8,   21,  44,  8,   3,   77,
        2,   103, 34,  23,  78,  2,   67,  2,   79,  46,  1,   98
    };

    randStepCount++;
    if (randStepCount > 35) randStepCount = 0;

    return randtable[randStepCount] + scrollX + scrollY + randStepCount + playerX + playerY;
}

/*
Read the next color from the palette animation array and load it in.
*/
static void StepPalette(byte *pal_table)
{
    paletteStepCount++;
    if (pal_table[(word)paletteStepCount] == END_ANIMATION) paletteStepCount = 0;

    /* Jump by 8 converts COLORS into MODE1_COLORS */
    SetPaletteRegister(
        PALETTE_KEY_INDEX,
        pal_table[(word)paletteStepCount] < 8 ?
            pal_table[(word)paletteStepCount] : pal_table[(word)paletteStepCount] + 8
    );
}

/*
Handle palette animation for this frame.
*/
void AnimatePalette(void)
{
    static byte lightningState = 0;

#ifdef EXPLOSION_PALETTE
    if (paletteAnimationNum == PAL_ANIM_EXPLOSIONS) return;
#endif  /* EXPLOSION_PALETTE */

    switch (paletteAnimationNum) {
    case PAL_ANIM_LIGHTNING:
        if (lightningState == 2) {
            lightningState = 0;
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_DARKGRAY);
        } else if (lightningState == 1) {
            lightningState = 2;
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_LIGHTGRAY);
        } else if (rand() < 1500) {
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_WHITE);
            StartSound(SND_THUNDER);
            lightningState = 1;
        } else {
            SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_BLACK);
            lightningState = 0;
        }
        break;

    /*
    Palette tables passed to StepPalette() must use COLORS, *not* MODE1_COLORS!
    */
    case PAL_ANIM_R_Y_W:  /* red-yellow-white */
        {
            static byte rywTable[] = {
                RED, RED, LIGHTRED, LIGHTRED, YELLOW, YELLOW, WHITE, WHITE,
                YELLOW, YELLOW, LIGHTRED, LIGHTRED, END_ANIMATION
            };

            StepPalette(rywTable);
        }
        break;

    case PAL_ANIM_R_G_B:  /* red-green-blue */
        {
            static byte rgbTable[] = {
                BLACK, BLACK, RED, RED, LIGHTRED, RED, RED,
                BLACK, BLACK, GREEN, GREEN, LIGHTGREEN, GREEN, GREEN,
                BLACK, BLACK, BLUE, BLUE, LIGHTBLUE, BLUE, BLUE, END_ANIMATION
            };

            StepPalette(rgbTable);
        }
        break;

    case PAL_ANIM_MONO:  /* monochrome */
        {
            static byte monoTable[] = {
                BLACK, BLACK, DARKGRAY, LIGHTGRAY, WHITE, LIGHTGRAY, DARKGRAY,
                END_ANIMATION
            };

            StepPalette(monoTable);
        }
        break;

    case PAL_ANIM_W_R_M:  /* white-red-magenta */
        {
            static byte wrmTable[] = {
                WHITE, WHITE, WHITE, WHITE, WHITE, WHITE, RED, LIGHTMAGENTA,
                END_ANIMATION
            };

            StepPalette(wrmTable);
        }
        break;
    }
}

/*
Draw a single line of text with the first character at the specified X/Y origin.

A primitive form of markup is supported (all values are zero-padded decimal):
- \xFBnnn: Draw cartoon image nnn at the current position.
- \xFCnnn: Wait nnn times WaitHard(3) before drawing each character and play a
  per-character typewriter sound effect.
- \xFDnnn: Draw player sprite nnn at the current position.
- \xFEnnniii: Draw sprite type nnn, frame iii at the current position.

The draw position is not adjusted when cartoons/sprites are inserted -- the
caller must follow each of these sequences with an appropriate number of space
characters to clear the graphic before writing additional text.

NOTE: C's parser treats strings like "\xFC003" as hex literals with more than
two digits, resulting in a compile-time error. In calling code, you'll see text
broken up like "\xFC""003". This is intentional, and the only reasonable
workaround. (Unless we go octal...)
*/
void DrawTextLine(word x_origin, word y_origin, char *text)
{
    register int x = 0;
    register word delay = 0;
    word delayleft = 0;

    EGA_MODE_DEFAULT();

    while (text[x] != '\0') {
        if (text[x] == '\xFE' || text[x] == '\xFB' || text[x] == '\xFD' || text[x] == '\xFC') {
            char lookahead[4];
            word sequence1, sequence2;

            lookahead[0] = text[x + 1];
            lookahead[1] = text[x + 2];
            lookahead[2] = text[x + 3];
            lookahead[3] = '\0';
            sequence1 = atoi(lookahead);

            /*
            Be careful in here! The base address of the `text` array changes:

                (assume `text` contains the string "demonstrate")
                text[0] == 'd';  (true)
                text += 4;
                text[0] == 'n';  (also true!)
            */
            if (text[x] == '\xFD') {  /* draw player sprite */
                DrawPlayer(sequence1, x_origin + x, y_origin, DRAW_MODE_ABSOLUTE);
                text += 4;

            } else if (text[x] == '\xFB') {  /* draw cartoon image */
                DrawCartoon(sequence1, x_origin + x, y_origin);
                text += 4;

            } else if (text[x] == '\xFC') {  /* inter-character wait + sound */
                text += 4;
                /* atoi() here wastefully recalculates the value in `sequence1` */
                delayleft = delay = atoi(lookahead);

            } else {  /* \xFE; draw actor sprite */
                lookahead[0] = text[x + 4];
                lookahead[1] = text[x + 5];
                lookahead[2] = text[x + 6];
                lookahead[3] = '\0';
                sequence2 = atoi(lookahead);

                DrawSprite(sequence1, sequence2, x_origin + x, y_origin, DRAW_MODE_ABSOLUTE);
                text += 7;
            }

            continue;
        }

        if (delay != 0 && lastScancode == SCANCODE_SPACE) {
            WaitHard(1);
        } else if (delayleft != 0) {
            WaitHard(3);

            delayleft--;
            if (delayleft != 0) continue;
            delayleft = delay;

            if (text[x] != ' ') {
                StartSound(SND_TEXT_TYPEWRITER);
            }
        }

        if (text[x] >= 'a') {  /* lowercase */
            DrawSpriteTile(
                /* `FONT_LOWER_A` is equal to ASCII 'a' */
                fontTileData + FONT_LOWER_A + ((text[x] - 'a') * 40), x_origin + x, y_origin
            );
        } else {  /* uppercase, digits, and symbols */
            DrawSpriteTile(
                /* `FONT_UP_ARROW` is equal to ASCII (CP437) "upwards arrow" */
                fontTileData + FONT_UP_ARROW + ((text[x] - '\x18') * 40), x_origin + x, y_origin
            );
        }

        x++;
    }
}

/*
Replace the entire screen contents with a full-size (320x200) image.
*/
void DrawFullscreenImage(word image_num)
{
    byte *destbase = MK_FP(0xa000, 0);

    if (image_num != IMAGE_TITLE && image_num != IMAGE_CREDITS) {
        StopMusic();
    }

    if (image_num != miscDataContents) {
        FILE *fp = GroupEntryFp(fullscreenImageNames[image_num]);

        miscDataContents = image_num;

        fread(miscData, 32000, 1, fp);
        fclose(fp);
    }

    EGA_MODE_DEFAULT();
    EGA_BIT_MASK_DEFAULT();
    FadeOut();
    SelectDrawPage(0);

    {  /* for scope */
        register word srcbase;
        register int i;
        word mask = 0x0100;

        for (srcbase = 0; srcbase < 32000; srcbase += 8000) {
            outport(0x03c4, 0x0002 | mask);

            for (i = 0; i < 8000; i++) {
                *(destbase + i) = *(miscData + i + srcbase);
            }

            mask <<= 1;
        }
    }

    SelectActivePage(0);
    FadeIn();
}

/*
Trigger playback of a new sound.

If a sound is playing, it will be interrupted if the new sound has equal or
greater priority.
*/
void StartSound(word sound_num)
{
    if (soundPriority[sound_num] < activeSoundPriority) return;

    activeSoundPriority = soundPriority[sound_num];
    isNewSound = true;
    activeSoundIndex = sound_num - 1;
    enableSpeaker = false;
}

/*
Load row-planar tile image data into EGA memory.
*/
void CopyTilesToEGA(byte *source, word dest_length, word dest_offset)
{
    word i;
    word mask;
    byte *src = source;
    byte *dest = MK_FP(0xa000, dest_offset);

    for (i = 0; i < dest_length; i++) {
        for (mask = 0x0100; mask < 0x1000; mask = mask << 1) {
            outport(0x03c4, mask | 0x0002);

            *(dest + i) = *(src++);
        }
    }
}

/*
Draw the static game world (backdrop plus all solid/masked map tiles), windowed
to the current scroll position.

Note about `EGA_OFFSET_*`: DrawSolidTile() interprets the source offset given to
it as relative to the start of the area of video memory which is used to hold
solid tiles (and backdrops), so it adds an offset of `EGA_OFFSET_SOLID_TILES` to
it. But CopyTilesToEGA() uses absolute addresses (i.e., relative to the start of
video memory). This means we need to adjust our backdrop addresses, which are
given as absolute addresses, before we can use them as arguments to
DrawSolidTile().

How the parallax scrolling works

The backdrop scrolls in steps of 4 pixels, while the rest of the gameworld
(tiles and sprites) moves in 8-pixel steps. This difference in scrolling speed
creates the parallax effect, adding an illusion of depth to the scene.

However, the backdrop image data is arranged in a way that doesn't lend itself
to drawing with a 4-pixel source offset. It's grouped into 8x8 pixel tiles in
order to allow for fast drawing via the EGA's latch registers(see DrawSolidTile
in lowlevel.asm). If we wanted to draw this data with a 4-pixel offset, we
would need to draw the second half of one tile for the first 4 pixels, and then
draw the first half of the next tile for the next 4 pixels. This kind of
partial tile drawing is not possible via the latch copy technique, so we would
lose all the speed benefits it gives us (there might be some way to make it
work via the EGA's bitmasks, but it would still be slower and a lot more
complicated).

In order to make 4-pixel offsets possible, the game instead creates copies of
the backdrop image that are shifted up/left by 4 pixels, and uploads these
shifted versions to video memory next to the unmodified backdrop image. This
happens ahead of time, during backdrop loading (see LoadBackdropData).

With these copies in place, now it's just a matter of selecting the correct
version depending on the scroll positions. Even positions use the unmodified
backdrop, odd positions use the shifted version. If only scrollX is odd, we
thus use the copy that's shifted left by 4. If only scrollY is odd, we use the
version that's shifted up by 4, and if both scrollX and scrollY are odd, we
need to use the 3rd copy, which is shifted in both directions. The end result
is that the backdrop appears to move by 4 pixels when the scroll position
changes by one, due to this scheme of switching between the different images.

Now, there's more to it than just the image switching, of course. If that was
all we would do, the backdrop would simply appear to jump back and forth by 4
pixels as the scroll position alternates between odd and even. But the backdrop
keeps moving forward, until it eventually wraps around. At scroll position
(4, 0) for example, the regular (not shifted) backdrop is drawn, but starting
with the 3rd tile from the left instead of the 1st one. How is this achieved?
This is where the backdrop start offset lookup table comes into play. This
lookup table is an optimization, so before we look at how it works, let's first
talk about how we would implement this in a more straightforward (but less
optimal) way.

To draw the backdrop with an offset of N tiles, we need to add an offset to the
source address. An offset of 8 skips one tile horizontally, 320 skips one row
of tiles vertically. Each change in scroll position results in an offset of 4
pixels, with odd positions being handled by the image selection as described
above. Thus, dividing the scroll position by two gives us a tile column/row
index for offseting the start address. Concretely:

    offsetX = scrollX / 2 * 8
    offsetY = scrollY / 2 * 320

So now we have a way to draw the backdrop offset (scrolled) by a number of
tiles, but we still need to handle wrap-around. Let's again say that scrollX is
4, so we draw the backdrop starting at column index 2 (3rd column of tiles).
This means we run out of tiles to draw before we reach the right side of the
screen, since we've skipped the first 2. We need to draw these skipped tiles
after drawing the originally right-most tile, so that the backdrop wraps around
and repeats.

This could be accomplished by doing 'offsetX % 320'. Sticking with starting at
index 2, we would offset all tile addresses by 16. Once we've drawn 38 tiles
out of 40 (the width of the entire screen), we're already at offset 320
(38*8 + 16), which would result in drawing the first tile of the source image's
2nd row, and would draw whatever is next in video memory once we reach the
bottom-most row of tiles. By doing a modulo, we instead end up back at 0, which
gives us what we want - drawing the left-most tile right after we've drawn the
right-most one. For vertical scrolling, it's the same except that we need to do
a modulo by 5760 (320*18).

Putting this all together, a possible way to implement the backdrop drawing
would be:

    // before the loop
    bdStartOffsetX = hasHScrollBackdrop ? scrollX / 2 * 8 : 0;
    bdStartOffsetY = hasVScrollBackdrop ? scrollY / 2 * 320 : 0;
    yOffset = 0;

    // inside the loop
    DrawSolidTile(
      bdBase +
      (bdStartOffsetX + xTile) % 320 +
      (bdStartOffsetY + yOffset) % 5760,
      xTile + destoff);

    // Once we finish drawing a row of tiles
    yOffset += 320;

This would work, and is not too complicated. But on the hardware of the time,
modulo operations are expensive: On an x86 CPU, they are implemented via the
DIV or IDIV instruction, and these instructions take 22 and 25 cycles,
respectively (according to
https://www2.math.uni-wuppertal.de/~fpf/Uebungen/GdR-SS02/opcode_i.html).
Compared to the 2 or 3 cycles needed for an addition or logical operation, this
is quite a lot - about an order of magnitude slower. Since the game has to
redraw the entire screen full of backdrop and map tiles every frame, making the
drawing fast was clearly important. The solution the developers settled on was
to use a lookup table.

The idea is to create a table of start offsets for all of the 40*18 tiles a
backdrop is comprised off, and then to repeat this table in both directions
(i.e., horizontally and vertically). So the entire table is a 80x36 grid of
values, stored in a 1-dimensional array. The first 40 entries in the first row
of the table are values from 0 to 312, which is the same as the offsets we need
to use when drawing a backdrop at scroll position (0,0). The next 40 entries
are the same values again, followed by the values for the 2nd row of tiles etc.
Also see InitializeBackdropTable.

Now if our scrollX is 4 again, we start at index 2 in the lookup table, which
gives us an offset of 16 as before. We keep going until we reach the 38th tile
column. At this point, we are at index 40 in the table. Because the table
repeats after the first 40 values, we can just keep reading and we will get
offsets 0 and 8 - which are the correct offsets to use in order to draw the two
left-most columns of backdrop tiles. The expensive modulo operation has now
become a cheaper memory read. The table also repeats in the vertical direction,
so the same applies to vertical scrolling. Accessing a specific row in the
table can be done by doing row * 80.

(Side-note: On a modern CPU, this scheme would actually be a pessimization,
since memory reads are much slower than doing arithmetic operations. In the
time it takes to fetch the lookup table from main memory, the CPU could perform
tons of modulo operations. Now there are CPU caches which alleviate this
somewhat, but even if the table would always be in the cache when we need it,
it would still take up precious cache space that might be better utilized for
other things.)

All that remains now is to calculate a base table index based on the scroll
position, and then for each backdrop tile that we want to draw, we use that
base index plus the index of the current tile column. After each row of tiles,
we increment the base index by 80 to skip to the next row in the lookup table.

Calculating the base index still requires modulo operations, but we only need to
do this once before the loop that draws all map/backdrop tiles. Within the
loop, we only need to do table lookups and additions.
*/
void DrawMapRegion(void)
{
    register word ymap;
    word dstoff = 321;  /* skip 40*8 pixel rows, then one more to skip a col */
    word ymapmax;
    word yscreen = 1;  /* skip one col; there is a blank border there */
    word *mapcell;
    word bdoff;
    word bdsrc = EGA_OFFSET_BDROP_EVEN - EGA_OFFSET_SOLID_TILES;

    if (hasHScrollBackdrop) {
        if (scrollX % 2 != 0) {
            /* Use the version of the backdrop that's shifted to the left */
            bdsrc = EGA_OFFSET_BDROP_ODD_X - EGA_OFFSET_SOLID_TILES;
        } else {
            /* Redundant assignment; uses the unmodified backdrop */
            bdsrc = EGA_OFFSET_BDROP_EVEN - EGA_OFFSET_SOLID_TILES;
        }
    }

    if (scrollY > maxScrollY) scrollY = maxScrollY;

    if (hasVScrollBackdrop && (scrollY % 2 != 0)) {
        /*
        This offset turns EGA_OFFSET_BDROP_EVEN into EGA_OFFSET_BDROP_ODD_Y, and
        EGA_OFFSET_BDROP_ODD_X into EGA_OFFSET_BDROP_ODD_XY. This makes it so
        that whenever scrollY is odd, a version of the backdrop is used that's
        shifted up by 4 pixels.
        */
        bdsrc += EGA_OFFSET_BDROP_ODD_Y - EGA_OFFSET_BDROP_EVEN;
    }

    /* Compute the half-speed start value for indexing into the backdrop table */
    bdoff =
        (hasVScrollBackdrop ? 80 * ((scrollY / 2) % BACKDROP_HEIGHT) : 0) +
        (hasHScrollBackdrop ?       (scrollX / 2) % BACKDROP_WIDTH   : 0);

    EGA_MODE_LATCHED_WRITE();

    ymapmax = (scrollY + scrollH) << mapYPower;
    ymap = scrollY << mapYPower;

    do {
        register int x = 0;

        do {
            mapcell = mapData.w + ymap + x + scrollX;

            if (*mapcell < TILE_STRIPED_PLATFORM) {
                /* "Air" tile or platform direction command; show just backdrop */
                DrawSolidTile(bdsrc + *(backdropTable + bdoff + x), x + dstoff);
            } else if (*mapcell >= TILE_MASKED_0) {
                /* Masked tile with backdrop showing through transparent areas */
                DrawSolidTile(bdsrc + *(backdropTable + bdoff + x), x + dstoff);
                DrawMaskedTile(maskedTileData + *mapcell, x + 1, yscreen);
            } else {
                /* Solid map tile */
                DrawSolidTile(*mapcell, x + dstoff);
            }

            x++;
        } while (x < SCROLLW);

        dstoff += 320;
        yscreen++;
        bdoff += 80;
        ymap += mapWidth;
    } while (ymap < ymapmax);
}

/*
Is any part of the sprite frame at x,y visible within the screen's scroll area?
*/
bool IsSpriteVisible(word sprite_type, word frame, word x_origin, word y_origin)
{
    register word width, height;
    word offset = *(actorInfoData + sprite_type) + (frame * 4);

    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    return (
        (scrollX <= x_origin && scrollX + SCROLLW > x_origin) ||
        (scrollX >= x_origin && x_origin + width > scrollX)
    ) && (
        (scrollY + scrollH > (y_origin - height) + 1 && scrollY + scrollH <= y_origin) ||
        (y_origin >= scrollY && scrollY + scrollH > y_origin)
    );
}

/*
Can the passed sprite frame move to x,y considering the direction, and how?

NOTE: `dir` does not adjust the x,y values. Therefore, the passed x,y should
always reflect the location the sprite wants to move into, and *not* the
location where it currently is.
*/
word TestSpriteMove(
    word dir, word sprite_type, word frame, word x_origin, word y_origin
) {
    word *mapcell;
    register word i;
    register word height;
    word width;
    word offset = *(actorInfoData + sprite_type) + (frame * 4);

    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);

    switch (dir) {
    case DIR4_NORTH:
        mapcell = MAP_CELL_ADDR(x_origin, (y_origin - height) + 1);

        for (i = 0; i < width; i++) {
            if (TILE_BLOCK_NORTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_SOUTH:
        mapcell = MAP_CELL_ADDR(x_origin, y_origin);

        for (i = 0; i < width; i++) {
            if (TILE_SLOPED(*(mapcell + i))) return MOVE_SLOPED;

            if (TILE_BLOCK_SOUTH(*(mapcell + i))) return MOVE_BLOCKED;
        }

        break;

    case DIR4_WEST:
        if (x_origin == 0) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x_origin, y_origin);

        for (i = 0; i < height; i++) {
            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_WEST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            if (TILE_BLOCK_WEST(*mapcell)) return MOVE_BLOCKED;

            mapcell -= mapWidth;
        }

        break;

    case DIR4_EAST:
        if (x_origin + width == mapWidth) return MOVE_BLOCKED;

        mapcell = MAP_CELL_ADDR(x_origin + width - 1, y_origin);

        for (i = 0; i < height; i++) {
            if (
                i == 0 &&
                TILE_SLOPED(*mapcell) &&
                !TILE_BLOCK_EAST(*(mapcell - mapWidth))
            ) return MOVE_SLOPED;

            if (TILE_BLOCK_EAST(*mapcell)) return MOVE_BLOCKED;

            mapcell -= mapWidth;
        }

        break;
    }

    return MOVE_FREE;
}

/*
Is the passed sprite frame at x,y (#1) touching another passed sprite (#2)?

Only used by IsNearExplosion().
*/
static bool IsIntersecting(
    word sprite1, word frame1, word x1, word y1,
    word sprite2, word frame2, word x2, word y2
) {
    register word height1;
    word width1, offset1;
    register word height2;
    word width2, offset2;

    offset1 = *(actorInfoData + sprite1) + (frame1 * 4);
    height1 = *(actorInfoData + offset1);
    width1 = *(actorInfoData + offset1 + 1);

    offset2 = *(actorInfoData + sprite2) + (frame2 * 4);
    height2 = *(actorInfoData + offset2);
    width2 = *(actorInfoData + offset2 + 1);

    if (x1 > mapWidth && x1 <= WORD_MAX) {
        /*
        This is papering over the case where something spawned an explosion
        whose origin is off the left edge of the screen. Conceptually this is a
        negative X, but our math is unsigned and it underflows around to 0xFFFF.

        By adding the sprite's width to the underflowed X, it overflows back to
        the positive side of zero, becoming the width decreased by abs(X). With
        a reduced width, X can be zeroed and still produce the right answer.

        I suspect the author had `x1 <= -1` in the test but I don't want to poke
        the "constant out of range" bear.
        */
        width1 = x1 + width1;
        x1 = 0;
    }

    return (
        (x2 <= x1 && x2 + width2 > x1) || (x2 >= x1 && x1 + width1 > x2)
    ) && (
        (y1 - height1 < y2 && y2 <= y1) || (y2 - height2 < y1 && y1 <= y2)
    );
}

/*
Draw an actor sprite frame at {x,y}_origin with the requested mode.
*/
word last_height;
word last_width;
void DrawSprite(word sprite_type, word frame, word x_origin, word y_origin, word mode)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;
    DrawFunction drawfn;

    EGA_MODE_DEFAULT();

    offset = *(actorInfoData + sprite_type) + (frame * 4);
    height = *(actorInfoData + offset);
    width = *(actorInfoData + offset + 1);
    last_height = height; last_width = width;

    src = actorTileData[*(actorInfoData + offset + 3)] + *(actorInfoData + offset + 2);

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
        drawfn = DrawSpriteTileTranslucent;
        break;
    }

    /* `mode` would go to ax if this was a switch, which doesn't happen */
    if (mode == DRAW_MODE_FLIPPED)  goto flipped;
    if (mode == DRAW_MODE_IN_FRONT) goto infront;
    if (mode == DRAW_MODE_ABSOLUTE) goto absolute;

    y = (y_origin - height) + 1;
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
            if (y == y_origin) {
                EGA_BIT_MASK_DEFAULT();
                break;
            }
            x = x_origin;
            y++;
        } else {
            x++;
        }
    }

    return;

flipped:
    y = y_origin;
    for (;;) {
        if (
            x >= scrollX && scrollX + SCROLLW > x &&
            y >= scrollY && scrollY + scrollH > y &&
            !TILE_IN_FRONT(MAP_CELL_DATA(x, y))
        ) {
            DrawSpriteTileFlipped(src, (x - scrollX) + 1, (y - scrollY) + 1);
        }

        src += 40;

        if (x == x_origin + width - 1) {
            if (y == (y_origin - height) + 1) break;
            x = x_origin;
            y--;
        } else {
            x++;
        }
    }

    return;

infront:
    y = (y_origin - height) + 1;
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

    return;

absolute:
    y = (y_origin - height) + 1;
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
}

/*
Draw a cartoon frame at x_origin,y_origin.
*/
void DrawCartoon(byte frame, word x_origin, word y_origin)
{
    word x = x_origin;
    word y;
    word height, width;
    word offset;
    byte *src;

    EGA_BIT_MASK_DEFAULT();
    EGA_MODE_DEFAULT();

    if (isCartoonDataLoaded != true) {  /* explicit compare against 1 */
        isCartoonDataLoaded = true;
        LoadCartoonData("CARTOON.MNI");
    }

    offset = *cartoonInfoData + (frame * 4);
    height = *(cartoonInfoData + offset);
    width = *(cartoonInfoData + offset + 1);

    y = (y_origin - height) + 1;
    src = mapData.b + *(cartoonInfoData + offset + 2);

    for (;;) {
        DrawSpriteTile(src, x, y);

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
Perform one frame of movement on every platform in the map.

This makes a waaay unsafe assumption about the Platform struct packing.
*/
void MovePlatforms(void)
{
    register word i;

    for (i = 0; i < numPlatforms; i++) {
        register word x;
        Platform *plat = platforms + i;
        word newdir;

        for (x = 2; x < 7; x++) {
            /* This is an ugly method of reading Platform.mapstash */
            SetMapTile(*((word *)plat + x), plat->x + x - 4, plat->y);
        }

        newdir = GetMapTile(plat->x, plat->y) / 8;

        if (playerDeadTime == 0 && plat->y - 1 == playerY && arePlatformsActive) {
            MovePlayerPlatform(plat->x - 2, plat->x + 2, newdir, newdir);
        }

        if (arePlatformsActive) {
            plat->x += dir8X[newdir];
            plat->y += dir8Y[newdir];
        }

        for (x = 2; x < 7; x++) {
            /* Again, now writing to Platform.mapstash */
            *((word *)plat + x) = GetMapTile(plat->x + x - 4, plat->y);
        }

        for (x = 2; x < 7; x++) {
            SetMapTile(TILE_BLUE_PLATFORM + ((x - 2) * 8), plat->x + x - 4, plat->y);
        }
    }
}

/*
Perform SetMapTile(), repeated `count` times horizontally.
*/
void SetMapTileRepeat(word value, word count, word x_origin, word y_origin)
{
    word x;

    for (x = 0; x < count; x++) {
        SetMapTile(value, x_origin + x, y_origin);
    }
}

/*
Perform SetMapTile() four times horizontally, with unique values.
*/
void SetMapTile4(
    word val1, word val2, word val3, word val4, word x_origin, word y_origin
) {
    SetMapTile(val1, x_origin,     y_origin);
    SetMapTile(val2, x_origin + 1, y_origin);
    SetMapTile(val3, x_origin + 2, y_origin);
    SetMapTile(val4, x_origin + 3, y_origin);
}

/*
Perform one frame of movement on every fountain in the map.
*/
void MoveFountains(void)
{
    word i;

    for (i = 0; i < numFountains; i++) {
        Fountain *fnt = fountains + i;

        if (fnt->delayleft != 0) {
            fnt->delayleft--;
            continue;
        }

        fnt->stepcount++;

        if (fnt->stepcount == fnt->stepmax) {
            fnt->stepcount = 0;
            fnt->dir = !fnt->dir;  /* flip between north and south */
            fnt->delayleft = 10;
            continue;
        }

        SetMapTile(TILE_EMPTY, fnt->x,     fnt->y);
        SetMapTile(TILE_EMPTY, fnt->x + 2, fnt->y);

        if (playerDeadTime == 0 && fnt->y - 1 == playerY) {
            if (fnt->dir != DIR4_NORTH) {
                MovePlayerPlatform(fnt->x, fnt->x + 2, DIR8_NONE, DIR8_SOUTH);
            } else {
                MovePlayerPlatform(fnt->x, fnt->x + 2, DIR8_NONE, DIR8_NORTH);
            }
        }

        if (fnt->dir != DIR4_NORTH) {
            fnt->y++;
            fnt->height--;
        } else {
            fnt->y--;
            fnt->height++;
        }

        SetMapTile(TILE_INVISIBLE_PLATFORM, fnt->x,     fnt->y);
        SetMapTile(TILE_INVISIBLE_PLATFORM, fnt->x + 2, fnt->y);
    }
}

/*
Draw all fountains, and handle contact between their streams and the player.
*/
void DrawFountains(void)
{
    static word slowcount = 0;
    static word fastcount = 0;
    word i;

    fastcount++;
    if (fastcount % 2 != 0) {
        slowcount++;
    }

    for (i = 0; i < numFountains; i++) {
        word y;
        Fountain *fnt = fountains + i;

        DrawSprite(SPR_FOUNTAIN, slowcount % 2, fnt->x, fnt->y + 1, DRAW_MODE_NORMAL);

        for (y = 0; fnt->height + 1 > y; y++) {
            DrawSprite(SPR_FOUNTAIN, (slowcount % 2) + 2, fnt->x + 1, fnt->y + y + 1, DRAW_MODE_NORMAL);

            if (IsTouchingPlayer(SPR_FOUNTAIN, 2, fnt->x + 1, fnt->y + y + 1)) {
                HurtPlayer();
            }
        }
    }
}

/*
Return the map tile value at the passed x,y position.
*/
word GetMapTile(word x, word y)
{
    return MAP_CELL_DATA(x, y);
}

/*
Lighten each area of the map that a light touches.

The map defines the top edge of each column of the cone. This function fills all
tiles south of the defined light actor until a south-blocking tile is hit.
*/
void DrawLights(void)
{
    register word i;

    if (!areLightsActive) return;

    EGA_MODE_DEFAULT();

    for (i = 0; i < numLights; i++) {
        register word y;
        word xorigin, yorigin;
        word side = lights[i].side;

        xorigin = lights[i].x;
        yorigin = lights[i].y;

        if (
            xorigin >= scrollX && scrollX + SCROLLW > xorigin &&
            yorigin >= scrollY && scrollY + scrollH - 1 >= yorigin
        ) {
            if (side == LIGHT_SIDE_WEST) {
                LightenScreenTileWest((xorigin - scrollX) + 1, (yorigin - scrollY) + 1);
            } else if (side == LIGHT_SIDE_MIDDLE) {
                LightenScreenTile((xorigin - scrollX) + 1, (yorigin - scrollY) + 1);
            } else {  /* LIGHT_SIDE_EAST */
                LightenScreenTileEast((xorigin - scrollX) + 1, (yorigin - scrollY) + 1);
            }
        }

        for (y = yorigin + 1; yorigin + LIGHT_CAST_DISTANCE > y; y++) {
            if (TILE_BLOCK_SOUTH(GetMapTile(xorigin, y))) break;

            if (
                xorigin >= scrollX && scrollX + SCROLLW > xorigin &&
                y >= scrollY && scrollY + scrollH - 1 >= y
            ) {
                LightenScreenTile((xorigin - scrollX) + 1, (y - scrollY) + 1);
            }
        }
    }
}

/*
Add sparkles to slippery areas of the map; add raindrops to empty areas of sky.
*/
void DrawRandomEffects(void)
{
    word x = scrollX + random(SCROLLW);
    word y = scrollY + random(scrollH);
    word maptile = GetMapTile(x, y);

    if (random(2U) != 0 && TILE_SLIPPERY(maptile)) {
        NewDecoration(SPR_SPARKLE_SLIPPERY, 5, x, y, DIR8_NONE, 1);
    }

    if (hasRain) {
        y = scrollY + 1;

        if (GetMapTile(x, y) == TILE_EMPTY) {
            NewDecoration(SPR_RAINDROP, 1, x, y, DIR8_SOUTHWEST, 20);
        }
    }
}

static word numShards = MAX_SHARDS;

/*
Deactivate every element in the shards array, freeing them for re-use.
*/
static void InitializeShards(void)
{
    word i;

    for (i = 0; i < numShards; i++) {
        shards[i].age = 0;
    }
}

/*
Insert the requested shard into the first free spot in the shards array.
*/
void NewShard(word sprite_type, word frame, word x_origin, word y_origin)
{
    /*
    `xmode` adds some variety to the horizontal movement of each shard:
      0: moving east
      1: moving west
      2: no horizontal movement
      3: moving east, double speed
      4: moving west, double speed

    INTERESTING: This never gets reset, so shard behavior is different for each
    run through the demo playback.
    */
    static word xmode = 0;
    word i;

    xmode++;
    if (xmode == 5) xmode = 0;

    for (i = 0; i < numShards; i++) {
        Shard *sh = shards + i;

        if (sh->age != 0) continue;

        sh->sprite = sprite_type;
        sh->x = x_origin;
        sh->y = y_origin;
        sh->frame = frame;
        sh->age = 1;
        sh->xmode = xmode;
        sh->bounced = false;

        break;
    }
}

/*
Animate one frame for each active shard, expiring old ones in the process.
*/
void MoveAndDrawShards(void)
{
    word i;

    for (i = 0; i < numShards; i++) {
        Shard *sh = shards + i;

        if (sh->age == 0) continue;

        if (sh->xmode == 0 || sh->xmode == 3) {
            if (TestSpriteMove(DIR4_EAST, sh->sprite, sh->frame, sh->x + 1, sh->y + 1) == MOVE_FREE) {
                sh->x++;

                if (sh->xmode == 3) {
                    sh->x++;
                }
            }
        } else if (sh->xmode == 1 || sh->xmode == 4) {
            if (TestSpriteMove(DIR4_WEST, sh->sprite, sh->frame, sh->x - 1, sh->y + 1) == MOVE_FREE) {
                sh->x--;

                if (sh->xmode == 4) {
                    sh->x--;
                }
            }
        }

restart:
        if (sh->age < 5) {
            sh->y -= 2;
        }

        if (sh->age == 5) {
            sh->y--;
        } else if (sh->age == 8) {
            if (TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE) {
                sh->age = 3;
                sh->y += 2;
                goto restart;
            }

            sh->y++;
        }

        if (sh->age >= 9) {
            if (sh->age > 16 && !IsSpriteVisible(sh->sprite, sh->frame, sh->x, sh->y)) {
                sh->age = 0;
                continue;
            }

            if (
                !sh->bounced &&
                TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE
            ) {
                sh->age = 3;
                sh->bounced = true;
                StartSound(SND_SHARD_BOUNCE);
                goto restart;
            }

            sh->y++;

            if (
                !sh->bounced &&
                TestSpriteMove(DIR4_SOUTH, sh->sprite, sh->frame, sh->x, sh->y + 1) != MOVE_FREE
            ) {
                sh->age = 3;
                sh->bounced = true;
                StartSound(SND_SHARD_BOUNCE);
                goto restart;
            }

            sh->y++;
        }

        if (sh->age == 1) {
            DrawSprite(sh->sprite, sh->frame, sh->x, sh->y, DRAW_MODE_WHITE);
        } else {
            DrawSprite(sh->sprite, sh->frame, sh->x, sh->y, DRAW_MODE_FLIPPED);
        }

        sh->age++;
        if (sh->age > 40) sh->age = 0;
    }
}

static word numExplosions = MAX_EXPLOSIONS;

/*
Deactivate every element in the explosions array, freeing them for re-use.
*/
static void InitializeExplosions(void)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        explosions[i].age = 0;
    }
}

/*
Insert the requested explosion into the first free spot in the explosions array.

Each explosion is 6x6 tiles. The Y position will be 2 tiles lower on screen than
the specified `y_origin`.
*/
void NewExplosion(word x_origin, word y_origin)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        Explosion *ex = explosions + i;

        if (ex->age != 0) continue;

        ex->age = 1;
        ex->x = x_origin;
        ex->y = y_origin + 2;

        StartSound(SND_EXPLOSION);

        break;
    }
}

/*
Animate one frame for each active explosion, expiring old ones in the process.
*/
void DrawExplosions(void)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        Explosion *ex = explosions + i;

        if (ex->age == 0) continue;

#ifdef EXPLOSION_PALETTE
        if (paletteAnimationNum == PAL_ANIM_EXPLOSIONS) {
            byte paletteColors[] = {
                MODE1_WHITE, MODE1_YELLOW, MODE1_WHITE, MODE1_BLACK, MODE1_YELLOW,
                MODE1_WHITE, MODE1_YELLOW, MODE1_BLACK, MODE1_BLACK
            };

            SetPaletteRegister(PALETTE_KEY_INDEX, paletteColors[ex->age - 1]);
        }
#endif  /* EXPLOSION_PALETTE */

        if (ex->age == 1) {
            NewDecoration(SPR_SPARKLE_LONG, 8, ex->x + 2, ex->y - 2, DIR8_NONE, 1);
        }

        DrawSprite(SPR_EXPLOSION, (ex->age - 1) % 4, ex->x, ex->y, DRAW_MODE_NORMAL);

        if (IsTouchingPlayer(SPR_EXPLOSION, (ex->age - 1) % 4, ex->x, ex->y)) {
            HurtPlayer();
        }

        ex->age++;
        if (ex->age == 9) {
            ex->age = 0;
            NewDecoration(SPR_SMOKE_LARGE, 6, ex->x + 1, ex->y - 1, DIR8_NORTH, 1);
        }
    }
}

/*
Return true if *any* explosion is touching the specified sprite.
*/
bool IsNearExplosion(word sprite_type, word frame, word x_origin, word y_origin)
{
    word i;

    for (i = 0; i < numExplosions; i++) {
        /* HACK: Read explosions[i].age; had to write this garbage for parity */
        if (**((word (*)[3]) explosions + i) != 0) {
            Explosion *ex = explosions + i;

            if (IsIntersecting(SPR_EXPLOSION, 0, ex->x, ex->y, sprite_type, frame, x_origin, y_origin)) {
                return true;
            }
        }
    }

    return false;
}

static word numSpawners = MAX_SPAWNERS;

/*
Deactivate every element in the spawners array, freeing them for re-use.
*/
static void InitializeSpawners(void)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        spawners[i].actor = ACT_BASKET_NULL;
    }
}

/*
Insert the requested spawner into the first free spot in the spawners array.
*/
void NewSpawner(word actor_type, word x_origin, word y_origin)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        Spawner *sp = spawners + i;

        if (sp->actor != ACT_BASKET_NULL) continue;

        sp->actor = actor_type;
        sp->x = x_origin;
        sp->y = y_origin;
        sp->age = 0;

        break;
    }
}

/*
Animate one frame for each active spawner, expiring old ones in the process.

NOTE: This function only behaves correctly for actors whose sprite type number
matches the actor type number. While spawning, frame 0 of the sprite is used.
*/
void MoveAndDrawSpawners(void)
{
    word i;

    for (i = 0; i < numSpawners; i++) {
        Spawner *sp = spawners + i;

        if (sp->actor == ACT_BASKET_NULL) continue;

        sp->age++;

        if (
            ((void)sp->y--, TestSpriteMove(DIR4_NORTH, sp->actor, 0, sp->x, sp->y)) != MOVE_FREE ||
            (
                sp->age < 9 &&
                ((void)sp->y--, TestSpriteMove(DIR4_NORTH, sp->actor, 0, sp->x, sp->y)) != MOVE_FREE
            )
        ) {
            NewActor(sp->actor, sp->x, sp->y + 1);
            DrawSprite(sp->actor, 0, sp->x, sp->y + 1, DRAW_MODE_NORMAL);
            sp->actor = ACT_BASKET_NULL;

        } else if (sp->age == 11) {
            NewActor(sp->actor, sp->x, sp->y);
            DrawSprite(sp->actor, 0, sp->x, sp->y, DRAW_MODE_FLIPPED);
            sp->actor = ACT_BASKET_NULL;

        } else {
            DrawSprite(sp->actor, 0, sp->x, sp->y, DRAW_MODE_FLIPPED);
        }
    }
}

static word numDecorations = MAX_DECORATIONS;

/*
Deactivate every element in the decorations array, freeing them for re-use.
*/
static void InitializeDecorations(void)
{
    word i;

    for (i = 0; i < numDecorations; i++) {
        decorations[i].alive = false;
    }
}

/*
Insert the given decoration into the first free spot in the decorations array.
*/
void NewDecoration(
    word sprite_type, word num_frames, word x_origin, word y_origin, word dir,
    word num_times
) {
    word i;

    for (i = 0; i < numDecorations; i++) {
        Decoration *dec = decorations + i;

        if (dec->alive) continue;

        dec->alive = true;
        dec->sprite = sprite_type;
        dec->numframes = num_frames;
        dec->x = x_origin;
        dec->y = y_origin;
        dec->dir = dir;
        dec->numtimes = num_times;

        decorationFrame[i] = 0;

        break;
    }
}

/*
Animate one frame for each active decoration, expiring old ones in the process.
*/
void MoveAndDrawDecorations(void)
{
    int i;

    for (i = 0; i < (int)numDecorations; i++) {
        Decoration *dec = decorations + i;

        if (!dec->alive) continue;

        /* Possible BUG: dec->numframes should be decorationFrame[i] instead. */
        if (IsSpriteVisible(dec->sprite, dec->numframes, dec->x, dec->y)) {
            if (dec->sprite != SPR_SPARKLE_SLIPPERY) {
                DrawSprite(dec->sprite, decorationFrame[i], dec->x, dec->y, DRAW_MODE_NORMAL);
            } else {
                DrawSprite(dec->sprite, decorationFrame[i], dec->x, dec->y, DRAW_MODE_IN_FRONT);
            }

            if (dec->sprite == SPR_RAINDROP) {
                dec->x--;
                dec->y += random(3);
            }

            dec->x += dir8X[dec->dir];
            dec->y += dir8Y[dec->dir];

            decorationFrame[i]++;
            if (decorationFrame[i] == dec->numframes) {
                decorationFrame[i] = 0;
                if (dec->numtimes != 0) {
                    dec->numtimes--;
                    if (dec->numtimes == 0) {
                        dec->alive = false;
                    }
                }
            }
        } else {
            dec->alive = false;
        }
    }
}

/*
Add six pieces of pounce debris radiating outward from x,y.
*/
void NewPounceDecoration(word x, word y)
{
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 1, y,     DIR8_SOUTHWEST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 3, y,     DIR8_SOUTHEAST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 4, y - 2, DIR8_EAST,      2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 3, y - 4, DIR8_NORTHEAST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x + 1, y - 4, DIR8_NORTHWEST, 2);
    NewDecoration(SPR_POUNCE_DEBRIS, 6, x,     y - 2, DIR8_WEST,      2);
}

/*
Can the passed sprite/frame be destroyed by an explosion?

Return true if so, and handle special cases. Otherwise returns false. As a side
effect, adds a shard decoration and adds to the player's score if the sprite is
explodable.
*/
bool CanExplode(word sprite_type, word frame, word x_origin, word y_origin)
{
    switch (sprite_type) {
    case SPR_ARROW_PISTON_W:
    case SPR_ARROW_PISTON_E:
    case SPR_SPIKES_FLOOR:
    case SPR_SPIKES_FLOOR_RECIP:
    case SPR_SAW_BLADE:
    case SPR_CABBAGE:
    case SPR_SPEAR:
    case SPR_JUMPING_BULLET:
    case SPR_STONE_HEAD_CRUSHER:
    case SPR_GHOST:
    case SPR_MOON:
    case SPR_HEART_PLANT:
    case SPR_BABY_GHOST:
    case SPR_ROAMER_SLUG:
    case SPR_BABY_GHOST_EGG:
    case SPR_SHARP_ROBOT_FLOOR:
    case SPR_SHARP_ROBOT_CEIL:
    case SPR_CLAM_PLANT:
    case SPR_PARACHUTE_BALL:
    case SPR_SPIKES_E:
    case SPR_SPIKES_E_RECIP:
    case SPR_SPIKES_W:
    case SPR_SPARK:
    case SPR_EYE_PLANT:
    case SPR_RED_JUMPER:
    case SPR_SUCTION_WALKER:
    case SPR_SPIT_WALL_PLANT_E:
    case SPR_SPIT_WALL_PLANT_W:
    case SPR_SPITTING_TURRET:
    case SPR_RED_CHOMPER:
    case SPR_PINK_WORM:
    case SPR_HINT_GLOBE:
    case SPR_PUSHER_ROBOT:
    case SPR_SENTRY_ROBOT:
    case SPR_PINK_WORM_SLIME:
    case SPR_DRAGONFLY:
    case SPR_BIRD:
    case SPR_ROCKET:
    case SPR_74:  /* probably for ACT_BABY_GHOST_EGG_PROX; never happens */
    case SPR_84:  /* " " " ACT_CLAM_PLANT_CEIL " " " */
    case SPR_96:  /* " " " ACT_EYE_PLANT_CEIL " " " */
        if (sprite_type == SPR_HINT_GLOBE) {
            NewActor(ACT_SCORE_EFFECT_12800, x_origin, y_origin);
        }

        if (
            (sprite_type == SPR_SPIKES_FLOOR_RECIP || sprite_type == SPR_SPIKES_E_RECIP) &&
            frame == 2  /* retracted */
        ) return false;

        NewShard(sprite_type, frame, x_origin, y_origin);
        AddScoreForSprite(sprite_type);

        if (sprite_type == SPR_EYE_PLANT) {
            if (numEyePlants == 1) {
                NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
            }

            NewDecoration(SPR_SPARKLE_LONG, 8, x_origin, y_origin, DIR8_NONE, 1);
            NewSpawner(ACT_BOMB_IDLE, x_origin, y_origin);

            numEyePlants--;
        }

        return true;
    }

    return false;
}

/*
Handle destruction of the passed barrel, and spawn an actor from its contents.
*/
void DestroyBarrel(word index)
{
    Actor *act = actors + index;

    act->dead = true;

    NewShard(act->data2, 0, act->x - 1, act->y);
    NewShard(act->data2, 1, act->x + 1, act->y - 1);
    NewShard(act->data2, 2, act->x + 3, act->y);
    NewShard(act->data2, 3, act->x + 2, act->y + 2);

    if (GameRand() % 2 != 0) {
        StartSound(SND_BARREL_DESTROY_1);
    } else {
        StartSound(SND_BARREL_DESTROY_2);
    }

    NewSpawner(act->data1, act->x + 1, act->y);

    if (numBarrels == 1) {
        NewActor(ACT_SPEECH_WOW_50K, playerX - 1, playerY - 5);
    }
    numBarrels--;
}

/*
Prepare the video hardware for the possibility of showing the in-game help menu.

Eventually handles keyboard/joystick input, returning a result byte that
indicates if the game should end or if a level change is needed.
*/
byte ProcessGameInputHelper(word active_page, byte demo_state)
{
    byte result;

    EGA_MODE_LATCHED_WRITE();

    SelectDrawPage(active_page);

    result = ProcessGameInput(demo_state);

    SelectDrawPage(!active_page);

    return result;
}

/*
Fill the backdrop offset lookup table.

backdropTable is a 1-dimensional array storing a 2-dimensional lookup table of
80x36 values. The values are simply counting up from 0 up to 5,752 in steps of
8, but after every 40 values, the previous 40 values are repeated. The second
half of the table is repeating the first half. In other words, within a row, the
values at indices 0 to 39 are identical to the values at indices 40 to 79.
Within the whole table, rows 0 to 17 are identical to rows 18 to 35.

To make this a bit easier to imagine, let's say the table is only 8x6. If that
were the case, it would look like this:

                           +-- repeating...
                           V
    |  0 |  8 | 16 | 24 |  0 |  8 | 16 | 24 |
    | 32 | 40 | 48 | 56 | 32 | 40 | 48 | 56 |
    | 64 | 72 | 80 | 88 | 64 | 72 | 80 | 88 |
    |  0 |  8 | 16 | 24 |  0 |  8 | 16 | 24 | <-- repeating...
    | 32 | 40 | 48 | 56 | 32 | 40 | 48 | 56 |
    | 64 | 72 | 80 | 88 | 64 | 72 | 80 | 88 |

Why the table is set up like this is explained in detail in DrawMapRegion().

*/
void InitializeBackdropTable(void)
{
    int x, y;
    word offset = 0;

    for (y = 0; y < BACKDROP_HEIGHT; y++) {
        for (x = 0; x < BACKDROP_WIDTH; x++) {
            /*
            Magic numbers:
            - 80 is double BACKDROP_WIDTH, since the table is twice as wide
              as a backdrop image.
            - 40 is basically BACKDROP_WIDTH, to skip horizontally over the
              first copy of the data in the table.
            - 1440 is double (BACKDROP_WIDTH * BACKDROP_HEIGHT), to skip
              vertically over the top two copies of the data in the table.
            - 1480 is the combination of 1440 and 40.
            - 8 is the step value required to move from one solid tile to a
              subsequent tile in the EGA memory's address space.
            */

            /* First half of the table (rows 0 to 17) */
            *(backdropTable + (y * 80) + x) =               /* 1st half of row */
            *(backdropTable + (y * 80) + x + 40) = offset;  /* 2nd half of row */

            /* Second half of the table (rows 18 to 35) */
            *(backdropTable + (y * 80) + x + 1480) =          /* 2nd half of row */
            *(backdropTable + (y * 80) + x + 1440) = offset;  /* 1st half of row */

            offset += 8;
        }
    }
}

/*
Update the programmable interval timer with the next PC speaker sound chunk.
This is also the central pacemaker for game clock ticks.
*/
void PCSpeakerService(void)
{
    static word soundCursor = 0;

    gameTickCount++;

    if (isNewSound) {
        isNewSound = false;
        soundCursor = 0;
        enableSpeaker = true;
    }

    if (*(soundDataPtr[activeSoundIndex] + soundCursor) == END_SOUND) {
        enableSpeaker = false;
        activeSoundPriority = 0;

        outportb(0x0061, inportb(0x0061) & ~0x02);
    }

    if (enableSpeaker) {
        word sample = *(soundDataPtr[activeSoundIndex] + soundCursor);

        if (sample == 0 && isSoundEnabled) {
            outportb(0x0061, inportb(0x0061) & ~0x03);
        } else if (isSoundEnabled) {
            outportb(0x0043, 0xb6);
            outportb(0x0042, (byte) (sample & 0x00ff));
            outportb(0x0042, (byte) (sample >> 8));
            outportb(0x0061, inportb(0x0061) | 0x03);
        }

        soundCursor++;
    } else {
        outportb(0x0061, inportb(0x0061) & ~0x02);
    }
}

/*
Write a page of "PC-ASCII" text and attributes to the screen, then emit a series
of newline characters to ensure the text cursor clears the bottom of what was
just written.
*/
void DrawFullscreenText(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);
    byte *dest = MK_FP(0xb800, 0);

    fread(backdropTable, 4000, 1, fp);
    movmem(backdropTable, dest, 4000);
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");  /* 22 of these */
}

/*
Ensure the system has an EGA adapter, and verify there's enough free memory. If
either are not true, exit back to DOS.

If execution got this far, one memory check already succeeded: DOS reads the
16-bit (little-endian) value at offset Ah in the EXE header, which is the number
of 16-byte paragraphs that need to be allocated in addition to the total size of
the executable's load image. The total size of both values averages around 141
KiB depending on the episode, which is already decucted from the amount reported
by coreleft() here.

The memory test in this function checks for the *additional* amount that will be
dynamically allocated during Startup(). There will be up to fifteen total calls
to malloc(), requesting a maximum total of 389,883 bytes of memory. Each
separate call for `malloc(bytes)` really subtracts `((bytes + 0x17) >> 4) << 4`
from what's reported by coreleft(), so the final amount ends up being 390,080.

NOTE: This function assumes the video mode has already been set to Dh.
*/
void ValidateSystem(void)
{
    union REGS x86regs;
    dword bytesfree;

    /* INT 10h,Fh: Get Video State - Puts current video mode number into AL. */
    x86regs.h.ah = 0x0f;
    int86(0x10, &x86regs, &x86regs);
    if (x86regs.h.al != 0x0d) {
        textmode(C80);
        printf("EGA Card not detected!\n");
        /* BUG: AdLib isn't shut down here; hoses DOSBox */
        exit(EXIT_SUCCESS);
    }

    bytesfree = coreleft();

    if (
        /* Empirically, real usage values are 383,072 and 7,008. */
        ( isAdLibPresent && bytesfree < 383792L + 7000) ||
        (!isAdLibPresent && bytesfree < 383792L)
    ) {
        StopAdLib();
        textmode(C80);
        DrawFullscreenText("NOMEMORY.mni");
        exit(EXIT_SUCCESS);
    }
}

/*
Clear the screen, then redraw the in-game status bar onto both video pages.
*/
static void ClearGameScreen(void)
{
    SelectDrawPage(0);
    DrawStaticGameScreen();

    SelectDrawPage(1);
    DrawStaticGameScreen();
}

/*
Append a relative `file` name to an absolute `dir` name, producing a complete
absolute file name. Length of the returned string is limited to 80 bytes max,
which is never a problem in DOS.
*/
char *JoinPath(char *dir, char *file)
{
    static char joinPathBuffer[80];
    int dstoff;
    word srcoff;

    if (*dir == '\0') return file;

    for (dstoff = 0; *(dir + dstoff) != '\0'; dstoff++) {
        *(joinPathBuffer + dstoff) = *(dir + dstoff);
    }

    *(joinPathBuffer + dstoff++) = '\\';

    for (srcoff = 0; *(file + srcoff) != '\0'; srcoff++) {
        *(joinPathBuffer + dstoff++) = *(file + srcoff);
    }

    /* BUG: Output is not properly null-terminated. */

    return joinPathBuffer;
}

/*
Load game state from a save file. The slot number is a single character, '1'
through '9', or the letter 'T' for the temporary save file. Returns true if the
load was successful, or false if the specified save file didn't exist. Also
capable of exiting the game if a manipulated save file is loaded.
*/
bbool LoadGameState(char slot_char)
{
    static char *filename = FILENAME_BASE ".SV ";
    FILE *fp;
    int checksum;

    *(filename + SAVE_SLOT_INDEX) = slot_char;

    fp = fopen(JoinPath(writePath, filename), "rb");
    if (fp == NULL) {
        fclose(fp);  /* Nice. */

        return false;
    }

    playerHealth = getw(fp);
    fread(&gameScore, 4, 1, fp);
    gameStars = getw(fp);
    levelNum = getw(fp);
    playerBombs = getw(fp);
    playerHealthCells = getw(fp);
    usedCheatCode = getw(fp);
    sawBombHint = getw(fp);
    pounceHintState = getw(fp);
    sawHealthHint = getw(fp);

    checksum = playerHealth + (word)gameStars + levelNum + playerBombs + playerHealthCells;

    if (getw(fp) != checksum) {
        ShowAlteredFileError();
        ExitClean();
    }

    fclose(fp);

    return true;
}

/*
Save the current game state to a save file. The slot number is a single
character, '1' through '9', or the letter 'T' for the temporary save file.
*/
static void SaveGameState(char slot_char)
{
    static char *filename = FILENAME_BASE ".SV ";
    FILE *fp;
    word checksum;

    *(filename + SAVE_SLOT_INDEX) = slot_char;

    fp = fopen(JoinPath(writePath, filename), "wb");

    putw(playerHealth, fp);
    fwrite(&gameScore, 4, 1, fp);
    putw((word)gameStars, fp);
    putw(levelNum, fp);
    putw(playerBombs, fp);
    putw(playerHealthCells, fp);
    putw(usedCheatCode, fp);
    putw(true, fp);  /* bomb hint */
    putw(POUNCE_HINT_SEEN, fp);
    putw(true, fp);  /* health hint */
    checksum = playerHealth + (word)gameStars + levelNum + playerBombs + playerHealthCells;
    putw(checksum, fp);

    fclose(fp);
}

/*
Present a UI for restoring a saved game, and return the result of the prompt.
*/
byte PromptRestoreGame(void)
{
    byte scancode;
    word x = UnfoldTextFrame(11, 7, 28, "Restore a game.", "Press ESC to quit.");

    DrawTextLine(x, 14, " What game number (1-9)?");
    scancode = WaitSpinner(x + 24, 14);

    if (scancode == SCANCODE_ESC || scancode == SCANCODE_SPACE || scancode == SCANCODE_ENTER) {
        return RESTORE_GAME_ABORT;
    }

    if (scancode >= SCANCODE_1 && scancode < SCANCODE_0) {
        DrawScancodeCharacter(x + 24, 14, scancode);

        if (!LoadGameState('1' + (scancode - SCANCODE_1))) {
            return RESTORE_GAME_NOT_FOUND;
        } else {
            return RESTORE_GAME_SUCCESS;
        }
    } else {
        x = UnfoldTextFrame(11, 4, 28, "Invalid game number!", "Press ANY key.");
        WaitSpinner(x + 25, 13);
    }

    return RESTORE_GAME_ABORT;
}

/*
Present a UI for saving the game. As the prompt says, the save file will be
written with the state of the game when the level was last started.
*/
static void PromptSaveGame(void)
{
    byte scancode;
    word tmphealth, tmpbombs, tmplevel, tmpstars, tmpbars;
    dword tmpscore;
    word x = UnfoldTextFrame(8, 10, 28, "Save a game.", "Press ESC to quit.");

    DrawTextLine(x, 11, " What game number (1-9)?");
    DrawTextLine(x, 13, " NOTE: Game is saved at");
    DrawTextLine(x, 14, " BEGINNING of level.");
    scancode = WaitSpinner(x + 24, 11);

    if (scancode == SCANCODE_ESC || scancode == SCANCODE_SPACE || scancode == SCANCODE_ENTER) {
        return;
    }

    if (scancode >= SCANCODE_1 && scancode < SCANCODE_0) {
        DrawScancodeCharacter(x + 24, 11, scancode);

        tmphealth = playerHealth;
        tmpbombs = playerBombs;
        tmpstars = (word)gameStars;
        tmplevel = levelNum;
        tmpbars = playerHealthCells;
        tmpscore = gameScore;

        LoadGameState('T');
        SaveGameState('1' + (scancode - SCANCODE_1));

        playerHealth = tmphealth;
        playerBombs = tmpbombs;
        gameStars = tmpstars;
        levelNum = tmplevel;
        gameScore = tmpscore;
        playerHealthCells = tmpbars;

        x = UnfoldTextFrame(7, 4, 20, "Game Saved.", "Press ANY key.");
        WaitSpinner(x + 17, 9);
    } else {
        x = UnfoldTextFrame(11, 4, 28, "Invalid game number!", "Press ANY key.");
        WaitSpinner(x + 25, 13);
    }
}

/*
Present a UI for the "warp mode" debug feature. This abandons any progress made
in the current level, and jumps unconditionally to the new level. Returns true
if the request was acceptable, or false if the input was bad or Esc was pressed.
*/
bbool PromptLevelWarp(void)
{
#ifdef HAS_MAP_11
#   define MAX_MAP "13"
    word levels[] = {0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 20, 2, 3};
#else
#   define MAX_MAP "12"
    word levels[] = {0, 1, 4, 5, 8, 9, 12, 13, 16, 17, 2, 3};
#endif  /* HAS_MAP_11 */
    char buffer[3];
    int x = UnfoldTextFrame(2, 4, 28, "Warp Mode!", "Enter level (1-" MAX_MAP "):");
#undef MAX_MAP

    ReadAndEchoText(x + 21, 4, buffer, 2);

    /* Repurposing x here to hold the level index */
    x = atoi(buffer) - 1;

    if (x >= 0 && x <= (int)(sizeof levels / sizeof levels[0]) - 1) {
        levelNum = x;  /* no effect, next two calls both clobber this */
        LoadGameState('T');
        InitializeLevel(levels[x]);

        return true;
    }

    return false;
}

/*
Display the slimmed-down in-game help menu. Returns a result byte that indicates
if the game should continue, restart on a different level, or exit.
*/
byte ShowHelpMenu(void)
{
    word x = UnfoldTextFrame(2, 12, 22, "HELP MENU", "Press ESC to quit.");
    DrawTextLine(x, 5,  " S)ave your game");
    DrawTextLine(x, 6,  " R)estore a game");
    DrawTextLine(x, 7,  " H)elp");
    DrawTextLine(x, 8,  " G)ame redefine");
    DrawTextLine(x, 9,  " V)iew High Scores");
    DrawTextLine(x, 10, " Q)uit Game");

    for (;;) {
        byte scancode = WaitSpinner(29, 12);

        switch (scancode) {
        case SCANCODE_G:
            ShowGameRedefineMenu();
            return HELP_MENU_CONTINUE;
        case SCANCODE_S:
            PromptSaveGame();
            return HELP_MENU_CONTINUE;
        case SCANCODE_R:
            {  /* for scope */
                byte result = PromptRestoreGame();
                if (result == RESTORE_GAME_SUCCESS) {
                    InitializeLevel(levelNum);
                    return HELP_MENU_RESTART;
                } else if (result == RESTORE_GAME_NOT_FOUND) {
                    ShowRestoreGameError();
                }
            }
            return HELP_MENU_CONTINUE;
        case SCANCODE_V:
            ShowHighScoreTable();
            return HELP_MENU_CONTINUE;
        case SCANCODE_Q:
            return HELP_MENU_QUIT;
        case SCANCODE_H:
            ShowHintsAndKeys(1);
            return HELP_MENU_CONTINUE;
        case SCANCODE_ESC:
            return HELP_MENU_CONTINUE;
        }
    }
}

/*
Read the next byte of demo data into the global command variables. Return true
if the end of the demo data has been reached, otherwise return false.
*/
bbool ReadDemoFrame(void)
{
    cmdWest  = (bbool)(*(miscData + demoDataPos) & 0x01);
    cmdEast  = (bbool)(*(miscData + demoDataPos) & 0x02);
    cmdNorth = (bbool)(*(miscData + demoDataPos) & 0x04);
    cmdSouth = (bbool)(*(miscData + demoDataPos) & 0x08);
    cmdJump  = (bbool)(*(miscData + demoDataPos) & 0x10);
    cmdBomb  = (bbool)(*(miscData + demoDataPos) & 0x20);
    winLevel =  (bool)(*(miscData + demoDataPos) & 0x40);

    demoDataPos++;

    if (demoDataPos > demoDataLength) return true;

    return false;
}

/*
Pack the current state of all the global command variables into a byte, then
append that byte to the demo data. Return true if the demo data storage is full,
otherwise return false.
*/
bbool WriteDemoFrame(void)
{
    if (demoDataLength > 4998) return true;

    /*
    This function runs early enough in the game loop that this assignment
    doesn't change the behavior of any player/actor touch.
    */
    winLevel = isKeyDown[SCANCODE_X];

    *(miscData + demoDataPos) = cmdWest | (cmdEast  << 1) | (cmdNorth << 2) |
        (cmdSouth << 3) | (cmdJump  << 4) | (cmdBomb  << 5) | (winLevel << 6);

    demoDataPos++;
    demoDataLength++;

    return false;
}

/*
Draw the "Super Star Bonus" screen, deduct from the star count, and add to the
player's score.
*/
void ShowStarBonus(void)
{
    register word stars;
    word i = 0;

    StopMusic();

    if (gameStars == 0) {
        /* Fade-out is not necessary; every caller fades after this returns. */
        FadeOut();
        return;
    }

    FadeWhiteCustom(3);
    SelectDrawPage(0);
    SelectActivePage(0);
    ClearScreen();

    UnfoldTextFrame(2, 14, 30, "Super Star Bonus!!!!", "");
    DrawSprite(SPR_STAR, 2, 8, 8, DRAW_MODE_ABSOLUTE);
    DrawTextLine(14, 7, "X 1000 =");
    DrawNumberFlushRight(27, 7, gameStars * 1000);
    WaitHard(50);  /* nothing is visible during this delay! */
    DrawTextLine(10, 12, "YOUR SCORE =  ");
    DrawNumberFlushRight(29, 12, gameScore);
    FadeIn();
    WaitHard(100);

    for (stars = (word)gameStars; stars > 0; stars--) {
        register word x;

        gameScore += 1000;

        WaitHard(15);

        for (x = 0; x < 7; x++) {
            /* Clear score area -- not strictly needed, numbers grow and are opaque */
            DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, 23 + x, 12);
        }

        StartSound(SND_BIG_PRIZE);
        DrawNumberFlushRight(29, 12, gameScore);

        /* Increment i for each star collected, stopping at 78 */
        if (i / 6 < 13) i++;

        for (x = 0; x < 16; x++) {
            if (x < 7) {
                /* Clear x1000 area -- needed as the number of digits shrinks */
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, 22 + x, 7);
            }

            if (i % 8 == 1) {
                /* Clear rank area -- needed because letter chars have transparency */
                DrawSpriteTile(fontTileData + FONT_BACKGROUND_GRAY, 13 + x, 14);
            }
        }

        DrawNumberFlushRight(27, 7, (stars - 1) * 1000L);

        /*
        BUG: Due to differences in division (6 vs. 8), "Radical!", "Incredible",
        and "Towering" never display in the game.

        NOTE: If the player collected 78 or more stars, `i / 6` indexes past the
        end of `starBonusRanks[]`. This doesn't cause trouble because `i` is
        capped at 78, and 78 % 8 is never 1.
        */
        if (i % 8 == 1) {
            DrawTextLine(13, 14, starBonusRanks[i / 6]);
        }
    }

    WaitHard(400);

    gameStars = 0;
}

/*
Show the intermission screen that bookends every bonus stage.
*/
static void ShowSectionIntermission(char *top_text, char *bottom_text)
{
    word x;

    FadeOut();
    SelectDrawPage(0);
    SelectActivePage(0);
    ClearScreen();

    x = UnfoldTextFrame(6, 4, 30, top_text, bottom_text);
    FadeIn();
    WaitSpinner(x + 27, 8);

    ShowStarBonus();

    FadeOut();
    ClearScreen();
}

/*
Handle progression to the next level.
*/
void NextLevel(void)
{
    word stars = (word)gameStars;

    if (demoState != DEMO_STATE_NONE) {
        switch (levelNum) {
        case 0:
            levelNum = 13;
            break;
        case 13:
            levelNum = 5;
            break;
        case 5:
            levelNum = 9;
            break;
        case 9:
            levelNum = 16;
            break;
        }

        return;
    }

    switch (levelNum) {
    case 2:  /* Lesser bonus levels... */
    case 6:
    case 10:
    case 14:
    case 18:
    case 22:
    case 26:
        levelNum++;
        /* FALL THROUGH */

    case 3:  /* Better bonus levels... */
    case 7:
    case 11:
    case 15:
    case 19:
    case 23:
    case 27:
        ShowSectionIntermission("Bonus Level Completed!!", "Press ANY key.");
        /* FALL THROUGH */

    case 0:  /* First levels in each section... */
    case 4:
    case 8:
    case 12:
    case 16:
    case 20:
    case 24:
        levelNum++;
        break;

    case 1:  /* Second levels in each section... */
    case 5:
    case 9:
    case 13:
    case 17:
    case 21:
    case 25:
        ShowSectionIntermission("Section Completed!", "Press ANY key.");

        if (stars > 24) {
            /* Fade/clear not strictly needed */
            FadeOutCustom(0);
            ClearScreen();
            DrawFullscreenImage(IMAGE_BONUS);
            StartSound(SND_BONUS_STAGE);
            if (stars > 49) {
                levelNum++;
            }
            levelNum++;
            WaitHard(150);
        } else {
            levelNum += 3;
        }

        break;
    }
}

/*
Insert either a regular actor or a special actor into the world. The map format
reserves types 0..31 for special actors, and map types 31+ become regular actors
starting from type 0. (Map actor type 31 has overlapping branch coverage, but as
a special actor it is a no-op due to no matching switch case.)

The caller must provide the index for regular actors, which is convoluted.
*/
void NewMapActorAtIndex(
    word index, word map_actor_type, word x_origin, word y_origin
) {
    if (map_actor_type < 32) {
        switch (map_actor_type) {
        case SPA_PLAYER_START:
            if (x_origin > mapWidth - 15) {
                scrollX = mapWidth - SCROLLW;
            } else if ((int)x_origin - 15 >= 0 && mapYPower > 5) {
                scrollX = x_origin - 15;
            } else {
                scrollX = 0;
            }

            if ((int)y_origin - 10 >= 0) {
                scrollY = y_origin - 10;
            } else {
                scrollY = 0;
            }

            playerX = x_origin;
            playerY = y_origin;

            break;

        case SPA_PLATFORM:
            platforms[numPlatforms].x = x_origin;
            platforms[numPlatforms].y = y_origin;
            numPlatforms++;

            break;

        case SPA_FOUNTAIN_SMALL:
        case SPA_FOUNTAIN_MEDIUM:
        case SPA_FOUNTAIN_LARGE:
        case SPA_FOUNTAIN_HUGE:
            fountains[numFountains].x = x_origin - 1;
            fountains[numFountains].y = y_origin - 1;
            fountains[numFountains].dir = DIR4_NORTH;
            fountains[numFountains].stepcount = 0;
            fountains[numFountains].height = 0;
            fountains[numFountains].stepmax = map_actor_type * 3;
            fountains[numFountains].delayleft = 0;
            numFountains++;

            break;

        case SPA_LIGHT_WEST:
        case SPA_LIGHT_MIDDLE:
        case SPA_LIGHT_EAST:
            if (numLights == MAX_LIGHTS - 1) break;

            /* Normalize SPA_LIGHT_* into LIGHT_SIDE_* */
            lights[numLights].side = map_actor_type - SPA_LIGHT_WEST;
            lights[numLights].x = x_origin;
            lights[numLights].y = y_origin;
            numLights++;

            break;
        }
    }

    if (map_actor_type < 31) return;

    if (NewActorAtIndex(index, map_actor_type - 31, x_origin, y_origin)) {
        numActors++;
    }
}

/*
Track the backdrop parameters (backdrop number and horizontal/vertical scroll
flags) and return true if they have changed since the last call. Otherwise
return false.
*/
static bbool IsNewBackdrop(word backdrop_num)
{
    static word lastnum = WORD_MAX;
    static word lasth = WORD_MAX;
    static word lastv = WORD_MAX;

    if (backdrop_num != lastnum || hasHScrollBackdrop != lasth || hasVScrollBackdrop != lastv) {
        lastnum = backdrop_num;
        lasth = hasHScrollBackdrop;
        lastv = hasVScrollBackdrop;

        return true;
    }

    return false;
}

/*
Reset all of the global variables to prepare for (re)entry into a map. These are
a mix of player movement variables, actor interactivity flags, and map state.
*/
static void InitializeMapGlobals(void)
{
    winGame = false;
    playerClingDir = DIR4_NONE;
    isPlayerFalling = true;
    cmdJumpLatch = true;
    playerJumpTime = 0;
    playerFallTime = 1;
    isPlayerRecoiling = false;
    playerRecoilLeft = 0;
    playerFaceDir = DIR4_EAST;
    playerFrame = PLAYER_WALK_1;
    playerBaseFrame = PLAYER_BASE_EAST;
    playerDeadTime = 0;
    winLevel = false;
    playerHurtCooldown = 40;
    transporterTimeLeft = 0;
    activeTransporter = 0;
    isPlayerInPipe = false;
    scooterMounted = 0;
    isPlayerNearTransporter = false;
    isPlayerNearHintGlobe = false;
    areForceFieldsActive = true;
    blockMovementCmds = false;

    ClearPlayerDizzy();

    blockActionCmds = false;
    arePlatformsActive = true;
    isPlayerInvincible = false;
    paletteStepCount = 0;
    randStepCount = 0;
    playerFallDeadTime = 0;
    sawHurtBubble = false;
    sawAutoHintGlobe = false;
    numBarrels = 0;
    numEyePlants = 0;
    pounceStreak = 0;

    sawJumpPadBubble =
        sawMonumentBubble =
        sawScooterBubble =
        sawTransporterBubble =
        sawPipeBubble =
        sawBossBubble =
        sawPusherRobotBubble =
        sawBearTrapBubble =
        sawMysteryWallBubble =
        sawTulipLauncherBubble =
        sawHamburgerBubble = false;
}

/*
Switch to a new level (or reload the current one) and perform all related
initialization tasks.
*/
bbool mask_init = false;
void InitializeLevel(word level_num)
{
    static word mapVariables, musicNum;
    FILE *fp;
    word bdnum;

    if (level_num == 0 && isNewGame) {
        DrawFullscreenImage(IMAGE_ONE_MOMENT);
        /* All my childhood, I wondered what it was doing here. It's bupkis. */
        WaitSoft(300);
    } else {
        FadeOut();
    }
    
    fp = GroupEntryFp(mapNames[level_num]);
    mapVariables = getw(fp);
    fclose(fp);

    StopMusic();

    hasRain = (bool)(mapVariables & 0x0020);
    bdnum = mapVariables & 0x001f;
    lvlBdnum = bdnum;
    hasHScrollBackdrop = (bool)(mapVariables & 0x0040);
    hasVScrollBackdrop = (bool)(mapVariables & 0x0080);
    paletteAnimationNum = (byte)(mapVariables >> 8) & 0x07;
    musicNum = (mapVariables >> 11) & 0x001f;
    lvlMusicNum = musicNum;

    InitializeMapGlobals();

    if (IsNewBackdrop(bdnum)) {
        LoadBackdropData(backdropNames[bdnum], mapData.b);
    }

    LoadMapData(level_num);

    if (level_num == 0 && isNewGame) {
        FadeOut();
        isNewGame = false;
    }

    if (demoState == DEMO_STATE_NONE && !level_edit) {
        switch (level_num) {
        case 0:
        case 1:
        case 4:
        case 5:
        case 8:
        case 9:
        case 12:
        case 13:
        case 16:
        case 17:
            SelectDrawPage(0);
            SelectActivePage(0);
            ClearScreen();
            FadeIn();
            ShowLevelIntro(level_num);
            WaitSoft(150);
            FadeOut();
            break;
        }
    }
    
    InitializeShards();
    InitializeExplosions();
    InitializeDecorations();
    ClearPlayerPush();
    InitializeSpawners();

    if (!level_edit) ClearGameScreen();
    SelectDrawPage(activePage);
    activePage = !activePage;
    SelectActivePage(activePage);

    if (!level_edit) SaveGameState('T');
    StartGameMusic(musicNum);
    
    if (!isAdLibPresent) {
        tileAttributeData = miscData + 5000;
        miscDataContents = IMAGE_TILEATTR;
        LoadTileAttributeData("TILEATTR.MNI");
    }

    if (!mask_init) {
        LoadMaskedTileData("MASKTILE.MNI");
        mask_init = true;
    }

    if (!level_edit) FadeIn();

#ifdef EXPLOSION_PALETTE
    if (paletteAnimationNum == PAL_ANIM_EXPLOSIONS) {
        SetPaletteRegister(PALETTE_KEY_INDEX, MODE1_BLACK);
    }
#endif  /* EXPLOSION_PALETTE */
}

/*
Set all variables that pertain to the state of the overarching game. These are
set once at the beginning of each episode and retain their values across levels.
*/
void InitializeEpisode(void)
{
    gameScore = 0;
    playerHealth = 4;
    playerHealthCells = 3;
    levelNum = 0;
    playerBombs = 0;
    gameStars = 0;
    demoDataPos = 0;
    demoDataLength = 0;
    usedCheatCode = false;
    sawBombHint = false;
    sawHealthHint = false;
}