#pragma once
#include "Character.h"

enum class ProvState {
    IDLE,
    GO_TO_PATROL,
    GO_AMMO_DEPOT,
    GO_TO_WARRIOR,
    SUPPLY,
    WOUNDED 
};

class Warrior;

class Provider : public Character {
public:
    ProvState state;

    // Mission
    Warrior* targetWarrior;

    // Depot location
    Point depotAmmo;
    bool hasDepotAmmo;

    // Patrol
    Point patrolTarget;
    bool hasPatrolTarget;
    int nextPatrolSwitchTick;

    // Wounded state
    bool reportedWounded;

    Provider(Point p, int team);

    void update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) override;

    // Commander interface
    void assignTask(Warrior* target);
    bool isIdle() const;
};