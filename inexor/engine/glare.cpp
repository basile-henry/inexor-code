#include "inexor/engine/engine.h"
#include "inexor/engine/rendertarget.h"

static struct glaretexture : rendertarget
{
    bool dorender()
    {
        extern void drawglare();
        drawglare();
        return true;
    }
} glaretex;

void cleanupglare()
{
    glaretex.cleanup(true);
}

VARFP(glaresize, 6, 8, 10, cleanupglare());
VARP(glare, 0, 0, 1);
VARP(blurglare, 0, 4, 7);
VARP(blurglaresigma, 1, 50, 200);

VAR(debugglare, 0, 0, 1);

void viewglaretex()
{
    if(!glare) return;
    glaretex.debug();
}

bool glaring = false;

void drawglaretex()
{
    if(!glare) return;

    glaretex.render(1<<glaresize, 1<<glaresize, blurglare, blurglaresigma/100.0f);
}

FVARP(glarescale, 0, 1, 8);

void addglare()
{
    if(!glare) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    SETSHADER(screenrect);

    glBindTexture(GL_TEXTURE_2D, glaretex.rendertex);

    gle::colorf(glarescale, glarescale, glarescale);

    screenquad(1, 1);

    glDisable(GL_BLEND);
}
     
