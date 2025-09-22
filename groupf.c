#include "glue.h"

/*
Get the file size of the named group entry, in bytes.
*/
static dword GroupEntryLength(char *entry_name)
{
    fclose(GroupEntryFp(entry_name));

    return lastGroupEntryLength;
}

/*
Load font data into system memory.

The font data on disk has an inverted transparency mask relative to what the
rest of the game expects, so negate those bits while loading.
*/
static void LoadFontTileData(char *entry_name, byte *dest, word length)
{
    int i;
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);

    /* Ideally should be `length`, not a literal 4000. */
    for (i = 0; i < 4000; i += 5) {
        *(dest + i) = ~*(dest + i);
    }
}

/*
Load sound data into system memory.

There are three group entries with sound data. `skip` allows them to be arranged
into a single linear pointer array.
*/
static void LoadSoundData(char *entry_name, word *dest, int skip)
{
    int i;
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, (word)GroupEntryLength(entry_name), 1, fp);
    fclose(fp);

    for (i = 0; i < 23; i++) {
        soundDataPtr[i + skip] = dest + (*(dest + (i * 8) + 8) >> 1);
        soundPriority[i + skip + 1] = (byte)*(dest + (i * 8) + 9);
    }
}

/*
Read a group entry into system memory.
*/
static void LoadGroupEntryData(char *entry_name, byte *dest, word length)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);
}

/*
Load actor tile data into system memory.
*/
static void LoadActorTileData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(actorTileData[0], WORD_MAX, 1, fp);
    fread(actorTileData[1], WORD_MAX, 1, fp);
    /* CAREFUL: Wraparound. Also, "ACTORS.MNI" should ideally use entry_name. */
    fread(actorTileData[2], (word)GroupEntryLength("ACTORS.MNI") + 2, 1, fp);
    fclose(fp);
}

/*
Read a group entry containing "info" data into system memory.

Used when loading {ACTR,CART,PLYR}INFO.MNI entries.

NOTE: This is identical to the more general-purpose LoadGroupEntryData(), except
that here `dest` is a word pointer.
*/
static void LoadInfoData(char *entry_name, word *dest, word length)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(dest, length, 1, fp);
    fclose(fp);
}

/*
Load cartoon data into system memory.
*/
void LoadCartoonData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(mapData.b, (word)GroupEntryLength(entry_name), 1, fp);
    fclose(fp);
}

/*
Read the tile attribute data into the designated memory location.
*/
void LoadTileAttributeData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(tileAttributeData, 7000, 1, fp);
    fclose(fp);
}

/*
Read the masked tile data into the designated memory location.
*/
void LoadMaskedTileData(char *entry_name)
{
    FILE *fp = GroupEntryFp(entry_name);

    fread(maskedTileData, 40000U, 1, fp);
    fclose(fp);
}

/*
Return a file pointer matching the passed group entry name and update
lastGroupEntryLength with the size of the entry's data. This function tries, in
order: entry inside the STN file, entry inside the VOL file, file in the current
working directory with a name matching the passed entry name. If nothing is
found, assuming the program doesn't crash due to performing a bunch of
operations on a null pointer, return NULL.

NOTE: This uses a 20-byte buffer and an 11-byte comparison on a file format
whose entry names are each 12 bytes long. Fun ensues.

NOTE: The STN/VOL format is not formally specified anywhere, so it's not clear
if the offsets/lengths read from it are supposed to be interpreted as signed or
unsigned. This function uses a bit of a mismash of the two.
*/
FILE *GroupEntryFp(char *entry_name)
{
    char header[1000];
    char name[20];
    FILE *fp;
    dword offset;
    int i;
    bool found;

    /* Make an uppercased copy of the entry name as recklessly as possible */
    for (i = 0; i < 19; i++) {
        name[i] = *(entry_name + i);
    }
    name[19] = '\0';
    strupr(name);

    /*
    Attempt to read from the STN file
    */
    fp = fopen(stnGroupFilename, "rb");
    found = false;
    fread(header, 1, 960, fp);

    /* Final iteration, if reached, will cause a read out of bounds */
    for (i = 0; i < 980; i += 20) {
        if (header[i] == '\0') break;  /* no more entries */

        if (strncmp(header + i, name, 11) == 0) {
            offset = i + 12;
            found = true;
        }
    }

    if (!found) {
        fclose(fp);

        /*
        Attempt to read from the VOL file
        */
        fp = fopen(volGroupFilename, "rb");
        fread(header, 1, 960, fp);

        /* Final iteration, if reached, will cause a read out of bounds */
        for (i = 0; i < 980; i += 20) {
            if (header[i] == '\0') break;  /* no more entries */

            if (strncmp(header + i, name, 11) == 0) {
                offset = i + 12;
                found = true;
            }
        }

        if (!found) {
            fclose(fp);

            /*
            Attempt to read from the current working directory
            */
            fp = fopen(entry_name, "rb");
            i = fileno(fp);
            lastGroupEntryLength = filelength(i);

            return fp;
        }
    }

    /* Here `offset` points to the entry's header data */
    fseek(fp, offset, SEEK_SET);
    fread(&offset, 4, 1, fp);
    fread(&lastGroupEntryLength, 4, 1, fp);

    /* Now `offset` points to the first byte of the entry's data */
    fseek(fp, offset, SEEK_SET);

    return fp;
}

/*
Read music data from the group entry referred to by the music number, and store
it into the passed Music pointer.
*/
Music *LoadMusicData(word music_num, Music *dest)
{
    FILE *fp;
    Music *localdest = dest;  /* not clear why this copy is needed */

    miscDataContents = IMAGE_NONE;

    fp = GroupEntryFp(musicNames[music_num]);
    fread(&dest->datahead, 1, (word)lastGroupEntryLength + 2, fp);
    localdest->length = (word)lastGroupEntryLength;

    SetMusicState(true);

    fclose(fp);

    return localdest;
}

/*
Flush the recorded demo data to disk.
*/
void SaveDemoData(void)
{
    FILE *fp = fopen("PREVDEMO.MNI", "wb");
    miscDataContents = IMAGE_DEMO;

    putw(demoDataLength, fp);
    fwrite(miscData, demoDataLength, 1, fp);

    fclose(fp);
}

/*
Read demo data into memory.
*/
void LoadDemoData(void)
{
    FILE *fp = GroupEntryFp("PREVDEMO.MNI");
    miscDataContents = IMAGE_DEMO;

    if (fp == NULL) {
        /* These were already set in InitializeEpisode() */
        demoDataLength = 0;
        demoDataPos = 0;
    } else {
        demoDataLength = getw(fp);
        fread(miscData, demoDataLength, 1, fp);
    }

    fclose(fp);
}

/*
Load data from a map file, initialize global state, and build all actors.

This makes a waaay unsafe assumption about the Platform struct packing.
*/
void LoadMapData(word level_num)
{
    word i;
    word actorwords;
    word t;  /* holds a map actor's *T*ype OR a *T*ile's horizontal position */
    FILE *fp = GroupEntryFp(mapNames[level_num]);

    isCartoonDataLoaded = false;

    getw(fp);  /* skip over map flags */
    mapWidth = getw(fp);

    switch (mapWidth) {
    case 1 << 5:
        mapYPower = 5;
        break;
    case 1 << 6:
        mapYPower = 6;
        break;
    case 1 << 7:
        mapYPower = 7;
        break;
    case 1 << 8:
        mapYPower = 8;
        break;
    case 1 << 9:
        mapYPower = 9;
        break;
    case 1 << 10:
        mapYPower = 10;
        break;
    case 1 << 11:
        mapYPower = 11;
        break;
    }

    actorwords = getw(fp);  /* total size of actor data, in words */
    numActors = 0;
    numPlatforms = 0;
    numFountains = 0;
    numLights = 0;
    areLightsActive = true;
    hasLightSwitch = false;

    fread(mapData.w, actorwords, 2, fp);

    for (i = 0; i < actorwords; i += 3) {  /* each actor is 3 words long */
        register word x, y;

        t = *(mapData.w + i);
        x = *(mapData.w + i + 1);
        y = *(mapData.w + i + 2);
        NewMapActorAtIndex(numActors, t, x, y);

        if (numActors > MAX_ACTORS - 1) break;
    }

    fread(mapData.b, WORD_MAX, 1, fp);
    fclose(fp);

    for (i = 0; i < numPlatforms; i++) {
        for (t = 2; t < 7; t++) {
            /* This is an ugly method of accessing Platform.mapstash */
            *((word *)(platforms + i) + t) = MAP_CELL_DATA_SHIFTED(
                platforms[i].x, platforms[i].y, t - 4
            );
        }
    }

    levelNum = level_num;
    maxScrollY = (word)(0x10000L / (mapWidth * 2)) - (scrollH + 1);
}

/*
Load the specified backdrop image data into the video memory. Requires a scratch
buffer twice the size of BACKDROP_SIZE *plus* 640 bytes to perform this work.

Depending on the current backdrop scroll settings, up to four versions of the
backdrop are copied into EGA video memory. This is due to how the parallax
scrolling works, which is explained in detail in DrawMapRegion().
*/
void LoadBackdropData(char *entry_name, byte *scratch)
{
    FILE *fp = GroupEntryFp(entry_name);

    EGA_MODE_DEFAULT();
    EGA_BIT_MASK_DEFAULT();

    miscDataContents = IMAGE_NONE;
    fread(scratch, BACKDROP_SIZE, 1, fp);

    /*
    Copy the unmodified backdrop. This is all we need if there is no backdrop
    scrolling.
    */
    CopyTilesToEGA(scratch, BACKDROP_SIZE_EGA_MEM, EGA_OFFSET_BDROP_EVEN);

    if (hasHScrollBackdrop) {
        /*
        To do horizontal backdrop scrolling, we need a copy of the backdrop
        shifted left by 4 pixels.
        */
        WrapBackdropHorizontal(scratch, scratch + BACKDROP_SIZE);
        CopyTilesToEGA(scratch + BACKDROP_SIZE, BACKDROP_SIZE_EGA_MEM, EGA_OFFSET_BDROP_ODD_X);
    }

    if (hasVScrollBackdrop) {
        /*
        To do vertical backdrop scrolling, we need a copy of the backdrop
        shifted up by 4 pixels.
        */
        WrapBackdropVertical(scratch, miscData + 5000, scratch + (2 * BACKDROP_SIZE));
        CopyTilesToEGA(miscData + 5000, BACKDROP_SIZE_EGA_MEM, EGA_OFFSET_BDROP_ODD_Y);

        /*
        If horizontal scrolling is also enabled, we additionally need one that's
        shifted both left and up by 4. Here, the assumption is that scratch +
        BACKDROP_SIZE holds a copy of the backdrop that's already shifted to the
        left, which will be the case if hasHScrollBackdrop is also true, but not
        otherwise. This is also why the code just above uses miscData + 5000 to
        store the shifted copy of the backdrop, instead of using scratch +
        BACKDROP_SIZE as the horizontal version of this code does.

        If horizontal scrolling is not enabled, the game still does this, but it
        will end up operating on random garbage data. However, that doesn't
        really cause any issues because EGA_OFFSET_BDROP_ODD_XY is never used if
        horizontal scrolling is not enabled.
        */
        WrapBackdropVertical(scratch + BACKDROP_SIZE, miscData + 5000, scratch + (2 * BACKDROP_SIZE));
        CopyTilesToEGA(miscData + 5000, BACKDROP_SIZE_EGA_MEM, EGA_OFFSET_BDROP_ODD_XY);
    }

    fclose(fp);
}

/*
Start with a bang. Set video mode, initialize the AdLib, install the keyboard
service, initialize the PC speaker state, allocate enough memory, then show the
pre-title image.

While the pre-title image is up, load the config file, then allocate and
generate/load a whole slew of data to each arena. This takes a noticable amount
of time on a slower machine. At the end, move onto displaying the copyright
screen.

Nothing allocated here is ever explicitly freed. DOS gets it all back when the
program eventually exits.
*/
void Startup(void)
{
    /*
    Mode Dh is EGA/VGA 40x25 characters with an 8x8 pixel box.
    320x200 graphics resolution, 16 color, 8 pages, 0xA000 screen segment
    */
    SetVideoMode(0x0d);

    StartAdLib();

    ValidateSystem();

    totalMemFreeBefore = coreleft();

    disable();

    savedInt9 = getvect(9);
    setvect(9, KeyboardInterruptService);

    enableSpeaker = false;
    activeSoundPriority = 0;
    gameTickCount = 0;
    isSoundEnabled = true;

    enable();

    miscData = malloc(35000U);

    soundData1 = malloc((word)GroupEntryLength("SOUNDS.MNI"));
    soundData2 = malloc((word)GroupEntryLength("SOUNDS2.MNI"));
    soundData3 = malloc((word)GroupEntryLength("SOUNDS3.MNI"));

    LoadSoundData("SOUNDS.MNI",  soundData1, 0);
    LoadSoundData("SOUNDS2.MNI", soundData2, 23);
    LoadSoundData("SOUNDS3.MNI", soundData3, 46);

    DrawFullscreenImage(IMAGE_PRETITLE);

    StartSound(SND_NEW_GAME);
    
    WaitSoft(200);

    LoadConfigurationData(JoinPath(writePath, FILENAME_BASE ".CFG"));

    SetBorderColorRegister(MODE1_BLACK);

    InitializeBackdropTable();

    maskedTileData = malloc(40000U);

    playerTileData = malloc((word)GroupEntryLength("PLAYERS.MNI"));

    mapData.b = malloc(WORD_MAX);

    /*
    16-bit nightmare here. Each actor data chunk is limited to 65,535 bytes,
    the first two chunks are full, and the last one gets the low word remander
    plus the two bytes that didn't fit into the first two chunks. If you find
    yourself asking "hey, what happens if there aren't two-and-a-bit chunks
    worth of data in the file" you get a shiny gold star.
    */
    actorTileData[0] = malloc(WORD_MAX);
    actorTileData[1] = malloc(WORD_MAX);
    actorTileData[2] = malloc((word)GroupEntryLength("ACTORS.MNI") + 2);

    LoadGroupEntryData("STATUS.MNI", actorTileData[0], 7296);
    CopyTilesToEGA(actorTileData[0], 7296 / 4, EGA_OFFSET_STATUS_TILES);

    LoadGroupEntryData("TILES.MNI", actorTileData[0], 64000U);
    CopyTilesToEGA(actorTileData[0], 64000U / 4, EGA_OFFSET_SOLID_TILES);

    LoadActorTileData("ACTORS.MNI");

    LoadGroupEntryData("PLAYERS.MNI", playerTileData, (word)GroupEntryLength("PLAYERS.MNI"));

    actorInfoData = malloc((word)GroupEntryLength("ACTRINFO.MNI"));
    LoadInfoData("ACTRINFO.MNI", actorInfoData, (word)GroupEntryLength("ACTRINFO.MNI"));

    playerInfoData = malloc((word)GroupEntryLength("PLYRINFO.MNI"));
    LoadInfoData("PLYRINFO.MNI", playerInfoData, (word)GroupEntryLength("PLYRINFO.MNI"));

    cartoonInfoData = malloc((word)GroupEntryLength("CARTINFO.MNI"));
    LoadInfoData("CARTINFO.MNI", cartoonInfoData, (word)GroupEntryLength("CARTINFO.MNI"));

    fontTileData = malloc(4000);
    LoadFontTileData("FONTS.MNI", fontTileData, 4000);

    if (isAdLibPresent) {
        tileAttributeData = malloc(7000);
        LoadTileAttributeData("TILEATTR.MNI");
    }

    totalMemFreeAfter = coreleft();

    ClearScreen();

    ShowCopyright();

    isJoystickReady = false;
}