#include "glue.h"

/* this won't work in Turbo C unfortunately,
 * the switch-case labels have to be hard-coded:
 */ 
    /* const word LV2 = 0; */

void testevent(void) {
    mid_box("wrong event my dude(s)!","","","");
}
void testhurt(void) {
    char sp[40];
    sprintf(sp,"%s%d%s","Hurt by actor #",hurt_act,"!");
    hurt_act = 9999; /* the easiest way to avoid self-bomb triggers */
    mid_box(sp,"","","");
}
void testpick(void) {
    char sp[40];
    sprintf(sp,"%s%d%s","Picked up actor #",pick_act,"!");
    pick_act = 9999;
    mid_box(sp,"","","");
}
void testjump(void) {
    char sp[40];
    sprintf(sp,"%s%d%s","Pounced on actor #",jump_act,"!");
    jump_act = 9999;
    mid_box(sp,"","","");
}
void starpick(void) {
    word choice1,choice2;
    choice1 = random(7);
    choice2 = random(7);
    pick_act = 9999;
    choice_box(choice1,"           vai",choice2,"            ?");
}
word fetch_ids[4] = {160,214,215,216/*,225*/};
typedef struct {
    word n,id,sp,prog,src,type;
} Quest;
Quest quests[4]; word q=0;
void fetch(word index) {
    word i; for (i=0; i<q; i++) {
        if (quests[i].id==actors[index].type) {
            quests[i].prog++;
        }
    }
}
void qdisplay(void) {
    word i; for (i=0; i<q; i++) {
        char qprog[40]; char img[40];
        ClearAreaGray(1+i*7,1,6,4);
        if (quests[i].prog<quests[i].n) {
            sprintf(qprog,"%d of %d",quests[i].prog,quests[i].n);
            /* DrawSprite doesn't display anything, */
            /* so i use the text parser instead. */
            /*DrawSprite(quests[i].sp,0,i*8,0,DRAW_MODE_NORMAL);*/
            sprintf(img, "%s%.3d000","\xFE",quests[i].sp);
            DrawTextLine(1+i*7+2,3,img);
            DrawTextLine(1+i*7,1+3,qprog);
        } else {
            sprintf(qprog," %d",quests[i].src);
            /* DrawSprite doesn't display anything, */
            /* so i use the text parser instead. */
            /*DrawSprite(quests[i].sp,0,i*8,0,DRAW_MODE_NORMAL);*/
            sprintf(img, "%s%.3d001","\xFE",124);
            DrawTextLine(1+i*7+1,3,img);
            DrawTextLine(1+i*7,1+3,qprog);
        }
    }
}
void wormjump(void) {
    /* quests 
     * collect x of these: <img>
     */
    if (q<4) {
        if (actors[jump_act].data5==1) {
            char qprompt[40]; word i; Quest newq; bool fresh;
            newq.type = random(2);
            newq.n = 3+random(3);
            newq.prog = 0;
            newq.src = jump_act;
            do {
                newq.id = fetch_ids[random(4)];
                fresh = true;
                for (i=0; i<q; i++) {
                    if (quests[i].id==newq.id) {
                        fresh = false;
                        break;
                    }
                }
            } while(!fresh);
            for (i=0; i<MAX_ACTORS; i++) {
                if (actors[i].type==newq.id) {
                    newq.sp = actors[i].sprite;
                    break;
                }
            }
            mid_box("I have a quest for you...","","","");
            sprintf(qprompt,"%s%d%s \xFE%.3d000"/* (%.3d)"*/,"Collect ",newq.n," of these:",newq.sp/*,quest_id*/);
            mid_box(qprompt,"","","");
            quests[q]=newq; q++;
        } else if (actors[jump_act].data5==0) {
            char qprompt[40]; word i; bool win=false;
            for (i=0; i<q; i++) {
                if (quests[i].src==jump_act && quests[i].prog>=quests[i].n) {
                    win=true; break;
                } 
            }
            if (win) {sprintf(qprompt,"         Superb!");} else {sprintf(qprompt,"          Fail!");}
            mid_box("Finishing quest...","","",qprompt);
            starpick();
        } else {
            mid_box("I never did","anything wroooong!","","");
        } 
    } else {
        mid_box("Don't you already have","quite a few quests?","","");
    }
    jump_act = 9999;
}

typedef void (*EventFunc)(void);

/* originally these were supposed to store event ids, 
 * resulting in a confusing extra layer of redirection...
 *
 * well, the bottom line is i can still redirect an event
 * by just assigning a new function pointer to the slot.
 */ 
/* word* */  EventFunc hurt_evs[MAX_ACTORS]; /* _ecat=0 */
/* word* */  EventFunc pick_evs[MAX_ACTORS]; /* _ecat=1 */
/* word* */  EventFunc jump_evs[MAX_ACTORS]; /* _ecat=2 */
/* word* */  EventFunc bomb_evs[MAX_ACTORS]; /* _ecat=3 */
/* word** */ EventFunc* gen_evs[4] = {hurt_evs,
                                      pick_evs,
                                      jump_evs,
                                      bomb_evs};
static EventFunc* fetch_ecat(word _ecat) {
    EventFunc* ecat; /*Actor* aid; word eid;*/
    switch (_ecat) {
        case 0:
        case 1:
        case 2:
        case 3:
            ecat = gen_evs[_ecat];
            break;
        default:
            return NULL;
    }

    return ecat;
}

static EventFunc fetch_event_cache(EventFunc* ecat, word aid) {
    /* for when ecat has already been fetched & vetted */
    EventFunc cure = testevent;

    /* now the event category 'ecat' is one of 
     * hurt_evs, pick_evs, jump_evs, or bomb_evs.
     */
    /* let's figure out who started the event.
     */

    if ( (aid<0) || (aid>MAX_ACTORS) ) return testevent;
    /* if ( (actors + aid)->dead ) return testevent; */

    /* so which event is actually being launched here. */

    cure = *(ecat + aid);

    /* this happens anyway on failure but it's */
    /* just for the sake of being explicit. */
    if (cure == testevent) {
        return testevent;
    }
    
    /* alright, 'cure' is something other than 'testevent': */
    return cure;
}

/*static EventFunc fetch_event(word _ecat, word aid) {
    EventFunc* ecat = fetch_ecat(_ecat);
    if (ecat == NULL) { mid_box("Event MISSINGNO. request","","",""); return testevent; }

    return fetch_event_cache(ecat, aid);
}*/

/*static void registevt(EventFunc* ecat, word aid, EventFunc cure) {
    ecat[aid] = cure;
}*/

static void assign(word _ecat, word aid, EventFunc newe) {
    EventFunc* ecat = fetch_ecat(_ecat);
    if (ecat == NULL) { mid_box("Event MISSINGNO. assign","","",""); return; }
    ecat[aid] = newe;
}

/*static bool has_event(word _ecat, word aid) {
    return (fetch_event(_ecat,aid) != testevent);
}*/

void launch(word _ecat, word aid/*, word _eid*/) {
    EventFunc* ecat; EventFunc cure;

    ecat = fetch_ecat(_ecat);
    if (ecat == NULL) { mid_box("Event MISSINGNO. category","","",""); return; }
    /* note the missing 'fetch_event' since we */
    /* need to know both 'ecat' and 'cure'. */
    cure = fetch_event_cache(ecat, aid);
    if (cure == testevent) { mid_box("Event MISSINGNO. launch","","",""); return; }

    /* maybe each 'cure' doesn't need to handle 
     * textbox creation & timing windows over and over,
     * only the variable result of an event.
     *
     * .. then again i'd end up juggling so many arguments
     * and you know that's not how i roll.
     *
     * nonetheless, the common calls can be encapsulated.
     */

    /* >>>> */ cure(); /* <<<< */

    /* alright, our job here is done.
     *
     * 'cure' starts a chain reaction
     * somewhere in the game loop.
     */

    /* nevermind this layer of event redirection since 
     * you can literally just store function pointers.
     */
    /* eid = events[_eid]; */
}

void init_lv_events(word ln) {
    word i,j;
    for (j=0; j<4; j++) { for (i=0; i<MAX_ACTORS; i++) {
        gen_evs[j][i] = testevent;
        if (j==0) gen_evs[j][i] = testhurt;
        if (j==1) gen_evs[j][i] = testpick;
        if (j==2) gen_evs[j][i] = testjump;
        if (j==1 && (actors[i].type==ACT_STAR_FLOAT || actors[i].type==ACT_STAR)) {
            gen_evs[j][i] = starpick;
        }
        if (j==2 && actors[i].type==ACT_PINK_WORM) {
            gen_evs[j][i] = wormjump;
        }
    }}
    switch (ln) {
        case 9:
            /* assign hurt (_ecat=0) event, for actor no. 3, as 'void2'. */
            /* assign(0,3,void2); */
            /*assign(1,263,starpick);
            assign(1,237,starpick);*/
            break;
        default:
            break;
    }
}