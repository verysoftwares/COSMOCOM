/* welcome to prompt.c */
/* it is where the prompts are stored stored stored */

#include "glue.h"

/* nää on että missä dialogissa ollaan */
word now = 8;
bbool now0 = false;

/* diag 0: 
   So by taking damage here, you'll 
   peel layers off Cosmo's psyche.
*/
/* mut mihinkä kohtaan koodia sää upotat tämmösen promptin ja pittäähän se muutenki ensin kirjottaa */
/* also try these on for size size size */
/* tulee 1 virhe: */
/*void (void) {
}*/
/* tulee 2 virhettä: */
/*void void(void) {
}*/
/* noniin nyt päästiin asiaan */
void void2(void) {
    int x;
    x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
    DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
    DrawTextLine(x-1, 4, "\xFC""003 So by taking damage here, you'll");
    DrawTextLine(x, 5, "\xFC""003 peel layers off Cosmo's psyche.");
    WaitSpinner(x + 28-2-1, 9+1);
    now0 = true;
}
/* edelleen mihin sää upotat tään ku ei tiiä mistä peli alakaa! */

/* i suppose this won't take in any context, */
/* context would be stored outside (the balls). */
void JustPrompt(void) {
    int x;

    EGA_MODE_LATCHED_WRITE();
    SelectDrawPage(activePage);

    /* diag 2: 
       Google declares the slogan 
    */
    if (now == 2) { 
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
    if (now == 3) {
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
       "I'm gonna make a song that lasts 109090901.3 years!!!!!!!!!!!"
        My dude, there was never any doubt about that.
    */
    if (now == 4) {
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x-1, 4, "\xFC""003 I'm gonna make a song that");
        DrawTextLine(x, 5, "\xFC""003 lasts 109090901.3 years!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        WaitSpinner(x + 28-2-1, 9+1);
        DrawTextLine(0, 6, "\xFC""003 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        WaitSpinner(x + 28-2-1, 9+1);
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 My dude, there was never");
        DrawTextLine(x, 5, "\xFC""003 any doubt about that!");
        WaitSpinner(x + 28-2-1, 9+1);
        now++;
        return;
    }

    /* diag 5:
       Terence McKenna
    */
    if (now == 5) {
        x = UnfoldTextFrame(1, scrollH-2+1+6+1, 38, "COSMIC HINT!", " Press any key to exit.");
        DrawTextLine(x+6+1, scrollH-4+1+6+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 I'd say Terence McKenna stands as");
        DrawTextLine(x, 5, "\xFC""003 an example of such a role model. He");
        DrawTextLine(x, 6, "\xFC""003 is one type of genius, but not the");
        DrawTextLine(x, 7, "\xFC""003 same type as Einstein, as Terence");
        DrawTextLine(x, 8, "\xFC""003 didn't come up with another huge");
        DrawTextLine(x, 9, "\xFC""003 innovation, he was more untangling");
        DrawTextLine(x, 10, "\xFC""003 the traps previous innovations had");
        DrawTextLine(x, 11, "\xFC""003 lead to.");
        WaitSpinner(x + 38-2-1, scrollH-2-1+6+1+1);
        x = UnfoldTextFrame(1, 11+1, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 He was frighteningly good");
        DrawTextLine(x, 5, "\xFC""003 at this, and his legacy");
        DrawTextLine(x, 6, "\xFC""003 shall live on in Youtube");
        DrawTextLine(x, 7, "\xFC""003 clips with really dist");
        DrawTextLine(x, 8, "\xFC""003 racting background music.");
        WaitSpinner(x + 28-2-1, 9+1+1);
        now++;
        return;
    }

    /* diag 6:
       classical 2007 era Youtube Poops
    */
    if (now == 6) {
        x = UnfoldTextFrame(1, scrollH-2+1+6+1, 38, "COSMIC HINT!", " Press any key to exit.");
        DrawTextLine(x+6+1, scrollH-4+1+6+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 I'm boutta expound my knowledge of");
        DrawTextLine(x, 5, "\xFC""003 the,,,, classical 2007 era Youtube");
        DrawTextLine(x, 6, "\xFC""003 Poops.");
        WaitSpinner(x + 38-2-1, scrollH-2-1+6+1+1);
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 Heavily implying that I");
        DrawTextLine(x, 5, "\xFC""003 know more than one cate");
        DrawTextLine(x, 6, "\xFC""003 gory.");
        WaitSpinner(x + 28-2-1, 9+1);
        x = UnfoldTextFrame(1, scrollH-2+1+6+1, 38, "COSMIC HINT!", " Press any key to exit.");
        DrawTextLine(x+6+1, scrollH-4+1+6+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 With no upper limit on how many I");
        DrawTextLine(x, 5, "\xFC""003 know either. Perhaps we need not");
        DrawTextLine(x, 6, "\xFC""003 only look into the future from");
        DrawTextLine(x, 7, "\xFC""003 2007, there may be a class of,,,,");
        DrawTextLine(x, 8, "\xFC""003 \"\"\"prehistoric\"\"\" Youtube Poops as");
        DrawTextLine(x, 9, "\xFC""003 well.");
        WaitSpinner(x + 38-2-1, scrollH-2-1+6+1+1);
        now++;
        return;
    }

    /* diag 7:
       My dude, this peace is what all true warriors strive for.
    */
    if (now == 7) {
        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 My dude, this peace is what");
        DrawTextLine(x, 5, "\xFC""003 all true warriors strive for.");
        WaitSpinner(x + 28-2-1, 9+1);
        now++;
        return;
    }

    /* diag 8:
       I'm not afraid of you.
    */
    if (now == 8) {
        x = UnfoldTextFrame(1, 1+1, 28, "", "");
        DrawTextLine(x, 1+1, "\xFC""003 I'm not afraid of you.");
        WaitSpinner(x + 28-2-1, 1+1);
        x = UnfoldTextFrame(1+3+1, 3+1, 28, "", "");
        DrawTextLine(x, 4+1+1, "\xFC""003 I'm afraid of what I'll");
        DrawTextLine(x, 5+1+1, "\xFC""003 say to you if I ever");
        DrawTextLine(x, 6+1+1, "\xFC""003 see you again.");
        WaitSpinner(x + 28-2-1, 3+1+1+1+1+1);
        x = UnfoldTextFrame(1+3+3+1+1+1, 1, 28, "", "");
        DrawTextLine(x, 1+3+3+1+1+1, "\xFC""003 So take my advice and");
        DrawTextLine(x, 1+3+3+1+1+1+1, "\xFC""003 don't");
        DrawTextLine(x, 1+3+3+2+1+1+1, "\xFC""003 contact");
        DrawTextLine(x, 1+3+3+3+1+1+1, "\xFC""003 me");
        DrawTextLine(x, 1+3+3+4+1+1+1, "\xFC""003 anymore.");
        WaitSpinner(x + 28-2-1, 1+3+3+4+1+1+1);
        now++;
        return;
    }

    /* diag 9:
       "he will survive"
    */
    if (now == 9) {
        x = UnfoldTextFrame(1, 1+1, 28, "", "");
        DrawTextLine(x, 1+1, "\xFC""003 It's not a matter of saying \"I will");
        WaitSpinner(x + 28-2-1, 1+1);
        DrawTextLine(x, 1+2, "\xFC""003 survive\", because that was never in");
        DrawTextLine(x, 1+3, "\xFC""003 doubt, and repeating that to myself");
        DrawTextLine(x, 1+4, "\xFC""003 seems a little bit frightening from");
        DrawTextLine(x, 1+5, "\xFC""003 the perspective of not having a");
        DrawTextLine(x, 1+6, "\xFC""003 spiritual teacher present with you.");
        x = UnfoldTextFrame(1+3+1, 3+1, 28, "", "");
        DrawTextLine(x, 1+8, "\xFC""003 However you have come so far that");
        DrawTextLine(x, 1+9, "\xFC""003 all such level of magic would offer");
        DrawTextLine(x+1, 1+9, "\xFC""003 all such level of magic would offer");
        DrawTextLine(x, 1+10, "\xFC""003 are some really expedient means to");
        DrawTextLine(x, 1+11, "\xFC""003 get to where you find yourself now.");
        DrawTextLine(x+1, 1+8, "\xFC""003 However you have come so far that");
        WaitSpinner(x + 28-2-1, 3+1+1+1+1+1);
        x = UnfoldTextFrame(1+3+3+1+1+1, 1, 28, "", "");
        DrawTextLine(x, 1+13, "\xFC""003 Someone else needs these healing");
        DrawTextLine(x, 1+14, "\xFC""003 powers more than you, or you");
        DrawTextLine(x, 1+15, "\xFC""003 might've been at need for them");
        DrawTextLine(x, 1+16, "\xFC""003 earlier, had you not persevered");
        DrawTextLine(x, 1+17, "\xFC""003 through sheer careful patience.");
        WaitSpinner(x + 28-2-1, 1+3+3+4+1+1+1);
        WaitSpinner(x + 28-2-1+1, 1+3+3+4+1+1+1);
        WaitSpinner(x + 28-2-1+1+1, 1+3+3+4+1+1+1);

        x = UnfoldTextFrame(1, 11, 28, "COSMIC HINT!", "Press any key to exit.");
        DrawTextLine(x+3-2, 8+1, "Press SPACE to hurry or");
        DrawTextLine(x, 4, "\xFC""003 Considering this you end up");
        DrawTextLine(x, 5, "\xFC""003 closing the tab.");
        WaitSpinner(x + 28-2-1, 9+1);

        now++;
        printf("The story is not an End yet!\nThe japanese Heart is not satisfied!\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.\n.");  /* 22 of these */
        exit(EXIT_SUCCESS);
    }

    /* diag 10:
       "read at your own risk"
       oho tää meni jotenki jo
    */
}