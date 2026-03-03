#ifndef MAP_H
#define MAP_H

#include "../physics/movement.h"

#define MAP_MAX_BOXES 64

typedef struct {
    Vector3 min;
    Vector3 max;
    Color   color;
} MapBox;

typedef struct {
    MapBox boxes[MAP_MAX_BOXES];
    int    count;
} Map;

void map_build(Map *m);
void map_draw(const Map *m);
void map_collide(const Map *m, PlayerState *p);

#endif
