#include "Provider.h"
#include "Warrior.h"
#include "Commander.h"
#include "Game.h"
#include "AI.h"
#include <cstdlib>

Provider::Provider(Point p, int team) : Character(p, 'P', team) {
    state = ProvState::IDLE;
    targetWarrior = nullptr;
    hasDepotAmmo = false;
    hasPatrolTarget = false;
    nextPatrolSwitchTick = 0;  
    reportedWounded = false;
}

void Provider::assignTask(Warrior* target) {
    
    if ((state == ProvState::IDLE || state == ProvState::GO_TO_PATROL) && target) {
        targetWarrior = target;
        state = ProvState::GO_AMMO_DEPOT;
        path.clear();
    }
}

bool Provider::isIdle() const {
    return state == ProvState::IDLE || state == ProvState::GO_TO_PATROL;
}

void Provider::update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    if (!isAlive() || !pGame) return;
    if (pCommander && !pCommander->isAlive()) pCommander = nullptr;

    tick++;

    //  check if theres a living medic on my team 
    auto teamHasMedicAlive = [&]() -> bool {
        const auto& myTeam = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();
        for (auto* c : myTeam) if (c && c->isAlive() && c->icon == 'M') return true;
        return false;
        };

    //Check if wounded 
    if (hp <= WOUNDED_THRESHOLD) {
        if (!reportedWounded && teamHasMedicAlive()) {
            std::cout << "  Team " << teamId << " Provider at (" << pos.x << "," << pos.y
                << ") is WOUNDED! HP=" << hp << "\n";
            if (pCommander) pCommander->reportWounded(this);
            reportedWounded = true;
            state = ProvState::WOUNDED;   // go find cover & wait
            path.clear();
            hasPatrolTarget = false;
        }
        
    }
    else {
        // healed above threshold 
        reportedWounded = false;
        if (state == ProvState::WOUNDED) {
            state = ProvState::IDLE;
            path.clear();
            hasPatrolTarget = false;
        }
    }

    //  Enemy spotting 
    const std::vector<Character*>& opps =
        (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();

    Point moveDir = { pos.x - prevPos.x, pos.y - prevPos.y };
    if (moveDir.x == 0 && moveDir.y == 0) moveDir = { 1, 0 };

    std::vector<unsigned char> cone;
    computeFOVCone(map, pos, (float)moveDir.x, (float)moveDir.y,
        FOV_RADIUS_WARRIOR, FOV_HALF_ANGLE_DEG, cone);

    for (auto* o : opps) {
        if (o->isAlive() && cone[o->pos.y * MSZ + o->pos.x]) {
            if (pCommander) pCommander->reportEnemySpotted(o->pos);
            break;
        }
    }

    //  FSM 
    switch (state) {
    case ProvState::IDLE: {
        // opportunistic: auto-pick a warrior who needs ammo
        if (Warrior* next = pGame->findNearestWarriorNeedingAmmo(this)) {
            assignTask(next);
            break;
        }

        // Patrol
        if (!hasPatrolTarget || tick >= nextPatrolSwitchTick) {
            patrolTarget = pGame->pickExplorationAnchor(teamId);
            hasPatrolTarget = true;

            nextPatrolSwitchTick = tick + PATROL_SWITCH_MIN +
                (std::rand() % std::max(1, PATROL_SWITCH_MAX - PATROL_SWITCH_MIN + 1));

            state = ProvState::GO_TO_PATROL;
        }
        break;
    }

    case ProvState::GO_TO_PATROL: {
        if (hasPatrolTarget) {
            if (path.empty()) {
                path = aStarPath(map, safety, pos, patrolTarget, LAMBDA_SAFETY);
            }
            if (!path.empty()) {
                stepAlongPath(map, pGame->getAllCharacters());
            }
            if (manhattan(pos, patrolTarget) <= 1) {
                hasPatrolTarget = false;
                state = ProvState::IDLE;
            }
        }
        else {
            state = ProvState::IDLE;
        }
        break;
    }

    case ProvState::GO_AMMO_DEPOT: {
        if (!hasDepotAmmo) {
            state = ProvState::IDLE;
            targetWarrior = nullptr;
            break;
        }
        if (path.empty()) {
            path = aStarPath(map, safety, pos, depotAmmo, LAMBDA_SAFETY);
        }
        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }
        if (manhattan(pos, depotAmmo) <= 1) {
            state = ProvState::GO_TO_WARRIOR;
            path.clear();
        }
        break;
    }

    case ProvState::GO_TO_WARRIOR: {
        if (targetWarrior && targetWarrior->isAlive()) {
            Point targetPos = targetWarrior->pos;

            if (path.empty() || manhattan(path.back(), targetPos) > 2) {
                path = aStarPath(map, safety, pos, targetPos, LAMBDA_SAFETY);
            }
            if (!path.empty()) {
                stepAlongPath(map, pGame->getAllCharacters());
            }
            if (manhattan(pos, targetPos) <= 1) {
                state = ProvState::SUPPLY;
                path.clear();
            }
        }
        else {
            state = ProvState::IDLE;
            targetWarrior = nullptr;
        }
        break;
    }

    case ProvState::SUPPLY: {
        if (targetWarrior && targetWarrior->isAlive()) {
            bool gaveSomething = false;

            if (targetWarrior->needsAmmo()) {
                targetWarrior->refillAmmo();
                gaveSomething = true;
                std::cout << " Provider gave bullets to warrior!\n";
            }
            if (targetWarrior->needsGrenades()) {
                targetWarrior->refillGrenades();
                gaveSomething = true;
                std::cout << " Provider gave grenades to warrior!\n";
            }
            if (gaveSomething) {
                targetWarrior->reportedLowAmmo = false;
            }
            targetWarrior = nullptr;
            state = ProvState::IDLE;
        }
        else {
            targetWarrior = nullptr;
            state = ProvState::IDLE;
        }

        if (state == ProvState::IDLE) {
            Warrior* next = pGame->findNearestWarriorNeedingAmmo(this);
            if (next) assignTask(next);
            else state = ProvState::GO_TO_PATROL;
        }
        break;
    }

    //  wounded flow (cover & wait, only if a medic exists) 
    case ProvState::WOUNDED: {
        // If medic died meanwhile 
        if (!teamHasMedicAlive()) {
            reportedWounded = false;
            path.clear();
            hasPatrolTarget = false;
            state = ProvState::IDLE;
            break;
        }

        // Healed enough 
        if (hp > WOUNDED_THRESHOLD) {
            reportedWounded = false;
            path.clear();
            hasPatrolTarget = false;
            state = ProvState::IDLE;
            break;
        }

        // Not healed yet 
        if (path.empty()) {
            std::vector<Point> enemies;   // hide from the enemies the team knows about
            if (pCommander) enemies = pCommander->getKnownEnemyPositions();
            if (enemies.empty())
                for (const auto& ch : pGame->getAllCharacters())
                    if (ch->teamId != teamId) enemies.push_back(ch->pos);
            Point cover;
            if (bfsFindCover(map, safety, enemies, pos, DEFEND_MAX_RADIUS, DEFEND_MAX_SAFETY, cover)) {
                path = aStarPath(map, safety, pos, cover, LAMBDA_SAFETY);
            }
            else {
                break;
            }
        }

        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }

        // Once reached cover
        if (!path.empty() && pos == path.back()) {
            path.clear(); // stop moving once we're basically there
        }
        break;
    }
    } 
}

