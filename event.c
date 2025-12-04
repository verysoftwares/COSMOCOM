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
    mid_box(sp,"","","");
}
void testpick(void) {
    char sp[40];
    sprintf(sp,"%s%d%s","Picked up actor #",pick_act,"!");
    mid_box(sp,"","","");
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
    if ( (actors + aid)->dead ) return testevent;

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
    }}

    switch (ln) {
        case 9:
            /* assign hurt (_ecat=0) event, for actor no. 3, as 'void2'. */
            assign(0,3,void2);
            break;
        default:
            break;
    }
}