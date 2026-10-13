#include "Warrior.h"
#include "Commander.h"
#include "Game.h"
#include "AI.h"
#include <cstdlib>

// Enemy positions to hide from: what the commander knows, or (without a commander
// or without any intel) every living enemy
static std::vector<Point> threatPositions(Character* self) {
    std::vector<Point> enemies;
    if (self->pCommander) enemies = self->pCommander->getKnownEnemyPositions();
    if (enemies.empty() && self->pGame) {
        for (const auto& ch : self->pGame->getAllCharacters())
            if (ch->teamId != self->teamId) enemies.push_back(ch->pos);
    }
    return enemies;
}

Warrior::Warrior(Point p, int team) : Character(p, 'W', team) {
    state = WarState::IDLE;
    hasPatrolTarget = false;
    hasAdvanceTarget = false;
    nextPatrolSwitchTick = 0;  
    reportedWounded = false;
    reportedLowAmmo = false;
    reachedCover = false;
    bulletCooldown = 0;
}

void Warrior::orderAdvance(Point target) {
    advanceTarget = target;
    hasAdvanceTarget = true;
    state = WarState::ADVANCE;
    path.clear();
}

void Warrior::orderAttack(Point target) {
    advanceTarget = target;
    hasAdvanceTarget = true;
    state = WarState::ATTACK;
    path.clear();
}

void Warrior::orderDefend() {
    hasAdvanceTarget = false;
    state = WarState::DEFEND;
    path.clear();
}


void Warrior::update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    if (!pGame) return;
    if (pCommander && !pCommander->isAlive()) pCommander = nullptr;

    tick++;

    //  team helpers 
    auto teamHasMedicAlive = [&]() -> bool {
        const auto& myTeam = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();
        for (auto* c : myTeam) if (c && c->isAlive() && c->icon == 'M') return true;
        return false;
        };
    auto teamHasProviderAlive = [&]() -> bool {
        const auto& myTeam = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();
        for (auto* c : myTeam) if (c && c->isAlive() && c->icon == 'P') return true;
        return false;
        };

    //  Check for death first 
    if (hp <= 0) {
        hp = 0;
        return;
    }

    //  Critical state checks 
    // wounded trigger: only if a medic is alive
    if (hp > 0 && hp <= WOUNDED_THRESHOLD && state != WarState::WOUNDED && !reportedWounded) {
        if (teamHasMedicAlive()) {
            std::cout << " Team " << teamId << " Warrior at (" << pos.x << "," << pos.y
                << ") is WOUNDED! HP=" << hp << "\n";
            state = WarState::WOUNDED;
            reportedWounded = false;
            path.clear();
        }
       
    }
    // low ammo trigger: only if a provider is alive 
    else if ((bullets <= LOW_AMMO_THRESHOLD || grenades <= 1) && !reportedLowAmmo) {
        if (pCommander) {
            pCommander->reportNeedAmmo(this); // commander can know regardless
        }
        if (teamHasProviderAlive()) {
            reportedLowAmmo = true;
            state = WarState::LOW_AMMO;   // only wait if a provider can actually resupply
            path.clear();
            hasAdvanceTarget = false;
        }
    }

    //  Enemy spotting 
    const std::vector<Character*>& opps = (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();

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

    // FSM Logic
    switch (state) {
    case WarState::IDLE: {
        // Patrol logic 
        if (!hasPatrolTarget || tick >= nextPatrolSwitchTick) {
            patrolTarget = pGame->pickExplorationAnchor(teamId);

            // Add jitter
            int dx = (std::rand() % (2 * PATROL_JITTER_RADIUS + 1)) - PATROL_JITTER_RADIUS;
            int dy = (std::rand() % (2 * PATROL_JITTER_RADIUS + 1)) - PATROL_JITTER_RADIUS;
            patrolTarget.x = std::max(0, std::min(MSZ - 1, patrolTarget.x + dx));
            patrolTarget.y = std::max(0, std::min(MSZ - 1, patrolTarget.y + dy));

            hasPatrolTarget = true;

            // Set next switch time
            nextPatrolSwitchTick = tick + PATROL_SWITCH_MIN +
                (std::rand() % std::max(1, PATROL_SWITCH_MAX - PATROL_SWITCH_MIN + 1));

            // Immediately transition to advancing
            orderAdvance(patrolTarget);
        }
        break;
    }

    case WarState::ADVANCE: {
        if (hasAdvanceTarget) {
            if (path.empty()) {
                path = aStarPath(map, safety, pos, advanceTarget, LAMBDA_SAFETY);
            }

            if (!path.empty()) {
                stepAlongPath(map, pGame->getAllCharacters());
            }
            else {
                // small random nudge if no path
                int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
                int i = rand() % 4;
                int nx = pos.x + dirs[i][0];
                int ny = pos.y + dirs[i][1];
                if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
                    pos = { nx, ny };
                }
            }

            // Reached target
            if (pos == advanceTarget) {
                state = WarState::IDLE;
                hasPatrolTarget = false;
                hasAdvanceTarget = false;
            }
        }
        else {
            state = WarState::IDLE;
        }
        break;
    }

    case WarState::ATTACK: {
        auto anyEnemyAlive = [&]() -> bool {
            for (auto* o : opps) if (o && o->isAlive()) return true;
            return false;
            };
        auto enemyAtTargetAlive = [&]() -> bool {
            for (auto* o : opps)
                if (o && o->isAlive() && o->pos.x == advanceTarget.x && o->pos.y == advanceTarget.y)
                    return true;
            return false;
            };

        if (!anyEnemyAlive()) {
            state = WarState::IDLE;
            hasAdvanceTarget = false;
            path.clear();
            break;
        }

        if (!enemyAtTargetAlive()) {
            Character* nearest = nullptr;
            int best = 1000000000;
            for (auto* o : opps) {
                if (!o || !o->isAlive()) continue;
                int d = manhattan(pos, o->pos);
                if (d < best) { best = d; nearest = o; }
            }
            if (nearest) {
                advanceTarget = nearest->pos;
                hasAdvanceTarget = true;
                path.clear();
            }
            else {
                state = WarState::IDLE;
                hasAdvanceTarget = false;
                path.clear();
                break;
            }
        }

        if (hasAdvanceTarget) {
            int dist = manhattan(pos, advanceTarget);

            if (dist > FIRE_RANGE) {
                if (path.empty()) {
                    path = aStarPath(map, safety, pos, advanceTarget, LAMBDA_SAFETY);
                }
                if (!path.empty()) {
                    stepAlongPath(map, pGame->getAllCharacters());
                }
                else {
                    state = WarState::IDLE;
                    hasAdvanceTarget = false;
                }
            }
            else {
                path.clear();
            }
        }
        else {
            state = WarState::IDLE;
        }
        break;
    }

    case WarState::DEFEND: {
        if (path.empty()) {
            std::vector<Point> enemies;

            if (pCommander) enemies = pCommander->getKnownEnemyPositions();
            else {
                for (const auto& ch : pGame->getAllCharacters())
                    if (ch->teamId != teamId) enemies.push_back(ch->pos);
            }

            Point cover;
            if (bfsFindCover(map, safety, enemies, pos,
                DEFEND_MAX_RADIUS, DEFEND_MAX_SAFETY, cover)) {
                path = aStarPath(map, safety, pos, cover, LAMBDA_SAFETY);
            }
            else {
                state = WarState::IDLE;
            }
        }
        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }
        break;
    }

    case WarState::WOUNDED: {
        // healed (e.g. by the medic while still on the way to cover): back to duty
        if (hp > WOUNDED_THRESHOLD) {
            reachedCover = false;
            hasAdvanceTarget = false;
            path.clear();
            reportedWounded = false;
            state = WarState::IDLE;
            break;
        }

        if (!teamHasMedicAlive()) {
            reachedCover = false;
            hasAdvanceTarget = false;
            path.clear();
            reportedWounded = false;
            state = WarState::IDLE;
            break;
        }

        // report once
        if (pCommander && !reportedWounded) {
            std::cout << " Team " << teamId << " Warrior reporting wounded to Commander\n";
            pCommander->reportWounded(this);
            reportedWounded = true;
        }

        // already at cover and still wounded 
        if (reachedCover && hp <= WOUNDED_THRESHOLD) {
            path.clear();
            hasAdvanceTarget = false;
            break;
        }

        // healed enough 
        if (reachedCover && hp > WOUNDED_THRESHOLD) {
            reachedCover = false;
            reportedWounded = false;
            state = WarState::IDLE;
            break;
        }

        // not at cover yet 
        if (path.empty() && !hasAdvanceTarget) {
            std::vector<Point> enemies = threatPositions(this);
            Point cover;
            if (bfsFindCover(map, safety, enemies, pos, DEFEND_MAX_RADIUS, DEFEND_MAX_SAFETY, cover)) {
                std::cout << " Team " << teamId << " Warrior seeking cover at ("
                    << cover.x << "," << cover.y << ")\n";
                path = aStarPath(map, safety, pos, cover, LAMBDA_SAFETY);
                advanceTarget = cover;
                hasAdvanceTarget = true;
            }
            else {
                break;
            }
        }

        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }

        if (hasAdvanceTarget && manhattan(pos, advanceTarget) <= 0) {
            reachedCover = true;
            hasAdvanceTarget = false;
            path.clear();
        }
        break;
    }

    case WarState::LOW_AMMO: {
        if (!teamHasProviderAlive()) {
            reportedLowAmmo = false;   // allow re-report later if provider returns
            reachedCover = false;
            hasAdvanceTarget = false;
            path.clear();
            state = WarState::IDLE;
            break;
        }

        // got resupplied
        if (bullets > LOW_AMMO_THRESHOLD && grenades > LOW_AMMO_GRENADES) {
            std::cout << "Team " << teamId << " Warrior got ammo! Returning to IDLE\n";
            reportedLowAmmo = false;
            reachedCover = false;
            state = WarState::IDLE;
            break;
        }

        // already at cover 
        if (reachedCover && (bullets <= LOW_AMMO_THRESHOLD || grenades <= LOW_AMMO_GRENADES)) {
            path.clear();
            hasAdvanceTarget = false;
            break;
        }

        // not at cover yet 
        if (path.empty() && !hasAdvanceTarget) {
            std::vector<Point> enemies = threatPositions(this);
            Point cover;
            if (bfsFindCover(map, safety, enemies, pos, DEFEND_MAX_RADIUS, DEFEND_MAX_SAFETY, cover)) {
                path = aStarPath(map, safety, pos, cover, LAMBDA_SAFETY);
                advanceTarget = cover;
                hasAdvanceTarget = true;
            }
            else {
                break; 
            }
        }

        if (!path.empty()) {
            stepAlongPath(map, pGame->getAllCharacters());
        }

        if (hasAdvanceTarget && manhattan(pos, advanceTarget) <= 0) {
            reachedCover = true;
            hasAdvanceTarget = false;
            path.clear();
        }
        break;
    }

    } 
}


bool Warrior::needsAmmo() const {
    return bullets <= LOW_AMMO_THRESHOLD; // same threshold that puts the warrior in LOW_AMMO
}

bool Warrior::needsGrenades() const {
    return grenades < DEFAULT_GRENADES;
}

void Warrior::refillAmmo() {
    bullets = DEFAULT_BULLETS;
}

void Warrior::refillGrenades() {
    grenades = DEFAULT_GRENADES;
}


