#include "Bullet.h"
#include "Definitions.h"
#include <cmath>
#include "glut.h"

Bullet::Bullet(double xPos, double yPos, double angle)
{
    x = xPos;
    y = yPos;
    dirX = std::cos(angle);
    dirY = std::sin(angle);
    isMoving = true;  // Start moving immediately
}

// Movement of the bullet
void Bullet::Move(int map[MSZ][MSZ])
{
    if (!isMoving) return;

   
    double tmpX = x + BULLET_SPEED * dirX;
    double tmpY = y + BULLET_SPEED * dirY;

    int nx = (int)tmpX;
    int ny = (int)tmpY;

	// check if out of bounds
    if (!inBounds(nx, ny)) {
        isMoving = false;   
        return;
    }

	// check if hit obstacle
    int cell = map[ny][nx];
    if (blocksFire(cell)) {
        isMoving = false;   
        return;
    }

	// else, continue moving
    x = tmpX;
    y = tmpY;
}


void Bullet::Show()
{
    if (!isMoving) return;

    // short tracer in the shooter's team color, bright head
    const Color3& tc = (teamId == 0) ? COLOR_TEAM0 : COLOR_TEAM1;
    glLineWidth(2.5f);
    glColor3f(tc.r, tc.g, tc.b);
    glBegin(GL_LINES);
    glVertex2d(x - dirX * 1.2, y - dirY * 1.2);
    glVertex2d(x, y);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(1.0f, 0.95f, 0.6f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(x, y);
    for (int i = 0; i <= 10; ++i)
    {
        double a = i * (2.0 * PI / 10);
        glVertex2d(x + 0.22 * std::cos(a), y + 0.22 * std::sin(a));
    }
    glEnd();
}


void Bullet::UpdateSecurityMap(int map[MSZ][MSZ], double securityMap[MSZ][MSZ])
{
    isMoving = true;
    double tmpX = x, tmpY = y;

    while (isMoving)
    {
        tmpX += BULLET_SPEED * dirX;
        tmpY += BULLET_SPEED * dirY;
        if (tmpX >= 0 && tmpX < MSZ &&
            tmpY >= 0 && tmpY < MSZ &&
            map[(int)tmpY][(int)tmpX] != STONE)
        {
            x = tmpX;
            y = tmpY;
            securityMap[(int)y][(int)x] += SECURITY;
        }
        else
            isMoving = false;
    }
}
