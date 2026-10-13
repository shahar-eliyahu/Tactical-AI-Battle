#include "AI.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

//  Line of Sight 
bool hasLOS(int map[MSZ][MSZ], Point a, Point b) {
    int x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (!inBounds(x0, y0)) return false;
        if (x0 == x1 && y0 == y1) return true;

        // Don't check origin cell
        if (!(x0 == a.x && y0 == a.y) && blocksLOS(map[y0][x0]))
            return false;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

//  Line of Fire 
bool hasLOF(int map[MSZ][MSZ], Point a, Point b) {
    int x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (!inBounds(x0, y0)) return false;
        if (x0 == x1 && y0 == y1) return true;

        // Don't check origin cell
        if (!(x0 == a.x && y0 == a.y) && blocksFire(map[y0][x0]))
            return false;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

//  Field of View 
void computeFOV(int map[MSZ][MSZ], Point origin, int radius, std::vector<unsigned char>& mask) {
    mask.assign(MSZ * MSZ, 0);

    int x0 = std::max(0, origin.x - radius);
    int x1 = std::min(MSZ - 1, origin.x + radius);
    int y0 = std::max(0, origin.y - radius);
    int y1 = std::min(MSZ - 1, origin.y + radius);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (hasLOS(map, origin, { x, y })) {
                mask[y * MSZ + x] = 1;
            }
        }
    }
}

//  Field of View (Cone) 
void computeFOVCone(int map[MSZ][MSZ], Point origin, float dirx, float diry,
    int radius, float halfAngleDeg, std::vector<unsigned char>& mask) {
    mask.assign(MSZ * MSZ, 0);

    // Normalize direction
    float len = std::sqrt(dirx * dirx + diry * diry);
    if (len < 1e-6f) { dirx = 1.0f; diry = 0.0f; len = 1.0f; }
    dirx /= len; diry /= len;

    float cosThresh = std::cos(halfAngleDeg * float(PI) / 180.0f);

    int x0 = std::max(0, origin.x - radius);
    int x1 = std::min(MSZ - 1, origin.x + radius);
    int y0 = std::max(0, origin.y - radius);
    int y1 = std::min(MSZ - 1, origin.y + radius);

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int dx = x - origin.x;
            int dy = y - origin.y;

            if (dx * dx + dy * dy > radius * radius) continue;

            float vlen = std::sqrt(float(dx * dx + dy * dy));
            if (vlen < 1e-6f) {
                mask[y * MSZ + x] = 1;
                continue;
            }

            float vx = dx / vlen, vy = dy / vlen;
            float dot = vx * dirx + vy * diry;

            if (dot < cosThresh) continue;  // Outside cone
            if (!hasLOS(map, origin, { x, y })) continue;

            mask[y * MSZ + x] = 1;
        }
    }
}

//  A* Pathfinding 
struct NodeRec {
    Point p;
    float g = 0, f = 0;
    NodeRec(Point p, float g, float h) : p(p), g(g), f(h) {}
};

struct NodeCmp {
    bool operator()(const NodeRec& a, const NodeRec& b) const {
        return a.f > b.f;
    }
};

inline float hCost(Point a, Point b) {
    return float(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

static std::vector<Point> reconstruct(Point cur, const std::vector<int>& cameFrom) {
    std::vector<Point> path;
    int idx = cur.y * MSZ + cur.x;
    path.push_back(cur);

    while (cameFrom[idx] != -1) {
        idx = cameFrom[idx];
        path.push_back({ idx % MSZ, idx / MSZ });
    }

    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Point> aStarPath(int map[MSZ][MSZ], double safety[MSZ][MSZ],
    Point start, Point goal, float lambdaSafety) {
    std::priority_queue<NodeRec, std::vector<NodeRec>, NodeCmp> open;
    std::vector<uint8_t> closed(MSZ * MSZ, 0);
    std::vector<int> cameFrom(MSZ * MSZ, -1);
    std::vector<float> gScore(MSZ * MSZ, 1e9f);

    auto idx = [](Point p) { return p.y * MSZ + p.x; };

    gScore[idx(start)] = 0.f;
    open.push({ start, 0.f, hCost(start, goal) });

    const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    while (!open.empty()) {
        NodeRec cur = open.top();
        open.pop();

        if (cur.p == goal) return reconstruct(cur.p, cameFrom);
        if (closed[idx(cur.p)]) continue;
        closed[idx(cur.p)] = 1;

        for (auto& d : dirs) {
            Point nb{ cur.p.x + d[0], cur.p.y + d[1] };

            if (!inBounds(nb.x, nb.y)) continue;
            if (!passableForAgent(map[nb.y][nb.x])) continue;

            float step = 1.0f + lambdaSafety * float(safety[nb.y][nb.x]);
            float tentative = gScore[idx(cur.p)] + step;

            if (tentative < gScore[idx(nb)]) {
                gScore[idx(nb)] = tentative;
                cameFrom[idx(nb)] = idx(cur.p);
                float f = tentative + hCost(nb, goal);
                open.push({ nb, tentative, f });
            }
        }
    }

    return {};  
}

// BFS Cover Search 
static bool isCoveredFromEnemies(int map[MSZ][MSZ], Point cell,
    const std::vector<Point>& enemies) {
    for (const auto& e : enemies) {
        if (hasLOS(map, e, cell)) return false;
    }
    return true;
}


bool bfsFindCover(int map[MSZ][MSZ], double safety[MSZ][MSZ],
    const std::vector<Point>& knownEnemies, Point from,
    int maxRadius, double maxSafety, Point& outCover) {
    std::queue<Point> q;
    std::vector<uint8_t> vis(MSZ * MSZ, 0);
    auto idx = [](Point p) { return p.y * MSZ + p.x; };

    q.push(from);
    vis[idx(from)] = 1;

    const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

    while (!q.empty()) {
        Point cur = q.front();
        q.pop();

        int dist = manhattan(cur, from);
        if (dist > maxRadius) continue;

        // Check if this is good cover
        if (passableForAgent(map[cur.y][cur.x]) &&
            safety[cur.y][cur.x] <= maxSafety &&
            isCoveredFromEnemies(map, cur, knownEnemies)) {
            outCover = cur;
            return true;
        }

        for (auto& d : dirs) {
            Point nb{ cur.x + d[0], cur.y + d[1] };

            if (!inBounds(nb.x, nb.y)) continue;
            if (vis[idx(nb)]) continue;

            vis[idx(nb)] = 1;
            q.push(nb);
        }
    }

    return false;  // No cover found
}




