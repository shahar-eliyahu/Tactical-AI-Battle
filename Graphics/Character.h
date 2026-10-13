#pragma once
#include "Definitions.h"
#include <vector>
#include <iostream>

// Forward declarations
class Game;
class Commander;

// Base class for all characters
class Character {
public:
    // Position and movement
    Point pos;
    Point prevPos;
    std::vector<Point> path;

    // Identity
    char icon;
    int teamId;

    // Stats
    int hp;
    int bullets;
    int grenades;

    // Timing
    int tick;
    int moveTickCounter;

    // References (set by Game during init)
    Game* pGame;
    Commander* pCommander;

    bool isBeingHealedByMedic = false;

    // Constructor
    Character(Point p, char icon, int team);
    virtual ~Character() {}

    // Core interface
    virtual void update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) = 0;

    // Common functionality
    bool isAlive() const { return hp > 0; }
    void onDamaged(int amount);
    void heal(int amount);
    void reload(int ammoAmount);
    void stepAlongPath(int map[MSZ][MSZ], const std::vector<Character*>& allChars);
 

private:
    int stuckCounter = 0;   // how much time we didnt move
    Point lastPos = { -1, -1 }; // the last cell we moved to
};