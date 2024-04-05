// wfc.h
//
#ifndef WFC_H
#define WFC_H 1

#include <stdbool.h>

typedef struct {
    int texU, texV;
    float rot;
    int weight;
} Tile;

extern Tile Tileset[30];

#define TileCount (sizeof Tileset / sizeof Tileset[0])
#define COORD(x,y,w) ((x) + (y) * (w))

typedef enum {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    DIRS_COUNT
} WFC_Dir;

typedef struct
{
    bool isCollapsed;
    int validTileCount;
    float sumWeights;
    bool validTiles[TileCount];
} WFC_Cell;

typedef struct
{
    int fromIdx;
    int toIdx;
    WFC_Dir dir;
} WFC_Prop;

#define MAX_PROPS 256
typedef struct
{
    bool initialized;
    // Accessing propagator:
    // d * TileCount * TileCount + tFrom * TileCount + tTo
    bool propagator[DIRS_COUNT * TileCount * TileCount];

    // Queued up propagations
    // Using a stack this time
    WFC_Prop props[MAX_PROPS];
    int propCount;

    int outputW, outputH;
    WFC_Cell* wave; 
} WFC_State;

void LoadTileset(const char* path, int tSize, int nTilesX, int nTilesY);
void UnloadTileset();
void DrawTileset();
void ExampleInputWindow(int x, int y, float scale);
void InitWFC(WFC_State* wfc);
void DestroyWFC(WFC_State* wfc);
void AdvanceWFC(WFC_State* wfc);
void FullWFC(WFC_State* wfc);
void DrawWFC(WFC_State* wfc, int x, int y, float scale);
#endif // WFC_H
