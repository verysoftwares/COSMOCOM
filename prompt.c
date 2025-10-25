#include "glue.h"

/* i suppose this won't take in any context, */
/* context would be stored outside (the balls). */
void JustPrompt(void) {
    int x;

    EGA_MODE_LATCHED_WRITE();
    SelectDrawPage(activePage);

    x = UnfoldTextFrame(1, scrollH-2+1+6+1, 38, "COSMIC HINT!", "");
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
    DrawTextLine(x+6, scrollH-4+1+6+1, "Press SPACE to hurry or");
    DrawTextLine(x+6, scrollH-3+1+6+1, " Press any key to exit.");
    WaitSpinner(x + 38-2-1, scrollH-2-1+6+1+1);
}