#include "Commander.h"
#include "Warrior.h"
#include "Medic.h"
#include "Provider.h"
#include "Game.h"
#include "AI.h"
#include <algorithm>

Commander::Commander(Point p, int team) : Character(p, 'C', team) {
    state = CommanderState::IDLE;
    medic = nullptr;
    provider = nullptr;
    lastEnemySpottedTick = 0;
    ticksSinceLastOrder = 0;
    hasAttackTarget = false;
    reportedWounded = false;
    combinedFOV.resize(MSZ * MSZ, 0);
    usedCovers.reserve(MAX_COVER_HISTORY);
}

bool Commander::isAloneInTeam() {
	//  check if there are any alive warriors

	// check warriors
    for (auto* w : warriors) {
        if (w && w->isAlive()) return false;  
    }

	// check medic
    if (medic && medic->isAlive()) return false;

	// check supplier
    if (provider && provider->isAlive()) return false;

	// the commander is alone
    return true;
}

bool Commander::wasAlreadyUsed(Point p) {
    for (const auto& used : usedCovers) {
        if (manhattan(used, p) <= 2) {
            return true;
        }
    }
    return false;
}

void Commander::markCoverAsUsed(Point p) {
    usedCovers.push_back(p);

    if (usedCovers.size() > MAX_COVER_HISTORY) {
        usedCovers.erase(usedCovers.begin());
    }

    coverResetCounter++;

    if (coverResetCounter > MAX_COVER_HISTORY * 2) {
        usedCovers.clear();
        coverResetCounter = 0;
        std::cout << "Team " << teamId << " Commander: Resetting cover history\n";
    }
}

bool Commander::pickNextCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    std::vector<Point> enemyPtsVisible, enemyPtsAll;
    const auto& enemies = (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();
    enemyPtsVisible.reserve(enemies.size());
    enemyPtsAll.reserve(enemies.size());

    for (auto* e : enemies) {
        if (!e || !e->isAlive()) continue;
        enemyPtsAll.push_back(e->pos);
        int idx = e->pos.y * MSZ + e->pos.x;
        if (idx >= 0 && idx < (int)combinedFOV.size() && combinedFOV[idx]) {
            enemyPtsVisible.push_back(e->pos);
        }
    }
    const auto& enemyRef = !enemyPtsVisible.empty() ? enemyPtsVisible : enemyPtsAll;

    Point outCover;
    bool foundUnused = false;

    for (int attempt = 0; attempt < 5; attempt++) {
        bool ok = bfsFindCover(map, safety, enemyRef, pos,
            CMD_COVER_MAX_RADIUS, CMD_COVER_MAX_SAFETY, outCover);

        if (!ok || (outCover.x == pos.x && outCover.y == pos.y)) {
            continue;
        }

        if (!wasAlreadyUsed(outCover)) {
            foundUnused = true;
            break;
        }
    }

    if (!foundUnused) {
        Point best = pos;
        double bestVal = safety[pos.y][pos.x];
        for (int dy = -12; dy <= 12; ++dy) {
            for (int dx = -12; dx <= 12; ++dx) {
                int nx = pos.x + dx, ny = pos.y + dy;
                if (!inBounds(nx, ny) || !passableForAgent(map[ny][nx])) continue;

                Point candidate = { nx, ny };
                if (wasAlreadyUsed(candidate)) continue;

                double s = safety[ny][nx];
                if (s < bestVal) {
                    bestVal = s;
                    best = candidate;
                }
            }
        }

        if (best.x == pos.x && best.y == pos.y) {
            usedCovers.clear();
            coverResetCounter = 0;
            return false;
        }
        outCover = best;
    }

    auto newPath = aStarPath(map, safety, pos, outCover, LAMBDA_SAFETY);
    if (newPath.empty()) return false;

    coverTarget = outCover;
    coverPath.swap(newPath);
    hasCoverTarget = true;
    markCoverAsUsed(outCover);

    int dwell = CMD_COVER_DWELL_MIN +
        (std::rand() % std::max(1, CMD_COVER_DWELL_MAX - CMD_COVER_DWELL_MIN + 1));
    coverWaitUntil = tick + dwell;

    return true;
}

// Picks nearest cover considering safety, distance, and usage history
bool Commander::pickNearestCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    Point best = pos;
    double bestScore = 1e18;

    for (int dy = -8; dy <= 8; ++dy) {
        for (int dx = -8; dx <= 8; ++dx) {
            int nx = pos.x + dx;
            int ny = pos.y + dy;
            if (!inBounds(nx, ny) || !passableForAgent(map[ny][nx])) continue;

            Point candidate = { nx, ny };
            double usedPenalty = wasAlreadyUsed(candidate) ? 100.0 : 0.0;

            double s = safety[ny][nx];
            int dist = std::abs(dx) + std::abs(dy);

            double score = s + 0.3 * dist + usedPenalty;

            if (score < bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
    }

    if (best.x == pos.x && best.y == pos.y) return false;

    coverTarget = best;
    coverPath = aStarPath(map, safety, pos, best, LAMBDA_SAFETY);
    hasCoverTarget = !coverPath.empty();
    coverWaitUntil = tick + 30;

    if (hasCoverTarget) {
        markCoverAsUsed(best);
    }

    return hasCoverTarget;
}

// Picks best cover for retreat considering safety, distance from enemies, and usage history
bool Commander::pickBestRetreatCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    Point best = pos;
    double bestScore = -1e18;

    const auto& enemies = (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();
    std::vector<Point> enemyPts;
    for (auto* e : enemies) {
        if (e && e->isAlive()) enemyPts.push_back(e->pos);
    }

    for (int dy = -20; dy <= 20; ++dy) {
        for (int dx = -20; dx <= 20; ++dx) {
            int nx = pos.x + dx;
            int ny = pos.y + dy;
            if (!inBounds(nx, ny) || !passableForAgent(map[ny][nx])) continue;

            Point candidate = { nx, ny };
            double usedPenalty = wasAlreadyUsed(candidate) ? -200.0 : 50.0;

            double s = safety[ny][nx];

            int minEnemyDist = 1e9;
            for (const auto& ep : enemyPts) {
                minEnemyDist = std::min(minEnemyDist,
                    std::abs(ep.x - nx) + std::abs(ep.y - ny));
            }

            double score = minEnemyDist * 2.0 - s * 50.0 + usedPenalty;

            if (score > bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
    }

    if (best.x == pos.x && best.y == pos.y) return false;

    coverTarget = best;
    coverPath = aStarPath(map, safety, pos, best, LAMBDA_SAFETY * 1.5);
    hasCoverTarget = !coverPath.empty();
    coverWaitUntil = tick + 10;

    if (hasCoverTarget) {
        markCoverAsUsed(best);
    }

    return hasCoverTarget;
}

void Commander::reportEnemySpotted(Point pos) {
    bool found = false;
    for (size_t i = 0; i < knownEnemyPositions.size(); i++) {
        if (knownEnemyPositions[i] == pos) {
            knownEnemyTicks[i] = tick;   // refresh
            found = true;
            break;
        }
    }

    if (!found) {
        knownEnemyPositions.push_back(pos);
        knownEnemyTicks.push_back(tick);
    }

    lastEnemySpottedTick = tick;
}

// Assigns medic to wounded character if available
void Commander::reportWounded(Character* c) {
    if (medic && medic->isAlive() && medic->isIdle()) {
        std::cout << "Team " << teamId << " Commander assigning Medic to wounded "
            << c->icon << " at (" << c->pos.x << "," << c->pos.y << ")\n";
        medic->assignTask(c);
    }
    else {
        if (!medic) {
            std::cout << " Team " << teamId << " Commander: No medic available!\n";
        }
        else if (!medic->isAlive()) {
            std::cout << " Team " << teamId << " Commander: Medic is dead!\n";
        }
        else {
            std::cout << " Team " << teamId << " Commander: Medic is busy (state="
                << (int)medic->state << ")\n";
        }
    }
}

void Commander::reportNeedAmmo(Warrior* w) {
    if (provider && provider->isAlive() && provider->isIdle()) {
        provider->assignTask(w);
    }
}

void Commander::updateCombinedFOV(int map[MSZ][MSZ]) {
    combinedFOV.assign(MSZ * MSZ, 0);

    std::vector<unsigned char> myFOV;
    Point myDir = { pos.x - prevPos.x, pos.y - prevPos.y };   // look where we move
    if (myDir.x == 0 && myDir.y == 0) myDir = { teamId == 0 ? 1 : -1, 0 };
    computeFOVCone(map, pos, (float)myDir.x, (float)myDir.y,
        FOV_RADIUS_COMMANDER, FOV_HALF_ANGLE_DEG, myFOV);

    for (int i = 0; i < MSZ * MSZ; i++) {
        if (myFOV[i]) combinedFOV[i] = 1;
    }

    for (auto* w : warriors) {
        if (!w->isAlive()) continue;

        Point dir = { w->pos.x - w->prevPos.x, w->pos.y - w->prevPos.y };
        if (dir.x == 0 && dir.y == 0) dir = { 1, 0 };

        std::vector<unsigned char> wFOV;
        computeFOVCone(map, w->pos, (float)dir.x, (float)dir.y,
            FOV_RADIUS_WARRIOR, FOV_HALF_ANGLE_DEG, wFOV);

        for (int i = 0; i < MSZ * MSZ; i++) {
            if (wFOV[i]) combinedFOV[i] = 1;
        }
    }

    if (medic && medic->isAlive()) {
        Point dir = { medic->pos.x - medic->prevPos.x, medic->pos.y - medic->prevPos.y };
        if (dir.x == 0 && dir.y == 0) dir = { 1, 0 };

        std::vector<unsigned char> mFOV;
        computeFOVCone(map, medic->pos, (float)dir.x, (float)dir.y,
            FOV_RADIUS_WARRIOR, FOV_HALF_ANGLE_DEG, mFOV);

        for (int i = 0; i < MSZ * MSZ; i++) {
            if (mFOV[i]) combinedFOV[i] = 1;
        }
    }

    if (provider && provider->isAlive()) {
        Point dir = { provider->pos.x - provider->prevPos.x,
                     provider->pos.y - provider->prevPos.y };
        if (dir.x == 0 && dir.y == 0) dir = { 1, 0 };

        std::vector<unsigned char> pFOV;
        computeFOVCone(map, provider->pos, (float)dir.x, (float)dir.y,
            FOV_RADIUS_WARRIOR, FOV_HALF_ANGLE_DEG, pFOV);

        for (int i = 0; i < MSZ * MSZ; i++) {
            if (pFOV[i]) combinedFOV[i] = 1;
        }
    }
}

int Commander::countAliveWarriors() {
    int count = 0;
    for (auto* w : warriors) {
        if (w->isAlive()) count++;
    }
    return count;
}

int Commander::countAliveEnemies() {
    if (!pGame) return 0;

    const std::vector<Character*>& enemies = (teamId == 0) ?
        pGame->getTeam1() : pGame->getTeam0();

    int count = 0;
    for (auto* e : enemies) {
        if (e->isAlive()) count++;
    }
    return count;
}

Point Commander::findBestAttackPosition() {
    if (knownEnemyPositions.empty()) {
        Point forward = pos;
        forward.x += (teamId == 0) ? 20 : -20;
        forward.x = std::max(0, std::min(MSZ - 1, forward.x));
        return forward;
    }

    int sumX = 0, sumY = 0;
    for (const auto& p : knownEnemyPositions) {
        sumX += p.x;
        sumY += p.y;
    }

    Point target;
    target.x = sumX / knownEnemyPositions.size();
    target.y = sumY / knownEnemyPositions.size();

    int dx = (teamId == 0) ? -5 : 5;
    target.x += dx;
    target.x = std::max(0, std::min(MSZ - 1, target.x));

    return target;
}


// Makes strategic decision based on current state and battlefield conditions
void Commander::makeStrategicDecision(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    int aliveWarriors = countAliveWarriors();
    int aliveEnemies = countAliveEnemies();

    if (aliveWarriors == 0) {
        state = CommanderState::IDLE;
        return;
    }

    bool recentEnemyContact = (tick - lastEnemySpottedTick) < SEEN_ENEMIES;
    bool outnumbered = aliveWarriors < aliveEnemies;
    bool hasAdvantage = aliveWarriors > aliveEnemies;

    if (state == CommanderState::ATTACKING) {
        if (outnumbered && recentEnemyContact) {
            state = CommanderState::DEFENDING;

            for (auto* w : warriors) {
                if (w->isAlive()) {
                    w->orderDefend();
                }
            }

            ticksSinceLastOrder = 0;
        }
    }
    else if (state == CommanderState::DEFENDING) {
        if (hasAdvantage || !recentEnemyContact) {
            state = CommanderState::ATTACKING;

            attackTarget = findBestAttackPosition();
            hasAttackTarget = true;

            for (auto* w : warriors) {
                if (w->isAlive()) {
                    w->orderAttack(attackTarget);
                }
            }

            ticksSinceLastOrder = 0;
        }
    }
    else {
        if (recentEnemyContact) {
            if (outnumbered) {
                state = CommanderState::DEFENDING;

                for (auto* w : warriors) {
                    if (w->isAlive()) {
                        w->orderDefend();
                    }
                }
            }
            else {
                state = CommanderState::ATTACKING;

                attackTarget = findBestAttackPosition();
                hasAttackTarget = true;

                for (auto* w : warriors) {
                    if (w->isAlive()) {
                        w->orderAttack(attackTarget);
                    }
                }
            }

            ticksSinceLastOrder = 0;
        }
        else {
            state = CommanderState::IDLE;
        }
    }
}

// Determines if there is an immediate threat from enemies
bool Commander::isImmediateThreat(int map[MSZ][MSZ],
    const std::vector<Character*>& enemies) const
{
    for (auto* e : enemies) {
        if (!e || !e->isAlive()) continue;

        int dist = manhattan(pos, e->pos);

        if (dist <= 4) return true;

        if (dist <= 12 && hasLOF(map, pos, e->pos)) return true;

        int idx = e->pos.y * MSZ + e->pos.x;
        if (idx >= 0 && idx < (int)combinedFOV.size() && combinedFOV[idx]) {
            if (dist <= 14) return true;
        }
    }
    return false;
}


void Commander::update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    if (!isAlive() || !pGame) return;

    tick++;
    ticksSinceLastOrder++;

    // forget old sightings, so attack targets follow where enemies are now
    for (size_t i = 0; i < knownEnemyPositions.size(); ) {
        if (tick - knownEnemyTicks[i] > ENEMY_INTEL_MAX_AGE) {
            knownEnemyPositions.erase(knownEnemyPositions.begin() + i);
            knownEnemyTicks.erase(knownEnemyTicks.begin() + i);
        }
        else ++i;
    }

    // Move only every N ticks
    bool canMove = (tick % MOVE_EVERY_N_TICKS == 0);

    //  is there a living medic on my team
    auto teamHasMedicAlive = [&]() -> bool {
        const auto& myTeam = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();
        for (auto* c : myTeam) if (c && c->isAlive() && c->icon == 'M') return true;
        return false;
        };

    //  If commander is alone - surrender 
    if (isAloneInTeam() && state != CommanderState::SURRENDERED) {
        state = CommanderState::SURRENDERED;
        hasCoverTarget = false;
        coverPath.clear();
        std::cout << "Team " << teamId << " Commander SURRENDERS! All team members dead!\n";
        return;
    }
    if (state == CommanderState::SURRENDERED) {
        return;
    }

    // Wounded trigger (gate by medic presence) 
    if (hp <= WOUNDED_THRESHOLD) {
        if (state != CommanderState::WOUNDED && teamHasMedicAlive()) {
            std::cout << "Team " << teamId << " Commander at (" << pos.x << "," << pos.y
                << ") is WOUNDED! HP=" << hp << "\n";
            reportWounded(this);        // ask medic to come
            reportedWounded = true;
            state = CommanderState::WOUNDED;
            hasCoverTarget = false;     
            coverPath.clear();
        }
        
    }
    else {
        // healed above threshold
        reportedWounded = false;
        if (state == CommanderState::WOUNDED) {
            state = CommanderState::IDLE;
            hasCoverTarget = false;
            coverPath.clear();
        }
    }

    //  Combined FOV 
    updateCombinedFOV(map);

    if (tick % 10 == 0 && medic && medic->isAlive() && medic->isIdle()) {
        const std::vector<Character*>& team = (teamId == 0) ? pGame->getTeam0() : pGame->getTeam1();
        for (auto* member : team) {
            if (member->isAlive() && member->hp <= WOUNDED_THRESHOLD) {
                reportWounded(member);
                break;
            }
        }
    }

    if (state == CommanderState::WOUNDED) {
        // If medic died meanwhile -> stop waiting; resume normal ops
        if (!teamHasMedicAlive()) {
            reportedWounded = false;
            hasCoverTarget = false;
            coverPath.clear();
            state = CommanderState::IDLE;
            
        }
        else {
           
            if (!hasCoverTarget) {
                pickNearestCover(map, safety); 
            }

            if (hasCoverTarget) {
                // Move to cover 
                if (manhattan(pos, coverTarget) <= 1) {
                    return;
                }

                if (canMove) {
                    if (coverPath.empty()) {
                        if (tick - coverReplanTick >= CMD_COVER_REPLAN_COOLDOWN) {
                            coverReplanTick = tick;
                            auto replanned = aStarPath(map, safety, pos, coverTarget, LAMBDA_SAFETY);
                            if (!replanned.empty()) coverPath.swap(replanned);
                        }
                    }

                    if (!coverPath.empty()) {
                        Point next = coverPath.front();

                        bool blocked = false;
                        for (auto* c : pGame->getAllCharacters()) {
                            if (c != this && c->isAlive() && c->pos == next) { blocked = true; break; }
                        }

                        if (!blocked) {
                            prevPos = pos;
                            pos = next;
                            coverPath.erase(coverPath.begin());
                        }
                        else {
                            coverPath.clear();
                        }
                    }
                    else {
                        static const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
                        int k = std::rand() % 4;
                        int nx = pos.x + dirs[k][0];
                        int ny = pos.y + dirs[k][1];
                        if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
                            bool occ = false;
                            for (auto* c : pGame->getAllCharacters()) {
                                if (c != this && c->isAlive() && c->pos.x == nx && c->pos.y == ny) { occ = true; break; }
                            }
                            if (!occ) {
                                prevPos = pos;
                                pos = { nx, ny };
                            }
                        }
                    }
                }
                return; 
            }
           
        }
    }

    //  Strategic decisions 
    if (ticksSinceLastOrder > 25) {
        makeStrategicDecision(map, safety);
    }

    //  Immediate danger handling 
    const std::vector<Character*>& enemies = (teamId == 0) ? pGame->getTeam1() : pGame->getTeam0();
    bool danger = isImmediateThreat(map, enemies);
    int aliveWarriors = countAliveWarriors();

    if (danger) {
        if (aliveWarriors > 0) {
            if (state != CommanderState::DEFENDING) {
                state = CommanderState::DEFENDING;
                std::cout << "Team " << teamId << " Commander UNDER ATTACK! Sending warriors to engage!\n";

                Point closestEnemy = pos;
                int minDist = 1e9;
                for (auto* e : enemies) {
                    if (e && e->isAlive()) {
                        int dist = manhattan(pos, e->pos);
                        if (dist < minDist) { minDist = dist; closestEnemy = e->pos; }
                    }
                }
                for (auto* w : warriors) if (w->isAlive()) w->orderAttack(closestEnemy);
                ticksSinceLastOrder = 0;
            }
            if (!hasCoverTarget || manhattan(pos, coverTarget) > 12) {
                pickNearestCover(map, safety);
            }
        }
        else {
            if (state != CommanderState::IDLE) {
                std::cout << "Team " << teamId << " Commander RETREATING! No warriors to defend!\n";
                state = CommanderState::IDLE;
            }
            if (!hasCoverTarget || manhattan(pos, coverTarget) < 15) {
                pickBestRetreatCover(map, safety);
            }
        }
    }
    else {
        if (!hasCoverTarget) {
            pickNextCover(map, safety);
        }
    }

    //  Cover movement 
    if (hasCoverTarget) {
        if (manhattan(pos, coverTarget) <= 1) {
            if (tick < coverWaitUntil) return;

            hasCoverTarget = false;
            coverPath.clear();

            if (danger) {
                if (aliveWarriors > 0) pickNearestCover(map, safety);
                else pickBestRetreatCover(map, safety);
            }
            else {
                pickNextCover(map, safety);
            }
            return;
        }

        if (canMove) {
            if (coverPath.empty()) {
                if (tick - coverReplanTick >= CMD_COVER_REPLAN_COOLDOWN) {
                    coverReplanTick = tick;
                    auto replanned = aStarPath(map, safety, pos, coverTarget, LAMBDA_SAFETY);
                    if (!replanned.empty()) coverPath.swap(replanned);
                }
            }

            if (!coverPath.empty()) {
                Point next = coverPath.front();

                bool blocked = false;
                for (auto* c : pGame->getAllCharacters()) {
                    if (c != this && c->isAlive() && c->pos == next) { blocked = true; break; }
                }

                if (!blocked) {
                    prevPos = pos;
                    pos = next;
                    coverPath.erase(coverPath.begin());
                }
                else {
                    coverPath.clear();
                }
            }
            else {
                static const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
                int k = std::rand() % 4;
                int nx = pos.x + dirs[k][0];
                int ny = pos.y + dirs[k][1];
                if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
                    bool occ = false;
                    for (auto* c : pGame->getAllCharacters()) {
                        if (c != this && c->isAlive() && c->pos.x == nx && c->pos.y == ny) { occ = true; break; }
                    }
                    if (!occ) {
                        prevPos = pos;
                        pos = { nx, ny };
                    }
                }
            }
        }
        return;
    }

    //  Small random walk when idle 
    if (canMove) {
        static const int dirs[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
        int k = std::rand() % 4;
        int nx = pos.x + dirs[k][0];
        int ny = pos.y + dirs[k][1];
        if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
            bool occ = false;
            for (auto* c : pGame->getAllCharacters()) {
                if (c != this && c->isAlive() && c->pos.x == nx && c->pos.y == ny) { occ = true; break; }
            }
            if (!occ) {
                prevPos = pos;
                pos = { nx, ny };
            }
        }
    }
}
