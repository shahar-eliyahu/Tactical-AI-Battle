#include "Grenade.h"
#include <cmath>
#include <algorithm>
#include "glut.h"


Grenade::Grenade(double sx, double sy, double tx_, double ty_, int teamId_,
    int fuse, double speed)
{
    x = sx; y = sy;
    tx = tx_; ty = ty_;
    double dx = tx - x, dy = ty - y;
    double len = std::sqrt(dx * dx + dy * dy);
    if (len == 0) { dirX = 0; dirY = 0; }
    else { dirX = dx / len; dirY = dy / len; }

    flySpeed = speed;
    fuseTicks = fuse;
    ageTicks = 0;
    isExploded = false;
    teamId = teamId_;

    // Init pointers
    for (int i = 0; i < NUM_BULLETS; ++i) bullets[i] = nullptr;
}

Grenade::~Grenade()
{
    for (int i = 0; i < NUM_BULLETS; ++i) delete bullets[i];
}

void Grenade::explodeNow(int map[MSZ][MSZ])
{
    if (isExploded) return;
    isExploded = true;

    double teta = (360.0 / NUM_BULLETS) * PI / 180.0;
    double angle = 0.0;

    for (int i = 0; i < NUM_BULLETS; ++i, angle += teta)
    {
        bullets[i] = new Bullet(x, y, angle);
        bullets[i]->setTeamId(teamId); 
        bullets[i]->setIsMoving(true);
    }
}

void Grenade::Tick(int map[MSZ][MSZ])
{
    if (!isExploded)
    {
        if (flySpeed > 0.0)
        {
            double nx = x + dirX * flySpeed;
            double ny = y + dirY * flySpeed;

            // If we hit an obstacle or went out of bounds – we will explode on the spot
            bool hitObstacle =
                !(nx >= 0 && nx < MSZ && ny >= 0 && ny < MSZ) ||
                (map[(int)ny][(int)nx] == STONE);

            // If it arrived close enough to the destination – it is considered as arrival
            double distNext = std::hypot((tx - nx), (ty - ny));
            bool reachedTarget = distNext <= 0.75; 

            if (hitObstacle || reachedTarget)
            {
                x = nx; y = ny; 
                explodeNow(map);
            }
            else
            {
                x = nx; y = ny;
            }
        }

        ageTicks++;
        if (!isExploded && ageTicks >= fuseTicks)
        {
            explodeNow(map);
        }
    }
    else
    {
        explosionTicks++;
        for (int i = 0; i < NUM_BULLETS; ++i)
            if (bullets[i]) bullets[i]->Move(map);
    }
}

static void grenadeDisc(double x, double y, double r)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(x, y);
    for (int i = 0; i <= 24; ++i)
    {
        double a = i * (2.0 * PI / 24);
        glVertex2d(x + r * std::cos(a), y + r * std::sin(a));
    }
    glEnd();
}

void Grenade::Show()
{
    //  Explosion: a flash that grows to the real blast radius, then fades,
    //  with a ring marking exactly the area that took damage
    if (isExploded)
    {
        double t = std::min(1.0, explosionTicks / (double)GRENADE_VISUAL_TICKS);
        double grow = std::min(1.0, explosionTicks / 6.0);
        double r = GRENADE_BLAST_RADIUS * grow;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4d(1.0, 0.55, 0.1, 0.45 * (1.0 - t));
        grenadeDisc(x, y, r);
        glColor4d(1.0, 0.9, 0.4, 0.7 * (1.0 - t));
        grenadeDisc(x, y, r * 0.45);

        glLineWidth(2.0f);
        glColor4d(0.85, 0.2, 0.05, 0.9 * (1.0 - t));
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 40; ++i)
        {
            double a = i * (2.0 * PI / 40);
            glVertex2d(x + GRENADE_BLAST_RADIUS * std::cos(a), y + GRENADE_BLAST_RADIUS * std::sin(a));
        }
        glEnd();
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
        return;
    }

    //  Flight: dark body with a ring in the thrower's team color
    const Color3& tc = (teamId == 0) ? COLOR_TEAM0 : COLOR_TEAM1;
    glColor3f(tc.r, tc.g, tc.b);
    grenadeDisc(x, y, 0.55);
    glColor3f(0.15f, 0.17f, 0.12f);
    grenadeDisc(x, y, 0.38);
}



void Grenade::setIsExploded(bool value)
{
    if (value && !isExploded)
    {
        for (int i = 0; i < NUM_BULLETS; ++i) delete bullets[i];
        isExploded = true;
        double teta = (360.0 / NUM_BULLETS) * PI / 180.0;
        double angle = 0.0;
        for (int i = 0; i < NUM_BULLETS; ++i, angle += teta)
        {
            bullets[i] = new Bullet(x, y, angle);
            bullets[i]->setTeamId(teamId);
            bullets[i]->setIsMoving(true);
        }
    }
}

void Grenade::UpdateSecurityMap(int map[MSZ][MSZ], double securityMap[MSZ][MSZ])
{
    if (!isExploded) return;

    for (int i = 0; i < NUM_BULLETS; ++i)
        if (bullets[i]) bullets[i]->UpdateSecurityMap(map, securityMap);
}
