/* The AI improvement, part 1: the computer players follow the human's
 * transmitter network (option --ai-improve / [game] ai_improve, off by
 * default; docs/ki-verbesserung.md).  Not a fix -- a game option, decompiled
 * build only, never under --difftest.  Off: none of these do anything and
 * nothing is drawn, the original game bit for bit.
 *
 *   ai_improve_on          the option is on (1 normal, 2 strong; 0 off)
 *   ai_improve_step        ai_tick (2DD9:000C), every step of a computer player:
 *                          its state made at the first step, watching the
 *                          strongest human, the wait, the reaction's start
 *   ai_improve_rebuild     ai_tick: the computer player's coverage map is
 *                          rebuilt from step 0 (its follow-up search starts anew)
 *   ai_improve_wants       ai_want_transmitter (2DD9:4B84): a follow-up
 *                          purchase is due -- the transmitter pot and the hour
 *                          do not matter
 *   ai_improve_scan        ai_scan_transmitter_site (2DD9:4862): 1 if it took
 *                          the call -- the follow-up search, 30 points a step,
 *                          the site rule, the purchase from the cash
 *   ai_improve_follow_fees pay_daily_costs (1DA8:0FB9): the fees of the
 *                          follow-up transmitters: the cash and half the
 *                          transmitter pot pay them, the other pots nothing
 *   ai_improve_save_size / ai_improve_save / ai_improve_load
 *                          the state in a savegame (the fourth part of the
 *                          "MTVX" extension, src/actions.c); a savegame
 *                          without it, or a new game, starts the state anew */
#ifndef AI_IMPROVE_H
#define AI_IMPROVE_H
#include "dec.h"

int ai_improve_on(void);
void ai_improve_step(u16 player);
void ai_improve_rebuild(u16 player);
int ai_improve_wants(u16 player);
int ai_improve_scan(u16 player);
u32 ai_improve_follow_fees(u16 player);
u16 ai_improve_save_size(void);          /* 0: no state to save (the option is off) */
void ai_improve_save(u8 *buf);           /* ai_improve_save_size() bytes */
void ai_improve_load(const u8 *buf, u16 len);   /* NULL: none in the savegame */
#endif
