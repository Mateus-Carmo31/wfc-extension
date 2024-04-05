#include "ui.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <iostream>
#include <utility>
#include <vector>
#include <map>

#define WFC_METRICS
#include "wfc_heuristic_v2.h"

Tile* LoadTilesetFromImage(const char* path, int tileCount);

const char* tilesetsPath = "../assets/wfc-tilesets";
// Pairs of tileset name + tile size.
const std::pair<const char*, int> tilesets[] = {
    {"Castle", 7},
    {"Circles", 32},
    {"Circuit", 14},
    {"FloorPlan", 9},
    {"Knots", 10},
    {"Rooms", 3},
    // {"Summer", 48},
};

// CCW
enum {
    RIGHT,
    UP,
    LEFT,
    DOWN,
};

class TilesetDisplay
{
public:
    TilesetDisplay(int outputX, int outputY): outputX(outputX), outputY(outputY) {}
    int LoadFromXML(const char* tilesetsPath, std::pair<const char*, int> tilesetData);
    Tile* GetTileData() { return tileArray.data(); }
    int GetTileCount() { return tileArray.size(); }
    void DrawTileset(float x, float y, float scale);
    void DrawWFC(float x, float y, float scale);
    ~TilesetDisplay();
    WFC_State wfc = {};
private:
    bool isInitialized = false;
    std::map<std::string, int> tileIds;
    std::map<int, std::pair<Texture, float>> tileTexs;
    std::vector<Tile> tileArray;
    void ClearTiles();

    int outputX;
    int outputY;
    float tileSize;
};

int main(void)
{
    InitWindow(800, 450, "WFC Example 3 - Simple WFC");

    SetTargetFPS(60);

    // const int tileCount = 10;
    // Tile* tileset = LoadTileset();

    TilesetSelector tsel {(std::pair<const char*, int>*) tilesets, 6};
    Button selButton {{30, 20}};

    TilesetDisplay td {10, 10};
    td.LoadFromXML(tilesetsPath, tilesets[0]);

    // XML: "tilename number_of_ccw_rotations"
    float stepDelay = 0.01, timer = 0.0;
    bool autostep = false;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            if (td.wfc.isFinished)
            {
                TraceLog(LOG_INFO, "Resetting wfc.");
                WFC_Reset(&td.wfc);
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
            if (td.wfc.isFinished)
            {
                WFC_Reset(&td.wfc);
            }
            WFC_Run(&td.wfc);

            TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", td.wfc.totalIterations, td.wfc.totalObservations, td.wfc.totalPropagations, td.wfc.totalResets));
            autostep = false;
            timer = stepDelay;
        }

        if (IsKeyPressed(KEY_S))
        {
            WFC_DoStep(&td.wfc);

            if (td.wfc.isFinished)
            {
                TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", td.wfc.totalIterations, td.wfc.totalObservations, td.wfc.totalPropagations, td.wfc.totalResets));
            }
            autostep = false;
            timer = stepDelay;
        }

        if (autostep)
        {
            timer = timer - GetFrameTime();
            if (timer <= 0.0)
            {
                WFC_DoStep(&td.wfc);

                if (td.wfc.isFinished)
                {
                    TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", td.wfc.totalIterations, td.wfc.totalObservations, td.wfc.totalPropagations, td.wfc.totalResets));
                    autostep = false;
                    timer = stepDelay;
                }
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        tsel(650, 200);
        if (selButton(650, 230))
        {
            td.LoadFromXML(tilesetsPath, tilesets[tsel.current]);
            autostep = false;
        }

        td.DrawTileset(10, 20, 4);
        td.DrawWFC(200, 10, 4);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

int TilesetDisplay::LoadFromXML(const char* tilesetsPath, std::pair<const char*, int> tilesetData)
{
    if (tilesetsPath == nullptr || tilesetData.first == nullptr)
        return 1;

    if (isInitialized)
    {
        ClearTiles();
        isInitialized = false;
    }

    if (wfc.initialized)
        WFC_CleanUp(&wfc);

    std::string path = tilesetsPath;
    path.append("/");
    path.append(tilesetData.first);
    path.append(".xml");

    std::string texturepath = tilesetsPath;
    texturepath += "/";
    texturepath += tilesetData.first;
    texturepath += "/";

    tileSize = tilesetData.second;

    tinyxml2::XMLDocument doc;
    doc.LoadFile(path.c_str());

    tinyxml2::XMLElement* tilesetDocRoot = doc.FirstChildElement("set");
    if (tilesetDocRoot == nullptr)
        return 1;

    int count = 0;
    std::vector<char> symmetries;
    std::map<int, int> nextRotation; // Keep track of what tile is a 90 ccw rotation of which
    std::map<int, int> mirrorX; // Keep track of the horizontal mirror of each tile

    const auto rot = [&](int t) -> int {
        return nextRotation.find(t)->second;
    };

    const auto mirX = [&](int t) -> int {
        return mirrorX.find(t)->second;
    };

    for (tinyxml2::XMLElement* elem = tilesetDocRoot->FirstChildElement("tiles")->FirstChildElement("tile"); elem != nullptr; elem = elem->NextSiblingElement("tile"))
    {
        const char * tilename, * symmetry;
        float weight = 1;
        elem->QueryAttribute("name", &tilename);
        elem->QueryAttribute("symmetry", &symmetry);
        elem->QueryAttribute("weight", &weight);

        std::cout << "[" << count << "]" << "Found tile '" << tilename << "' (" << symmetry << ")\n";

        std::string tilePath = (texturepath + tilename + ".png");
        Image tileImg = LoadImage(tilePath.c_str());
        Texture tex = LoadTextureFromImage(tileImg);
        this->tileTexs.insert({count, {tex, 0}});
        this->tileArray.push_back({count, weight});
        this->tileIds.insert({tilename, count++});
        symmetries.push_back(symmetry[0]);

        // Tiles that can be either vertical or horizontal
        if (symmetry[0] == 'I' || symmetry[0] == '\\')
        {
            this->tileTexs.insert({count, {tex, 90}});
            this->tileArray.push_back({count, weight});
            std::string new_name = tilename;
            new_name += " 1";
            this->tileIds.insert({new_name, count++});

            symmetries.push_back(symmetry[0]);

            nextRotation.insert({count-1, count-2});
            nextRotation.insert({count-2, count-1});

            if (symmetry[0] == 'I')
            {
                mirrorX.insert({count-2, count-2});
                mirrorX.insert({count-1, count-1});
            }

            if (symmetry[0] == '\\')
            {
                mirrorX.insert({count-2, count-1});
                mirrorX.insert({count-1, count-2});
            }
        }
        // Tiles that have 4 possible configurations
        else if (symmetry[0] == 'L' || symmetry[0] == 'T')
        {
            this->tileTexs.insert({count, {tex, 90}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 1", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tex, 180}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 2", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tex, 270}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 3", count++});
            symmetries.push_back(symmetry[0]);

            nextRotation.insert({count-4, count-3});
            nextRotation.insert({count-3, count-2});
            nextRotation.insert({count-2, count-1});
            nextRotation.insert({count-1, count-4});
            if (symmetry[0] == 'L')
            {
                mirrorX.insert({count-4, count-3});
                mirrorX.insert({count-3, count-4});
                mirrorX.insert({count-2, count-1});
                mirrorX.insert({count-1, count-2});
            }

            if (symmetry[0] == 'T')
            {
                mirrorX.insert({count-4, count-4});
                mirrorX.insert({count-3, count-1});
                mirrorX.insert({count-1, count-3});
                mirrorX.insert({count-2, count-2});
            }
        }
        else if (symmetry[0] == 'F')
        {
            ImageFlipHorizontal(&tileImg);
            Texture tileFlipped = LoadTextureFromImage(tileImg);

            this->tileTexs.insert({count, {tex, 90}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 1", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tex, 180}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 2", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tex, 270}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 3", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tileFlipped, 0}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 4", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tileFlipped, 90}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 5", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tileFlipped, 180}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 6", count++});
            symmetries.push_back(symmetry[0]);

            this->tileTexs.insert({count, {tileFlipped, 270}});
            this->tileArray.push_back({count, weight});
            this->tileIds.insert({std::string {tilename} + " 7", count++});
            symmetries.push_back(symmetry[0]);

            nextRotation.insert({count-8, count-7});
            nextRotation.insert({count-7, count-6});
            nextRotation.insert({count-6, count-5});
            nextRotation.insert({count-5, count-8});

            nextRotation.insert({count-4, count-3});
            nextRotation.insert({count-3, count-2});
            nextRotation.insert({count-2, count-1});
            nextRotation.insert({count-1, count-4});

            mirrorX.insert({count-8, count-4});
            mirrorX.insert({count-7, count-3});
            mirrorX.insert({count-6, count-2});
            mirrorX.insert({count-5, count-1});

            mirrorX.insert({count-4, count-8});
            mirrorX.insert({count-3, count-7});
            mirrorX.insert({count-2, count-6});
            mirrorX.insert({count-1, count-5});
        }
        // Tiles that don't rotate (they're the same in all rotations)
        else
        {
            nextRotation.insert({count-1, count-1});
            mirrorX.insert({count-1, count-1});
        }

        UnloadImage(tileImg);
    }

    // Initialize internal wfc state
    WFC_Init(&this->wfc, this->tileArray.data(), tileArray.size(), 4);
    if (!wfc.initialized)
    {
        return 1;
    }

    // Create cells and set their neighbors.
    // Since neighbors are indices, we can set them before they're actually
    // added to the WFC. Not really safe, but meh.
    for (int i = 0; i < outputX * outputY; i++)
    {
        int idx = WFC_AddCell(&this->wfc);
        if (idx % outputX > 0)
            WFC_AddNeighbor(&wfc, idx, idx-1, LEFT);
        if (idx % outputX < outputX-1)
            WFC_AddNeighbor(&wfc, idx, idx+1, RIGHT);
        if (idx / outputX > 0)
            WFC_AddNeighbor(&wfc, idx, idx-outputX, UP);
        if (idx / outputX < outputY-1)
            WFC_AddNeighbor(&wfc, idx, idx+outputX, DOWN);
    }

    for (tinyxml2::XMLElement* elem = tilesetDocRoot->FirstChildElement("neighbors")->FirstChildElement("neighbor"); elem != nullptr; elem = elem->NextSiblingElement("neighbor"))
    {
        const char *left, *right;
        elem->QueryAttribute("left", &left);
        elem->QueryAttribute("right", &right);

        std::cout << "Neighbor: " << left << " -> " << right << '\n';

        int l = tileIds.find(left)->second;
        int r = tileIds.find(right)->second;

        WFC_SetRule(&this->wfc, tileArray[l], tileArray[r], RIGHT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(l)], tileArray[rot(r)], UP, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(l))], tileArray[rot(rot(r))], LEFT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(rot(l)))], tileArray[rot(rot(rot(r)))], DOWN, true);

        WFC_SetRule(&this->wfc, tileArray[r], tileArray[l], LEFT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(r)], tileArray[rot(l)], DOWN, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(r))], tileArray[rot(rot(l))], RIGHT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(rot(r)))], tileArray[rot(rot(rot(l)))], UP, true);

        WFC_SetRule(&this->wfc, tileArray[mirX(r)], tileArray[mirX(l)], RIGHT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(mirX(r))], tileArray[rot(mirX(l))], UP, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(mirX(r)))], tileArray[rot(rot(mirX(l)))], LEFT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(rot(mirX(r))))], tileArray[rot(rot(rot(mirX(l))))], DOWN, true);

        WFC_SetRule(&this->wfc, tileArray[mirX(l)], tileArray[mirX(r)], LEFT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(mirX(l))], tileArray[rot(mirX(r))], DOWN, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(mirX(l)))], tileArray[rot(rot(mirX(r)))], RIGHT, true);
        WFC_SetRule(&this->wfc, tileArray[rot(rot(rot(mirX(l))))], tileArray[rot(rot(rot(mirX(r))))], UP, true);

        // int l = leftId, r = rightId;
        // for (int i = 0; i < 4; i++)
        // {
        //     std::cout << "Adding rule: " << l << " allows " << r << " in direction " << RIGHT + i << '\n';
        //     WFC_SetRule(&this->wfc, tileArray[l], tileArray[r], RIGHT + i, true);

        //     int mirXL = mirrorX.find(l)->second;
        //     int mirXR = mirrorX.find(r)->second;

        //     if (mirXL)
        
        //     l = nextRotation.find(l)->second;
        //     r = nextRotation.find(r)->second;
        // }
    }

    isInitialized = true;
    return 0;
}

void TilesetDisplay::DrawTileset(float x, float y, float scale)
{
    if (!isInitialized || !wfc.initialized)
        return;

    scale = 10 * scale / tileSize;

    int c = 0;
    const int grid = 3;
    for (const auto& tile : tileIds)
    {
        auto tileTexInfo = tileTexs.find(tile.second)->second;
        DrawTexturePro(
            tileTexInfo.first,
            {0,0,tileSize,tileSize},
            {x + ((c % grid) * tileSize + tileSize/2) * scale, y + ((int) (c / grid) * tileSize + tileSize/2) * scale, tileSize * scale, tileSize * scale},
            {tileSize/2 * scale, tileSize/2 * scale}, -tileTexInfo.second,
            WHITE);
        c++;
    }
}

void TilesetDisplay::DrawWFC(float x, float y, float scale)
{
    if (!isInitialized)
        return;

    scale = 10 * scale / tileSize;

    for (int i = 0; i < outputX * outputY; i++)
    {
        WFC_Cell& cell = wfc.wave[i];
        int my = i / outputX;
        int mx = i - my * outputX;
        Color col = WHITE;
        col.a = 255/cell.validTileCount;

        Rectangle drawBounds = {x + (mx * tileSize) * scale, y + (my * tileSize) * scale, tileSize * scale, tileSize * scale};

        for (int t = 0; t < wfc.tileCount; t++)
        {
            if (!cell.validTiles[t])
                continue;

            auto tileTexInfo = tileTexs.find(t)->second;
            DrawTexturePro(
                tileTexInfo.first,
                {0, 0, tileSize, tileSize},
                {x + (mx * tileSize + tileSize/2.f) * scale, y + (my * tileSize + tileSize/2.0f) * scale, tileSize * scale, tileSize * scale},
                {tileSize / 2.f * scale, tileSize / 2.f * scale},
                -tileTexInfo.second,
                col
                );
        }

        if (CheckCollisionPointRec(GetMousePosition(), drawBounds))
        {
            DrawText(TextFormat("Tile %d: %d neighbors, %d valid tiles", cell.idx, cell.neighborCount, cell.validTileCount),
                     200, 420, 10, BLACK);
        }
    }
}

void TilesetDisplay::ClearTiles()
{
    for (const auto& entry : this->tileTexs)
    {
        if (entry.second.first.id != 0)
            UnloadTexture(entry.second.first);
    }
    this->tileIds.clear();
    this->tileTexs.clear();
    this->tileArray.clear();
}

TilesetDisplay::~TilesetDisplay()
{
    for (const auto& entry : this->tileTexs)
    {
        if (entry.second.first.id != 0)
            UnloadTexture(entry.second.first);
    }

    WFC_CleanUp(&this->wfc);
}
