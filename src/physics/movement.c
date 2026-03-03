#include "movement.h"
#include <math.h>

/* Apply ground friction using CS/Quake formula with stopspeed scaling. */
static void apply_friction(PlayerState *p, float dt)
{
    float speed = sqrtf(p->velocity.x * p->velocity.x +
                        p->velocity.z * p->velocity.z);
    if (speed < 0.5f) {
        p->velocity.x = 0.0f;
        p->velocity.z = 0.0f;
        return;
    }
    /* At low speeds, scale friction up to stopspeed to avoid sliding forever */
    float control  = speed < PLAYER_STOP_SPEED ? PLAYER_STOP_SPEED : speed;
    float newspeed = speed - control * PLAYER_FRICTION * dt;
    if (newspeed < 0.0f) newspeed = 0.0f;
    newspeed /= speed;
    p->velocity.x *= newspeed;
    p->velocity.z *= newspeed;
}

/*
 * Quake-style acceleration:
 *   only add velocity in wish_dir up to wish_speed.
 *   accel_speed = accel * wish_speed * dt, capped by remaining headroom.
 */
static void accelerate(PlayerState *p, Vector3 wish_dir, float wish_speed,
                        float accel, float dt)
{
    if (wish_speed == 0.0f) return;
    float current    = p->velocity.x * wish_dir.x + p->velocity.z * wish_dir.z;
    float add        = wish_speed - current;
    if (add <= 0.0f) return;
    float accel_speed = accel * wish_speed * dt;
    if (accel_speed > add) accel_speed = add;
    p->velocity.x += accel_speed * wish_dir.x;
    p->velocity.z += accel_speed * wish_dir.z;
}

void player_init(PlayerState *p)
{
    p->position  = (Vector3){ 0.0f, PLAYER_EYE_STAND, 0.0f };
    p->velocity  = (Vector3){ 0.0f, 0.0f, 0.0f };
    p->yaw       = 0.0f;
    p->pitch     = 0.0f;
    p->on_ground = true;
    p->crouching = false;
}

void player_update(PlayerState *p, float dt)
{
    /* --- Mouse look --- */
    Vector2 md  = GetMouseDelta();
    p->yaw     += md.x * MOUSE_SENSITIVITY;
    p->pitch   -= md.y * MOUSE_SENSITIVITY;
    if (p->pitch >  89.0f) p->pitch =  89.0f;
    if (p->pitch < -89.0f) p->pitch = -89.0f;

    /* --- Wish direction from WASD --- */
    float fwd_in   = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    float right_in = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));

    float yr = p->yaw * DEG2RAD;
    /* Horizontal movement vectors — pitch does not affect move direction in CS */
    Vector3 h_fwd   = {  sinf(yr), 0.0f, -cosf(yr) };
    Vector3 h_right = {  cosf(yr), 0.0f,  sinf(yr) };

    Vector3 wish_dir = {
        h_fwd.x * fwd_in + h_right.x * right_in,
        0.0f,
        h_fwd.z * fwd_in + h_right.z * right_in
    };

    /* Normalize diagonal movement */
    float wish_len = sqrtf(wish_dir.x * wish_dir.x + wish_dir.z * wish_dir.z);
    if (wish_len > 1.0f) {
        wish_dir.x /= wish_len;
        wish_dir.z /= wish_len;
        wish_len = 1.0f;
    }

    /* --- Crouch --- */
    p->crouching      = IsKeyDown(KEY_LEFT_CONTROL);
    float max_speed   = p->crouching ? PLAYER_SPEED_CROUCH : PLAYER_SPEED_GROUND;
    float wish_speed  = wish_len * max_speed;

    /* --- Ground vs air --- */
    if (p->on_ground) {
        apply_friction(p, dt);
        accelerate(p, wish_dir, wish_speed, PLAYER_ACCEL_GROUND, dt);
        if (IsKeyPressed(KEY_SPACE)) {
            p->velocity.y = PLAYER_JUMP_VELOCITY;
            p->on_ground  = false;
        }
    } else {
        /* Air accel: wish_speed capped at 30 — low cap is what gives CS its feel */
        float air_wish = wish_speed < PLAYER_AIR_SPEED_CAP ? wish_speed
                                                            : PLAYER_AIR_SPEED_CAP;
        accelerate(p, wish_dir, air_wish, PLAYER_ACCEL_AIR, dt);
        p->velocity.y -= PLAYER_GRAVITY * dt;
    }

    /* --- Integrate position --- */
    p->position.x += p->velocity.x * dt;
    p->position.y += p->velocity.y * dt;
    p->position.z += p->velocity.z * dt;

    /* Collision and on_ground are resolved by map_collide() after this call. */
}

Camera3D player_camera(const PlayerState *p)
{
    float yr = p->yaw   * DEG2RAD;
    float pr = p->pitch * DEG2RAD;

    /* Look target: eye position + view direction unit vector */
    Vector3 target = {
        p->position.x + sinf(yr) * cosf(pr),
        p->position.y + sinf(pr),
        p->position.z - cosf(yr) * cosf(pr)
    };

    Camera3D cam    = { 0 };
    cam.position    = p->position;
    cam.target      = target;
    cam.up          = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy        = CAMERA_FOV;
    cam.projection  = CAMERA_PERSPECTIVE;
    return cam;
}
