/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

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
// cl_parse.c  -- parse a message received from the server

#include "common.hpp"
#include "host.hpp"
#include "quakedef.hpp"
#include "bgmusic.hpp"
#include "vr.hpp"
#include "worldtext.hpp"
#include "cdaudio.hpp"
#include "console.hpp"
#include "quakedef_macros.hpp"
#include "sbar.hpp"
#include "net.hpp"
#include "mathlib.hpp"
#include "glquake.hpp"
#include "protocol.hpp"
#include "msg.hpp"
#include "sys.hpp"
#include "server.hpp"
#include "q_sound.hpp"
#include "screen.hpp"
#include "cmd.hpp"
#include "client.hpp"
#include "snd_voip.hpp"

#include <limits>

const char* svc_strings[128] = {
    "svc_bad", "svc_nop", "svc_disconnect", "svc_updatestat",
    "svc_version",   // [long] server version
    "svc_setview",   // [short] entity number
    "svc_sound",     // <see code>
    "svc_time",      // [float] server time
    "svc_print",     // [string] null terminated string
    "svc_stufftext", // [string] stuffed into client's console buffer
                     // the string should be \n terminated
    "svc_setangle",  // [vec3] set the view angle to this absolute value

    "svc_serverinfo",   // [long] version
                        // [string] signon string
                        // [string]..[0]model cache [string]...[0]sounds cache
                        // [string]..[0]item cache
    "svc_lightstyle",   // [byte] [string]
    "svc_updatename",   // [byte] [string]
    "svc_updatefrags",  // [byte] [short]
    "svc_clientdata",   // <shortbits + data>
    "svc_stopsound",    // <see code>
    "svc_updatecolors", // [byte] [byte]
    "svc_particle",     // [vec3] <variable>
    "svc_damage",       // [byte] impact [byte] blood [vec3] from

    "svc_spawnstatic",
    /*"OBSOLETE svc_spawnbinary"*/ "21 svc_spawnstatic_fte",
    "svc_spawnbaseline",

    "svc_temp_entity", // <variable>
    "svc_setpause", "svc_signonnum", "svc_centerprint", "svc_killedmonster",
    "svc_foundsecret", "svc_spawnstaticsound", "svc_intermission",
    "svc_finale",  // [string] music [string] text
    "svc_cdtrack", // [byte] track [byte] looptrack
    "svc_sellscreen", "svc_cutscene",
    // johnfitz -- new server messages
    "35 svc_worldtext_hsethalign", // 35
    "36",                          // 36
    "svc_skybox_fitz",             // 37					// [string] skyname
    "38",                          // 38
    "39",                          // 39
    "svc_bf_fitz",                 // 40						// no data
    "svc_fog_fitz",                // 41					// [byte] density [byte] red [byte]
                                   // green [byte] blue [float] time
    "svc_spawnbaseline2_fitz",     // 42			// support for large modelindex,
                                   // large framenum, alpha, using flags
    "svc_spawnstatic2_fitz",       // 43			// support for large modelindex,
                                   // large framenum, alpha, using flags
    "svc_spawnstaticsound2_fitz",  //	44		// [coord3] [short] samp [byte]
                                   // vol [byte] aten
    "45 svc_particle2",            // 45
    "46 svc_worldtext_hmake",      // 46
    "47 svc_worldtext_hsettext",   // 47
    "48 svc_worldtext_hsetpos",    // 48
    "49 svc_worldtext_hsetangles", // 49
                                   // johnfitz

    // spike -- particle stuff, and padded to 128 to avoid possible crashes.
    "50 svc_downloaddata_dp",       // 50
    "51 svc_updatestatbyte",        // 51
    "52 svc_effect_dp",             // 52
    "53 svc_effect2_dp",            // 53
    "54 svc_precache",              // 54	//[short] type+idx [string] name
    "55 svc_baseline2_dp",          // 55
    "56 svc_spawnstatic2_dp",       // 56
    "57 svc_entities_dp",           // 57
    "58 svc_csqcentities",          // 58
    "59 svc_spawnstaticsound2_dp",  // 59
    "60 svc_trailparticles",        // 60
    "61 svc_pointparticles",        // 61
    "62 svc_pointparticles1",       // 62
    "63 svc_particle2_fte",         // 63
    "64 svc_particle3_fte",         // 64
    "65 svc_particle4_fte",         // 65
    "66 svc_spawnbaseline_fte",     // 66
    "67 svc_customtempent_fte",     // 67
    "68 svc_selectsplitscreen_fte", // 68
    "69 svc_showpic_fte",           // 69
    "70 svc_hidepic_fte",           // 70
    "71 svc_movepic_fte",           // 71
    "72 svc_updatepic_fte",         // 72
    "73",                           // 73
    "74",                           // 74
    "75",                           // 75
    "76 svc_csqcentities_fte",      // 76
    "77",                           // 77
    "78 svc_updatestatstring_fte",  // 78
    "79 svc_updatestatfloat_fte",   // 79
    "80",                           // 80
    "81",                           // 81
    "82",                           // 82
    "83 svc_cgamepacket_fte",       // 83
    "84 svc_voicechat_fte",         // 84
    "85 svc_setangledelta_fte",     // 85
    "86 svc_updateentities_fte",    // 86
    "87 svc_brushedit_fte",         // 87
    "88 svc_updateseats_fte",       // 88
    "89",                           // 89
    "90",                           // 90
    "91",                           // 91
    "92",                           // 92
    "93",                           // 93
    "94",                           // 94
    "95",                           // 95
    "96",                           // 96
    "97",                           // 97
    "98",                           // 98
    "99",                           // 99
    "100",                          // 100
    "101",                          // 101
    "102",                          // 102
    "103",                          // 103
    "104",                          // 104
    "105",                          // 105
    "106",                          // 106
    "107",                          // 107
    "108",                          // 108
    "109",                          // 109
    "110",                          // 110
    "111",                          // 111
    "112",                          // 112
    "113",                          // 113
    "114",                          // 114
    "115",                          // 115
    "116",                          // 116
    "117",                          // 117
    "118",                          // 118
    "119",                          // 119
    "120",                          // 120
    "121",                          // 121
    "122",                          // 122
    "123",                          // 123
    "124",                          // 124
    "125",                          // 125
    "126",                          // 126
    "127",                          // 127
};

bool warn_about_nehahra_protocol; // johnfitz

extern qvec3 v_punchangles[2]; // johnfitz

//=============================================================================

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
entity_t* CL_EntityNum(int num)
{
    // johnfitz -- check minimum number too
    if(num < 0)
    {
        Host_Error("CL_EntityNum: %i is an invalid number", num);
    }
    // john

    if(num >= cl.num_entities)
    {
        if(num >= cl.max_edicts)
        {
            // johnfitz -- no more MAX_EDICTS
            Host_Error("CL_EntityNum: %i is an invalid number", num);
        }
        while(cl.num_entities <= num)
        {
            cl.entities[cl.num_entities].colormap = vid.colormap;
            cl.entities[cl.num_entities].lerpflags |=
                LERP_RESETMOVE | LERP_RESETANIM; // johnfitz
            cl.num_entities++;
        }
    }

    return &cl.entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket()
{
    int field_mask = MSG_ReadByte();
    int volume;
    float attenuation;

    if(field_mask & SND_VOLUME)
    {
        volume = MSG_ReadByte();
    }
    else
    {
        volume = DEFAULT_SOUND_PACKET_VOLUME;
    }

    if(field_mask & SND_ATTENUATION)
    {
        attenuation = MSG_ReadByte() / 64.0;
    }
    else
    {
        attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    int ent;
    int channel;
    if(field_mask & SND_LARGEENTITY)
    {
        ent = (unsigned short)MSG_ReadShort();
        channel = MSG_ReadByte();
    }
    else
    {
        channel = (unsigned short)MSG_ReadShort();
        ent = channel >> 3;
        channel &= 7;
    }

    int sound_num;
    if(field_mask & SND_LARGESOUND)
    {
        sound_num = (unsigned short)MSG_ReadShort();
    }
    else
    {
        sound_num = MSG_ReadByte();
    }
    // johnfitz

    // johnfitz -- check soundnum
    if(sound_num >= MAX_SOUNDS)
    {
        Host_Error("CL_ParseStartSoundPacket: %i > MAX_SOUNDS", sound_num);
    }
    // johnfitz

    if(ent > cl.max_edicts)
    {
        // johnfitz -- no more MAX_EDICTS
        Host_Error("CL_ParseStartSoundPacket: ent = %i", ent);
    }

    const qvec3 pos = MSG_ReadVec3(cl.protocolflags);

    S_StartSound(ent, channel, cl.sound_precache[sound_num], pos,
        volume / 255.0, attenuation);
}

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
static byte net_olddata[NET_MAXMESSAGE];
void CL_KeepaliveMessage()
{
    float time;
    static float lastmsg;
    int ret;
    sizebuf_t old;
    byte* olddata;

    if(sv.active)
    {
        return; // no need if server is local
    }
    if(cls.demoplayback)
    {
        return;
    }

    // read messages from server, should just be nops
    olddata = net_olddata;
    old = net_message;
    memcpy(olddata, net_message.data, net_message.cursize);

    do
    {
        ret = CL_GetMessage();
        switch(ret)
        {
            default: Host_Error("CL_KeepaliveMessage: CL_GetMessage failed");
            case 0: break; // nothing waiting
            case 1:
                Host_Error("CL_KeepaliveMessage: received a message");
                break;
            case 2:
                if(MSG_ReadByte() != svc_nop)
                {
                    Host_Error("CL_KeepaliveMessage: datagram wasn't a nop");
                }
                break;
        }
    } while(ret);

    net_message = old;
    memcpy(net_message.data, olddata, net_message.cursize);

    // check time
    time = Sys_DoubleTime();
    if(time - lastmsg < 5)
    {
        return;
    }
    lastmsg = time;

    // write out a nop
    Con_Printf("--> client to server keepalive\n");

    MSG_WriteByte(&cls.message, clc_nop);
    NET_SendMessage(cls.netcon, &cls.message);
    SZ_Clear(&cls.message);
}

/*
==================
CL_ParseServerInfo
==================
*/
void CL_ParseServerInfo()
{
    const char* str;
    int i;
    int nummodels;

    int numsounds;
    char model_precache[MAX_MODELS][MAX_QPATH];
    char sound_precache[MAX_SOUNDS][MAX_QPATH];

    Con_DPrintf("Serverinfo packet received.\n");

    // ericw -- bring up loading plaque for map changes within a demo.
    //          it will be hidden in CL_SignonReply.
    if(cls.demoplayback)
    {
        SCR_BeginLoadingPlaque();
    }

    //
    // wipe the client_state_t struct
    //
    CL_ClearState();
    VR_OnClientClearState();

    // parse protocol version number
    i = MSG_ReadLong();
    // johnfitz -- support multiple protocols
    if(i != PROTOCOL_QUAKEVR)
    {
        Con_Printf("\n"); // because there's no newline after serverinfo print
        Host_Error(
            "Server returned protocol %i, not %i\n", i, PROTOCOL_QUAKEVR);
    }
    cl.protocol = i;
    // johnfitz

    cl.protocolflags = 0;

    // parse maxclients
    cl.maxclients = MSG_ReadByte();
    if(cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
    {
        Host_Error("Bad maxclients (%u) from server", cl.maxclients);
    }
    cl.scores = (scoreboard_t*)Hunk_AllocName(
        cl.maxclients * sizeof(*cl.scores), "scores");

    // parse gametype
    cl.gametype = MSG_ReadByte();

    // parse signon message
    str = MSG_ReadString();
    q_strlcpy(cl.levelname, str, sizeof(cl.levelname));

    // seperate the printfs so the server message can have a color
    Con_Printf("\n%s\n", Con_Quakebar(40)); // johnfitz
    Con_Printf("%c%s\n", 2, str);

    // johnfitz -- tell user which protocol this is
    Con_Printf("Using protocol %i\n", i);

    // first we go through and touch all of the precache data that still
    // happens to be in the cache, so precaching something else doesn't
    // needlessly purge it

    // precache models
    memset(cl.model_precache, 0, sizeof(cl.model_precache));
    for(nummodels = 1;; nummodels++)
    {
        str = MSG_ReadString();
        if(!str[0])
        {
            break;
        }
        if(nummodels == MAX_MODELS)
        {
            Host_Error("Server sent too many model precaches");
        }
        q_strlcpy(model_precache[nummodels], str, MAX_QPATH);
        Mod_TouchModel(str);
    }

    // johnfitz -- check for excessive models
    if(nummodels >= 256)
    {
        Con_DWarning("%i models exceeds standard limit of 256 (max = %d).\n",
            nummodels, MAX_MODELS);
    }
    // johnfitz

    // precache sounds
    memset(cl.sound_precache, 0, sizeof(cl.sound_precache));
    for(numsounds = 1;; numsounds++)
    {
        str = MSG_ReadString();
        if(!str[0])
        {
            break;
        }
        if(numsounds == MAX_SOUNDS)
        {
            Host_Error("Server sent too many sound precaches");
        }
        q_strlcpy(sound_precache[numsounds], str, MAX_QPATH);
        S_TouchSound(str);
    }

    // johnfitz -- check for excessive sounds
    if(numsounds >= 256)
    {
        Con_DWarning("%i sounds exceeds standard limit of 256 (max = %d).\n",
            numsounds, MAX_SOUNDS);
    }
    // johnfitz

    //
    // now we try to load everything else until a cache allocation fails
    //

    // copy the naked name of the map file to the cl structure -- O.S
    COM_StripExtension(
        COM_SkipPath(model_precache[1]), cl.mapname, sizeof(cl.mapname));

    for(i = 1; i < nummodels; i++)
    {
        cl.model_precache[i] =
            Mod_ForName_WithFallback(model_precache[i], "progs/player.mdl");
        if(cl.model_precache[i] == nullptr)
        {
            Host_Error("Model %s not found", model_precache[i]);
        }
        CL_KeepaliveMessage();
    }

    for(i = 1; i < numsounds; i++)
    {
        cl.sound_precache[i] = S_PrecacheSound(sound_precache[i]);
        CL_KeepaliveMessage();
    }

    // local state
    cl.entities[0].model = cl.worldmodel = cl.model_precache[1];

    R_NewMap();

    // johnfitz -- clear out string; we don't consider identical
    // messages to be duplicates if the map has changed in between
    con_lastcenterstring[0] = 0;
    // johnfitz

    Hunk_Check(); // make sure nothing is hurt

    noclip_anglehack = false; // noclip is turned off at start

    warn_about_nehahra_protocol =
        true; // johnfitz -- warn about nehahra protocol hack once per server
              // connection

    // johnfitz -- reset developer stats
    memset(&dev_stats, 0, sizeof(dev_stats));
    memset(&dev_peakstats, 0, sizeof(dev_peakstats));
    memset(&dev_overflows, 0, sizeof(dev_overflows));

    S_Voip_MapChange();
}

/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
static void CL_ParseUpdate(int bits)
{
    int i;
    qmodel_t* model;
    unsigned int modnum;
    bool forcelink;
    entity_t* ent;
    int num;
    int skin;

    if(cls.signon == SIGNONS - 1)
    {
        // first update is the final signon stage
        cls.signon = SIGNONS;
        CL_SignonReply();
    }

    if(bits & U_MOREBITS)
    {
        i = MSG_ReadByte();
        bits |= (i << 8);
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(cl.protocol == PROTOCOL_QUAKEVR)
    {
        if(bits & U_EXTEND1)
        {
            bits |= MSG_ReadByte() << 16;
        }
        if(bits & U_EXTEND2)
        {
            bits |= MSG_ReadByte() << 24;
        }
    }
    // johnfitz

    if(bits & U_LONGENTITY)
    {
        num = MSG_ReadShort();
    }
    else
    {
        num = MSG_ReadByte();
    }

    ent = CL_EntityNum(num);

    if(ent->msgtime != cl.mtime[1])
    {
        forcelink = true; // no previous frame to lerp from
    }
    else
    {
        forcelink = false;
    }

    // johnfitz -- lerping
    if(ent->msgtime + 0.2 < cl.mtime[0])
    {
        // more than 0.2 seconds since the last message (most
        // entities think every 0.1 sec)
        ent->lerpflags |= LERP_RESETANIM; // if we missed a think, we'd be
    }
    // lerping from the wrong frame
    // johnfitz

    ent->msgtime = cl.mtime[0];

    if(bits & U_MODEL)
    {
        modnum = MSG_ReadByte();
        if(modnum >= MAX_MODELS)
        {
            Host_Error("CL_ParseModel: bad modnum");
        }
    }
    else
    {
        modnum = ent->baseline.modelindex;
    }

    if(bits & U_FRAME)
    {
        ent->frame = MSG_ReadByte();
    }
    else
    {
        ent->frame = ent->baseline.frame;
    }

    if(bits & U_COLORMAP)
    {
        i = MSG_ReadByte();
    }
    else
    {
        i = ent->baseline.colormap;
    }
    if(!i)
    {
        ent->colormap = vid.colormap;
    }
    else
    {
        if(i > cl.maxclients)
        {
            Sys_Error("i >= cl.maxclients");
        }
        ent->colormap = cl.scores[i - 1].translations;
    }
    if(bits & U_SKIN)
    {
        skin = MSG_ReadByte();
    }
    else
    {
        skin = ent->baseline.skin;
    }
    if(skin != ent->skinnum)
    {
        ent->skinnum = skin;
        if(num > 0 && num <= cl.maxclients)
        {
            R_TranslateNewPlayerSkin(
                num - 1); // johnfitz -- was R_TranslatePlayerSkin
        }
    }
    if(bits & U_EFFECTS)
    {
        ent->effects = MSG_ReadByte();
    }
    else
    {
        ent->effects = ent->baseline.effects;
    }

    // shift the known values for interpolation
    ent->msg_origins[1] = ent->msg_origins[0];
    ent->msg_angles[1] = ent->msg_angles[0];
    ent->msg_scales[1] = ent->msg_scales[0];


    const auto doIt = [&](const auto fn, const int bit, auto& target,
                          const auto& baselineData, const int index) {
        if(bits & bit)
        {
            target[index] = fn(cl.protocolflags);
        }
        else
        {
            target[index] = baselineData[index];
        }
    };

    // clang-format off
    doIt(&MSG_ReadCoord, U_ORIGIN1, ent->msg_origins[0], ent->baseline.origin, 0);
    doIt(&MSG_ReadAngle, U_ANGLE1, ent->msg_angles[0], ent->baseline.angles, 0);
    doIt(&MSG_ReadCoord, U_SCALE, ent->msg_scales[0], ent->baseline.scale, 0);

    doIt(&MSG_ReadCoord, U_ORIGIN2, ent->msg_origins[0], ent->baseline.origin, 1);
    doIt(&MSG_ReadAngle, U_ANGLE2, ent->msg_angles[0], ent->baseline.angles, 1);
    doIt(&MSG_ReadCoord, U_SCALE, ent->msg_scales[0], ent->baseline.scale, 1);

    doIt(&MSG_ReadCoord, U_ORIGIN3, ent->msg_origins[0], ent->baseline.origin, 2);
    doIt(&MSG_ReadAngle, U_ANGLE3, ent->msg_angles[0], ent->baseline.angles, 2);
    doIt(&MSG_ReadCoord, U_SCALE, ent->msg_scales[0], ent->baseline.scale, 2);
    // clang-format on

    if(bits & U_SCALE)
    {
        ent->scale_origin = MSG_ReadVec3(cl.protocolflags);
    }

    // johnfitz -- lerping for movetype_step entities
    if(bits & U_STEP)
    {
        ent->lerpflags |= LERP_MOVESTEP;
        ent->forcelink = true;
    }
    else
    {
        ent->lerpflags &= ~LERP_MOVESTEP;
    }
    // johnfitz

    // johnfitz -- PROTOCOL_QUAKEVR
    if(cl.protocol == PROTOCOL_QUAKEVR)
    {
        if(bits & U_ALPHA)
        {
            ent->alpha = MSG_ReadByte();
        }
        else
        {
            ent->alpha = ent->baseline.alpha;
        }

        if(bits & U_FRAME2)
        {
            ent->frame = (ent->frame & 0x00FF) | (MSG_ReadByte() << 8);
        }

        if(bits & U_MODEL2)
        {
            modnum = (modnum & 0x00FF) | (MSG_ReadByte() << 8);
        }

        if(bits & U_LERPFINISH)
        {
            ent->lerpfinish = ent->msgtime + ((float)(MSG_ReadByte()) / 255);
            ent->lerpflags |= LERP_FINISH;
        }
        else
        {
            ent->lerpflags &= ~LERP_FINISH;
        }
    }
    // johnfitz

    // johnfitz -- moved here from above
    model = cl.model_precache[modnum];
    if(model != ent->model)
    {
        ent->model = model;
        // automatic animation (torches, etc) can be either all together
        // or randomized
        if(model)
        {
            if(model->synctype == ST_RAND)
            {
                ent->syncbase = (float)(rand() & 0x7fff) / 0x7fff;
            }
            else
            {
                ent->syncbase = 0.0;
            }
        }
        else
        {
            forcelink = true; // hack to make null model players work
        }
        if(num > 0 && num <= cl.maxclients)
        {
            R_TranslateNewPlayerSkin(
                num - 1); // johnfitz -- was R_TranslatePlayerSkin
        }

        ent->lerpflags |= LERP_RESETANIM; // johnfitz -- don't lerp animation
                                          // across model changes
    }
    // johnfitz

    if(forcelink)
    {
        // didn't have an update last message
        ent->msg_origins[1] = ent->msg_origins[0];
        ent->origin = ent->msg_origins[0];
        ent->msg_angles[1] = ent->msg_angles[0];
        ent->angles = ent->msg_angles[0];
        ent->msg_scales[1] = ent->msg_scales[0];
        ent->scale = ent->msg_scales[0];
        ent->forcelink = true;
    }
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline(entity_t* ent, int version) // johnfitz -- added argument
{
    int i;
    int bits; // johnfitz

    // johnfitz -- PROTOCOL_QUAKEVR
    bits = (version == 2) ? MSG_ReadByte() : 0;
    ent->baseline.modelindex =
        (bits & B_LARGEMODEL) ? MSG_ReadShort() : MSG_ReadByte();
    ent->baseline.frame =
        (bits & B_LARGEFRAME) ? MSG_ReadShort() : MSG_ReadByte();
    // johnfitz

    ent->baseline.colormap = MSG_ReadByte();
    ent->baseline.skin = MSG_ReadByte();
    for(i = 0; i < 3; i++)
    {
        ent->baseline.origin[i] = MSG_ReadCoord(cl.protocolflags);
        ent->baseline.angles[i] = MSG_ReadAngle(cl.protocolflags);
    }

    ent->baseline.alpha =
        (bits & B_ALPHA) ? MSG_ReadByte()
                         : ENTALPHA_DEFAULT; // johnfitz -- PROTOCOL_QUAKEVR
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
void CL_ParseClientdata()
{
    int bits =
        (unsigned int)MSG_ReadLong(); // johnfitz -- read bits here isntead
                                      // of in CL_ParseServerMessage()

    // johnfitz -- PROTOCOL_QUAKEVR
    if(bits & SU_EXTEND1)
    {
        bits |= (MSG_ReadByte() << 16);
    }
    if(bits & SU_EXTEND2)
    {
        bits |= (MSG_ReadByte() << 24);
    }
    // johnfitz

    if(bits & SU_VIEWHEIGHT)
    {
        cl.viewheight = MSG_ReadChar();
    }
    else
    {
        cl.viewheight = DEFAULT_VIEWHEIGHT;
    }

    if(bits & SU_IDEALPITCH)
    {
        cl.idealpitch = MSG_ReadChar();
    }
    else
    {
        cl.idealpitch = 0;
    }

    cl.mvelocity[1] = cl.mvelocity[0];
    for(int i = 0; i < 3; i++)
    {
        if(bits & (SU_PUNCH1 << i))
        {
            cl.punchangle[i] = MSG_ReadChar();
        }
        else
        {
            cl.punchangle[i] = 0;
        }

        if(bits & (SU_VELOCITY1 << i))
        {
            cl.mvelocity[0][i] = MSG_ReadChar() * 16;
        }
        else
        {
            cl.mvelocity[0][i] = 0;
        }
    }

    // johnfitz -- update v_punchangles
    if(v_punchangles[0][0] != cl.punchangle[0] ||
        v_punchangles[0][1] != cl.punchangle[1] ||
        v_punchangles[0][2] != cl.punchangle[2])
    {
        v_punchangles[1] = v_punchangles[0];
        v_punchangles[0] = cl.punchangle;
    }
    // johnfitz

    // [always sent]	if (bits & SU_ITEMS)
    int i = MSG_ReadLong();

    // TODO VR: (P1) should we send other item flags as well?
    if(cl.items != i)
    {
        // set flash times
        Sbar_Changed();
        for(int j = 0; j < 32; j++)
        {
            if((i & (1 << j)) && !(cl.items & (1 << j)))
            {
                cl.item_gettime[j] = cl.time;
            }
        }
        cl.items = i;
    }

    cl.onground = (bits & SU_ONGROUND) != 0;
    cl.inwater = (bits & SU_INWATER) != 0;

    if(bits & SU_WEAPONFRAME)
    {
        cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte();
    }
    else
    {
        cl.stats[STAT_WEAPONFRAME] = 0;
    }

    if(bits & SU_ARMOR)
    {
        i = MSG_ReadByte();
    }
    else
    {
        i = 0;
    }
    if(cl.stats[STAT_ARMOR] != i)
    {
        cl.stats[STAT_ARMOR] = i;
        Sbar_Changed();
    }

    if(bits & SU_WEAPON)
    {
        i = MSG_ReadByte();
    }
    else
    {
        i = 0;
    }
    if(cl.stats[STAT_WEAPON] != i)
    {
        cl.stats[STAT_WEAPON] = i;
        Sbar_Changed();
    }

    i = MSG_ReadShort();
    if(cl.stats[STAT_HEALTH] != i)
    {
        cl.stats[STAT_HEALTH] = i;
        Sbar_Changed();
    }

    i = MSG_ReadByte();
    if(cl.stats[STAT_AMMO] != i)
    {
        cl.stats[STAT_AMMO] = i;
        Sbar_Changed();
    }

    i = MSG_ReadByte();
    if(cl.stats[STAT_AMMO2] != i)
    {
        cl.stats[STAT_AMMO2] = i;
        Sbar_Changed();
    }

    i = MSG_ReadShort();
    if(cl.stats[STAT_AMMOCOUNTER] != i)
    {
        cl.stats[STAT_AMMOCOUNTER] = i;
        Sbar_Changed();
    }

    i = MSG_ReadShort();
    if(cl.stats[STAT_AMMOCOUNTER2] != i)
    {
        cl.stats[STAT_AMMOCOUNTER2] = i;
        Sbar_Changed();
    }

    for(int i = 0; i < 4; i++)
    {
        int j = MSG_ReadByte();
        if(cl.stats[STAT_SHELLS + i] != j)
        {
            cl.stats[STAT_SHELLS + i] = j;
            Sbar_Changed();
        }
    }

    i = MSG_ReadByte();

    if(standard_quake)
    {
        if(cl.stats[STAT_ACTIVEWEAPON] != i)
        {
            cl.stats[STAT_ACTIVEWEAPON] = i;
            Sbar_Changed();
        }
    }
    else
    {
        if(cl.stats[STAT_ACTIVEWEAPON] != (1 << i))
        {
            cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
            Sbar_Changed();
        }
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(bits & SU_WEAPON2)
    {
        cl.stats[STAT_WEAPON] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_ARMOR2)
    {
        cl.stats[STAT_ARMOR] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_AMMO2)
    {
        cl.stats[STAT_AMMO] |= (MSG_ReadByte() << 8);
        cl.stats[STAT_AMMO2] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_SHELLS2)
    {
        cl.stats[STAT_SHELLS] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_NAILS2)
    {
        cl.stats[STAT_NAILS] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_ROCKETS2)
    {
        cl.stats[STAT_ROCKETS] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_CELLS2)
    {
        cl.stats[STAT_CELLS] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_WEAPONFRAME2)
    {
        cl.stats[STAT_WEAPONFRAME] |= (MSG_ReadByte() << 8);
    }
    if(bits & SU_WEAPONALPHA)
    {
        cl.viewent.alpha = MSG_ReadByte();
    }
    else
    {
        cl.viewent.alpha = ENTALPHA_DEFAULT;
    }
    // johnfitz

    // TODO VR: (P2) do we need all these bits?
    if(bits & SU_VR_WEAPON2)
    {
        cl.stats[STAT_WEAPON2] = MSG_ReadByte();
        cl.stats[STAT_WEAPONMODEL2] = MSG_ReadByte();
    }
    else
    {
        cl.stats[STAT_WEAPON2] = 0;
        cl.stats[STAT_WEAPONMODEL2] = 0;
    }

    if(bits & SU_VR_WEAPONFRAME2)
    {
        cl.stats[STAT_WEAPONFRAME2] = MSG_ReadByte();
    }
    else
    {
        cl.stats[STAT_WEAPONFRAME2] = 0;
    }

    // TODO VR: (P2) weapon ids in holsters - not sure what this todo is
    // checking, need to check what is being sent. I think model strings are
    // being sent and used as keys...
    if(bits & SU_VR_HOLSTERS)
    {
        cl.stats[STAT_HOLSTERWEAPON0] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPON1] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPON2] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPON3] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPON4] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPON5] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL0] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL1] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL2] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL3] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL4] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONMODEL5] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS0] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS1] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS2] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS3] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS4] = MSG_ReadByte();
        cl.stats[STAT_HOLSTERWEAPONFLAGS5] = MSG_ReadByte();
    }

    // TODO VR: (P2) some data is sent twice, can optimize for MP
    cl.stats[STAT_MAINHAND_WID] = MSG_ReadByte();
    cl.stats[STAT_OFFHAND_WID] = MSG_ReadByte();
    cl.stats[STAT_WEAPONFLAGS] = MSG_ReadByte();
    cl.stats[STAT_WEAPONFLAGS2] = MSG_ReadByte();

    // johnfitz -- lerping
    // ericw -- this was done before the upper 8 bits of
    // cl.stats[STAT_WEAPON] were filled in, breaking on large maps like
    // zendar.bsp
    if(cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]])
    {
        cl.viewent.lerpflags |=
            LERP_RESETANIM; // don't lerp animation across model changes
    }

    if(cl.offhand_viewent.model !=
        cl.model_precache[cl.stats[STAT_WEAPONMODEL2]])
    {
        cl.offhand_viewent.lerpflags |=
            LERP_RESETANIM; // don't lerp animation across model changes
    }
    // johnfitz
}

/*
=====================
CL_NewTranslation
=====================
*/
void CL_NewTranslation(int slot)
{
    int i;
    int j;
    int top;
    int bottom;
    byte* dest;
    byte* source;

    if(slot > cl.maxclients)
    {
        Sys_Error("CL_NewTranslation: slot > cl.maxclients");
    }
    dest = cl.scores[slot].translations;
    source = vid.colormap;
    memcpy(dest, vid.colormap, sizeof(cl.scores[slot].translations));
    top = cl.scores[slot].colors & 0xf0;
    bottom = (cl.scores[slot].colors & 15) << 4;
    R_TranslatePlayerSkin(slot);

    for(i = 0; i < VID_GRADES; i++, dest += 256, source += 256)
    {
        if(top < 128)
        {
            // the artists made some backwards ranges.  sigh.
            memcpy(dest + TOP_RANGE, source + top, 16);
        }
        else
        {
            for(j = 0; j < 16; j++)
            {
                dest[TOP_RANGE + j] = source[top + 15 - j];
            }
        }

        if(bottom < 128)
        {
            memcpy(dest + BOTTOM_RANGE, source + bottom, 16);
        }
        else
        {
            for(j = 0; j < 16; j++)
            {
                dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
            }
        }
    }
}

/*
=====================
CL_ParseStatic
=====================
*/
void CL_ParseStatic(int version) // johnfitz -- added a parameter
{
    entity_t* ent;
    int i;

    i = cl.num_statics;
    if(i >= cl.max_static_entities)
    {
        int ec = 64;
        entity_t** newstatics = (entity_t**)realloc(cl.static_entities,
            sizeof(*newstatics) * (cl.max_static_entities + ec));
        entity_t* newents = (entity_t*)Hunk_Alloc(sizeof(*newents) * ec);
        if(!newstatics || !newents) Host_Error("Too many static entities");
        cl.static_entities = newstatics;
        while(ec--) cl.static_entities[cl.max_static_entities++] = newents++;
    }

    ent = cl.static_entities[i];
    cl.num_statics++;
    CL_ParseBaseline(ent, version); // johnfitz -- added second parameter

    // copy it to the current state

    ent->model = cl.model_precache[ent->baseline.modelindex];
    ent->lerpflags |= LERP_RESETANIM; // johnfitz -- lerping
    ent->frame = ent->baseline.frame;

    ent->colormap = vid.colormap;
    ent->skinnum = ent->baseline.skin;
    ent->effects = ent->baseline.effects;
    ent->alpha = ent->baseline.alpha; // johnfitz -- alpha

    ent->origin = ent->baseline.origin;
    ent->angles = ent->baseline.angles;
    R_AddEfrags(ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
void CL_ParseStaticSound(int version) // johnfitz -- added argument
{
    qvec3 org = MSG_ReadVec3(cl.protocolflags);
    int sound_num;
    int vol;
    int atten;

    // johnfitz -- PROTOCOL_QUAKEVR
    if(version == 2)
    {
        sound_num = MSG_ReadShort();
    }
    else
    {
        sound_num = MSG_ReadByte();
    }
    // johnfitz

    vol = MSG_ReadByte();
    atten = MSG_ReadByte();

    S_StaticSound(cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x) \
    if(cl_shownet.value == 2) Con_Printf("%3i:%s\n", msg_readcount - 1, x);

// mods and servers might not send the \n instantly.
// some mods bug out and omit the \n entirely, this function helps prevent the
// damage from spreading too much. some servers or mods use //prefixed commands
// as extensions to avoid spam about unrecognised commands. proquake has its own
// extension coding thing.
static void CL_ParseStuffText(const char* msg)
{
    const char* str;
    q_strlcat(cl.stuffcmdbuf, msg, sizeof(cl.stuffcmdbuf));
    while((str = strchr(cl.stuffcmdbuf, '\n')))
    {
        bool handled;
        str++; // skip past the \n

        // handle //prefixed commands
        if(cl.stuffcmdbuf[0] == '/' && cl.stuffcmdbuf[1] == '/')
        {
            handled = Cmd_ExecuteString(cl.stuffcmdbuf + 2, src_server);
            if(!handled)
            {
                Con_DPrintf("Server sent unknown command %s\n", Cmd_Argv(1));
            }
        }
        else
        {
            handled = Cmd_ExecuteString(cl.stuffcmdbuf, src_server);
        }
        if(!handled)
        {
            Cbuf_AddTextLen(cl.stuffcmdbuf, str - cl.stuffcmdbuf);
        }
        memmove(cl.stuffcmdbuf, str, Q_strlen(str) + 1);
    }
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage()
{
    int cmd;
    int i;
    const char* str; // johnfitz
    int total;
    int j;
    int lastcmd; // johnfitz

    //
    // if recording demos, copy the message out
    //
    if(cl_shownet.value == 1)
    {
        Con_Printf("%i ", net_message.cursize);
    }
    else if(cl_shownet.value == 2)
    {
        Con_Printf("------------------\n");
    }

    cl.onground = false; // unless the server says otherwise
                         //
                         // parse the message
                         //
    MSG_BeginReading();

    lastcmd = 0;
    while(true)
    {
        if(msg_badread)
        {
            Host_Error("CL_ParseServerMessage: Bad server message");
        }

        cmd = MSG_ReadByte();

        if(cmd == -1)
        {
            SHOWNET("END OF MESSAGE");
            return; // end of message
        }

        // if the high bit of the command byte is set, it is a fast update
        if(cmd & U_SIGNAL) // johnfitz -- was 128, changed for clarity
        {
            SHOWNET("fast update");
            CL_ParseUpdate(cmd & 127);
            continue;
        }

        SHOWNET(svc_strings[cmd]);

        // other commands
        switch(cmd)
        {
            default:
                Host_Error("Illegible server message %s, previous was %s",
                    svc_strings[cmd],
                    svc_strings[lastcmd]); // johnfitz -- added
                                           // svc_strings[lastcmd]
                break;

            case svc_nop:
                //	Con_Printf ("svc_nop\n");
                break;

            case svc_time:
                cl.mtime[1] = cl.mtime[0];
                cl.mtime[0] = MSG_ReadFloat();
                break;

            case svc_clientdata:
                CL_ParseClientdata(); // johnfitz -- removed bits parameter,
                                      // we will read this inside
                                      // CL_ParseClientdata()
                break;

            case svc_version:
                i = MSG_ReadLong();
                // johnfitz -- support multiple protocols
                if(i != PROTOCOL_QUAKEVR)
                {
                    Host_Error("Server returned protocol %i, not %i\n", i,
                        PROTOCOL_QUAKEVR);
                }
                cl.protocol = i;
                // johnfitz
                break;

            case svc_disconnect: Host_EndGame("Server disconnected\n");

            case svc_print: Con_Printf("%s", MSG_ReadString()); break;

            case svc_centerprint:
                // johnfitz -- log centerprints to console
                str = MSG_ReadString();
                SCR_CenterPrint(str);
                Con_LogCenterPrint(str);
                // johnfitz
                break;

            case svc_stufftext: CL_ParseStuffText(MSG_ReadString()); break;

            case svc_damage: V_ParseDamage(); break;

            case svc_serverinfo:
                CL_ParseServerInfo();
                vid.recalc_refdef = true; // leave intermission full screen
                break;

            case svc_setangle:
                for(i = 0; i < 3; i++)
                {
                    cl.viewangles[i] = MSG_ReadAngle(cl.protocolflags);
                }
                VR_SetAngles(cl.viewangles);
                break;

            case svc_setview:
                cl.viewentity = MSG_ReadShort();
                VR_PushYaw();
                break;

            case svc_lightstyle:
                i = MSG_ReadByte();
                if(i >= MAX_LIGHTSTYLES)
                {
                    Sys_Error("svc_lightstyle > MAX_LIGHTSTYLES");
                }
                q_strlcpy(
                    cl_lightstyle[i].map, MSG_ReadString(), MAX_STYLESTRING);
                cl_lightstyle[i].length = Q_strlen(cl_lightstyle[i].map);
                // johnfitz -- save extra info
                if(cl_lightstyle[i].length)
                {
                    total = 0;
                    cl_lightstyle[i].peak = 'a';
                    for(j = 0; j < cl_lightstyle[i].length; j++)
                    {
                        total += cl_lightstyle[i].map[j] - 'a';
                        cl_lightstyle[i].peak = q_max(
                            cl_lightstyle[i].peak, cl_lightstyle[i].map[j]);
                    }
                    cl_lightstyle[i].average =
                        total / cl_lightstyle[i].length + 'a';
                }
                else
                {
                    cl_lightstyle[i].average = cl_lightstyle[i].peak = 'm';
                }
                // johnfitz
                break;

            case svc_sound: CL_ParseStartSoundPacket(); break;

            case svc_stopsound:
                i = MSG_ReadShort();
                S_StopSound(i >> 3, i & 7);
                break;

            case svc_updatename:
                Sbar_Changed();
                i = MSG_ReadByte();
                if(i >= cl.maxclients)
                {
                    Host_Error(
                        "CL_ParseServerMessage: svc_updatename > "
                        "MAX_SCOREBOARD");
                }
                q_strlcpy(
                    cl.scores[i].name, MSG_ReadString(), MAX_SCOREBOARDNAME);
                break;

            case svc_updatefrags:
                Sbar_Changed();
                i = MSG_ReadByte();
                if(i >= cl.maxclients)
                {
                    Host_Error(
                        "CL_ParseServerMessage: svc_updatefrags > "
                        "MAX_SCOREBOARD");
                }
                cl.scores[i].frags = MSG_ReadShort();
                break;

            case svc_updatecolors:
                Sbar_Changed();
                i = MSG_ReadByte();
                if(i >= cl.maxclients)
                {
                    Host_Error(
                        "CL_ParseServerMessage: svc_updatecolors > "
                        "MAX_SCOREBOARD");
                }
                cl.scores[i].colors = MSG_ReadByte();
                CL_NewTranslation(i);
                break;

            case svc_particle: R_ParseParticleEffect(); break;
            case svc_particle2: R_ParseParticle2Effect(); break;

            case svc_spawnbaseline:
                i = MSG_ReadShort();
                // must use CL_EntityNum() to force cl.num_entities up
                CL_ParseBaseline(CL_EntityNum(i),
                    1); // johnfitz -- added second parameter
                break;

            case svc_spawnstatic:
                CL_ParseStatic(1); // johnfitz -- added parameter
                break;

            case svc_temp_entity: CL_ParseTEnt(); break;

            case svc_setpause:
                cl.paused = MSG_ReadByte();
                if(cl.paused)
                {
                    CDAudio_Pause();
                    BGM_Pause();
                }
                else
                {
                    CDAudio_Resume();
                    BGM_Resume();
                }
                break;

            case svc_signonnum:
                i = MSG_ReadByte();
                if(i <= cls.signon)
                {
                    Host_Error("Received signon %i when at %i", i, cls.signon);
                }
                cls.signon = i;
                // johnfitz -- if signonnum==2, signon packet has been fully
                // parsed, so check for excessive static ents and efrags
                if(i == 2)
                {
                    if(cl.num_statics > 128)
                    {
                        Con_DWarning(
                            "%i static entities exceeds standard limit of "
                            "128 "
                            "(max = %d).\n",
                            cl.num_statics, MAX_STATIC_ENTITIES);
                    }
                    R_CheckEfrags();
                }
                // johnfitz
                CL_SignonReply();
                break;

            case svc_killedmonster: cl.stats[STAT_MONSTERS]++; break;

            case svc_foundsecret: cl.stats[STAT_SECRETS]++; break;

            case svc_updatestat:
                i = MSG_ReadByte();
                if(i < 0 || i >= MAX_CL_STATS)
                {
                    Sys_Error("svc_updatestat: %i is invalid", i);
                }
                cl.stats[i] = MSG_ReadLong();
                ;
                break;

            case svc_spawnstaticsound:
                CL_ParseStaticSound(1); // johnfitz -- added parameter
                break;

            case svc_cdtrack:
                cl.cdtrack = MSG_ReadByte();
                cl.looptrack = MSG_ReadByte();
                if((cls.demoplayback || cls.demorecording) &&
                    (cls.forcetrack != -1))
                {
                    BGM_PlayCDtrack((byte)cls.forcetrack, true);
                }
                else
                {
                    BGM_PlayCDtrack((byte)cl.cdtrack, true);
                }
                break;

            case svc_intermission:
                cl.intermission = 1;
                cl.completed_time = cl.time;
                vid.recalc_refdef = true; // go to full screen
                break;

            case svc_finale:
                cl.intermission = 2;
                cl.completed_time = cl.time;
                vid.recalc_refdef = true; // go to full screen
                // johnfitz -- log centerprints to console
                str = MSG_ReadString();
                SCR_CenterPrint(str);
                Con_LogCenterPrint(str);
                // johnfitz
                break;

            case svc_cutscene:
                cl.intermission = 3;
                cl.completed_time = cl.time;
                vid.recalc_refdef = true; // go to full screen
                // johnfitz -- log centerprints to console
                str = MSG_ReadString();
                SCR_CenterPrint(str);
                Con_LogCenterPrint(str);
                // johnfitz
                break;

            case svc_sellscreen: Cmd_ExecuteString("help", src_command); break;

            // johnfitz -- new svc types
            case svc_skybox: Sky_LoadSkyBox(MSG_ReadString()); break;

            case svc_bf: Cmd_ExecuteString("bf", src_command); break;

            case svc_fog: Fog_ParseServerMessage(); break;

            case svc_spawnbaseline2: // PROTOCOL_QUAKEVR
                i = MSG_ReadShort();
                // must use CL_EntityNum() to force cl.num_entities up
                CL_ParseBaseline(CL_EntityNum(i), 2);
                break;

            case svc_spawnstatic2: // PROTOCOL_QUAKEVR
                CL_ParseStatic(2);
                break;

            case svc_spawnstaticsound2: // PROTOCOL_QUAKEVR
                CL_ParseStaticSound(2);
                break;
                // johnfitz

            case svc_worldtext_hmake:
            {
                cl.OnMsg_WorldTextHMake();
                break;
            }

            case svc_worldtext_hsettext:
            {
                cl.OnMsg_WorldTextHSetText();
                break;
            }

            case svc_worldtext_hsetpos:
            {
                cl.OnMsg_WorldTextHSetPos();
                break;
            }

            case svc_worldtext_hsetangles:
            {
                cl.OnMsg_WorldTextHSetAngles();
                break;
            }

            case svc_worldtext_hsethalign:
            {
                cl.OnMsg_WorldTextHSetHAlign();
                break;
            }

            // voicechat, because we can. why reduce packet sizes if you're not
            // going to use that extra space?!?
            case svcfte_voicechat:
            {
                S_Voip_Parse();
                break;
            }
                // spike
        }

        lastcmd = cmd; // johnfitz
    }
}
