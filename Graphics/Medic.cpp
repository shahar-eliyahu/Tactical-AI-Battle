#include "Medic.h"
#include "Warrior.h"
#include "Commander.h"
#include "Game.h"
#include "AI.h"
#include <cstdlib>
#include <iostream>

Medic::Medic(Point p, int team) : Character(p, 'M', team) {
    state = MedicState::IDLE;
    targetCharacter = nullptr;
    hasDepotMed = false;
    hasPatrolTarget = false;
    nextPatrolSwitchTick = 0;  
}

void Medic::assignTask(Character* target) {
    if ((state == MedicState::IDLE || state == MedicState::GO_TO_PATROL) && target) {
        std::cout << " Team " << teamId << " Medic assigned to " << target->icon
            << " at (" << target->pos.x << "," << target->pos.y
            << ") - Going to depot\n";
        targetCharacter = target;
        state = MedicState::GO_MED_DEPOT;
        path.clear();
    }
}

bool Medic::isIdle() const {
    return state == MedicState::IDLE || state == MedicState::GO_TO_PATROL;
}


Character* Medic::findClosestAllyToHeal(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    const auto& mine = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();

    Character* best = nullptr;
    int bestDist = 1000000000;

    for (auto* c : mine) {
        if (!c || c == this || !c->isAlive()) continue;

        if (c->isBeingHealedByMedic) continue;

        int d = manhattan(pos, c->pos);
        if (d < bestDist) {
            bestDist = d;
            best = c;
        }
    }
    return best;
}


void Medic::update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    if (!isAlive() || !pGame) return;

    tick++;

    // Commander status 
    const bool commanderDown = (!pCommander || !pCommander->isAlive());

    //  Self-heal if idle and wounded 
    if (hp <= WOUNDED_THRESHOLD && state == MedicState::IDLE) {
        std::cout << " Team " << teamId << " Medic is wounded, healing self!\n";
        heal(MAX_HEALTH);
        std::cout << " Team " << teamId << " Medic healed self [HP: "
            << WOUNDED_THRESHOLD << " -> " << hp << "]\n";
    }

    //  Enemy spotting 
    {
        const std::vector<Character*>& opps = (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();

        Point moveDir = { pos.x - prevPos.x, pos.y - prevPos.y };
        if (moveDir.x == 0 && moveDir.y == 0) moveDir = { 1, 0 };

        std::vector<unsigned char> cone;
        computeFOVCone(map, pos, (float)moveDir.x, (float)moveDir.y,
            FOV_RADIUS_WARRIOR, FOV_HALF_ANGLE_DEG, cone);

        if (pCommander) {
            for (auto* o : opps) {
                if (o && o->isAlive() && cone[o->pos.y * MSZ + o->pos.x]) {
                    pCommander->reportEnemySpotted(o->pos);
                    break;
                }
            }
        }
    }

    // Autonomy when commander is down 
    if (commanderDown) {
        // If out of stock  go refill at depot (if exists)
        if (medStock <= 0 && state != MedicState::GO_MED_DEPOT) {
            if (hasDepotMed) {
                targetCharacter = nullptr;
                state = MedicState::GO_MED_DEPOT;
                path.clear();
            }
        }
        // If we have stock and are free  pick closest ally (even full HP)
        if ((state == MedicState::IDLE || state == MedicState::GO_TO_PATROL) && medStock > 0) {
            Character* cand = findClosestAllyToHeal(map, safety);
            if (cand) {
                targetCharacter = cand;
                state = MedicState::GO_TO_WOUNDED;
                path.clear();
            }
        }
    }

    //  FSM 
    switch (state) {
    case MedicState::IDLE: {
        // Start/refresh patrol immediately if no specific task
        if (!hasPatrolTarget || tick >= nextPatrolSwitchTick) {
            patrolTarget = pGame->pickExplorationAnchor(teamId);
            hasPatrolTarget = true;

            nextPatrolSwitchTick = tick + PATROL_SWITCH_MIN +
                (std::rand() % std::max(1, PATROL_SWITCH_MAX - PATROL_SWITCH_MIN + 1));

            state = MedicState::GO_TO_PATROL;
        }
        break;
    }

    case MedicState::GO_TO_PATROL: {
        if (hasPatrolTarget) {
            if (path.empty()) {
                path = aStarPath(map, safety, pos, patrolTarget, LAMBDA_SAFETY);
            }
            if (!path.empty()) {
                stepAlongPath(map, pGame->getAllCharacters());
            }
            if (manhattan(pos, patrolTarget) <= 1) {
                hasPatrolTarget = false;
                state = MedicState::IDLE;
            }
        }
        else {
            state = MedicState::IDLE;
        }
        break;
    }

    case MedicState::GO_MED_DEPOT: {
        if (!hasDepotMed) {
            // No depot configured
            targetCharacter = nullptr;
            state = MedicState::IDLE;
            break;
        }

        if (path.empty()) {
            path = aStarPath(map, safety, pos, depotMed, LAMBDA_SAFETY);
        }
        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }

        // Arrived at depot
        if (manhattan(pos, depotMed) <= 1) {
            medStock = MEDIC_STOCK_MAX; // refill

            if (!commanderDown && targetCharacter && targetCharacter->isAlive()) {
                state = MedicState::GO_TO_WOUNDED; // resume ordered mission
            }
            else {
                targetCharacter = nullptr;// autonomous loop will pick a new ally
                state = MedicState::IDLE;
            }
            path.clear();
        }
        break;
    }

    case MedicState::GO_TO_WOUNDED: {
        if (targetCharacter && targetCharacter->isAlive()) {
            const Point targetPos = targetCharacter->pos;

            // Replan if target moved far from our last path end
            if (path.empty() || manhattan(path.back(), targetPos) > 2) {
                path = aStarPath(map, safety, pos, targetPos, LAMBDA_SAFETY);
            }
            if (!path.empty()) {
                stepAlongPath(map, pGame->getAllCharacters());
            }

            if (manhattan(pos, targetPos) <= 1) {
                state = MedicState::HEAL;
                path.clear();
            }
        }
        else {
            // Target invalid/dead 
            targetCharacter = nullptr;
            state = MedicState::IDLE;
        }
        break;
    }

    case MedicState::HEAL: {
        if (targetCharacter && targetCharacter->isAlive()) {
            int oldHP = targetCharacter->hp;

            targetCharacter->isBeingHealedByMedic = true;
            targetCharacter->heal(MAX_HEALTH); // counts as a "treatment" even if already full
            targetCharacter->isBeingHealedByMedic = false;

            std::cout << " Team " << teamId << " Medic HEALED ally at ("
                << targetCharacter->pos.x << "," << targetCharacter->pos.y
                << ") [HP: " << oldHP << " -> " << targetCharacter->hp << "]\n";

            if (medStock > 0) medStock--; // consume one treatment from stock
        }

        targetCharacter = nullptr;

        if (commanderDown) {
            // In autonomy: always return to depot after every treatment
            if (hasDepotMed) {
                state = MedicState::GO_MED_DEPOT;
                path.clear();
            }
            else {
                state = MedicState::IDLE;
            }
        }
        else {
            // With commander
            if (medStock <= 0 && hasDepotMed) {
                state = MedicState::GO_MED_DEPOT;
            }
            else {
                state = MedicState::IDLE;
            }
        }
        break;
    }

    } 
}
