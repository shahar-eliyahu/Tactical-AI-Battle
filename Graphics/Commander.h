#pragma once
#include "Character.h"
#include "Definitions.h"
#include <vector>


enum class CommanderState {
    IDLE,
    PLANNING,
    ATTACKING,
    DEFENDING,
    SURRENDERED,
    WOUNDED 
};

class Warrior;
class Medic;
class Provider;

class Commander : public Character {
public:
    CommanderState state;

    // Team references (set by Game)
    std::vector<Warrior*> warriors;
    Medic* medic;
    Provider* provider;

    //  Cover-hopping state 
    std::vector<Point> coverPath; 
    bool  hasCoverTarget = false; 
    Point coverTarget = { 0, 0 }; 
    int   coverWaitUntil = 0;     
    int   coverReplanTick = 0;    

    // Intelligence
    std::vector<Point> knownEnemyPositions;
    std::vector<int> knownEnemyTicks;   // when each position was last reported

    // Returns a copy of known enemy positions
    std::vector<Point> getKnownEnemyPositions() const { return knownEnemyPositions; }

    std::vector<unsigned char> combinedFOV;
    int lastEnemySpottedTick;

    // Decision making
    int ticksSinceLastOrder;
    Point attackTarget = { 0, 0 };
    bool hasAttackTarget;

    // Wounded state
    bool reportedWounded;

    Commander(Point p, int team);

    void update(float dt, int map[MSZ][MSZ], double safety[MSZ][MSZ]) override;

    // Reporting interface (called by team members)
    void reportEnemySpotted(Point pos);
    void reportWounded(Character* c);
    void reportNeedAmmo(Warrior* w);

private:
    bool hasRetreatPath = false;
    std::vector<Point> retreatPath;

    std::vector<Point> usedCovers;        
    int coverResetCounter = 0;            
    static const int MAX_COVER_HISTORY = 8;  

    bool isImmediateThreat(int map[MSZ][MSZ], const std::vector<Character*>& enemies) const;


    void updateCombinedFOV(int map[MSZ][MSZ]);
    void makeStrategicDecision(int map[MSZ][MSZ], double safety[MSZ][MSZ]);

    Point findBestAttackPosition();
    int countAliveWarriors();
    int countAliveEnemies();

    bool isAloneInTeam();

    bool pickNextCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]);

    bool pickNearestCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]);

    bool pickBestRetreatCover(int map[MSZ][MSZ], double safety[MSZ][MSZ]);

    bool wasAlreadyUsed(Point p);         
    void markCoverAsUsed(Point p);        
};