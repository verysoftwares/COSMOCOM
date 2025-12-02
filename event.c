#include "glue.h"

/* this won't work in Turbo C unfortunately,
 * the case statements have to be hard-coded
 */ 
/* const word LV2 = 0; */

void testevent(void) {
    printf("wrong event my dude(s)!");
}

typedef void (*EventFunc)(void);

/* originally these were supposed to store event ids, 
 * resulting in a confusing extra layer of redirection...
 *
 * well, the bottom line is i can still redirect an event
 * by just assigning a new function pointer to the slot.
 */ 
/* word* */  EventFunc hurt_evs[256];
/* word* */  EventFunc pick_evs[256];
/* word* */  EventFunc jump_evs[256];
/* word* */  EventFunc bomb_evs[256];
/* word** */ EventFunc* gen_evs[4] = {hurt_evs,
                                      pick_evs,
                                      jump_evs,
                                      bomb_evs};

void assign(word _ecat, word aid, EventFunc efunc) {
    EventFunc* ecat;
    /* sanity check re-done in switch-case */
    /*if (_ecat<0 || _ecat>=4) return;*/

    ecat = gen_evs[_ecat];

    switch (_ecat) {
        case 0:
            /* event src: taken damage */
        case 1:
            /* event src: collect actor */
            /*if (levelNum==9 && aid==252) {
            }*/
            break;
        case 2:
            /* event src: jump on actor */
        case 3:
            /* event src: bomb actor */
            ecat[aid] = efunc;
            break;
        case 4:
            /* hardcoded special case no.1 */
            break;
        default:
            /* ??? */
            break;
    }
}

void launch(word _ecat, word aid/*, word _eid*/) {
    EventFunc* ecat; /*Actor* aid; word eid;*/
    EventFunc cure;
    switch (_ecat) {
        case 0:
        case 1:
        case 2:
        case 3:
            ecat = gen_evs[_ecat];
            break;
        default:
            return;
    }

    /* now the event category 'ecat' is one of 
     * hurt_evs, pick_evs, jump_evs, or bomb_evs.
     */

    /* let's figure out who started the event.
     */

    if (aid<0 || aid>MAX_ACTORS) return; 
    if ( (actors + aid)->dead ) return;

    /* so which event is actually being launched here. */

    cure = *(ecat + aid);
    if (cure==testevent) {
        return;
    }

    /* maybe each 'cure' doesn't need to handle 
     * textbox creation & timing windows over and over,
     * only the variable result of an event.
     *
     * .. then again i'd end up juggling so many arguments
     * and you know that's not how i roll.
     *
     * nonetheless, the common calls can be encapsulated.
     */

              /*vvvvvv*/
    /*>>>>>>*/  cure(); /*<<<<<<*/
              /*^^^^^^*/

    /* alright, our job here is done.
     *
     * 'cure' starts a chain reaction
     * somewhere in the game loop.
     */

    /* nevermind this layer of event redirection, 
     * you can literally just store function pointers.
     */
    /* eid = events[_eid]; */
}

void init_lv_events(void) {
    word i,j;
    for (j=0; j<4; j++) { for (i=0; i<256; i++) {
        gen_evs[j][i] = testevent;
    }}

    switch (levelNum) {
        case 0:
            /* assign hurt (_ecat=0) testevent for actor no. 3 */
            assign(0,3,testevent);
            break;
        default:
            break;
    }
}