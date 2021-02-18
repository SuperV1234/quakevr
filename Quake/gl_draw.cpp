/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// draw.c -- 2d drawing

#include <GL/glew.h>

#include "quakedef.hpp"
#include "console.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "quakedef_macros.hpp"
#include "sbar.hpp"
#include "wad.hpp"
#include "gl_texmgr.hpp"
#include "qpic.hpp"
#include "glquake.hpp"
#include "sys.hpp"
#include "screen.hpp"
#include "draw.hpp"
#include "client.hpp"
#include "quakeglm_qvec3_togl.hpp"
#include "quakeglm_qvec4_togl.hpp"

// extern unsigned char d_15to8table[65536]; //johnfitz -- never used

bool premul_hud = false;                                     // true;
cvar_t scr_conalpha = {"scr_conalpha", "0.5", CVAR_ARCHIVE}; // johnfitz

qpic_t* draw_disc;
qpic_t* draw_backtile;

gltexture_t* char_texture; // johnfitz
qpic_t *pic_ovr, *pic_ins; // johnfitz -- new cursor handling
qpic_t* pic_nul;           // johnfitz -- for missing gfx, don't crash

// johnfitz -- new pics
byte pic_ovr_data[8][8] = {
    {255, 255, 255, 255, 255, 255, 255, 255},
    {255, 15, 15, 15, 15, 15, 15, 255},
    {255, 15, 15, 15, 15, 15, 15, 2},
    {255, 15, 15, 15, 15, 15, 15, 2},
    {255, 15, 15, 15, 15, 15, 15, 2},
    {255, 15, 15, 15, 15, 15, 15, 2},
    {255, 15, 15, 15, 15, 15, 15, 2},
    {255, 255, 2, 2, 2, 2, 2, 2},
};

byte pic_ins_data[9][8] = {
    {15, 15, 255, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {15, 15, 2, 255, 255, 255, 255, 255},
    {255, 2, 2, 255, 255, 255, 255, 255},
};

byte pic_nul_data[8][8] = {
    {252, 252, 252, 252, 0, 0, 0, 0},
    {252, 252, 252, 252, 0, 0, 0, 0},
    {252, 252, 252, 252, 0, 0, 0, 0},
    {252, 252, 252, 252, 0, 0, 0, 0},
    {0, 0, 0, 0, 252, 252, 252, 252},
    {0, 0, 0, 0, 252, 252, 252, 252},
    {0, 0, 0, 0, 252, 252, 252, 252},
    {0, 0, 0, 0, 252, 252, 252, 252},
};

byte pic_stipple_data[8][8] = {
    {255, 0, 0, 0, 255, 0, 0, 0},
    {0, 0, 255, 0, 0, 0, 255, 0},
    {255, 0, 0, 0, 255, 0, 0, 0},
    {0, 0, 255, 0, 0, 0, 255, 0},
    {255, 0, 0, 0, 255, 0, 0, 0},
    {0, 0, 255, 0, 0, 0, 255, 0},
    {255, 0, 0, 0, 255, 0, 0, 0},
    {0, 0, 255, 0, 0, 0, 255, 0},
};

byte pic_crosshair_data[8][8] = {
    {255, 255, 255, 255, 255, 255, 255, 255},
    {255, 255, 255, 8, 9, 255, 255, 255},
    {255, 255, 255, 6, 8, 2, 255, 255},
    {255, 6, 8, 8, 6, 8, 8, 255},
    {255, 255, 2, 8, 8, 2, 2, 2},
    {255, 255, 255, 7, 8, 2, 255, 255},
    {255, 255, 255, 255, 2, 2, 255, 255},
    {255, 255, 255, 255, 255, 255, 255, 255},
};
// johnfitz

typedef struct
{
    gltexture_t* gltexture;
    float sl, tl, sh, th;
} glpic_t;

canvastype currentcanvas = CANVAS_NONE; // johnfitz -- for GL_SetCanvas

//==============================================================================
//
//  PIC CACHING
//
//==============================================================================

typedef struct cachepic_s
{
    char name[MAX_QPATH];
    qpic_t pic;
    byte padding[32]; // for appended glpic
} cachepic_t;

#define MAX_CACHED_PICS 512 // Spike -- increased to avoid csqc issues.
cachepic_t menu_cachepics[MAX_CACHED_PICS];
int menu_numcachepics;

byte menuplyr_pixels[4096];

//  scrap allocation
//  Allocate all the little status bar obejcts into a single texture
//  to crutch up stupid hardware / drivers

#define MAX_SCRAPS 2
#define BLOCK_WIDTH 256
#define BLOCK_HEIGHT 256

int scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte scrap_texels[MAX_SCRAPS]
                 [BLOCK_WIDTH *
                     BLOCK_HEIGHT]; // johnfitz -- removed *4 after BLOCK_HEIGHT
bool scrap_dirty;
gltexture_t* scrap_textures[MAX_SCRAPS]; // johnfitz


/*
================
Scrap_AllocBlock

returns an index into scrap_texnums[] and the position inside it
================
*/
int Scrap_AllocBlock(int w, int h, int* x, int* y)
{
    int i;
    int j;
    int best;
    int best2;
    int texnum;

    for(texnum = 0; texnum < MAX_SCRAPS; texnum++)
    {
        best = BLOCK_HEIGHT;

        for(i = 0; i < BLOCK_WIDTH - w; i++)
        {
            best2 = 0;

            for(j = 0; j < w; j++)
            {
                if(scrap_allocated[texnum][i + j] >= best)
                {
                    break;
                }
                if(scrap_allocated[texnum][i + j] > best2)
                {
                    best2 = scrap_allocated[texnum][i + j];
                }
            }
            if(j == w)
            {
                // this is a valid spot
                *x = i;
                *y = best = best2;
            }
        }

        if(best + h > BLOCK_HEIGHT)
        {
            continue;
        }

        for(i = 0; i < w; i++)
        {
            scrap_allocated[texnum][*x + i] = best + h;
        }

        return texnum;
    }

    Sys_Error("Scrap_AllocBlock: full"); // johnfitz -- correct function name
    return 0;                            // johnfitz -- shut up compiler
}

/*
================
Scrap_Upload -- johnfitz -- now uses TexMgr
================
*/
void Scrap_Upload()
{
    char name[8];
    int i;

    for(i = 0; i < MAX_SCRAPS; i++)
    {
        sprintf(name, "scrap%i", i);
        scrap_textures[i] =
            TexMgr_LoadImage(nullptr, name, BLOCK_WIDTH, BLOCK_HEIGHT,
                SRC_INDEXED, scrap_texels[i], "", (src_offset_t)scrap_texels[i],
                (premul_hud ? TEXPREF_PREMULTIPLY : 0) | TEXPREF_ALPHA |
                    TEXPREF_OVERWRITE | TEXPREF_NOPICMIP);
    }

    scrap_dirty = false;
}

/*
================
Draw_PicFromWad
================
*/
qpic_t* Draw_PicFromWad(const char* name)
{
    int i;
    cachepic_t* pic;
    qpic_t* p;
    glpic_t gl;
    src_offset_t offset; // johnfitz
    lumpinfo_t* info;

    // Spike -- added cachepic stuff here, to avoid glitches if the function is
    // called multiple times with the same image.
    for(pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
    {
        if(!strcmp(name, pic->name))
        {
            return &pic->pic;
        }
    }
    if(menu_numcachepics == MAX_CACHED_PICS)
    {
        Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
    }

    p = (qpic_t*)W_GetLumpName(name, &info);
    if(!p)
    {
        Con_SafePrintf("W_GetLumpName: %s not found\n", name);
        return pic_nul; // johnfitz
    }

    // QSS
    if(info->size < sizeof(int) * 2 || 8 + p->width * p->height < info->size)
    {
        Sys_Error("Draw_PicFromWad: pic \"%s\" truncated", name);
    }

    // QSS
    if(info->type != TYP_QPIC)
    {
        Sys_Error("Draw_PicFromWad: lump \"%s\" is not a qpic", name);
    }

    // load little ones into the scrap
    if(p->width < 64 && p->height < 64)
    {
        int x;
        int y;
        int i;
        int j;
        int k;
        int texnum;

        texnum = Scrap_AllocBlock(p->width, p->height, &x, &y);
        scrap_dirty = true;
        k = 0;
        for(i = 0; i < p->height; i++)
        {
            for(j = 0; j < p->width; j++, k++)
            {
                scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] =
                    p->data[k];
            }
        }
        gl.gltexture = scrap_textures[texnum]; // johnfitz -- changed to an
                                               // array
        // johnfitz -- no longer go from 0.01 to 0.99
        gl.sl = x / (float)BLOCK_WIDTH;
        gl.sh = (x + p->width) / (float)BLOCK_WIDTH;
        gl.tl = y / (float)BLOCK_WIDTH;
        gl.th = (y + p->height) / (float)BLOCK_WIDTH;
    }
    else
    {
        char texturename[64]; // johnfitz
        q_snprintf(texturename, sizeof(texturename), "%s:%s", WADFILENAME,
            name); // johnfitz

        offset = (src_offset_t)p - (src_offset_t)wad_base +
                 sizeof(int) * 2; // johnfitz

        gl.gltexture = TexMgr_LoadImage(nullptr, texturename, p->width,
            p->height, SRC_INDEXED, p->data, WADFILENAME, offset,
            (premul_hud ? TEXPREF_PREMULTIPLY : 0) | TEXPREF_ALPHA |
                TEXPREF_PAD | TEXPREF_NOPICMIP); // johnfitz -- TexMgr
        gl.sl = 0;
        gl.sh = (float)p->width /
                (float)TexMgr_PadConditional(p->width); // johnfitz
        gl.tl = 0;
        gl.th = (float)p->height /
                (float)TexMgr_PadConditional(p->height); // johnfitz
    }

    menu_numcachepics++;
    strcpy(pic->name, name);
    pic->pic = *p;
    memcpy(pic->pic.data, &gl, sizeof(glpic_t));

    return &pic->pic;
}

qpic_t* Draw_GetCachedPic(const char* path)
{
    cachepic_t* pic;
    int i;

    for(pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
    {
        if(!strcmp(path, pic->name))
        {
            return &pic->pic;
        }
    }
    return nullptr;
}

/*
================
Draw_CachePic
================
*/
qpic_t* Draw_TryCachePic(const char* path)
{
    cachepic_t* pic;
    int i;
    qpic_t* dat;
    glpic_t gl;

    for(pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
    {
        if(!strcmp(path, pic->name))
        {
            return &pic->pic;
        }
    }
    if(menu_numcachepics == MAX_CACHED_PICS)
    {
        Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
    }
    menu_numcachepics++;
    strcpy(pic->name, path);

    if(strcmp("lmp", COM_FileGetExtension(path)))
    {
        char npath[MAX_QPATH];
        COM_StripExtension(path, npath, sizeof(npath));
        gl.gltexture = TexMgr_LoadImage(nullptr, npath, 0, 0, SRC_EXTERNAL,
            nullptr, npath, 0, TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP);

        pic->pic.width = gl.gltexture->width;
        pic->pic.height = gl.gltexture->height;

        gl.sl = 0;
        gl.sh = (float)pic->pic.width /
                (float)TexMgr_PadConditional(pic->pic.width); // johnfitz
        gl.tl = 0;
        gl.th = (float)pic->pic.height /
                (float)TexMgr_PadConditional(pic->pic.height); // johnfitz
        memcpy(pic->pic.data, &gl, sizeof(glpic_t));

        return &pic->pic;
    }

    //
    // load the pic from disk
    //
    dat = (qpic_t*)COM_LoadTempFile(path, nullptr);
    if(!dat)
    {
        return nullptr;
    }
    SwapPic(dat);

    // HACK HACK HACK --- we need to keep the bytes for
    // the translatable player picture just for the menu
    // configuration dialog
    if(!strcmp(path, "gfx/menuplyr.lmp"))
    {
        memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);
    }

    pic->pic.width = dat->width;
    pic->pic.height = dat->height;

    gl.gltexture = TexMgr_LoadImage(nullptr, path, dat->width, dat->height,
        SRC_INDEXED, dat->data, path, sizeof(int) * 2,
        (premul_hud ? TEXPREF_PREMULTIPLY : 0) | TEXPREF_ALPHA | TEXPREF_PAD |
            TEXPREF_NOPICMIP); // johnfitz -- TexMgr
    gl.sl = 0;
    gl.sh = (float)dat->width /
            (float)TexMgr_PadConditional(dat->width); // johnfitz
    gl.tl = 0;
    gl.th = (float)dat->height /
            (float)TexMgr_PadConditional(dat->height); // johnfitz
    memcpy(pic->pic.data, &gl, sizeof(glpic_t));

    return &pic->pic;
}

qpic_t* Draw_CachePic(const char* path)
{
    qpic_t* pic = Draw_TryCachePic(path);
    if(!pic)
    {
        Sys_Error("Draw_CachePic: failed to load %s", path);
    }
    return pic;
}

/*
================
Draw_MakePic -- johnfitz -- generate pics from internal data
================
*/
qpic_t* Draw_MakePic(const char* name, int width, int height, byte* data)
{
    const int flags = TEXPREF_NEAREST | TEXPREF_ALPHA | TEXPREF_PERSIST |
                      TEXPREF_NOPICMIP | TEXPREF_PAD;

    glpic_t gl;

    qpic_t* pic = (qpic_t*)Hunk_Alloc(sizeof(qpic_t) - 4 + sizeof(glpic_t));
    pic->width = width;
    pic->height = height;

    gl.gltexture = TexMgr_LoadImage(nullptr, name, width, height, SRC_INDEXED,
        data, "", (src_offset_t)data, flags);
    gl.sl = 0;
    gl.sh = (float)width / (float)TexMgr_PadConditional(width);
    gl.tl = 0;
    gl.th = (float)height / (float)TexMgr_PadConditional(height);
    memcpy(pic->data, &gl, sizeof(glpic_t));

    return pic;
}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
===============
Draw_LoadPics -- johnfitz
===============
*/
void Draw_LoadPics()
{
    src_offset_t offset;
    lumpinfo_t* info;
    byte* data = (byte*)W_GetLumpName("conchars", &info);

    // QSS
    if(!data || info->size < 128 * 128)
    {
        Sys_Error("Draw_LoadPics: couldn't load conchars");
    }

    // QSS
    if(info->size != 128 * 128)
    {
        Con_Warning(
            "Invalid size for gfx.wad conchars lump - attempting to ignore for "
            "compat.\n");
    }
    else if(info->type != TYP_MIPTEX)
    {
        Con_DWarning(
            "Invalid type for gfx.wad conchars lump - attempting to ignore for "
            "compat.\n"); // not really a miptex, but certainly NOT a qpic.
    }

    offset = (src_offset_t)data - (src_offset_t)wad_base;
    char_texture = TexMgr_LoadImage(nullptr, WADFILENAME ":conchars", 128, 128,
        SRC_INDEXED, data, WADFILENAME, offset,
        (premul_hud ? TEXPREF_PREMULTIPLY : 0) | TEXPREF_ALPHA |
            TEXPREF_NEAREST | TEXPREF_NOPICMIP | TEXPREF_CONCHARS);

    draw_disc = Draw_PicFromWad("disc");
    draw_backtile = Draw_PicFromWad("backtile");
}

/*
===============
Draw_NewGame -- johnfitz
===============
*/
void Draw_NewGame()
{
    cachepic_t* pic;
    int i;

    // empty scrap and reallocate gltextures
    memset(scrap_allocated, 0, sizeof(scrap_allocated));
    memset(scrap_texels, 255, sizeof(scrap_texels));

    Scrap_Upload(); // creates 2 empty gltextures

    // empty lmp cache
    for(pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
    {
        pic->name[0] = 0;
    }
    menu_numcachepics = 0;

    // reload wad pics
    W_LoadWadFile(); // johnfitz -- filename is now hard-coded for honesty
    Draw_LoadPics();
    SCR_LoadPics();
    Sbar_LoadPics();
    PR_ReloadPics(false);
}

/*
===============
Draw_Init -- johnfitz -- rewritten
===============
*/
void Draw_Init()
{
    Cvar_RegisterVariable(&scr_conalpha);

    // clear scrap and allocate gltextures
    memset(scrap_allocated, 0, sizeof(scrap_allocated));
    memset(scrap_texels, 255, sizeof(scrap_texels));

    Scrap_Upload(); // creates 2 empty textures

    // create internal pics
    pic_ins = Draw_MakePic("ins", 8, 9, &pic_ins_data[0][0]);
    pic_ovr = Draw_MakePic("ovr", 8, 8, &pic_ovr_data[0][0]);
    pic_nul = Draw_MakePic("nul", 8, 8, &pic_nul_data[0][0]);

    // load game pics
    Draw_LoadPics();
}

//==============================================================================
//
//  2D DRAWING
//
//==============================================================================

/*
================
Draw_CharacterQuad -- johnfitz -- seperate function to spit out verts
================
*/
void Draw_CharacterQuad(int x, int y, char num, float scale)
{
    const int row = num >> 4;
    const int col = num & 15;

    const float frow = row * 0.0625;
    const float fcol = col * 0.0625;
    const float size = 0.0625;
    const float inc = 8 * scale;

    glTexCoord2f(fcol, frow);
    glVertex2f(x, y);
    glTexCoord2f(fcol + size, frow);
    glVertex2f(x + inc, y);
    glTexCoord2f(fcol + size, frow + size);
    glVertex2f(x + inc, y + inc);
    glTexCoord2f(fcol, frow + size);
    glVertex2f(x, y + inc);
}

/*
================
Draw_Character -- johnfitz -- modified to call Draw_CharacterQuad
================
*/
void Draw_Character(int x, int y, int num, float scale)
{
    num &= 255;

    if(num == 32)
    {
        return; // don't waste verts on spaces
    }

    GL_Bind(char_texture);
    glBegin(GL_QUADS);

    Draw_CharacterQuad(x, y, (char)num, scale);

    glEnd();
}

/*
================
Draw_String -- johnfitz -- modified to call Draw_CharacterQuad
================
*/
void Draw_String(int x, int y, const char* str, float scale)
{
    if(y <= -8)
    {
        return; // totally off screen
    }

    GL_Bind(char_texture);
    glBegin(GL_QUADS);

    while(*str)
    {
        if(*str != 32)
        {
            // don't waste verts on spaces
            Draw_CharacterQuad(x, y, *str, scale);
        }
        str++;
        x += 8 * scale;
    }

    glEnd();
}

/*
=============
Draw_Pic -- johnfitz -- modified
=============
*/
void Draw_Pic(int x, int y, qpic_t* pic)
{
    glpic_t* gl;

    if(scrap_dirty)
    {
        Scrap_Upload();
    }
    gl = (glpic_t*)pic->data;
    GL_Bind(gl->gltexture);
    glBegin(GL_QUADS);
    glTexCoord2f(gl->sl, gl->tl);
    glVertex2f(x, y);
    glTexCoord2f(gl->sh, gl->tl);
    glVertex2f(x + pic->width, y);
    glTexCoord2f(gl->sh, gl->th);
    glVertex2f(x + pic->width, y + pic->height);
    glTexCoord2f(gl->sl, gl->th);
    glVertex2f(x, y + pic->height);
    glEnd();
}

void Draw_SubPic(float x, float y, float w, float h, qpic_t* pic, float s1,
    float t1, float s2, float t2)
{
    glpic_t* gl;

    s2 += s1;
    t2 += t1;

    if(scrap_dirty)
    {
        Scrap_Upload();
    }
    gl = (glpic_t*)pic->data;
    GL_Bind(gl->gltexture);
    glBegin(GL_QUADS);
    glTexCoord2f(
        gl->sl * (1 - s1) + s1 * gl->sh, gl->tl * (1 - t1) + t1 * gl->th);
    glVertex2f(x, y);
    glTexCoord2f(
        gl->sl * (1 - s2) + s2 * gl->sh, gl->tl * (1 - t1) + t1 * gl->th);
    glVertex2f(x + w, y);
    glTexCoord2f(
        gl->sl * (1 - s2) + s2 * gl->sh, gl->tl * (1 - t2) + t2 * gl->th);
    glVertex2f(x + w, y + h);
    glTexCoord2f(
        gl->sl * (1 - s1) + s1 * gl->sh, gl->tl * (1 - t2) + t2 * gl->th);
    glVertex2f(x, y + h);
    glEnd();
}

// Spike -- this is for CSQC to do fancy drawing.
void Draw_PicPolygon(qpic_t* pic, unsigned int numverts, polygonvert_t* verts)
{
    glpic_t* gl;

    if(scrap_dirty)
    {
        Scrap_Upload();
    }
    gl = (glpic_t*)pic->data;
    GL_Bind(gl->gltexture);
    glBegin(GL_TRIANGLE_FAN);
    while(numverts-- > 0)
    {
        glColor4fv(toGlVec(verts->rgba));
        glTexCoord2f(gl->sl * (1 - verts->st[0]) + verts->st[0] * gl->sh,
            gl->tl * (1 - verts->st[1]) + verts->st[1] * gl->th);
        glVertex2f(verts->xy[0], verts->xy[1]);
        verts++;
    }
    glEnd();
}

/*
=============
Draw_TransPicTranslate -- johnfitz -- rewritten to use texmgr to do translation

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate(int x, int y, qpic_t* pic, int top, int bottom)
{
    static int oldtop = -2;
    static int oldbottom = -2;

    if(top != oldtop || bottom != oldbottom)
    {
        auto* p = (glpic_t*)pic->data;
        gltexture_t* glt = p->gltexture;
        oldtop = top;
        oldbottom = bottom;
        TexMgr_ReloadImage(glt, top, bottom);
    }
    Draw_Pic(x, y, pic);
}

/*
================
Draw_ConsoleBackground -- johnfitz -- rewritten
================
*/
void Draw_ConsoleBackground()
{
    qpic_t* pic;
    float alpha;

    pic = Draw_CachePic("gfx/conback.lmp");
    pic->width = vid.conwidth;
    pic->height = vid.conheight;

    alpha = (con_forcedup) ? 1.0 : scr_conalpha.value;

    GL_SetCanvas(CANVAS_CONSOLE); // in case this is called from weird places

    if(alpha > 0.0)
    {
        if(alpha < 1.0)
        {
            if(premul_hud)
            {
                glColor4f(alpha, alpha, alpha, alpha);
            }
            else
            {
                glEnable(GL_BLEND);
                glDisable(GL_ALPHA_TEST);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

                glColor4f(1, 1, 1, alpha);
            }
        }

        Draw_Pic(0, 0, pic);

        if(alpha < 1.0)
        {
            if(!premul_hud)
            {
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
                glEnable(GL_ALPHA_TEST);
                glDisable(GL_BLEND);
            }
            glColor4f(1, 1, 1, 1);
        }
    }
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear(int x, int y, int w, int h)
{
    glpic_t* gl;

    gl = (glpic_t*)draw_backtile->data;

    glColor3f(1, 1, 1);
    GL_Bind(gl->gltexture);
    glBegin(GL_QUADS);
    glTexCoord2f(x / 64.0, y / 64.0);
    glVertex2f(x, y);
    glTexCoord2f((x + w) / 64.0, y / 64.0);
    glVertex2f(x + w, y);
    glTexCoord2f((x + w) / 64.0, (y + h) / 64.0);
    glVertex2f(x + w, y + h);
    glTexCoord2f(x / 64.0, (y + h) / 64.0);
    glVertex2f(x, y + h);
    glEnd();
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(
    int x, int y, int w, int h, int c, float alpha) // johnfitz -- added alpha
{
    byte* pal = (byte*)
        d_8to24table; // johnfitz -- use d_8to24table instead of host_basepal

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);       // johnfitz -- for alpha
    glDisable(GL_ALPHA_TEST); // johnfitz -- for alpha
    glColor4f(pal[c * 4] / 255.0, pal[c * 4 + 1] / 255.0,
        pal[c * 4 + 2] / 255.0, alpha); // johnfitz -- added alpha

    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();

    glColor3f(1, 1, 1);
    glDisable(GL_BLEND);     // johnfitz -- for alpha
    glEnable(GL_ALPHA_TEST); // johnfitz -- for alpha
    glEnable(GL_TEXTURE_2D);
}

/*
================
Draw_FadeScreen -- johnfitz -- revised
================
*/
void Draw_FadeScreen()
{
    if(vr_enabled.value)
    {
        return;
    }

    GL_SetCanvas(CANVAS_DEFAULT);

    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glColor4f(0, 0, 0, 0.5);
    glBegin(GL_QUADS);

    glVertex2f(0, 0);
    glVertex2f(glwidth, 0);
    glVertex2f(glwidth, glheight);
    glVertex2f(0, glheight);

    glEnd();
    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    Sbar_Changed();
}

/*
================
GL_SetCanvas -- johnfitz -- support various canvas types
================
*/
void GL_SetCanvas(canvastype newcanvas)
{
    extern vrect_t scr_vrect;
    float s;
    int lines;

    if(newcanvas == currentcanvas)
    {
        return;
    }

    currentcanvas = newcanvas;

    if(VR_EnabledAndNotFake() && !con_forcedup)
    {
        return;
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    switch(newcanvas)
    {
        case CANVAS_DEFAULT:
            glOrtho(0, glwidth, glheight, 0, -99999, 99999);
            glViewport(glx, gly, glwidth, glheight);
            break;
        case CANVAS_CONSOLE:
            s = q_min((float)glwidth / 800.0, (float)glheight / 600.0);
            s = CLAMP(1.0, scr_menuscale.value, s);
            lines =
                vid.conheight - (scr_con_current * vid.conheight / glheight);
            glOrtho(0, 800, 600, 0, -99999, 99999);
            glViewport(glx + (glwidth - 800 * s) / 2,
                gly + (glheight - 600 * s) / 2, 800 * s, 600 * s);
            break;
        case CANVAS_NOTIFY:
            lines =
                vid.conheight - (scr_con_current * vid.conheight / glheight);
            glOrtho(
                0, vid.conwidth, vid.conheight + lines, lines, -99999, 99999);
            glViewport(glx, gly, glwidth, glheight);
            break;
        case CANVAS_MENU:
            s = q_min((float)glwidth / 800.0, (float)glheight / 600.0);
            s = CLAMP(1.0, scr_menuscale.value, s);
            // ericw -- doubled width to 640 to accommodate long keybindings
            glOrtho(0, 800, 600, 0, -99999, 99999);
            glViewport(glx + (glwidth - 800 * s) / 2,
                gly + (glheight - 600 * s) / 2, 800 * s, 600 * s);
            break;
        case CANVAS_CSQC:
            s = CLAMP(1.0, scr_sbarscale.value, (float)glwidth / 320.0);
            glOrtho(0, glwidth / s, glheight / s, 0, -99999, 99999);
            glViewport(glx, gly, glwidth, glheight);
            break;
        case CANVAS_SBAR:
            s = CLAMP(1.0, scr_sbarscale.value, (float)glwidth / 320.0);
            if(cl.gametype == GAME_DEATHMATCH)
            {
                glOrtho(0, glwidth / s, 48, 0, -99999, 99999);
                glViewport(glx, gly, glwidth, 48 * s);
            }
            else
            {
                glOrtho(-200, 420, 48, 0, -99999, 99999);
                glViewport(glx + (glwidth - 420 * s) / 2, gly, 600 * s, 48 * s);
            }
            break;
        case CANVAS_WARPIMAGE:
            glOrtho(0, 128, 0, 128, -99999, 99999);
            glViewport(glx, gly + glheight - gl_warpimagesize, gl_warpimagesize,
                gl_warpimagesize);
            break;
        case CANVAS_CROSSHAIR: // 0,0 is center of viewport
            s = CLAMP(1.0, scr_crosshairscale.value, 10.0);
            glOrtho(scr_vrect.width / -2 / s, scr_vrect.width / 2 / s,
                scr_vrect.height / 2 / s, scr_vrect.height / -2 / s, -99999,
                99999);
            glViewport(scr_vrect.x, glheight - scr_vrect.y - scr_vrect.height,
                scr_vrect.width & ~1, scr_vrect.height & ~1);
            break;
        case CANVAS_BOTTOMLEFT:                // used by devstats
            s = (float)glwidth / vid.conwidth; // use console scale
            glOrtho(0, 320, 200, 0, -99999, 99999);
            glViewport(glx, gly, 320 * s, 200 * s);
            break;
        case CANVAS_BOTTOMRIGHT:               // used by fps/clock
            s = (float)glwidth / vid.conwidth; // use console scale
            glOrtho(0, 320, 200, 0, -99999, 99999);
            glViewport(glx + glwidth - 320 * s, gly, 320 * s, 200 * s);
            break;
        case CANVAS_TOPRIGHT: // used by disc
            s = 1;
            glOrtho(0, 320, 200, 0, -99999, 99999);
            glViewport(glx + glwidth - 320 * s, gly + glheight - 200 * s,
                320 * s, 200 * s);
            break;
        default: Sys_Error("GL_SetCanvas: bad canvas type");
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/*
================
GL_Set2D -- johnfitz -- rewritten
================
*/
void GL_Set2D()
{
    currentcanvas = CANVAS_INVALID;
    GL_SetCanvas(CANVAS_DEFAULT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    if(premul_hud)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
    else
    {
        glDisable(GL_BLEND);
        glEnable(GL_ALPHA_TEST);
    }
    glColor4f(1, 1, 1, 1);
}
