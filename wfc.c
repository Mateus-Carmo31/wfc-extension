#include "wfc.h"
#include "raylib.h"
#include "raymath.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

Tile Tileset[30];
Texture tilesetTex;
int tileNum = 0;
Vector2 tileSize = {0,0};

// =========== TILESET ============

void LoadTileset(const char* path, int tSize, int nTilesX, int nTilesY)
{
    assert(path != NULL);
    assert(tSize > 0 && nTilesX > 0 && nTilesY > 0);

    tilesetTex = LoadTexture(path);

    for (int i = 0; i < nTilesX * nTilesY; i++)
    {
	int y = i / nTilesX;
	int x = i - y * nTilesX;
	Tileset[i] = (Tile) {x,y,0,0};
    }

    tileNum = nTilesX * nTilesY;
    tileSize.x = tileSize.y = tSize;
}

void UnloadTileset()
{
    assert(tilesetTex.id > 0);
    UnloadTexture(tilesetTex);
    tileNum = 0;
    tileSize.x = tileSize.y = 0;
}

void DrawTileset()
{
    for (int i = 0; i < tileNum; i++)
    {
	DrawTexturePro(
	    tilesetTex,
	    (Rectangle) {
		Tileset[i].texU * tileSize.x, Tileset[i].texV * tileSize.y,
		tileSize.x, tileSize.y
	    },
	    (Rectangle) {
		Tileset[i].texU * tileSize.x, Tileset[i].texV * tileSize.y,
		tileSize.x, tileSize.y
	    },
	    (Vector2) {0},
	    0,
	    WHITE);
    }
}

// ============ WFC ==============

#define INPUT_SIZE 10
int inputExample[INPUT_SIZE * INPUT_SIZE] = {0};
int selectedBrushTile = -1;

void ExampleInputWindow(int x, int y, float scale)
{
    assert(tilesetTex.id > 0);

    // === Draw Drawing Area ===
    DrawRectangleLinesEx(
	(Rectangle) {x, y, tileSize.x * scale * INPUT_SIZE, tileSize.y * scale * INPUT_SIZE},
	2, BLACK
    );

    Vector2 mousePos = GetMousePosition();
    int mX = (int) (mousePos.x - x) / (tileSize.x * scale);
    int mY = (int) (mousePos.y - y) / (tileSize.y * scale);
    /* TraceLog(LOG_INFO, TextFormat("(%d,%d)", mX, mY)); */
    int mouseIdx = COORD(mX,mY,INPUT_SIZE);

    for (int i = 0; i < INPUT_SIZE * INPUT_SIZE; i++)
    {
	Tile* t = &Tileset[inputExample[i]];
	int tY = i / INPUT_SIZE;
	int tX = i - INPUT_SIZE * tY;
	Rectangle drawSpace = (Rectangle) {
		x + tX * tileSize.x * scale, y + tY * tileSize.y * scale,
		tileSize.x * scale, tileSize.y * scale
	};

	DrawTexturePro(
	    tilesetTex,
	    (Rectangle) {
		t->texU * tileSize.x, t->texV * tileSize.y,
		tileSize.x, tileSize.y
	    },
	    drawSpace,
	    (Vector2) {0},
	    t->rot,
	    WHITE);

	if (CheckCollisionPointRec(mousePos, drawSpace))
	    DrawRectangleLinesEx(drawSpace, 2, BLACK);
    }

    if (CheckCollisionPointRec(
	    mousePos,
	    (Rectangle) {x,y,tileSize.x*scale*INPUT_SIZE,tileSize.y*scale*INPUT_SIZE})
	&& IsMouseButtonDown(MOUSE_LEFT_BUTTON) && selectedBrushTile >= 0)
    {
	inputExample[mouseIdx] = selectedBrushTile;
	/* TraceLog(LOG_INFO, TextFormat("Setting idx %d to Tile %d", mouseIdx, selectedBrushTile)); */
    }

    // === Draw Selector Window === 

    float selectorWindowY = y + tileSize.y * scale * INPUT_SIZE + INPUT_SIZE;
    float selectorWindowH = tileSize.y * scale * (float) (tileNum / INPUT_SIZE);
    for (int i = 0; i < tileNum; i++)
    {
	Tile* t = &Tileset[i];
	int tY = i / INPUT_SIZE;
	int tX = i - INPUT_SIZE * tY;
	Rectangle drawSpace = (Rectangle) {
		x + tX * tileSize.x * scale, selectorWindowY + tY * tileSize.y * scale,
		tileSize.x * scale, tileSize.y * scale
	};
	DrawTexturePro(
	    tilesetTex,
	    (Rectangle) {
		t->texU * tileSize.x, t->texV * tileSize.y,
		tileSize.x, tileSize.y
	    },
	    drawSpace,
	    (Vector2) {0},
	    t->rot, WHITE);

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
	    if (CheckCollisionPointRec(mousePos, drawSpace))
		selectedBrushTile = i;
	}

	if (i == selectedBrushTile)
	    DrawRectangleLinesEx(drawSpace, 2, YELLOW);
    }

    DrawRectangleLinesEx(
	(Rectangle) {
	    x, selectorWindowY,
	    tileSize.x * scale * INPUT_SIZE, selectorWindowH},
	2, BLACK
    );
}

#define IDX(tFrom, tTo, dir) ((dir) * TileCount * TileCount + (tFrom) * TileCount + (tTo))
void InitWFC(WFC_State* wfc)
{
    assert(wfc != NULL);
    assert(!wfc->initialized && wfc->outputW > 0 && wfc->outputH > 0);

    wfc->wave = malloc(sizeof(WFC_Cell) * wfc->outputW * wfc->outputH);
    if (wfc->wave == NULL)
    {
	return;
    }

    memset(wfc->propagator, 0, DIRS_COUNT * TileCount * TileCount);
    /* TraceLog(LOG_INFO, TextFormat("%d", TileCount)); */
    for (int i = 0; i < INPUT_SIZE * INPUT_SIZE; i++)
    {
	Tileset[inputExample[i]].weight += 1;

	if (i % INPUT_SIZE != 0)
	{
	    int left = IDX(inputExample[i], inputExample[i-1], LEFT);
	    wfc->propagator[left] = true;
	}
	if (i % INPUT_SIZE != INPUT_SIZE - 1)
	{
	    int right = IDX(inputExample[i], inputExample[i+1], RIGHT);
	    wfc->propagator[right] = true;
	}
	if (i - INPUT_SIZE >= 0)
	{
	    int up = IDX(inputExample[i], inputExample[i - INPUT_SIZE], UP);
	    wfc->propagator[up] = true;
	}
	if (i + INPUT_SIZE < INPUT_SIZE * INPUT_SIZE)
	{
	    int down = IDX(inputExample[i], inputExample[i + INPUT_SIZE], DOWN);
	    wfc->propagator[down] = true;
	}
    }

    for (int i = 0; i < wfc->outputW * wfc->outputH; i++)
    {
	wfc->wave[i].isCollapsed = false;
	wfc->wave[i].sumWeights = 0;
	for (int tc = 0; tc < tileNum; tc++)
	{
	    wfc->wave[i].validTiles[tc] = true;
	    wfc->wave[i].sumWeights += Tileset[tc].weight;
	}
	wfc->wave[i].validTileCount = tileNum;
    }

    memset(wfc->props, 0, sizeof wfc->props);
    wfc->propCount = 0;
    wfc->initialized = true;
    TraceLog(LOG_INFO, TextFormat("Initializing WFC: %dx%d, %d tiles (TileCount: %d)", wfc->outputW, wfc->outputH, tileNum, TileCount));
}

void DestroyWFC(WFC_State* wfc)
{
    assert(wfc != NULL);
    assert(wfc->initialized);

    free(wfc->wave);
    wfc->initialized = false;
}

static inline void WFC_AddProp(WFC_State* wfc, int fromIdx, int toIdx, WFC_Dir dir)
{
    if (wfc->propCount < MAX_PROPS)
    {
	wfc->props[wfc->propCount] = (WFC_Prop) {fromIdx, toIdx, dir};
	wfc->propCount++;
    }
}

void Collapse(WFC_State* wfc, int cellIdx)
{
    int validTileIdxs[TileCount] = {0};
    int validTileCount = 0;
    WFC_Cell* cellToCollapse = &wfc->wave[cellIdx];
    for (int i = 0; i < tileNum; i++)
    {
	if (cellToCollapse->validTiles[i] == true)
	{
	    validTileIdxs[validTileCount++] = i;
	}
    }

    // TODO: this feels dumb... I'm running through the tiles twice!
    // uses cumulative density logic
    int choice = rand() % (int) cellToCollapse->sumWeights;
    int collapsedVal;
    for (int i = 0; i < validTileCount; i++) {
	int weight = Tileset[validTileIdxs[i]].weight;
	if (choice >= weight)
	{
	    choice -= weight;
	}
	else
	{
	    collapsedVal = validTileIdxs[i];
	    break;
	}
    }

    // Set valid cell and weights for collapsed cell
    memset(cellToCollapse->validTiles, 0, sizeof cellToCollapse->validTiles);
    cellToCollapse->validTiles[collapsedVal] = true;
    cellToCollapse->isCollapsed = true;
    cellToCollapse->validTileCount = 1;
    // FIXME: I'm pretty sure this causes problems. Check again!
    cellToCollapse->sumWeights = 0;

    if (cellIdx % wfc->outputW != 0)
	WFC_AddProp(wfc, cellIdx, cellIdx-1, LEFT);
    if (cellIdx % wfc->outputW != wfc->outputW - 1)
	WFC_AddProp(wfc, cellIdx, cellIdx+1, RIGHT);
    if (cellIdx - wfc->outputW >= 0)
	WFC_AddProp(wfc, cellIdx, cellIdx-wfc->outputW, UP);
    if (cellIdx + wfc->outputW < wfc->outputH * wfc->outputW)
	WFC_AddProp(wfc, cellIdx, cellIdx+wfc->outputW, DOWN);

    TraceLog(LOG_INFO, TextFormat("Collapsed cell %d to Tile %d.", cellIdx, collapsedVal));
}

int Observe(WFC_State* wfc)
{
    double min = DBL_MAX;
    int arg_min = -1;

    for (int i = 0; i < wfc->outputH * wfc->outputW; i++)
    {
	if (wfc->wave[i].isCollapsed == true)
	    continue;

	double entropy = 0;
	for (int t = 0; t < tileNum; t++)
	{
	    if (wfc->wave[i].validTiles[t] == false)
		continue;

	    double p = Tileset[t].weight / wfc->wave[i].sumWeights;
	    if (p == 0)
	    {
		/* arg_min = -1; */
		continue; // TODO: is this correct, even?
	    }

	    entropy += p * log(p);
	}

	if (entropy < min)
	{
	    min = entropy;
	    arg_min = i;
	}
    }

    // TODO: melhor tratamento AQUI!!!!!
    if (min == 0)
    {
	return -1;
    }

    if (arg_min != -1)
	Collapse(wfc, arg_min);

    return arg_min;
}

static inline bool PropsLeft(WFC_State* wfc)
{
    return wfc->propCount > 0;
}

// TODO: parece estar produzindo newSumWeights = 0 de alguma forma...
// - Colocar eliminação de propagações já existentes
// - Fazer reset (pode ser simplesmente falha normal mesmo)
void Propagate(WFC_State* wfc)
{
    assert(wfc->propCount > 0);
    WFC_Prop p = wfc->props[wfc->propCount-1];
    wfc->propCount--;

    /* TraceLog(LOG_INFO, TextFormat("Doing Prop %d -> %d.", p.fromIdx, p.toIdx)); */

    WFC_Cell* destCell = &wfc->wave[p.toIdx];
    WFC_Cell* srcCell = &wfc->wave[p.fromIdx];

    int newValidCount = 0;
    float newSumWeights = 0;
    int lastEnabled = -1;

    bool changed = false;
    for (int destTile = 0; destTile < tileNum; destTile++)
    {
	if (destCell->validTiles[destTile] == false)
	    continue;

	bool isAllowed = false; // will be true if one tile from src allows this tile at dest
	for (int srcTile = 0; srcTile < tileNum; srcTile++)
	{
	    if (srcCell->validTiles[srcTile] == false)
		continue;

	    if (wfc->propagator[IDX(srcTile, destTile, p.dir)])
	    {
		isAllowed = true;
		break;
	    }
	}

	if (isAllowed)
	{
	    destCell->validTiles[destTile] = true;
	    newValidCount++;
	    newSumWeights += Tileset[destTile].weight;
	    lastEnabled = destTile;
	}
	else
	{
	    destCell->validTiles[destTile] = false;
	}
    }

    if (newSumWeights != destCell->sumWeights || newValidCount != destCell->validTileCount)
    {
	destCell->validTileCount = newValidCount;
	destCell->sumWeights = newSumWeights;

	if (p.toIdx % wfc->outputW != 0 && p.dir != RIGHT)
	    WFC_AddProp(wfc, p.toIdx, p.toIdx-1, LEFT);
	if (p.toIdx % wfc->outputW != wfc->outputW - 1 && p.dir != LEFT)
	    WFC_AddProp(wfc, p.toIdx, p.toIdx+1, RIGHT);
	if (p.toIdx - wfc->outputW >= 0 && p.dir != DOWN)
	    WFC_AddProp(wfc, p.toIdx, p.toIdx-wfc->outputW, UP);
	if (p.toIdx + wfc->outputW < wfc->outputH * wfc->outputW && p.dir != UP)
	    WFC_AddProp(wfc, p.toIdx, p.toIdx+wfc->outputW, DOWN);

	/* TraceLog(LOG_INFO, TextFormat("Tile %d was changed by prop.", p.toIdx)); */
    }
}

void AdvanceWFC(WFC_State* wfc)
{
    assert(wfc != NULL);

    int collapsed = Observe(wfc);
    if (collapsed < 0)
	return;

    /* Observe(wfc); */
    /* Propagate(wfc); */
    /* Propagate(wfc); */
    /* Propagate(wfc); */
    /* Propagate(wfc); */
    /* Propagate(wfc); */

    while (PropsLeft(wfc))
    {
	Propagate(wfc);
    }
}

void FullWFC(WFC_State* wfc)
{
    assert(wfc != NULL);

    while (Observe(wfc) >= 0)
    {
	while (PropsLeft(wfc))
	{
	    Propagate(wfc);
	}
    }
}

void DrawWFC(WFC_State* wfc, int x, int y, float scale)
{
    assert(wfc != NULL);
    if (!wfc->initialized)
	return;

    Rectangle bounds = {
	x, y,
	wfc->outputW * tileSize.x * scale,
	wfc->outputH * tileSize.y * scale
    };

    for (int i = 0; i < wfc->outputW * wfc->outputH; i++)
    {
	int ty = i / wfc->outputW;
	int tx = i - ty * wfc->outputW;

	WFC_Cell* cell = &wfc->wave[i];
	Rectangle drawBounds = {
	    x + tx * tileSize.x * scale, y + ty * tileSize.y * scale,
	    tileSize.x * scale, tileSize.y * scale
	};

	Color drawCol;
	if (cell->validTileCount == 0)
	{
	    drawCol = BLACK;
	}
	else if (cell->validTileCount == 1)
	{
	    drawCol = WHITE;
	}
	else
	{
	    drawCol = ColorFromNormalized((Vector4) {1,1,1,1./cell->validTileCount});
	}
	for (int t = 0; t < tileNum; t++)
	{
	    // FIXME: if there's no valid tiles, nothing is drawn
	    if (cell->validTiles[t] == false) continue;
	    Tile* tile = &Tileset[t];
	    DrawTexturePro(
		tilesetTex,
		(Rectangle) {
		    tile->texU * tileSize.x, tile->texV * tileSize.y,
		    tileSize.x, tileSize.y
		},
		drawBounds,
		(Vector2) {0},
		0,
		drawCol);
	}

	if (CheckCollisionPointRec(GetMousePosition(), drawBounds))
	{
	    DrawText(
		TextFormat("(%d, %d): valid_tiles: %d",
			 tx, ty, cell->validTileCount),
		x, y + 420, 10, BLACK);
	}
    }

    DrawRectangleLinesEx(bounds, 2, BLACK);
}
