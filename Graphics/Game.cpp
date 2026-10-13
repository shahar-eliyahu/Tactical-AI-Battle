#include "Game.h"
#include "AI.h"
#include "Graphics.h"
#include <cstdlib>
#include <ctime>
#include <iostream>


static void setDepotBlock(int map[MSZ][MSZ], Point topLeft, int type) {
    for (int dy = 0; dy < DEPOT_SIZE; ++dy) {
        for (int dx = 0; dx < DEPOT_SIZE; ++dx) {
            int x = topLeft.x + dx;
            int y = topLeft.y + dy;
            if (!inBounds(x, y)) continue;
            map[y][x] = type;
        }
    }
}

// A team is still fighting while it has a living member that has not surrendered
static int countActive(const std::vector<Character*>& team) {
    int n = 0;
    for (auto* c : team) {
        if (!c || !c->isAlive()) continue;
        if (c->icon == 'C' && static_cast<Commander*>(c)->state == CommanderState::SURRENDERED) continue;
        n++;
    }
    return n;
}

// A team can still fight while it has a warrior who has ammo, or a living provider
// to bring ammo. Medics, providers and commanders cannot shoot, so a team without
// such a warrior can no longer win and the battle is decided.
static bool canStillFight(const std::vector<Character*>& team) {
    bool providerAlive = false;
    for (auto* c : team)
        if (c && c->isAlive() && c->icon == 'P') providerAlive = true;
    for (auto* c : team) {
        if (!c || !c->isAlive() || c->icon != 'W') continue;
        if (c->bullets > 0 || c->grenades > 0 || providerAlive) return true;
    }
    return false;
}

Game::Game() {
    // random seed is set once in main(), so every new battle gets a new map

    gameTick = 0;
    gameOver = false;
    winningTeam = -1;
    viewMode = ViewMode::Game;

    commander0 = nullptr;
    commander1 = nullptr;
    selectedCommander = nullptr; 

    // Initialize dummy grenade spawn timer
    nextDummyGrenadeSpawn = DUMMY_GRENADE_SPAWN_MIN +
        (std::rand() % (DUMMY_GRENADE_SPAWN_MAX - DUMMY_GRENADE_SPAWN_MIN));
}


Game::~Game() {
    for (auto* c : allCharacters) {
        delete c;
    }

    for (auto* g : grenades) {
        delete g;
    }

    for (auto* b : bullets) {      
        delete b;
    }
}


void Game::init() {
    initMap();
    initDepots();
    initTeams();

    selectedCommander = commander0;

    std::cout << "Game initialized!\n";
    std::cout << "Team 0 (Blue): " << team0.size() << " units\n";
    std::cout << "Team 1 (Orange): " << team1.size() << " units\n";
}

void Game::initMap() {
    // Clear map
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            map[i][j] = EMPTY;
            safety[i][j] = 0.0;
        }
    }

    // Add stones (rocks)
    for (int i = 0; i < NUMBER_OF_ROCKS; i++) {
        int x = std::rand() % MSZ;
        int y = std::rand() % MSZ;
        int w = 2 + std::rand() % 2;
        int h = 2 + std::rand() % 2;

        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (inBounds(nx, ny)) {
                    map[ny][nx] = STONE;
                }
            }
        }
    }

    // Add trees
    for (int i = 0; i < NUMBER_OF_TREES; i++) {
        int x = std::rand() % MSZ;
        int y = std::rand() % MSZ;

        if (map[y][x] == EMPTY) {
            map[y][x] = TREE;
        }
    }

    // Add water
    for (int i = 0; i < NUMBER_OF_WATER; i++) {
        int x = std::rand() % MSZ;
        int y = std::rand() % MSZ;
        int w = 2 + std::rand() % 3;
        int h = 2 + std::rand() % 3;

        for (int dy = 0; dy < h; dy++) {
            for (int dx = 0; dx < w; dx++) {
                int nx = x + dx;
                int ny = y + dy;
                if (inBounds(nx, ny) && map[ny][nx] == EMPTY) {
                    map[ny][nx] = WATER;
                }
            }
        }
    }
}

void Game::initDepots() {
    // Team 0 depots 
    depotAmmo0 = { SPAWN_MARGIN, MSZ / 2 - 5 };
    depotMed0 = { SPAWN_MARGIN, MSZ / 2 + 5 };

    // Team 1 depots 
    depotAmmo1 = { MSZ - SPAWN_MARGIN - 1, MSZ / 2 - 5 };
    depotMed1 = { MSZ - SPAWN_MARGIN - 1, MSZ / 2 + 5 };

    setDepotBlock(map, depotAmmo0, AMMO_DEPOT);
    setDepotBlock(map, depotMed0, MED_DEPOT);
    setDepotBlock(map, depotAmmo1, AMMO_DEPOT);
    setDepotBlock(map, depotMed1, MED_DEPOT);
}



void Game::initTeams() {
   
    Point cmdPos0 = { SPAWN_MARGIN + 5, MSZ / 2 };
    commander0 = new Commander(cmdPos0, 0);
    team0.push_back(commander0);

    Warrior* w0_1 = new Warrior({ SPAWN_MARGIN + 8, MSZ / 2 - 5 }, 0);
    Warrior* w0_2 = new Warrior({ SPAWN_MARGIN + 8, MSZ / 2 + 5 }, 0);
    team0.push_back(w0_1);
    team0.push_back(w0_2);
    commander0->warriors.push_back(w0_1);
    commander0->warriors.push_back(w0_2);

    Medic* m0 = new Medic({ SPAWN_MARGIN + 3, MSZ / 2 }, 0);
    m0->depotMed = depotMed0;
    m0->hasDepotMed = true;
    team0.push_back(m0);
    commander0->medic = m0;

    Provider* p0 = new Provider({ SPAWN_MARGIN + 3, MSZ / 2 + 3 }, 0);
    p0->depotAmmo = depotAmmo0;
    p0->hasDepotAmmo = true;
    team0.push_back(p0);
    commander0->provider = p0;

    // Team 1 Orange 
    Point cmdPos1 = { MSZ - SPAWN_MARGIN - 5, MSZ / 2 };
    commander1 = new Commander(cmdPos1, 1);
    team1.push_back(commander1);

    Warrior* w1_1 = new Warrior({ MSZ - SPAWN_MARGIN - 8, MSZ / 2 - 5 }, 1);
    Warrior* w1_2 = new Warrior({ MSZ - SPAWN_MARGIN - 8, MSZ / 2 + 5 }, 1);
    team1.push_back(w1_1);
    team1.push_back(w1_2);
    commander1->warriors.push_back(w1_1);
    commander1->warriors.push_back(w1_2);

    Medic* m1 = new Medic({ MSZ - SPAWN_MARGIN - 3, MSZ / 2 }, 1);
    m1->depotMed = depotMed1;
    m1->hasDepotMed = true;
    team1.push_back(m1);
    commander1->medic = m1;

    Provider* p1 = new Provider({ MSZ - SPAWN_MARGIN - 3, MSZ / 2 + 3 }, 1);
    p1->depotAmmo = depotAmmo1;
    p1->hasDepotAmmo = true;
    team1.push_back(p1);
    commander1->provider = p1;

    // Combine all characters
    allCharacters.insert(allCharacters.end(), team0.begin(), team0.end());
    allCharacters.insert(allCharacters.end(), team1.begin(), team1.end());

    // Set references
    for (auto* c : team0) {
        c->pGame = this;
        c->pCommander = commander0;
    }

    for (auto* c : team1) {
        c->pGame = this;
        c->pCommander = commander1;
    }
}

void Game::updateSafetyMap() {
    //  Decay existing danger
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            safety[i][j] *= SAFETY_DECAY;
            if (safety[i][j] < 0.001)
                safety[i][j] = 0.0;
        }
    }

    //  Danger from dummy grenades
    for (const auto& dg : dummyGrenades) {
        addSafetyThreatAtPosition(dg.pos, dg.dangerRadius, DUMMY_GRENADE_INTENSITY);
    }

    for (auto* b : bullets) {
        if (!b || !b->isMoving) continue;

        Point p{ (int)b->x, (int)b->y };
        addSafetyThreatAtPosition(p, BULLET_DANGER_RADIUS, BULLET_DANGER_INTENSITY);
    }
}




void Game::addSafetyThreatAtPosition(Point pos, float radius, float intensity) {
    int cx = pos.x;
    int cy = pos.y;
    int r = (int)radius;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;

            if (!inBounds(nx, ny)) continue;

            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) continue;

            // Higher danger closer to threat
            double danger = ((radius - dist) / radius) * intensity;

            // Add danger to cells visible from threat
            if (hasLOS(map, pos, { nx, ny })) {
                safety[ny][nx] += danger;
            }
        }
    }
}

void Game::spawnDummyGrenade() {
    // Random position on map
    Point pos;
    pos.x = 10 + std::rand() % (MSZ - 20);  // Avoid edges
    pos.y = 10 + std::rand() % (MSZ - 20);

    // Make sure it's on passable terrain
    if (!passableForAgent(map[pos.y][pos.x])) {
        return;  // Try again next time
    }

    DummyGrenade dg;
    dg.pos = pos;
    dg.ticksRemaining = DUMMY_GRENADE_LIFETIME;
    dg.dangerRadius = DUMMY_GRENADE_DANGER_RADIUS;

    dummyGrenades.push_back(dg);

    std::cout << "Dummy grenade spawned at (" << pos.x << "," << pos.y << ")\n";
}

void Game::updateDummyGrenades() {
    // Update existing dummy grenades
    for (auto it = dummyGrenades.begin(); it != dummyGrenades.end(); ) {
        it->ticksRemaining--;

        if (it->ticksRemaining <= 0) {
            std::cout << "Dummy grenade expired at (" << it->pos.x << "," << it->pos.y << ")\n";
            it = dummyGrenades.erase(it);
        }
        else {
            ++it;
        }
    }

    // Spawn new dummy grenade if time
    nextDummyGrenadeSpawn--;
    if (nextDummyGrenadeSpawn <= 0) {
        spawnDummyGrenade();

        // Schedule next spawn
        nextDummyGrenadeSpawn = DUMMY_GRENADE_SPAWN_MIN +
            (std::rand() % (DUMMY_GRENADE_SPAWN_MAX - DUMMY_GRENADE_SPAWN_MIN + 1));
    }
}


void Game::doCombat() {
    // Check each warrior for combat opportunities
    for (auto* attacker : allCharacters) {
        if (!attacker->isAlive()) continue;
        if (attacker->icon != 'W') continue;
        Warrior* w = static_cast<Warrior*>(attacker);

        const std::vector<Character*>& enemies = (w->teamId == 0) ? team1 : team0;

        //  First check grenade throwing 
        if (w->grenades > 0 && w->grenadeCooldown <= 0) {
            Point bestTarget = { -1, -1 };
            int maxEnemies = 0;
            int closestDist = 9999;

            // Find the best position to throw a grenade
            for (auto* e1 : enemies) {
                if (!e1->isAlive()) continue;
                int dist = manhattan(w->pos, e1->pos);

                if (dist >= 3 && dist <= GRENADE_RANGE) {
                    int count = 0;
                    for (auto* e2 : enemies) {
                        if (!e2->isAlive()) continue;
                        if (manhattan(e1->pos, e2->pos) <= GRENADE_BLAST_RADIUS)
                            count++;
                    }
                    if (count > maxEnemies || (count == maxEnemies && dist < closestDist)) {
                        maxEnemies = count;
                        bestTarget = e1->pos;
                        closestDist = dist;
                    }
                }
            }

            bool hasLOS = false;
            if (maxEnemies >= 1)
                hasLOS = hasLOF(map, w->pos, bestTarget);

            bool shouldThrow =
                (maxEnemies >= 2) ||
                (maxEnemies >= 1 && !hasLOS) ||
                (maxEnemies >= 1 && closestDist >= 7);

            if (shouldThrow && bestTarget.x != -1) {
                w->grenades--;
                w->grenadeCooldown = GRENADE_COOLDOWN;

                Grenade* g = new Grenade(
                    w->pos.x + 0.5, w->pos.y + 0.5,
                    bestTarget.x + 0.5, bestTarget.y + 0.5,
                    w->teamId,
                    GRENADE_FUSE_TICKS,
                    GRENADE_FLY_SPEED
                );
                grenades.push_back(g);

                std::cout << "GRENADE! Team " << w->teamId << " W at ("
                    << w->pos.x << "," << w->pos.y << ") -> ("
                    << bestTarget.x << "," << bestTarget.y
                    << ") targeting " << maxEnemies << " enemies [dist="
                    << closestDist << "]\n";

                addSafetyThreatAtPosition(bestTarget, GRENADE_BLAST_RADIUS * 1.5f, 3.0f);

                tempMarkers.push_back({ bestTarget, 20, 'o' });
                continue;
            }
        }

        if (w->grenadeCooldown > 0)
            w->grenadeCooldown--;

        // Normal Shooting 
        if (w->bullets <= 0) continue;

        // Pause between shots
        if (w->bulletCooldown > 0) {
            w->bulletCooldown--;
            continue;
        }

        Character* target = nullptr;
        int minDist = FIRE_RANGE + 1;
        for (auto* e : enemies) {
            if (!e->isAlive()) continue;
            int dist = manhattan(w->pos, e->pos);
            if (dist <= FIRE_RANGE && dist < minDist) {
                if (hasLOF(map, w->pos, e->pos)) {
                    target = e;
                    minDist = dist;
                }
            }
        }

        if (target) {
            // Consume ammo and set cooldown
            w->bullets--;
            w->bulletCooldown = 10; // Pause time between shots (10 ticks)

            // Create a visible bullet 
            double sx = w->pos.x + 0.5;   // start at center of tile
            double sy = w->pos.y + 0.5;
            double tx = target->pos.x + 0.5;
            double ty = target->pos.y + 0.5;

            double angle = std::atan2(ty - sy, tx - sx);

            Bullet* b = new Bullet(sx, sy, angle);
			b->setTeamId(w->teamId);   // to avoid friendly fire
            bullets.push_back(b);

            std::cout << "FIRE! Team " << w->teamId << " W at ("
                << w->pos.x << "," << w->pos.y << ") -> Team "
                << target->teamId << " " << target->icon << " at ("
                << target->pos.x << "," << target->pos.y
                << ") [dist=" << minDist << "]\n";

            addSafetyThreatAtPosition(target->pos, FIRE_RANGE * 0.2f, 0.1f);

            // Temporary effect for shooting
            tempMarkers.push_back({ target->pos, 15, '*' });
        }
    }
}


void Game::checkGameOver() {
    if (gameOver) return;

    // A team is out when it can no longer fight (see canStillFight)
    int alive0 = canStillFight(team0) ? 1 : 0;
    int alive1 = canStillFight(team1) ? 1 : 0;

    if (alive0 == 0 && alive1 == 0) {
        gameOver = true;
        winningTeam = -1; // Draw
        std::cout << "\n=== GAME OVER ===\nDraw: neither team can fight.\n";
        return;
    }
    if (alive0 == 0) {
        gameOver = true;
        winningTeam = 1;
        std::cout << "\n=== GAME OVER ===\nTeam 1 (Orange) WINS!\n";
        return;
    }
    if (alive1 == 0) {
        gameOver = true;
        winningTeam = 0;
        std::cout << "\n=== GAME OVER ===\nTeam 0 (Blue) WINS!\n";
        return;
    }
}


Point Game::pickExplorationAnchor(int teamId) {
    if (teamId == 0) {
        int x = SPAWN_MARGIN + 10 + std::rand() % (MSZ / 2);
        int y = std::rand() % MSZ;
        return { x, y };
    }
    else {
        int x = MSZ - SPAWN_MARGIN - 10 - std::rand() % (MSZ / 2);
        int y = std::rand() % MSZ;
        return { x, y };
    }
}

void Game::cycleViewMode() {
    switch (viewMode) {
    case ViewMode::Game:
        viewMode = ViewMode::Visibility;
        std::cout << "View: Visibility Map (Both Teams)\n";
        break;
    case ViewMode::Visibility:
        viewMode = ViewMode::Safety;
        std::cout << "View: Safety Map\n";
        break;
    case ViewMode::Safety:
        viewMode = ViewMode::Game;
        std::cout << "View: Game\n";
        break;
    }
}


void Game::update(float dt) {
    if (gameOver || paused) return;
    gameTick++;

    // Debug: Print status every 100 ticks
    if (gameTick % 100 == 0) {
        std::cout << "\n=== Tick " << gameTick << " ===\n";
        std::cout << "Team 0 (Blue):\n";
        for (auto* c : team0) {
            if (c->isAlive()) {
                std::cout << "  " << c->icon << " at (" << c->pos.x << "," << c->pos.y << ")";
                if (c->icon == 'W') {
                    Warrior* w = static_cast<Warrior*>(c);
                    std::cout << " bullets=" << w->bullets
                        << " grenades=" << w->grenades
                        << " state=" << (int)w->state;
                }
                std::cout << "\n";
            }
        }
        std::cout << "Team 1 (Orange):\n";
        for (auto* c : team1) {
            if (c->isAlive()) {
                std::cout << "  " << c->icon << " at (" << c->pos.x << "," << c->pos.y << ")";
                if (c->icon == 'W') {
                    Warrior* w = static_cast<Warrior*>(c);
                    std::cout << " bullets=" << w->bullets
                        << " grenades=" << w->grenades
                        << " state=" << (int)w->state;
                }
                std::cout << "\n";
            }
        }
        std::cout << "Active grenades: " << grenades.size() << "\n";
        std::cout << "Active bullets: " << bullets.size() << "\n";
    }

    // Update decoy grenades 
    updateDummyGrenades();

    // Real grenades update 
    for (auto* g : grenades) {
        if (!g) continue;
        g->Tick(map);

        // If the grenade just exploded: danger map + damage, exactly once
        if (g->HasExploded() && !g->damageApplied) {
            g->damageApplied = true;
            //  Update safety map
            g->UpdateSecurityMap(map, safety);

            // Apply AoE damage to characters inside blast radius
            double gx = g->getX();
            double gy = g->getY();

            for (auto* c : allCharacters) {
                if (!c || !c->isAlive()) continue;

                // Do not damage same team
                if (c->teamId == g->getTeamId()) continue;

                double cx = c->pos.x + 0.5;
                double cy = c->pos.y + 0.5;

                double dx = gx - cx;
                double dy = gy - cy;
                double dist2 = dx * dx + dy * dy;

                if (dist2 <= GRENADE_BLAST_RADIUS * GRENADE_BLAST_RADIUS) {
                    c->onDamaged(GRENADE_DAMAGE);
                    std::cout << "GRENADE HIT! Team " << c->teamId
                        << " " << c->icon << " at ("
                        << c->pos.x << "," << c->pos.y
                        << ") [HP: " << c->hp << "]\n";
                }
            }
        }
    }

    // Clear grenades that exploded and finished their explosion animation
    grenades.erase(
        std::remove_if(grenades.begin(), grenades.end(),
            [](Grenade* g) {
                if (g && g->isExploded && g->explosionTicks >= GRENADE_VISUAL_TICKS) {
                    delete g;
                    return true;
                }
                return false;
            }),
        grenades.end()
    );

    updateSafetyMap();

    // Update all characters
    for (auto* c : allCharacters) {
        if (c->isAlive()) {
            c->tick = gameTick;
            c->update(dt, map, safety);
        }
    }

    // Battles (shooting / grenades decision logic)
    doCombat();

    //  Bullets update 
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        Bullet* b = *it;
        if (!b) {
            it = bullets.erase(it);
            continue;
        }

        b->Move(map);
        bool removeBullet = false;
        if (!b->isMoving) {
            removeBullet = true;
        }
        else {
            for (auto* c : allCharacters) {
                if (!c || !c->isAlive()) continue;
                if (c->teamId == b->getTeamId()) continue;

                double cx = c->pos.x + 0.5;
                double cy = c->pos.y + 0.5;

                double dx = b->x - cx;
                double dy = b->y - cy;
                double dist2 = dx * dx + dy * dy;

                if (dist2 <= HIT_RADIUS * HIT_RADIUS) {
                    c->onDamaged(FIRE_DAMAGE);
                    std::cout << "Bullet hit Team " << c->teamId
                        << " " << c->icon << " at ("
                        << c->pos.x << "," << c->pos.y
                        << ") [HP: " << c->hp << "]\n";
                    b->setIsMoving(false);
                    removeBullet = true;
                    break;
                }
            }
        }

        if (removeBullet) {
            delete b;
            it = bullets.erase(it);
        }
        else {
            ++it;
        }
    }

    // Update temporary markers 
    for (auto it = tempMarkers.begin(); it != tempMarkers.end(); ) {
        if (--it->ttl <= 0)
            it = tempMarkers.erase(it);
        else
            ++it;
    }

    checkGameOver();
}


// Short text for what a unit is doing right now
static const char* stateLabel(Character* c) {
    if (auto* w = dynamic_cast<Warrior*>(c)) {
        switch (w->state) {
        case WarState::IDLE:     return "Patrolling";
        case WarState::ADVANCE:  return "Advancing";
        case WarState::ATTACK:   return "Attacking";
        case WarState::DEFEND:   return "Taking cover";
        case WarState::WOUNDED:  return "Wounded";
        case WarState::LOW_AMMO: return "Low ammo";
        }
    }
    if (auto* m = dynamic_cast<Medic*>(c)) {
        switch (m->state) {
        case MedicState::IDLE:
        case MedicState::GO_TO_PATROL:  return "Patrolling";
        case MedicState::GO_MED_DEPOT:  return "To med depot";
        case MedicState::GO_TO_WOUNDED: return "To wounded";
        case MedicState::HEAL:          return "Healing";
        }
    }
    if (auto* p = dynamic_cast<Provider*>(c)) {
        switch (p->state) {
        case ProvState::IDLE:
        case ProvState::GO_TO_PATROL:  return "Patrolling";
        case ProvState::GO_AMMO_DEPOT: return "To ammo depot";
        case ProvState::GO_TO_WARRIOR: return "Delivering";
        case ProvState::SUPPLY:        return "Resupplying";
        case ProvState::WOUNDED:       return "Wounded";
        }
    }
    if (auto* cm = dynamic_cast<Commander*>(c)) {
        switch (cm->state) {
        case CommanderState::IDLE:        return "Holding";
        case CommanderState::PLANNING:    return "Planning";
        case CommanderState::ATTACKING:   return "Ordering attack";
        case CommanderState::DEFENDING:   return "Ordering defense";
        case CommanderState::SURRENDERED: return "Surrendered";
        case CommanderState::WOUNDED:     return "Wounded";
        }
    }
    return "";
}

static const char* roleName(char icon) {
    switch (icon) {
    case 'C': return "Commander";
    case 'W': return "Warrior";
    case 'M': return "Medic";
    case 'P': return "Provider";
    }
    return "?";
}

void Game::drawTeamStatus(const std::vector<Character*>& team, int teamId, float top) {
    const float x0 = MSZ + 2.0f;
    const Color3& tc = teamColor(teamId);

    // header
    setColor(tc);
    drawRectF(x0, top - 0.6f, 1.0f, 2.4f);
    setColor(COLOR_PANEL_TEXT);
    drawText(x0 + 2.0f, top, teamId == 0 ? "BLUE TEAM" : "ORANGE TEAM", GLUT_BITMAP_HELVETICA_18);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d / %d active", countActive(team), (int)team.size());
    setColor(COLOR_PANEL_DIM);
    drawText(MSZ + PANEL_W - 2.0f - textWidth(buf), top, buf);

    float y = top - 5.0f;
    int warriorNo = 0;
    for (auto* c : team) {
        bool alive = c->isAlive();
        float cx = x0 + 1.2f, cy = y + 1.0f;

        // icon like on the map
        if (c->icon == 'C') setColor(alive ? COLOR_GOLD : COLOR_PANEL_DIM); else glColor3f(1, 1, 1);
        drawDisc(cx, cy, 1.45f);
        setColor(alive ? tc : COLOR_BAR_BG);
        drawDisc(cx, cy, 1.2f);
        char s[2] = { c->icon, 0 };
        glColor3f(1, 1, 1);
        drawTextCentered(cx, cy - 0.55f, s);

        // name + health
        if (c->icon == 'W') std::snprintf(buf, sizeof(buf), "Warrior %d", ++warriorNo);
        else std::snprintf(buf, sizeof(buf), "%s", roleName(c->icon));
        setColor(alive ? COLOR_PANEL_TEXT : COLOR_PANEL_DIM);
        drawText(x0 + 3.5f, y + 1.1f, buf);

        if (alive) {
            drawHealthBar(x0 + 15.0f, y + 1.1f, 11.0f, 1.0f, c->hp);
            std::snprintf(buf, sizeof(buf), "%d", c->hp);
            setColor(COLOR_PANEL_DIM);
            drawText(x0 + 27.0f, y + 1.1f, buf, GLUT_BITMAP_HELVETICA_10);

            // state (+ ammo for warriors)
            if (c->icon == 'W')
                std::snprintf(buf, sizeof(buf), "%s  |  bullets %d  grenades %d", stateLabel(c), c->bullets, c->grenades);
            else
                std::snprintf(buf, sizeof(buf), "%s", stateLabel(c));
            drawText(x0 + 3.5f, y - 0.9f, buf, GLUT_BITMAP_HELVETICA_10);
        }
        else {
            setColor(COLOR_HP_LOW);
            drawText(x0 + 15.0f, y + 1.1f, "Killed in action", GLUT_BITMAP_HELVETICA_10);
        }
        y -= 4.6f;
    }
}

void Game::drawPanel() {
    const float x0 = MSZ + 2.0f, right = MSZ + PANEL_W - 2.0f;
    char buf[64];

    setColor(COLOR_PANEL_BG);
    drawRectF((float)MSZ, 0.0f, (float)PANEL_W, (float)MSZ);

    // title + time
    setColor(COLOR_PANEL_TEXT);
    drawText(x0, MSZ - 4.0f, "TACTICAL AI BATTLE", GLUT_BITMAP_HELVETICA_18);
    std::snprintf(buf, sizeof(buf), "Tick %d%s", gameTick, paused ? "   PAUSED" : "");
    setColor(paused ? COLOR_GOLD : COLOR_PANEL_DIM);
    drawText(x0, MSZ - 6.6f, buf);

    // current view
    const char* view = viewMode == ViewMode::Game ? "Battlefield"
        : viewMode == ViewMode::Visibility ? "Visibility (what each team sees)"
        : "Danger map (safety)";
    setColor(COLOR_PANEL_DIM);
    drawText(x0, MSZ - 9.4f, "View:");
    setColor(COLOR_PANEL_TEXT);
    drawText(x0 + 4.2f, MSZ - 9.4f, view);

    drawTeamStatus(team0, 0, MSZ - 14.5f);
    drawTeamStatus(team1, 1, MSZ - 43.0f);

    // legend
    float ly = 18.0f;
    setColor(COLOR_PANEL_DIM);
    drawText(x0, ly, "MAP");
    DrawDepot({ (int)x0, (int)(ly - 5.5f) }, AMMO_DEPOT, 0);
    setColor(COLOR_PANEL_TEXT);
    drawText(x0 + 4.0f, ly - 4.5f, "Ammo depot", GLUT_BITMAP_HELVETICA_10);
    DrawDepot({ (int)(x0 + 16.0f), (int)(ly - 5.5f) }, MED_DEPOT, 1);
    setColor(COLOR_PANEL_TEXT);
    drawText(x0 + 20.0f, ly - 4.5f, "Med depot", GLUT_BITMAP_HELVETICA_10);

    setColor(COLOR_STONE);  drawRectF(x0, ly - 9.0f, 1.6f, 1.6f);
    setColor(COLOR_PANEL_TEXT); drawText(x0 + 2.4f, ly - 8.7f, "Rock", GLUT_BITMAP_HELVETICA_10);
    setColor(COLOR_TREE);   drawDisc(x0 + 10.8f, ly - 8.2f, 0.8f, 12);
    setColor(COLOR_PANEL_TEXT); drawText(x0 + 12.2f, ly - 8.7f, "Tree", GLUT_BITMAP_HELVETICA_10);
    setColor(COLOR_WATER);  drawRectF(x0 + 19.0f, ly - 9.0f, 1.6f, 1.6f);
    setColor(COLOR_PANEL_TEXT); drawText(x0 + 21.4f, ly - 8.7f, "Water", GLUT_BITMAP_HELVETICA_10);

    // controls
    setColor(COLOR_PANEL_DIM);
    drawText(x0, 4.6f, "V / right click  change view", GLUT_BITMAP_HELVETICA_10);
    drawText(x0, 2.4f, "P pause    R new battle    Esc quit", GLUT_BITMAP_HELVETICA_10);
    (void)right;
}

void Game::drawEndScreen() {
    enableBlend();
    glColor4f(0, 0, 0, 0.5f);
    drawRectF(0, 0, (float)MSZ, (float)MSZ);
    glDisable(GL_BLEND);

    setColor(COLOR_PANEL_BG);
    drawRectF(MSZ / 2.0f - 28, MSZ / 2.0f - 9, 56, 18);

    const char* title = winningTeam == 0 ? "BLUE TEAM WINS"
        : winningTeam == 1 ? "ORANGE TEAM WINS" : "DRAW";
    if (winningTeam >= 0) setColor(teamColor(winningTeam)); else setColor(COLOR_PANEL_TEXT);
    drawTextCentered(MSZ / 2.0f, MSZ / 2.0f + 2.5f, title, GLUT_BITMAP_TIMES_ROMAN_24);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Battle lasted %d ticks", gameTick);
    setColor(COLOR_PANEL_TEXT);
    drawTextCentered(MSZ / 2.0f, MSZ / 2.0f - 2.0f, buf);
    setColor(COLOR_PANEL_DIM);
    drawTextCentered(MSZ / 2.0f, MSZ / 2.0f - 5.5f, "Press R for a new battle", GLUT_BITMAP_HELVETICA_10);
}

void Game::draw() {
    // Map + depots
    DrawMap(map);
    DrawDepot(depotAmmo0, AMMO_DEPOT, 0);
    DrawDepot(depotMed0, MED_DEPOT, 0);
    DrawDepot(depotAmmo1, AMMO_DEPOT, 1);
    DrawDepot(depotMed1, MED_DEPOT, 1);

    // Overlays based on view mode
    if (viewMode == ViewMode::Visibility) {
        if (commander0) DrawVisibilityOverlay(commander0->combinedFOV, 0);
        if (commander1) DrawVisibilityOverlay(commander1->combinedFOV, 1);
    }
    else if (viewMode == ViewMode::Safety) {
        DrawSafetyHeat(map, safety);
    }

    // Shot / grenade target markers
    enableBlend();
    for (const auto& m : tempMarkers) {
        float cx = m.pos.x + 0.5f, cy = m.pos.y + 0.5f;
        if (m.symbol == 'o') {             // grenade aim point: crosshair
            glColor4f(0.85f, 0.15f, 0.1f, 0.8f);
            glLineWidth(2.0f);
            glBegin(GL_LINES);
            glVertex2f(cx - 1.2f, cy); glVertex2f(cx + 1.2f, cy);
            glVertex2f(cx, cy - 1.2f); glVertex2f(cx, cy + 1.2f);
            glEnd();
            drawRing(cx, cy, 0.9f, 20);
            glLineWidth(1.0f);
        }
        else {                             // shot target: fading flash
            glColor4f(1.0f, 0.3f, 0.1f, m.ttl / 15.0f);
            glLineWidth(2.0f);
            drawRing(cx, cy, 1.6f, 20);
            glLineWidth(1.0f);
        }
    }
    glDisable(GL_BLEND);

    // Units, then projectiles on top
    DrawCharacters(allCharacters);
    for (auto* b : bullets) if (b) b->Show();
    for (auto* g : grenades) if (g) g->Show();

    drawPanel();
    if (gameOver) drawEndScreen();
}


Warrior* Game::findNearestWarriorNeedingAmmo(const Provider* provider) {
    const std::vector<Character*>& team = (provider->teamId == 0) ? team0 : team1;
    Warrior* best = nullptr;
    int bestDist = 9999;

    for (auto* ch : team) {
        Warrior* w = dynamic_cast<Warrior*>(ch);
        if (w && w->isAlive() && (w->needsAmmo() || w->needsGrenades())) {
            int d = manhattan(provider->pos, w->pos);
            if (d < bestDist) {
                bestDist = d;
                best = w;
            }
        }
    }
    return best;
}


