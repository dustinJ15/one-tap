#ifndef MOVEMENT_H
#define MOVEMENT_H

#include "raylib.h"
#include <stdbool.h>

typedef struct {
    float yaw_delta;   /* degrees */
    float pitch_delta; /* degrees */
    int   fwd;         /* +1/-1/0 */
    int   right;       /* +1/-1/0 */
    bool  jump;        /* rising edge only */
    bool  crouch;
} PlayerInput;

/* CS 1.6 movement constants */
#define PLAYER_SPEED_GROUND   250.0f   /* max run speed (units/s) */
#define PLAYER_SPEED_CROUCH    68.0f   /* max crouch speed */
#define PLAYER_AIR_SPEED_CAP   30.0f   /* wish_speed cap in air */
#define PLAYER_ACCEL_GROUND     5.6f   /* sv_accelerate */
#define PLAYER_ACCEL_AIR       10.0f   /* sv_airaccelerate */
#define PLAYER_FRICTION         4.0f   /* sv_friction */
#define PLAYER_STOP_SPEED     100.0f   /* sv_stopspeed */
#define PLAYER_GRAVITY        800.0f   /* sv_gravity (units/s^2) */
#define PLAYER_JUMP_VELOCITY  268.0f   /* upward velocity on jump */
#define PLAYER_EYE_STAND       72.0f   /* eye height standing */
#define PLAYER_EYE_CROUCH      36.0f   /* eye height crouching */
#define PLAYER_HALF_WIDTH      16.0f   /* collision box half-width (X and Z) */

#define MOUSE_SENSITIVITY       0.1f
#define CAMERA_FOV             90.0f

typedef struct {
    Vector3 position;   /* eye position in world space */
    Vector3 velocity;
    float   yaw;        /* horizontal angle, degrees, clockwise from -Z */
    float   pitch;      /* vertical angle, degrees, clamped [-89, 89] */
    bool    on_ground;
    bool    crouching;
    int     health;
} PlayerState;

void     player_init(PlayerState *p);
void     player_update(PlayerState *p, const PlayerInput *in, float dt);
Camera3D player_camera(const PlayerState *p);

#endif
