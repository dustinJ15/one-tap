#ifndef ROUND_H
#define ROUND_H

#include <stdbool.h>

#define ROUND_TIME_SECS  115   /* 1:55 */
#define ROUND_END_SECS     5   /* scoreboard display time */
#define BUY_TIME_SECS     15   /* buy phase at round start */

typedef enum { TEAM_CT = 0, TEAM_T = 1 } Team;

typedef enum {
    PHASE_LIVE = 0,
    PHASE_END  = 1,
    PHASE_BUY  = 2,
} RoundPhase;

typedef struct {
    RoundPhase phase;
    float      timer;           /* seconds remaining (PHASE_LIVE) */
    float      buy_timer;       /* seconds remaining (PHASE_BUY) */
    float      end_timer;       /* seconds remaining (PHASE_END) */
    int        ct_score;
    int        t_score;
    int        win_team;        /* 0=none/time 1=CT 2=T */
    int        ct_loss_streak;
    int        t_loss_streak;
} RoundState;

void round_init(RoundState *r);
void round_start(RoundState *r);

/* Tick during PHASE_BUY. Returns true when buy phase ends (→ PHASE_LIVE). */
bool round_tick_buy(RoundState *r, float dt);

/* Tick during PHASE_LIVE. Returns true when round just ended. */
bool round_tick_live(RoundState *r, float dt, int ct_alive, int t_alive,
                     int ct_total, int t_total);

/* Tick during PHASE_END. Returns true when end phase expires (restart). */
bool round_tick_end(RoundState *r, float dt);

#endif
