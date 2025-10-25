#include "glue.h"

/* diag 0: 
   So by taking damage here, you'll 
   peel layers of Cosmo's psyche.
*/

/* i suppose this won't take in any context, */
/* context would be stored outside (the balls). */
void JustPrompt(void) {
    int x;

    EGA_MODE_LATCHED_WRITE();
    SelectDrawPage(activePage);

    /* diag 2: 
       Google declares the slogan 
    */
    if (now == 1) { 
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 Google declares the");
        DrawTextLine(x, 5, "\xFC""003 slogan, \"don't be evil\".");
        WaitSpinner(x + 28-2-1, 9+1);
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 At the same time,");
        DrawTextLine(x, 5, "\xFC""003 Leonard Somero");
        DrawTextLine(x, 6, "\xFC""003 Productions declares");
        WaitSpinner(x + 28-2-1, 9+1);
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 \"don't make a lot of");
        DrawTextLine(x, 5, "\xFC""003 cool games my dude and");
        DrawTextLine(x, 6, "\xFC""003 don't make them really");
        DrawTextLine(x, 7, "\xFC""003 fast either\"");
        WaitSpinner(x + 28-2-1, 9+1);
        now++;
        return;
    }

    /* diag 3 after the pounceHintWhatever */
    if (now == 2) {
        x = UnfoldTextFrame(1, scrollH-2+1+6+1, 38, "COSMIC HINT!", " Press any key to exit.");
        DrawTextLine(x+6+1, scrollH-4+1+6+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 In this way, you start experiencing");
        DrawTextLine(x, 5, "\xFC""003 exponential growth. But it's not");
        DrawTextLine(x, 6, "\xFC""003 something that goes on forever, you");
        DrawTextLine(x, 7, "\xFC""003 might have heard of a singularity,");
        DrawTextLine(x, 8, "\xFC""003 and this is a fundamental truth");
        DrawTextLine(x, 9, "\xFC""003 resulting from the study of limits");
        DrawTextLine(x, 10, "\xFC""003 introduced in calculus. So when a");
        DrawTextLine(x, 11, "\xFC""003 function goes to infinity, there's");
        DrawTextLine(x, 12, "\xFC""003 no sense to talk about what comes");
        DrawTextLine(x, 13, "\xFC""003 later in the x axis, considering");
        DrawTextLine(x, 14, "\xFC""003 we're only discussing a graph that");
        DrawTextLine(x, 15, "\xFC""003 doesn't describe a Platonic reality");
        DrawTextLine(x, 16, "\xFC""003 of mathematics anymore. You can");
        DrawTextLine(x, 17, "\xFC""003 maybe have aleph class infinities");
        DrawTextLine(x, 18, "\xFC""003 as you begin to introduce multiple");
        DrawTextLine(x, 19, "\xFC""003 perspectives, but my dude just take");
        DrawTextLine(x, 20, "\xFC""003 care of yourself first.");
        WaitSpinner(x + 38-2-1, scrollH-2-1+6+1+1);
        now++;
        return;
    }

    /* diag 4:
       "I'm gonna make a song that lasts 109090901.3 years!"
        My dude, there was never any doubt about that.
    */

    /* diag 5:
       Terence McKenna
    */

    /* diag 6:
       classical 2007 era Youtube Poops
    */

    /* diag 7:
       My dude, this peace is what all true warriors strive for.
    */

    /* diag 8:
       I'm not afraid of you.
    */

    /* diag 9:
       "he will survive"
    */

    /* diag 10:
       "read at your own risk"
    */
}