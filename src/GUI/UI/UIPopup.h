#pragma once

#include <string>

#include "Engine/Objects/MonsterEnums.h"

#include "Library/Geometry/Point.h"
#include "Library/Geometry/Rect.h"

void DrawPopupWindow(int uX, int uY, int uWidth, int uHeight);  // idb

/** Side of the square portrait box that the MM7 monster info popup draws the monster's sprite into. */
constexpr int monsterPopupPortraitSize = 128;

/** MM6's monster popup is a fixed square with no content measuring, MM6.EXE 0x41161B. */
constexpr int monsterPopupMm6WindowSize = 256;

/**
 * Monster sprites are far taller than the portrait box, so both games ship a per-monster-family table saying
 * which part of the sprite the popup should frame.
 *
 * @param monsterId                 Monster whose portrait is being drawn.
 * @return                          Y offset at which the monster's sprite is drawn, negative offsets crop the top
 *                                  of the sprite. In MM7 it is relative to the top of the portrait box, in MM6 to
 *                                  the game's own portrait baseline - see `monsterPopupMm6PortraitTop`.
 */
int monsterPopupPortraitYOffset(MonsterId monsterId);

/**
 * @param mouseX                    Cursor x in 640x480 UI coordinates.
 * @return                          MM6's fixed popup rect, placed 30px from the cursor on whichever side fits.
 */
Recti monsterPopupMm6WindowRect(int mouseX);

/**
 * @param window                    Popup rect from `monsterPopupMm6WindowRect`.
 * @return                          The area MM6 gives the portrait - the window inset by 12 top/left and
 *                                  14 right/bottom, i.e. 230x230. MM6.EXE 0x41CFF6-0x41D038.
 */
Recti monsterPopupMm6PortraitArea(const Recti &window);

/**
 * MM6 measures the portrait from the window, not from the portrait area: the sprite buffer's top lands at
 * `window.y + 50 + offset`, 38px below the area's own top. MM6.EXE 0x41D06E.
 *
 * @param window                    Popup rect from `monsterPopupMm6WindowRect`.
 * @param monsterId                 Monster whose portrait is being drawn.
 * @return                          Screen y at which the sprite buffer's top row is drawn.
 */
int monsterPopupMm6PortraitTop(const Recti &window, MonsterId monsterId);

/**
 * If `mousePos` is over a character portrait, uses the picked item on that character and return true. Note that using
 * an item can either consume that item, or display an error string (E.g. "Crossbow can not be used that way").
 *
 * @param mousePos                  Mouse position in 640x480 UI coordinates.
 * @return                          Whether the picked item was consumed.
 */
bool tryUseItemOnPortrait(Pointi mousePos);

/**
 * Right-click dispatcher for the current screen. Called every frame from `GUI_UpdateWindows` while
 * `holdingMouseRightButton` is set, as its last step, so the resulting popup lands on top of every
 * window's `Update()` output.
 *
 * Not every branch is pure rendering. Does potion mixing, identification, repair, plays the monster-id speech reaction.
 *
 * @param mousePos                  Mouse position in 640x480 UI coordinates.
 * @offset 0x00416D62
 */
void UI_OnMouseRightClick(Pointi mousePos);

class GraphicsImage;

extern GraphicsImage *parchment;
extern GraphicsImage *messagebox_corner_x;       // 5076AC
extern GraphicsImage *messagebox_corner_y;       // 5076B4
extern GraphicsImage *messagebox_corner_z;       // 5076A8
extern GraphicsImage *messagebox_corner_w;       // 5076B0
extern GraphicsImage *messagebox_border_top;     // 507698
extern GraphicsImage *messagebox_border_bottom;  // 5076A4
extern GraphicsImage *messagebox_border_left;    // 50769C
extern GraphicsImage *messagebox_border_right;   // 5076A0

extern bool holdingMouseRightButton;
extern bool rightClickItemActionPerformed;
extern bool identifyOrRepairReactionPlayed;
extern bool monsterIdReactionPlayed;
