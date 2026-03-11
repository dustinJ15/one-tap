#include "round.h"
#include <string.h>

void round_init(RoundState *r)
{
    memset(r, 0, sizeof(*r));
    round_start(r);
}

void round_start(RoundState *r)
{
    r->phase     = PHASE_LIVE;
    r->timer     = (float)ROUND_TIME_SECS;
    r->end_timer = (float)ROUND_END_SECS;
    r->win_team  = 0;
}

bool round_tick_live(RoundState *r, float dt, int ct_alive, int t_alive,
                     int ct_total, int t_total)
{
    if (r->phase != PHASE_LIVE) return false;

    r->timer -= dt;
    if (r->timer < 0.0f) r->timer = 0.0f;

    bool end = false;

    /* Elimination win conditions — only check when both teams are present */
    if (ct_total > 0 && t_total > 0) {
        if (t_alive == 0)  { r->win_team = 1; end = true; } /* CT win */
        if (ct_alive == 0) { r->win_team = 2; end = true; } /* T win */
    }

    /* Time expired — T wins */
    if (r->timer <= 0.0f && !end) {
        r->win_team = 2;
        end = true;
    }

    if (end) {
        if (r->win_team == 1) r->ct_score++;
        else                  r->t_score++;
        r->phase     = PHASE_END;
        r->end_timer = (float)ROUND_END_SECS;
    }

    return end;
}

bool round_tick_end(RoundState *r, float dt)
{
    if (r->phase != PHASE_END) return false;

    r->end_timer -= dt;
    if (r->end_timer <= 0.0f) {
        r->end_timer = 0.0f;
        return true;
    }
    return false;
}
