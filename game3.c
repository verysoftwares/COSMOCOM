#include "glue.h"

word t;
bool skip = false;
void GFXTest(void) {
    word start_spr = 0;
    word pagespr = 0;
    byte key;
    skip = false;

    while (start_spr <= 266) {
        lastScancode = SCANCODE_NULL;
        t = 0;

        FadeOut();
        ClearScreen2();
        FadeIn();

        pagespr = DebugPage(start_spr, pagespr, false);
        if (pagespr==999) break;

        /* page transition */
        StartSound(SND_DESTROY_SATELLITE);
        key = DebugKeyPause();
        if (key==SCANCODE_ESC) {
            break;
        }
        else if (key==SCANCODE_SPACE) {
            skip=!skip;
        }
        start_spr = NextUniqueSprite(pagespr);
        pagespr = start_spr;
    }

    StartSound(SND_WIN_LEVEL);
    FadeOut();
    ClearScreen();
    FadeIn();
}

void ClearScreen2()
{
    ClearAreaGray(0,0,40,25);
}

void DrawNumber(dword value, word x_origin, word y_origin)
{
    char text[16];
    int x, length;

    if (level_edit && (y_origin==0 || y_origin>scrollH)) return;

    EGA_MODE_DEFAULT();

    ultoa(value, text, 10);
    length = strlen(text);

    for (x = 0; x < length; x++) {
        if (level_edit && ((x_origin + x)==0 || (x_origin + x)>=39)) continue;
        DrawSpriteTile(
            fontTileData + FONT_0 + ((text[x] - '0') * 40), x_origin + x, y_origin
        );
    }
}

int LenNumber(dword value) {
    char text[16];
    ultoa(value, text, 10);
    return strlen(text);
}

word SpriteHeight(word sprite_type, word frame) {
    word offset = *(actorInfoData + sprite_type) + (frame * 4);
    return *(actorInfoData + offset);
}
word SpriteWidth(word sprite_type, word frame) {
    word offset = *(actorInfoData + sprite_type) + (frame * 4);
    word width = *(actorInfoData + offset + 1);
    int tlen = LenNumber(sprite_type);
    return max(width, tlen);
}

word NextUniqueSprite(word sprite_type) {
    word prev_spr_offset;
    if (sprite_type>=252) return 267; 
    prev_spr_offset = *(actorInfoData + sprite_type);
    sprite_type++;
    while (sprite_type<266 && *(actorInfoData + sprite_type)==prev_spr_offset) sprite_type++;
    return sprite_type;
}

word SpriteFrames(word sprite_type) {
    word myoffset, nextoffset;
    if (sprite_type>=252) return 1; /* we won't have to read OOB */
    myoffset = *(actorInfoData + sprite_type);
    nextoffset = *(actorInfoData + NextUniqueSprite(sprite_type));
    return (nextoffset - myoffset) / 4;
}
word SpriteSize(word sprite_type, SizeFunction size_fn) {
    word maxval = 0;
    word fr;
    word maxfr = SpriteFrames(sprite_type);
    for (fr=0; fr<maxfr; fr++) {
        if (size_fn(sprite_type, fr) > maxval) maxval = size_fn(sprite_type, fr);
    }
    return maxval;
}

void ClearAreaGray(word sx, word sy, word sw, word sh) {
    word x, y;

    EGA_MODE_LATCHED_WRITE();

    for (y = sy * 320; y < (sy+sh) * 320; y += 320) {
        for (x = sx; x < sx+sw; x++) {
            DrawSolidTile(TILE_DARK_GRAY, y + x);
        }
    }
}
void DebugSpriteBG(word spr, word sx, word sy) {
    word sw, sh;
    sw = SpriteSize(spr,SpriteWidth);
    sh = SpriteSize(spr,SpriteHeight);

    ClearAreaGray(sx,sy,sw,sh);
}
word prev_fr = 999;
word prev_spr = 999;
word prev_sx = 0;
word prev_sy = 0;
void DebugSprite(word spr, word sx, word sy, bool new) {
    word fr; 
    word sf = SpriteFrames(spr);
    if (sf==1 && t>0) return;
    fr = (t/12)%sf;

    if (new) StartSound(SND_BIG_OBJECT_HIT);
    DebugSpriteBG(spr, sx, sy);
    DrawSprite(spr, fr, sx, sy+(SpriteHeight(spr,fr)-1)+(SpriteSize(spr,SpriteHeight)-SpriteHeight(spr,fr)), DRAW_MODE_ABSOLUTE);
    WaitHard(1);
    if (new) {
        DrawNumber(spr, sx, sy+SpriteSize(spr,SpriteHeight));
        WaitHard(1);
    }

    prev_fr = fr; prev_spr = spr; prev_sx = sx; prev_sy = sy;
}
word last_spr;
word DebugRow(word start_spr, word pagespr, word sy, bool new, bool observe) {
    word sx = 0;
    word row_height = 0;
    last_spr = start_spr;
    
    while (last_spr <= pagespr) {
        if (!observe && last_spr==pagespr) {
            DebugSprite(last_spr, sx, sy, new);
            new = false;
        }

        if (SpriteSize(last_spr,SpriteHeight) > row_height) {
            row_height = SpriteSize(last_spr,SpriteHeight);
        }
        sx += SpriteSize(last_spr,SpriteWidth);

        last_spr = NextUniqueSprite(last_spr);
        if (last_spr>266) return 0;

        if (sx+SpriteSize(last_spr,SpriteWidth) > 40) {
            if (observe) last_spr = start_spr;
            return row_height;
        }
    }

    return 0;
}
byte DebugKeyPause() {
    lastScancode = SCANCODE_NULL;  /* will get modified by the keyboard interrupt service */

    while ((lastScancode & 0x80) == 0) {
        /* used to re-render the whole page but this no longer makes sense */
        /* DebugPage(start_spr, pagespr, true); */
        DebugSprite(prev_spr, prev_sx, prev_sy, false);
        t++;
    }

    return lastScancode & ~0x80;    
}
word DebugPage(word start_spr, word pagespr, bool observe) {
    word sy = 0;
    word row_height;
    bool new = !observe && start_spr==pagespr;
    word spr = start_spr;
    /* word orig_spr = start_spr; */
    
    /* if (t%3==0) ClearScreen2(); */

    while (spr <= pagespr) {
        row_height = DebugRow(start_spr, pagespr, sy, new, false);
        new = false;

        if (last_spr>266) return last_spr;
        spr = last_spr;

        if (row_height > 0) {
            sy += row_height+1;
            start_spr = spr;

            if (sy+DebugRow(start_spr, 267, sy, false, true)+1 >= 25) {
                /* time to swap pages */
                break;
            }
        } 

        if (spr>pagespr && !observe) {
            if (!skip) {
                byte key = DebugKeyPause();
                if (key==SCANCODE_ESC) {
                    return 999;
                }
                else if (key==SCANCODE_SPACE) {
                    skip=true;
                }
            } 
            pagespr = spr;
            new = true;
            t = 0;
        }
    }

    t++;

    return pagespr;
}

word ln = 4;
bbool level_edit = false;
word scrollH = 18;
bool info;
bbool mtrans = false;

void ChangeLevel(int add) {
    if (ln==0 && add<0) return;
    if (ln==27 && add>0) return;

    StartSound(SND_NEW_GAME);
    ln = ln+add;
    InitializeLevel(ln);
    info = true;
    scrollH = 18;
    maxScrollY = (word)(0x10000L / (mapWidth * 2)) - scrollH;
    mtrans = true;
}

static bbool isKeyHeld[BYTE_MAX];
static bbool held_init = false;
bbool tapped(word scn) {
    if (!held_init) {
        word k;
        for (k=0; k<BYTE_MAX; k++) isKeyHeld[k]=false;
        held_init = true;
    }

    if (isKeyDown[scn]) {
        if (!isKeyHeld[scn]) {
            isKeyHeld[scn] = true;
            return true;
        } else return false;
    } else {
        if (isKeyHeld[scn]) isKeyHeld[scn] = false;
    }
    return false;
}

/* to resolve the tile/masked tile dualism from a drawing standpoint */
void GenDrawTile(word tile, word x, word y) {
    if (tile < TILE_MASKED_0) DrawSolidTile(tile, y*320 + x);
    else DrawMaskedTile(maskedTileData + tile, x, y);
}

bbool cursor_move(Cursor *c, word xmin, word ymin, word xmax, word ymax) {
    if (isKeyDown[scancodeNorth] && c->y > ymin) { c->y--; return true; }
    if (isKeyDown[scancodeSouth] && c->y < ymax) { c->y++; return true; }
    if (isKeyDown[scancodeWest] && c->x > xmin) { c->x--; return true; }
    if (isKeyDown[scancodeEast] && c->x < xmax) { c->x++; return true; }
    return false;
}

void focus_actor(Cursor *c, int add) {
    /* pick next actor */
    do
    if (add<0 && c->tile==0) c->tile=numActors-1;
    else if (add>0 && c->tile==numActors-1) c->tile=0;
    else c->tile=c->tile+add;
    while (add!=0 && actors[c->tile].dead);

    /* and focus camera on them */
    scrollX=0;
    if (actors[c->tile].x>=19) scrollX=actors[c->tile].x-19;
    if (scrollX>mapWidth-38) scrollX=mapWidth-38;
    scrollY=0;
    if (actors[c->tile].y>=scrollH/2) scrollY=actors[c->tile].y-scrollH/2;
    if (scrollY>maxScrollY) scrollY=maxScrollY;    
}

bbool edit_actors;
Cursor c;
Cursor c2;
Cursor ca;
word st;
void LevelTest(void) {
    word *tile_at;
    bbool redraw = true;

    c.x = 1; c.y = 1;
    c.active = false;
    c.tile = 0;
    c2.x=0; c2.y=0;
    c2.active=true;
    c2.tile=0;
    ca.x=0; ca.y=0;
    ca.active=false;
    ca.tile=0;

    t = 0; st = 0;
    ln = 4;
    level_edit = true;
    edit_actors = true;
    ChangeLevel(0);

    SelectDrawPage(0);
    ClearScreen();
    SelectDrawPage(1);
    ClearScreen();

    while (1) {
        tile_at = mapData.w + ((scrollY + c.y-1) << mapYPower) + (scrollX + c.x-1);
                    
        if (redraw || c.active) {
            SelectDrawPage(activePage);

            DrawMapRegion();

            MoveAndDrawActors();
            
            if (info) {
                char msg[39]; byte h=0;
                ClearAreaGray(1,scrollH+1,40-2,24-scrollH);
                if (!c.active) {
                    sprintf(msg,"Level #%u (%u,%u)",ln,scrollX,scrollY);
                    DrawTextLine(1,scrollH+1+h,msg); h++;
                    sprintf(msg,"BG #%u (%s)",lvlBdnum,backdropNames[lvlBdnum]);
                    DrawTextLine(1,scrollH+1+h,msg); h++;
                    sprintf(msg,"Music #%u (%s)",lvlMusicNum,musicNames[lvlMusicNum]);
                    DrawTextLine(1,scrollH+1+h,msg); h++;
                    sprintf(msg,"%u actors",numActors);
                    DrawTextLine(1,scrollH+1+h,msg); h++;
                } else {
                    word i;
                    sprintf(msg,"Tile #%u (%u,%u)",*tile_at,scrollX+c.x-1,scrollY+c.y-1);
                    DrawTextLine(1,scrollH+1+h,msg); h++;
                    GenDrawTile(c.tile,c.x,c.y);
                    for (i=0; i<(t/4)%3; i++) LightenScreenTile(c.x, c.y);
                }
            }

            if (mtrans) {
                FadeIn();
                mtrans = false;
            }

            SelectActivePage(activePage);
            activePage = !activePage;

            redraw = false;
        } /* if (redraw) */
        
        if (st) st--;

        if (isKeyDown[SCANCODE_ESC]) break;
        if (tapped(SCANCODE_C) && !ca.active) { c.active = !c.active; StartSound(SND_PLANT_MOUTH_OPEN); redraw=true; }
        if (tapped(SCANCODE_A) && !c.active) { ca.active = !ca.active; if (ca.active) focus_actor(&ca,0); StartSound(SND_PLANT_MOUTH_OPEN); redraw=true; }
        if (tapped(SCANCODE_T)) {
            TileView();
            redraw=true; 
        }

        if (!c.active && !ca.active) {
            /* global state */
            if (isKeyDown[scancodeNorth] && scrollY>0) { scrollY--; if (!st) { StartSound(SND_SCOOTER_PUTT); st=4; } redraw=true; }
            if (isKeyDown[scancodeSouth] && scrollY<maxScrollY) { scrollY++; if (!st) { StartSound(SND_SCOOTER_PUTT); st=4; } redraw=true; }
            if (isKeyDown[scancodeWest] && scrollX>0) { scrollX--; if (!st) { StartSound(SND_SCOOTER_PUTT); st=4; } redraw=true; }
            if (isKeyDown[scancodeEast] && scrollX<mapWidth-38) { scrollX++; if (!st) { StartSound(SND_SCOOTER_PUTT); st=4; } redraw=true; }
            if (tapped(SCANCODE_Q)) { ChangeLevel(-1); redraw=true; } 
            if (tapped(SCANCODE_W)) { ChangeLevel( 1); redraw=true; } 
            if (tapped(SCANCODE_S)) { edit_actors = !edit_actors; redraw=true; }
            if (tapped(SCANCODE_I)) {
                info = !info;
                if (info) scrollH = 18;
                else scrollH = 25-1;
                maxScrollY = (word)(0x10000L / (mapWidth * 2)) - scrollH;
                redraw=true; 
            }
        } else if (c.active) {
            /* tile edit state */
            if (cursor_move(&c,1,1,38,scrollH)) {
                if (!st) { StartSound(SND_PLAYER_FOOTSTEP); st=12; }
                redraw=true;
            }
            if (tapped(SCANCODE_P)) {
                c.tile = *tile_at;
                StartSound(SND_TULIP_INGEST);
                redraw=true;
            }
            if (isKeyDown[SCANCODE_O]) {
                *tile_at = c.tile;
                StartSound(SND_PLACE_BOMB);
                redraw=true;
            }
        } else if (ca.active) {
            /* actor edit state */
            if (numActors>0) {
                if (tapped(SCANCODE_Z)) {
                    focus_actor(&ca,-1);
                    redraw=true;
                }
                if (tapped(SCANCODE_X)) {
                    focus_actor(&ca,1);
                    redraw=true;
                }
            }
            if (tapped(SCANCODE_C)) {
                word type=actors[ca.tile].type;
                if (type==0) type=ACT_EP2_END_LINE;
                else type=type-1;
                NewActorAtIndex(ca.tile,type,actors[ca.tile].x,actors[ca.tile].y);
                redraw=true;
            }
            if (tapped(SCANCODE_V)) {
                word type=actors[ca.tile].type;
                if (type==ACT_EP2_END_LINE) type=0;
                else type=type+1;
                NewActorAtIndex(ca.tile,type,actors[ca.tile].x,actors[ca.tile].y);
                redraw=true;
            }
        }

        WaitHard(1);

        t++;
    }
    scrollH = 18;
    maxScrollY = (word)(0x10000L / (mapWidth * 2)) - scrollH;
    level_edit = false;
    mask_init = false;
}

word tile_start = 0;
void TileView(void) {
    word x, y;
    word tile;
    word i;
    
    SelectDrawPage(activePage);
    SelectActivePage(activePage);
    activePage = !activePage;
    
    ClearScreen2();
    
    while(1) {
        tile = tile_start;

        for (y = 0; y < 25; y++) {
            for (x = 0; x < 40; x++) {
                GenDrawTile(tile, x, y);

                if (tile < TILE_MASKED_0) tile+=8;
                else tile+=40;
            }
            /* WaitHard(1); */
        }

        if (st) st--;
        if (cursor_move(&c2,0,0,40,25)) {
            if (!st) { StartSound(SND_PLAYER_FOOTSTEP); st=12; }
        }
        for (i=0; i<(t/4)%3; i++) LightenScreenTile(c2.x, c2.y);
        if (tapped(SCANCODE_P)) {
            if (tile_start<TILE_MASKED_0) c.tile = tile_start+(c2.y*40+c2.x)*8;
            else c.tile = tile_start+(c2.y*40+c2.x)*40;
            StartSound(SND_TULIP_INGEST);
        }
        if (tapped(SCANCODE_Q)) {
            if (tile_start<25*80*4) tile_start=0; 
            else tile_start=tile_start-25*80*4; 
            ClearScreen2();
        }
        if (tapped(SCANCODE_W)) { 
            /* if (tile_start>=tmax-25*80*4) tile_start=tmax-25*80*4; */
            if (tile_start<TILE_MASKED_0) tile_start=tile_start+25*80*4; 
            else tile_start=0;
            ClearScreen2();
        }
        if (tapped(SCANCODE_T) || tapped(SCANCODE_ESC)) {
            /* FadeOut(); */
            ClearScreen();
            SelectDrawPage(activePage);
            SelectActivePage(activePage);
            ClearScreen();
            mtrans = true;
            return;
        }

        WaitHard(1);
        t++;
    }
}