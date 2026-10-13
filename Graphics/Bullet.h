#pragma once
#include "Definitions.h"
const double SECURITY = 0.003;

class Bullet
{
private:
    double dirX, dirY;
    int teamId = 0; 

public:
    double x, y;
    bool isMoving;
    Bullet(double xPos, double yPos, double angle);
    void Move(int map[MSZ][MSZ]);
    void Show();
    int getTeamId() const { return teamId; }
    void setIsMoving(bool value) { isMoving = value; }
    void setTeamId(int id) { teamId = id; } 
    void UpdateSecurityMap(int map[MSZ][MSZ], double securityMap[MSZ][MSZ]);
};
