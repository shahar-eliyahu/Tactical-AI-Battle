#pragma once
#include "Character.h"

enum class WarState {
    IDLE,        // Patrolling 0
    ADVANCE,     // Moving to objective 1
    ATTACK,      // Engaging enemy 2
    DEFEND,      // Seeking cover 3
    WOUNDED,     // Injured, seeking help 4 
    LOW_AMMO     // Need resupply 5
};

class Warrior : public Character {
public:
    WarState state;

    // Patrol
    Point patrolTarget;
    bool hasPatrolTarget;
    int nextPatrolSwitchTick;
    int grenadeCooldown = 0;
 
    // Orders from commander
    Point advanceTarget;
    bool hasAdvanceTarget;

    // Reporting flags
    bool reportedWounded;
    bool reportedLowAmmo;
    bool reachedCover;  

    Warrior(Point p, int team);

    void update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) override;

    // Commander interface
    void orderAdvance(Point target);
    void orderAttack(Point target);
    void orderDefend();

    bool needsAmmo() const;
    bool needsGrenades() const;
    void refillAmmo();
    void refillGrenades();

    int bulletCooldown;  // How many ticks are left until the next shot


};