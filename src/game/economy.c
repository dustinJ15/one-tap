#include "economy.h"
#include <stdbool.h>

const WeaponDef WEAPONS[WEAPON_COUNT] = {
    /*           name    price  ammo$ mag res  dmg  rate  rld   move  semi */
    [WEAPON_PISTOL] = { "Pistol", 0,   200,  12,  36,  35, 0.0f,  1.0f, 1.0f, true  },
    [WEAPON_AK]     = { "AK",  2700,   400,  30,  90, 100, 0.10f, 2.5f, 0.85f, false },
    [WEAPON_M4]     = { "M4",  3100,   400,  30,  90,  70, 0.09f, 3.1f, 0.90f, false },
    [WEAPON_AWP]    = { "AWP", 4750,   300,  10,  30, 100, 1.50f, 3.7f, 0.70f, true  },
};

void economy_add(int *money, int amount)
{
    *money += amount;
    if (*money > MONEY_MAX) *money = MONEY_MAX;
    if (*money < 0)         *money = 0;
}

int economy_loss_bonus(int streak)
{
    int capped = streak - 1;
    if (capped > 4) capped = 4;
    return MONEY_LOSS + capped * MONEY_LOSS_BONUS;
}
