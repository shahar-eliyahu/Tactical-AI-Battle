#pragma once
#include <iostream>
#include "Definitions.h"
#include <vector>
#include <queue>
#include <cmath>

bool hasLOS(int map[MSZ][MSZ], Point a, Point b);

bool hasLOF(int map[MSZ][MSZ], Point a, Point b);

void computeFOV(int map[MSZ][MSZ], Point origin, int radius, std::vector<unsigned char>& mask);

void computeFOVCone(int map[MSZ][MSZ], Point origin, float dirx, float diry,
    int radius, float halfAngleDeg, std::vector<unsigned char>& mask);

std::vector<Point> aStarPath(int map[MSZ][MSZ], double safety[MSZ][MSZ],
    Point start, Point goal, float lambdaSafety);

bool bfsFindCover(int map[MSZ][MSZ], double safety[MSZ][MSZ],
    const std::vector<Point>& knownEnemies, Point from,
    int maxRadius, double maxSafety, Point& outCover);