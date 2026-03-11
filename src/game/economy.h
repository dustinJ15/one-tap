#ifndef ECONOMY_H
#define ECONOMY_H

#define MONEY_START       800
#define MONEY_MAX       16000
#define MONEY_KILL        300
#define MONEY_WIN        3250
#define MONEY_LOSS       1400
#define MONEY_LOSS_BONUS  500   /* added per consecutive loss, capped at 4x */

typedef enum {
    WEAPON_PISTOL = 0,
    WEAPON_AK     = 1,
    WEAPON_M4     = 2,
    WEAPON_AWP    = 3,
    WEAPON_COUNT
} WeaponId;

typedef struct {
    const char *name;
    int         price;
    int         ammo_price;    /* cost to refill reserve */
    int         ammo_mag;
    int         ammo_reserve;
    int         damage;
    float       fire_rate;     /* seconds between shots */
    float       reload_time;   /* seconds to reload */
    float       move_factor;   /* max speed multiplier (1.0 = normal) */
    bool        semi_auto;     /* true = one shot per click */
} WeaponDef;

extern const WeaponDef WEAPONS[WEAPON_COUNT];

void economy_add(int *money, int amount);
int  economy_loss_bonus(int streak);

#endif
