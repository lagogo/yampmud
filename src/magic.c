
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "fd_property.h"

/*
 * Lookup a skill by name.
 */
int skill_lookup(const char *name)
{
  int sn;

  for (sn = 0; sn < MAX_SKILL; sn++)
  {
    if (!str_prefix(name, skill_table[sn].name))
      return sn;
  }
  return -1;
}

int find_spell(CHAR_DATA * ch, const char *name)
{
  /* finds a spell the character can cast if possible */
  int sn;

  if (IS_NPC(ch))
    return skill_lookup(name);

  for (sn = 1; sn < MAX_SKILL; sn++)
  {
    if (!str_prefix(name, skill_table[sn].name))
    {
      if (ch->pcdata->learned[sn] > 0)
        return sn;
    }
  }
  return -1;
}

/*
 * Lookup a skill by slot number.
 * Used for object loading.
 */
int slot_lookup(int slot)
{
  int sn;

  if (slot <= 0)
    return -1;

  for (sn = 2; sn < MAX_SKILL; sn++)
  {
    if (slot == skill_table[sn].slot)
      return sn;
  }

  if (fBootDb)
  {
    bug("Slot_lookup: bad slot %d.", slot);
    abort();
  }

  return -1;
}

/*
 * Utter mystical words for an sn.
 */
void say_spell(CHAR_DATA * ch, int sn)
{
  char buf[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  CHAR_DATA *rch;
  char *pName;
  int iSyl;
  int length;

  struct syl_type
  {
    char *old;
    char *new;
  };

  static const struct syl_type syl_table[] = {
    {" ", " "},
    {"ar", "abra"},
    {"au", "kada"},
    {"bless", "fido"},
    {"blind", "nose"},
    {"bur", "mosa"},
    {"ceit", "shartyn"},
    {"cu", "judi"},
    {"death", "khaos"},
    {"de", "oculo"},
    {"en", "unso"},
    {"geance", "sahtalus"},
    {"killer", "samoth"},
    {"light", "dies"},
    {"lo", "hi"},
    {"mor", "zak"},
    {"move", "sido"},
    {"ness", "lacri"},
    {"ning", "illa"},
    {"per", "duda"},
    {"ra", "gru"},
    {"fresh", "ima"},
    {"re", "candus"},
    {"son", "sabru"},
    {"tect", "infra"},
    {"tri", "cula"},
    {"ven", "nofo"},
    {"a", "a"}, {"b", "b"}, {"c", "q"}, {"d", "e"},
    {"e", "z"}, {"f", "y"}, {"g", "o"}, {"h", "p"},
    {"i", "u"}, {"j", "y"}, {"k", "t"}, {"l", "r"},
    {"m", "w"}, {"n", "i"}, {"o", "a"}, {"p", "s"},
    {"q", "d"}, {"r", "f"}, {"s", "g"}, {"t", "h"},
    {"u", "j"}, {"v", "z"}, {"w", "x"}, {"x", "n"},
    {"y", "l"}, {"z", "k"},
    {"", ""}
  };

  buf[0] = '\0';
  for (pName = skill_table[sn].name; *pName != '\0'; pName += length)
  {
    for (iSyl = 0; (length = (int) strlen(syl_table[iSyl].old)) != 0; iSyl++)
    {
      if (!str_prefix(syl_table[iSyl].old, pName))
      {
        strcat(buf, syl_table[iSyl].new);
        break;
      }
    }

    if (length == 0)
      length = 1;
  }

  sprintf(buf2, "$n utters the words, '%s'.", buf);
  sprintf(buf, "$n utters the words, '%s'.", skill_table[sn].name);

  for (rch = ch->in_room->people; rch; rch = rch->next_in_room)
  {
    if (rch != ch)
      act(ch->class == rch->class ? buf : buf2, ch, NULL, rch, TO_VICT);
  }

  return;
}

/*
 * Compute a saving throw.
 * Negative apply's make saving throw better.
 */
bool saves_spell(int level, CHAR_DATA * victim, int dam_type)
{
  int save;

  save = 50 + (victim->level - level) * 2 - victim->saving_throw;
  if (IS_AFFECTED(victim, AFF_BERSERK))
    save += victim->level / 2;

  switch (check_immune(victim, dam_type))
  {
    case IS_IMMUNE:
      return true;
    case IS_RESISTANT:
      save += 5;
      break;
    case IS_VULNERABLE:
      save -= 5;
      break;
  }

  if (!IS_NPC(victim) && class_table[victim->class].fMana)
    save = 9 * save / 10;
  save = URANGE(5, save, 95);
  return number_percent() < save;
}

/* RT save for dispels */

bool saves_dispel(int dis_level, int spell_level, int duration)
{
  int save;

  if (duration == -1)
    return true;
  //        spell_level += 5;
  // No, we dont want perm effects dispelled.
  /* very hard to dispel permanent effects */

  save = 50 + (spell_level - dis_level) * 4;
  save = URANGE(5, save, 95);
  return number_percent() < save;
}

/* co-routine for dispel magic and cancellation */

bool check_dispel(int dis_level, CHAR_DATA * victim, int sn)
{
  AFFECT_DATA *af;

  if (is_affected(victim, sn))
  {
    for (af = victim->affected; af != NULL; af = af->next)
    {
      if (af->type == sn)
      {
        if (!saves_dispel(dis_level, af->level, af->duration))
        {
          affect_strip(victim, sn);
          if (skill_table[sn].msg_off)
          {
            if (skill_table[sn].msg_off != NULL)
            {
              send_to_char(skill_table[sn].msg_off, victim);
              send_to_char("\n\r", victim);
            }
          }
          return true;
        }
        else
          af->level--;
      }
    }
  }
  return false;
}

/* for finding mana costs -- temporary version */
int mana_cost(CHAR_DATA * ch, int min_mana, int level)
{
  if (ch->level + 2 == level)
    return 1000;
  return UMAX(min_mana, (100 / (2 + ch->level - level)));
}

/*
 * The kludgy global is for spells who want more stuff from command line.
 */
char *target_name;
char *third_name;
char fullarg[MSL];

bool castkill = false;

CH_CMD(do_cast)
{
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  char arg3[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  void *vo;
  int mana;
  int sn;
  int i;
  int target;
  bool found;
  bool mobdeath = false;

  /* 
   * Switched NPC's can cast spells, but others can't.
   */
  if (IS_NPC(ch) && ch->desc == NULL)
    return;

  sprintf(fullarg, "%s", argument);
  target_name = one_argument(argument, arg1);
  third_name = one_argument(target_name, arg2);
  strcpy(target_name, arg2);
  one_argument(third_name, arg3);

  if (arg1[0] == '\0')
  {
    send_to_char("Cast which what where?\n\r", ch);
    return;
  }

  if (IS_SHIELDED(ch, SHD_SILENCE))
  {
    send_to_char("You are silenced and can not cast spells.", ch);
    return;
  }

  if (ch->stunned)
  {
    send_to_char("You're still a little woozy.\n\r", ch);
    return;
  }

  found = false;
  if ((sn = find_spell(ch, arg1)) > 0)
    for (i = 0; i < 5; i++)
    {
      if (pc_race_table[ch->race].skills[i] == NULL)
        break;
      if (!str_cmp(pc_race_table[ch->race].skills[i], skill_table[sn].name))
        found = true;
    }
  else
  {
    send_to_char("I dont recall any spells by that name.\n\r", ch);
    return;
  }

  if ((sn = find_spell(ch, arg1)) < 0 ||
      (!IS_NPC(ch) &&
       ((ch->level < skill_table[sn].skill_level[ch->class] && !found) ||
        ch->pcdata->learned[sn] == 0 ||
        skill_table[sn].spell_fun == spell_null)))
  {
    send_to_char("You don't know any spells of that name.\n\r", ch);
    return;
  }

  if (ch->position < skill_table[sn].minimum_position)
  {
    send_to_char("You can't concentrate enough.\n\r", ch);
    return;
  }

  if (ch->level + 2 == skill_table[sn].skill_level[ch->class])
    mana = 50;
  else
    mana =
      UMAX(skill_table[sn].min_mana,
           100 / (2 + ch->level - skill_table[sn].skill_level[ch->class]));

  if (!str_cmp(skill_table[sn].name, "restore mana"))
    mana = 1;

  /* 
   * Locate targets.
   */
  victim = NULL;
  obj = NULL;
  vo = NULL;
  target = TARGET_NONE;

  switch (skill_table[sn].target)
  {
    default:
      bug("Do_cast: bad target for sn %d.", sn);
      return;

    case TAR_IGNORE:
      break;

    case TAR_CHAR_OFFENSIVE:
      if (arg2[0] == '\0')
      {
        if ((victim = ch->fighting) == NULL)
        {
          send_to_char("Cast the spell on whom?\n\r", ch);
          return;
        }
      }
      else
      {
        if ((victim = get_char_room(ch, target_name)) == NULL)
        {
          send_to_char("They aren't here.\n\r", ch);
          return;
        }
      }

      if (!IS_NPC(ch))
      {

        if (is_safe(ch, victim) && victim != ch)
        {
          send_to_char("Not on that target.\n\r", ch);
          return;
        }
      }

      if (IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim)
      {
        send_to_char("You can't do that on your own follower.\n\r", ch);
        return;
      }

      if ((!IS_NPC(ch) && !IS_NPC(victim)) &&
          (!IS_IMMORTAL(ch)) && (!IS_IMMORTAL(victim)) &&
          (ch != victim) && (!skill_table[sn].socket) &&
          (!str_cmp(ch->pcdata->socket, victim->pcdata->socket)))
      {
        send_to_char("Spell failed.\n\r", ch);
        return;
      }

      vo = (void *) victim;
      target = TARGET_CHAR;
      break;

    case TAR_CHAR_DEFENSIVE:
      if (arg2[0] == '\0')
      {
        victim = ch;
      }
      else
      {
        if ((victim = get_char_room(ch, target_name)) == NULL)
        {
          send_to_char("They aren't here.\n\r", ch);
          return;
        }
      }

      if ((!IS_NPC(ch) && !IS_NPC(victim)) &&
          (!IS_IMMORTAL(ch)) && (!IS_IMMORTAL(victim)) &&
          (ch != victim) && (!skill_table[sn].socket) &&
          (!str_cmp(ch->pcdata->socket, victim->pcdata->socket)))
      {
        send_to_char("Spell failed.\n\r", ch);
        return;
      }

      vo = (void *) victim;
      target = TARGET_CHAR;
      break;

    case TAR_CHAR_SELF:
      if (arg2[0] != '\0' && !is_name(target_name, ch->name))
      {
        send_to_char("You cannot cast this spell on another.\n\r", ch);
        return;
      }

      vo = (void *) ch;
      target = TARGET_CHAR;
      break;

    case TAR_OBJ_INV:
      if (arg2[0] == '\0')
      {
        send_to_char("What should the spell be cast upon?\n\r", ch);
        return;
      }

      if ((obj = get_obj_carry(ch, target_name)) == NULL)
      {
        send_to_char("You are not carrying that.\n\r", ch);
        return;
      }

      vo = (void *) obj;
      target = TARGET_OBJ;
      break;

    case TAR_OBJ_CHAR_OFF:
      if (arg2[0] == '\0')
      {
        if ((victim = ch->fighting) == NULL)
        {
          send_to_char("Cast the spell on whom or what?\n\r", ch);
          return;
        }

        target = TARGET_CHAR;
      }
      else if ((victim = get_char_room(ch, target_name)) != NULL)
      {
        target = TARGET_CHAR;
      }

      if (target == TARGET_CHAR)  /* check the sanity of the attack */
      {
        if (is_safe_spell(ch, victim, false) && victim != ch)
        {
          send_to_char("Not on that target.\n\r", ch);
          return;
        }

        if (IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim)
        {
          send_to_char("You can't do that on your own follower.\n\r", ch);
          return;
        }

        if ((!IS_NPC(ch) && !IS_NPC(victim)) &&
            (!IS_IMMORTAL(ch)) && (!IS_IMMORTAL(victim)) &&
            (ch != victim) && (!skill_table[sn].socket) &&
            (!str_cmp(ch->pcdata->socket, victim->pcdata->socket)))
        {
          send_to_char("Spell failed.\n\r", ch);
          return;
        }

        vo = (void *) victim;
      }
      else if ((obj = get_obj_here(ch, target_name)) != NULL)
      {
        vo = (void *) obj;
        target = TARGET_OBJ;
      }
      else
      {
        send_to_char("You don't see that here.\n\r", ch);
        return;
      }
      break;

    case TAR_OBJ_CHAR_DEF:
      if (arg2[0] == '\0')
      {
        vo = (void *) ch;
        target = TARGET_CHAR;
      }
      else if ((victim = get_char_room(ch, target_name)) != NULL)
      {

        if ((!IS_NPC(ch) && !IS_NPC(victim)) &&
            (!IS_IMMORTAL(ch)) && (!IS_IMMORTAL(victim)) &&
            (ch != victim) && (!skill_table[sn].socket) &&
            (!str_cmp(ch->pcdata->socket, victim->pcdata->socket)))
        {
          send_to_char("Spell failed.\n\r", ch);
          return;
        }

        vo = (void *) victim;
        target = TARGET_CHAR;
      }
      else if ((obj = get_obj_carry(ch, target_name)) != NULL)
      {
        vo = (void *) obj;
        target = TARGET_OBJ;
      }
      else
      {
        send_to_char("You don't see that here.\n\r", ch);
        return;
      }
      break;

    case TAR_OBJ_TRAN:
      {
        if (arg2[0] == '\0')
        {
          send_to_char("Transport what to whom?\n\r", ch);
          return;
        }
        if (arg3[0] == '\0')
        {
          send_to_char("Transport it to whom?\n\r", ch);
          return;
        }
        if ((obj = get_obj_carry(ch, target_name)) == NULL)
        {
          send_to_char("You are not carrying that.\n\r", ch);
          return;
        }
        if ((victim = get_char_world(ch, third_name)) == NULL ||
            IS_NPC(victim))
        {
          send_to_char("They aren't here.\n\r", ch);
          return;
        }
        if (!IS_NPC(ch) && ch->mana < mana)
        {
          send_to_char("You don't have enough mana.\n\r", ch);
          return;
        }
        if (obj->wear_loc != WEAR_NONE)
        {
          send_to_char("You must remove it first.\n\r", ch);
          return;
        }
        if (IS_SET(victim->act, PLR_NOTRAN) && ch->level < SQUIRE)
        {
          send_to_char("They don't want it.\n\r", ch);
          return;
        }
        if (victim->carry_number + get_obj_number(obj) > can_carry_n(victim))
        {
          act("$N has $S hands full.", ch, NULL, victim, TO_CHAR);
          return;
        }
        if (get_carry_weight(victim) + get_obj_weight(obj) >
            can_carry_w(victim))
        {
          act("$N can't carry that much weight.", ch, NULL, victim, TO_CHAR);
          return;
        }
        if (!can_see_obj(victim, obj))
        {
          act("$N can't see it.", ch, NULL, victim, TO_CHAR);
          return;
        }
        if ((!IS_NPC(ch) && !IS_NPC(victim)) &&
            (!IS_IMMORTAL(ch)) && (!IS_IMMORTAL(victim)) &&
            (ch != victim) && (!skill_table[sn].socket) &&
            (!str_cmp(ch->pcdata->socket, victim->pcdata->socket)))
        {
          send_to_char("Spell failed.\n\r", ch);
          return;
        }
        if (!IS_IMMORTAL(ch))
          WAIT_STATE(ch, skill_table[sn].beats);
        if (!can_drop_obj(ch, obj) || IS_OBJ_STAT(obj, ITEM_QUEST))
        {
          send_to_char("It seems happy where it is.\n\r", ch);
          check_improve(ch, sn, false, 1);
          ch->mana -= mana / 3;
          return;
        }
        if ((obj->pIndexData->vnum == OBJ_VNUM_VOODOO) && (ch->level <= HERO))
        {
          send_to_char("You can't transport voodoo dolls.\n\r", ch);
          check_improve(ch, sn, false, 1);
          ch->mana -= mana / 3;
          return;
        }

        if (number_percent() > get_skill(ch, sn))
        {
          send_to_char("You lost your concentration.\n\r", ch);
          check_improve(ch, sn, false, 1);
          ch->mana -= mana / 2;
        }
        else
        {
          ch->mana -= mana;
          obj_from_char(obj);
          obj_to_char(obj, victim);
          act("$p glows {Ggreen{x, then disappears.", ch, obj, victim,
              TO_CHAR);
          act("$p suddenly appears in your inventory.", ch, obj, victim,
              TO_VICT);
          act("$p glows {Ggreen{x, then disappears from $n's inventory.", ch,
              obj, victim, TO_NOTVICT);
          check_improve(ch, sn, true, 1);
          if (IS_OBJ_STAT(obj, ITEM_FORCED) && (victim->level <= HERO))
          {
            do_wear(victim, obj->name);
          }
        }
      }
      return;
  }

  if (!IS_NPC(ch) && ch->mana < mana)
  {
    send_to_char("You don't have enough mana.\n\r", ch);
    return;
  }

  if (str_cmp(skill_table[sn].name, "ventriloquate"))
    say_spell(ch, sn);

  if (!IS_IMMORTAL(ch))
    WAIT_STATE(ch, skill_table[sn].beats);

  if (number_percent() > get_skill(ch, sn))
  {
    send_to_char("You lost your concentration.\n\r", ch);
    check_improve(ch, sn, false, 1);
    ch->mana -= mana / 2;
  }
  else
  {
    castkill = false;
    ch->mana -= mana;
    if (IS_NPC(ch) || class_table[ch->class].fMana)
      /* class has spells */
      (*skill_table[sn].spell_fun) (sn, ch->level, ch, vo, target);
    else
      (*skill_table[sn].spell_fun) (sn, 3 * ch->level / 4, ch, vo, target);
    check_improve(ch, sn, true, 1);
  }

  if (!castkill && (skill_table[sn].target == TAR_CHAR_OFFENSIVE ||
                    (skill_table[sn].target == TAR_OBJ_CHAR_OFF &&
                     target == TARGET_CHAR)) && victim != ch &&
      victim->master != ch)
  {
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;

    for (vch = ch->in_room->people; vch; vch = vch_next)
    {
      vch_next = vch->next_in_room;
      if (victim == vch && victim->fighting == NULL)
      {
        multi_hit(victim, ch, TYPE_UNDEFINED, &mobdeath);
        break;
      }
    }
  }
  castkill = false;
  return;
}

/*
 * Cast spells at targets using a magical object.
 */
void obj_cast_spell(int sn, int level, CHAR_DATA * ch, CHAR_DATA * victim,
                    OBJ_DATA * obj, bool * mobdeath)
{
  void *vo;
  int target = TARGET_NONE;

  if (sn <= 0)
    return;

  if (sn >= MAX_SKILL || skill_table[sn].spell_fun == 0)
  {
    bug("Obj_cast_spell: bad sn %d.", sn);
    return;
  }

  if (ch->fighting != NULL)
  {
    if (!IS_IMMORTAL(ch))
      WAIT_STATE(ch, skill_table[sn].beats);
  }

  switch (skill_table[sn].target)
  {
    default:
      bug("Obj_cast_spell: bad target for sn %d.", sn);
      return;

    case TAR_IGNORE:
      vo = NULL;
      break;

    case TAR_CHAR_OFFENSIVE:
      if (victim == NULL)
        victim = ch->fighting;
      if (victim == NULL)
      {
        send_to_char("You can't do that.\n\r", ch);
        return;
      }
      if (is_safe(ch, victim) && ch != victim)
      {
        send_to_char("Something isn't right...\n\r", ch);
        return;
      }
      vo = (void *) victim;
      target = TARGET_CHAR;
      break;

    case TAR_CHAR_DEFENSIVE:
    case TAR_CHAR_SELF:
      if (victim == NULL)
        victim = ch;
      vo = (void *) victim;
      target = TARGET_CHAR;
      break;

    case TAR_OBJ_INV:
      if (obj == NULL)
      {
        send_to_char("You can't do that.\n\r", ch);
        return;
      }
      vo = (void *) obj;
      target = TARGET_OBJ;
      break;

    case TAR_OBJ_CHAR_OFF:
      if (victim == NULL && obj == NULL)
      {
        if (ch->fighting != NULL)
          victim = ch->fighting;
        else
        {
          send_to_char("You can't do that.\n\r", ch);
          return;
        }
      }
      if (victim != NULL)
      {
        if (is_safe_spell(ch, victim, false) && ch != victim)
        {
          send_to_char("Somehting isn't right...\n\r", ch);
          return;
        }

        vo = (void *) victim;
        target = TARGET_CHAR;
      }
      else
      {
        vo = (void *) obj;
        target = TARGET_OBJ;
      }
      break;

    case TAR_OBJ_CHAR_DEF:
      if (victim == NULL && obj == NULL)
      {
        vo = (void *) ch;
        target = TARGET_CHAR;
      }
      else if (victim != NULL)
      {
        vo = (void *) victim;
        target = TARGET_CHAR;
      }
      else
      {
        vo = (void *) obj;
        target = TARGET_OBJ;
      }

      break;
  }

  target_name = "";
  (*skill_table[sn].spell_fun) (sn, level, ch, vo, target);

  if ((skill_table[sn].target == TAR_CHAR_OFFENSIVE ||
       (skill_table[sn].target == TAR_OBJ_CHAR_OFF &&
        target == TARGET_CHAR)) && victim != ch && victim->master != ch)
  {
    CHAR_DATA *vch;
    CHAR_DATA *vch_next;

    for (vch = ch->in_room->people; vch; vch = vch_next)
    {
      vch_next = vch->next_in_room;
      if (victim == vch && victim->fighting == NULL)
      {
        multi_hit(victim, ch, TYPE_UNDEFINED, mobdeath);
        break;
      }
    }
  }

  return;
}

/*
 * Spell functions.
 */
MAGIC(spell_acid_blast)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int adam, edam;
  bool mobdeath = false;

  adam = dice((level / 3), 33);
  edam = dice((level / 3), 33);

  if (saves_spell(level, victim, DAM_ACID))
    adam /= 1.5;
  if (saves_spell(level, victim, DAM_ENERGY))
    edam /= 1.5;

  if (victim->in_room == ch->in_room)
  {
    xdamage(ch, victim, adam, sn, DAM_ACID, true, VERBOSE_STD, &mobdeath);
    if (!mobdeath)
      xdamage(ch, victim, edam, sn, DAM_ENERGY, true, VERBOSE_STD, &mobdeath);
  }
  return;
}

MAGIC(spell_armor)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already armored.\n\r", ch);
      else
        act("$N is already armored.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = 2 + level;
  af.duration = 26;
  af.modifier = -30;
  af.location = APPLY_AC;
  af.bitvector = 0;
  affect_to_char(victim, &af);

  send_to_char("You feel someone protecting you.\n\r", victim);
  if (ch != victim && !silent)
    act("$N is protected by your magic.", ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_bless)
{
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  /* deal with the object case first */
  if (target == TARGET_OBJ)
  {
    obj = (OBJ_DATA *) vo;
    if (IS_OBJ_STAT(obj, ITEM_BLESS))
    {
      act("$p is already blessed.", ch, obj, NULL, TO_CHAR);
      return;
    }

    if (IS_OBJ_STAT(obj, ITEM_EVIL))
    {
      AFFECT_DATA *paf;

      paf = affect_find(obj->affected, gsn_curse);
      if (!saves_dispel(level, paf != NULL ? paf->level : obj->level, 0))
      {
        if (paf != NULL)
          affect_remove_obj(obj, paf);
        act("$p glows a pale blue.", ch, obj, NULL, TO_ALL);
        REMOVE_BIT(obj->extra_flags, ITEM_EVIL);
        return;
      }
      else
      {
        act("The evil of $p is too powerful for you to overcome.", ch,
            obj, NULL, TO_CHAR);
        return;
      }
    }

    af.where = TO_OBJECT;
    af.type = sn;
    af.level = level;
    af.duration = 20 + level;
    af.location = APPLY_SAVES;
    af.modifier = -5;
    af.bitvector = ITEM_BLESS;
    affect_to_obj(obj, &af);

    act("$p glows with a holy aura.", ch, obj, NULL, TO_ALL);
    return;
  }

  /* character target */
  victim = (CHAR_DATA *) vo;

  if (victim->position == POS_FIGHTING || is_affected(victim, sn))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already blessed.\n\r", ch);
      else
        act("$N already has divine favor.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = 20 + level;
  af.location = APPLY_HITROLL;
  af.modifier = level / 2;
  af.bitvector = 0;
  affect_to_char(victim, &af);
  af.bitvector = SHD_BLESS;
  af.location = APPLY_SAVING_SPELL;
  af.modifier = 0 - level / 2;
  affect_to_char(victim, &af);
  send_to_char("You feel righteous.\n\r", victim);

  if ((ch != victim) && !silent)
    act("You grant $N the favor of your god.", ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_blindness)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_BLIND) || saves_spell(level, victim, DAM_OTHER))
  {
    send_to_char("Your blindness fails.\n\r", ch);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level + 5;
  af.location = APPLY_HITROLL;
  af.modifier = -20;
  af.duration = 3 + level;
  af.bitvector = AFF_BLIND;
  affect_to_char(victim, &af);
  send_to_char("You are blinded!\n\r", victim);
  act("$n appears to be blinded.", victim, NULL, NULL, TO_ROOM);
  return;
}

MAGIC(spell_burning_hands)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  dam = number_range((15 + level) / 3, 12);
  if (saves_spell(level, victim, DAM_FIRE))
    dam /= 1.5;
  xdamage(ch, victim, dam, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_call_lightning)
{
  CHAR_DATA *vch;
  CHAR_DATA *vch_next;
  int dam;
  bool mobdeath = false;

  if (!IS_OUTSIDE(ch))
  {
    send_to_char("You must be out of doors.\n\r", ch);
    return;
  }

  if (weather_info.sky < SKY_RAINING)
  {
    send_to_char("You need bad weather.\n\r", ch);
    return;
  }

  dam = dice(level / 2, 8);

  act("$G's {Ylightning{x strikes your foes!", ch, NULL, NULL, TO_CHAR);
  act("$n calls $G's {Ylightning{x to strike $s foes!", ch, NULL, NULL,
      TO_ROOM);

  for (vch = char_list; vch != NULL; vch = vch_next)
  {
    vch_next = vch->next;
    if (vch->in_room == NULL)
      continue;
    if (vch->in_room == ch->in_room)
    {
      if (vch != ch && (IS_NPC(ch) ? !IS_NPC(vch) : IS_NPC(vch)))
      {
        if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(vch)))
        {
          ch->attacker = true;
          vch->attacker = false;
        }
        xdamage(ch, vch,
                saves_spell(level, vch, DAM_LIGHTNING) ? dam / 2 : dam, sn,
                DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
      }
      continue;
    }

    if (vch->in_room->area == ch->in_room->area && IS_OUTSIDE(vch) &&
        IS_AWAKE(vch))
      send_to_char("{z{YLightning{x flashes in the sky.\n\r", vch);
  }

  return;
}

/* RT calm spell stops all fighting in the room */

MAGIC(spell_calm)
{
  CHAR_DATA *vch;
  int mlevel = 0;
  int count = 0;
  int high_level = 0;
  int chance;
  AFFECT_DATA af;

  /* get sum of all mobile levels in the room */
  for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
  {
    if (vch->position == POS_FIGHTING)
    {
      count++;
      if (IS_NPC(vch))
        mlevel += vch->level;
      else
        mlevel += vch->level / 2;
      high_level = UMAX(high_level, vch->level);
    }
  }

  /* compute chance of stopping combat */
  chance = 4 * level - high_level + 2 * count;

  if (IS_IMMORTAL(ch))          /* always works */
    mlevel = 0;

  if (number_range(0, chance) >= mlevel)  /* hard to stop large fights */
  {
    for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
    {
      if (IS_NPC(vch) &&
          (IS_SET(vch->imm_flags, IMM_MAGIC) || IS_SET(vch->act, ACT_UNDEAD)))
        return;

      if (IS_AFFECTED(vch, AFF_CALM) ||
          IS_AFFECTED(vch, AFF_BERSERK) ||
          is_affected(vch, skill_lookup("frenzy")))
        return;

      send_to_char("A wave of calm passes over you.\n\r", vch);

      if (vch->fighting || vch->position == POS_FIGHTING)
        stop_fighting(vch, false);

      af.where = TO_AFFECTS;
      af.type = sn;
      af.level = level;
      af.duration = level / 4;

      af.location = APPLY_HITROLL;
      if (!IS_NPC(vch))
        af.modifier = -5;
      else
        af.modifier = -2;
      af.bitvector = AFF_CALM;
      affect_to_char(vch, &af);

      af.location = APPLY_DAMROLL;
      affect_to_char(vch, &af);
    }
  }
}

MAGIC(spell_cancellation)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool found = false;

  if (IS_SET(ch->in_room->room_flags, ROOM_ARENA))
  {
    send_to_char("Not while in the arena you dont.\n\r", ch);
    return;
  }

  level += 2;

  if (!IS_NPC(victim) && victim->position != POS_SITTING && ch != victim)
  {
    send_to_char("They must be sitting to be canceled.\n\r", ch);
    return;
  }

  if ((!IS_NPC(ch) && IS_NPC(victim) &&
       !(IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim)) ||
      (IS_NPC(ch) && !IS_NPC(victim)))
  {
    send_to_char("You failed, try dispel magic.\n\r", ch);
    return;
  }

  /* unlike dispel magic, the victim gets NO save */

  /* begin running through the spells */

  if (check_dispel(level, victim, skill_lookup("silence")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("armor")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("bless")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("blindness")))
  {
    found = true;
    act("$n is no longer blinded.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("calm")))
  {
    found = true;
    act("$n no longer looks so peaceful...", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("change sex")))
  {
    found = true;
    act("$n looks more like $mself again.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("charm person")))
  {
    found = true;
    act("$n regains $s free will.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("chill touch")))
  {
    found = true;
    act("$n looks warmer.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("curse")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("mana shield")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("lifeforce")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect evil")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect good")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect hidden")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect invis")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect hidden")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect magic")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("faerie fire")))
  {
    act("$n's outline fades.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("fly")))
  {
    act("$n falls to the ground!", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("frenzy")))
  {
    act("$n no longer looks so wild.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("giant strength")))
  {
    act("$n no longer looks so mighty.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("haste")))
  {
    act("$n is no longer moving so quickly.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("infravision")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("invis")))
  {
    act("$n fades into existance.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("mass invis")))
  {
    act("$n fades into existance.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("pass door")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("protection evil")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("protection good")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("sanctuary")))
  {
    act("The white aura around $n's body vanishes.", victim, NULL, NULL,
        TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("shield")))
  {
    act("The shield protecting $n vanishes.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("sleep")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("slow")))
  {
    act("$n is no longer moving so slowly.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("stone skin")))
  {
    act("$n's skin regains its normal texture.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("weaken")))
  {
    act("$n looks stronger.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("terror")))
  {
    act("$n is no longer so afraid.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (found)
    send_to_char("Ok.\n\r", ch);
  else
    send_to_char("Spell failed.\n\r", ch);
}

MAGIC(spell_cause_light)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dice(50, 15) + level, sn, DAM_HARM, true, VERBOSE_STD,
          &mobdeath);
  return;
}

MAGIC(spell_cause_critical)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dice(80, 25) + level, sn, DAM_HARM, true, VERBOSE_STD,
          &mobdeath);
  return;
}

MAGIC(spell_cause_serious)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dice(160, 50) + level, sn, DAM_HARM, true, VERBOSE_STD,
          &mobdeath);
  return;
}

MAGIC(spell_chain_lightning)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  CHAR_DATA *tmp_vict, *last_vict, *next_vict;
  bool found;
  int dam;
  bool mobdeath = false;


  /* first strike */

  act("A lightning bolt leaps from $n's hand and arcs to $N.", ch, NULL,
      victim, TO_ROOM);
  act("A lightning bolt leaps from your hand and arcs to $N.", ch, NULL,
      victim, TO_CHAR);
  act("A lightning bolt leaps from $n's hand and hits you!", ch, NULL,
      victim, TO_VICT);

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  dam = dice(level, 30);
  if (saves_spell(level, victim, DAM_LIGHTNING))
    dam /= 3;
  xdamage(ch, victim, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
  if (mobdeath)
    last_vict = NULL;
  else
    last_vict = victim;
  mobdeath = false;
  level -= 4;                   /* decrement damage */

  /* new targets */
  while (level > 0)
  {
    found = false;
    for (tmp_vict = ch->in_room->people; tmp_vict != NULL;
         tmp_vict = next_vict)
    {
      next_vict = tmp_vict->next_in_room;
      if (!is_safe_spell(ch, tmp_vict, true) && tmp_vict != last_vict)
      {
        found = true;
        last_vict = tmp_vict;
        act("The bolt arcs to $n!", tmp_vict, NULL, NULL, TO_ROOM);
        act("The bolt hits you!", tmp_vict, NULL, NULL, TO_CHAR);
        dam = dice(level, 25);
        if (saves_spell(level, tmp_vict, DAM_LIGHTNING))
          dam /= 3;
        xdamage(ch, tmp_vict, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD,
                &mobdeath);
        level -= 4;             /* decrement damage */
      }
    }                           /* end target searching loop */

    if (!found)                 /* no target found, hit the caster */
    {
      if (ch == NULL)
        return;

      if (last_vict == ch)      /* no double hits */
      {
        act("The bolt seems to have fizzled out.", ch, NULL, NULL, TO_ROOM);
        act("The bolt grounds out through your body.", ch, NULL, NULL,
            TO_CHAR);
        return;
      }

      last_vict = ch;
      act("The bolt arcs to $n...whoops!", ch, NULL, NULL, TO_ROOM);
      send_to_char("You are struck by your own lightning!\n\r", ch);
      dam = dice(level, 15);
      if (saves_spell(level, ch, DAM_LIGHTNING))
        dam /= 3;
      xdamage(ch, ch, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
      level -= 4;               /* decrement damage */

      if (ch == NULL)
        return;
    }
    /* now go back and find more targets */
  }
}

MAGIC(spell_change_sex)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  int change;

  if (is_affected(victim, sn))
  {
    if (victim == ch)
      send_to_char("You've already been changed.\n\r", ch);
    else
      act("$N has already had $s sex changed.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (saves_spell((level + 25), victim, DAM_OTHER))
  {
    act("$N resists the change.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (victim->sex == 0)
  {
    change = (number_range(1, 2));
  }
  else if (victim->sex == 2)
  {
    change = 0 - (number_range(1, 2));
  }
  else if (number_range(1, 20) <= 10)
  {
    change = -1;
  }
  else
  {
    change = 1;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = number_fuzzy(level / 20);
  af.location = APPLY_SEX;
  af.modifier = victim->sex + change;
  af.bitvector = 0;
  affect_to_char(victim, &af);

  send_to_char("You feel different.\n\r", victim);
  act("$n doesn't look like $mself anymore...", victim, NULL, NULL, TO_ROOM);
  return;
}

MAGIC(spell_charm_person)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (is_safe(ch, victim))
    return;

  if (victim == ch)
  {
    send_to_char("You like yourself even better!\n\r", ch);
    return;
  }

  if (IS_AFFECTED(victim, AFF_CHARM) || IS_AFFECTED(ch, AFF_CHARM) ||
      level < victim->level || IS_SET(victim->imm_flags, IMM_CHARM) ||
      saves_spell(level, victim, DAM_CHARM))
    return;

  if (IS_SET(victim->in_room->room_flags, ROOM_LAW))
  {
    send_to_char
      ("The King does not allow charming in the city limits.\n\r", ch);
    return;
  }

  /*    if ( ch->pet != NULL )
     {
     send_to_char ( "You can't have more than one pet.\n\r", ch );
     return;
     } */
  victim->ptype = 1;
  if (victim->master)
    stop_follower(victim);
  add_follower(victim, ch);
  ch->pet = victim;
  victim->leader = ch;
  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = number_fuzzy(level / 4);

  af.location = 0;
  af.modifier = 0;
  af.bitvector = AFF_CHARM;
  affect_to_char(victim, &af);
  act("Isn't $n just so nice?", ch, NULL, victim, TO_VICT);
  if (ch != victim)
  {
    act("$N looks at you with adoring eyes.", ch, NULL, victim, TO_CHAR);
  }
  return;
}

MAGIC(spell_chill_touch)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  int dam;
  bool mobdeath = false;

  if (is_affected(ch, sn))
  {
    if (victim == ch)
      send_to_char("Your skin is already as cold as it can be.\n\r", ch);
    else
      act("$N's skins already as cold as it can be.", ch, NULL, victim,
          TO_CHAR);
    return;
  }

  dam = level + dice(15, (level / 2));
  if (!saves_spell(level, victim, DAM_COLD))
  {
    act("$n turns blue and shivers.", victim, NULL, NULL, TO_ROOM);
    af.where = TO_AFFECTS;
    af.type = sn;
    af.level = level / 2;
    af.duration = 2;
    af.location = APPLY_STR;
    af.modifier = -2;
    af.bitvector = 0;
    affect_join(victim, &af);
  }
  else
    dam /= 1.5;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  xdamage(ch, victim, dam, sn, DAM_COLD, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_colour_spray)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, stun;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  dam = dice(level * 5, 5);
  stun =
    (ch->level / 7) + (get_curr_stat(ch, STAT_DEX) -
                       get_curr_stat(victim, STAT_WIS));
  if (saves_spell(level, victim, DAM_LIGHT))
    dam /= 1.5;
  else if (number_percent() <= stun)
  {
    send_to_char("You are mesmerized by the swirling colors.\n\r", victim);
    act("$n is mesmerized by the swirling colors.\n\r", victim, NULL, victim,
        TO_ROOM);
    victim->stunned = 2;
  }
  xdamage(ch, victim, dam, sn, DAM_LIGHT, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_continual_light)
{
  OBJ_DATA *light;

  if (target_name[0] != '\0')   /* do a glow on some object */
  {
    light = get_obj_carry(ch, target_name);

    if (light == NULL)
    {
      send_to_char("You don't see that here.\n\r", ch);
      return;
    }

    if (IS_OBJ_STAT(light, ITEM_GLOW))
    {
      act("$p is already glowing.", ch, light, NULL, TO_CHAR);
      return;
    }

    SET_BIT(light->extra_flags, ITEM_GLOW);
    act("$p glows with a white light.", ch, light, NULL, TO_ALL);
    return;
  }

  light = create_object(get_obj_index(OBJ_VNUM_LIGHT_BALL));
  obj_to_room(light, ch->in_room);
  act("$n twiddles $s thumbs and $p appears.", ch, light, NULL, TO_ROOM);
  act("You twiddle your thumbs and $p appears.", ch, light, NULL, TO_CHAR);
  return;
}

MAGIC(spell_control_weather)
{
  if (!str_cmp(target_name, "better"))
    weather_info.change += dice(level / 3, 4);

  else if (!str_cmp(target_name, "worse"))
    weather_info.change -= dice(level / 3, 4);

  else
    send_to_char("Do you want it to get better or worse?\n\r", ch);

  send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_create_food)
{
  OBJ_DATA *mushroom;

  mushroom = create_object(get_obj_index(OBJ_VNUM_MUSHROOM));
  mushroom->value[0] = level / 2;
  mushroom->value[1] = level;
  obj_to_room(mushroom, ch->in_room);
  act("$p suddenly appears.", ch, mushroom, NULL, TO_ROOM);
  act("$p suddenly appears.", ch, mushroom, NULL, TO_CHAR);
  return;
}

MAGIC(spell_create_rose)
{
  OBJ_DATA *rose;

  if (ch->alignment > 400)
  {
    rose = create_object(get_obj_index(OBJ_VNUM_WROSE));
    act("$n has created a beautiful {Wwhite rose{x.", ch, rose, NULL,
        TO_ROOM);
    send_to_char("You create a beautiful {Wwhite rose{x.\n\r", ch);
    obj_to_char(rose, ch);
    return;
  }
  if (ch->alignment < -400)
  {
    rose = create_object(get_obj_index(OBJ_VNUM_BROSE));
    act("$n has created a beautiful {Dblack rose{x.", ch, rose, NULL,
        TO_ROOM);
    send_to_char("You create a beautiful {Dblack rose{x.\n\r", ch);
    obj_to_char(rose, ch);
    return;
  }
  rose = create_object(get_obj_index(OBJ_VNUM_RROSE));
  act("$n has created a beautiful {Rred rose{x.", ch, rose, NULL, TO_ROOM);
  send_to_char("You create a beautiful {Rred rose{x.\n\r", ch);
  obj_to_char(rose, ch);
  return;
}

MAGIC(spell_create_spring)
{
  OBJ_DATA *spring;

  spring = create_object(get_obj_index(OBJ_VNUM_SPRING));
  spring->timer = level;
  obj_to_room(spring, ch->in_room);
  act("$p flows from the ground.", ch, spring, NULL, TO_ROOM);
  act("$p flows from the ground.", ch, spring, NULL, TO_CHAR);
  return;
}

MAGIC(spell_create_water)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  int water;

  if (obj->item_type != ITEM_DRINK_CON)
  {
    send_to_char("It is unable to hold water.\n\r", ch);
    return;
  }

  if (obj->value[2] != LIQ_WATER && obj->value[1] != 0)
  {
    send_to_char("It contains some other liquid.\n\r", ch);
    return;
  }

  water =
    UMIN(level * (weather_info.sky >= SKY_RAINING ? 4 : 2),
         obj->value[0] - obj->value[1]);

  if (water > 0)
  {
    obj->value[2] = LIQ_WATER;
    obj->value[1] += water;
    if (!is_name("water", obj->name))
    {
      char buf[MAX_STRING_LENGTH];

      sprintf(buf, "%s water", obj->name);
      free_string(obj->name);
      obj->name = str_dup(buf);
    }
    act("$p is filled.", ch, obj, NULL, TO_CHAR);
  }

  return;
}

MAGIC(spell_cure_blindness)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  if (!is_affected(victim, gsn_blindness))
  {
    if (victim == ch)
      send_to_char("You aren't blind.\n\r", ch);
    else
      act("$N doesn't appear to be blinded.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (check_dispel(level, victim, gsn_blindness))
  {
    send_to_char("Your vision returns!\n\r", victim);
    act("$n is no longer blinded.", victim, NULL, NULL, TO_ROOM);
  }
  else
    send_to_char("Spell failed.\n\r", ch);
}

MAGIC(spell_cure_critical)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int heal;

  heal = dice(3, 12) + level;
  victim->hit = UMIN(victim->hit + heal, victim->max_hit);
  update_pos(victim);
  send_to_char("You feel better!\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

/* RT added to cure plague */
MAGIC(spell_cure_disease)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  if (!is_affected(victim, gsn_plague))
  {
    if (victim == ch)
      send_to_char("You aren't ill.\n\r", ch);
    else
      act("$N doesn't appear to be diseased.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (check_dispel(level, victim, gsn_plague))
  {
    send_to_char("Your sores vanish.\n\r", victim);
    act("$n looks relieved as $s sores vanish.", victim, NULL, NULL, TO_ROOM);
  }
  else
    send_to_char("Spell failed.\n\r", ch);
}

MAGIC(spell_cure_light)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int heal;

  heal = dice(2, 8) + level / 3;
  victim->hit = UMIN(victim->hit + heal, victim->max_hit);
  update_pos(victim);
  send_to_char("You feel better!\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_cure_poison)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  if (!is_affected(victim, gsn_poison))
  {
    if (victim == ch)
      send_to_char("You aren't poisoned.\n\r", ch);
    else
      act("$N doesn't appear to be poisoned.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (check_dispel(level, victim, gsn_poison))
  {
    send_to_char("A warm feeling runs through your body.\n\r", victim);
    act("$n looks much better.", victim, NULL, NULL, TO_ROOM);
  }
  else
    send_to_char("Spell failed.\n\r", ch);
}

MAGIC(spell_cure_serious)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int heal;

  heal = dice(3, 8) + level / 2;
  victim->hit = UMIN(victim->hit + heal, victim->max_hit);
  update_pos(victim);
  send_to_char("You feel better!\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_curse)
{
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  AFFECT_DATA af;

  /* deal with the object case first */
  if (target == TARGET_OBJ)
  {
    obj = (OBJ_DATA *) vo;
    if (IS_OBJ_STAT(obj, ITEM_EVIL))
    {
      act("$p is already filled with evil.", ch, obj, NULL, TO_CHAR);
      return;
    }

    if (IS_OBJ_STAT(obj, ITEM_BLESS))
    {
      AFFECT_DATA *paf;

      paf = affect_find(obj->affected, skill_lookup("bless"));
      if (!saves_dispel(level, paf != NULL ? paf->level : obj->level, 0))
      {
        if (paf != NULL)
          affect_remove_obj(obj, paf);
        act("$p glows with a red aura.", ch, obj, NULL, TO_ALL);
        REMOVE_BIT(obj->extra_flags, ITEM_BLESS);
        return;
      }
      else
      {
        act("The holy aura of $p is too powerful for you to overcome.", ch,
            obj, NULL, TO_CHAR);
        return;
      }
    }

    af.where = TO_OBJECT;
    af.type = sn;
    af.level = level;
    af.duration = 2 * level;
    af.location = APPLY_SAVES;
    af.modifier = +5;
    af.bitvector = ITEM_EVIL;
    affect_to_obj(obj, &af);

    act("$p glows with a malevolent aura.", ch, obj, NULL, TO_ALL);
    return;
  }

  /* character curses */
  victim = (CHAR_DATA *) vo;

  if (IS_AFFECTED(victim, AFF_CURSE) ||
      saves_spell(level, victim, DAM_NEGATIVE))
  {
    send_to_char("Your curse fails.\n\r", ch);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = 2 * level;
  af.location = APPLY_HITROLL;
  af.modifier = -1 * (level / 8);
  af.bitvector = AFF_CURSE;
  affect_to_char(victim, &af);

  af.location = APPLY_SAVING_SPELL;
  af.modifier = level / 8;
  affect_to_char(victim, &af);

  send_to_char("You feel unclean.\n\r", victim);
  if (ch != victim)
    act("$N looks very uncomfortable.", ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_demonfire)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && !IS_EVIL(ch))
  {
    victim = ch;
    send_to_char("The demons turn upon you!\n\r", ch);
  }

  ch->alignment = UMAX(-1000, ch->alignment - 50);

  if (victim != ch)
  {
    act("$n calls forth the demons of Hell upon $N!", ch, NULL, victim,
        TO_ROOM);
    act("$n has assailed you with the demons of Hell!", ch, NULL, victim,
        TO_VICT);
    send_to_char("You conjure forth the demons of hell!\n\r", ch);
  }

  dam = dice((15 + level) / 2, 10);
  if (ch->alignment < victim->alignment)
  {
    if (ch->alignment < 0 && victim->alignment >= 0)
      dam += (victim->alignment - ch->alignment) * (level / 25);
    else if (ch->alignment < 0 && victim->alignment < 0)
      dam += 0 - (victim->alignment - ch->alignment) * (level / 25);
  }
  if (dam < 0)
    dam /= 5;

  spell_curse(gsn_curse, 3 * level / 4, ch, (void *) victim, TARGET_CHAR);
  xdamage(ch, victim, dam, sn, DAM_NEGATIVE, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_angeldust)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && !IS_GOOD(ch))
  {
    victim = ch;
    send_to_char("The angels turn upon you!\n\r", ch);
  }

  ch->alignment = UMIN(ch->alignment + 50, 1000);

  if (victim != ch)
  {
    act("$n calls forth heavenly angels to fight $N!", ch, NULL, victim,
        TO_ROOM);
    act("$n calls forth heavenly angels to attack you!", ch, NULL, victim,
        TO_VICT);
    send_to_char("You conjure forth a band of angels!\n\r", ch);
  }

  dam = dice((15 + level) / 2, 10);
  if (ch->alignment > victim->alignment)
  {
    if (ch->alignment > 0 && victim->alignment <= 0)
      dam += (ch->alignment - victim->alignment) * (level / 25);
    else if (ch->alignment > 0 && victim->alignment > 0)
      dam += 0 - (ch->alignment - victim->alignment) * (level / 25);
  }
  if (dam < 0)
    dam /= 5;

  spell_curse(gsn_curse, 3 * level / 4, ch, (void *) victim, TARGET_CHAR);
  xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_EARTH, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_detect_evil)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_DETECT_EVIL))
  {
    if (victim == ch)
      send_to_char("You can already sense evil.\n\r", ch);
    else
      act("$N can already detect evil.", ch, NULL, victim, TO_CHAR);
    return;
  }
  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.modifier = 0;
  af.location = APPLY_NONE;
  af.bitvector = AFF_DETECT_EVIL;
  affect_to_char(victim, &af);
  send_to_char("Your eyes tingle.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_detect_good)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_DETECT_GOOD))
  {
    if (victim == ch)
      send_to_char("You can already sense good.\n\r", ch);
    else
      act("$N can already detect good.", ch, NULL, victim, TO_CHAR);
    return;
  }
  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.modifier = 0;
  af.location = APPLY_NONE;
  af.bitvector = AFF_DETECT_GOOD;
  affect_to_char(victim, &af);
  send_to_char("Your eyes tingle.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_detect_hidden)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_DETECT_HIDDEN))
  {
    if (victim == ch)
      send_to_char("You are already as alert as you can be. \n\r", ch);
    else
      act("$N can already sense hidden lifeforms.", ch, NULL, victim,
          TO_CHAR);
    return;
  }
  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = AFF_DETECT_HIDDEN;
  affect_to_char(victim, &af);
  send_to_char("Your awareness improves.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_detect_invis)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_DETECT_INVIS))
  {
    if (victim == ch)
      send_to_char("You can already see invisible.\n\r", ch);
    else
      act("$N can already see invisible things.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.modifier = 0;
  af.location = APPLY_NONE;
  af.bitvector = AFF_DETECT_INVIS;
  affect_to_char(victim, &af);
  send_to_char("Your eyes tingle.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_detect_magic)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_DETECT_MAGIC))
  {
    if (victim == ch)
      send_to_char("You can already sense magical auras.\n\r", ch);
    else
      act("$N can already detect magic.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.modifier = 0;
  af.location = APPLY_NONE;
  af.bitvector = AFF_DETECT_MAGIC;
  affect_to_char(victim, &af);
  send_to_char("Your eyes tingle.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_detect_poison)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;

  if (obj->item_type == ITEM_DRINK_CON || obj->item_type == ITEM_FOOD)
  {
    if (obj->value[3] != 0)
      send_to_char("You smell poisonous fumes.\n\r", ch);
    else
      send_to_char("It looks delicious.\n\r", ch);
  }
  else
  {
    send_to_char("It doesn't look poisoned.\n\r", ch);
  }

  return;
}

MAGIC(spell_dispel_evil)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && IS_EVIL(ch))
    victim = ch;

  if (IS_GOOD(victim))
  {
    act("Light protects $N.", ch, NULL, victim, TO_ROOM);
    return;
  }

  if (IS_NEUTRAL(victim))
  {
    act("$N does not seem to be affected.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (victim->hit > (ch->level * 4))
    dam = dice(level, 4);

  else
    dam = UMAX(victim->hit, dice(level, 4));

  if (saves_spell(level, victim, DAM_HOLY))
    dam /= 2;
  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_dispel_good)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && IS_GOOD(ch))
    victim = ch;

  if (IS_EVIL(victim))
  {
    act("Darkness protects $N.", ch, NULL, victim, TO_ROOM);
    return;
  }

  if (IS_NEUTRAL(victim))
  {
    act("$N does not seem to be affected.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (victim->hit > (ch->level * 4))
    dam = dice(level, 4);

  else
    dam = UMAX(victim->hit, dice(level, 4));

  if (saves_spell(level, victim, DAM_NEGATIVE))
    dam /= 2;
  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dam, sn, DAM_NEGATIVE, true, VERBOSE_STD, &mobdeath);
  return;
}

/* modified for enhanced use */

MAGIC(spell_dispel_magic)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool found = false;

  if (saves_spell(level, victim, DAM_OTHER))
  {
    send_to_char("You feel a brief tingling sensation.\n\r", victim);
    send_to_char("You failed.\n\r", ch);
    return;
  }

  /* begin running through the spells */

  if (check_dispel(level, victim, skill_lookup("silence")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("armor")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("bless")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("blindness")))
  {
    found = true;
    act("$n is no longer blinded.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("calm")))
  {
    found = true;
    act("$n no longer looks so peaceful...", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("change sex")))
  {
    found = true;
    act("$n looks more like $mself again.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("charm person")))
  {
    found = true;
    act("$n regains $s free will.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("chill touch")))
  {
    found = true;
    act("$n looks warmer.", victim, NULL, NULL, TO_ROOM);
  }

  if (check_dispel(level, victim, skill_lookup("curse")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect evil")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect good")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect hidden")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect invis")))
    found = true;

  found = true;

  if (check_dispel(level, victim, skill_lookup("detect hidden")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("detect magic")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("faerie fire")))
  {
    act("$n's outline fades.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("fly")))
  {
    act("$n falls to the ground!", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("frenzy")))
  {
    act("$n no longer looks so wild.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("giant strength")))
  {
    act("$n no longer looks so mighty.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("haste")))
  {
    act("$n is no longer moving so quickly.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("infravision")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("invis")))
  {
    act("$n fades into existance.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("mass invis")))
  {
    act("$n fades into existance.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("pass door")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("protection evil")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("protection good")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("terror")))
  {
    act("$n is no longer so afraid.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (IS_SHIELDED(victim, SHD_SANCTUARY) &&
      !saves_dispel(level, victim->level, -1) &&
      !is_affected(victim, skill_lookup("sanctuary")))
  {
    REMOVE_BIT(victim->shielded_by, SHD_SANCTUARY);
    act("The white aura around $n's body vanishes.", victim, NULL, NULL,
        TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("shield")))
  {
    act("The shield protecting $n vanishes.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("sleep")))
    found = true;

  if (check_dispel(level, victim, skill_lookup("slow")))
  {
    act("$n is no longer moving so slowly.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("stone skin")))
  {
    act("$n's skin regains its normal texture.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (check_dispel(level, victim, skill_lookup("weaken")))
  {
    act("$n looks stronger.", victim, NULL, NULL, TO_ROOM);
    found = true;
  }

  if (found)
    send_to_char("Ok.\n\r", ch);
  else
    send_to_char("Spell failed.\n\r", ch);
  return;
}

MAGIC(spell_earthquake)
{
  CHAR_DATA *vch;
  CHAR_DATA *vch_next;
  bool mobdeath = false;

  send_to_char("The earth trembles beneath your feet!\n\r", ch);
  act("$n makes the earth tremble and shiver.", ch, NULL, NULL, TO_ROOM);

  for (vch = char_list; vch != NULL; vch = vch_next)
  {
    vch_next = vch->next;
    mobdeath = false;
    if (vch->in_room == NULL)
      continue;
    if (vch->in_room == ch->in_room)
    {
      if (vch != ch && !is_safe_spell(ch, vch, true) &&
          !IS_AFFECTED(vch, AFF_FLYING))
      {
        if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(vch)))
        {
          ch->attacker = true;
          vch->attacker = false;
        }
        xdamage(ch, vch, level + dice(level + 2, 5), sn, DAM_EARTH, true,
                VERBOSE_STD, &mobdeath);
      }
      continue;
    }

    if (vch->in_room->area == ch->in_room->area)
      send_to_char("The earth trembles and shivers.\n\r", vch);
  }

  return;
}

MAGIC(spell_enchant_armor)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  AFFECT_DATA *paf;
  int result, fail;
  int ac_bonus, added;
  bool ac_found = false;

  if (obj->item_type != ITEM_ARMOR)
  {
    send_to_char("That isn't an armor.\n\r", ch);
    return;
  }

  if (IS_OBJ_STAT(obj, ITEM_QUEST))
  {
    send_to_char("You can't enchant quest items.\n\r", ch);
    return;
  }

  if (obj->wear_loc != -1)
  {
    send_to_char("The item must be carried to be enchanted.\n\r", ch);
    return;
  }

  /* this means they have no bonus */
  ac_bonus = 0;
  fail = 25;                    /* base 25% chance of failure */

  /* find the bonuses */

  if (!obj->enchanted)
    for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location == APPLY_AC)
      {
        ac_bonus = paf->modifier;
        ac_found = true;
        fail += 5 * (ac_bonus * ac_bonus);
      }

      else                      /* things get a little harder */
        fail += 20;
    }

  for (paf = obj->affected; paf != NULL; paf = paf->next)
  {
    if (paf->location == APPLY_AC)
    {
      ac_bonus = paf->modifier;
      ac_found = true;
      fail += 5 * (ac_bonus * ac_bonus);
    }

    else                        /* things get a little harder */
      fail += 20;
  }

  /* apply other modifiers */
  fail -= level;

  if (IS_OBJ_STAT(obj, ITEM_BLESS))
    fail -= 15;
  if (IS_OBJ_STAT(obj, ITEM_GLOW))
    fail -= 5;

  fail = URANGE(5, fail, 85);

  result = number_percent();

  /* the moment of truth */
  if (result < (fail / 5))      /* item destroyed */
  {
    act("$p flares blindingly... and evaporates!", ch, obj, NULL, TO_CHAR);
    act("$p flares blindingly... and evaporates!", ch, obj, NULL, TO_ROOM);
    extract_obj(obj);
    obj = NULL;
    return;
  }

  if (result < (fail / 3))      /* item disenchanted */
  {
    AFFECT_DATA *paf_next;

    act("$p glows brightly, then fades...oops.", ch, obj, NULL, TO_CHAR);
    act("$p glows brightly, then fades.", ch, obj, NULL, TO_ROOM);
    obj->enchanted = true;

    /* remove all affects */
    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
      paf_next = paf->next;
      free_affect(paf);
    }
    obj->affected = NULL;

    /* clear all flags */
    obj->extra_flags = 0;
    return;
  }

  if (result <= fail)           /* failed, no bad result */
  {
    send_to_char("Nothing seemed to happen.\n\r", ch);
    return;
  }

  /* okay, move all the old flags into new vectors if we have to */
  if (!obj->enchanted)
  {
    AFFECT_DATA *af_new;

    obj->enchanted = true;

    for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
    {
      af_new = new_affect();

      af_new->next = obj->affected;
      obj->affected = af_new;

      af_new->where = paf->where;
      af_new->type = UMAX(0, paf->type);
      af_new->level = paf->level;
      af_new->duration = paf->duration;
      af_new->location = paf->location;
      af_new->modifier = paf->modifier;
      af_new->bitvector = paf->bitvector;
    }
  }

  if (result <= (90 - level / 5)) /* success! */
  {
    act("$p shimmers with a gold aura.", ch, obj, NULL, TO_CHAR);
    act("$p shimmers with a gold aura.", ch, obj, NULL, TO_ROOM);
    SET_BIT(obj->extra_flags, ITEM_MAGIC);
    added = -1;
  }

  else                          /* exceptional enchant */
  {
    act("$p glows a brillant gold!", ch, obj, NULL, TO_CHAR);
    act("$p glows a brillant gold!", ch, obj, NULL, TO_ROOM);
    SET_BIT(obj->extra_flags, ITEM_MAGIC);
    SET_BIT(obj->extra_flags, ITEM_GLOW);
    added = -2;
  }

  /* now add the enchantments */

  if (obj->level < LEVEL_ANCIENT)
    obj->level = UMIN(LEVEL_ANCIENT - 1, obj->level + 1);

  if (ac_found)
  {
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location == APPLY_AC)
      {
        paf->type = sn;
        paf->modifier += added;
        paf->level = UMAX(paf->level, level);
      }
    }
  }
  else                          /* add a new affect */
  {
    paf = new_affect();

    paf->where = TO_OBJECT;
    paf->type = sn;
    paf->level = level;
    paf->duration = -1;
    paf->location = APPLY_AC;
    paf->modifier = added;
    paf->bitvector = 0;
    paf->next = obj->affected;
    obj->affected = paf;
  }

}

MAGIC(spell_enchant_weapon)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  AFFECT_DATA *paf;
  int result, fail;
  int hit_bonus, dam_bonus, added;
  bool hit_found = false, dam_found = false;

  if (obj->item_type != ITEM_WEAPON)
  {
    send_to_char("That isn't a weapon.\n\r", ch);
    return;
  }

  if (IS_OBJ_STAT(obj, ITEM_QUEST))
  {
    send_to_char("You can't enchant quest items.\n\r", ch);
    return;
  }

  if (obj->wear_loc != -1)
  {
    send_to_char("The item must be carried to be enchanted.\n\r", ch);
    return;
  }

  /* this means they have no bonus */
  hit_bonus = 0;
  dam_bonus = 0;
  fail = 25;                    /* base 25% chance of failure */

  /* find the bonuses */

  if (!obj->enchanted)
    for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location == APPLY_HITROLL)
      {
        hit_bonus = paf->modifier;
        hit_found = true;
        fail += 2 * (hit_bonus * hit_bonus);
      }

      else if (paf->location == APPLY_DAMROLL)
      {
        dam_bonus = paf->modifier;
        dam_found = true;
        fail += 2 * (dam_bonus * dam_bonus);
      }

      else                      /* things get a little harder */
        fail += 25;
    }

  for (paf = obj->affected; paf != NULL; paf = paf->next)
  {
    if (paf->location == APPLY_HITROLL)
    {
      hit_bonus = paf->modifier;
      hit_found = true;
      fail += 2 * (hit_bonus * hit_bonus);
    }

    else if (paf->location == APPLY_DAMROLL)
    {
      dam_bonus = paf->modifier;
      dam_found = true;
      fail += 2 * (dam_bonus * dam_bonus);
    }

    else                        /* things get a little harder */
      fail += 25;
  }

  /* apply other modifiers */
  fail -= 3 * level / 2;

  if (IS_OBJ_STAT(obj, ITEM_BLESS))
    fail -= 15;
  if (IS_OBJ_STAT(obj, ITEM_GLOW))
    fail -= 5;

  fail = URANGE(5, fail, 95);

  result = number_percent();

  /* the moment of truth */
  if (result < (fail / 5))      /* item destroyed */
  {
    act("$p shivers violently and explodes!", ch, obj, NULL, TO_CHAR);
    act("$p shivers violently and explodeds!", ch, obj, NULL, TO_ROOM);
    extract_obj(obj);
    obj = NULL;
    return;
  }

  if (result < (fail / 2))      /* item disenchanted */
  {
    AFFECT_DATA *paf_next;

    act("$p glows brightly, then fades...oops.", ch, obj, NULL, TO_CHAR);
    act("$p glows brightly, then fades.", ch, obj, NULL, TO_ROOM);
    obj->enchanted = true;

    /* remove all affects */
    for (paf = obj->affected; paf != NULL; paf = paf_next)
    {
      paf_next = paf->next;
      free_affect(paf);
    }
    obj->affected = NULL;

    /* clear all flags */
    obj->extra_flags = 0;
    return;
  }

  if (result <= fail)           /* failed, no bad result */
  {
    send_to_char("Nothing seemed to happen.\n\r", ch);
    return;
  }

  /* okay, move all the old flags into new vectors if we have to */
  if (!obj->enchanted)
  {
    AFFECT_DATA *af_new;

    obj->enchanted = true;

    for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
    {
      af_new = new_affect();

      af_new->next = obj->affected;
      obj->affected = af_new;

      af_new->where = paf->where;
      af_new->type = UMAX(0, paf->type);
      af_new->level = paf->level;
      af_new->duration = paf->duration;
      af_new->location = paf->location;
      af_new->modifier = paf->modifier;
      af_new->bitvector = paf->bitvector;
    }
  }

  if (result <= (100 - level / 5))  /* success! */
  {
    act("$p glows blue.", ch, obj, NULL, TO_CHAR);
    act("$p glows blue.", ch, obj, NULL, TO_ROOM);
    SET_BIT(obj->extra_flags, ITEM_MAGIC);
    added = 1;
  }

  else                          /* exceptional enchant */
  {
    act("$p glows a brillant blue!", ch, obj, NULL, TO_CHAR);
    act("$p glows a brillant blue!", ch, obj, NULL, TO_ROOM);
    SET_BIT(obj->extra_flags, ITEM_MAGIC);
    SET_BIT(obj->extra_flags, ITEM_GLOW);
    added = 2;
  }

  /* now add the enchantments */

  if (obj->level < LEVEL_ANCIENT - 1)
    obj->level = UMIN(LEVEL_ANCIENT - 1, obj->level + 1);

  if (dam_found)
  {
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location == APPLY_DAMROLL)
      {
        paf->type = sn;
        paf->modifier += added;
        paf->level = UMAX(paf->level, level);
        if (paf->modifier > 4)
          SET_BIT(obj->extra_flags, ITEM_HUM);
      }
    }
  }
  else                          /* add a new affect */
  {
    paf = new_affect();

    paf->where = TO_OBJECT;
    paf->type = sn;
    paf->level = level;
    paf->duration = -1;
    paf->location = APPLY_DAMROLL;
    paf->modifier = added;
    paf->bitvector = 0;
    paf->next = obj->affected;
    obj->affected = paf;
  }

  if (hit_found)
  {
    for (paf = obj->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location == APPLY_HITROLL)
      {
        paf->type = sn;
        paf->modifier += added;
        paf->level = UMAX(paf->level, level);
        if (paf->modifier > 4)
          SET_BIT(obj->extra_flags, ITEM_HUM);
      }
    }
  }
  else                          /* add a new affect */
  {
    paf = new_affect();

    paf->type = sn;
    paf->level = level;
    paf->duration = -1;
    paf->location = APPLY_HITROLL;
    paf->modifier = added;
    paf->bitvector = 0;
    paf->next = obj->affected;
    obj->affected = paf;
  }

}

/*
 * Drain XP, MANA, HP.
 * Caster gains HP.
 */
MAGIC(spell_energy_drain)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (victim != ch)
  {
    ch->alignment = UMAX(-1000, ch->alignment - 50);
    if (ch->pet != NULL)
      ch->pet->alignment = ch->alignment;
  }

  if (saves_spell(level, victim, DAM_NEGATIVE))
  {
    send_to_char("You feel a momentary chill.\n\r", victim);
    send_to_char("You think of cute puppies instead.\n\r", ch);
    return;
  }

  if (victim->level <= 2)
  {
    dam = ch->hit + 1;
  }
  else
  {
    gain_exp(victim, 0 - number_range(level / 2, 3 * level / 2));
    victim->mana /= 2;
    victim->move /= 2;
    dam = dice(1, level);
    ch->hit += dam;
  }
  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  send_to_char("You feel your life slipping away!\n\r", victim);
  send_to_char("Wow....what a rush!\n\r", ch);
  xdamage(ch, victim, dam, sn, DAM_NEGATIVE, true, VERBOSE_STD, &mobdeath);

  return;
}

MAGIC(spell_fireball)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  dam = dice(level * 2, 33);
  if (saves_spell(level, victim, DAM_FIRE))
    dam /= 1.5;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  fire_effect(victim, level, dam, TARGET_CHAR);
  xdamage(ch, victim, dam, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_fireproof)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  AFFECT_DATA af;

  if (IS_OBJ_STAT(obj, ITEM_BURN_PROOF))
  {
    act("$p is already protected from burning.", ch, obj, NULL, TO_CHAR);
    return;
  }

  af.where = TO_OBJECT;
  af.type = sn;
  af.level = level;
  af.duration = number_fuzzy(level / 4);
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = ITEM_BURN_PROOF;

  affect_to_obj(obj, &af);

  act("You protect $p from fire.", ch, obj, NULL, TO_CHAR);
  act("$p is surrounded by a protective aura.", ch, obj, NULL, TO_ROOM);
}

MAGIC(spell_flamestrike)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  dam = dice((13 + level) / 4, 15);
  if (saves_spell(level, victim, DAM_FIRE) &&
      saves_spell(level, victim, DAM_HOLY))
    dam /= 1.5;
  xdamage(ch, victim, dam + dice(3, 5), sn, DAM_FIRE, true, VERBOSE_STD,
          &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_faerie_fire)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_FAERIE_FIRE))
    return;
  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.location = APPLY_AC;
  af.modifier = 3 * level;
  af.bitvector = AFF_FAERIE_FIRE;
  affect_to_char(victim, &af);
  send_to_char("You are surrounded by a pink outline.\n\r", victim);
  act("$n is surrounded by a pink outline.", victim, NULL, NULL, TO_ROOM);
  return;
}

MAGIC(spell_faerie_fog)
{
  CHAR_DATA *ich;

  act("$n conjures a cloud of purple smoke.", ch, NULL, NULL, TO_ROOM);
  send_to_char("You conjure a cloud of purple smoke.\n\r", ch);

  for (ich = ch->in_room->people; ich != NULL; ich = ich->next_in_room)
  {
    if (ich->invis_level > 0)
      continue;

    if (ich == ch || saves_spell(level, ich, DAM_OTHER))
      continue;

    affect_strip(ich, gsn_invis);
    affect_strip(ich, gsn_mass_invis);
    affect_strip(ich, gsn_sneak);
    REMOVE_BIT(ich->affected_by, AFF_HIDE);
    REMOVE_BIT(ich->shielded_by, SHD_INVISIBLE);
    REMOVE_BIT(ich->affected_by, AFF_SNEAK);
    act("$n is revealed!", ich, NULL, NULL, TO_ROOM);
    send_to_char("You are revealed!\n\r", ich);
  }

  return;
}

MAGIC(spell_floating_disc)
{
  OBJ_DATA *disc, *floating;

  floating = get_eq_char(ch, WEAR_FLOAT);
  if (floating != NULL && IS_OBJ_STAT(floating, ITEM_NOREMOVE))
  {
    act("You can't remove $p.", ch, floating, NULL, TO_CHAR);
    return;
  }

  disc = create_object(get_obj_index(OBJ_VNUM_DISC));
  disc->value[0] = ch->level * 10;  /* 10 pounds per level capacity */
  disc->value[3] = ch->level * 5; /* 5 pounds per level max per item */
  disc->timer = ch->level * 2 - number_range(0, level / 2);

  act("$n has created a floating black disc.", ch, NULL, NULL, TO_ROOM);
  send_to_char("You create a floating disc.\n\r", ch);
  obj_to_char(disc, ch);
  wear_obj(ch, disc, true, false);
  return;
}

MAGIC(spell_fly)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (IS_AFFECTED(victim, AFF_FLYING))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already airborne.\n\r", ch);
      else
        act("$N doesn't need your help to fly.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level + 50;
  af.location = 0;
  af.modifier = 0;
  af.bitvector = AFF_FLYING;
  affect_to_char(victim, &af);

  send_to_char("Your feet rise off the ground.\n\r", victim);
  if (!silent)
    act("$n's feet rise off the ground.", victim, NULL, NULL, TO_ROOM);
  return;
}

/* RT clerical berserking spell */

MAGIC(spell_frenzy)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn) || IS_AFFECTED(victim, AFF_BERSERK))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already in a frenzy.\n\r", ch);
      else
        act("$N is already in a frenzy.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  if (is_affected(victim, skill_lookup("calm")))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("Why don't you just relax for a while?\n\r", ch);
      else
        act("$N doesn't look like $e wants to fight anymore.", ch, NULL,
            victim, TO_CHAR);
    }
    return;
  }

  if ((IS_GOOD(ch) && !IS_GOOD(victim)) ||
      (IS_NEUTRAL(ch) && !IS_NEUTRAL(victim)) || (IS_EVIL(ch) &&
                                                  !IS_EVIL(victim)))
  {
    if (!silent)
      act("Your god doesn't seem to like $N", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = level / 3;
  af.modifier = level / 6;
  af.bitvector = SHD_FRENZY;

  af.location = APPLY_HITROLL;
  affect_to_char(victim, &af);

  af.location = APPLY_DAMROLL;
  affect_to_char(victim, &af);

  af.modifier = 10 * (level / 12);
  af.location = APPLY_AC;
  affect_to_char(victim, &af);

  send_to_char("You are filled with holy wrath!\n\r", victim);
  if (!silent)
    act("$n gets a wild look in $s eyes!", victim, NULL, NULL, TO_ROOM);
}

/* RT ROM-style gate */

MAGIC(spell_gate)
{
  CHAR_DATA *victim;
  bool gate_pet;

  if ((victim = get_char_world(ch, target_name)) == NULL || victim == ch || victim->in_room == NULL || !can_see_room(ch, victim->in_room) || IS_SET(victim->in_room->room_flags, ROOM_SAFE) || IS_SET(victim->in_room->room_flags, ROOM_PRIVATE) || IS_SET(victim->in_room->room_flags, ROOM_SOLITARY) || IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) || IS_SET(ch->in_room->room_flags, ROOM_NO_RECALL) || victim->level >= level + 5 || (is_clan(ch) && (is_clan(victim) && ((!is_same_clan(ch, victim) && (clan_table[ch->clan].pkill) && (clan_table[victim->clan].pkill)) || clan_table[victim->clan].independent))) || (!IS_NPC(victim) && victim->level >= LEVEL_ANCIENT)  /* NOT 
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             trust 
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           */
      || (IS_NPC(victim) && IS_SET(victim->imm_flags, IMM_SUMMON)) ||
      (IS_NPC(victim) && saves_spell(level, victim, DAM_OTHER)))
  {
    send_to_char("You failed.\n\r", ch);
    return;
  }
  if (strstr(victim->in_room->area->builders, "Unlinked"))
  {
    send_to_char("You can't gate to areas that aren't linked yet!\n\r", ch);
    return;
  }

  if (ch->fighting != NULL)
  {
    send_to_char("You can not gate out of combat.\n\r", ch);
    return;
  }

  if (ch->pet != NULL && ch->in_room == ch->pet->in_room)
    gate_pet = true;
  else
    gate_pet = false;

  act("$n steps through a gate and vanishes.", ch, NULL, NULL, TO_ROOM);
  send_to_char("You step through a gate and vanish.\n\r", ch);
  char_from_room(ch);
  char_to_room(ch, victim->in_room);

  act("$n has arrived through a gate.", ch, NULL, NULL, TO_ROOM);
  do_look(ch, "auto");

  if (gate_pet)
  {
    act("$n steps through a gate and vanishes.", ch->pet, NULL, NULL,
        TO_ROOM);
    send_to_char("You step through a gate and vanish.\n\r", ch->pet);
    char_from_room(ch->pet);
    char_to_room(ch->pet, victim->in_room);
    act("$n has arrived through a gate.", ch->pet, NULL, NULL, TO_ROOM);
    do_look(ch->pet, "auto");
  }
}

MAGIC(spell_giant_strength)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already as strong as you can get!\n\r", ch);
      else
        act("$N can't get any stronger.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.location = APPLY_STR;
  af.modifier = 1 + (level >= 18) + (level >= 25) + (level >= 32);
  af.bitvector = 0;
  affect_to_char(victim, &af);

  send_to_char("Your muscles surge with heightened power!\n\r", victim);
  if (!silent)
    act("$n's muscles surge with heightened power.", victim, NULL, NULL,
        TO_ROOM);
  return;
}

MAGIC(spell_harm)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dice(222, 78) + level, sn, DAM_HARM, true, VERBOSE_STD,
          &mobdeath);
  return;
}

MAGIC(spell_divinewrath)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  int dam;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  dam = (number_range(800, 2500) * 8);
  if (saves_spell(level, victim, DAM_HOLY))
    dam /= 1.5;

  xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
  {
    xdamage(ch, victim, dam, sn, DAM_LIGHT, true, VERBOSE_STD, &mobdeath);

    if (!mobdeath)
    {
      if (number_range(1, 100) >= 98)
      {
        victim->stunned = number_range(2, 4);
        send_to_char("You are {Ystunned{x by the heavenly light!\n\r",
                     victim);
        act("$n is {Ystunned{x by the heavenly light!", victim, NULL, NULL,
            TO_ROOM);
      }
      if (number_percent() >= 98 && !IS_AFFECTED(victim, AFF_BLIND))
      {
        af.where = TO_AFFECTS;
        af.type = sn;
        af.level = level + 5;
        af.location = APPLY_HITROLL;
        af.modifier = -20;
        af.duration = number_range(1, 3);
        af.bitvector = AFF_BLIND;
        affect_to_char(victim, &af);
        send_to_char("You are blinded by the heavenly light!\n\r", victim);
        act("$n is blinded by the heavenly light!", victim, NULL, NULL,
            TO_ROOM);
      }
    }
  }

  return;
}

/* RT haste spell */

MAGIC(spell_haste)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn) || IS_AFFECTED(victim, AFF_HASTE) ||
      IS_SET(victim->off_flags, OFF_FAST))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You can't move any faster!\n\r", ch);
      else
        act("$N is already moving as fast as $E can.", ch, NULL, victim,
            TO_CHAR);
    }
    return;
  }

  if (IS_AFFECTED(victim, AFF_SLOW))
  {
    if (!check_dispel(level, victim, skill_lookup("slow")))
    {
      if ((victim != ch) && !silent)
        send_to_char("Spell failed.\n\r", ch);
      send_to_char("You feel momentarily faster.\n\r", victim);
      return;
    }
    if (!silent)
      act("$n is moving less slowly.", victim, NULL, NULL, TO_ROOM);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  if (victim == ch)
    af.duration = level / 2;
  else
    af.duration = level / 4;

  af.location = APPLY_DEX;
  af.modifier = 1 + (level >= 18) + (level >= 25) + (level >= 32);
  af.bitvector = AFF_HASTE;
  affect_to_char(victim, &af);

  send_to_char("You feel yourself moving more quickly.\n\r", victim);
  if (!silent)
  {
    act("$n is moving more quickly.", victim, NULL, NULL, TO_ROOM);
    if (ch != victim)
      send_to_char("Ok.\n\r", ch);
  }
  return;
}

MAGIC(spell_heal)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  victim->hit = UMIN(victim->hit + level / 10 * 18, victim->max_hit);
  update_pos(victim);
  send_to_char("A warm feeling fills your body.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_fullheal)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  if (ch->fighting != NULL)
  {
    send_to_char("Not while your fighting!\n\r", ch);
    return;
  }
  victim->hit = victim->max_hit;
  update_pos(victim);
  send_to_char("With a flash of light you are fully healed.\n\r", victim);
  if (ch != victim)
    send_to_char("They are now fully healed.\n\r", ch);

  if (IS_SHIELDED(victim, SHD_HEMORRHAGE) &&
      check_dispel(level, victim, skill_lookup("hemorrhage")))
  {
    act("$n stops $N's bleeding.", ch, NULL, victim, TO_ROOM);
    act("You stop $N's bleeding.", ch, NULL, victim, TO_CHAR);
  }
  return;
}

MAGIC(spell_heat_metal)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  OBJ_DATA *obj_lose, *obj_next;
  int dam;
  bool fail = true;
  bool mobdeath = false;

  dam = number_range(ch->level, ch->level * 5);
  if (!saves_spell(level + 2, victim, DAM_FIRE) &&
      !IS_SET(victim->imm_flags, IMM_FIRE))
  {
    for (obj_lose = victim->carrying; obj_lose != NULL; obj_lose = obj_next)
    {
      obj_next = obj_lose->next_content;
      if (number_range(1, 2 * level) > obj_lose->level &&
          !saves_spell(level, victim, DAM_FIRE) &&
          !IS_OBJ_STAT(obj_lose, ITEM_NONMETAL) &&
          !IS_OBJ_STAT(obj_lose, ITEM_BURN_PROOF))
      {
        switch (obj_lose->item_type)
        {
          case ITEM_ARMOR:
            if (obj_lose->wear_loc != -1) /* remove the item */
            {
              if (can_drop_obj(victim, obj_lose) &&
                  (obj_lose->weight / 10) < number_range(1,
                                                         2 *
                                                         get_curr_stat(victim,
                                                                       STAT_DEX))
                  && remove_obj(victim, obj_lose->wear_loc, true, false))
              {
                act("$n yelps and throws $p to the ground!",
                    victim, obj_lose, NULL, TO_ROOM);
                act("You remove and drop $p before it burns you.", victim,
                    obj_lose, NULL, TO_CHAR);
                dam += (number_range(1, obj_lose->level) / 3);
                obj_from_char(obj_lose);
                obj_to_room(obj_lose, victim->in_room);
                fail = false;
              }
              else              /* stuck on the body! ouch! */
              {
                act("Your skin is seared by $p!", victim, obj_lose, NULL,
                    TO_CHAR);
                dam += (number_range(1, obj_lose->level));
                fail = false;
              }

            }
            else                /* drop it if we can */
            {
              if (can_drop_obj(victim, obj_lose))
              {
                act("$n yelps and throws $p to the ground!", victim, obj_lose,
                    NULL, TO_ROOM);
                act("You and drop $p before it burns you.", victim, obj_lose,
                    NULL, TO_CHAR);
                dam += (number_range(1, obj_lose->level) / 6);
                obj_from_char(obj_lose);
                obj_to_room(obj_lose, victim->in_room);
                fail = false;
              }
              else              /* cannot drop */
              {
                act("Your skin is seared by $p!", victim, obj_lose, NULL,
                    TO_CHAR);
                dam += (number_range(1, obj_lose->level) / 2);
                fail = false;
              }
            }
            break;
          case ITEM_WEAPON:
            if (obj_lose->wear_loc != -1) /* try to drop it */
            {
              if (IS_WEAPON_STAT(obj_lose, WEAPON_FLAMING))
                continue;

              if (can_drop_obj(victim, obj_lose) &&
                  remove_obj(victim, obj_lose->wear_loc, true, false))
              {
                act("$n is burned by $p, and throws it to the ground.",
                    victim, obj_lose, NULL, TO_ROOM);
                send_to_char
                  ("You throw your red-hot weapon to the ground!\n\r",
                   victim);
                dam += 1;
                obj_from_char(obj_lose);
                obj_to_room(obj_lose, victim->in_room);
                fail = false;
              }
              else              /* YOWCH! */
              {
                send_to_char("Your weapon sears your flesh!\n\r", victim);
                dam += number_range(1, obj_lose->level);
                fail = false;
              }
            }
            else                /* drop it if we can */
            {
              if (can_drop_obj(victim, obj_lose))
              {
                act("$n throws a burning hot $p to the ground!", victim,
                    obj_lose, NULL, TO_ROOM);
                act("You and drop $p before it burns you.", victim, obj_lose,
                    NULL, TO_CHAR);
                dam += (number_range(1, obj_lose->level) / 6);
                obj_from_char(obj_lose);
                obj_to_room(obj_lose, victim->in_room);
                fail = false;
              }
              else              /* cannot drop */
              {
                act("Your skin is seared by $p!", victim, obj_lose, NULL,
                    TO_CHAR);
                dam += (number_range(1, obj_lose->level) / 2);
                fail = false;
              }
            }
            break;
        }
      }
    }
  }
  if (fail)
  {
    send_to_char("Your spell had no effect.\n\r", ch);
    send_to_char("You feel momentarily warmer.\n\r", victim);
  }
  else                          /* damage! */
  {
    if (saves_spell(level, victim, DAM_FIRE))
      dam /= 1.5;
    xdamage(ch, victim, dam, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
  }
}

MAGIC(spell_prayer)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && !IS_GOOD(ch))
  {
    victim = ch;
    send_to_char("Your prayer goes unheard!\n\r", ch);
  }

  ch->alignment = UMIN(ch->alignment + 50, 1000);

  dam = dice(level, 40);
  if (ch->alignment > victim->alignment)
  {
    if ((ch->alignment > 0) && (victim->alignment <= 0))
      dam += (ch->alignment - victim->alignment) * (level / 25);
    else if ((ch->alignment > 0) && (victim->alignment > 0))
      dam += 0 - (ch->alignment - victim->alignment) * (level / 25);
  }

  if (dam < 0)
    dam /= 5;
  xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_SOUND, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath && !saves_spell(level, victim, DAM_HOLY) &&
      get_curr_stat(ch, STAT_WIS) > get_curr_stat(victim, STAT_WIS))
    spell_blindness(skill_lookup("blindness"), level, ch, (void *) victim,
                    TARGET_CHAR);
  return;
}

MAGIC(spell_blasphemy)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if (!IS_NPC(ch) && !IS_EVIL(ch))
  {
    victim = ch;
    send_to_char("You are punished for your blasphemy!\n\r", ch);
  }

  ch->alignment = UMAX(-1000, ch->alignment - 50);

  dam = dice(level, 40);
  if (ch->alignment < victim->alignment)
  {
    if (ch->alignment < 0 && victim->alignment >= 0)
      dam += (victim->alignment - ch->alignment) * (level / 25);
    else if (ch->alignment < 0 && victim->alignment < 0)
      dam += 0 - (victim->alignment - ch->alignment) * (level / 25);
  }
  if (dam < 0)
    dam /= 5;

  xdamage(ch, victim, dam, sn, DAM_NEGATIVE, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_SOUND, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath && !saves_spell(level, victim, DAM_NEGATIVE) &&
      get_curr_stat(ch, STAT_WIS) > get_curr_stat(victim, STAT_WIS))
    spell_weaken(skill_lookup("weaken"), level, ch, (void *) victim,
                 TARGET_CHAR);
  return;
}

MAGIC(spell_identify)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  char buf[MAX_STRING_LENGTH];
  AFFECT_DATA *paf;

  sprintf(buf,
          "{WObject {w'{g%s{w'{W is type{g %s{W, extra flags {g%s{W.{x\n\r{WWeight is {g%d{x, {Wvalue is {g%d{W, level is {g%d{W.{x\n\r",
          obj->name, item_type_name(obj),
          extra_bit_name(obj->extra_flags), obj->weight / 10, obj->cost,
          obj->level);
  send_to_char(buf, ch);

  switch (obj->item_type)
  {
    case ITEM_SCROLL:
    case ITEM_POTION:
    case ITEM_PILL:
      sprintf(buf, "{WLevel {g%ld{W spells of:", obj->value[0]);
      send_to_char(buf, ch);

      if (obj->value[1] >= 0 && obj->value[1] < MAX_SKILL)
      {
        send_to_char(" {w'{g", ch);
        send_to_char(skill_table[obj->value[1]].name, ch);
        send_to_char("{w'{x", ch);
      }

      if (obj->value[2] >= 0 && obj->value[2] < MAX_SKILL)
      {
        send_to_char(" {w'{g", ch);
        send_to_char(skill_table[obj->value[2]].name, ch);
        send_to_char("{w'{x", ch);
      }

      if (obj->value[3] >= 0 && obj->value[3] < MAX_SKILL)
      {
        send_to_char(" {w'{g", ch);
        send_to_char(skill_table[obj->value[3]].name, ch);
        send_to_char("{w'{x", ch);
      }

      if (obj->value[4] >= 0 && obj->value[4] < MAX_SKILL)
      {
        send_to_char(" {w'{g", ch);
        send_to_char(skill_table[obj->value[4]].name, ch);

        send_to_char("{w'{x", ch);
      }

      send_to_char("{x.\n\r", ch);
      break;

    case ITEM_WAND:
    case ITEM_STAFF:
    case ITEM_INSTRUMENT:
      sprintf(buf, "Has %ld charges of level %ld", obj->value[2],
              obj->value[0]);
      send_to_char(buf, ch);

      if (obj->value[3] >= 0 && obj->value[3] < MAX_SKILL)
      {
        send_to_char(" '", ch);
        send_to_char(skill_table[obj->value[3]].name, ch);
        send_to_char("'", ch);
      }

      send_to_char(".\n\r", ch);
      break;

    case ITEM_DRINK_CON:
      sprintf(buf, "It holds %s-colored %s.\n\r",
              liq_table[obj->value[2]].liq_color,
              liq_table[obj->value[2]].liq_name);
      send_to_char(buf, ch);
      break;

    case ITEM_CONTAINER:
    case ITEM_PIT:
      sprintf(buf,
              "Capacity: %ld#  Maximum weight: %ld#  flags: %s\n\r",
              obj->value[0], obj->value[3], cont_bit_name(obj->value[1]));
      send_to_char(buf, ch);
      if (obj->value[4] != 100)
      {
        sprintf(buf, "Weight multiplier: %ld%%\n\r", obj->value[4]);

        send_to_char(buf, ch);
      }
      break;

    case ITEM_WEAPON:
      send_to_char("Weapon type is ", ch);
      switch (obj->value[0])
      {
        case (WEAPON_EXOTIC):
          send_to_char("exotic.\n\r", ch);
          break;
        case (WEAPON_SWORD):
          send_to_char("sword.\n\r", ch);
          break;
        case (WEAPON_DAGGER):
          send_to_char("dagger.\n\r", ch);
          break;
        case (WEAPON_SPEAR):
          send_to_char("spear/staff.\n\r", ch);
          break;
        case (WEAPON_MACE):
          send_to_char("mace/club.\n\r", ch);
          break;
        case (WEAPON_AXE):
          send_to_char("axe.\n\r", ch);
          break;
        case (WEAPON_FLAIL):
          send_to_char("flail.\n\r", ch);
          break;
        case (WEAPON_WHIP):
          send_to_char("whip.\n\r", ch);
          break;
        case (WEAPON_POLEARM):
          send_to_char("polearm.\n\r", ch);
          break;
        default:
          send_to_char("unknown.\n\r", ch);
          break;
      }
      if (obj->clan)
      {
        sprintf(buf, "Damage is variable.\n\r");
      }
      else
      {
        sprintf(buf, "Damage is %ldd%ld (average %ld).\n\r",
                obj->value[1], obj->value[2],
                (1 + obj->value[2]) * obj->value[1] / 2);
      }
      send_to_char(buf, ch);
      if (obj->value[4])        /* weapon flags */
      {
        sprintf(buf, "Weapons flags: %s\n\r", weapon_bit_name(obj->value[4]));
        send_to_char(buf, ch);
      }
      break;

    case ITEM_ARMOR:
      if (obj->clan)
      {
        sprintf(buf, "Armor class is variable.\n\r");
      }
      else
      {
        sprintf(buf,
                "{WArmor class is {g%ld{W pierce, {g%ld {Wbash,{g %ld{W slash, and {g%ld{W vs. magic.{x\n\r",
                obj->value[0], obj->value[1], obj->value[2], obj->value[3]);
      }
      send_to_char(buf, ch);
      break;
  }
  if (is_clan_obj(obj))
  {
    sprintf(buf, "This object is owned by the [{%s%s{x] clan.\n\r",
            clan_table[obj->clan].pkill ? "B" : "M",
            clan_table[obj->clan].who_name);
    send_to_char(buf, ch);
  }
  if (is_class_obj(obj))
  {
    sprintf(buf, "This object may only be used by a %s.\n\r",
            class_table[obj->class].name);
    send_to_char(buf, ch);
  }
  if (!obj->enchanted)
    for (paf = obj->pIndexData->affected; paf != NULL; paf = paf->next)
    {
      if (paf->location != APPLY_NONE && paf->modifier != 0)
      {
        sprintf(buf, "Affects %s by %d.\n\r",
                affect_loc_name(paf->location), paf->modifier);
        send_to_char(buf, ch);
        if (paf->bitvector)
        {
          switch (paf->where)
          {
            case TO_AFFECTS:
              sprintf(buf, "Adds %s affect.\n",
                      affect_bit_name(paf->bitvector));
              break;
            case TO_OBJECT:
              sprintf(buf, "Adds %s object flag.\n",
                      extra_bit_name(paf->bitvector));
              break;
            case TO_IMMUNE:
              sprintf(buf, "Adds immunity to %s.\n",
                      imm_bit_name(paf->bitvector));
              break;
            case TO_RESIST:
              sprintf(buf, "Adds resistance to %s.\n\r",
                      imm_bit_name(paf->bitvector));
              break;
            case TO_VULN:
              sprintf(buf, "Adds vulnerability to %s.\n\r",
                      imm_bit_name(paf->bitvector));
              break;
            case TO_SHIELDS:
              sprintf(buf, "Adds %s shield.\n",
                      shield_bit_name(paf->bitvector));
              break;
            default:
              sprintf(buf, "Unknown bit %d: %ld\n\r",
                      paf->where, paf->bitvector);
              break;
          }
          send_to_char(buf, ch);
        }
      }
    }

  for (paf = obj->affected; paf != NULL; paf = paf->next)
  {
    if (paf->location != APPLY_NONE && paf->modifier != 0)
    {
      sprintf(buf, "Affects %s by %d",
              affect_loc_name(paf->location), paf->modifier);
      send_to_char(buf, ch);
      if (paf->duration > -1)
        sprintf(buf, ", %d hours.\n\r", paf->duration);
      else
        sprintf(buf, ".\n\r");
      send_to_char(buf, ch);
      if (paf->bitvector)
      {
        switch (paf->where)
        {
          case TO_AFFECTS:
            sprintf(buf, "Adds %s affect.\n",
                    affect_bit_name(paf->bitvector));
            break;
          case TO_OBJECT:
            sprintf(buf, "Adds %s object flag.\n",
                    extra_bit_name(paf->bitvector));
            break;
          case TO_WEAPON:
            sprintf(buf, "Adds %s weapon flags.\n",
                    weapon_bit_name(paf->bitvector));
            break;
          case TO_IMMUNE:
            sprintf(buf, "Adds immunity to %s.\n",
                    imm_bit_name(paf->bitvector));
            break;
          case TO_RESIST:
            sprintf(buf, "Adds resistance to %s.\n\r",
                    imm_bit_name(paf->bitvector));
            break;
          case TO_VULN:
            sprintf(buf, "Adds vulnerability to %s.\n\r",
                    imm_bit_name(paf->bitvector));
            break;
          case TO_SHIELDS:
            sprintf(buf, "Adds %s shield.\n",
                    shield_bit_name(paf->bitvector));
            break;
          default:
            sprintf(buf, "Unknown bit %d: %ld\n\r", paf->where,
                    paf->bitvector);
            break;
        }
        send_to_char(buf, ch);
      }

    }
  }
  return;
}

MAGIC(spell_infravision)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_INFRARED))
  {
    if (victim == ch)
      send_to_char("You can already see in the dark.\n\r", ch);
    else
      act("$N already has infravision.\n\r", ch, NULL, victim, TO_CHAR);
    return;
  }
  act("$n's eyes glow red.\n\r", ch, NULL, NULL, TO_ROOM);

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = 2 * level;
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = AFF_INFRARED;
  affect_to_char(victim, &af);
  send_to_char("Your eyes glow red.\n\r", victim);
  return;
}

MAGIC(spell_invis)
{
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  AFFECT_DATA af;

  if (IS_SET(ch->in_room->room_flags, ROOM_ARENA))
  {
    send_to_char("Not while in the arena you dont.\n\r", ch);
    return;
  }

  /* object invisibility */
  if (target == TARGET_OBJ)
  {
    obj = (OBJ_DATA *) vo;

    if (IS_OBJ_STAT(obj, ITEM_INVIS))
    {
      act("$p is already invisible.", ch, obj, NULL, TO_CHAR);
      return;
    }

    af.where = TO_OBJECT;
    af.type = sn;
    af.level = level;
    af.duration = level + 12;
    af.location = APPLY_NONE;
    af.modifier = 0;
    af.bitvector = ITEM_INVIS;
    affect_to_obj(obj, &af);

    act("$p fades out of sight.", ch, obj, NULL, TO_ALL);
    return;
  }

  /* character invisibility */
  victim = (CHAR_DATA *) vo;

  if (IS_SHIELDED(victim, SHD_INVISIBLE))
    return;

  act("$n fades out of existence.", victim, NULL, NULL, TO_ROOM);

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = level + 12;
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = SHD_INVISIBLE;
  affect_to_char(victim, &af);
  send_to_char("You fade out of existence.\n\r", victim);
  return;
}

MAGIC(spell_vanish)
{
  CHAR_DATA *victim;
  AFFECT_DATA af;

  victim = (CHAR_DATA *) vo;

  if (IS_SET(ch->in_room->room_flags, ROOM_ARENA))
  {
    send_to_char("Not while in the arena you dont.\n\r", ch);
    return;
  }
  if (victim != ch)
  {
    send_to_char("You can not make others vanish!\n\r", ch);
    return;
  }
  if (IS_SHIELDED(victim, SHD_VANISH))
  {
    send_to_char("You have already vanished.\n\r", ch);
    return;
  }
  else
  {
    act("$n vanishes suddenly..", victim, NULL, NULL, TO_ROOM);

    af.where = TO_SHIELDS;
    af.type = sn;
    af.level = level;
    af.duration = level + 698;
    af.location = APPLY_NONE;
    af.modifier = 0;
    af.bitvector = SHD_VANISH;
    affect_to_char(victim, &af);
    send_to_char("You vanish suddenly..\n\r", victim);

    return;
  }
}

MAGIC(spell_know_alignment)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  char *msg;
  int ap;

  ap = victim->alignment;

  if (ap > 700)
    msg = "$N has a pure and good aura.";
  else if (ap > 350)
    msg = "$N is of excellent moral character.";
  else if (ap > 100)
    msg = "$N is often kind and thoughtful.";
  else if (ap > -100)
    msg = "$N doesn't have a firm moral commitment.";
  else if (ap > -350)
    msg = "$N lies to $S friends.";
  else if (ap > -700)
    msg = "$N is a black-hearted murderer.";
  else
    msg = "$N is the embodiment of pure evil!.";

  act(msg, ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_lightning_bolt)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  dam = number_range((11 + level) / 2, 10);
  if (saves_spell(level, victim, DAM_LIGHTNING))
    dam /= 1.5;
  xdamage(ch, victim, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_locate_object)
{
  char buf[MAX_INPUT_LENGTH];
  BUFFER *buffer;
  OBJ_DATA *obj;
  OBJ_DATA *in_obj;
  bool found;
  int number = 0, max_found;

  found = false;
  number = 0;
  max_found = IS_IMMORTAL(ch) ? 200 : 2 * level;

  buffer = new_buf();

  for (obj = object_list; obj != NULL; obj = obj->next)
  {
    if (!can_see_obj(ch, obj) || !is_name(target_name, obj->name) ||
        IS_OBJ_STAT(obj, ITEM_NOLOCATE) ||
        number_percent() > 2 * level || ch->level < obj->level)
      continue;

    found = true;
    number++;

    for (in_obj = obj; in_obj->in_obj != NULL; in_obj = in_obj->in_obj)
      ;

    if (in_obj->carried_by != NULL && can_see(ch, in_obj->carried_by))
    {
      sprintf(buf, "one is carried by %s\n\r", PERS(in_obj->carried_by, ch));
    }
    else
    {
      if (IS_IMMORTAL(ch) && in_obj->in_room != NULL)
        sprintf(buf, "one is in %s [Room %ld]\n\r",
                in_obj->in_room->name, in_obj->in_room->vnum);
      else
        sprintf(buf, "one is in %s\n\r",
                in_obj->in_room ==
                NULL ? "somewhere" : in_obj->in_room->name);
    }

    buf[0] = UPPER(buf[0]);
    add_buf(buffer, buf);

    if (number >= max_found)
      break;
  }

  if (!found)
    send_to_char("Nothing like that in heaven or earth.\n\r", ch);
  else
    send_to_char(buf_string(buffer), ch);

  free_buf(buffer);

  return;
}

MAGIC(spell_magic_missile)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, hits, i;
  bool mobdeath = false;

  hits = 1 + (ch->level / 10);

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  for (i = 1; i <= hits && !mobdeath; i++)
  {
    dam = number_range(20, 50);
    if (ch->level > 19)
      dam *= (ch->level / 20);
    if (saves_spell(level, victim, DAM_ENERGY))
      dam /= 1.5;
    xdamage(ch, victim, dam, sn, DAM_ENERGY, true, VERBOSE_STD, &mobdeath);
  }
  return;
}

MAGIC(spell_mass_healing)
{
  CHAR_DATA *gch;
  int heal_num, refresh_num;

  heal_num = skill_lookup("heal");
  refresh_num = skill_lookup("refresh");

  for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
  {
    if ((IS_NPC(ch) && IS_NPC(gch)) || (!IS_NPC(ch) && !IS_NPC(gch)))
    {
      spell_heal(heal_num, level, ch, (void *) gch, TARGET_CHAR);
      spell_refresh(refresh_num, level, ch, (void *) gch, TARGET_CHAR);
    }
  }
}

MAGIC(spell_mass_invis)
{
  AFFECT_DATA af;
  CHAR_DATA *gch;

  for (gch = ch->in_room->people; gch != NULL; gch = gch->next_in_room)
  {
    if (!is_same_group(gch, ch) || IS_SHIELDED(gch, SHD_INVISIBLE))
      continue;
    act("$n slowly fades out of existence.", gch, NULL, NULL, TO_ROOM);
    send_to_char("You slowly fade out of existence.\n\r", gch);

    af.where = TO_SHIELDS;
    af.type = sn;
    af.level = level / 2;
    af.duration = 24;
    af.location = APPLY_NONE;
    af.modifier = 0;
    af.bitvector = SHD_INVISIBLE;
    affect_to_char(gch, &af);
  }
  send_to_char("Ok.\n\r", ch);

  return;
}

MAGIC(spell_null)
{
  send_to_char("That's not a spell!\n\r", ch);
  return;
}

MAGIC(spell_pass_door)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_PASS_DOOR))
  {
    if (victim == ch)
      send_to_char("You are already out of phase.\n\r", ch);
    else
      act("$N is already shifted out of phase.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = number_fuzzy(level / 4);
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = AFF_PASS_DOOR;
  affect_to_char(victim, &af);
  act("$n turns translucent.", victim, NULL, NULL, TO_ROOM);
  send_to_char("You turn translucent.\n\r", victim);
  return;
}

/* RT plague spell, very nasty */

MAGIC(spell_plague)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (saves_spell(level, victim, DAM_DISEASE) ||
      (IS_NPC(victim) && IS_SET(victim->act, ACT_UNDEAD)))
  {
    if (ch == victim)
      send_to_char("You feel momentarily ill, but it passes.\n\r", ch);
    else
      act("$N seems to be unaffected.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level * 3 / 4;

  af.duration = level;
  af.location = APPLY_STR;
  af.modifier = -5;
  af.bitvector = AFF_PLAGUE;
  affect_join(victim, &af);

  send_to_char
    ("You scream in agony as plague sores erupt from your skin.\n\r", victim);
  act("$n screams in agony as plague sores erupt from $s skin.", victim,
      NULL, NULL, TO_ROOM);
}

MAGIC(spell_poison)
{
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  AFFECT_DATA af;

  if (target == TARGET_OBJ)
  {
    obj = (OBJ_DATA *) vo;

    if (obj->item_type == ITEM_FOOD || obj->item_type == ITEM_DRINK_CON)
    {
      if (IS_OBJ_STAT(obj, ITEM_BLESS) || IS_OBJ_STAT(obj, ITEM_BURN_PROOF))
      {
        act("Your spell fails to corrupt $p.", ch, obj, NULL, TO_CHAR);
        return;
      }
      obj->value[3] = 1;
      act("$p is infused with poisonous vapors.", ch, obj, NULL, TO_ALL);
      return;
    }

    if (obj->item_type == ITEM_WEAPON)
    {
      if (IS_WEAPON_STAT(obj, WEAPON_FLAMING) ||
          IS_WEAPON_STAT(obj, WEAPON_FROST) ||
          IS_WEAPON_STAT(obj, WEAPON_VAMPIRIC) ||
          IS_WEAPON_STAT(obj, WEAPON_SHARP) ||
          IS_WEAPON_STAT(obj, WEAPON_VORPAL) ||
          IS_WEAPON_STAT(obj, WEAPON_SHOCKING) ||
          IS_WEAPON_STAT(obj, WEAPON_MANADRAIN) ||
          IS_OBJ_STAT(obj, ITEM_BLESS) || IS_OBJ_STAT(obj, ITEM_BURN_PROOF))
      {
        act("You can't seem to envenom $p.", ch, obj, NULL, TO_CHAR);
        return;
      }

      if (IS_WEAPON_STAT(obj, WEAPON_POISON))
      {
        act("$p is already envenomed.", ch, obj, NULL, TO_CHAR);
        return;
      }

      af.where = TO_WEAPON;
      af.type = sn;
      af.level = level / 2;
      af.duration = level / 8;
      af.location = 0;
      af.modifier = 0;
      af.bitvector = WEAPON_POISON;
      affect_to_obj(obj, &af);

      act("$p is coated with deadly venom.", ch, obj, NULL, TO_ALL);
      return;
    }

    act("You can't poison $p.", ch, obj, NULL, TO_CHAR);
    return;
  }

  victim = (CHAR_DATA *) vo;

  if (saves_spell(level, victim, DAM_POISON))
  {
    act("$n turns slightly green, but it passes.", victim, NULL, NULL,
        TO_ROOM);
    send_to_char("You feel momentarily ill, but it passes.\n\r", victim);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level;
  af.location = APPLY_STR;
  af.modifier = -2;
  af.bitvector = AFF_POISON;
  affect_join(victim, &af);
  send_to_char("You feel very sick.\n\r", victim);
  act("$n looks very ill.", victim, NULL, NULL, TO_ROOM);
  return;
}

MAGIC(spell_protection_evil)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_SHIELDED(victim, SHD_PROTECT_EVIL) ||
      IS_SHIELDED(victim, SHD_PROTECT_GOOD))
  {
    if (victim == ch)
      send_to_char("You are already protected.\n\r", ch);
    else
      act("$N is already protected.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = 24;
  af.location = APPLY_SAVING_SPELL;
  af.modifier = -1;
  af.bitvector = SHD_PROTECT_EVIL;
  affect_to_char(victim, &af);
  send_to_char("You feel holy and pure.\n\r", victim);
  if (ch != victim)
    act("$N is protected from evil.", ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_protection_good)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_SHIELDED(victim, SHD_PROTECT_GOOD) ||
      IS_SHIELDED(victim, SHD_PROTECT_EVIL))
  {
    if (victim == ch)
      send_to_char("You are already protected.\n\r", ch);
    else
      act("$N is already protected.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = 24;
  af.location = APPLY_SAVING_SPELL;
  af.modifier = -1;
  af.bitvector = SHD_PROTECT_GOOD;
  affect_to_char(victim, &af);
  send_to_char("You feel aligned with darkness.\n\r", victim);
  if (ch != victim)
    act("$N is protected from good.", ch, NULL, victim, TO_CHAR);
  return;
}

MAGIC(spell_ray_of_truth)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, align;
  bool mobdeath = false;

  act("$n raises $s hand, and a blinding ray of light shoots forth!", ch,
      NULL, ch, TO_ROOM);
  send_to_char
    ("You raise your hand and a blinding ray of light shoots forth!\n\r", ch);

  if ((IS_EVIL(ch) && IS_EVIL(victim)) || (IS_GOOD(ch) && IS_GOOD(victim))
      || (IS_NEUTRAL(ch) && IS_NEUTRAL(victim)))
  {
    act("$n seems unharmed by the light.", victim, NULL, victim, TO_ROOM);
    send_to_char("The light seems powerless to affect you.\n\r", victim);
    return;
  }

  dam = dice(level, 15);
  if (saves_spell(level, victim, DAM_LIGHT))
    dam /= 1.5;

  align = (ch->alignment - victim->alignment) * (level / 125);
  if (align < 0)
    align = 0 - align;

  dam += align;
  dam /= 2;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dam, sn, DAM_LIGHT, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    xdamage(ch, victim, dam, sn, DAM_HOLY, true, VERBOSE_STD, &mobdeath);
  if (!mobdeath)
    spell_blindness(gsn_blindness, 3 * level / 4, ch, (void *) victim,
                    TARGET_CHAR);
}

MAGIC(spell_recharge)
{
  OBJ_DATA *obj = (OBJ_DATA *) vo;
  int chance, percent;

  if ((obj->item_type != ITEM_WAND) & (obj->item_type != ITEM_STAFF) &
      (obj->item_type != ITEM_INSTRUMENT))
  {
    send_to_char("That item does not carry charges.\n\r", ch);
    return;
  }

  if (obj->value[3] >= 3 * level / 2)
  {
    send_to_char("Your skills are not great enough for that.\n\r", ch);
    return;
  }

  if (obj->value[1] == 0)
  {
    send_to_char("That item has already been recharged once.\n\r", ch);
    return;
  }

  chance = 40 + 2 * level;

  chance -= obj->value[3];      /* harder to do high-level spells */
  chance -= (obj->value[1] - obj->value[2]) * (obj->value[1] - obj->value[2]);

  chance = UMAX(level / 2, chance);

  percent = number_percent();

  if (percent < chance / 2)
  {
    act("$p glows softly.", ch, obj, NULL, TO_CHAR);
    act("$p glows softly.", ch, obj, NULL, TO_ROOM);
    obj->value[2] = UMAX(obj->value[1], obj->value[2]);
    obj->value[1] = 0;
    return;
  }

  else if (percent <= chance)
  {
    int chargeback, chargemax;

    act("$p glows softly.", ch, obj, NULL, TO_CHAR);
    act("$p glows softly.", ch, obj, NULL, TO_CHAR);

    chargemax = obj->value[1] - obj->value[2];

    if (chargemax > 0)
      chargeback = UMAX(1, chargemax * percent / 100);
    else
      chargeback = 0;

    obj->value[2] += chargeback;
    obj->value[1] = 0;
    return;
  }

  else if (percent <= UMIN(95, 3 * chance / 2))
  {
    send_to_char("Nothing seems to happen.\n\r", ch);
    if (obj->value[1] > 1)
      obj->value[1]--;
    return;
  }

  else                          /* whoops! */
  {
    act("$p glows brightly and explodes!", ch, obj, NULL, TO_CHAR);
    act("$p glows brightly and explodes!", ch, obj, NULL, TO_ROOM);
    extract_obj(obj);
    obj = NULL;
  }
}

MAGIC(spell_refresh)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  victim->move = UMIN(victim->move + level, victim->max_move);
  if (victim->max_move == victim->move)
    send_to_char("You feel fully refreshed!\n\r", victim);
  else
    send_to_char("You feel less tired.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_remove_curse)
{
  CHAR_DATA *victim;
  OBJ_DATA *obj;
  bool found = false;

  /* do object cases first */
  if (target == TARGET_OBJ)
  {
    obj = (OBJ_DATA *) vo;

    if (IS_OBJ_STAT(obj, ITEM_NODROP) || IS_OBJ_STAT(obj, ITEM_NOREMOVE))
    {
      if (!IS_OBJ_STAT(obj, ITEM_NOUNCURSE) &&
          !saves_dispel(level + 2, obj->level, 0))
      {
        REMOVE_BIT(obj->extra_flags, ITEM_NODROP);
        REMOVE_BIT(obj->extra_flags, ITEM_NOREMOVE);
        act("$p glows blue.", ch, obj, NULL, TO_ALL);
        return;
      }

      act("The curse on $p is beyond your power.", ch, obj, NULL, TO_CHAR);
      return;
    }
    else
    {
      act("There is no curse on $p.", ch, obj, NULL, TO_CHAR);
      return;
    }
  }

  /* characters */
  victim = (CHAR_DATA *) vo;

  if (check_dispel(level, victim, gsn_curse))
  {
    send_to_char("You feel better.\n\r", victim);
    act("$n looks more relaxed.", victim, NULL, NULL, TO_ROOM);
  }

  for (obj = victim->carrying; (obj != NULL && !found);
       obj = obj->next_content)
  {
    if ((IS_OBJ_STAT(obj, ITEM_NODROP) ||
         IS_OBJ_STAT(obj, ITEM_NOREMOVE)) &&
        !IS_OBJ_STAT(obj, ITEM_NOUNCURSE))
    {                           /* attempt to remove curse */
      if (!saves_dispel(level, obj->level, 0))
      {
        found = true;
        REMOVE_BIT(obj->extra_flags, ITEM_NODROP);
        REMOVE_BIT(obj->extra_flags, ITEM_NOREMOVE);
        act("Your $p glows blue.", victim, obj, NULL, TO_CHAR);
        act("$n's $p glows blue.", victim, obj, NULL, TO_ROOM);
      }
    }
  }
}

MAGIC(spell_restore_mana)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;

  victim->mana = UMIN(victim->mana + level * 2, victim->max_mana);
  if (victim->max_mana == victim->mana)
    send_to_char("You feel fully focused!\n\r", victim);
  else
    send_to_char("You feel more focused.\n\r", victim);
  if (ch != victim)
    send_to_char("Ok.\n\r", ch);
  return;
}

MAGIC(spell_sanctuary)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already in sanctuary.\n\r", ch);
      else
        act("$N is already in sanctuary.", ch, NULL, victim, TO_CHAR);
    }
    return;
  }

  af.where = TO_SHIELDS;
  af.type = sn;
  af.level = level;
  af.duration = level / 2;
  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = SHD_SANCTUARY;
  affect_to_char(victim, &af);

  if (!silent)
    act("$n is surrounded by a white aura.", victim, NULL, NULL, TO_ROOM);
  send_to_char("You are surrounded by a white aura.\n\r", victim);
  return;
}

MAGIC(spell_shield)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;
  bool silent = false;

  GetCharProperty(ch, PROPERTY_BOOL, "silent_spellup", &silent);

  if (is_affected(victim, sn))
  {
    if (!silent)
    {
      if (victim == ch)
        send_to_char("You are already shielded from harm.\n\r", ch);
      else
        act("$N is already protected by a shield.", ch, NULL, victim,
            TO_CHAR);
    }
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = 20 + level;
  af.location = APPLY_AC;
  af.modifier = -40;
  af.bitvector = 0;
  affect_to_char(victim, &af);

  if (!silent)
    act("$n is surrounded by a force shield.", victim, NULL, NULL, TO_ROOM);
  send_to_char("You are surrounded by a force shield.\n\r", victim);
  return;
}

MAGIC(spell_shocking_grasp)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  dam = number_range((9 + level) / 2, 12);
  if (saves_spell(level, victim, DAM_LIGHTNING))
    dam /= 1.5;
  xdamage(ch, victim, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_sleep)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (IS_AFFECTED(victim, AFF_SLEEP) ||
      (IS_NPC(victim) && IS_SET(victim->act, ACT_UNDEAD)) ||
      (level + 5) < victim->level ||
      saves_spell(level - 4, victim, DAM_CHARM))
    return;

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = 4 + level;

  af.location = APPLY_NONE;
  af.modifier = 0;
  af.bitvector = AFF_SLEEP;
  affect_join(victim, &af);

  if (IS_AWAKE(victim))
  {
    send_to_char("You feel very sleepy ..... zzzzzz.\n\r", victim);
    act("$n goes to sleep.", victim, NULL, NULL, TO_ROOM);
    victim->position = POS_SLEEPING;
  }
  return;
}

MAGIC(spell_slow)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (is_affected(victim, sn) || IS_AFFECTED(victim, AFF_SLOW))
  {
    if (victim == ch)
      send_to_char("You can't move any slower!\n\r", ch);
    else
      act("$N can't get any slower than that.", ch, NULL, victim, TO_CHAR);
    return;
  }

  if (saves_spell(level, victim, DAM_OTHER) ||
      IS_SET(victim->imm_flags, IMM_MAGIC))
  {
    if (victim != ch)
      send_to_char("Nothing seemed to happen.\n\r", ch);
    send_to_char("You feel momentarily lethargic.\n\r", victim);
    return;
  }

  if (IS_AFFECTED(victim, AFF_HASTE))
  {
    if (!check_dispel(level, victim, skill_lookup("haste")))
    {
      if (victim != ch)
        send_to_char("Spell failed.\n\r", ch);
      send_to_char("You feel momentarily slower.\n\r", victim);
      return;
    }

    act("$n is moving less quickly.", victim, NULL, NULL, TO_ROOM);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level / 2;
  af.location = APPLY_DEX;
  af.modifier = -1 - (level >= 18) - (level >= 25) - (level >= 32);
  af.bitvector = AFF_SLOW;
  affect_to_char(victim, &af);
  send_to_char("You feel yourself slowing d o w n...\n\r", victim);
  act("$n starts to move in slow motion.", victim, NULL, NULL, TO_ROOM);
  return;
}

MAGIC(spell_stone_skin)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (is_affected(ch, sn))
  {
    if (victim == ch)
      send_to_char("Your skin is already as hard as a rock.\n\r", ch);
    else
      act("$N is already as hard as can be.", ch, NULL, victim, TO_CHAR);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = 40 + level;
  af.location = APPLY_AC;
  af.modifier = -45;
  af.bitvector = SHD_STONE_SKIN;
  affect_to_char(victim, &af);
  act("$n's skin turns to stone.", victim, NULL, NULL, TO_ROOM);
  send_to_char("Your skin turns to stone.\n\r", victim);
  return;
}

MAGIC(spell_summon)
{
  CHAR_DATA *victim;

  if ((victim = get_char_world(ch, target_name)) == NULL || victim == ch
      || victim->in_room == NULL ||
      IS_SET(ch->in_room->room_flags, ROOM_SAFE) ||
      IS_SET(victim->in_room->room_flags, ROOM_SAFE) ||
      IS_SET(victim->in_room->room_flags, ROOM_PRIVATE) ||
      IS_SET(victim->in_room->room_flags, ROOM_SOLITARY) ||
      IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) ||
      (IS_NPC(victim) && IS_SET(victim->act, ACT_AGGRESSIVE))
      // || (IS_NPC(victim) && is_gqmob(ch, victim->pIndexData->vnum) !=
      // -1) 
      || victim->level >= level + 3 || (is_clan(ch) &&
                                        (is_clan(victim) &&
                                         ((!is_same_clan(ch, victim)
                                           && (clan_table[ch->clan].
                                               pkill) &&
                                           (clan_table[victim->clan].
                                            pkill)) ||
                                          clan_table[victim->clan].
                                          independent))) ||
      (!IS_NPC(victim) && victim->level >= LEVEL_IMMORTAL) ||
      victim->fighting != NULL || (IS_NPC(victim) &&
                                   IS_SET(victim->imm_flags,
                                          IMM_SUMMON)) ||
      (IS_NPC(victim) && victim->pIndexData->pShop != NULL) ||
      (!IS_NPC(victim) && IS_SET(victim->act, PLR_NOSUMMON)) ||
      (IS_NPC(victim) && saves_spell(level, victim, DAM_OTHER)))

  {
    send_to_char("You failed.\n\r", ch);
    return;
  }

  if (strstr(victim->in_room->area->builders, "Unlinked"))
  {
    send_to_char
      ("You can not summon from the area where that is located.\n\r", ch);
    return;
  }

  act("$n disappears suddenly.", victim, NULL, NULL, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, ch->in_room);
  act("$n arrives suddenly.", victim, NULL, NULL, TO_ROOM);
  act("$n has summoned you!", ch, NULL, victim, TO_VICT);
  do_look(victim, "auto");
  return;
}

MAGIC(spell_transport)
{
  return;
}

MAGIC(spell_teleport)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  ROOM_INDEX_DATA *pRoomIndex;

  if (victim->in_room == NULL ||
      IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) ||
      (victim != ch && IS_SET(victim->imm_flags, IMM_SUMMON)) ||
      (!IS_NPC(ch) && victim->fighting != NULL) || (victim != ch &&
                                                    (saves_spell
                                                     (level - 5,
                                                      victim, DAM_OTHER))))
  {
    send_to_char("You failed.\n\r", ch);
    return;
  }
  if (strstr(victim->in_room->area->builders, "Unlinked"))
  {
    send_to_char("You can't gate to areas that aren't linked yet!\n\r", ch);
    return;
  }

  pRoomIndex = get_random_room(victim);

  if (victim != ch)
    send_to_char("You have been teleported!\n\r", victim);

  act("$n vanishes!", victim, NULL, NULL, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, pRoomIndex);
  act("$n slowly fades into existence.", victim, NULL, NULL, TO_ROOM);
  do_look(victim, "auto");
  return;
}

MAGIC(spell_ventriloquate)
{
  char buf1[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  char speaker[MAX_INPUT_LENGTH];
  CHAR_DATA *vch;

  target_name = one_argument(target_name, speaker);

  sprintf(buf1, "%s says '{S%s{x'.\n\r", speaker, target_name);
  sprintf(buf2, "Someone makes %s say '{S%s{x'.\n\r", speaker, target_name);
  buf1[0] = UPPER(buf1[0]);

  for (vch = ch->in_room->people; vch != NULL; vch = vch->next_in_room)
  {
    if (!is_name(speaker, vch->name))
      send_to_char(saves_spell(level, vch, DAM_CHARM) ? buf2 : buf1, vch);
  }

  return;
}

MAGIC(spell_weaken)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (is_affected(victim, sn) || saves_spell(level, victim, DAM_OTHER))
  {
    send_to_char("You fail to weaken your victim.\n\r", ch);
    return;
  }

  af.where = TO_AFFECTS;
  af.type = sn;
  af.level = level;
  af.duration = level / 2;
  af.location = APPLY_STR;
  af.modifier = -2 * (level / 5);
  af.bitvector = AFF_WEAKEN;
  affect_to_char(victim, &af);
  send_to_char("You feel your strength slip away.\n\r", victim);
  act("$n looks tired and weak.", victim, NULL, NULL, TO_ROOM);
  return;
}

/* RT recall spell is back */

MAGIC(spell_word_of_recall)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  ROOM_INDEX_DATA *location;

  if (IS_NPC(victim))
    return;

  if ((location = get_room_index(ROOM_VNUM_TEMPLE)) == NULL)
  {
    send_to_char("You are completely lost.\n\r", victim);
    return;
  }

  if (IS_SET(victim->in_room->room_flags, ROOM_NO_RECALL) ||
      IS_AFFECTED(victim, AFF_CURSE))
  {
    send_to_char("Spell failed.\n\r", victim);
    return;
  }

  if (victim->fighting != NULL)
    stop_fighting(victim, true);

  ch->move /= 2;
  act("$n disappears.", victim, NULL, NULL, TO_ROOM);
  char_from_room(victim);
  char_to_room(victim, location);
  act("$n appears in the room.", victim, NULL, NULL, TO_ROOM);
  do_look(victim, "auto");
}

/*
 * NPC spells.
 */
MAGIC(spell_acid_breath)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, hp_dam, dice_dam, hpch;
  bool mobdeath = false;

  act("$n spits acid at $N.", ch, NULL, victim, TO_NOTVICT);
  act("$n spits a stream of corrosive acid at you.", ch, NULL, victim,
      TO_VICT);
  act("You spit acid at $N.", ch, NULL, victim, TO_CHAR);

  hpch = UMAX(12, ch->hit);
  hp_dam = number_range(hpch / 11 + 1, hpch / 6);
  dice_dam = dice(level, 26);

  dam = UMAX(hp_dam + dice_dam / 10, dice_dam + hp_dam / 10);

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  if (saves_spell(level, victim, DAM_ACID))
  {
    acid_effect(victim, level / 2, dam / 4, TARGET_CHAR);

    xdamage(ch, victim, dam / 2, sn, DAM_ACID, true, VERBOSE_STD, &mobdeath);
  }
  else
  {
    acid_effect(victim, level, dam, TARGET_CHAR);
    xdamage(ch, victim, dam, sn, DAM_ACID, true, VERBOSE_STD, &mobdeath);
  }
}

MAGIC(spell_fire_breath)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  CHAR_DATA *vch, *vch_next;
  int dam, hp_dam, dice_dam;
  int hpch;
  bool mobdeath = false;

  act("$n breathes forth a cone of fire.", ch, NULL, victim, TO_NOTVICT);
  act("$n breathes a cone of hot fire over you!", ch, NULL, victim, TO_VICT);
  act("You breath forth a cone of fire.", ch, NULL, NULL, TO_CHAR);

  hpch = UMAX(10, ch->hit);
  hp_dam = number_range(hpch / 9 + 1, hpch / 5);
  dice_dam = dice(level, 30);

  dam = UMAX(hp_dam + dice_dam / 10, dice_dam + hp_dam / 10);
  fire_effect(victim->in_room, level, dam / 2, TARGET_ROOM);

  for (vch = victim->in_room->people; vch != NULL; vch = vch_next)
  {
    vch_next = vch->next_in_room;

    if (is_safe_spell(ch, vch, true) ||
        (IS_NPC(vch) && IS_NPC(ch) &&
         (ch->fighting != vch || vch->fighting != ch)))
      continue;

    if (vch == victim)          /* full damage */
    {
      if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
      {
        ch->attacker = true;
        victim->attacker = false;
      }
      if (saves_spell(level, vch, DAM_FIRE))
      {
        fire_effect(vch, level / 2, dam / 4, TARGET_CHAR);

        xdamage(ch, vch, dam / 2, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
      }
      else
      {
        fire_effect(vch, level, dam, TARGET_CHAR);
        xdamage(ch, vch, dam, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
      }
    }
    else                        /* partial damage */
    {
      if (saves_spell(level - 2, vch, DAM_FIRE))
      {
        fire_effect(vch, level / 4, dam / 8, TARGET_CHAR);
        xdamage(ch, vch, dam / 4, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
      }
      else
      {
        fire_effect(vch, level / 2, dam / 4, TARGET_CHAR);
        xdamage(ch, vch, dam / 2, sn, DAM_FIRE, true, VERBOSE_STD, &mobdeath);
      }
    }
  }
}

MAGIC(spell_frost_breath)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  CHAR_DATA *vch, *vch_next;
  int dam, hp_dam, dice_dam, hpch;
  bool mobdeath = false;

  act("$n breathes out a freezing cone of frost!", ch, NULL, victim,
      TO_NOTVICT);
  act("$n breathes a freezing cone of frost over you!", ch, NULL, victim,
      TO_VICT);
  act("You breath out a cone of frost.", ch, NULL, NULL, TO_CHAR);

  hpch = UMAX(12, ch->hit);
  hp_dam = number_range(hpch / 11 + 1, hpch / 6);
  dice_dam = dice(level, 20);

  dam = UMAX(hp_dam + dice_dam / 10, dice_dam + hp_dam / 10);
  cold_effect(victim->in_room, level, dam / 2, TARGET_ROOM);

  for (vch = victim->in_room->people; vch != NULL; vch = vch_next)
  {
    vch_next = vch->next_in_room;

    if (is_safe_spell(ch, vch, true) ||
        (IS_NPC(vch) && IS_NPC(ch) &&
         (ch->fighting != vch || vch->fighting != ch)))
      continue;

    if (vch == victim)          /* full damage */
    {
      if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
      {
        ch->attacker = true;
        victim->attacker = false;
      }
      if (saves_spell(level, vch, DAM_COLD))
      {
        cold_effect(vch, level / 2, dam / 4, TARGET_CHAR);
        xdamage(ch, vch, dam / 2, sn, DAM_COLD, true, VERBOSE_STD, &mobdeath);
      }
      else
      {
        cold_effect(vch, level, dam, TARGET_CHAR);
        xdamage(ch, vch, dam, sn, DAM_COLD, true, VERBOSE_STD, &mobdeath);
      }
    }
    else
    {
      if (saves_spell(level - 2, vch, DAM_COLD))
      {
        cold_effect(vch, level / 4, dam / 8, TARGET_CHAR);
        xdamage(ch, vch, dam / 4, sn, DAM_COLD, true, VERBOSE_STD, &mobdeath);
      }
      else
      {
        cold_effect(vch, level / 2, dam / 4, TARGET_CHAR);
        xdamage(ch, vch, dam / 2, sn, DAM_COLD, true, VERBOSE_STD, &mobdeath);
      }
    }
  }
}

MAGIC(spell_gas_breath)
{
  CHAR_DATA *vch;
  CHAR_DATA *vch_next;
  int dam, hp_dam, dice_dam, hpch;
  bool mobdeath = false;

  act("$n breathes out a cloud of poisonous gas!", ch, NULL, NULL, TO_ROOM);
  act("You breath out a cloud of poisonous gas.", ch, NULL, NULL, TO_CHAR);

  hpch = UMAX(16, ch->hit);
  hp_dam = number_range(hpch / 15 + 1, 8);
  dice_dam = dice(level, 15);

  dam = UMAX(hp_dam + dice_dam / 10, dice_dam + hp_dam / 10);
  poison_effect(ch->in_room, level, dam, TARGET_ROOM);

  for (vch = ch->in_room->people; vch != NULL; vch = vch_next)
  {
    vch_next = vch->next_in_room;

    if (is_safe_spell(ch, vch, true) ||
        (IS_NPC(ch) && IS_NPC(vch) &&
         (ch->fighting == vch || vch->fighting == ch)))
      continue;

    if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(vch)))
    {
      ch->attacker = true;
      vch->attacker = false;
    }
    if (saves_spell(level, vch, DAM_POISON))
    {
      poison_effect(vch, level / 2, dam / 4, TARGET_CHAR);
      xdamage(ch, vch, dam / 2, sn, DAM_POISON, true, VERBOSE_STD, &mobdeath);
    }
    else
    {
      poison_effect(vch, level, dam, TARGET_CHAR);
      xdamage(ch, vch, dam, sn, DAM_POISON, true, VERBOSE_STD, &mobdeath);
    }
  }
}

MAGIC(spell_lightning_breath)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, hp_dam, dice_dam, hpch;
  bool mobdeath = false;

  act("$n breathes a bolt of lightning at $N.", ch, NULL, victim, TO_NOTVICT);
  act("$n breathes a bolt of lightning at you!", ch, NULL, victim, TO_VICT);
  act("You breathe a bolt of lightning at $N.", ch, NULL, victim, TO_CHAR);

  hpch = UMAX(10, ch->hit);
  hp_dam = number_range(hpch / 9 + 1, hpch / 5);
  dice_dam = dice(level, 32);

  dam = UMAX(hp_dam + dice_dam / 10, dice_dam + hp_dam / 10);

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  if (saves_spell(level, victim, DAM_LIGHTNING))
  {
    shock_effect(victim, level / 2, dam / 4, TARGET_CHAR);
    xdamage(ch, victim, dam / 2, sn, DAM_LIGHTNING, true, VERBOSE_STD,
            &mobdeath);
  }
  else
  {
    shock_effect(victim, level, dam, TARGET_CHAR);
    xdamage(ch, victim, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD, &mobdeath);
  }
}

/*
 * Spells for mega1.are from Glop/Erkenbrand.
 */
MAGIC(spell_general_purpose)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  dam = number_range(25, 100);
  if (saves_spell(level, victim, DAM_PIERCE))
    dam /= 2;
  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dam, sn, DAM_PIERCE, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_high_explosive)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam;
  bool mobdeath = false;

  dam = number_range(30, 120);
  if (saves_spell(level, victim, DAM_PIERCE))
    dam /= 2;
  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }
  xdamage(ch, victim, dam, sn, DAM_PIERCE, true, VERBOSE_STD, &mobdeath);
  return;
}

MAGIC(spell_4x)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  AFFECT_DATA af;

  if (is_affected(victim, sn))
    return;
  af.where = TO_AFFECTS;
  af.type = sn;
  af.duration = level / 20;     /* Feel free to give more or less */
  af.location = APPLY_NONE;
  af.modifier = 4;

  af.bitvector = 0;
  affect_to_char(victim, &af);
  send_to_char("{YYou begin to learn more quickly from your kills!{x\n\r",
               victim);
  act("{Y$n{x {Wseems to{x {Gshimmer with knowledge.{x", victim, NULL, NULL,
      TO_ROOM);
  return;
}

MAGIC(spell_acid_rain)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  int dam, i;
  bool mobdeath = false;

  if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
  {
    ch->attacker = true;
    victim->attacker = false;
  }

  act
    ("{CW{ca{Cv{ce{Cs{g of {Ga{gc{Gi{gd{Gi{gc {Brain {gcalled by $n shower down upon {R$N{g!{x",
     ch, NULL, victim, TO_NOTVICT);
  act
    ("{CW{ca{Cv{ce{Cs{g of {Ga{gc{Gi{gd{Gi{gc {Brain {gcalled by $n shower down upon {Ryou{g!{x",
     ch, NULL, victim, TO_VICT);
  act
    ("{CW{ca{Cv{ce{Cs{g of {Ga{gc{Gi{gd{Gi{gc {Brain {gshower down upon {R$N{g!{x",
     ch, NULL, victim, TO_CHAR);
  i = 8;
  while (i > 0 && !mobdeath)
  {
    dam = dice(level, level / 10);
    if (saves_spell(level, victim, DAM_ACID))
      dam /= 1.5;
    xdamage(ch, victim, dam, sn, DAM_ACID, true, VERBOSE_STD, &mobdeath);
    i--;
  }
  return;
}

MAGIC(spell_elemental_fury)
{
  CHAR_DATA *victim = (CHAR_DATA *) vo;
  CHAR_DATA *vic_next;
  int dam, cchan;
  bool mobdeath = false;

  act("$n unleashes the {Gf{gu{Yr{Ry{x of the {Re{Yl{Ge{Cm{Be{Gn{Yt{Rs{x!",
      ch, NULL, NULL, TO_ROOM);
  send_to_char
    ("You unleash the {Gf{gu{Yr{Ry{x of the {Re{Yl{Ge{Cm{Be{Gn{Yt{Rs{x!\n\r",
     ch);

  cchan = ((get_skill(ch, gsn_elemental_fury)) - 25);

  for (victim = char_list; victim != NULL; victim = vic_next)
  {
    vic_next = victim->next;
    mobdeath = false;
    if (victim->in_room == NULL)
      continue;
    if (victim->in_room == ch->in_room)
    {
      if (victim != ch && !is_safe_spell(ch, victim, true))
      {
        if ((ch->fighting == NULL) && (!IS_NPC(ch)) && (!IS_NPC(victim)))
        {
          ch->attacker = true;
          victim->attacker = false;
        }
        if (number_percent() <= cchan)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act("$n is harmed by the element of {Ga{gc{Gi{Gd{x!", victim,
                NULL, NULL, TO_ROOM);
            send_to_char("You are harmed by the element of {Ga{gc{Gi{Gd{x!",
                         victim);
            xdamage(ch, victim, dam, sn, DAM_ACID, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
        if (number_percent() <= cchan && !mobdeath)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act("$n is harmed by the element of {Ci{Wc{ce{x!", victim, NULL,
                NULL, TO_ROOM);
            send_to_char("You are harmed by the element of {Ci{Wc{ce{x!",
                         victim);
            xdamage(ch, victim, dam, sn, DAM_COLD, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
        if (number_percent() <= cchan && !mobdeath)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act("$n is harmed by the element of {Rf{Yi{rr{Re{x!", victim,
                NULL, NULL, TO_ROOM);
            send_to_char("You are harmed by the element of {Rf{Yi{rr{Re{x!",
                         victim);
            xdamage(ch, victim, dam, sn, DAM_FIRE, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
        if (number_percent() <= cchan && !mobdeath)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act
              ("$n is harmed by the element of {yl{Yi{Wg{Dh{Yt{wn{Wi{Yn{yg{x!",
               victim, NULL, NULL, TO_ROOM);
            send_to_char
              ("You are harmed by the element of {yl{Yi{Wg{Dh{Yt{wn{Wi{Yn{yg{x!",
               victim);
            xdamage(ch, victim, dam, sn, DAM_LIGHTNING, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
        if (number_percent() <= cchan && !mobdeath)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act("$n is harmed by the element of {ye{ga{yr{Gt{gh{x!", victim,
                NULL, NULL, TO_ROOM);
            send_to_char
              ("You are harmed by the element of {ye{ga{yr{Gt{gh{x!", victim);
            xdamage(ch, victim, dam, sn, DAM_EARTH, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
        if (number_percent() <= cchan && !mobdeath)
        {
          dam = dice(level * 5, 6);
          if (victim->in_room == ch->in_room)
          {
            act("$n is harmed by the element of {Cw{Ba{ct{Ce{Br{x!", victim,
                NULL, NULL, TO_ROOM);
            send_to_char
              ("You are harmed by the element of {Cw{Ba{ct{Ce{Br{x!", victim);
            xdamage(ch, victim, dam, sn, DAM_WATER, true, VERBOSE_STD,
                    &mobdeath);
          }
        }
      }
      continue;
    }
  }
  return;
}

// Summon spells!
MAGIC(spell_summon_mephit)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_summon_elemental)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_summon_companion)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_summon_archon)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_summon_songbird)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_summon_resurrect)
{
  get_summon_type(ch, fullarg, sn);
  return;
}

MAGIC(spell_bone_barrier)
{
  get_summon_type(ch, fullarg, sn);
  return;
}
