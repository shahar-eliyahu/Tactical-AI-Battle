#include "Character.h"

Character::Character(Point p, char icon, int team) {
    pos = p;
    prevPos = p;
    this->icon = icon;
    this->teamId = team;
    hp = MAX_HEALTH;
    bullets = DEFAULT_BULLETS;
    grenades = DEFAULT_GRENADES;
    moveTickCounter = 0;
    tick = 0;
    pGame = nullptr;
    pCommander = nullptr;
}

void Character::onDamaged(int amount) {
    int oldHP = hp;
    hp -= amount;
    if (hp < 0) hp = 0;

    std::cout  << icon << " of Team " << teamId
              << " took " << amount << " damage! [HP: " << oldHP << " → " << hp << "]\n";

	//check for death
    if (oldHP > 0 && hp == 0) {
        std::cout  << icon << " of Team " << teamId
                  << " has DIED at position (" << pos.x << "," << pos.y << ")!\n";
    }
}


void Character::heal(int amount) {
    if (!isAlive()) return;
    hp = std::min(hp + amount, MAX_HEALTH);
}

void Character::reload(int ammoAmount) {
    bullets = std::min(bullets + ammoAmount, DEFAULT_BULLETS);
}

void Character::stepAlongPath(int map[MSZ][MSZ], const std::vector<Character*>& allChars) {
    if (!isAlive()) return;

    // Move every N ticks
    if ((++moveTickCounter % MOVE_EVERY_N_TICKS) != 0) return;

    //  Anti-stuck detection 
    if (pos == lastPos) stuckCounter++;
    else {
        stuckCounter = 0;
        lastPos = pos;
    }

    //  If the character is stuck for a long time - reset path so it searches for a new destination
    if (stuckCounter > 30) {
        path.clear();
        stuckCounter = 0;
    }

    //  If there is no path at all - random fallback
    if (path.empty()) {
        int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        int i = rand() % 4;
        int nx = pos.x + dirs[i][0];
        int ny = pos.y + dirs[i][1];
        if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
            prevPos = pos;
            pos = {nx, ny};
        }
        return;
    }

    Point nextStep = path.front();

    // Check if there is another character on the path
    bool occupied = false;
    for (auto* c : allChars) {
        if (c->isAlive() && c != this && c->pos == nextStep) {
            occupied = true;
            break;
        }
    }

    bool canPass = passableForAgent(map[nextStep.y][nextStep.x]);

    if (!occupied && canPass) {
        path.erase(path.begin());
        prevPos = pos;
        pos = nextStep;
    } 
    else {
        // Blocked  try a random small step
        path.clear();
        int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        int i = rand() % 4;
        int nx = pos.x + dirs[i][0];
        int ny = pos.y + dirs[i][1];
        if (inBounds(nx, ny) && passableForAgent(map[ny][nx])) {
            prevPos = pos;
            pos = {nx, ny};
        }
    }
}
