
/***************************************************************************
 *  File: mem.c                                                            *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 *                                                                         *
 *  This code was freely distributed with the The Isles 1.1 source code,   *
 *  and has been used here for OLC - OLC would not be what it is without   *
 *  all the previous coders who released their source code.                *
 *                                                                         *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "merc.h"
#include "recycle.h"
#include "db.h"
#include "fd_property.h"


RESET_DATA *new_reset_data(void)
{
  RESET_DATA *pReset;

  pReset = (RESET_DATA *) malloc(sizeof(*pReset));
  memset(pReset, 0, sizeof(*pReset));
  top_reset++;

  pReset->next = NULL;
  pReset->command = 'X';
  pReset->arg1 = 0;
  pReset->arg2 = 0;
  pReset->arg3 = 0;
  pReset->arg4 = 0;

  return pReset;
}

void free_reset_data(RESET_DATA * pReset)
{
  top_reset--;
  free(pReset);
  return;
}

AREA_DATA *new_area(void)
{
  static int top_area_vnum = 0;
  AREA_DATA *pArea;
  char buf[MAX_INPUT_LENGTH];

  pArea = (AREA_DATA *) malloc(sizeof(*pArea));
  memset(pArea, 0, sizeof(*pArea));
  top_area++;
  top_area_vnum++;

  pArea->next = NULL;
  pArea->next_sort = NULL;
  pArea->reset_first = NULL;
  pArea->reset_last = NULL;
  pArea->name = str_dup("New area");
  pArea->area_flags = AREA_ADDED;
  pArea->security = 1;
  pArea->builders = str_dup("None");
  pArea->credits = str_dup("[000-000] OLC    New Area");
  pArea->min_vnum = 0;
  pArea->max_vnum = 0;
  pArea->age = 0;
  pArea->nplayer = 0;
  pArea->empty = true;          /* ROM patch */
  pArea->vnum = top_area_vnum;
  sprintf(buf, "area%d.are", pArea->vnum);
  pArea->file_name = str_dup(buf);
  pArea->low_range = 0;
  pArea->high_range = 0;
  pArea->repop_msg = NULL;
  pArea->id = 0;

  return pArea;
}

void free_area(AREA_DATA * pArea)
{
  free_string(pArea->name);
  free_string(pArea->file_name);
  free_string(pArea->builders);
  free_string(pArea->credits);
  top_area--;
  free(pArea);
  return;
}

EXIT_DATA *new_exit(void)
{
  EXIT_DATA *pExit;

  pExit = (EXIT_DATA *) malloc(sizeof(*pExit));
  memset(pExit, 0, sizeof(*pExit));
  top_exit++;

  pExit->u1.to_room = NULL;     /* ROM OLC */
  pExit->u1.vnum = 0;
  pExit->next = NULL;
  pExit->exit_info = 0;
  pExit->key = 0;
  pExit->keyword = &str_empty[0];
  pExit->description = &str_empty[0];
  pExit->rs_flags = 0;
  pExit->orig_door = 0;

  return pExit;
}

void free_exit(EXIT_DATA * pExit)
{
  top_exit--;
  free_string(pExit->keyword);
  free_string(pExit->description);
  free(pExit);
  return;
}

ROOM_INDEX_DATA *new_room_index(void)
{
  ROOM_INDEX_DATA *pRoom;
  int door;

  pRoom = (ROOM_INDEX_DATA *) malloc(sizeof(*pRoom));
  memset(pRoom, 0, sizeof(*pRoom));
  top_room++;

  pRoom->next = NULL;
  pRoom->people = NULL;
  pRoom->contents = NULL;
  pRoom->extra_descr = NULL;
  pRoom->area = NULL;
  pRoom->property = NULL;

  for (door = 0; door < MAX_DIR; door++)
  {
    pRoom->exit[door] = NULL;
    pRoom->old_exit[door] = NULL;
  }

  pRoom->reset_first = NULL;
  pRoom->reset_last = NULL;


  pRoom->name = &str_empty[0];
  pRoom->description = &str_empty[0];
  pRoom->owner = &str_empty[0];
  pRoom->vnum = 0;
  pRoom->room_flags = 0;
  pRoom->light = 0;
  pRoom->sector_type = 0;
  pRoom->clan = 0;
  pRoom->heal_rate = 100;
  pRoom->mana_rate = 100;
  pRoom->tele_dest = 0;

  return pRoom;
}

void free_room_index(ROOM_INDEX_DATA * pRoom)
{
  int door;
  EXTRA_DESCR_DATA *pExtra;
  RESET_DATA *pReset;
  PROPERTY *pProp, *pProp_next;

  top_room--;

  free_string(pRoom->name);
  free_string(pRoom->description);
  free_string(pRoom->owner);

  for (door = 0; door < MAX_DIR; door++)
  {
    if (pRoom->exit[door])
      free_exit(pRoom->exit[door]);
  }

  for (pExtra = pRoom->extra_descr; pExtra; pExtra = pExtra->next)
  {
    free_extra_descr(pExtra);
  }

  for (pReset = pRoom->reset_first; pReset; pReset = pReset->next)
  {
    free_reset_data(pReset);
  }

  pProp = pRoom->property;
  while (pProp)
  {
    pProp_next = pProp->next;
    free_property(pProp);
    pProp = pProp_next;
  }

  free(pRoom);
  return;
}

SHOP_DATA *new_shop(void)
{
  SHOP_DATA *pShop;
  int buy;

  pShop = (SHOP_DATA *) malloc(sizeof(*pShop));
  memset(pShop, 0, sizeof(*pShop));
  top_shop++;

  pShop->next = NULL;
  pShop->keeper = 0;

  for (buy = 0; buy < MAX_TRADE; buy++)
    pShop->buy_type[buy] = 0;

  pShop->profit_buy = 100;
  pShop->profit_sell = 100;
  pShop->open_hour = 0;
  pShop->close_hour = 23;

  return pShop;
}

void free_shop(SHOP_DATA * pShop)
{
  top_shop--;
  free(pShop);
  return;
}

OBJ_INDEX_DATA *new_obj_index(void)
{
  OBJ_INDEX_DATA *pObj;
  int value;

  pObj = (OBJ_INDEX_DATA *) malloc(sizeof(*pObj));
  memset(pObj, 0, sizeof(*pObj));
  top_obj_index++;

  pObj->next = NULL;
  pObj->extra_descr = NULL;
  pObj->affected = NULL;
  pObj->area = NULL;
  pObj->name = str_dup("no name");
  pObj->short_descr = str_dup("(no short description)");
  pObj->description = str_dup("(no description)");
  pObj->vnum = 0;
  pObj->reset_num = 0;
  pObj->item_type = ITEM_TRASH;
  pObj->extra_flags = 0;
  pObj->wear_flags = 0;
  pObj->level = 0;
  pObj->count = 0;
  pObj->weight = 0;
  pObj->cost = 0;
  pObj->material = str_dup("unknown");  /* ROM */
  pObj->condition = 100;        /* ROM */
  for (value = 0; value < 5; value++) /* 5 - ROM */
    pObj->value[value] = 0;
  pObj->clan = 0;
  pObj->class = 0;
  pObj->property = NULL;

  return pObj;
}

void free_obj_index(OBJ_INDEX_DATA * pObj)
{
  EXTRA_DESCR_DATA *pExtra;
  AFFECT_DATA *pAf;

  top_obj_index--;

  free_string(pObj->name);
  free_string(pObj->short_descr);
  free_string(pObj->description);
  free_string(pObj->material);
  for (pAf = pObj->affected; pAf; pAf = pAf->next)
  {
    free_affect(pAf);
  }

  for (pExtra = pObj->extra_descr; pExtra; pExtra = pExtra->next)
  {
    free_extra_descr(pExtra);
  }

  free(pObj);
  return;
}

MOB_INDEX_DATA *new_mob_index(void)
{
  MOB_INDEX_DATA *pMob;

  pMob = (MOB_INDEX_DATA *) malloc(sizeof(*pMob));
  memset(pMob, 0, sizeof(*pMob));
  top_mob_index++;

  pMob->next = NULL;
  pMob->spec_fun = NULL;
  pMob->pShop = NULL;
  pMob->mprogs = NULL;
  pMob->area = NULL;
  pMob->player_name = str_dup("no name");
  pMob->short_descr = str_dup("(no short description)");
  pMob->long_descr = str_dup("(no long description)\n\r");
  pMob->description = &str_empty[0];
  pMob->vnum = 0;
  pMob->group = 0;
  pMob->count = 0;
  pMob->killed = 0;
  pMob->sex = 0;
  pMob->level = 0;
  pMob->act = ACT_IS_NPC;
  pMob->act2 = 0;
  pMob->affected_by = 0;
  pMob->shielded_by = 0;
  pMob->alignment = 0;
  pMob->hitroll = 0;
  pMob->race = race_lookup("human");  /* - Hugin */
  pMob->form = 0;               /* ROM patch -- Hugin */
  pMob->parts = 0;              /* ROM patch -- Hugin */
  pMob->imm_flags = 0;          /* ROM patch -- Hugin */
  pMob->res_flags = 0;          /* ROM patch -- Hugin */
  pMob->vuln_flags = 0;         /* ROM patch -- Hugin */
  pMob->material = str_dup("unknown");  /* -- Hugin */
  pMob->off_flags = 0;          /* ROM patch -- Hugin */
  pMob->size = SIZE_MEDIUM;     /* ROM patch -- Hugin */
  pMob->ac[AC_PIERCE] = 0;      /* ROM patch -- Hugin */
  pMob->ac[AC_BASH] = 0;        /* ROM patch -- Hugin */
  pMob->ac[AC_SLASH] = 0;       /* ROM patch -- Hugin */
  pMob->ac[AC_EXOTIC] = 0;      /* ROM patch -- Hugin */
  pMob->hit[DICE_NUMBER] = 0;   /* ROM patch -- Hugin */
  pMob->hit[DICE_TYPE] = 0;     /* ROM patch -- Hugin */
  pMob->hit[DICE_BONUS] = 0;    /* ROM patch -- Hugin */
  pMob->mana[DICE_NUMBER] = 0;  /* ROM patch -- Hugin */
  pMob->mana[DICE_TYPE] = 0;    /* ROM patch -- Hugin */
  pMob->mana[DICE_BONUS] = 0;   /* ROM patch -- Hugin */
  pMob->damage[DICE_NUMBER] = 0;  /* ROM patch -- Hugin */
  pMob->damage[DICE_TYPE] = 0;  /* ROM patch -- Hugin */
  pMob->damage[DICE_NUMBER] = 0;  /* ROM patch -- Hugin */
  pMob->start_pos = POS_STANDING; /* -- Hugin */
  pMob->default_pos = POS_STANDING; /* -- Hugin */
  pMob->wealth = 0;
  pMob->dam_type = 0;
  pMob->mprog_flags = 0;
  pMob->die_descr = &str_empty[0];
  pMob->say_descr = &str_empty[0];
  pMob->property = NULL;

  return pMob;
}

void free_mob_index(MOB_INDEX_DATA * pMob)
{
  top_mob_index--;
  free_string(pMob->player_name);
  free_string(pMob->short_descr);
  free_string(pMob->long_descr);
  free_string(pMob->description);
  free_mprog(pMob->mprogs);
  free_string(pMob->material);
  free_shop(pMob->pShop);

  free(pMob);
  return;
}

MPROG_CODE *mpcode_free;

MPROG_CODE *new_mpcode(void)
{
  MPROG_CODE *NewCode;

  NewCode = (MPROG_CODE *) malloc(sizeof(*NewCode));
  memset(NewCode, 0, sizeof(*NewCode));
  top_mprog_index++;

  NewCode->vnum = 0;
  NewCode->code = str_dup("");
  NewCode->next = NULL;

  return NewCode;
}

void free_mpcode(MPROG_CODE * pMcode)
{
  top_mprog_index--;
  free_string(pMcode->code);
  free(pMcode);
  return;
}
