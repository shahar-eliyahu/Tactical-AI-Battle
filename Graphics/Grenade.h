#pragma once
#include "Bullet.h"
#include "Definitions.h"

const int NUM_BULLETS = 10;

class Grenade
{
private:
    Bullet* bullets[NUM_BULLETS];

    // Position & movement
    double x, y;          // current position
    double tx, ty;        // target position
    double dirX, dirY;    // normalized direction of flight
    double flySpeed;      // flight speed

    // Timing
    int fuseTicks;        // how many ticks until explosion
    int ageTicks;         // how long the grenade has existed

    // Team ownership (who threw the grenade)
    int teamId;          

    void explodeNow(int map[MSZ][MSZ]);

public:
    int  explosionTicks = 0;   // ticks since the explosion (advanced in Tick, not in Show)
    bool isExploded;
    bool damageApplied = false; // blast damage is applied exactly once

    // Main constructor
    Grenade(double sx, double sy,
        double tx, double ty,
        int teamId,
        int fuse = GRENADE_FUSE_TICKS,
        double speed = GRENADE_FLY_SPEED);

    // Convenience constructor: static grenade 
    Grenade(double xPos, double yPos)
        : Grenade(xPos, yPos, xPos, yPos, -1, 0, 0) {
    }
    ~Grenade();

    void Tick(int map[MSZ][MSZ]);
    void Show();

    void setIsExploded(bool value);
    void UpdateSecurityMap(int map[MSZ][MSZ], double securityMap[MSZ][MSZ]);
    bool HasExploded() const { return isExploded; }

   
    double getX() const { return x; }
    double getY() const { return y; }
    int    getTeamId() const { return teamId; }
};
