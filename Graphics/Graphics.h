#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "glut.h"
#include "Definitions.h"
#include "Character.h"

// ================== Basic shapes ==================

inline void setColor(const Color3& c, float a = 1.0f) { glColor4f(c.r, c.g, c.b, a); }

inline Color3 mixColor(const Color3& a, const Color3& b, float t) {
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

inline const Color3& teamColor(int team) { return team == 0 ? COLOR_TEAM0 : COLOR_TEAM1; }

inline void drawCellQuad(int j, int i) {
    glBegin(GL_POLYGON);
    glVertex2d(j, i);
    glVertex2d(j, i + 1);
    glVertex2d(j + 1, i + 1);
    glVertex2d(j + 1, i);
    glEnd();
}

inline void drawRectF(float x, float y, float w, float h) {
    glBegin(GL_POLYGON);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

inline void drawRectOutline(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

inline void drawDisc(float x, float y, float r, int seg = 20) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= seg; i++) {
        float a = (float)(2.0 * PI * i / seg);
        glVertex2f(x + r * std::cos(a), y + r * std::sin(a));
    }
    glEnd();
}

inline void drawRing(float x, float y, float r, int seg = 32) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = (float)(2.0 * PI * i / seg);
        glVertex2f(x + r * std::cos(a), y + r * std::sin(a));
    }
    glEnd();
}

inline void enableBlend() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// ================== Text ==================

// x, y in map units; bitmap fonts have a fixed pixel size
inline void drawText(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    glRasterPos2f(x, y);
    for (const char* p = s; *p; ++p) glutBitmapCharacter(font, *p);
}

inline float textWidth(const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    return glutBitmapLength(font, (const unsigned char*)s) / (float)CELL_PX;
}

inline void drawTextCentered(float cx, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_12) {
    drawText(cx - textWidth(s, font) / 2, y, s, font);
}

// ================== Health bar ==================

inline void drawHealthBar(float x0, float y0, float w, float h, int hp) {
    float ratio = std::max(0.0f, std::min(1.0f, hp / (float)MAX_HEALTH));
    setColor(COLOR_BAR_BG);
    drawRectF(x0, y0, w, h);
    const Color3& fill = (hp > WOUNDED_THRESHOLD) ? COLOR_HP_FILL
        : (hp > WOUNDED_THRESHOLD / 2 ? COLOR_HP_MID : COLOR_HP_LOW);
    setColor(fill);
    drawRectF(x0, y0, w * ratio, h);
}

// ================== Map ==================

inline void DrawMap(int map[MSZ][MSZ]) {
    // floor in one quad, then only the special cells
    setColor(COLOR_EMPTY);
    drawRectF(0, 0, MSZ, MSZ);

    glBegin(GL_QUADS);
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            int cell = map[i][j];
            if (cell != STONE && cell != WATER) continue;
            setColor(cell == STONE ? COLOR_STONE : COLOR_WATER);
            glVertex2f((float)j, (float)i); glVertex2f(j + 1.0f, (float)i);
            glVertex2f(j + 1.0f, i + 1.0f); glVertex2f((float)j, i + 1.0f);
        }
    }
    glEnd();

    // trees: one round canopy per tree cell (the cell that blocks sight)
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            if (map[i][j] != TREE) continue;
            setColor(COLOR_TREE_DARK);
            drawDisc(j + 0.5f, i + 0.5f, 0.62f, 10);
            setColor(COLOR_TREE);
            drawDisc(j + 0.45f, i + 0.55f, 0.48f, 10);
        }
    }
}

// 3x3 depot block: team-tinted floor, team border, icon for its type
inline void DrawDepot(Point topLeft, int type, int team) {
    const Color3& tc = teamColor(team);
    float x = (float)topLeft.x, y = (float)topLeft.y, s = (float)DEPOT_SIZE;

    setColor(mixColor(tc, { 1, 1, 1 }, 0.75f));
    drawRectF(x, y, s, s);
    glLineWidth(3.0f);
    setColor(tc);
    drawRectOutline(x, y, s, s);
    glLineWidth(1.0f);

    float cx = x + s / 2, cy = y + s / 2;
    if (type == MED_DEPOT) {
        glColor3f(1, 1, 1);
        drawRectF(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f);
        setColor(COLOR_MED);
        drawRectF(cx - 0.25f, cy - 0.8f, 0.5f, 1.6f);
        drawRectF(cx - 0.8f, cy - 0.25f, 1.6f, 0.5f);
    }
    else {
        // three cartridges
        for (int k = -1; k <= 1; k++) {
            float bx = cx + k * 0.55f;
            setColor(COLOR_AMMO);
            drawRectF(bx - 0.2f, cy - 0.9f, 0.4f, 1.2f);
            setColor(COLOR_AMMO_TIP);
            drawDisc(bx, cy + 0.3f, 0.2f, 8);
            drawRectF(bx - 0.2f, cy + 0.3f, 0.4f, 0.25f);
        }
    }
}

// ================== Units ==================

inline void DrawUnit(const Character* c) {
    const Color3& tc = teamColor(c->teamId);
    float x = c->pos.x + 0.5f, y = c->pos.y + 0.5f;
    bool cmd = (c->icon == 'C');
    float r = cmd ? 1.5f : 1.25f;

    // outline (gold for the commander), body, role letter
    if (cmd) setColor(COLOR_GOLD); else glColor3f(1, 1, 1);
    drawDisc(x, y, r + 0.25f);
    setColor(tc);
    drawDisc(x, y, r);

    char s[2] = { c->icon, 0 };
    glColor3f(1, 1, 1);
    drawText(x - textWidth(s, GLUT_BITMAP_HELVETICA_12) / 2, y - 0.55f, s, GLUT_BITMAP_HELVETICA_12);

    // health bar above
    drawHealthBar(x - 1.4f, y + r + 0.5f, 2.8f, 0.5f, c->hp);
}

inline void DrawCharacters(const std::vector<Character*>& chars) {
    for (auto c : chars)
        if (c->isAlive()) DrawUnit(c);
}

// ================== Overlays ==================

inline void DrawVisibilityOverlay(const std::vector<unsigned char>& fovMask, int team) {
    const Color3& tc = teamColor(team);
    enableBlend();
    setColor(tc, 0.22f);
    glBegin(GL_QUADS);
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            if (!fovMask[i * MSZ + j]) continue;
            glVertex2f((float)j, (float)i); glVertex2f(j + 1.0f, (float)i);
            glVertex2f(j + 1.0f, i + 1.0f); glVertex2f((float)j, i + 1.0f);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
}

inline void DrawSafetyHeat(int map[MSZ][MSZ], double safety[MSZ][MSZ]) {
    double mx = 1e-9;
    for (int i = 0; i < MSZ; i++)
        for (int j = 0; j < MSZ; j++)
            if (safety[i][j] > mx) mx = safety[i][j];

    enableBlend();
    glBegin(GL_QUADS);
    for (int i = 0; i < MSZ; i++) {
        for (int j = 0; j < MSZ; j++) {
            if (map[i][j] == STONE) continue;
            double v = safety[i][j] / mx;
            if (v <= 1e-3) continue;
            float a = std::min(HEAT_ALPHA_MAX,
                HEAT_ALPHA_MIN + (float)v * (HEAT_ALPHA_MAX - HEAT_ALPHA_MIN));
            glColor4f(0.85f, 0.1f * (1.0f - (float)v), 0.05f, a);
            glVertex2f((float)j, (float)i); glVertex2f(j + 1.0f, (float)i);
            glVertex2f(j + 1.0f, i + 1.0f); glVertex2f((float)j, i + 1.0f);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
}
