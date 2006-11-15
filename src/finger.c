/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
*	ROM 2.4 is copyright 1993-1995 Russ Taylor			   *
*	ROM has been brought to you by the ROM consortium		   *
*	    Russ Taylor (rtaylor@pacinfo.com)				   *
*	    Gabrielle Taylor (gtaylor@pacinfo.com)			   *
*	    Brian Moore (rom@rom.efn.org)				   *
*	By using this code, you have agreed to follow the terms of the	   *
*	ROM license, in the file Rom24/doc/rom.license			   *
***************************************************************************/

/*************************************************************************** 
*       ROT 1.4 is copyright 1996-1997 by Russ Walsh                       * 
*       By using this code, you have agreed to follow the terms of the     * 
*       ROT license, in the file doc/rot.license                           * 
***************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "merc.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "lookup.h"

#define MAX_NEST	100
static OBJ_DATA *rgObjNest[MAX_NEST];

CH_CMD ( do_finger )
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    CHAR_DATA *victim;
    FILE *fp;
    bool fOld;

    one_argument ( argument, arg );

    if ( arg[0] == '\0' )
    {
        send_to_char ( "Finger whom?\n\r", ch );
        return;
    }

    victim = new_char (  );
    victim->pcdata = new_pcdata (  );
    fOld = false;
//    sprintf ( buf, "gzip -dfq %s%s.gz", PLAYER_DIR, capitalize ( arg ) );
//    system ( buf );
    sprintf ( buf, "%s%s", PLAYER_DIR, capitalize ( arg ) );
    if ( ( fp = file_open ( buf, "r" ) ) != NULL )
    {
        int iNest;

        for ( iNest = 0; iNest < MAX_NEST; iNest++ )
            rgObjNest[iNest] = NULL;

        fOld = true;
        for ( ;; )
        {
            char letter;
            char *word;

            letter = fread_letter ( fp );
            if ( letter == '*' )
            {
                fread_to_eol ( fp );
                continue;
            }

            if ( letter != '#' )
            {
                bug ( "Load_char_obj: # not found.", 0 );
                break;
            }

            word = fread_word ( fp );
            if ( !str_cmp ( word, "PLAYER" ) )
                fread_char ( victim, fp );
            else if ( !str_cmp ( word, "OBJECT" ) )
                break;
            else if ( !str_cmp ( word, "O" ) )
                break;
            else if ( !str_cmp ( word, "PET" ) )
                break;
            else if ( !str_cmp ( word, "END" ) )
                break;
            else
            {
                bug ( "Load_char_obj: bad section.", 0 );
                break;
            }
        }
        file_close ( fp );
//        sprintf ( buf, "gzip -fq %s%s", PLAYER_DIR, capitalize ( arg ) );
//        system ( buf );
//        sprintf ( buf, "rm -f %s", capitalize ( arg ) );
//        system ( buf );
    }
    if ( !fOld )
    {
        send_to_char ( "No player by that name exists.\n\r", ch );
        free_pcdata ( victim->pcdata );
        free_char ( victim );
        return;
    }
    if ( ( victim->level > LEVEL_ANCIENT ) && ( victim->level > ch->level ) )
    {
        send_to_char ( "The gods wouldn't like that.\n\r", ch );
        free_pcdata ( victim->pcdata );
        free_char ( victim );
        return;
    }

/*  OLD PRINT
    printf_to_char ( ch,
                     "{Y====================\n\r" "{Y|{cName{x :{g %-12s{x"
                     "{cLevel{x:{g %-12d{x\n\r" "{Y|{cClass{x:{g %-12s{x"
                     "{cRace{x :{g %-12s{x\n\r" "{Y|{cClan{x :{g %-12s{x"
                     "{cRank{x :{g %-12s{x\n\r"
                     "{Y|{cPK{x   :{g %-12ld{x"
                     "{cPD{x   :{g %-12ld{x\n\r"
                     "{Y|{cAWin{x :{g %-12d{x" "{cAloss{x:{g %-12d{x\n\r"
		     "{Y|{cHours{x:{g %-12d{x" "{cRPs Total/Current {x: {g%-6d / %-6d{x\n\r"
		     "{Y=====================\n\r" "Last Logged off %s{x\r",
                     victim->name, victim->level,
                     class_table[victim->class].name,
                     pc_race_table[victim->race].name,
                     clan_table[victim->clan].exname,
                     clan_rank_table[victim->clan_rank].title_of_rank[victim->sex],
                     victim->pcdata->pkkills, victim->pcdata->pkdeaths,
                     victim->pcdata->awins, victim->pcdata->alosses,
		     ( ( int ) ( victim->played + current_time - victim->logon ) / 3600 ),
                     victim->rpst, victim->rps,
                     ( char * ) ctime ( &victim->llogoff ) );
*/

    printf_to_char ( ch,
                     "{W( {+P{+L{+A{+Y{+E{+R {+I{+N{+F{+O{+R{+M{+A{+T{+I{+O{+N{W: {R%-10s"
                     " {x%s {W)\n\r" 
                     "  {CL{cevel {W: {g%-9d" "{CR{cace{W: {g%-9s" "{CC{class{W: {g%s\n\r"
                     "  {CC{clan  {W: {g%-24s" "{CR{cank {W: {g%-9s\n\r"
                     "  {CP{clayer {CK{cills{W: {g%-10ld" "{CP{clayer {CD{ceaths{W: {g%ld\n\r"
                     "  {CA{crena {CW{cins  {W: {g%-10d" "{CA{crena  {CL{cosses{W: {g%d\n\r"
                     "  {CR{cole-play {CP{coints{W: {g%-3d {C({cCurrent{C)"
                     " {W/ {g%-3d {C({cTotal{C){x\n\r"
                     " {W   {+L{+a{+s{+t {+L{+o{+g{+g{+e{+d o{+f{+f {x%-25s{x\n\r",
                     victim->name, victim->pcdata->title, victim->level, 
                     pc_race_table[victim->race].name,
                     class_table[victim->class].name,
                     clan_table[victim->clan].exname,
                     clan_rank_table[victim->clan_rank].title_of_rank[victim->sex],
                     victim->pcdata->pkkills, victim->pcdata->pkdeaths,
                     victim->pcdata->awins, victim->pcdata->alosses,
                     victim->rps, victim->rpst,
                     ( char * ) ctime ( &victim->llogoff ) );

/* It doesn't like this.
    if ( ( victim = get_char_world ( ch, arg ) ) != NULL && !IS_NPC ( victim ) )
    {
        act ( "          $N is on right {=n{=o{=w{x!", ch, NULL, victim, TO_CHAR );
    }
*/

/* Fixed small error in code so I could compile. -- Dist */

    printf_to_char ( ch, "\n\r" );
    free_pcdata ( victim->pcdata );
    free_char ( victim );
    return;
}
