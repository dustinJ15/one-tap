#include "economy.h"

const WeaponDef WEAPONS[WEAPON_COUNT] = {
    [WEAPON_PISTOL] = { "Pistol", 0 },
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
