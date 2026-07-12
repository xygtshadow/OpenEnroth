#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "GUI/GUIWindow.h"

class GUIFont;

/**
 * Vertical scroll of MM6's new-game prologue crawl - the narrative text that pans over the sky
 * down to the New Sorpigal gate, on the screen that offers Create Party / Quick Start.
 *
 * @param elapsedMs                 Milliseconds since the prologue screen opened.
 * @return                          Row of `seg_scrl.pcx` to show at the top of the viewport, in
 *                                  `[0, 580]`.
 */
int mm6SegueScrollY(int64_t elapsedMs);

/**
 * MM6's new-game prologue ("segue") screen, MM6.EXE @0x452bd0. Sits between the main menu's New
 * Game and party creation: the prologue text crawls up over a backdrop that pans from the sky down
 * to the New Sorpigal gate, and the party can be created from scratch (Create Party) or taken
 * as-is (Quick Start).
 *
 * Note that this is NOT a bit-exact port of the original's drawing. MM6 blits the text into its
 * copy of `seg_scrl.pcx` once and then scrolls the bitmap; we draw the text over the scrolled
 * bitmap every frame and let the renderer's clip rect cut it. The result on screen is the same,
 * but it needs no scratch bitmap.
 */
class GUIWindow_Mm6Segue : public GUIWindow {
 public:
    GUIWindow_Mm6Segue();
    virtual ~GUIWindow_Mm6Segue();

    virtual void Update() override;

    /**
     * Draws the pressed frame of the button that the passed message came from. Mirrors
     * `GUIWindow_MainMenu::processMessage` - call it from the game state that owns this window.
     *
     * The owning state must also pump `GUI_UpdateWindows()` before it transitions away, the way
     * `MainMenuState::update()` does: this spawns an `OnButtonClick` that holds a raw pointer to a
     * button *this* window owns and dereferences it one frame later, so tearing the window down
     * without that pump leaves the click dangling.
     *
     * @param message               Message that was just posted by one of this window's buttons.
     */
    void processMessage(UIMessageType message);

 private:
    /**
     * One wrapped line of the crawl. Both fields are fixed for the life of the window - the crawl is
     * laid out once, in the constructor, so that `Update` does nothing but draw.
     */
    struct PrologueLine {
        std::string text;
        int offsetX = 0; // Centers the line in the crawl's rect, see `GUIFont::AlignText_Center`.
    };

 private:
    std::unique_ptr<GUIFont> _font;

    GraphicsImage *_background = nullptr;   // segue_bg.pcx - frame, banner, and both button faces.
    GraphicsImage *_scroll = nullptr;       // seg_scrl.pcx - 512x900, sky at the top, the gate at the bottom.
    GraphicsImage *_createPressed = nullptr;
    GraphicsImage *_quickPressed = nullptr;

    std::vector<PrologueLine> _prologueLines; // intro.str, wrapped and centered once, at construction.
    int _lineSpacing = 0;
    int64_t _startedMs = 0;

    GUIButton *_btnCreateParty = nullptr;
    GUIButton *_btnQuickStart = nullptr;
};
