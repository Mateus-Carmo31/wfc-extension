#include "raylib.h"
#include <cstdlib>
#include <ctime>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#define WFC_METRICS
#include "wfc_heuristic_v2.h"

const int screenWidth = 800, screenHeight = 450;
const float padding = 50;
int layerCount = 7;

std::vector<Vector2> points;
std::vector<std::vector<int>> layers;
std::set<std::pair<int, int>> neighbors;

WFC_State wfc {};
Tile tiles[6] = {
    {0, 1},
    {1, 1},
    {2, 1},
    {3, 1},
    {4, 1},
    {5, 1},
};
struct TileData {
    std::string name;
    Texture tex;
} tileset[6] = {
    {"unknown"},
    {"enemy"},
    {"chest"},
    {"key"},
    {"bedroll"},
    {"door"},
};

void GeneratePoints();
void StartWFC();

int main(void)
{
    InitWindow(800, 450, "WFC Example 4 - Graph");

    SetTargetFPS(60);

    const float svgSize = 128;
    for (auto& tile : tileset)
    {
        Image img = LoadImageSvg(TextFormat("../assets/%s-icon.svg", tile.name.c_str()), svgSize, svgSize);
        tile.tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    GeneratePoints();
    StartWFC();

    bool autostep = false;
    float timer, stepDelay = 0.1f;
    timer = stepDelay;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            if (wfc.isFinished)
            {
                TraceLog(LOG_INFO, "Resetting wave.");
                WFC_Reset(&wfc);
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
            if (wfc.isFinished)
            {
                WFC_Reset(&wfc);
            }
            WFC_Run(&wfc);

            TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", wfc.totalIterations, wfc.totalObservations, wfc.totalPropagations, wfc.totalResets));
            autostep = false;
            timer = stepDelay;
        }

        if (IsKeyPressed(KEY_S))
        {
            if (wfc.isFinished)
            {
                WFC_Reset(&wfc);
            }
            
            WFC_DoStep(&wfc);

            if (wfc.isFinished)
            {
                TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", wfc.totalIterations, wfc.totalObservations, wfc.totalPropagations, wfc.totalResets));
            }
            autostep = false;
            timer = stepDelay;
        }

        if (autostep)
        {
            timer = timer - GetFrameTime();
            if (timer <= 0.0)
            {
                WFC_DoStep(&wfc);

                if (wfc.isFinished)
                {
                    TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", wfc.totalIterations, wfc.totalObservations, wfc.totalPropagations, wfc.totalResets));
                    autostep = false;
                    timer = stepDelay;
                }
            }
        }

        if (IsKeyPressed(KEY_R))
        {
            GeneratePoints();
            StartWFC();
            autostep = false;
        }

        BeginDrawing();
        ClearBackground(WHITE);
        for (auto& neighbor : neighbors)
        {
            // std::cout << "Neighbors: " << neighbor.first << " and " << neighbor.second << '\n';
            DrawLineV(points[neighbor.first], points[neighbor.second], BLACK);
        }
        for (int i = 0; i < points.size(); i++)
        {
            if (CheckCollisionPointCircle(GetMousePosition(), points[i], 15))
            {
                std::stringstream desc {};
                desc << "Cell " << i << ": Neighbors: " << wfc.wave[i].neighborCount;
                desc << " - Valid Tiles: { ";
                for (int t = 0; t < wfc.tileCount; t++)
                {
                    if (wfc.wave[i].validTiles[t])
                        desc << tileset[t].name << ", ";
                }
                desc << " } - Collapsed? " << wfc.wave[i].isCollapsed;
                // DrawText(TextFormat("Cell %d: Neighbors: %d - Valid Tiles: %d - Collapsed? %s", i, wfc.wave[i].neighborCount, wfc.wave[i].validTileCount, 
                                    // wfc.wave[i].isCollapsed ? "Yes" : "No"), 20, screenHeight-20, 10, BLACK);
                std::string descStr = desc.str();
                DrawText(descStr.c_str(), 20, screenHeight-20, 10, BLACK);

                for (int n = 0; n < wfc.wave[i].neighborCount; n++)
                {
                    // std::cout << wfc.wave[i].neighborCount << '\n';
                    DrawCircleLinesV(points[wfc.wave[i].neighbors[n].idx], 24, BLUE);
                }
            }
            if (wfc.wave[i].isCollapsed)
                DrawTexturePro(
                    tileset[wfc.wave[i].collapsedTile].tex,
                    {0, 0, svgSize, svgSize},
                    {points[i].x, points[i].y, svgSize * 0.3f, svgSize * 0.3f},
                    {svgSize * 0.3f/2,svgSize * 0.3f/2},
                    0,
                    WHITE
                    );
            else
                DrawCircleV(points[i], svgSize * 0.3f/2, BLACK);
            // DrawCircleV(points[i], 10, tileColors[wfc.wave[i].collapsedTile]);
        }
        EndDrawing();
    }

    for (auto& tile : tileset)
    {
        UnloadTexture(tile.tex);
    }

    WFC_CleanUp(&wfc);

    CloseWindow();
    return 0;
}

void GeneratePoints()
{
    points.clear();
    layers.clear();
    neighbors.clear();
    
    int layerSpacing = (screenWidth - padding) / (layerCount-1);
    for (int i = 0; i < layerCount; i++)
    {
        layers.emplace_back();
        // int pointsInLayer = i < layerCount/2 ? i+1 : layerCount - i;
        int pointsInLayer = i+1;
        float pointSpacing = (screenHeight - padding) / (pointsInLayer+1);
        for (int l = 1; l <= pointsInLayer; l++)
        {
            layers[i].push_back(points.size());
            points.push_back({padding / 2 + layerSpacing * i, padding + pointSpacing * l});
        }
    }

    for (int i = 0; i < layers.size()-1; i++)
    {
        for (int p = 0; p < layers[i].size(); p++)
        {
            if (layers[i].size() == layers[i+1].size())
            {
                neighbors.insert({layers[i][p], layers[i+1][p]});
            }
            else if (layers[i].size() >= layers[i+1].size())
            {
                if (p == 0)
                {
                    neighbors.insert({layers[i][p], layers[i+1][p]});
                }
                else if (p == layers[i].size()-1)
                {
                    neighbors.insert({layers[i][p], layers[i+1][p-1]});
                }
                else
                {
                    neighbors.insert({layers[i][p], layers[i+1][p-1]});
                    neighbors.insert({layers[i][p], layers[i+1][p]});
                }
            }
            else
            {
                neighbors.insert({layers[i][p], layers[i+1][p]});
                neighbors.insert({layers[i][p], layers[i+1][p+1]});
            }
        }
    }

    // Adds some random jitter to points to make it more distinct
    float jitter = 30;
    float mean_y = 0;
    for (auto& p : points)
    {
        p.x += ((float) rand() / RAND_MAX) * jitter - jitter/2;
        if (p.x < padding)
            p.x = padding;
        if (p.x > screenWidth-padding)
            p.x = screenWidth-padding;
        
        p.y += ((float) rand() / RAND_MAX) * jitter - jitter/2;
        if (p.y < padding)
            p.y = padding;
        if (p.y > screenHeight-padding)
            p.y = screenHeight-padding;

        mean_y += p.y;
    }
    mean_y /= points.size();

    for (auto& p : points)
    {
        p.y -= mean_y - screenHeight/2.0;
    }
}

void StartWFC()
{
    if (wfc.initialized)
        WFC_CleanUp(&wfc);

    WFC_Init(&wfc, tiles, 6, 1);

    // Add points
    for (auto& p : points)
    {
        WFC_AddCell(&wfc);
    }

    // Connect each point to the point in the neighboring grid cell
    for (auto neighbor : neighbors)
    {
        WFC_AddNeighbor(&wfc, neighbor.first, neighbor.second, 0);
    }

    // WFC_SetRule(&wfc, tiles[0], tiles[0], 0, true);
    // WFC_SetRule(&wfc, tiles[0], tiles[1], 0, true);
    // WFC_SetRule(&wfc, tiles[0], tiles[2], 0, true);

    // WFC_SetRule(&wfc, tiles[1], tiles[0], 0, true);

    // WFC_SetRule(&wfc, tiles[2], tiles[0], 0, true);

    // "Unknown" rules. Allow everything except chests and doors
    WFC_SetRule(&wfc, tiles[0], tiles[0], 0, true);
    WFC_SetRule(&wfc, tiles[0], tiles[1], 0, true);
    WFC_SetRule(&wfc, tiles[0], tiles[3], 0, true);
    WFC_SetRule(&wfc, tiles[0], tiles[4], 0, true);

    // "Enemy" rules. Allow everything except chests and doors
    WFC_SetRule(&wfc, tiles[1], tiles[0], 0, true); // Unknown
    WFC_SetRule(&wfc, tiles[1], tiles[1], 0, true); // Enemy
    WFC_SetRule(&wfc, tiles[1], tiles[3], 0, true); // Key
    WFC_SetRule(&wfc, tiles[1], tiles[4], 0, true); // Chest

    // "Chest" rules. Allow everything except keys, chests and doors
    WFC_SetRule(&wfc, tiles[2], tiles[0], 0, true);
    WFC_SetRule(&wfc, tiles[2], tiles[1], 0, true);
    WFC_SetRule(&wfc, tiles[2], tiles[4], 0, true);

    // "Key" rules. Only allow chests or doors
    WFC_SetRule(&wfc, tiles[3], tiles[2], 0, true);
    WFC_SetRule(&wfc, tiles[3], tiles[5], 0, true);

    // "Bedroll" rules. Only allow unknowns, enemies and keys
    WFC_SetRule(&wfc, tiles[4], tiles[0], 0, true);
    WFC_SetRule(&wfc, tiles[4], tiles[1], 0, true);
    WFC_SetRule(&wfc, tiles[4], tiles[3], 0, true);

    // "Door" rules. Only allow unknowns, enemies and bedrolls
    WFC_SetRule(&wfc, tiles[5], tiles[0], 0, true);
    WFC_SetRule(&wfc, tiles[5], tiles[1], 0, true);
    WFC_SetRule(&wfc, tiles[5], tiles[4], 0, true);

    WFC_SetTileTo(&wfc, 0, 5);
}
