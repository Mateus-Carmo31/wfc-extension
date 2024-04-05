#include "raylib.h"
#include <ctime>
#include <math.h>
#include <set>
#include <unordered_map>
#include <vector>

#define WFC_METRICS
#include "wfc_heuristic_v2.h"
#include "tileset.hpp"

class Map {
public:
    struct Region {
        Color color;
    };

    struct GridCell {
        int tile;
        int region;
    };

    Map(int width, int height, const char* tilesetTomlPath): width(width), height(height), tileset(tilesetTomlPath) {
        data.resize(width * height, {0, -1});
    }
    ~Map();

    void GenerateRandomRegions(int regionNumber);
    void StartWFC();
    void Generate();
    void Step();
    void Draw(float x, float y, float scale, bool drawRegions);
    WFC_State wfc {};
private:
    // Tile tileset[3] = {{0, 1}, {1, 1}, {2, 1}};
    Tileset tileset;

    std::vector<Region> regions;
    std::vector<GridCell> data;
    int width, height;
};

int main(void)
{
    InitWindow(640, 480, "WFC Example 5 - Regions");

    SetTargetFPS(60);

    // Tileset tset { "../assets/tilemap_battle.toml" };

    Map* map = new Map (20, 16, "../assets/tilemap_battle.toml");
    map->GenerateRandomRegions(4);
    map->StartWFC();

    bool autostep = false;
    bool drawRegions = true;
    float timer, stepDelay = 0.01;
    timer = stepDelay;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_E))
        {
            drawRegions = !drawRegions;
        }

        if (IsKeyPressed(KEY_R))
        {
            delete map;
            map = new Map (20, 16, "../assets/tilemap_battle.toml");
            map->GenerateRandomRegions(4);
            map->StartWFC();
        }

        if (IsKeyPressed(KEY_SPACE))
        {
            if (map->wfc.isFinished)
            {
                TraceLog(LOG_INFO, "Resetting example.");
                WFC_Reset(&map->wfc);
                autostep = true;
                timer = stepDelay;
            }
            else
            {
                autostep = !autostep;
                TraceLog(LOG_INFO, TextFormat("Setting autostep to %d.", autostep));
                timer = stepDelay;
            }
        }

        if (IsKeyPressed(KEY_ENTER))
        {
            if (map->wfc.isFinished)
            {
                WFC_Reset(&map->wfc);
            }
            map->Generate();

            TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", map->wfc.totalIterations, map->wfc.totalObservations, map->wfc.totalPropagations, map->wfc.totalResets));
            autostep = false;
            timer = stepDelay;
        }

        if (IsKeyPressed(KEY_S))
        {
            if (map->wfc.isFinished)
                WFC_Reset(&map->wfc);

            map->Step();

            if (map->wfc.isFinished)
            {
                TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", map->wfc.totalIterations, map->wfc.totalObservations, map->wfc.totalPropagations, map->wfc.totalResets));
            }
            autostep = false;
            timer = stepDelay;
        }

        if (autostep)
        {
            timer = timer - GetFrameTime();
            if (timer <= 0.0)
            {
                map->Step();

                if (map->wfc.isFinished)
                {
                    TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", map->wfc.totalIterations, map->wfc.totalObservations, map->wfc.totalPropagations, map->wfc.totalResets));
                    autostep = false;
                    timer = stepDelay;
                }
            }
        }

        BeginDrawing();
            ClearBackground(ColorFromNormalized({132/255., 198/255., 105/255., 1}));
            map->Draw(50, 10, 1.5, drawRegions);
        EndDrawing();

        timer += GetFrameTime();
    }

    delete map;

    CloseWindow();
}

void Map::GenerateRandomRegions(int regionNumber)
{
    SetRandomSeed(time(NULL));

    regions.resize(regionNumber);

    std::vector<std::set<int>> regionGroups;
    regionGroups.resize(regionNumber);

    std::unordered_map<int, int> tileToRegion;

    auto whatRegionIsTile = [&tileToRegion](int tile) -> int {
        auto it = tileToRegion.find(tile);
        if (it == tileToRegion.end())
        {
            return -1;
        }
        return it->second;
    };

    for (int i = 0; i < regions.size(); i++)
    {
        int randomPoint = GetRandomValue(0, width * height - 1);
        tileToRegion.insert({randomPoint, i});
        regionGroups[i].insert(randomPoint);
        regions[i].color = ColorFromHSV(360.0f * i / regions.size(), 1.0f, 1.0f);
    }

    while(1)
    {
        bool expansionHappened = false;
        for (int regIdx = 0; regIdx < regions.size(); regIdx++)
        {
            std::set<int> toAdd;
            for (const auto tIdx : regionGroups[regIdx])
            {
                int tx = tIdx % width;
                int ty = tIdx / width;

                if (tx > 0 && whatRegionIsTile(tIdx - 1) == -1)
                {
                    tileToRegion.insert({tIdx - 1, regIdx});
                    toAdd.insert(tIdx - 1);
                }
                
                if (tx < width - 1 && whatRegionIsTile(tIdx + 1) == -1)
                {
                    tileToRegion.insert({tIdx + 1, regIdx});
                    toAdd.insert(tIdx + 1);
                }

                if (ty > 0 && whatRegionIsTile(tIdx - width) == -1)
                {
                    tileToRegion.insert({tIdx - width, regIdx});
                    toAdd.insert(tIdx - width);
                }

                if (ty < height - 1 && whatRegionIsTile(tIdx + width) == -1)
                {
                    tileToRegion.insert({tIdx + width, regIdx});
                    toAdd.insert(tIdx + width);
                }
            }

            if (!toAdd.empty())
            {
                expansionHappened = true;
                for (const auto tile : toAdd)
                    regionGroups[regIdx].insert(tile);
            }
        }

        if (!expansionHappened)
            break;
    }

    for (const auto& tileRegionPair : tileToRegion)
    {
        data[tileRegionPair.first].region = tileRegionPair.second;
    }
}

void Map::StartWFC() {
    using rel = Tileset::Relationships;
    
    WFC_Init(&wfc, tileset.GetWFCTiles(), tileset.GetTileCount(), rel::OTHER_REGION+1);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = WFC_AddCell(&wfc);
            int reg = data[idx].region;

            if (x > 0)
                WFC_AddNeighbor(&wfc, idx, idx - 1, rel::LEFT);
            
            if (y > 0)
                WFC_AddNeighbor(&wfc, idx, idx - width, rel::UP);
            
            if (x < width - 1)
                WFC_AddNeighbor(&wfc, idx, idx + 1, rel::RIGHT);

            if (y < height - 1)
                WFC_AddNeighbor(&wfc, idx, idx + width, rel::DOWN);
        }
    }

    for (int i1 = 0; i1 < width * height; i1++)
    {
        for (int i2 = i1 + 1; i2 < width * height; i2++)
        {
            if (data[i1].region == data[i2].region)
            {
                // if (i1 + 1 != i2 && i1 - 1 != i2 && i1 + width != i2 && i1 - width != i2)
                // {
                    WFC_AddNeighbor(&wfc, i1, i2, rel::SAME_REGION);
                    WFC_AddNeighbor(&wfc, i2, i1, rel::SAME_REGION);
                // }
            }
            else
            {
                WFC_AddNeighbor(&wfc, i1, i2, rel::OTHER_REGION);
                WFC_AddNeighbor(&wfc, i2, i1, rel::OTHER_REGION);
            }
        }
    }

    tileset.ConfigureWFC(&wfc);
}

void Map::Step()
{
    WFC_DoStep(&wfc);
}

void Map::Generate()
{
    WFC_Run(&wfc);

    for (int i = 0; i < data.size(); i++)
    {
        data[i].tile = wfc.wave[i].collapsedTile;
    }
}

void Map::Draw(float x, float y, float scale, bool drawRegions)
{
    const float tileSize = tileset.tileSize;
    Rectangle bounds { x, y, tileSize * width * scale, tileSize * height * scale};

    for (int tile = 0; tile < width * height; tile++)
    {
        int tx = tile % width;
        int ty = tile / width;
        WFC_Cell& cell = wfc.wave[tile];

        Rectangle tileBounds { x + tx * tileSize * scale, y + ty * tileSize * scale, tileSize * scale, tileSize * scale};

        if (cell.collapsedTile >= 0)
        {
            tileset.DrawTile(cell.collapsedTile, tileBounds.x, tileBounds.y, tileBounds.width);
        }
        else
        {
            DrawRectangleRec(tileBounds, GRAY);
        }


        if (CheckCollisionPointRec(GetMousePosition(), tileBounds))
        {
            DrawText(TextFormat("Tile %d: %d neighbors, %d valid tiles, entropy: %3f, collapsed to %d", cell.idx, cell.neighborCount, cell.validTileCount, log(cell.sumWeights) - cell.weightLogWeightSum / cell.sumWeights, cell.collapsedTile),
                     100, 400, 10, BLACK);
            if (IsKeyPressed(KEY_F))
            {
                for (int t = 0; t < wfc.tileCount; t++)
                {
                    if (!cell.validTiles[t]) continue;

                    TraceLog(LOG_INFO, TextFormat("%s", tileset.tiles[t].name.c_str()));
                }
            }
        }

        if (drawRegions)
            DrawRectangleLinesEx(tileBounds, 2, regions[data[tile].region].color);
    }

    DrawRectangleLinesEx(bounds, 2, BLACK);
}

Map::~Map()
{
    if (wfc.initialized)
        WFC_CleanUp(&wfc);
}
