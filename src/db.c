
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sqlite3.h>

#define IN_DB_C
#include "merc.h"
#include "db.h"
#include "recycle.h"
#include "music.h"
#include "tables.h"
#include "lookup.h"
#undef IN_DB_C

#include "olc.h"
#include "sql_io.h"
#include "fd_property.h"
#include "str_util.h"

extern int _filbuf args((FILE *));

/*
 * Locals.
 */
char *string_hash[MAX_KEY_HASH];

char *string_space;
char *top_string;

/*
 * Memory management.
 * Increase MAX_STRING if you have too.
 * Tune the others only if you understand what you're doing.
 */
#define			MAX_STRING	2100*1024

int sAllocString;

/*
 * Big mama top level function.
 */
void boot_db()
{
  /* 
   * Init some data space stuff.
   */
  {
    log_string("Init data space.");

    if ((string_space = calloc(1, MAX_STRING)) == NULL)
    {
      bug("Boot_db: can't alloc %d string space.", MAX_STRING);
      quit(1);
    }
    top_string = string_space;
    fBootDb = true;
  }

  /* 
   * Init random number generator.
   */
  {
    init_mm();
    auction_list = NULL;

  }

  /* 
   * Set time and weather.
   */
  {
    long lhour, lday, lmonth;

    log_string("Setting time and weather.");

    lhour = (current_time - 650336715) / (PULSE_TICK / PULSE_PER_SECOND);
    time_info.hour = lhour % 24;
    lday = lhour / 24;
    time_info.day = lday % 35;
    lmonth = lday / 35;
    time_info.month = lmonth % 17;
    time_info.year = lmonth / 17;

    if (time_info.hour < 5)
      weather_info.sunlight = SUN_DARK;
    else if (time_info.hour < 6)
      weather_info.sunlight = SUN_RISE;
    else if (time_info.hour < 19)
      weather_info.sunlight = SUN_LIGHT;
    else if (time_info.hour < 20)
      weather_info.sunlight = SUN_SET;
    else
      weather_info.sunlight = SUN_DARK;
    weather_info.change = 0;
    weather_info.mmhg = 960;
    if (time_info.month >= 7 && time_info.month <= 12)
      weather_info.mmhg += number_range(1, 50);
    else
      weather_info.mmhg += number_range(1, 80);
    if (weather_info.mmhg <= 980)
      weather_info.sky = SKY_LIGHTNING;
    else if (weather_info.mmhg <= 1000)
      weather_info.sky = SKY_RAINING;
    else if (weather_info.mmhg <= 1020)
      weather_info.sky = SKY_CLOUDY;
    else
      weather_info.sky = SKY_CLOUDLESS;
  }

  /* reboot counter */
  reboot_counter = 1440;        /* 12 hours */

  {
    FILE *w, *n;

    wizlock = (!(w = file_open(WIZLOCK_FILE, "r")) ? false : true);
    file_close(w);
    newlock = (!(n = file_open(NEWLOCK_FILE, "r")) ? false : true);
    file_close(n);
  }

  /* 
   * Assign gsn's for skills which have them.
   */
  {
    int sn;

    log_string("Assigning GSN's.");
    for (sn = 0; sn < MAX_SKILL; sn++)
    {
      if (skill_table[sn].pgsn != NULL)
        *skill_table[sn].pgsn = sn;
    }
  }

  load_properties();

  /* 
   * Read in all the area files.
   */
  {
    FILE *fpList;

    log_string("Reading Area List.");
    if ((fpList = file_open(AREA_LIST, "r")) == NULL)
    {
      perror(AREA_LIST);
      quit(1);
    }
    for (;;)
    {
      strcpy(strArea, fread_word(fpList));
      if (strArea[0] == '$')
        break;

      if (strArea[0] == '-')
      {
        fpArea = stdin;
      }
      else
      {

        sprintf(log_buf, "reading %s", strArea);
        log_string(log_buf);

        if ((fpArea = file_open(strArea, "r")) == NULL)
        {
          perror(strArea);
          quit(1);
        }
      }

      for (;;)
      {
        char *word;

        if (fread_letter(fpArea) != '#')
        {
          bug("Boot_db: # not found.", 0);
          quit(1);
        }

        word = fread_word(fpArea);

        if (word[0] == '$')
          break;
        else if (!str_cmp(word, "AREA"))
          load_area(fpArea);
        /* OLC */
        else if (!str_cmp(word, "AREADATA"))
          new_load_area(fpArea);
        else if (!str_cmp(word, "HELPS"))
          load_helps(fpArea, strArea);
        else if (!str_cmp(word, "MOBILES"))
          load_mobiles(fpArea);
        else if (!str_cmp(word, "MOBPROGS"))
          load_mobprogs(fpArea);
        else if (!str_cmp(word, "OBJECTS"))
          load_objects(fpArea);
        else if (!str_cmp(word, "RESETS"))
          load_resets(fpArea);
        else if (!str_cmp(word, "ROOMS"))
          load_rooms(fpArea);
        else if (!str_cmp(word, "SHOPS"))
          load_shops(fpArea);
        /* else if ( !str_cmp( word, "SOCIALS" ) ) load_socials
           (fpArea); */
        else if (!str_cmp(word, "SPECIALS"))
          load_specials(fpArea);
        else
        {
          bug("Boot_db: bad section name.", 0);
          quit(1);
        }
      }

      if (fpArea != stdin)
        file_close(fpArea);
      fpArea = NULL;
    }
    file_close(fpList);
  }
  /* 
   * Fix up exits.
   * Declare db booting over.
   * Reset all areas once.
   * Load up the songs, notes and ban files.
   */
  {
    log_string("Fixing exits.");
    fix_exits();
    fix_mobprogs();
    sort_areas_by_level();
    fBootDb = false;
    log_string("Area Update.");
    area_update();
    log_string("Loading Moveable Exits.");
    randomize_entrances(0);
    log_string("Loading Boards.");
    load_boards();
    load_disabled();
    log_string("Loading Bans.");
    load_bans();
    log_string("Loading Wizlist.");
    load_wizlist();
    log_string("Loading Clanlists.");
    load_clanlist();
    log_string("Loading Songs.");
    load_songs();
    log_string("Loading Bank Data.");
    load_bank();
    log_string("Loading Socials.");
    load_social_table();
    // log_string("Loading Gquest Data.");
    // load_gquest_data(); 
  }
  return;
}

/*
 * Snarf an 'area' header line.
 */
void load_area(FILE * fp)
{
  AREA_DATA *pArea;

  pArea = new_area();
  /* pArea->reset_first = NULL; pArea->reset_last = NULL; */
  fread_string(fp);
  pArea->file_name = fread_string(fp);

  pArea->area_flags = AREA_LOADING; /* OLC */
  pArea->security = MAX_SECURITY;
  pArea->builders = str_dup("None");  /* OLC */
  pArea->vnum = top_area;       /* OLC */
  pArea->repop_msg = fread_string(fp);
  pArea->name = fread_string(fp);
  pArea->credits = fread_string(fp);
  pArea->min_vnum = fread_long(fp);
  pArea->max_vnum = fread_long(fp);
  pArea->age = 15;
  pArea->nplayer = 0;
  pArea->empty = false;

  if (!area_first)
    area_first = pArea;
  if (area_last)
  {
    area_last->next = pArea;
    REMOVE_BIT(area_last->area_flags, AREA_LOADING);  /* OLC */
  }
  area_last = pArea;
  pArea->next = NULL;
  current_area = pArea;
  return;
}

/*
 * OLC
 * Use these macros to load any new area formats that you choose to
 * support on your MUD.  See the new_load_area format below for
 * a short example.
 */
#if defined(KEY)
#undef KEY
#endif

#define KEY( literal, field, value )                \
                if ( !str_cmp( word, literal ) )    \
                {                                   \
                    field  = value;                 \
                    fMatch = true;                  \
                    break;                          \
                 }

#define SKEY( string, field )                       \
                if ( !str_cmp( word, string ) )     \
                {                                   \
         _free_string( field, __FILE__, __LINE__ ); \
                    field = fread_string( fp );     \
                    fMatch = true;                  \
                    break;                          \
                                }

/* OLC
 * Snarf an 'area' header line.   Check this format.  MUCH better.  Add fields
 * too.
 *
 * #AREAFILE
 * Name   { All } Locke    Newbie School~
 * Repop  A teacher pops in the room and says, 'Repop coming!'~
 * Recall 3001
 * End
 */
void new_load_area(FILE * fp)
{
  AREA_DATA *pArea;
  char *word;
  bool fMatch;

  pArea = new_area();
  pArea->age = 15;
  pArea->nplayer = 0;
  pArea->file_name = str_dup(strArea);
  pArea->vnum = top_area;
  pArea->name = str_dup("New Area");
  pArea->builders = str_dup("");
  pArea->security = MAX_SECURITY;
  pArea->repop_msg = str_dup("REPOP!");
  pArea->min_vnum = 0;
  pArea->max_vnum = 0;
  pArea->area_flags = 0;

  for (;;)
  {
    word = feof(fp) ? "End" : fread_word(fp);
    fMatch = false;

    switch (UPPER(word[0]))
    {
      case 'N':
        SKEY("Name", pArea->name);
        break;
      case 'S':
        if (!str_cmp(word, "Security"))
        {
          pArea->security = fread_number(fp);
          if (pArea->security > MAX_SECURITY)
            pArea->security = MAX_SECURITY;
          fMatch = true;
          break;
        }
        break;
      case 'V':
        if (!str_cmp(word, "VNUMs"))
        {
          pArea->min_vnum = fread_long(fp);
          pArea->max_vnum = fread_long(fp);
        }
        break;
      case 'E':
        if (!str_cmp(word, "End"))
        {
          fMatch = true;
          if (area_first == NULL)
            area_first = pArea;
          if (area_last != NULL)
            area_last->next = pArea;
          area_last = pArea;
          pArea->next = NULL;
          current_area = pArea;
          store_area(pArea);
          return;
        }
        break;
      case 'B':
        SKEY("Builders", pArea->builders);
        break;
      case 'C':
        SKEY("Credits", pArea->credits);
        break;
      case 'R':
        SKEY("RepopMsg", pArea->repop_msg);
    }
  }
}

/*
 * Sets vnum range for area using OLC protection features.
 */
void assign_area_vnum(long vnum)
{
  if (area_last->min_vnum == 0 || area_last->max_vnum == 0)
    area_last->min_vnum = area_last->max_vnum = vnum;
  if (vnum != URANGE(area_last->min_vnum, vnum, area_last->max_vnum))
  {
    if (vnum < area_last->min_vnum)
      area_last->min_vnum = vnum;
    else
      area_last->max_vnum = vnum;
  }
  return;
}

/*
 * Snarf a help section.
 */
void load_helps(FILE * fp, char *fname)
{
  HELP_DATA *pHelp;
  int level;
  char *keyword;

  for (;;)
  {
    level = fread_number(fp);
    keyword = fread_string(fp);

    if (keyword[0] == '$')
      break;

    pHelp = new_help();
    pHelp->level = level;
    pHelp->keyword = keyword;

    pHelp->text = fread_string(fp);

    if (!str_cmp(pHelp->keyword, "greeting1"))
      help_greetinga = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "greeting1"))
      help_greetingb = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "greeting1"))
      help_greetingc = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "greeting1"))
      help_greetingd = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "greeting1"))
      help_greetinge = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "authors"))
      help_authors = str_dup(pHelp->text);
    if (!str_cmp(pHelp->keyword, "login"))
      help_login = str_dup(pHelp->text);

    store_help(pHelp);
    free_help(pHelp);
    pHelp = NULL;

    top_help++;
  }
  return;
}

/*
 * Adds a reset to a room.  OLC
 * Similar to add_reset in olc.c
 */
void new_reset(ROOM_INDEX_DATA * pR, RESET_DATA * pReset)
{
  RESET_DATA *pr;

  if (!pR)
    return;

  pr = pR->reset_last;

  if (!pr)
  {
    pR->reset_first = pReset;
    pR->reset_last = pReset;
  }
  else
  {
    pR->reset_last->next = pReset;
    pR->reset_last = pReset;
    pR->reset_last->next = NULL;
  }

  top_reset++;
  return;
}

/*
 * Snarf a reset section.
 */
void load_resets(FILE * fp)
{
  RESET_DATA *pReset;
  int iLastRoom = 0;
  int iLastObj = 0;

  if (!area_last)
  {
    bug("Load_resets: no #AREA seen yet.", 0);
    quit(1);
  }

  for (;;)
  {
    ROOM_INDEX_DATA *pRoomIndex;
    EXIT_DATA *pexit = NULL;
    char letter;
    OBJ_INDEX_DATA *temp_index;

    if ((letter = fread_letter(fp)) == 'S')
      break;

    if (letter == '*')
    {
      fread_to_eol(fp);
      continue;
    }

    pReset = new_reset_data();
    pReset->command = letter;
    /* if_flag */ fread_number(fp);
    pReset->arg1 = fread_long(fp);
    pReset->arg2 = fread_number(fp);
    pReset->arg3 = (letter == 'G' || letter == 'R') ? 0 : fread_long(fp);
    pReset->arg4 = (letter == 'P' || letter == 'M') ? fread_number(fp) : 0;
    fread_to_eol(fp);

    /* 
     * Validate parameters.
     * We're calling the index functions for the side effect.
     */
    switch (letter)
    {
      default:
        bug("Load_resets: bad command '%c'.", letter);
        quit(1);
        break;

      case 'M':
        get_mob_index(pReset->arg1);
        if ((pRoomIndex = get_room_index(pReset->arg3)))
        {
          new_reset(pRoomIndex, pReset);
          iLastRoom = pReset->arg3;
        }
        break;

      case 'O':
        temp_index = get_obj_index(pReset->arg1);
        temp_index->reset_num++;
        if ((pRoomIndex = get_room_index(pReset->arg3)))
        {
          new_reset(pRoomIndex, pReset);
          iLastObj = pReset->arg3;
        }
        break;

      case 'P':
        temp_index = get_obj_index(pReset->arg1);
        temp_index->reset_num++;
        if ((pRoomIndex = get_room_index(iLastObj)))
        {
          new_reset(pRoomIndex, pReset);
        }
        break;

      case 'G':
      case 'E':
        temp_index = get_obj_index(pReset->arg1);
        temp_index->reset_num++;
        if ((pRoomIndex = get_room_index(iLastRoom)))
        {
          new_reset(pRoomIndex, pReset);
          iLastObj = iLastRoom;
        }
        break;

      case 'D':
        pRoomIndex = get_room_index(pReset->arg1);

        if (pReset->arg2 < 0 || pReset->arg2 > (MAX_DIR - 1) ||
            !pRoomIndex || !(pexit = pRoomIndex->exit[pReset->arg2])
            || !IS_SET(pexit->rs_flags, EX_ISDOOR))
        {
          bug("Load_resets: 'D': exit %d not door.", pReset->arg2);
          quit(1);
        }

        switch (pReset->arg3)
        {
          default:
            bug("Load_resets: 'D': bad 'locks': %d.", pReset->arg3);
          case 0:
            break;
          case 1:
            SET_BIT(pexit->rs_flags, EX_CLOSED);
            SET_BIT(pexit->exit_info, EX_CLOSED);
            break;
          case 2:
            SET_BIT(pexit->rs_flags, EX_CLOSED | EX_LOCKED);
            SET_BIT(pexit->exit_info, EX_CLOSED | EX_LOCKED);
            break;
        }

        break;

      case 'R':
        pRoomIndex = get_room_index(pReset->arg1);

        if (pReset->arg2 < 0 || pReset->arg2 > MAX_DIR)
        {
          bug("Load_resets: 'R': bad exit %d.", pReset->arg2);
          quit(1);
        }

        if (pRoomIndex)
          new_reset(pRoomIndex, pReset);

        break;
    }

  }

  return;
}

/*
 * Snarf a room section.
 */
void load_rooms(FILE * fp)
{
  ROOM_INDEX_DATA *pRoomIndex;

  if (area_last == NULL)
  {
    bug("Load_resets: no #AREA seen yet.", 0);
    quit(1);
  }

  for (;;)
  {
    long vnum;
    char letter;
    int door;
    int iHash;

    letter = fread_letter(fp);
    if (letter != '#')
    {
      bug("Load_rooms: # not found.", 0);
      quit(1);
    }

    vnum = fread_long(fp);
    if (vnum == 0)
      break;

    if (vnum > top_vnum)
      top_vnum = vnum;

    fBootDb = false;
    if (get_room_index(vnum) != NULL)
    {
      bug("Load_rooms: vnum %d duplicated.", vnum);
      quit(1);
    }
    fBootDb = true;

    pRoomIndex = new_room_index();
    pRoomIndex->owner = str_dup("");
    pRoomIndex->people = NULL;
    pRoomIndex->contents = NULL;
    pRoomIndex->extra_descr = NULL;
    pRoomIndex->area = area_last;
    pRoomIndex->vnum = vnum;
    pRoomIndex->name = fread_string(fp);
    pRoomIndex->description = fread_string(fp);
    /* Area number */// fread_number( fp );
    pRoomIndex->tele_dest = fread_number(fp);
    pRoomIndex->room_flags = fread_flag(fp);
    /* horrible hack */
    if (2900 <= vnum && vnum < 3400)
      SET_BIT(pRoomIndex->room_flags, ROOM_LAW);
    //      if ( 20000 <= vnum && vnum < 21000)
    // SET_BIT(pRoomIndex->room_flags,ROOM_NOWHERE);
    pRoomIndex->sector_type = fread_number(fp);
    pRoomIndex->light = 0;
    for (door = 0; door < MAX_DIR; door++)
      pRoomIndex->exit[door] = NULL;

    /* defaults */
    pRoomIndex->heal_rate = 100;
    pRoomIndex->mana_rate = 100;

    for (;;)
    {
      letter = fread_letter(fp);

      if (letter == 'S')
        break;

      if (letter == 'H')        /* healing room */
        pRoomIndex->heal_rate = fread_number(fp);

      else if (letter == 'M')   /* mana room */
        pRoomIndex->mana_rate = fread_number(fp);

      else if (letter == 'C')   /* clan */
      {
        char *temp;

        if (pRoomIndex->clan)
        {
          bug("Load_rooms: duplicate clan fields.", 0);
          quit(1);
        }
        temp = fread_string(fp);
        pRoomIndex->clan = clan_lookup(temp);
        free_string(temp);
      }

      else if (letter == 'D')
      {
        EXIT_DATA *pexit;
        int locks;

        door = fread_number(fp);
        if (door < 0 || door >= MAX_DIR)
        {
          bug("Fread_rooms: vnum %d has bad door number.", vnum);
          quit(1);
        }

        pexit = new_exit();
        pexit->description = fread_string(fp);
        pexit->keyword = fread_string(fp);
        pexit->exit_info = 0;
        pexit->rs_flags = 0;    /* OLC */
        locks = fread_number(fp);
        pexit->key = fread_long(fp);
        pexit->u1.vnum = fread_long(fp);
        pexit->orig_door = door;  /* OLC */

        switch (locks)
        {
          case 1:
            pexit->exit_info = EX_ISDOOR;
            pexit->rs_flags = EX_ISDOOR;
            break;
          case 2:
            pexit->exit_info = EX_ISDOOR | EX_PICKPROOF;
            pexit->rs_flags = EX_ISDOOR | EX_PICKPROOF;
            break;
          case 3:
            pexit->exit_info = EX_ISDOOR | EX_NOPASS;
            pexit->rs_flags = EX_ISDOOR | EX_NOPASS;
            break;
          case 4:
            pexit->exit_info = EX_ISDOOR | EX_NOPASS | EX_PICKPROOF;
            pexit->rs_flags = EX_ISDOOR | EX_NOPASS | EX_PICKPROOF;
            break;
        }

        pRoomIndex->exit[door] = pexit;
        pRoomIndex->old_exit[door] = pexit;
        top_exit++;
      }
      else if (letter == 'E')
      {
        EXTRA_DESCR_DATA *ed;

        ed = new_extra_descr();
        ed->keyword = fread_string(fp);
        ed->description = fread_string(fp);
        ed->next = pRoomIndex->extra_descr;
        pRoomIndex->extra_descr = ed;
      }

      else if (letter == 'O')
      {
        if (pRoomIndex->owner[0] != '\0')
        {
          bug("Load_rooms: duplicate owner.", 0);
          quit(1);
        }

        pRoomIndex->owner = fread_string(fp);
      }

      else if (letter == 'P')
      {
        char key[MAX_STRING_LENGTH];
        char type[MAX_STRING_LENGTH];
        char value[MAX_STRING_LENGTH];
        int i;
        bool b;
        char c;
        long l;

        strcpy(key, fread_string_temp(fp));
        strcpy(type, fread_string_temp(fp));
        strcpy(value, fread_string_temp(fp));

        switch (which_keyword(type, "int", "bool", "string",
                              "char", "long", NULL))
        {
          case 1:
            i = atoi(value);
            SetRoomProperty(pRoomIndex, PROPERTY_INT, key, &i);
            break;
          case 2:
            switch (which_keyword(value, "true", "false", NULL))
            {
              case 1:
                b = true;
                SetRoomProperty(pRoomIndex, PROPERTY_BOOL, key, &b);
                break;
              case 2:
                b = false;
                SetRoomProperty(pRoomIndex, PROPERTY_BOOL, key, &b);
                break;
              default:
                ;
            }
            break;
          case 3:
            SetRoomProperty(pRoomIndex, PROPERTY_STRING, key, value);
            break;
          case 4:
            c = value[0];
            SetRoomProperty(pRoomIndex, PROPERTY_CHAR, key, &c);
            break;
          case 5:
            l = atol(value);
            SetRoomProperty(pRoomIndex, PROPERTY_LONG, key, &l);
            break;
          default:
            bugf("Property: unknown keyword %s", type);
        }
      }
      else
      {
        bug("Load_rooms: vnum %ld has flag not 'DES'.", vnum);
        quit(1);
      }
    }

    iHash = vnum % MAX_KEY_HASH;
    pRoomIndex->next = room_index_hash[iHash];
    room_index_hash[iHash] = pRoomIndex;
    top_room++;
    top_vnum_room = top_vnum_room < vnum ? vnum : top_vnum_room;  /* OLC */
    assign_area_vnum(vnum);     /* OLC */
  }

  return;
}

/*
 * Load mobprogs section
 */
void load_mobprogs(FILE * fp)
{
  MPROG_CODE *pMprog;

  if (area_last == NULL)
  {
    bug("Load_mobprogs: no #AREA seen yet.", 0);
    quit(1);
  }

  for (;;)
  {
    long vnum;
    char letter;

    letter = fread_letter(fp);
    if (letter != '#')
    {
      bug("Load_mobprogs: # not found.", 0);
      quit(1);
    }

    vnum = fread_long(fp);
    if (vnum == 0)
      break;

    fBootDb = false;
    if (get_mprog_index(vnum) != NULL)
    {
      bug("Load_mobprogs: vnum %d duplicated.", vnum);
      quit(1);
    }
    fBootDb = true;

    pMprog = new_mpcode();
    pMprog->vnum = vnum;
    pMprog->code = fread_string(fp);
    if (mprog_list == NULL)
      mprog_list = pMprog;
    else
    {
      pMprog->next = mprog_list;
      mprog_list = pMprog;
    }
  }
  return;
}

/*
 *  Translate mobprog vnums pointers to real code
 */
void fix_mobprogs(void)
{
  MOB_INDEX_DATA *pMobIndex;
  MPROG_LIST *list;
  MPROG_CODE *prog;
  int iHash;

  for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
  {
    for (pMobIndex = mob_index_hash[iHash]; pMobIndex != NULL;
         pMobIndex = pMobIndex->next)
    {
      for (list = pMobIndex->mprogs; list != NULL; list = list->next)
      {
        if ((prog = get_mprog_index(list->vnum)) != NULL)
          list->code = prog->code;
        else
        {
          bug("Fix_mobprogs: code vnum %d not found.", list->vnum);
          quit(1);
        }
      }
    }
  }
}

/*
 * Snarf a shop section.
 */
void load_shops(FILE * fp)
{
  SHOP_DATA *pShop;

  for (;;)
  {
    MOB_INDEX_DATA *pMobIndex;
    int iTrade;

    pShop = new_shop();
    pShop->keeper = fread_long(fp);
    if (pShop->keeper == 0)
      break;
    for (iTrade = 0; iTrade < MAX_TRADE; iTrade++)
      pShop->buy_type[iTrade] = fread_number(fp);
    pShop->profit_buy = fread_number(fp);
    pShop->profit_sell = fread_number(fp);
    pShop->open_hour = fread_number(fp);
    pShop->close_hour = fread_number(fp);
    fread_to_eol(fp);
    pMobIndex = get_mob_index(pShop->keeper);
    pMobIndex->pShop = pShop;

    if (shop_first == NULL)
      shop_first = pShop;
    if (shop_last != NULL)
      shop_last->next = pShop;

    shop_last = pShop;
    pShop->next = NULL;
    top_shop++;
  }

  return;
}

/*
 * Snarf spec proc declarations.
 */
void load_specials(FILE * fp)
{
  for (;;)
  {
    MOB_INDEX_DATA *pMobIndex;
    char letter;

    switch (letter = fread_letter(fp))
    {
      default:
        bug("Load_specials: letter '%c' not *MS.", letter);
        quit(1);

      case 'S':
        return;

      case '*':
        break;

      case 'M':
        pMobIndex = get_mob_index(fread_number(fp));
        pMobIndex->spec_fun = spec_lookup(fread_word(fp));
        if (pMobIndex->spec_fun == 0)
        {
          bug("Load_specials: 'M': vnum %d.", pMobIndex->vnum);
          quit(1);
        }
        break;
    }

    fread_to_eol(fp);
  }
}

/*
 * Translate all room exits from virtual to real.
 * Has to be done after all rooms are read in.
 * Check for bad reverse exits.
 */
void fix_exits(void)
{
  ROOM_INDEX_DATA *pRoomIndex;
  EXIT_DATA *pexit;
  int iHash;
  int door;

  for (iHash = 0; iHash < MAX_KEY_HASH; iHash++)
  {
    for (pRoomIndex = room_index_hash[iHash]; pRoomIndex != NULL;
         pRoomIndex = pRoomIndex->next)
    {
      bool fexit;

      fexit = false;
      for (door = 0; door < MAX_DIR; door++)
      {
        if ((pexit = pRoomIndex->exit[door]) != NULL)
        {
          if (pexit->u1.vnum <= 0 || get_room_index(pexit->u1.vnum) == NULL)
            pexit->u1.to_room = NULL;
          else
          {
            fexit = true;
            pexit->u1.to_room = get_room_index(pexit->u1.vnum);
          }
        }
      }
      if (!fexit)
        SET_BIT(pRoomIndex->room_flags, ROOM_NO_MOB);
    }
  }
}

/*
 * Repopulate areas periodically.
 */
void area_update(void)
{
  AREA_DATA *pArea;

  for (pArea = area_first; pArea != NULL; pArea = pArea->next)
  {

    if (++pArea->age < 3)
      continue;

    /* 
     * Check age and reset.
     * Note: Mud School resets every 3 minutes (not 15).
     */

    /*      if ( ( !pArea->empty && ( pArea->nplayer == 0 || pArea->age >= 15 ) ) */
    if ((pArea->nplayer == 0 || pArea->age >= 15))
    {
      ROOM_INDEX_DATA *pRoomIndex;

      reset_area(pArea);

      pArea->age = number_range(0, 3);
      pRoomIndex = get_room_index(ROOM_VNUM_SCHOOL);
      if (pRoomIndex != NULL && pArea == pRoomIndex->area)
      {
        pArea->age = 15 - 2;
      }
      else
      {
        /*    pRoomIndex = get_room_index( ROOM_VNUM_CLANS ); */
        if (pRoomIndex != NULL && pArea == pRoomIndex->area)
        {
          pArea->age = 15 - 4;
        }
        else if (pArea->nplayer == 0)
        {
          pArea->empty = true;
        }
      }
    }
  }

  return;
}

/* OLC
 * Reset one room.  Called by reset_area and olc.
 */
void reset_room(ROOM_INDEX_DATA * pRoom)
{
  RESET_DATA *pReset;
  CHAR_DATA *pMob;
  CHAR_DATA *mob;
  OBJ_DATA *pObj;
  CHAR_DATA *LastMob = NULL;
  OBJ_DATA *LastObj = NULL;
  int iExit;
  int level = 0;
  bool last;

  /*    if ( !pRoom )
     return; */

  pMob = NULL;
  last = false;

  for (iExit = 0; iExit < MAX_DIR; iExit++)
  {
    EXIT_DATA *pExit;

    if ((pExit = pRoom->exit[iExit])
        /* && !IS_SET( pExit->exit_info, EX_BASHED ) ROM OLC */ )
    {
      pExit->exit_info = pExit->rs_flags;
      if ((pExit->u1.to_room != NULL) &&
          ((pExit = pExit->u1.to_room->exit[rev_dir[iExit]])))
      {
        /* nail the other side */
        pExit->exit_info = pExit->rs_flags;
      }
    }
  }

  for (pReset = pRoom->reset_first; pReset != NULL; pReset = pReset->next)
  {
    MOB_INDEX_DATA *pMobIndex;
    OBJ_INDEX_DATA *pObjIndex;
    OBJ_INDEX_DATA *pObjToIndex;
    ROOM_INDEX_DATA *pRoomIndex;
    char buf[MAX_STRING_LENGTH];
    int count, limit = 0;

    switch (pReset->command)
    {
      default:
        bug("Reset_room: bad command %c.", pReset->command);
        break;

      case 'M':
        if (!(pMobIndex = get_mob_index(pReset->arg1)))
        {
          bug("Reset_room: 'M': bad vnum %d.", pReset->arg1);
          continue;
        }

        if ((pRoomIndex = get_room_index(pReset->arg3)) == NULL)
        {
          bug("Reset_area: 'R': bad vnum %d.", pReset->arg3);
          continue;
        }

        if (pMobIndex->count >= pReset->arg2)
        {
          last = false;
          break;
        }
        /* */
        count = 0;
        for (mob = pRoomIndex->people; mob != NULL; mob = mob->next_in_room)
          if (mob->pIndexData == pMobIndex)
          {
            count++;
            if (count >= pReset->arg4)
            {
              last = false;
              break;
            }
          }

        if (count >= pReset->arg4)
          break;

        /* */
        pMob = create_mobile(pMobIndex);

        /* 
         * Some more hard coding.
         */
        if (room_is_dark(pRoom))
          SET_BIT(pMob->affected_by, AFF_INFRARED);

        /* 
         * Pet shop mobiles get ACT_PET set.
         */
        {
          ROOM_INDEX_DATA *pRoomIndexPrev;

          pRoomIndexPrev = get_room_index(pRoom->vnum - 1);
          if (pRoomIndexPrev &&
              IS_SET(pRoomIndexPrev->room_flags, ROOM_PET_SHOP))
            SET_BIT(pMob->act, ACT_PET);
        }

        char_to_room(pMob, pRoom);

        LastMob = pMob;
        level = URANGE(0, pMob->level - 2, LEVEL_ANCIENT - 1);  /* -1 
                                                                   ROM 
                                                                 */
        last = true;
        break;

      case 'O':
        if (!(pObjIndex = get_obj_index(pReset->arg1)))
        {
          bug("Reset_room: 'O' 1 : bad vnum %d", pReset->arg1);
          sprintf(buf, "%ld %d %ld %d", pReset->arg1, pReset->arg2,
                  pReset->arg3, pReset->arg4);
          bug(buf, 1);
          continue;
        }

        if (!(pRoomIndex = get_room_index(pReset->arg3)))
        {
          bug("Reset_room: 'O' 2 : bad vnum %d.", pReset->arg3);
          sprintf(buf, "%ld %d %ld %d", pReset->arg1, pReset->arg2,
                  pReset->arg3, pReset->arg4);
          bug(buf, 1);
          continue;
        }

        if (count_obj_list(pObjIndex, pRoom->contents) > 0)
        {
          last = false;
          break;
        }

        pObj = create_object(pObjIndex);
        pObj->cost = 0;
        obj_to_room(pObj, pRoom);
        last = true;
        break;

      case 'P':
        if (!(pObjIndex = get_obj_index(pReset->arg1)))
        {
          bug("Reset_room: 'P': bad vnum %d.", pReset->arg1);
          continue;
        }
        if (!(pObjToIndex = get_obj_index(pReset->arg3)))
        {
          bug("Reset_room: 'P': bad vnum %d.", pReset->arg3);
          continue;
        }

        if (pReset->arg2 > 50)  /* old format */
          limit = 6;
        else if (pReset->arg2 == -1)  /* no limit */
          limit = 999;
        else
          limit = pReset->arg2;

        if ((LastObj = get_obj_type(pObjToIndex)) == NULL ||
            (LastObj->in_room == NULL && !last) || (pObjIndex->count >= limit)
            || (count =
                count_obj_list(pObjIndex, LastObj->contains)) > pReset->arg4)
        {
          last = false;
          break;
        }
        /* lastObj->level - ROM */

        while (count < pReset->arg4)
        {
          pObj = create_object(pObjIndex);
          obj_to_obj(pObj, LastObj);
          count++;
          if (pObjIndex->count >= limit)
            break;
        }

        /* fix object lock state! */
        LastObj->value[1] = LastObj->pIndexData->value[1];
        last = true;
        break;

      case 'G':
      case 'E':
        if (!(pObjIndex = get_obj_index(pReset->arg1)))
        {
          bug("Reset_room: 'E' or 'G': bad vnum %d.", pReset->arg1);
          continue;
        }

        if (!last)
          break;

        if (!LastMob)
        {
          bug("Reset_room: 'E' or 'G': null mob for vnum %d.", pReset->arg1);
          last = false;
          break;
        }

        if (LastMob->pIndexData->pShop) /* Shop-keeper? */
        {
          pObj = create_object(pObjIndex);
          SET_BIT(pObj->extra_flags, ITEM_INVENTORY); /* ROM OLC */
        }
        else                    /* ROM OLC else version */
        {
          int limit;

          if (pReset->arg2 > 50)  /* old format */
            limit = 6;
          else if (pReset->arg2 == -1 || pReset->arg2 == 0) /* no
                                                               limit 
                                                             */
            limit = 999;
          else
            limit = pReset->arg2;

          if (pObjIndex->count < limit || number_range(0, 4) == 0)
          {
            pObj = create_object(pObjIndex);
          }
          else
            break;
        }

        obj_to_char(pObj, LastMob);
        if (pReset->command == 'E')
          equip_char(LastMob, pObj, pReset->arg3);
        last = true;
        break;

      case 'D':
        break;

      case 'R':
        if (!(pRoomIndex = get_room_index(pReset->arg1)))
        {
          bug("Reset_room: 'R': bad vnum %d.", pReset->arg1);
          continue;
        }

        {
          EXIT_DATA *pExit;
          int d0;
          int d1;

          for (d0 = 0; d0 < pReset->arg2 - 1; d0++)
          {
            d1 = number_range(d0, pReset->arg2 - 1);
            pExit = pRoomIndex->exit[d0];
            pRoomIndex->exit[d0] = pRoomIndex->exit[d1];
            pRoomIndex->exit[d1] = pExit;
          }
        }
        break;
    }
  }

  return;
}

     /* OLC * Reset one area. */
void reset_area(AREA_DATA * pArea)
{
  ROOM_INDEX_DATA *pRoom;
  long vnum;

  for (vnum = pArea->min_vnum; vnum <= pArea->max_vnum; vnum++)
  {
    if ((pRoom = get_room_index(vnum)))
      reset_room(pRoom);
  }
  return;
}

/*
 * Create an instance of a mobile.
 */
CHAR_DATA *create_mobile(MOB_INDEX_DATA * pMobIndex)
{
  CHAR_DATA *mob;
  int i;
  AFFECT_DATA af;

  mobile_count++;

  if (pMobIndex == NULL)
  {
    bug("Create_mobile: NULL pMobIndex.", 0);
    quit(1);
  }

  mob = new_char();

  mob->pIndexData = pMobIndex;

  mob->name = str_dup(pMobIndex->player_name);  /* OLC */
  mob->short_descr = str_dup(pMobIndex->short_descr); /* OLC */
  mob->long_descr = str_dup(pMobIndex->long_descr); /* OLC */
  mob->description = str_dup(pMobIndex->description); /* OLC */
  mob->id = get_mob_id();
  mob->spec_fun = pMobIndex->spec_fun;
  mob->prompt = NULL;
  mob->mprog_target = NULL;

  if (pMobIndex->wealth == 0)
  {
    mob->silver = 0;
    mob->gold = 0;
  }
  else
  {
    long wealth;

    wealth = number_range(pMobIndex->wealth / 2, 3 * pMobIndex->wealth / 2);
    mob->gold = number_range(wealth / 200, wealth / 100);
    mob->silver = wealth - (mob->gold * 100);
  }

  /* read from prototype */
  mob->group = pMobIndex->group;
  mob->act = pMobIndex->act;
  mob->act2 = pMobIndex->act2;
  mob->comm = COMM_NOCHANNELS | COMM_NOSHOUT | COMM_NOTELL;
  mob->affected_by = pMobIndex->affected_by;
  mob->shielded_by = pMobIndex->shielded_by;
  mob->alignment = pMobIndex->alignment;
  mob->level = pMobIndex->level;
  mob->hitroll = pMobIndex->hitroll;
  mob->damroll = pMobIndex->damage[DICE_BONUS];
  mob->max_hit =
    dice(pMobIndex->hit[DICE_NUMBER],
         pMobIndex->hit[DICE_TYPE]) + pMobIndex->hit[DICE_BONUS];
  mob->hit = mob->max_hit;
  mob->max_mana =
    dice(pMobIndex->mana[DICE_NUMBER],
         pMobIndex->mana[DICE_TYPE]) + pMobIndex->mana[DICE_BONUS];
  mob->mana = mob->max_mana;
  mob->damage[DICE_NUMBER] = pMobIndex->damage[DICE_NUMBER];
  mob->damage[DICE_TYPE] = pMobIndex->damage[DICE_TYPE];
  mob->dam_type = pMobIndex->dam_type;
  if (mob->dam_type == 0)
    switch (number_range(1, 3))
    {
      case (1):
        mob->dam_type = 3;
        break;                  /* slash */
      case (2):
        mob->dam_type = 7;
        break;                  /* pound */
      case (3):
        mob->dam_type = 11;
        break;                  /* pierce */
    }
  for (i = 0; i < 4; i++)
    mob->armor[i] = pMobIndex->ac[i];
  mob->off_flags = pMobIndex->off_flags;
  mob->imm_flags = pMobIndex->imm_flags;
  mob->res_flags = pMobIndex->res_flags;
  mob->vuln_flags = pMobIndex->vuln_flags;
  mob->start_pos = pMobIndex->start_pos;
  mob->default_pos = pMobIndex->default_pos;
  mob->sex = pMobIndex->sex;
  if (mob->sex == 3)            /* random sex */
    mob->sex = number_range(1, 2);
  mob->race = pMobIndex->race;
  mob->form = pMobIndex->form;
  mob->parts = pMobIndex->parts;
  mob->size = pMobIndex->size;
  mob->material = str_dup(pMobIndex->material);
  mob->die_descr = pMobIndex->die_descr;
  mob->say_descr = pMobIndex->say_descr;

  /* computed on the spot */

  for (i = 0; i < MAX_STATS; i++)
    mob->perm_stat[i] = UMIN(25, 11 + mob->level / 4);

  if (IS_SET(mob->act, ACT_WARRIOR))
  {
    mob->perm_stat[STAT_STR] += 3;
    mob->perm_stat[STAT_INT] -= 1;
    mob->perm_stat[STAT_CON] += 2;
  }

  if (IS_SET(mob->act, ACT_THIEF))
  {
    mob->perm_stat[STAT_DEX] += 3;
    mob->perm_stat[STAT_INT] += 1;
    mob->perm_stat[STAT_WIS] -= 1;
  }

  if (IS_SET(mob->act, ACT_CLERIC))
  {
    mob->perm_stat[STAT_WIS] += 3;
    mob->perm_stat[STAT_DEX] -= 1;
    mob->perm_stat[STAT_STR] += 1;
  }

  if (IS_SET(mob->act, ACT_MAGE))
  {
    mob->perm_stat[STAT_INT] += 3;
    mob->perm_stat[STAT_STR] -= 1;
    mob->perm_stat[STAT_DEX] += 1;
  }

  if (IS_SET(mob->act, ACT_RANGER))
  {
    mob->perm_stat[STAT_STR] += 3;
    mob->perm_stat[STAT_CON] -= 1;
    mob->perm_stat[STAT_INT] += 1;
  }

  if (IS_SET(mob->act, ACT_DRUID))
  {
    mob->perm_stat[STAT_WIS] += 3;
    mob->perm_stat[STAT_STR] -= 1;
    mob->perm_stat[STAT_DEX] += 1;
  }

  if (IS_SET(mob->act, ACT_VAMPIRE))
  {
    mob->perm_stat[STAT_CON] += 3;
    mob->perm_stat[STAT_STR] += 1;
    mob->perm_stat[STAT_WIS] -= 1;
  }

  if (IS_SET(mob->off_flags, OFF_FAST))
    mob->perm_stat[STAT_DEX] += 2;

  mob->perm_stat[STAT_STR] += mob->size - SIZE_MEDIUM;
  mob->perm_stat[STAT_CON] += (mob->size - SIZE_MEDIUM) / 2;

  /* let's get some spell action */
  if (IS_SHIELDED(mob, SHD_SANCTUARY))
  {
    af.where = TO_SHIELDS;
    af.type = skill_lookup("sanctuary");
    af.level = mob->level;
    af.duration = -1;
    af.location = APPLY_NONE;
    af.modifier = 0;
    af.bitvector = SHD_SANCTUARY;
    affect_to_char(mob, &af);
  }

  if (IS_AFFECTED(mob, AFF_HASTE))
  {
    af.where = TO_AFFECTS;
    af.type = skill_lookup("haste");
    af.level = mob->level;
    af.duration = -1;
    af.location = APPLY_DEX;
    af.modifier =
      1 + (mob->level >= 18) + (mob->level >= 25) + (mob->level >= 32);
    af.bitvector = AFF_HASTE;
    affect_to_char(mob, &af);
  }

  if (IS_SHIELDED(mob, SHD_PROTECT_EVIL))
  {
    af.where = TO_SHIELDS;
    af.type = skill_lookup("protection evil");
    af.level = mob->level;
    af.duration = -1;
    af.location = APPLY_SAVES;
    af.modifier = -1;
    af.bitvector = SHD_PROTECT_EVIL;
    affect_to_char(mob, &af);
  }

  if (IS_SHIELDED(mob, SHD_PROTECT_GOOD))
  {
    af.where = TO_SHIELDS;
    af.type = skill_lookup("protection good");
    af.level = mob->level;
    af.duration = -1;
    af.location = APPLY_SAVES;
    af.modifier = -1;
    af.bitvector = SHD_PROTECT_GOOD;
    affect_to_char(mob, &af);
  }

  mob->position = mob->start_pos;

  /* link the mob to the world list */
  mob->next = char_list;
  char_list = mob;
  pMobIndex->count++;
  return mob;
}

/* duplicate a mobile exactly -- except inventory */
void clone_mobile(CHAR_DATA * parent, CHAR_DATA * clone)
{
  int i;
  AFFECT_DATA *paf;
  PROPERTY *prop;

  if (parent == NULL || clone == NULL || !IS_NPC(parent))
    return;

  /* start fixing values */
  clone->name = str_dup(parent->name);
  clone->version = parent->version;
  clone->short_descr = str_dup(parent->short_descr);
  clone->long_descr = str_dup(parent->long_descr);
  clone->description = str_dup(parent->description);
  clone->group = parent->group;
  clone->sex = parent->sex;
  clone->class = parent->class;
  clone->race = parent->race;
  clone->level = parent->level;
  clone->trust = 0;
  clone->timer = parent->timer;
  clone->wait = parent->wait;
  clone->hit = parent->hit;
  clone->max_hit = parent->max_hit;
  clone->mana = parent->mana;
  clone->max_mana = parent->max_mana;
  clone->move = parent->move;
  clone->max_move = parent->max_move;
  clone->gold = parent->gold;
  clone->silver = parent->silver;
  clone->exp = parent->exp;
  clone->act = parent->act;
  clone->act2 = parent->act2;
  clone->comm = parent->comm;
  clone->imm_flags = parent->imm_flags;
  clone->res_flags = parent->res_flags;
  clone->vuln_flags = parent->vuln_flags;
  clone->invis_level = parent->invis_level;
  clone->affected_by = parent->affected_by;
  clone->shielded_by = parent->shielded_by;
  clone->position = parent->position;
  clone->practice = parent->practice;
  clone->train = parent->train;
  clone->saving_throw = parent->saving_throw;
  clone->alignment = parent->alignment;
  clone->hitroll = parent->hitroll;
  clone->damroll = parent->damroll;
  clone->wimpy = parent->wimpy;
  clone->form = parent->form;
  clone->parts = parent->parts;
  clone->size = parent->size;
  clone->material = str_dup(parent->material);
  clone->off_flags = parent->off_flags;
  clone->dam_type = parent->dam_type;
  clone->start_pos = parent->start_pos;
  clone->default_pos = parent->default_pos;
  clone->spec_fun = parent->spec_fun;
  clone->die_descr = parent->die_descr;
  clone->say_descr = parent->say_descr;

  for (i = 0; i < 4; i++)
    clone->armor[i] = parent->armor[i];

  for (i = 0; i < MAX_STATS; i++)
  {
    clone->perm_stat[i] = parent->perm_stat[i];
    clone->mod_stat[i] = parent->mod_stat[i];
  }

  for (i = 0; i < 3; i++)
    clone->damage[i] = parent->damage[i];

  /* now add the affects */
  for (paf = parent->affected; paf != NULL; paf = paf->next)
    affect_to_char(clone, paf);

  for (prop = parent->property; prop; prop = prop->next)
    if (prop->sValue == NULL || prop->sValue[0] == 0)
      SetCharProperty(clone, prop->propIndex->type, prop->propIndex->key,
                      &prop->iValue);
    else
      SetCharProperty(clone, prop->propIndex->type, prop->propIndex->key,
                      prop->sValue);
}

/*
 * Create an instance of an object.
 */
OBJ_DATA *create_object(OBJ_INDEX_DATA * pObjIndex)
{
  OBJ_DATA *obj;
  int i;

  if (pObjIndex == NULL)
  {
    bug("Create_object: NULL pObjIndex.", 0);
    quit(1);
  }

  obj = new_obj();

  obj->pIndexData = pObjIndex;
  obj->in_room = NULL;
  obj->enchanted = false;

  obj->level = pObjIndex->level;
  obj->wear_loc = -1;

  obj->name = str_dup(pObjIndex->name); /* OLC */
  obj->short_descr = str_dup(pObjIndex->short_descr); /* OLC */
  obj->description = str_dup(pObjIndex->description); /* OLC */
  obj->material = str_dup(pObjIndex->material);
  obj->item_type = pObjIndex->item_type;
  obj->extra_flags = pObjIndex->extra_flags;
  obj->wear_flags = pObjIndex->wear_flags;
  obj->value[0] = pObjIndex->value[0];
  obj->value[1] = pObjIndex->value[1];
  obj->value[2] = pObjIndex->value[2];
  obj->value[3] = pObjIndex->value[3];
  obj->value[4] = pObjIndex->value[4];

  obj->weight = pObjIndex->weight;
  obj->clan = pObjIndex->clan;
  obj->class = pObjIndex->class;

  obj->cost = pObjIndex->cost;

  /* 
   * Mess with object properties.
   */
  switch (obj->item_type)
  {
    default:
      bug("Read_object: vnum %d bad type.", pObjIndex->vnum);
      break;

    case ITEM_LIGHT:
      if (obj->value[2] == 999)
        obj->value[2] = -1;
      break;
    case ITEM_SLOT_MACHINE:
      if (obj->value[2] > 5)
        obj->value[2] = 5;
      else if (obj->value[2] < 3)
        obj->value[2] = 3;
    case ITEM_FURNITURE:
    case ITEM_TRASH:
    case ITEM_CONTAINER:
    case ITEM_PIT:
    case ITEM_DRINK_CON:
    case ITEM_KEY:
    case ITEM_ITEMPILE:
    case ITEM_FOOD:
    case ITEM_BOAT:
    case ITEM_CORPSE_NPC:
    case ITEM_CORPSE_PC:
    case ITEM_FOUNTAIN:
    case ITEM_MAP:
    case ITEM_CLOTHING:
    case ITEM_PORTAL:
    case ITEM_TREASURE:
    case ITEM_WARP_STONE:
    case ITEM_DEMON_STONE:
    case ITEM_EXIT:
    case ITEM_ROOM_KEY:
    case ITEM_GEM:
    case ITEM_JEWELRY:
    case ITEM_SCROLL:
    case ITEM_WAND:
    case ITEM_STAFF:
    case ITEM_INSTRUMENT:
    case ITEM_WEAPON:
    case ITEM_ARMOR:
    case ITEM_POTION:
    case ITEM_PILL:
    case ITEM_MONEY:
      break;

    case ITEM_JUKEBOX:
      for (i = 0; i < 5; i++)
        obj->value[i] = -1;
      break;
  }

  obj->next = object_list;
  object_list = obj;
  pObjIndex->count++;

  return obj;
}

/* duplicate an object exactly -- except contents */
void clone_object(OBJ_DATA * parent, OBJ_DATA * clone)
{
  int i;
  //    AFFECT_DATA *paf;
  EXTRA_DESCR_DATA *ed, *ed_new;
  PROPERTY *prop;

  if (parent == NULL || clone == NULL)
    return;

  /* start fixing the object */
  clone->name = str_dup(parent->name);
  clone->short_descr = str_dup(parent->short_descr);
  clone->description = str_dup(parent->description);
  clone->item_type = parent->item_type;
  clone->extra_flags = parent->extra_flags;
  clone->wear_flags = parent->wear_flags;
  clone->weight = parent->weight;
  clone->cost = parent->cost;
  clone->level = parent->level;
  clone->condition = parent->condition;
  clone->material = str_dup(parent->material);
  clone->timer = parent->timer;
  clone->clan = parent->clan;

  for (i = 0; i < 5; i++)
    clone->value[i] = parent->value[i];

  /* extended desc */
  for (ed = parent->extra_descr; ed != NULL; ed = ed->next)
  {
    ed_new = new_extra_descr();
    ed_new->keyword = str_dup(ed->keyword);
    ed_new->description = str_dup(ed->description);
    ed_new->next = clone->extra_descr;
    clone->extra_descr = ed_new;
  }

  for (prop = parent->property; prop; prop = prop->next)
    if (prop->sValue == NULL || prop->sValue[0] == 0)
      SetObjectProperty(clone, prop->propIndex->type, prop->propIndex->key,
                        &prop->iValue);
    else
      SetObjectProperty(clone, prop->propIndex->type, prop->propIndex->key,
                        prop->sValue);
}

/*
 * Clear a new character.
 */
void clear_char(CHAR_DATA * ch)
{
  static CHAR_DATA ch_zero;
  int i;

  *ch = ch_zero;
  ch->name = &str_empty[0];
  ch->short_descr = &str_empty[0];
  ch->long_descr = &str_empty[0];
  ch->description = &str_empty[0];
  ch->prompt = &str_empty[0];
  ch->die_descr = str_dup("");
  ch->say_descr = str_dup("");
  ch->logon = current_time;
  ch->lines = PAGELEN;
  for (i = 0; i < 4; i++)
    ch->armor[i] = 100;
  ch->position = POS_STANDING;
  ch->hit = 100;
  ch->max_hit = 100;
  ch->mana = 100;
  ch->max_mana = 100;
  ch->move = 100;
  ch->max_move = 100;
  ch->on = NULL;
  for (i = 0; i < MAX_STATS; i++)
  {
    ch->perm_stat[i] = 13;
    ch->mod_stat[i] = 0;
  }
  return;
}

/*
 * Get an extra description from a list.
 */
char *get_extra_descr(const char *name, EXTRA_DESCR_DATA * ed)
{
  for (; ed != NULL; ed = ed->next)
  {
    if (is_name((char *) name, ed->keyword))
      return ed->description;
  }
  return NULL;
}

MPROG_CODE *get_mprog_index(long vnum)
{
  MPROG_CODE *prg;

  for (prg = mprog_list; prg; prg = prg->next)
  {
    if (prg->vnum == vnum)
      return (prg);
  }
  return NULL;
}

/*
 * Translates mob virtual number to its mob index struct.
 * Hash table lookup.
 */
MOB_INDEX_DATA *get_mob_index(long vnum)
{
  MOB_INDEX_DATA *pMobIndex;

  for (pMobIndex = mob_index_hash[vnum % MAX_KEY_HASH]; pMobIndex != NULL;
       pMobIndex = pMobIndex->next)
  {
    if (pMobIndex->vnum == vnum)
      return pMobIndex;
  }

  if (fBootDb)
  {
    bug("Get_mob_index: bad vnum %ld.", vnum);
    quit(1);
  }

  return NULL;
}

/*
 * Translates mob virtual number to its obj index struct.
 * Hash table lookup.
 */
OBJ_INDEX_DATA *get_obj_index(long vnum)
{
  OBJ_INDEX_DATA *pObjIndex;

  for (pObjIndex = obj_index_hash[vnum % MAX_KEY_HASH]; pObjIndex != NULL;
       pObjIndex = pObjIndex->next)
  {
    if (pObjIndex->vnum == vnum)
      return pObjIndex;
  }

  if (fBootDb)
  {
    bug("Get_obj_index: bad vnum %ld.", vnum);
    quit(1);
  }

  return NULL;
}

/*
 * Translates mob virtual number to its room index struct.
 * Hash table lookup.
 */
ROOM_INDEX_DATA *get_room_index(long vnum)
{
  ROOM_INDEX_DATA *pRoomIndex;

  for (pRoomIndex = room_index_hash[vnum % MAX_KEY_HASH]; pRoomIndex != NULL;
       pRoomIndex = pRoomIndex->next)
  {
    if (pRoomIndex->vnum == vnum)
      return pRoomIndex;
  }

  if (fBootDb)
  {
    bug("Get_room_index: bad vnum %ld.", vnum);
  }

  return NULL;
}

/*
 * Read a letter from a file.
 */
char fread_letter(FILE * fp)
{
  char c;

  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  return c;
}

/*
 * Read a number from a file.
 */
int fread_number(FILE * fp)
{
  int number;
  bool sign;
  char c;

  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  number = 0;

  sign = false;
  if (c == '+')
  {
    c = getc(fp);
  }
  else if (c == '-')
  {
    sign = true;
    c = getc(fp);
  }

  if (!isdigit(c))
  {
    bug("Fread_number: bad format.", 0);
    quit(1);
  }

  while (isdigit(c))
  {
    number = number * 10 + c - '0';
    c = getc(fp);
  }

  if (sign)
    number = 0 - number;

  if (c == '|')
    number += fread_number(fp);
  else if (c != ' ')
    ungetc(c, fp);

  return number;
}

/*
 * Read a number from a file in long int format.
 */
long fread_long(FILE * fp)
{
  long number;
  bool sign;
  char c;

  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  number = 0;

  sign = false;
  if (c == '+')
  {
    c = getc(fp);
  }
  else if (c == '-')
  {
    sign = true;
    c = getc(fp);
  }

  if (!isdigit(c))
  {
    bug("Fread_number: bad format.", 0);
    quit(1);
  }

  while (isdigit(c))
  {
    number = number * 10 + c - '0';
    c = getc(fp);
  }

  if (sign)
    number = 0 - number;

  if (c == '|')
    number += fread_long(fp);
  else if (c != ' ')
    ungetc(c, fp);

  return number;
}

long fread_flag(FILE * fp)
{
  int number;
  char c;
  bool negative = false;

  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  if (c == '-')
  {
    negative = true;
    c = getc(fp);
  }

  number = 0;

  if (!isdigit(c))
  {
    while (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'))
    {
      number += flag_convert(c);
      c = getc(fp);
    }
  }

  while (isdigit(c))
  {
    number = number * 10 + c - '0';
    c = getc(fp);
  }

  if (c == '|')
    number += fread_flag(fp);

  else if (c != ' ')
    ungetc(c, fp);

  if (negative)
    return -1 * number;

  return number;
}

long flag_convert(char letter)
{
  long bitsum = 0;
  char i;

  if ('A' <= letter && letter <= 'Z')
  {
    bitsum = 1;
    for (i = letter; i > 'A'; i--)
      bitsum *= 2;
  }
  else if ('a' <= letter && letter <= 'z')
  {
    bitsum = 67108864;          /* 2^26 */
    for (i = letter; i > 'a'; i--)
      bitsum *= 2;
  }

  return bitsum;
}

/*
 * Read and allocate space for a string from a file.
 * These strings are read-only and shared.
 * Strings are hashed:
 *   each string prepended with hash pointer to prev string,
 *   hash code is simply the string length.
 *   this function takes 40% to 50% of boot-up time.
 */
char *fread_string(FILE * fp)
{
  char *plast;
  char c;

  plast = top_string + sizeof(char *);
  if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
  {
    bug("Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
    quit(1);
  }

  /* 
   * Skip blanks.
   * Read first char.
   */
  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  if ((*plast++ = c) == '~')
    return &str_empty[0];

  for (;;)
  {
    /* 
     * Back off the char type lookup,
     *   it was too dirty for portability.
     *   -- Furey
     */

    switch (*plast = getc(fp))
    {
      default:
        plast++;
        break;

      case EOF:
        /* temp fix */
        bug("Fread_string: EOF", 0);
        return NULL;
        /* exit( 1 ); */
        break;

      case '\n':
        plast++;
        *plast++ = '\r';
        break;

      case '\r':
        break;

      case '~':
        plast++;
        {
          union
          {
            char *pc;
            char rgc[sizeof(char *)];
          }
          u1;
          int ic;
          int iHash;
          char *pHash;
          char *pHashPrev;
          char *pString;

          plast[-1] = '\0';
          iHash = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
          for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
          {
            for (ic = 0; ic < (int) sizeof(char *); ic++)
              u1.rgc[ic] = pHash[ic];
            pHashPrev = u1.pc;
            pHash += sizeof(char *);

            if (top_string[sizeof(char *)] == pHash[0] &&
                !str_cmp(top_string + sizeof(char *) + 1, pHash + 1))
              return pHash;
          }

          if (fBootDb)
          {
            pString = top_string;
            top_string = plast;
            u1.pc = string_hash[iHash];
            for (ic = 0; ic < (int) sizeof(char *); ic++)
              pString[ic] = u1.rgc[ic];
            string_hash[iHash] = pString;

            nAllocString += 1;
            sAllocString += top_string - pString;
            return pString + sizeof(char *);
          }
          else
          {
            return str_dup(top_string + sizeof(char *));
          }
        }
    }
  }
}

char *fread_string_eol(FILE * fp)
{
  static bool char_special[256 - EOF];
  char *plast;
  char c;

  if (char_special[EOF - EOF] != true)
  {
    char_special[EOF - EOF] = true;
    char_special['\n' - EOF] = true;
    char_special['\r' - EOF] = true;
  }

  plast = top_string + sizeof(char *);
  if (plast > &string_space[MAX_STRING - MAX_STRING_LENGTH])
  {
    bug("Fread_string: MAX_STRING %d exceeded.", MAX_STRING);
    quit(1);
  }

  /* 
   * Skip blanks.
   * Read first char.
   */
  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  if ((*plast++ = c) == '\n')
    return &str_empty[0];

  for (;;)
  {
    if (!char_special[(*plast++ = getc(fp)) - EOF])
      continue;

    switch (plast[-1])
    {
      default:
        break;

      case EOF:
        bug("Fread_string_eol  EOF", 0);
        quit(1);
        break;

      case '\n':
      case '\r':
        {
          union
          {
            char *pc;
            char rgc[sizeof(char *)];
          }
          u1;
          int ic;
          int iHash;
          char *pHash;
          char *pHashPrev;
          char *pString;

          plast[-1] = '\0';
          iHash = UMIN(MAX_KEY_HASH - 1, plast - 1 - top_string);
          for (pHash = string_hash[iHash]; pHash; pHash = pHashPrev)
          {
            for (ic = 0; ic < (int) sizeof(char *); ic++)
              u1.rgc[ic] = pHash[ic];
            pHashPrev = u1.pc;
            pHash += sizeof(char *);

            if (top_string[sizeof(char *)] == pHash[0] &&
                !str_cmp(top_string + sizeof(char *) + 1, pHash + 1))
              return pHash;
          }

          if (fBootDb)
          {
            pString = top_string;
            top_string = plast;
            u1.pc = string_hash[iHash];
            for (ic = 0; ic < (int) sizeof(char *); ic++)
              pString[ic] = u1.rgc[ic];
            string_hash[iHash] = pString;

            nAllocString += 1;
            sAllocString += top_string - pString;
            return pString + sizeof(char *);
          }
          else
          {
            return str_dup(top_string + sizeof(char *));
          }
        }
    }
  }
}

//
// Read a string from a file and store it in a static buffer (hence the temp).
//
char *fread_string_temp(FILE * fp)
{
  char *plast;
  char c;
  static char stringread[32768];
  // Make it big. very big. MSL isn't enough.
  // MSL is only 4608 bytes. The reading of
  // the beenthere data is 2*MAX_VNUMS/8,
  // which is 8192 bytes.
  plast = stringread;

  /*
   * Skip blanks.
   * Read first char.
   */
  do
  {
    c = getc(fp);
  }
  while (isspace(c));

  if ((*plast++ = c) == '~')
    return &str_empty[0];

  for (;;)
  {
    switch (*plast = getc(fp))
    {
      default:
        plast++;
        break;

      case EOF:
        bugf("fread_string(): EOF");
        abort();

      case '\n':
        plast++;
        *plast++ = '\r';
        break;

      case '\r':
        break;

      case '~':
        plast++;
        plast[-1] = 0;
        return stringread;
    }
  }
}


/*
 * Read to end of line (for comments).
 */
void fread_to_eol(FILE * fp)
{
  char c;

  do
  {
    c = getc(fp);
  }
  while (c != '\n' && c != '\r');

  do
  {
    c = getc(fp);
  }
  while (c == '\n' || c == '\r');

  ungetc(c, fp);
  return;
}

/*
 * Read one word (into static buffer).
 */
char *fread_word(FILE * fp)
{
  static char word[MAX_INPUT_LENGTH];
  char *pword;
  char cEnd;

  do
  {
    cEnd = getc(fp);
  }
  while (isspace(cEnd));

  if (cEnd == '\'' || cEnd == '"')
  {
    pword = word;
  }
  else
  {
    word[0] = cEnd;
    pword = word + 1;
    cEnd = ' ';
  }

  for (; pword < word + MAX_INPUT_LENGTH; pword++)
  {
    *pword = getc(fp);
    if (cEnd == ' ' ? isspace(*pword) : *pword == cEnd)
    {
      if (cEnd == ' ')
        ungetc(*pword, fp);
      *pword = '\0';
      return word;
    }
  }

  bug("Fread_word: word too long.", 0);
  quit(1);
  return NULL;
}

/*
 * Duplicate a string into dynamic memory.
 * Fread_strings are read-only and shared.
 */
char *str_dup(const char *str)
{
  char *str_new;

  if (str[0] == '\0')
    return &str_empty[0];

  if (str >= string_space && str < top_string)
    return (char *) str;

  str_new = malloc(strlen(str) + 1);
  memset(str_new, 0, strlen(str) + 1);
  strcpy(str_new, str);
  return str_new;
}

/*
 * Free a string.
 * Null is legal here to simplify callers.
 * Read-only shared strings are not touched.
 */
void _free_string(char *pstr, char *file, int line)
{
  if (pstr == NULL || pstr == &str_empty[0] ||
      (pstr >= string_space && pstr < top_string))
    return;

  free(pstr);
  return;
}

long convert_level(char *arg)
/* Code by Nebseni of Clandestine MUD */
{
  if (arg[0] == '\0')
    return 0;
  if (is_number(arg))
    return atoi(arg);
  else if (!str_cmp(arg, "imm") || !str_cmp(arg, "immortal"))
    return LEVEL_IMMORTAL;
  else if (!str_cmp(arg, "hero") || !str_cmp(arg, "hero+"))
    return LEVEL_HERO;
  else if (!str_cmp(arg, "ancient") || !str_cmp(arg, "ancient+"))
    return LEVEL_ANCIENT;
  else
    return 0;
}

long get_area_level(AREA_DATA * pArea)
/* Code by Nebseni of Clandestine MUD ...
   thanks to Erwin Andreasan for help with sscanf */
/* Returns long (MAX_LEVEL + 1)*low-level + high-level */
/* Example: Area with credits line: 
   {10 25} Nebseni  Clandestine Golf Course
   Max level 220
   returns 221*10 + 25 = 2235 */
/* Use modulo arithmetic for individual numbers, e.g.,
   lo_level = get_area_level(Area) / (MAX_LEVEL+1);
   hi_level = get_area_level(Area) % (MAX_LEVEL+1); */
{
  char high[MAX_STRING_LENGTH], low[MAX_STRING_LENGTH];

  if (2 != sscanf(pArea->credits, " %[^}]-%[^}]", low, high))
  {
    sscanf(pArea->credits, " %[^} ]", low);
    strcpy(high, "0");
  }
  return convert_level(low) * (MAX_LEVEL + 1) +
    ((convert_level(high) >
      0) ? convert_level(high) : (convert_level(low) ==
                                  LEVEL_ANCIENT) ? (LEVEL_IMMORTAL
                                                    - 1) : MAX_LEVEL);
}

void sort_areas_by_level(void)
/* Code by Nebseni of Clandestine MUD */
/* Creates a linked list starting with area_first_sorted, linking
   on pArea->next_sort. Sorted on get_area_level. */
{
  AREA_DATA *pArea1;
  AREA_DATA *pArea2;
  int thislevel;

  area_first_sorted = area_first;
  area_first_sorted->next_sort = NULL;

  /* if only one area it is already sorted! */
  if (area_first->next == NULL)
    return;

  /* iterate through the rest of the areas */
  for (pArea1 = area_first->next;;)
  {
    if ((thislevel =
         get_area_level(pArea1)) <= get_area_level(area_first_sorted))
    {
      /* this area goes at head of sorted list */
      pArea1->next_sort = area_first_sorted;
      area_first_sorted = pArea1;
    }
    else
    {
      /* iterate through list and insert appropriately */
      for (pArea2 = area_first_sorted;;)
      {
        if (pArea2->next_sort == NULL ||
            (thislevel <= get_area_level(pArea2->next_sort)))
        {
          pArea1->next_sort = pArea2->next_sort;
          pArea2->next_sort = pArea1;
          break;
        }
        pArea2 = pArea2->next_sort;
      }
    }
    if (pArea1->next != NULL)
    {
      pArea1 = pArea1->next;
    }
    else
      break;
  }
}

CH_CMD(do_areas)
{
  AREA_DATA *pArea;
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  int lo_level, hi_level;
  bool found = false;
  bool col = 0;

  argument = one_argument(argument, arg1);
  argument = one_argument(argument, arg2);

  lo_level =
    convert_level(arg1) ? URANGE(1, convert_level(arg1), MAX_LEVEL) : 0;
  hi_level =
    convert_level(arg2) ? URANGE(1, convert_level(arg2),
                                 MAX_LEVEL) : convert_level(arg1) ?
    lo_level : IS_IMMORTAL(ch) ? MAX_LEVEL : LEVEL_IMMORTAL - 1;

  printf_to_char(ch,
                 "\n\r{c=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=({YAreas{c)=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-={x\n\r\n\r");
  for (pArea = area_first_sorted;;)
  {
    if (pArea == NULL)
      break;

    if ((get_area_level(pArea) / (MAX_LEVEL + 1) <= hi_level) &&
        (get_area_level(pArea) % (MAX_LEVEL + 1) >= lo_level))
    {
      found = true;
      if (!strstr(pArea->builders, "Unlinked"))
        printf_to_char(ch, "{W[%-9s{W] " "{c%-30s " "{x({D%s{x)\n\r",
                       pArea->credits, pArea->name, pArea->builders);
    }
    if (pArea != NULL)
      pArea = pArea->next_sort;
    else
      break;
  }

  if (!found)
  {
    printf_to_char(ch, "No areas meeting those criteria.\n\r");
    return;
  }
  if (col)
  {
    printf_to_char(ch, "\n\r");
  }

  printf_to_char(ch,
                 "\n\r{c=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-={x\n\r");
  return;
}

CH_CMD(do_memory)
{
  char buf[MAX_STRING_LENGTH];

  sprintf(buf, "Affects %5d\n\r", top_affect);
  send_to_char(buf, ch);
  sprintf(buf, "Areas   %5d\n\r", top_area);
  send_to_char(buf, ch);
  sprintf(buf, "ExDes   %5d\n\r", top_ed);
  send_to_char(buf, ch);
  sprintf(buf, "Exits   %5d\n\r", top_exit);
  send_to_char(buf, ch);
  sprintf(buf, "Helps   %5d\n\r", top_help);
  send_to_char(buf, ch);
  sprintf(buf, "Socials %5d\n\r", maxSocial);
  send_to_char(buf, ch);
  sprintf(buf, "Mobs    %5d\n\r", top_mob_index);
  send_to_char(buf, ch);
  sprintf(buf, "(in use)%5d\n\r", mobile_count);
  send_to_char(buf, ch);
  sprintf(buf, "Objs    %5d\n\r", top_obj_index);
  send_to_char(buf, ch);
  sprintf(buf, "Resets  %5d\n\r", top_reset);
  send_to_char(buf, ch);
  sprintf(buf, "Rooms   %5d\n\r", top_room);
  send_to_char(buf, ch);
  sprintf(buf, "Shops   %5d\n\r", top_shop);
  send_to_char(buf, ch);
  sprintf(buf, "TopVnum %5ld\n\r", top_vnum);
  send_to_char(buf, ch);

  sprintf(buf, "Strings %5d strings of %7d kB (max %d kB).\n\r",
          nAllocString, sAllocString / 1024, MAX_STRING / 1024);
  send_to_char(buf, ch);

  return;
}

CH_CMD(do_dump)
{
  int count, count2, num_pcs, aff_count;
  CHAR_DATA *fch;
  MOB_INDEX_DATA *pMobIndex;
  PC_DATA *pc;
  OBJ_DATA *obj;
  OBJ_INDEX_DATA *pObjIndex;
  ROOM_INDEX_DATA *room;
  EXIT_DATA *exit;
  DESCRIPTOR_DATA *d;
  AFFECT_DATA *af;
  FILE *fp;
  long vnum;
  int nMatch = 0;

  /* open file */
  fp = file_open("mem.dmp", "w");

  /* report use of data structures */

  num_pcs = 0;
  aff_count = 0;

  /* mobile prototypes */
  fprintf(fp, "MobProt	%4d (%8d bytes)\n", top_mob_index,
          top_mob_index * (sizeof(*pMobIndex)));

  /* mobs */
  count = 0;
  count2 = 0;
  for (fch = char_list; fch != NULL; fch = fch->next)
  {
    count++;
    if (fch->pcdata != NULL)
      num_pcs++;
    for (af = fch->affected; af != NULL; af = af->next)
      aff_count++;
  }

  fprintf(fp, "Mobs	%4d (%8d bytes)\n", count, count * (sizeof(*fch)));

  /* pcdata */
  fprintf(fp, "Pcdata	%4d (%8d bytes)\n", num_pcs, num_pcs * (sizeof(*pc)));

  /* descriptors */
  count = 0;
  count2 = 0;
  for (d = descriptor_list; d != NULL; d = d->next)
    count++;

  fprintf(fp, "Descs	%4d (%8d bytes)\n", count, count * (sizeof(*d)));

  /* object prototypes */
  for (vnum = 0; nMatch < top_obj_index; vnum++)
    if ((pObjIndex = get_obj_index(vnum)) != NULL)
    {
      for (af = pObjIndex->affected; af != NULL; af = af->next)
        aff_count++;
      nMatch++;
    }

  fprintf(fp, "ObjProt	%4d (%8d bytes)\n", top_obj_index,
          top_obj_index * (sizeof(*pObjIndex)));

  /* objects */
  count = 0;
  count2 = 0;
  for (obj = object_list; obj != NULL; obj = obj->next)
  {
    count++;
    for (af = obj->affected; af != NULL; af = af->next)
      aff_count++;
  }

  fprintf(fp, "Objs	%4d (%8d bytes)\n", count, count * (sizeof(*obj)));

  fprintf(fp, "Affects	%4d (%8d bytes)\n", aff_count,
          aff_count * (sizeof(*af)));

  /* rooms */
  fprintf(fp, "Rooms	%4d (%8d bytes)\n", top_room,
          top_room * (sizeof(*room)));

  /* exits */
  fprintf(fp, "Exits	%4d (%8d bytes)\n", top_exit,
          top_exit * (sizeof(*exit)));

  file_close(fp);

  /* start printing out mobile data */
  fp = file_open("mob.dmp", "w");

  fprintf(fp, "\nMobile Analysis\n");
  fprintf(fp, "---------------\n");
  nMatch = 0;
  for (vnum = 0; nMatch < top_mob_index; vnum++)
    if ((pMobIndex = get_mob_index(vnum)) != NULL)
    {
      nMatch++;
      fprintf(fp, "#%ld %3d active %3d killed     %s\n",
              pMobIndex->vnum, pMobIndex->count, pMobIndex->killed,
              pMobIndex->short_descr);
    }
  file_close(fp);

  /* start printing out object data */
  fp = file_open("obj.dmp", "w");

  fprintf(fp, "\nObject Analysis\n");
  fprintf(fp, "---------------\n");
  nMatch = 0;
  for (vnum = 0; nMatch < top_obj_index; vnum++)
    if ((pObjIndex = get_obj_index(vnum)) != NULL)
    {
      nMatch++;
      fprintf(fp, "#%ld %3d active %3d reset      %s\n",
              pObjIndex->vnum, pObjIndex->count, pObjIndex->reset_num,
              pObjIndex->short_descr);
    }

  /* close file */
  file_close(fp);
}

/*
 * Stick a little fuzz on a number.
 */
int number_fuzzy(int number)
{
  switch (number_bits(2))
  {
    case 0:
      number -= 1;
      break;
    case 3:
      number += 1;
      break;
  }

  return UMAX(1, number);
}

/*
 * Generate a random number.
 */
int number_range(int from, int to)
{
  int power;
  int number;

  if (from == 0 && to == 0)
    return 0;

  if ((to = to - from + 1) <= 1)
    return from;

  for (power = 2; power < to; power <<= 1)
    ;

  while ((number = number_mm() & (power - 1)) >= to)
    ;

  return from + number;
}

/*
 * Generate a percentile roll.
 */
int number_percent(void)
{
  int percent;

  while ((percent = number_mm() & (128 - 1)) > 99)
    ;

  return 1 + percent;
}

/*
 * Generate a random door.
 */
int number_door(void)
{
  int door;

  while ((door = number_mm() & (8 - 1)) > 11)
    ;

  return door;
}

int number_bits(int width)
{
  return number_mm() & ((1 << width) - 1);
}

/*
 * I've gotten too many bad reports on OS-supplied random number generators.
 * This is the Mitchell-Moore algorithm from Knuth Volume II.
 * Best to leave the constants alone unless you've read Knuth.
 * -- Furey
 */

void init_mm()
{
  srandom(time(NULL) ^ getpid());
  return;
}

long number_mm(void)
{
  return random() >> 6;
}

/*
 * Roll some dice.
 */
int dice(int number, int size)
{
  int idice;
  int sum;

  switch (size)
  {
    case 0:
      return 0;
    case 1:
      return number;
  }

  for (idice = 0, sum = 0; idice < number; idice++)
    sum += number_range(1, size);

  return sum;
}

/*
 * Simple linear interpolation.
 */
int interpolate(int level, int value_00, int value_32)
{
  return value_00 + level * (value_32 - value_00) / 32;
}

/*
 * Removes the tildes from a string.
 * Used for player-entered strings that go into disk files.
 */
void smash_tilde(char *str)
{
  for (; *str != '\0'; str++)
  {
    if (*str == '~')
      *str = '-';
  }

  return;
}

/*
 * Compare strings, case insensitive.
 * Return true if different
 *   (compatibility with historical functions).
 */
bool str_cmp(const char *astr, const char *bstr)
{
  if (astr == NULL)
  {
    //        bug ( "Str_cmp: null astr.", 0 );
    return true;
  }

  if (bstr == NULL)
  {
    //        bug ( "Str_cmp: null bstr.", 0 );
    return true;
  }

  for (; *astr || *bstr; astr++, bstr++)
  {
    if (LOWER(*astr) != LOWER(*bstr))
      return true;
  }

  return false;
}

/*
 * Compare strings, case insensitive, for prefix matching.
 * Return true if astr not a prefix of bstr
 *   (compatibility with historical functions).
 */
bool str_prefix(const char *astr, const char *bstr)
{
  if (astr == NULL)
  {
    //        bug ( "Strn_cmp: null astr.", 0 );
    return true;
  }

  if (bstr == NULL)
  {
    //        bug ( "Strn_cmp: null bstr.", 0 );
    return true;
  }

  for (; *astr; astr++, bstr++)
  {
    if (LOWER(*astr) != LOWER(*bstr))
      return true;
  }

  return false;
}

/*
 * Compare strings, case sensitive, for prefix matching.
 * Return true if astr not a prefix of bstr
 *   (compatibility with historical functions).
 */
bool str_prefix_c(const char *astr, const char *bstr)
{
  if (astr == NULL)
  {
    //        bug ( "Strn_cmp: null astr.", 0 );
    return true;
  }

  if (bstr == NULL)
  {
    //        bug ( "Strn_cmp: null bstr.", 0 );
    return true;
  }

  for (; *astr; astr++, bstr++)
  {
    if (*astr != *bstr)
      return true;
  }

  return false;
}

/*
 * Compare strings, case insensitive, for match anywhere.
 * Returns true is astr not part of bstr.
 *   (compatibility with historical functions).
 */
bool str_infix(const char *astr, const char *bstr)
{
  int sstr1;
  int sstr2;
  int ichar;
  char c0;

  if ((c0 = LOWER(astr[0])) == '\0')
    return false;

  sstr1 = (int) strlen(astr);
  sstr2 = (int) strlen(bstr);

  for (ichar = 0; ichar <= sstr2 - sstr1; ichar++)
  {
    if (c0 == LOWER(bstr[ichar]) && !str_prefix(astr, bstr + ichar))
      return false;
  }

  return true;
}

/*
 * Compare strings, case sensitive, for match anywhere.
 * Returns true is astr not part of bstr.
 *   (compatibility with historical functions).
 */
bool str_infix_c(const char *astr, const char *bstr)
{
  int sstr1;
  int sstr2;
  int ichar;
  char c0;

  if ((c0 = astr[0]) == '\0')
    return false;

  sstr1 = (int) strlen(astr);
  sstr2 = (int) strlen(bstr);

  for (ichar = 0; ichar <= sstr2 - sstr1; ichar++)
  {
    if (c0 == bstr[ichar] && !str_prefix_c(astr, bstr + ichar))
      return false;
  }

  return true;
}

/*
 * Replace a substring in a string, case insensitive...Russ Walsh
 * looks for bstr within astr and replaces it with cstr.
 */
char *str_replace(char *astr, char *bstr, char *cstr)
{
  char newstr[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  bool found = false;
  int sstr1, sstr2;
  int ichar, jchar;
  char c0, c1, c2;

  if (((c0 = LOWER(astr[0])) == '\0') ||
      ((c1 = LOWER(bstr[0])) == '\0') || ((c2 = LOWER(cstr[0])) == '\0'))
    return astr;

  if (str_infix(bstr, astr))
    return astr;

  /* make sure we don't start an infinite loop */
  if (!str_infix(bstr, cstr))
    return astr;

  sstr1 = (int) strlen(astr);
  sstr2 = (int) strlen(bstr);
  jchar = 0;

  if (sstr1 < sstr2)
    return astr;

  for (ichar = 0; ichar <= sstr1 - sstr2; ichar++)
  {
    if (c1 == LOWER(astr[ichar]) && !str_prefix(bstr, astr + ichar))
    {
      found = true;
      jchar = ichar;
      ichar = sstr1;
    }
  }
  if (found)
  {
    buf[0] = '\0';
    for (ichar = 0; ichar < jchar; ichar++)
    {
      sprintf(newstr, "%c", astr[ichar]);
      strcat(buf, newstr);
    }
    strcat(buf, cstr);
    for (ichar = jchar + sstr2; ichar < sstr1; ichar++)
    {
      sprintf(newstr, "%c", astr[ichar]);
      strcat(buf, newstr);
    }
    sprintf(astr, "%s", str_replace(buf, bstr, cstr));
    return astr;
  }
  return astr;
}

/*
 * Replace a substring in a string, case sensitive...Russ Walsh
 * looks for bstr within astr and replaces it with cstr.
 */
char *str_replace_c(char *astr, char *bstr, char *cstr)
{
  char newstr[MAX_STRING_LENGTH];
  char buf[MAX_STRING_LENGTH];
  bool found = false;
  int sstr1, sstr2;
  int ichar, jchar;
  char c0, c1, c2;

  if (((c0 = astr[0]) == '\0') || ((c1 = bstr[0]) == '\0') ||
      ((c2 = cstr[0]) == '\0'))
    return astr;

  if (str_infix_c(bstr, astr))
    return astr;

  /* make sure we don't start an infinite loop */
  if (!str_infix_c(bstr, cstr))
    return astr;

  sstr1 = (int) strlen(astr);
  sstr2 = (int) strlen(bstr);
  jchar = 0;

  if (sstr1 < sstr2)
    return astr;

  for (ichar = 0; ichar <= sstr1 - sstr2; ichar++)
  {
    if (c1 == astr[ichar] && !str_prefix_c(bstr, astr + ichar))
    {
      found = true;
      jchar = ichar;
      ichar = sstr1;
    }
  }
  if (found)
  {
    buf[0] = '\0';
    for (ichar = 0; ichar < jchar; ichar++)
    {
      sprintf(newstr, "%c", astr[ichar]);
      strcat(buf, newstr);
    }
    strcat(buf, cstr);
    for (ichar = jchar + sstr2; ichar < sstr1; ichar++)
    {
      sprintf(newstr, "%c", astr[ichar]);
      strcat(buf, newstr);
    }
    sprintf(astr, "%s", str_replace_c(buf, bstr, cstr));
    return astr;
  }
  return astr;
}

/*
 * Compare strings, case insensitive, for suffix matching.
 * Return true if astr not a suffix of bstr
 *   (compatibility with historical functions).
 */
bool str_suffix(const char *astr, const char *bstr)
{
  int sstr1;
  int sstr2;

  sstr1 = (int) strlen(astr);
  sstr2 = (int) strlen(bstr);
  if (sstr1 <= sstr2 && !str_cmp(astr, bstr + sstr2 - sstr1))
    return false;
  else
    return true;
}

/*
 * Returns an initial-capped string.
 */
char *capitalize(const char *str)
{
  static char strcap[MAX_STRING_LENGTH];
  int i;

  for (i = 0; str[i] != '\0'; i++)
    strcap[i] = LOWER(str[i]);
  strcap[i] = '\0';
  strcap[0] = UPPER(strcap[0]);
  return strcap;
}

/*
 * Append a string to a file.
 */
void append_file(CHAR_DATA * ch, char *file, char *str)
{
  FILE *fp;

  if (IS_NPC(ch) || str[0] == '\0')
    return;

  if ((fp = file_open(file, "a")) == NULL)
  {
    perror(file);
    send_to_char("Could not open the file!\n\r", ch);
  }
  else
  {
    fprintf(fp, "[%ld] %s: %s\n", ch->in_room ? ch->in_room->vnum : 0,
            ch->name, str);
    file_close(fp);
  }
  return;
}

/*
 * Reports a bug.
 */
void bug(const char *str, int param)
{
  char buf[MAX_STRING_LENGTH];

  if (fpArea != NULL)
  {
    int iLine;
    int iChar;

    if (fpArea == stdin)
    {
      iLine = 0;
    }
    else
    {
      iChar = ftell(fpArea);
      fseek(fpArea, 0, 0);
      for (iLine = 0; ftell(fpArea) < iChar; iLine++)
      {
        while (getc(fpArea) != '\n')
          ;
      }
      fseek(fpArea, iChar, 0);
    }

    sprintf(buf, "[*****] FILE: %s LINE: %d", strArea, iLine);
    log_string(buf);
  }

  strcpy(buf, "[*****] BUG: ");
  sprintf(buf + strlen(buf), str, param);
  log_string(buf);
  wiznet(buf, NULL, NULL, WIZ_BUGS, 0, 0);

  return;
}

/*
 * Writes a string to the log.
 */
void log_string(const char *str)
{
  char *strtime;

  strtime = ctime(&current_time);
  strtime[strlen(strtime) - 1] = '\0';
  fprintf(stderr, "%s :: %s\n", strtime, str);

  return;

}

/*
 * This function is here to aid in debugging.
 * If the last expression in a function is another function call,
 *   gcc likes to generate a JMP instead of a CALL.
 * This is called "tail chaining."
 * It hoses the debugger call stack for that call.
 * So I make this the last call in certain critical functions,
 *   where I really need the call stack to be right for debugging!
 *
 * If you don't understand this, then LEAVE IT ALONE.
 * Don't remove any calls to tail_chain anywhere.
 *
 * -- Furey
 */
void tail_chain(void)
{
  return;
}

void randomize_entrances(long code)
{
  char buf[MAX_STRING_LENGTH];
  ROOM_INDEX_DATA *pRoomIndex;
  ROOM_INDEX_DATA *pToRoomIndex;
  OBJ_DATA *portal;
  OBJ_DATA *toportal;
  EXIT_DATA *pexit;
  int clannum, door, todoor;
  long room;

  if ((code == 0) || (code == ROOM_VNUM_CLANS))
  {
    for (clannum = 0; clannum < MAX_CLAN; clannum++)
    {
      room = clan_table[clannum].entrance;

      if (room == ROOM_VNUM_ALTAR)
        continue;

      if ((pRoomIndex = get_room_index(room)) == NULL)
      {
        bug("Clan Entrance: bad vnum %ld.", room);
        continue;
      }
      for (door = 0; door < 6; door++)
      {
        if (door == 5)
          todoor = 4;

        else if (door == 4)
          todoor = 5;
        else if (door < 2)
          todoor = door + 2;
        else
          todoor = door - 2;
        portal = get_obj_exit(dir_name[door], pRoomIndex->contents);
        if ((portal != NULL) && (portal->item_type == ITEM_EXIT))
        {
          pToRoomIndex = get_room_index(portal->value[0]);
          if (pToRoomIndex != NULL)
          {
            toportal = get_obj_exit(dir_name[todoor], pToRoomIndex->contents);
            if ((toportal != NULL) && (toportal->item_type == ITEM_EXIT))
            {
              extract_obj(toportal);
              toportal = NULL;
            }
          }
          extract_obj(portal);
          portal = NULL;
        }
      }
      for (;;)
      {
        door = number_range(0, 5);
        if (door == 5)
          todoor = 4;

        else if (door == 4)
          todoor = 5;
        else if (door < 2)
          todoor = door + 2;
        else
          todoor = door - 2;

        if ((pexit = pRoomIndex->exit[door]) == NULL)
        {
          for (;;)
          {
            pToRoomIndex = get_room_index(number_range(0, top_vnum));
            if (pToRoomIndex != NULL)
            {
              if (!IS_SET
                  (pToRoomIndex->room_flags, ROOM_PRIVATE) &&
                  !IS_SET(pToRoomIndex->room_flags, ROOM_SAFE)
                  && !IS_SET(pToRoomIndex->room_flags,
                             ROOM_SOLITARY) &&
                  !IS_SET(pToRoomIndex->room_flags,
                          ROOM_IMP_ONLY) &&
                  !IS_SET(pToRoomIndex->room_flags,
                          ROOM_GODS_ONLY) &&
                  !IS_SET(pToRoomIndex->room_flags,
                          ROOM_HEROES_ONLY) &&
                  !IS_SET(pToRoomIndex->room_flags,
                          ROOM_NEWBIES_ONLY) &&
                  !strstr(pToRoomIndex->area->builders,
                          "Unlinked") &&
                  !IS_SET(pToRoomIndex->room_flags, ROOM_LAW)
                  && !IS_SET(pToRoomIndex->room_flags,
                             ROOM_NOWHERE) &&
                  !IS_SET(pToRoomIndex->room_flags,
                          ROOM_LOCKED) &&
                  (pToRoomIndex->vnum != ROOM_VNUM_CHAIN) &&
                  (pToRoomIndex->exit[todoor] == NULL) &&
                  (pToRoomIndex->exit[todoor + 6] == NULL))
              {
                portal = get_obj_exit("exit", pRoomIndex->contents);
                if (portal == NULL)
                  break;
              }
            }
          }
          portal = create_object(get_obj_index(OBJ_VNUM_EXIT));
          sprintf(buf, "exit %s", dir_name[door]);
          free_string(portal->name);
          portal->name = str_dup(buf);
          free_string(portal->short_descr);
          portal->short_descr = str_dup(dir_name[door]);
          portal->value[0] = pToRoomIndex->vnum;
          obj_to_room(portal, pRoomIndex);
          toportal = create_object(get_obj_index(OBJ_VNUM_EXIT));
          sprintf(buf, "exit %s", dir_name[todoor]);
          free_string(toportal->name);
          toportal->name = str_dup(buf);
          free_string(toportal->short_descr);
          toportal->short_descr = str_dup(dir_name[todoor]);
          toportal->value[0] = pRoomIndex->vnum;
          obj_to_room(toportal, pToRoomIndex);
          buf[0] = '\0';
          break;
        }
      }
    }
    if (code != 0)
    {
      return;
    }
  }
}

/*
 * Load the bank information (economy info)
 * By Maniac from Mythran Mud
 */
void load_bank(void)
{
  FILE *fp;
  int number = 0;

  if (!(fp = file_open(BANK_FILE, "r")))
    return;

  for (;;)
  {
    char *word;
    char letter;

    do
    {
      letter = getc(fp);
      if (feof(fp))
      {
        file_close(fp);
        return;
      }
    }
    while (isspace(letter));
    ungetc(letter, fp);

    word = fread_word(fp);

    if (!str_cmp(word, "SHARE_VALUE"))
    {
      number = fread_number(fp);
      if (number > 0)
        share_value = number;
    }
  }
}
