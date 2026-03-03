#ifndef MAP_H
#define MAP_H

#include "../physics/movement.h"

typedef struct {
    bool    hit;
    Vector3 point;
    Vector3 normal;
    float   t;
} RayHit;

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

void   map_build(Map *m);
void   map_draw(const Map *m);
void   map_collide(const Map *m, PlayerState *p);
RayHit map_raycast(const Map *m, Vector3 origin, Vector3 dir);

#endif
