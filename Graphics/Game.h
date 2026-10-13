#pragma once
#include "Definitions.h"
#include "Character.h"
#include "Commander.h"
#include "Warrior.h"
#include "Medic.h"
#include "Provider.h"
#include <vector>
#include "Grenade.h"

enum class ViewMode {
    Game,
    Visibility,
    Safety
};

// Dummy grenade for safety map influence
struct DummyGrenade {
    Point pos;
    int ticksRemaining;
    float dangerRadius;
};


// Temporary visual marker (for shots or grenade impacts)
struct TempMarker {
    Point pos;
    int ttl; // ticks remaining
    char symbol; 
};


class Game {
private:
    // Map
    int map[MSZ][MSZ];
    double safety[MSZ][MSZ];

    // Teams
    std::vector<Character*> team0;
    std::vector<Character*> team1;
    std::vector<Character*> allCharacters;

    // Commanders
    Commander* commander0;
    Commander* commander1;

    // Depot locations
    Point depotAmmo0, depotMed0;
    Point depotAmmo1, depotMed1;

    // Game state
    int gameTick;
    bool gameOver;
    int winningTeam;

    // View mode
    ViewMode viewMode;

    // Dummy grenades for safety map
    std::vector<DummyGrenade> dummyGrenades;
    int nextDummyGrenadeSpawn;

    std::vector<TempMarker> tempMarkers;


public:
    Game();
    ~Game();

    void init();
    Commander* selectedCommander;
    void initMap();
    void initTeams();
    void initDepots();

    void update(float dt);
    void updateSafetyMap();
    void updateDummyGrenades();
    void spawnDummyGrenade();
    void addSafetyThreatAtPosition(Point pos, float radius, float intensity);
    void doCombat();
    void checkGameOver();

    void draw();
    Warrior* findNearestWarriorNeedingAmmo(const Provider* provider);
    std::vector<Grenade*> grenades;
    std::vector<Bullet*> bullets;
   

    // Getters
    const std::vector<Character*>& getTeam0() const { return team0; }
    const std::vector<Character*>& getTeam1() const { return team1; }

    std::vector<Character*> getAllCharacters() const {
        std::vector<Character*> aliveChars;
        for (auto* c : allCharacters) {
            if (c->isAlive()) {
                aliveChars.push_back(c);
            }
        }
        return aliveChars; 
    }

    int getTick() const { return gameTick; }

    // Helper for characters
    Point pickExplorationAnchor(int teamId);

    // View control
    void cycleViewMode();
    ViewMode getViewMode() const { return viewMode; }

    // Pause / result
    void togglePause() { if (!gameOver) paused = !paused; }
    bool isPaused() const { return paused; }
    bool isGameOver() const { return gameOver; }
    int getWinningTeam() const { return winningTeam; }

private:
    bool paused = false;
    void drawPanel();
    void drawTeamStatus(const std::vector<Character*>& team, int teamId, float top);
    void drawEndScreen();


};