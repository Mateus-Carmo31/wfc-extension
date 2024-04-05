// wfc_heuristic_v2.h
//
#ifndef WFC_HEURISTIC_V2_H
#define WFC_HEURISTIC_V2_H 1

//------------------------------------------------------------------------------------------
// Defines and Macros
//------------------------------------------------------------------------------------------

// Custom memory allocation
#ifndef WFC_MALLOC
#define WFC_MALLOC(sz) malloc(sz)
#endif

#ifndef WFC_CALLOC
#define WFC_CALLOC(n,sz) calloc(n,sz)
#endif

#ifndef WFC_REALLOC
#define WFC_REALLOC(ptr,sz) realloc(ptr,sz)
#endif

#ifndef WFC_FREE
#define WFC_FREE(ptr) free(ptr)
#endif

#define WFC_SUCCESS 1
#define WFC_ERROR -1

// Custom weights type
#ifndef WFC_WEIGHTS_TYPE
#define WFC_WEIGHTS_TYPE double
#endif

/* #define WFC_MAX_RESETS 1000 */

// Metrics and reset limit
#ifdef WFC_DEBUG
#include <stdio.h>
#define WFC_DEBUG_PRINT(str) puts(str)
#define WFC_DEBUG_PRINTF(str, ...) printf(str, __VA_ARGS__)
#else
#define WFC_DEBUG_PRINT(str)
#define WFC_DEBUG_PRINTF(str, ...)
#endif

#include <stdbool.h>

/*********************/
/* Tiles and Tileset */
/*********************/

typedef struct {
    int val;
    float weight;
} Tile;

/*************/
/* WFC Stuff */
/*************/

// Represents an individual cell in the wave
typedef struct WFC_Cell
{
    int idx;
    bool isCollapsed;
    int collapsedTile;

    // TODO: maybe I should use int sumWeights instead?
    float sumWeights;
    int validTileCount;
    bool* validTiles;
    int initialTile; // This is set by WFC_SetTileTo

    // Caching
    WFC_WEIGHTS_TYPE weightLogWeightSum;

    int neighborCount;
    int _neighborCap; // Used during generation.
    // FIXME: não dá pra usar esse rel pq os índices nesse vetor não batem, precisaria ser um hashmap...
    struct { int idx; int rel; }* neighbors; // NOTE: it's important to use this indexes here to avoid reallocation problems
} WFC_Cell;

// TODO: adicionar relação aqui, aí dá pra usar o rel pré-calculada
typedef struct
{
    int from;
    int to;
    int rel;
} WFC_Prop;

#define MAX_PROPS 256

// Represents the state of the WFC at the current step.
typedef struct WFC_State
{
    bool initialized;
    bool isFinished;
    bool _dirty;

    int tileCount;
    Tile* tileset;

    int relCount; // Count of relationships

    bool* propagator; // Length = Relationship count * Tile Count * Tile Count
    // Queued up propagations
    WFC_Prop props[MAX_PROPS];
    int propCount;

    /* int outputW, outputH; */
    int cellCount;
    int _cellCap;
    WFC_Cell* wave; 

#ifdef WFC_METRICS
    long maxResets;
    long totalIterations;
    long totalPropagations;
    long totalObservations;
    long totalResets;
#endif
} WFC_State;

// TODO: rethink this entire function pointer business. maybe improve the interface?
typedef int (*RelationshipFunction)(struct WFC_State*, int, int);

#ifdef __cplusplus
extern "C" {
#endif

    void WFC_Init(WFC_State* wfc, Tile* tileset, int tileCount, int relCount);
    void WFC_Reset(WFC_State* wfc);
    void WFC_CleanUp(WFC_State* wfc);

    int WFC_AddCell(WFC_State* wfc);
    int WFC_RemoveCell(WFC_State* wfc, int idx);
    int WFC_AddNeighbor(WFC_State* wfc, int idxCell, int idxNeighbor, int rel);
    int WFC_RemoveNeighbor(WFC_State* wfc, int idxCell, int idxNeighbor);
    int WFC_CalculateNeighbors(WFC_State* wfc, RelationshipFunction relFunc);
    void WFC_SetRule(WFC_State* wfc, Tile sTile, Tile dTile, int rel, bool allowed);

    void WFC_SetTileTo(WFC_State* wfc, int cellIdx, int tile);
    int WFC_DoStep(WFC_State* wfc);
    int WFC_Run(WFC_State* wfc);

#ifdef __cplusplus
}
#endif

/* #define WFC_IMPLEMENTATION */
#ifdef WFC_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <time.h>

const int alloc_inc = 4;

// Dinamically adjust neighbor lists.
// WARN: lists aren't guaranteed to be of the right size, have to adjust after.
static int WFC__AddToNeighborList(WFC_State* wfc, int cellIdx, int neighborIdx, int rel)
{
    if (wfc->wave[cellIdx].neighbors == NULL)
    {
        wfc->wave[cellIdx].neighbors = WFC_CALLOC(alloc_inc, sizeof wfc->wave[cellIdx].neighbors[0]);
        if (wfc->wave[cellIdx].neighbors == NULL)
            return 1;

        wfc->wave[cellIdx]._neighborCap = alloc_inc;
    }
    else if(wfc->wave[cellIdx].neighborCount == wfc->wave[cellIdx]._neighborCap)
    {
        wfc->wave[cellIdx]._neighborCap *= 2;
        void* new_ptr = WFC_REALLOC(wfc->wave[cellIdx].neighbors, wfc->wave[cellIdx]._neighborCap * sizeof wfc->wave[cellIdx].neighbors[0]);
        if (new_ptr == NULL)
        {
            WFC_FREE (wfc->wave[cellIdx].neighbors);
            wfc->wave[cellIdx].neighbors = NULL;
            return 1;
        }

        wfc->wave[cellIdx].neighbors = new_ptr;
    }

    wfc->wave[cellIdx].neighbors[wfc->wave[cellIdx].neighborCount  ].idx = neighborIdx;
    wfc->wave[cellIdx].neighbors[wfc->wave[cellIdx].neighborCount++].rel = rel;
    return 0;
}

static int WFC__NeighborSetup(WFC_State* wfc, RelationshipFunction relFunction)
{
    const int wfcSize = wfc->cellCount;
    for (int cell1 = 0; cell1 < wfcSize; cell1++)
    {
        for (int cell2 = cell1 + 1; cell2 < wfcSize; cell2++)
        {
            int rel = relFunction(wfc, cell1, cell2);
            if (rel >= 0)
            {
                if (WFC__AddToNeighborList(wfc, cell1, cell2, rel))
                    return 1;

                if (WFC__AddToNeighborList(wfc, cell2, cell1, rel))
                    return 1;
            }
        }
    }

    // Adjust the array sizes
    for (int i = 0; i < wfcSize; i++)
    {
        if (wfc->wave[i].neighborCount < wfc->wave[i]._neighborCap)
        {
            wfc->wave[i]._neighborCap = wfc->wave[i].neighborCount;
            void* new_ptr = WFC_REALLOC(wfc->wave[i].neighbors, wfc->wave[i]._neighborCap * sizeof wfc->wave[i].neighbors[0]);
            if (new_ptr == NULL)
            {
                WFC_FREE(wfc->wave[i].neighbors);
                wfc->wave[i].neighbors = NULL;
                return 1;
            }

            wfc->wave[i].neighbors = new_ptr;
        }
    }

    return 0;
}

void WFC_Init(WFC_State* wfc, Tile* tileset, int tileCount, int relCount)
{
    assert(wfc != NULL);
    assert(tileset != NULL);
    assert(tileCount > 0);
    assert(relCount > 0);
    assert(!wfc->initialized);

    srand(time(NULL));

    // Setting the tilesets, size and relationship function/count
    wfc->tileset = tileset;
    wfc->tileCount = tileCount;
    wfc->relCount = relCount;
    wfc->cellCount = wfc->_cellCap = 0;
    wfc->wave = NULL;

#ifdef WFC_METRICS
    wfc->totalIterations = 0;
    wfc->totalPropagations = 0;
    wfc->totalObservations = 0;
    wfc->totalResets = 0;
#endif

    // NOTE: could I just use "1" instead of sizeof(char) ?
    wfc->propagator = WFC_CALLOC(relCount * tileCount * tileCount, sizeof wfc->propagator[0]);
    if (wfc->propagator == NULL)
    {
        goto prop_alloc_error;
    }
    // WARN: The propagator is set up with WFC_SetRule!

    memset(wfc->props, 0, sizeof wfc->props);
    wfc->propCount = 0;

    wfc->initialized = true;
    wfc->isFinished = false;
    wfc->_dirty = false;
    return;

// this cleanup is hella dirty. fix it
neighbor_alloc_error:
    //  iterate through all wave cells and free the non-nulls.
    /* for (int i = 0; i < wfc->outputW * wfc->outputH; i++) */
    /* { */
    /*  if (wfc->wave[i].neighbors != NULL) */
    /*      WFC_FREE(wfc->wave[i].neighbors); */
    /* } */
    WFC_FREE(wfc->propagator);
prop_alloc_error:
valid_tiles_alloc_error:
    /* for (int i = 0; i < wfc->outputW * wfc->outputH; i++) */
    /* { */
    /*  if (wfc->wave[i].validTiles != NULL) */
    /*      WFC_FREE(wfc->wave[i].validTiles); */
    /* } */
    /* WFC_FREE(wfc->wave); */
wave_alloc_error:
    return;
}

// Internal reset. does not affect metrics
void WFC__Reset(WFC_State* wfc)
{
    assert(wfc != NULL);

    for (int i = 0; i < wfc->cellCount; i++)
    {
        wfc->wave[i].isCollapsed = false;
        wfc->wave[i].collapsedTile = -1;
        wfc->wave[i].sumWeights = 0;
        wfc->wave[i].weightLogWeightSum = 0;
        for (int tc = 0; tc < wfc->tileCount; tc++)
        {
            wfc->wave[i].validTiles[tc] = true;
            wfc->wave[i].sumWeights += wfc->tileset[tc].weight;
            wfc->wave[i].weightLogWeightSum += wfc->tileset[tc].weight * log(wfc->tileset[tc].weight);
        }
        wfc->wave[i].validTileCount = wfc->tileCount;
    }

    memset(wfc->props, 0, sizeof wfc->props);
    wfc->propCount = 0;

    for (int i = 0; i < wfc->cellCount; i++)
    {
        if (wfc->wave[i].initialTile > -1)
            WFC_SetTileTo(wfc, i, wfc->wave[i].initialTile);
    }

    wfc->initialized = true;
    wfc->isFinished = false;
}

// API reset. Clears up all the metrics, too!
void WFC_Reset(WFC_State* wfc)
{
    assert(wfc != NULL);

#ifdef WFC_METRICS
    wfc->totalIterations = wfc->totalObservations = wfc->totalPropagations = wfc->totalResets = 0;
#endif

    WFC__Reset(wfc);
}

void WFC_CleanUp(WFC_State* wfc)
{
    assert(wfc != NULL);

    // Free the propagator
    if (wfc->propagator != NULL)
    {
        WFC_FREE(wfc->propagator);
        wfc->propagator = NULL;
    }

    // Free the neighbors and validTiles in the wave
    for (int i = 0; i < wfc->cellCount; i++)
    {
        if (wfc->wave[i].validTiles != NULL)
            WFC_FREE(wfc->wave[i].validTiles);
        if (wfc->wave[i].neighbors != NULL)
            WFC_FREE(wfc->wave[i].neighbors);
    }

    // Free the wave
    if (wfc->wave != NULL)
    {
        WFC_FREE(wfc->wave);
        wfc->wave = NULL;
    }
    wfc->initialized = false;
    wfc->isFinished = false;
    wfc->propCount = 0;
}

int WFC_AddCell(WFC_State* wfc)
{
    assert(wfc != NULL);

    if (!wfc->initialized)
        return -1;

    if (wfc->wave == NULL)
    {
        wfc->wave = WFC_CALLOC(alloc_inc, sizeof wfc->wave[0]);
        if (wfc->wave == NULL)
        {
            return -1;
        }

        wfc->_cellCap = alloc_inc;
    }

    if (wfc->cellCount == wfc->_cellCap)
    {
        WFC_Cell* new_ptr = WFC_REALLOC(wfc->wave, wfc->_cellCap * 2 * sizeof wfc->wave[0]);
        if (new_ptr == NULL)
        {
            // Reallocate failed
            return -1;
        }

        wfc->wave = new_ptr;
        wfc->_cellCap *= 2;
    }

    int idx = wfc->cellCount;

    wfc->wave[idx].idx = idx;
    wfc->wave[idx].isCollapsed = false;
    wfc->wave[idx].collapsedTile = -1;
    wfc->wave[idx].sumWeights = 0;
    wfc->wave[idx].weightLogWeightSum = 0;
    wfc->wave[idx].initialTile = -1;

    // Allocate the valid tiles array
    wfc->wave[idx].validTiles = WFC_MALLOC(sizeof(Tile) * wfc->tileCount);
    if (wfc->wave[idx].validTiles == NULL)
    {
        return -1;
    }

    for (int tc = 0; tc < wfc->tileCount; tc++)
    {
        wfc->wave[idx].validTiles[tc] = true;
        wfc->wave[idx].sumWeights += wfc->tileset[tc].weight;
        wfc->wave[idx].weightLogWeightSum += wfc->tileset[tc].weight * log(wfc->tileset[tc].weight);
    }

    wfc->wave[idx].validTileCount = wfc->tileCount;
    wfc->wave[idx].neighbors = NULL;
    wfc->wave[idx].neighborCount = 0;
    wfc->wave[idx]._neighborCap = 0;

    wfc->cellCount++;
    wfc->_dirty = true;

    return idx;
}

int WFC_AddNeighbor(WFC_State* wfc, int idxCell, int idxNeighbor, int rel)
{
    assert (wfc != NULL);
    assert (idxCell >= 0 && idxNeighbor >= 0);

    /* if (idxCell >= wfc->tileCount || idxNeighbor >= wfc->tileCount) */
    /*  return 1; */

    /* if (rel < 0) */
    /*  rel = wfc->relFunction(wfc, idxCell, idxNeighbor); */

    return WFC__AddToNeighborList(wfc, idxCell, idxNeighbor, rel);
}

int WFC_CalculateNeighbors(WFC_State* wfc, RelationshipFunction relFunc)
{
    assert (wfc != NULL);

    // WARN: This is dumb, stupid, and possibly dangerous.
    for (int i = 0; i < wfc->cellCount; i++)
    {
        if (wfc->wave[i].neighbors != NULL)
        {
            WFC_FREE(wfc->wave[i].neighbors);
            wfc->wave[i].neighbors = NULL;
        }
    }

    return WFC__NeighborSetup(wfc, relFunc);
}

// Updates all cells in the WFC state if dirty, refitting dynamic arrays and recalculating neighbors.
// FIXME: I'm not treating this function's error case well enough in its usages!
static int WFC__RefitState (WFC_State* wfc)
{
    assert(wfc != NULL);

    if (!wfc->_dirty)
        return 0;

    // Readjust wave array's size
    if (wfc->_cellCap > wfc->cellCount)
    {
        WFC_Cell* new_ptr = WFC_REALLOC(wfc->wave, wfc->cellCount * sizeof wfc->wave[0]);
        if (new_ptr == NULL)
        {
            // Failed to reallocate
            return 1;
        }

        wfc->wave = new_ptr;
        wfc->_cellCap = wfc->cellCount;
    }

    /* int error = WFC_CalculateNeighbors(wfc); */
    /* if (error) */
    /*  return 1; */

    for (int i = 0; i < wfc->cellCount; i++)
    {
        WFC_Cell* cell = &wfc->wave[i];
        if (cell->neighbors == NULL)
            continue;

        if (cell->_neighborCap > cell->neighborCount)
        {
            cell->_neighborCap = cell->neighborCount;
            void* new_ptr = WFC_REALLOC(cell->neighbors, cell->_neighborCap * sizeof cell->neighbors[0]);
            if (new_ptr == NULL)
            {
                WFC_FREE(cell->neighbors);
                cell->neighbors = NULL;
                return 1;
            }

            cell->neighbors = new_ptr;
        }
    }

    wfc->_dirty = false;
    return 0;
}

static inline int WFC__PropIdx(WFC_State* wfc, int rel, int from, int to)
{
    return ((rel) * wfc->tileCount * wfc->tileCount) + (from) * wfc->tileCount + (to);
}

// NOTE: should I pass in the tile, or the tile index?
void WFC_SetRule(WFC_State* wfc, Tile sTile, Tile dTile, int rel, bool allowed)
{
    assert(wfc != NULL && wfc->initialized);
    assert(sTile.val < wfc->tileCount && dTile.val < wfc->tileCount);

    wfc->propagator[WFC__PropIdx(wfc, rel, sTile.val, dTile.val)] = allowed;
}

//------------------------------------------------------------------------------------------
// Actual WFC code
//------------------------------------------------------------------------------------------

static inline void WFC__AddProp(WFC_State* wfc, int from, int to, int rel)
{
    // TODO: should I validate props for uniqueness?
    if (wfc->propCount < MAX_PROPS)
    {
        wfc->props[wfc->propCount] = (WFC_Prop) { from, to , rel};
        wfc->propCount++;
    }
}

static inline bool WFC__PropsLeft(WFC_State* wfc)
{
    return wfc->propCount > 0;
}

int WFC__Propagate(WFC_State* wfc);

static inline void WFC__SetCollapsed(WFC_State* wfc, WFC_Cell* cellToCollapse, int toTile)
{
    // Set valid cell and weights for collapsed cell
    memset(cellToCollapse->validTiles, 0, sizeof(Tile) * wfc->tileCount);
    cellToCollapse->validTiles[toTile] = true;
    cellToCollapse->isCollapsed = true;
    cellToCollapse->collapsedTile = toTile;
    cellToCollapse->validTileCount = 1;
    cellToCollapse->sumWeights = 0;

    for (int n = 0; n < cellToCollapse->neighborCount; n++)
    {
        // TODO: change this indirection here by making an "observed" vector
        if (wfc->wave[cellToCollapse->neighbors[n].idx].isCollapsed)
            continue;
        WFC__AddProp(wfc, cellToCollapse->idx, cellToCollapse->neighbors[n].idx, cellToCollapse->neighbors[n].rel);
    }
}

void WFC_SetTileTo(WFC_State* wfc, int cellIdx, int tile)
{
    assert(wfc != NULL && wfc->initialized);
    assert(wfc->wave != NULL && cellIdx < wfc->cellCount);

    // In case the state is dirty.
    if (WFC__RefitState(wfc))
        return;

    WFC_Cell* cellToCollapse = &wfc->wave[cellIdx];

    if (cellToCollapse->isCollapsed)
        return;

    cellToCollapse->initialTile = tile;
    WFC__SetCollapsed(wfc, cellToCollapse, tile);

    while (WFC__PropsLeft(wfc))
    {
        WFC__Propagate(wfc);
    }
}

void WFC__Collapse(WFC_State* wfc, int cellIdx)
{
    /* int choice = rand() % (int) (wfc->wave[cellIdx].sumWeights); */
    float choice = (float) rand() / (float)(RAND_MAX / wfc->wave[cellIdx].sumWeights);
    int chosenTile = -1;
    for (int i = 0; i < wfc->tileCount; i++)
    {
        if (wfc->wave[cellIdx].validTiles[i] == false)
            continue;

        float weight = wfc->tileset[i].weight;
        if (choice >= weight)
        {
            choice -= weight;
        }
        else
        {
            chosenTile = wfc->tileset[i].val;
            break;
        }
    }

    // FIXME: this treatment is horrible
    if (chosenTile == -1)
        return;

    WFC__SetCollapsed(wfc, &wfc->wave[cellIdx], chosenTile);
}

// FIXME: doesn't seem to work when a cell has a known (0 valid elements). Test in the sudoku!
int WFC__Observe(WFC_State* wfc)
{
    double min = DBL_MAX; // The minimum entropy
    int arg_min = -1; // The index of the cell with minimum entropy

    for (int i = 0; i < wfc->cellCount; i++)
    {
        if (wfc->wave[i].isCollapsed == true)
            continue;

        double entropy = log(wfc->wave[i].sumWeights) - wfc->wave[i].weightLogWeightSum / wfc->wave[i].sumWeights;

        // Add a tiny random value to the entropy as noise
        entropy += ((double) rand() / RAND_MAX) * 0.01;

        if (entropy < min)
        {
            min = entropy;
            arg_min = i;
        }
    }

    if (min == 0)
        return -1;

    if (arg_min != -1)
    {
        WFC__Collapse(wfc, arg_min);
#ifdef WFC_METRICS 
        wfc->totalObservations += 1;
#endif
    }

    return arg_min;
}

int WFC__Propagate(WFC_State* wfc)
{
    assert(wfc->propCount > 0);
    WFC_Prop p = wfc->props[wfc->propCount-1];
    wfc->propCount--;
#ifdef WFC_METRICS 
    wfc->totalPropagations += 1;
#endif

    WFC_Cell* destCell = &wfc->wave[p.to];
    WFC_Cell* srcCell = &wfc->wave[p.from];

    WFC_DEBUG_PRINTF("Propagating %d -> %d... ", p.from, p.to);

    int newValidCount = destCell->validTileCount;
    float newSumWeights = destCell->sumWeights;
    float newSumLogWeights = destCell->weightLogWeightSum;
    int lastEnabled = -1;

    bool changed = false;
    for (int destTileIdx = 0; destTileIdx < wfc->tileCount; destTileIdx++)
    {
        if (destCell->validTiles[destTileIdx] == false)
            continue;

        bool isAllowed = false; // will be true if one tile from src allows this tile at dest
        for (int srcTileIdx = 0; srcTileIdx < wfc->tileCount; srcTileIdx++)
        {
            if (srcCell->validTiles[srcTileIdx] == false)
                continue;

            int rel = p.rel;
            if (wfc->propagator[WFC__PropIdx(wfc, rel, srcTileIdx, destTileIdx)])
            {
                isAllowed = true;
                break;
            }
        }

        if (isAllowed)
        {
            /* destCell->validTiles[destTileIdx] = true; */
            lastEnabled = destTileIdx;
        }
        else
        {
            destCell->validTiles[destTileIdx] = false;
            newValidCount--;
            newSumWeights -= wfc->tileset[destTileIdx].weight;
            newSumLogWeights -= wfc->tileset[destTileIdx].weight * log(wfc->tileset[destTileIdx].weight);
        }
    }

    if (newSumWeights != destCell->sumWeights || newValidCount != destCell->validTileCount)
    {
        if (newValidCount == 0)
            return 1;

        WFC_DEBUG_PRINTF("Changed valid tiles from %d to %d.\n", destCell->validTileCount, newValidCount);

        destCell->validTileCount = newValidCount;
        destCell->sumWeights = newSumWeights;
        destCell->weightLogWeightSum = newSumLogWeights;

        if (destCell->validTileCount == 1)
        {
            // FIXME: review this usage
            WFC__SetCollapsed(wfc, destCell, lastEnabled);
            return 0;
            /* destCell->collapsedTile = lastEnabled; */
            /* destCell->isCollapsed = true; */
        }

        for (int n = 0; n < destCell->neighborCount; n++)
        {
            if (destCell->neighbors[n].idx == p.from || wfc->wave[destCell->neighbors[n].idx].isCollapsed)
                continue;
            WFC__AddProp(wfc, p.to, destCell->neighbors[n].idx, destCell->neighbors[n].rel);
        }

        /* TraceLog(LOG_INFO, TextFormat("Tile %d was changed by prop.", p.toIdx)); */
    }
    else
    {
        WFC_DEBUG_PRINT("No changes.\n");
    }

    return 0;
}

int WFC_DoStep(WFC_State* wfc)
{
    assert(wfc != NULL && wfc->initialized);

#ifdef WFC_METRICS
    if (wfc->maxResets > 0 && wfc->totalResets >= wfc->maxResets)
    {
        return WFC_ERROR;
    }
#endif

    if (WFC__RefitState(wfc))
        return WFC_ERROR;

    int collapsed = WFC__Observe(wfc);
    if (collapsed < 0)
    {
        wfc->isFinished = true;
        return WFC_SUCCESS;
    }

    while (WFC__PropsLeft(wfc))
    {
        int err = WFC__Propagate(wfc);
        if (err)
        {
            WFC__Reset(wfc);
#ifdef WFC_METRICS 
            wfc->totalResets += 1;
#endif
        }
    }

#ifdef WFC_METRICS
    wfc->totalIterations += 1;
#endif

    return 0;
}

int WFC_Run(WFC_State* wfc)
{
    assert(wfc != NULL && wfc->initialized);

    if (WFC__RefitState(wfc))
        return WFC_ERROR;

    while (WFC__Observe(wfc) >= 0)
    {
        while (WFC__PropsLeft(wfc))
        {
            int err = WFC__Propagate(wfc);
            if (err)
            {
                WFC__Reset(wfc);
#ifdef WFC_METRICS 
                wfc->totalResets += 1;
#endif
            }

#ifdef WFC_METRICS
            if (wfc->maxResets > 0 && wfc->totalResets >= wfc->maxResets)
            {
                return WFC_ERROR;
            }
#endif
        }

#ifdef WFC_METRICS 
        wfc->totalIterations += 1;
#endif
    }

    wfc->isFinished = true;
    return WFC_SUCCESS;
}


#endif // WFC_IMPLEMENTATION

#endif // WFC_HEURISTIC_V2_H
