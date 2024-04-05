// ui.h
//
#ifndef UI_H
#define UI_H 1

#include "raylib.h"
#include <utility>

class Button
{
public:
    Button(Vector2 size): size(size) {}
    Vector2 size;
    bool operator()(float x, float y)
    {
	Vector2 mousePos = GetMousePosition();
	Rectangle bounds = {x, y, size.x, size.y};

	if (CheckCollisionPointRec(mousePos, bounds))
	{
	    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
		DrawRectangleRec(bounds, pressedCol);
	    else
		DrawRectangleRec(bounds, hoverCol);
	}
	else
	{
	    DrawRectangleRec(bounds, standbyCol);
	}

	if (CheckCollisionPointRec(mousePos, bounds))
	{
	    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
	    {
		return true;
	    }
	    else
	    {
		return false;
	    }
	}

	return false;
    }
private:
    static constexpr Color standbyCol = LIGHTGRAY;
    static constexpr Color hoverCol = GRAY;
    static constexpr Color pressedCol = DARKGRAY;
};

class TilesetSelector
{
public:
    TilesetSelector(std::pair<const char*, int>* tilesets, int count): tilesets(tilesets), count(count) {}
    int current = 0;
    void operator()(float x, float y)
    {
	const int fontSize = 20;
	const float padding = 4;
	int labelSize = MeasureText("Tileset: 00", fontSize);
	Rectangle bounds = {x, y, labelSize + padding, fontSize + padding};
	DrawRectangleRec(bounds, GRAY);
	DrawText("Tileset: ", x+padding/2, y+padding/2, fontSize, BLACK);
	DrawText(TextFormat("%d", current+1), x + padding/2 + labelSize - fontSize, y + padding/2, fontSize, BLACK);

	Vector2 mousePos = GetMousePosition();
	if (CheckCollisionPointRec(mousePos, bounds) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
	    current = (current+1) % count;
	}
    }
private:
    std::pair<const char*, int>* tilesets;
    int count;
};

#endif // UI_H
