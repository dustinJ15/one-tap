#ifndef WEAPONS_H
#define WEAPONS_H

#include "raylib.h"
#include "../render/map.h"
#include "../physics/movement.h"

#define WEAPON_SPREAD  1.0f   /* spread half-angle, degrees */

typedef struct {
    Sound sound;
} WeaponState;

void   weapon_init(WeaponState *w);
void   weapon_free(WeaponState *w);
RayHit weapon_shoot(const WeaponState *w, const Map *m, const PlayerState *p);

#endif
