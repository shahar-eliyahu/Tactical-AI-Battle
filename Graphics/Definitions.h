#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

// ================== Map & Display ==================
const int MSZ = 100;                    // Map size
const int CELL_PX = 8;                  // Pixels per cell
const int PANEL_W = 36;                 // Side panel width (in cells)
const int W = (MSZ + PANEL_W) * CELL_PX, H = MSZ * CELL_PX;   // Window size

const double PI = 3.14159265358979;
// Cell types
const int EMPTY = 0;
const int STONE = 1;
const int TREE = 2;
const int WATER = 3;
const int AMMO_DEPOT = 4;
const int MED_DEPOT = 5;

// ================== Point Structure ==================
struct Point {
    int x;
    int y;
    Point(int xVal = 0, int yVal = 0) : x(xVal), y(yVal) {}

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Point& other) const {
        return !(*this == other);
    }
};

// ================== Colors for Graphics ==================
struct Color3 {
    float r, g, b;
};

// Map colors
const Color3 COLOR_EMPTY = { 0.91f, 0.90f, 0.84f };     // Sand
const Color3 COLOR_STONE = { 0.40f, 0.41f, 0.45f };     // Rock gray
const Color3 COLOR_WATER = { 0.36f, 0.64f, 0.86f };     // Water blue
const Color3 COLOR_TREE = { 0.27f, 0.56f, 0.30f };
const Color3 COLOR_TREE_DARK = { 0.15f, 0.36f, 0.19f };
const Color3 COLOR_AMMO = { 0.80f, 0.62f, 0.22f };      // Brass cartridges
const Color3 COLOR_AMMO_TIP = { 0.45f, 0.33f, 0.12f };
const Color3 COLOR_MED = { 0.13f, 0.62f, 0.33f };       // Green cross

// Team colors
const Color3 COLOR_TEAM0 = { 0.18f, 0.42f, 0.88f };     // Blue
const Color3 COLOR_TEAM1 = { 0.93f, 0.45f, 0.12f };     // Orange
const Color3 COLOR_GOLD = { 1.00f, 0.82f, 0.25f };      // Commander ring

// Health bar colors
const Color3 COLOR_BAR_BG = { 0.15f, 0.15f, 0.15f };
const Color3 COLOR_HP_FILL = { 0.20f, 0.78f, 0.30f };
const Color3 COLOR_HP_MID = { 0.95f, 0.70f, 0.15f };
const Color3 COLOR_HP_LOW = { 0.90f, 0.15f, 0.15f };

// Side panel
const Color3 COLOR_PANEL_BG = { 0.12f, 0.13f, 0.17f };
const Color3 COLOR_PANEL_TEXT = { 0.95f, 0.95f, 0.95f };
const Color3 COLOR_PANEL_DIM = { 0.62f, 0.65f, 0.72f };

const int NUMBER_OF_ROCKS = 130;
const int NUMBER_OF_WATER = 50;
const int NUMBER_OF_TREES = 300;

// Safety map heat overlay
const float HEAT_ALPHA_MIN = 0.1f;
const float HEAT_ALPHA_MAX = 0.6f;

// ================== Character Stats ==================
const int MAX_HEALTH = 100;
const int DEFAULT_BULLETS = 10;
const int DEFAULT_GRENADES = 3;
const int WOUNDED_THRESHOLD = 50;       // Below this = wounded
const int LOW_AMMO_THRESHOLD = 5;      // Below this = low ammo
const int LOW_AMMO_GRENADES = 1;		// Below this = low grenades

// ================== Combat ==================
const double HIT_RADIUS = 0.6; 

// Shooting
const int FIRE_RANGE = 10;             // Shooting range in cells
const int FIRE_DAMAGE = 3;            // Damage per shot

const double BULLET_SPEED = 0.5;   // speed of bullet animation

// Bullet danger parameters
const float BULLET_DANGER_RADIUS = 2.0f;  // how far the danger spreads
const float BULLET_DANGER_INTENSITY = 0.5f; //how intense the danger is

const int GRENADE_RANGE = 12;          // Grenade throw range 
const int GRENADE_BLAST_RADIUS = 5;    // Explosion radius 
const int GRENADE_DAMAGE = 24;        // Damage to every enemy inside the blast radius (applied once)
const int GRENADE_VISUAL_TICKS = 30;   // How long the explosion stays on screen

const int GRENADE_COOLDOWN = 50;       // Ticks between grenades
const double GRENADE_FLY_SPEED = 0.6;  // Animation speed 
const int GRENADE_FUSE_TICKS = 30;     // Explosion delay 






// Dummy grenades 
const int DUMMY_GRENADE_SPAWN_MIN = 20;     // Min ticks between spawns
const int DUMMY_GRENADE_SPAWN_MAX = 100;    // Max ticks between spawns
const int DUMMY_GRENADE_LIFETIME = 30;      // How long they last
const float DUMMY_GRENADE_DANGER_RADIUS = 8.0f;  // Danger radius
const float DUMMY_GRENADE_INTENSITY = 0.9f;     



// ================== Movement ==================
const int MOVE_EVERY_N_TICKS = 5;      // Move frequency 
const int TICK_MS = 16;                // Game update interval (fixed, CPU independent)



// ================== Patrol ==================
const int PATROL_SWITCH_MIN = 80;
const int PATROL_SWITCH_MAX = 160;
const int PATROL_JITTER_RADIUS = 20;
const int SEEN_ENEMIES = 20;
const int ENEMY_INTEL_MAX_AGE = 150;   // Commander forgets enemy sightings older than this (ticks)



// ================== Vision ==================
const int FOV_RADIUS_COMMANDER = 35;
const int FOV_RADIUS_WARRIOR = 25;
const float FOV_HALF_ANGLE_DEG = 60.0f;



// ================== AI Pathfinding ==================
const float LAMBDA_SAFETY = 0.10f;     
const float SAFETY_DECAY = 0.95f;      // Safety map decay per tick (faster decay)
const int DEFEND_MAX_RADIUS = 60;      // Max search radius for cover
const double DEFEND_MAX_SAFETY = 5.0;  // Max acceptable danger




// ================== Depot Configuration ==================
const int SPAWN_MARGIN = 7;            // Spawn distance from edge
const int DEPOT_SIZE = 3;              // Depot width/height





// ===== Commander cover-hopping =====

const int  CMD_COVER_MAX_RADIUS = 100;
const double CMD_COVER_MAX_SAFETY = 0.45;
const int  CMD_COVER_DWELL_MIN = 60;
const int  CMD_COVER_DWELL_MAX = 120;
const int CMD_COVER_REPLAN_COOLDOWN = 20;


//  ================== Medic Constants ==================
const int MEDIC_STOCK_MAX = 1;


// ================== Helper Functions ==================
inline bool inBounds(int x, int y) {
    return x >= 0 && x < MSZ && y >= 0 && y < MSZ;
}

inline bool passableForAgent(int cell) {
    return cell != STONE && cell != WATER;
}

inline bool blocksLOS(int cell) {
    return cell == STONE || cell == TREE;
}

inline bool blocksFire(int cell) {
    return cell == STONE || cell == TREE;
}

inline int manhattan(Point a, Point b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

inline double euclidean(Point a, Point b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}