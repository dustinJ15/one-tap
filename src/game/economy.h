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
    WEAPON_COUNT
} WeaponId;

typedef struct {
    const char *name;
    int         price;
} WeaponDef;

extern const WeaponDef WEAPONS[WEAPON_COUNT];

/* Clamp money to [0, MONEY_MAX] after adding amount (negative to subtract). */
void economy_add(int *money, int amount);

/* Return loss reward for the given consecutive loss count (1-indexed). */
int economy_loss_bonus(int streak);

#endif
