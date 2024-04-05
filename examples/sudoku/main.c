#include "raylib.h"
/* #include "wfc.h" */
#define WFC_METRICS
#include "wfc_heuristic_v2.h"
#include <assert.h>
#include <time.h>
typedef enum { SameQuadrant, SameColumn, SameLine } SudokuRel;
static Vector2 sudokuSize = {9, 9};

int SudokuRelationship(WFC_State* wfc, int sCellIdx, int dCellIdx)
{
    int sCellY = sCellIdx / sudokuSize.x;
    int sCellX = sCellIdx - sCellY * sudokuSize.x;

    int dCellY = dCellIdx / sudokuSize.x;
    int dCellX = dCellIdx - dCellY * sudokuSize.x;

    if (sCellX / 3 == dCellX / 3 && sCellY / 3 == dCellY / 3)
        return SameQuadrant;
    else if (sCellX == dCellX)
        return SameColumn;
    else if (sCellY == dCellY)
        return SameLine;
    else
        return -1;
}

// Hardest puzzle from https://sandiway.arizona.edu/sudoku/examples.html
#define IDX(x, y) ((x) + (y) * 9)
void InitialState(WFC_State* wfc)
{
    WFC_SetTileTo(wfc, IDX(1,0), 2 -1);
    WFC_SetTileTo(wfc, IDX(3,1), 6 -1);
    WFC_SetTileTo(wfc, IDX(8,1), 3 -1);
    WFC_SetTileTo(wfc, IDX(1,2), 7 -1);
    WFC_SetTileTo(wfc, IDX(2,2), 4 -1);
    WFC_SetTileTo(wfc, IDX(4,2), 8 -1);
    WFC_SetTileTo(wfc, IDX(5,3), 3 -1);
    WFC_SetTileTo(wfc, IDX(8,3), 2 -1);
    WFC_SetTileTo(wfc, IDX(1,4), 8 -1);
    WFC_SetTileTo(wfc, IDX(4,4), 4 -1);
    WFC_SetTileTo(wfc, IDX(7,4), 1 -1);
    WFC_SetTileTo(wfc, IDX(0,5), 6 -1);
    WFC_SetTileTo(wfc, IDX(3,5), 5 -1);
    WFC_SetTileTo(wfc, IDX(4,6), 1 -1);
    WFC_SetTileTo(wfc, IDX(6,6), 7 -1);
    WFC_SetTileTo(wfc, IDX(7,6), 8 -1);
    WFC_SetTileTo(wfc, IDX(0,7), 5 -1);
    WFC_SetTileTo(wfc, IDX(5,7), 9 -1);
    WFC_SetTileTo(wfc, IDX(7,8), 4 -1);
}

void DrawSudoku(WFC_State* wfc, int x, int y, float scale);

int main(void)
{
    InitWindow(800, 450, "WFC Example 1 - Sudoku Solver");

    SetTargetFPS(60);

    Tile tileset[9] = {
        {0,1},
        {1,1},
        {2,1},
        {3,1},
        {4,1},
        {5,1},
        {6,1},
        {7,1},
        {8,1},
    };

    WFC_State wfc = {0};
    WFC_Init(&wfc, tileset, 9, 3);

    if (!wfc.initialized)
    {
        TraceLog(LOG_ERROR, "Failed to initialize WFC!");
        CloseWindow();
        return 1;
    }

    for (int i = 0; i < 9 * 9; i++)
    {
        WFC_AddCell(&wfc);
    }

    // Set up relationships
    for (int i = 0; i < wfc.tileCount; i++)
    {
        for (int j = 0; j < wfc.tileCount; j++)
        {
            // Allow equal numbers on the same quadrant, line or column only if they're different
            WFC_SetRule(&wfc, tileset[i], tileset[j], SameQuadrant, i != j);
            WFC_SetRule(&wfc, tileset[i], tileset[j], SameLine    , i != j);
            WFC_SetRule(&wfc, tileset[i], tileset[j], SameColumn  , i != j);
        }
    }

    WFC_CalculateNeighbors(&wfc, SudokuRelationship);
    InitialState(&wfc);

    TraceLog(LOG_INFO, "WFC initialized successfully!");

    int autostep = 0;
    float timer = 0.0, stepDelay = 0.012;
    
    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            if (wfc.isFinished)
            {
                TraceLog(LOG_INFO, "Resetting sudoku.");
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

        BeginDrawing();
            ClearBackground(RAYWHITE);

            DrawSudoku(&wfc, 0, 0, 2);
        EndDrawing();

        timer += GetFrameTime();
    }

    WFC_CleanUp(&wfc);
    CloseWindow();

    return 0;
}

void DrawSudoku(WFC_State* wfc, int x, int y, float scale)
{
    assert(wfc != NULL);
    if (!wfc->initialized)
        return;

    static Vector2 tileSize = {16, 16};
    static Color tileCols[9] = {
        RED,
        BLUE,
        GREEN,
        ORANGE,
        PURPLE,
        YELLOW,
        BROWN,
        PINK,
        SKYBLUE
    };

    Rectangle bounds = {
        x, y,
        sudokuSize.x * tileSize.x * scale,
        sudokuSize.y * tileSize.y * scale
    };

    for (int i = 0; i < sudokuSize.x * sudokuSize.y; i++)
    {
        int ty = i / sudokuSize.x;
        int tx = i - ty * sudokuSize.x;

        WFC_Cell* cell = &wfc->wave[i];
        Rectangle drawBounds = {
            x + tx * tileSize.x * scale + (tx / 3) * 1.5,
            y + ty * tileSize.y * scale + (ty / 3) * 1.5,
            tileSize.x * scale, tileSize.y * scale
        };

        Color drawCol = BLACK;

        if (cell->isCollapsed)
            drawCol = tileCols[cell->collapsedTile];
        else
        {
            // Spaces with less number obtions are brighter.
            drawCol.r = 255 * (wfc->tileCount - (float) cell->validTileCount / wfc->tileCount);
            drawCol.g = 255 * (wfc->tileCount - (float) cell->validTileCount / wfc->tileCount);
            drawCol.b = 255 * (wfc->tileCount - (float) cell->validTileCount / wfc->tileCount);

            /* int r = 0, g = 0, b = 0; */
            /* for (int t = 0; t < wfc->tileCount; t++) */
            /* { */
            /*     if (cell->validTiles[t] == false) continue; */
            /*     Tile* tile = &wfc->tileset[t]; */
            /*     r += tileCols[tile->val].r; */
            /*     g += tileCols[tile->val].g; */
            /*     b += tileCols[tile->val].b; */
            /* } */

            /* drawCol.r = r / cell->validTileCount; */
            /* drawCol.g = g / cell->validTileCount; */
            /* drawCol.b = b / cell->validTileCount; */
        }

        DrawRectangleRec(drawBounds, drawCol);
        if (cell->isCollapsed)
        {
            DrawText(TextFormat("%d", cell->collapsedTile), drawBounds.x + 5, drawBounds.y + 5, 10*scale, BLACK);
        }

        DrawRectangleLinesEx(drawBounds, 2, BLACK);

        if (CheckCollisionPointRec(GetMousePosition(), drawBounds))
        {
            DrawRectangleLinesEx(drawBounds, 5, WHITE);
            DrawText(
                TextFormat("Cell %d (%d, %d): valid_tiles: %d",
                           cell->idx, tx, ty, cell->validTileCount),
                x, y + 420, 20, BLACK);
        }
    }
    
}
