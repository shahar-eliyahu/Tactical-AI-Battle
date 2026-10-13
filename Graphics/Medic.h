#pragma once
#include "Character.h"

enum class MedicState {
    IDLE,
    GO_TO_PATROL,
    GO_MED_DEPOT,
    GO_TO_WOUNDED,
    HEAL
};

class Medic : public Character {
public:
    MedicState state;

    // Mission
    Character* targetCharacter;  

    // Depot location
    Point depotMed;
    bool hasDepotMed;

    // Patrol
    Point patrolTarget;
    bool hasPatrolTarget;
    int nextPatrolSwitchTick;

    Medic(Point p, int team);

    void update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) override;

    // Commander interface
    void assignTask(Character* target);  
    bool isIdle() const;

    int medStock = 0; 
    Character* findClosestAllyToHeal(int map[MSZ][MSZ], double safety[MSZ][MSZ]);

};