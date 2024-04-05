# Relationship-based Graph WFC

In this repository is an implementation of an extension to the WaveFunctionCollapse algorithm, as described in my Bachelor's Thesis (link pending).

The extension aims to extend WFC to work in non-grid scenarios, with the goal of being as flexible as possible when it comes to usage in different contexts. The implementation is made as a single header library in pure C.

There are a total of five examples implementing the extension:
- `wfc`
- `wfc_overlap`
- `sudoku`
- `graph`
- `regions`

## Running the code.

To build the examples, you can use the following commands.
``` shell
git clone --recurse-submodules https://github.com/Mateus-Carmo31/wfc-extension.git
cd wfc-extension
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCUSTOMIZE_BUILD=On -DSUPPORT_FILEFORMAT_SVG=On -G "Unix Makefiles" ..
make
```

## Controls

All examples utilize similar control schemes:
- **S**: WFC Generation Step (disables auto-stepping if enabled)
- **Space***: Enable/disable auto-stepping (automatically do a WFC step)
- **Enter**: Run entire WFC generation
- **R**: Reset current generation

`regions` example:
- **E**: Enable/disable region display

## Description of the Examples

### `wfc`

This example replicates the behavior of the standard WFC implementation by Maxim Gumin with the extension. A selector for tilesets is available on the right of the interface, and a button below allows for loading the currently selected tileset.

In this example, the extension will generate a map using the tiles provided by the tileset. The TinyXML2 library is used to load the XML files that provide the rules used in the WFC process.

### `wfc_overlap`

This example replicates the behavior of the overlapping WFC implementation by Maxim Gumin, but with the extension as backend.

A selector for example samples that will be used in the tile generation is available on the top left of the interface. The size of the extracted patterns can be selected from in the button below that.

The "periodic" button determines whether generation should consider patterns that "loop" around the edges of the sample.

This example suffers from possible inability of generating textures with certain samples: some samples won't be able to generate non-contradictory waves under certain conditions. In this case, an early shutdown is enabled when the number of resets exceed some threshold. The program will also print to the stdout when that occurs.

### `sudoku`

This example aims to generate a valid sudoku board through the extension.

The board will start with a selection of tiles already set, to add extra constraints to the generation. The time until resolution of a valid board can take rather long, so it can be better to skip generation with Enter.

### `graph`

This examples generates a branching map path using the extension. Each node in the path can be one of 6 possible types:

- Door
- Unknown Encounter (a question mark)
- Chest
- Key
- Bedroll
- Enemy

The rules of placement for the map allow each type to be followed by any other type, *with the exception of chests and doors*. Doors and chests can only appear after a key.

### `regions`

This examples generates a WFC-like map using tiles. However, the example also generates a set of regions, to which every tile of the map is assigned to one. Regions allow for rules over tiles in relation to all cells in the same region, or over other regions as well.

The rules are described in a TOML file, which also defines each tile that is available for generation. The structure of the file is as follows:

- Tiles are grouped into groups (eg. "grass1" is in group "grass").
- Tiles have a uv element that dictates where in the texture file that tile is.
- Each tile has a "slot" string for each direction. Tiles with at least one of the same tokens in the slot string will be allowed next to each other in the matching directions (eg. tiles with at least one matching token in directions right and left, up and down, down and up...). If one of the tokens starts with '!', the rest of the token will be parsed as the name of a group, and the tile won't allow any tile of that group in the slot direction.
- Tile groups with the property "one_per_region" forbid tiles from the same group appearing in the same region.
- Tiles with the property "unique" forbid themselves from appearing in any other region.

In the example, these functionalities are used in the following manner:
- Grass tiles can appear alongside each other.
- Roads can be surrounded by grass, but must connect to each other and terminate at proper endpoint tiles.
- Cities can only be surrounded by grass, and only allow roads under them.
- Only one color of city can appear per region (one per region)
- If a city of some color appears in a region, it doesn't appear again in any other regions (unique).

## External dependencies/assets

All examples make use of the Raylib library to perform rendering and input handling. The XML parsing library TinyXML2 is used in example `wfc`, and the TOML parsing library TOML++ is used in example `regions`.

Asset sources:
`wfc` and `wfc_overlap`:
- The tilesets and samples used are taken as-is from Maxim Gumin's original WFC repository (available [here](https://github.com/mxgmn/WaveFunctionCollapse/tree/master)).
- His description XML files are also used to extract the rules for each tileset.

`graph`:
- The images used to represent each node type were designed and drawn by me.

`regions`:
- The tileset used is a free-to-use asset provided by Kenney.nl.
