// tileset.hpp
//
#ifndef TILESET_H
#define TILESET_H 1

#include <string_view>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "wfc_heuristic_v2.h"
#include "toml.hpp"
#include "raylib.h"

struct TileData {
    float u, v;
    bool unique;
    bool one_per_region;
    std::string slots[4];
    const std::string name;
    const std::string group;
};

class Tileset {
public:
    enum Relationships {
        UP,
        DOWN,
        LEFT,
        RIGHT,
        SAME_REGION,
        OTHER_REGION,
    };

    Tileset(const char* path);
    ~Tileset() {
        if (tilemapTex.id > 0)
            UnloadTexture(tilemapTex);
    }

    float tileSize; 

    Tile* GetWFCTiles() {
        return &wfcTiles[0];
    }
    const int GetTileCount() const { return wfcTiles.size(); }
    void ConfigureWFC(WFC_State* wfc);
    void DrawTile(int tileIdx, float x, float y, float scale);
    std::vector<TileData> tiles;
private:
    toml::table tilesetTOML;

    std::vector<Tile> wfcTiles;
    Texture tilemapTex;
};

inline Tileset::Tileset(const char* path) {

    auto RegisterTile = [this](std::string tilename, std::string tileGroup, toml::table tileTable) -> int {
        TileData tile { .name = tilename, .group = tileGroup };

        tile.u = tileTable[toml::path ("uv")][0].value_or(0.f);
        tile.v = tileTable[toml::path ("uv")][1].value_or(0.f);
        tile.unique = tileTable[toml::path ("unique")].value_or(false);
        tile.slots[0] =  tileTable[toml::path ("slot_up")].value_or("");
        tile.slots[1] = tileTable[toml::path ("slot_down")].value_or("");
        tile.slots[2] = tileTable[toml::path ("slot_left")].value_or("");
        tile.slots[3] = tileTable[toml::path ("slot_right")].value_or("");

        Tile wfcTile = { (int) this->tiles.size(), tileTable[static_cast<toml::path>("weight")].value_or(1.f) };

        this->tiles.push_back(tile);
        this->wfcTiles.push_back(wfcTile);

        return wfcTile.val;
    };

    try
    {
        tilesetTOML = toml::parse_file(path);

        tileSize = tilesetTOML[toml::path ("tileSize")].value_or(16);

        std::string path = tilesetTOML[toml::path ("path")].value_or("");
        if (!path.empty())
            tilemapTex = LoadTexture(path.c_str());

        for (auto [tilename, tiledata] : tilesetTOML.ref<toml::table>())
        {
            if (!tiledata.is_table())
                continue;

            if (auto uv = tiledata[toml::path ("uv")].as_array())
            {
                std::cout << "Tile: " << tilename << '\n';
                RegisterTile(std::string(tilename), "", tiledata.ref<toml::table>());
            }
            else
            {
                std::cout << "Group: " << tilename << '\n';

                bool one_per_region = tiledata[toml::path ("one_per_region")].value_or(false);

                for (auto [groupedTile, gTileData] : tiledata.ref<toml::table>())
                {
                    if (!gTileData.is_table())
                        continue;

                    std::cout << "Tile: " << groupedTile << '\n';
                    int idx = RegisterTile(std::string(groupedTile), std::string (tilename), gTileData.ref<toml::table>());

                    tiles[idx].one_per_region = one_per_region;
                }
            }
        }
    }
    catch (const toml::parse_error& err)
    {
        throw;
    }
}

inline void Tileset::ConfigureWFC(WFC_State* wfc)
{
    if (wfc == NULL || !wfc->initialized)
        return;

    // Compares every tile to match slots.
    // When it comes to SAME_REGION and OTHER_REGION:
    // 1) The same tile should allow itself in the same region.
    // 2) The same tile should only allow itself in other regions if it's not unique.
    // 3) All tiles should allow all other tiles in other regions.
    for (int t1Idx = 0; t1Idx < tiles.size(); t1Idx++)
    {
        for (int t2Idx = t1Idx; t2Idx < tiles.size(); t2Idx++)
        {
            TileData& t1 = tiles[t1Idx];
            TileData& t2 = tiles[t2Idx];

            // "one per region" tiles prohibit any other tile of the same group in the same region
            // the "one per region" rule should only apply for tiles of the same group, and we don't want to block the same tile from appearing again in the same region if it's "one per region".
            bool limit_per_region = ((t1.one_per_region || t2.one_per_region ) && t1.group == t2.group && t1Idx != t2Idx);

            WFC_SetRule(wfc, Tile {t1Idx, 0}, Tile {t2Idx, 0}, SAME_REGION, !limit_per_region);
            WFC_SetRule(wfc, Tile {t2Idx, 0}, Tile {t1Idx, 0}, SAME_REGION, !limit_per_region);

            if (t1Idx == t2Idx)
            {
                // "unique" tiles do not allow themselves to appear in any other region. so we set this rule to false if unique
                WFC_SetRule(wfc, Tile {t1Idx, 0}, Tile {t2Idx, 0}, OTHER_REGION, !tiles[t1Idx].unique);
            }
            else
            {
                // all tiles by default should allow all others in other regions.
                WFC_SetRule(wfc, Tile {t1Idx, 0}, Tile {t2Idx, 0}, OTHER_REGION, true);
                WFC_SetRule(wfc, Tile {t2Idx, 0}, Tile {t1Idx, 0}, OTHER_REGION, true);

                // if any of the two tiles are "one per region", we continue here to avoid accidentally allowing them to exist adjacent to one another in the direction loop below
                if (limit_per_region)
                    continue;
            }

            for (int d = 0; d < 4; d++)
            {
                int od = d % 2 == 0 ? d+1 : d-1; // Opposite direction

                std::stringstream t1SlotsStream (t1.slots[d]), t2SlotsStream (t2.slots[od]);
                std::vector<std::string> t1Slots, t2Slots;
                std::string token;

                while (t1SlotsStream >> token)
                {
                    t1Slots.push_back(token);
                }

                while (t2SlotsStream >> token)
                {
                    t2Slots.push_back(token);
                }

                // bool hasCommonEl = std::find_first_of (t1Slots.begin(), t1Slots.end(), t2Slots.begin(), t2Slots.end()) != t1Slots.end();
                bool hasCommonEl = false;

                bool groupBan = false;
                for (const auto& s1 : t1Slots)
                {
                    // tiles that have a "!<some group name>" prohibit all tiles of that group in that direction, so skip
                    if (s1[0] == '!' && std::string_view {s1.data() + 1, s1.size()-1} == t2.group)
                    {
                        groupBan = true;
                        goto GROUP_BAN;
                    }
                    for (const auto& s2 : t2Slots)
                    {
                        // tiles that have a "!<some group name>" prohibit all tiles of that group in that direction, so skip
                        if (s2[0] == '!' && std::string_view {s2.data() + 1, s2.size()-1} == t1.group)
                        {
                            groupBan = true;
                            goto GROUP_BAN;
                        }

                        if (s1 == s2)
                        {
                            hasCommonEl = true;
                            continue;
                        }
                    }
                }
            GROUP_BAN:
                if (groupBan)
                    continue;
                
                if (hasCommonEl)
                {
                    WFC_SetRule(wfc, Tile {t1Idx, 0}, Tile {t2Idx, 0}, d, true);
                    WFC_SetRule(wfc, Tile {t2Idx, 0}, Tile {t1Idx, 0}, od, true);
                }
            }
        }
    }

    // std::unordered_map<char, int> slotToGroup;
    // std::vector<std::vector<int>[4]> groupedTiles;


    // // Group up tiles by what slots they have at what directions
    // for (int i = 0; i < tiles.size(); i++)
    // {
    //     auto& tile = tiles[i];

    //     for (int d = 0; d < 4; d++)
    //     {
    //         // Add a slot to the containers if it's not present
    //         if (slotToGroup.find(tile.slots[d]) == slotToGroup.end()) {
    //             int idx = groupedTiles.size();
    //             slotToGroup.insert({tile.slots[d], idx});
    //             groupedTiles.emplace_back();
    //             groupedTiles[idx][d].push_back(i);
    //         }
    //         else
    //         {
    //             int idx = slotToGroup[tile.slots[d]];
    //             groupedTiles[idx][d].push_back(i);
    //         }
    //     }
    // }

    // // Now add the relationships between tiles with matching slots to the WFC.
    // for (int tIdx = 0; tIdx < tiles.size(); tIdx++)
    // {
    //     for (int d = 0; d < 4; d++)
    //     {
    //         // Opposite dir
    //         int od = d % 2 == 0 ? d+1 : d-1;
    //         auto& tilesWithSlotAtDir = groupedTiles[slotToGroup[tiles[tIdx].slots[d]]][od];
    //         for (const auto otIdx : tilesWithSlotAtDir)
    //         {
    //             WFC_SetRule(wfc, Tile {tIdx, 0}, Tile {otIdx, 0}, d, true);
    //             WFC_SetRule(wfc, Tile {otIdx, 0}, Tile {tIdx, 0}, d, true);
    //         }
    //     }
    // }

    // // Add relationships for tiles in the same region
    // for (int t1 = 0; t1 < tiles.size(); t1++)
    // {
    //     for (int t2 = 0; t2 < tiles.size(); t2++)
    //     {
    //         WFC_SetRule(wfc, Tile {t1, 0}, Tile {t2, 0}, SAME_REGION, true);
    //         WFC_SetRule(wfc, Tile {t1, 0}, Tile {t2, 0}, OTHER_REGION, !tiles[t1].unique);

    //         WFC_SetRule(wfc, Tile {t2, 0}, Tile {t1, 0}, SAME_REGION, true);
    //         WFC_SetRule(wfc, Tile {t2, 0}, Tile {t1, 0}, OTHER_REGION, !tiles[t2].unique);
    //     }
    // }
}

inline void Tileset::DrawTile(int tileIdx, float x, float y, float scale)
{
    TileData& tile = tiles[tileIdx];
    Rectangle tileRect = {
        tile.u * tileSize,
        tile.v * tileSize,
        tileSize,
        tileSize,
    };

    DrawTexturePro(
        tilemapTex, tileRect,
        Rectangle { x, y, scale, scale, },
        {0, 0},
        0,
        WHITE
    );
}

#endif // TILESET_H
