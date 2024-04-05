#include "raylib.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <utility>
#include <vector>
#include <filesystem>

#define WFC_MAX_RESETS 100
#define WFC_METRICS
#include "wfc_heuristic_v2.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#undef RAYGUI_IMPLEMENTATION

// This matches with the order of (dx, dy) later!
enum {
    LEFT,
    DOWN,
    RIGHT,
    UP,
};

const char* samplesPathName = "../assets/wfc-samples/";

std::vector<std::pair<Image, std::string>> samples;
int currentSample = 0, patternSize = 2;
bool periodic = true;
Texture currentSampleTex {};

namespace fs = std::filesystem;

class WFCTexture {
public:
    WFCTexture (int width, int height, int sample, int patternSize, bool periodic);
    ~WFCTexture();
    void Generate();
    void GenStep();
    void Reset();
    bool isGenerated() const { return wfc.isFinished; }
    void Draw(float x, float y, float scale);
private:
    WFC_State wfc;
    std::vector<Tile> tiles;
    
    int sampleIdx;
    std::vector<Color> palette;
    int patternSize;
    std::vector<std::vector<int>> patterns;

    int outW, outH;
    Texture currentOutTex {};
    void Update();
    bool dirty = false;
};

int main (void)
{
    InitWindow(800, 450, "WFC Example 4 - WFC Overlap");
    SetTargetFPS(60);

    fs::path dirPath(samplesPathName);

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
    {
        std::cerr << "Directory does not exist or is not a directory\n";
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(dirPath))
    {
        if (fs::is_regular_file(entry) && entry.path().extension() == ".png")
        {
            std::string path = entry.path().u8string();
            // TraceLog(LOG_INFO, TextFormat("Found new sample: %s.\n", path.c_str()));
            Image img = LoadImage(path.c_str());
            samples.push_back({img, entry.path().filename().u8string()});
        }
    }

    TraceLog(LOG_INFO, TextFormat("%d samples loaded succesfully.", samples.size())); 

    // samples.push_back(LoadImage("../assets/wfc-samples/ColoredCity.png"));
    // currentSample = samples.size() - 1;
    currentSampleTex = LoadTextureFromImage(samples[currentSample].first);

    WFCTexture* mainWFCTexture = new WFCTexture (40, 32, currentSample, patternSize, periodic);
    patternSize = 0; // This is to avoid some bad behavior with raygui

    bool autostep = false;
    float timer, stepDelay = 0.1;
    timer = stepDelay;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_SPACE))
        {
            if (mainWFCTexture->isGenerated())
            {
                TraceLog(LOG_INFO, "Resetting texture.");
                mainWFCTexture->Reset();
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
            if (mainWFCTexture->isGenerated())
            {
                mainWFCTexture->Reset();
            }
            mainWFCTexture->Generate();
            autostep = false;
            timer = stepDelay;
        }

        if (IsKeyPressed(KEY_S))
        {
            mainWFCTexture->GenStep();
            autostep = false;
            timer = stepDelay;
        }

        if (autostep)
        {
            timer = timer - GetFrameTime();
            if (timer <= 0.0)
            {
                mainWFCTexture->GenStep();
            }
        }

        BeginDrawing();
            ClearBackground(GRAY);

            if (GuiButton({20, 40, 26, 26}, "#114#"))
            {
                currentSample = currentSample == 0 ? samples.size() - 1 : currentSample - 1;
                UnloadTexture(currentSampleTex);
                currentSampleTex = LoadTextureFromImage(samples[currentSample].first);
                delete mainWFCTexture;
                mainWFCTexture = new WFCTexture (40, 32, currentSample, patternSize+2, periodic);
                autostep = false;
                timer = stepDelay;
            }

            // GuiLabel({46, 20, 94, 26}, TextFormat("%s", samples[currentSample].second.c_str()));

            if (GuiButton({46, 40, 26, 26}, "#115#"))
            {
                currentSample = currentSample == samples.size() - 1 ? 0 : currentSample + 1;
                UnloadTexture(currentSampleTex);
                currentSampleTex = LoadTextureFromImage(samples[currentSample].first);
                delete mainWFCTexture;
                mainWFCTexture = new WFCTexture (40, 32, currentSample, patternSize+2, periodic);
                autostep = false;
                timer = stepDelay;
            }

            int temp = patternSize;
            GuiComboBox({20, 68, 120, 32}, "Pattern Size: 2;Pattern Size: 3;Pattern Size: 4", &patternSize);
            if (patternSize != temp)
            {
                TraceLog(LOG_INFO, TextFormat("Changed pattern size to %d.", patternSize+2));
                delete mainWFCTexture;
                mainWFCTexture = new WFCTexture (40, 32, currentSample, patternSize+2, periodic);
                autostep = false;
                timer = stepDelay;
            }

            temp = periodic;
            GuiToggle({20, 120, 100, 32}, "Periodic?", &periodic);
            if (periodic != temp) {
                TraceLog(LOG_INFO, TextFormat("Changed periodic to %s.", periodic ? "True" : "False"));
                delete mainWFCTexture;
                mainWFCTexture = new WFCTexture(40, 32, currentSample, patternSize + 2, periodic);
                autostep = false;
                timer = stepDelay;
            }

            DrawTextureEx(currentSampleTex, {20, 150}, 0, 5, WHITE);
            mainWFCTexture->Draw(300, 100, 10);

            DrawText(TextFormat("%s", samples[currentSample].second.c_str()), 20, 20, 20, BLACK);

        EndDrawing();
    }

    delete mainWFCTexture;

    for (auto& sample : samples)
    {
        UnloadImage(sample.first);
    }

    UnloadTexture(currentSampleTex);

    CloseWindow();

    return 0;
}

WFCTexture::WFCTexture (int width, int height, int sample, int patternSize, bool periodic)
{
    outW = width;
    outH = height;
    sampleIdx = sample;
    this->patternSize = patternSize; // TODO: unset this

    const Image& sampleImg = samples[sampleIdx].first;
    std::vector<char> palettedSample(sampleImg.width * sampleImg.height);

    // Extract palette from sample and generate an array with pixel -> samples.
    for (int i = 0; i < sampleImg.width * sampleImg.height; i++)
    {
        Color col = GetImageColor(sampleImg, i % sampleImg.width, i / sampleImg.width);
        const auto equalCols = [&](Color c) -> bool {
            if (c.r != col.r)
                return false;
            else if (c.g != col.g)
                return false;
            else if (c.b != col.b)
                return false;
            else if (c.a != col.a)
                return false;
            return true;
        };
        auto it = std::find_if(palette.begin(), palette.end(), equalCols);
        int colIdx = -1;
        if (it == palette.end())
        {
            palette.push_back(col);
            colIdx = palette.size() - 1;
        }
        else
            colIdx = std::distance(palette.begin(), it);

        palettedSample[i] = colIdx;
    }

    // We're gonna store the pattern idxs here, mapped to their hashes.
    std::map<long, int> patternIdxs;
    std::vector<float> patternWeights;

    // Function to generate a pattern given a point and a way to sample the image (a function that takes a point and returns a palette index).
    // this allows flexibility on how to extract parts of the image into patterns
    auto patternAt = [&](int imgX, int imgY, std::function<int (int,int)> sampler) -> std::vector<int> {
        std::vector<int> pat(patternSize * patternSize);        
        for (int dy = 0; dy < patternSize; dy++) for (int dx = 0; dx < patternSize; dx++) pat[dx + dy * patternSize] = sampler(dx, dy);
        return pat;
    };

    // Compute hash from pattern. Easier to do comparisons this way.
    auto hash = [](const std::vector<int>& p, int c) -> long {
        long res = 0, power = 1;
        for (int i = 0; i < p.size(); i++)
        {
            res += p[p.size() - 1 - i] * power;
            power *= c;
        }
        return res;
    };

    // Find all patterns in the image
    int c = palette.size();
    int ymax = periodic ? sampleImg.height : sampleImg.height - (patternSize-1);
    int xmax = periodic ? sampleImg.width  : sampleImg.width - (patternSize-1);
    for (int iy = 0; iy < ymax; iy++)
    {
        for (int ix = 0; ix < xmax; ix++)
        {
            std::vector<int> pat = patternAt(ix, iy, [&](int dx, int dy) -> int { return palettedSample[(ix + dx) % sampleImg.width + (iy + dy) % sampleImg.height * sampleImg.width]; });
            long h = hash(pat, c);

            if (auto found = patternIdxs.find(h); found != patternIdxs.end())
            {
                patternWeights[found->second] = patternWeights[found->second] + 1;
            }
            else
            {
                patternIdxs.insert({h, patternWeights.size()});
                patternWeights.push_back(1.0);
                patterns.push_back(pat);
            }
        }
    }

    // Map the patterns into tiles for the WFC lib.
    // Remember, each "tile" is actually a pattern, centered at the top left pixel
    for (int i = 0; i < patternWeights.size(); i++)
    {
        tiles.push_back({i, patternWeights[i]});
    }

    TraceLog(LOG_INFO, TextFormat("Extracted %d patterns.", patterns.size()));

    // Directional offsets. (dx, dy) form LEFT (-1,0), DOWN (0, 1), RIGHT (1, 0), UP (0, -1)
    static const int dx[] = {-1, 0, 1, 0};
    static const int dy[] = {0, 1, 0, -1};

    // Function to verify if two patterns overlap given an offset.
    static auto agrees = [&](std::vector<int>& p1, std::vector<int>& p2, int dx, int dy, int patSize) -> bool {
        int xmin = dx < 0 ? 0 : dx, xmax = dx < 0 ? dx + patSize : patSize, ymin = dy < 0 ? 0 : dy, ymax = dy < 0 ? dy + patSize : patSize;
        for (int y = ymin; y < ymax; y++) for (int x = xmin; x < xmax; x++) if (p1[x + patSize * y] != p2[x - dx + patSize * (y - dy)]) return false;
        return true;
    };

    WFC_Init(&wfc, &tiles[0], tiles.size(), 4);
    wfc.maxResets = WFC_MAX_RESETS;

    // Add cells in the grid
    for (int i = 0; i < width * height; i++)
    {
        WFC_AddCell(&wfc);
        if ((i / width) > 0)
            WFC_AddNeighbor(&wfc, i, i-width, UP);
        if ((i / width) < height-1)
            WFC_AddNeighbor(&wfc, i, i+width, DOWN);
        if ((i % width) > 0)
            WFC_AddNeighbor(&wfc, i, i-1, LEFT);
        if ((i % width) < width-1)
            WFC_AddNeighbor(&wfc, i, i+1, RIGHT);
    }

    // Generate rules between patterns based on their overlap.
    for (int d = 0; d < 4; d++)
    {
        for (int t1 = 0; t1 < tiles.size(); t1++)
        {
            for (int t2 = 0; t2 < tiles.size(); t2++)
            {
                WFC_SetRule(&wfc, tiles[t1], tiles[t2], d, agrees(patterns[t1], patterns[t2], dx[d], dy[d], patternSize));
            }
        }
    }

    dirty = true;
}

void WFCTexture::Generate()
{
    if (wfc.isFinished)
    {
        return;
    }

    WFC_Run(&wfc);

    if (wfc.totalResets >= wfc.maxResets)
    {
        TraceLog(LOG_ERROR, "WFC exceeded maximum reset limit!");
    }

    if (wfc.isFinished)
    {
        TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", wfc.totalIterations, wfc.totalObservations, wfc.totalPropagations, wfc.totalResets));
    }
    dirty = true;
}

void WFCTexture::GenStep()
{
    if (wfc.isFinished)
    {
        return;
    }

    WFC_DoStep(&this->wfc);

    if (wfc.totalResets >= wfc.maxResets)
    {
        TraceLog(LOG_ERROR, "WFC exceeded maximum reset limit!");
    }

    if (wfc.isFinished)
    {
        TraceLog(LOG_INFO, TextFormat("WFC finished with %d iterations. Observations: %d | Propagations: %d | Resets: %d", wfc.totalIterations, wfc.totalObservations, wfc.totalPropagations, wfc.totalResets));
    }
    dirty = true;
}

void WFCTexture::Reset()
{
    if (wfc.initialized)
    {
        WFC_Reset(&this->wfc);
        dirty = true;
    }
}

void WFCTexture::Update()
{
    Image newImage = GenImageColor(outW, outH, BLACK);
    
    for (int i = 0; i < outW * outH; i++)
    {
        int y = i / outW;
        int x = i - y * outW;
        WFC_Cell& cell = wfc.wave[i];

        if (cell.isCollapsed)
        {
            ImageDrawPixel(&newImage, x, y, palette[patterns[cell.collapsedTile][0]]);
            continue;
        }

        // Count of different patterns influencing this cell color.
        int contributors = 0;
        int r = 0, g = 0, b = 0;
        // Fetch all patterns around each cell to compute the proper color.
        for (int dy = 0; dy < patternSize; dy++) for (int dx = 0; dx < patternSize; dx++)
        {
            int adjX = x - dx;
            if (adjX < 0) adjX += outW;

            int adjY = y - dy;
            if (adjY < 0) adjY += outH;

            int adj = adjX + adjY * outW;

            // We're not treating images that wrap around so ignore surrounding patterns if they come from the other side of the wave.
            if (adjX + patternSize > outW || adjY + patternSize > outH || adjX < 0 || adjY < 0) continue;

            for (int t = 0; t < tiles.size(); t++)
            {
                if (wfc.wave[adj].validTiles[t] != true) continue;

                contributors++;
                r += palette[patterns[t][dx + dy * patternSize]].r;
                g += palette[patterns[t][dx + dy * patternSize]].g;
                b += palette[patterns[t][dx + dy * patternSize]].b;
            }
        }

        Color drawCol = {
            static_cast<unsigned char>(r / contributors),
            static_cast<unsigned char>(g / contributors),
            static_cast<unsigned char>(b / contributors),
            255
        };

        ImageDrawPixel(&newImage, x, y, drawCol);
    }

    if (currentOutTex.id <= 0)
        currentOutTex = LoadTextureFromImage(newImage);
    else
        UpdateTexture(currentOutTex, newImage.data);

    UnloadImage(newImage);
    
    dirty = false;
}

void WFCTexture::Draw(float x, float y, float scale)
{
    if (dirty)
        Update();

    if (currentOutTex.id <= 0)
        return;

    Rectangle drawBounds = {
        x, y,
        currentOutTex.width * scale,
        currentOutTex.height * scale
    };

    DrawTexturePro(currentOutTex,
                   {0, 0, static_cast<float>(currentOutTex.width), static_cast<float>(currentOutTex.height)},
                   drawBounds,
                   {0, 0},
                   0,
                   WHITE
        );

    Vector2 mousePos = GetMousePosition();
    if (CheckCollisionPointRec(mousePos, drawBounds))
    {
        int hoverX = (mousePos.x - x) / scale;
        int hoverY = (mousePos.y - y) / scale;
        int hoverCellIdx = hoverX + hoverY * currentOutTex.width;
        DrawText(TextFormat("Cell %d - Neighbors: %d - Tiles: %d (%s)", hoverCellIdx, wfc.wave[hoverCellIdx].neighborCount, wfc.wave[hoverCellIdx].validTileCount, wfc.wave[hoverCellIdx].isCollapsed ? "Collapsed" : "Not Collapsed"), 200, 10, 20, BLACK);
    }
}

WFCTexture::~WFCTexture()
{
    if (wfc.initialized)
    {
        WFC_CleanUp(&wfc);
    }

    if (currentOutTex.id > 0)
    {
        UnloadTexture(currentOutTex);
    }
}
