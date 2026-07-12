#include "GUI/UI/UIMm6Segue.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineGlobals.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Resources/ResourceManager.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"

#include "Library/Platform/Interface/Platform.h"

#include "Utility/String/Split.h"

// MM6's prologue crawl is two phases (menu loop @0x453193-0x453223). Both of the loop's deadlines
// are seeded with `start + 0x7148` (@0x452fba) and the pan is gated on a strict `jbe`, so nothing
// moves for the first 29s. The deadlines are compared against KERNEL32!GetTickCount (IAT slot
// 0x4b9080) - that's what pins the units to milliseconds: 0x7148 = 29000ms, 0x32 = 50ms.
static constexpr int64_t kMm6SegueHoldMs = 29000;
// After the hold the view pans down one pixel per step (@0x453217).
static constexpr int64_t kMm6SegueStepMs = 50;
// ...until it hits the bottom of the image (@0x4531fd). This bound is an asset property, not an EXE
// literal: the original computes it at runtime from the height of the loaded PCX (`movsx ecx,
// word [esp+0x32]; sub ecx, 0x140`). seg_scrl.pcx is 900 tall and the window it shows through is
// 320, so the pan stops at 580 - parking the gate scene at the bottom of seg_scrl.pcx in the
// window of the static segue_bg.pcx frame. `Mm6.SegueAssets` pins seg_scrl.pcx's height, which is
// what keeps this constant honest.
static constexpr int kMm6SegueMaxScrollY = 580;

// The window that seg_scrl.pcx shows through, cut into segue_bg.pcx. The original doesn't spell the
// origin out as coordinates - it blits the scrolled rows straight to framebuffer offset 0x20880
// (@0x452e6c), and at 16bpp on a 640-wide buffer that's 0x20880 / 2 = 104 * 640 + 64.
static constexpr Recti kMm6SegueViewport = {64, 104, 512, 320};

// MM6 draws the prologue text into its own copy of seg_scrl.pcx and scrolls the result, so on
// screen the text sits inset from the top left of the viewport and scrolls with it.
//
// The x inset is the EXE's: the text pass is a call to the shared text routine @0x443a40, whose
// `edx` argument is the left edge of the text rect - it's what that routine's `right = edx + w - 1`
// is built from (@0x443a65) - and the segue passes `edx = 0x14` (@0x453128). The y inset is NOT
// separately verified: the routine is handed y = 0 plus a destination pointer we didn't trace. But
// the same call also shortens the rect's height by 20 (`sub eax, 0x14` @0x453125), which only makes
// sense as the matching top inset.
static constexpr Pointi kMm6SegueTextOrigin = {20, 20};
// The rect's width is likewise the image width less the inset (`add ecx, -0x14` @0x45311d): the
// crawl is inset on the left and runs flush to the right edge of the window.
static constexpr int kMm6SegueTextWidth = kMm6SegueViewport.w - kMm6SegueTextOrigin.x;

// MM6.EXE creates the two buttons @0x452ec7 and @0x452ef1 (both are call sites of CreateButton,
// which itself lives @0x41a170). Their resting faces are painted into segue_bg.pcx, so the only
// button art the screen ships is the pressed frames - creat_dn and quick_dn, which are 218x40
// against these 217x41 hitboxes: a pixel wider and a pixel shorter.
//
// The two click handlers (@0x42fdea Create Party, @0x42fe0e Quick Start) flash that frame by handing
// the button object to the blitter @0x419320, which takes the button's own stored x/y (`mov ecx,
// [eax]` / `mov edx, [eax + 4]`) as its origin - so the frame is drawn at the hitbox origin, and
// these are the draw positions as well as the hitboxes.
static constexpr Pointi kMm6SegueCreatePartyPos = {74, 434};
static constexpr Pointi kMm6SegueQuickStartPos = {350, 434};
static constexpr Sizei kMm6SegueButtonSize = {217, 41};

// The original re-arms its deadline from the current time after each pixel (`now + 0x32`
// @0x453211) and moves at most one pixel per frame, so its pan actually drifts with frame time.
// This closed form is the idealization of that - frame-rate independent, and testable, which a
// 58-second crawl driven through the engine is not.
int mm6SegueScrollY(int64_t elapsedMs) {
    if (elapsedMs < kMm6SegueHoldMs)
        return 0;
    return static_cast<int>(std::min<int64_t>((elapsedMs - kMm6SegueHoldMs) / kMm6SegueStepMs, kMm6SegueMaxScrollY));
}

GUIWindow_Mm6Segue::GUIWindow_Mm6Segue() :
    GUIWindow(WINDOW_Mm6Segue, {0, 0}, render->GetRenderDimensions()) {
    _font = GUIFont::LoadFont("quick.fnt"); // @0x452c46.

    _background = assets->getImage_PCXFromIconsLOD("segue_bg.pcx");
    _scroll = assets->getImage_PCXFromIconsLOD("seg_scrl.pcx");
    _createPressed = assets->getImage_Alpha("creat_dn");
    _quickPressed = assets->getImage_Alpha("quick_dn");

    // intro.str is a run of NUL-terminated paragraphs, and the original rewrites every NUL in the
    // loaded buffer into a newline before drawing it (@0x452dfe-0x452e0a). Skip that and the crawl
    // is a single paragraph cut off at the first NUL.
    std::string prologue(engine->resources()->eventsData("intro.str").str());
    std::ranges::replace(prologue, '\0', '\n');

    // The crawl's layout is fixed for the life of the window, so do it once here instead of once
    // per frame. MM6 steps `fontHeight - 3` rows between lines: its text routine reads the height
    // byte of the .fnt header (`mov cl, byte [edi + 5]`), subtracts 3, and walks the destination
    // pointer down by that many rows for each line of the string (@0x443b0a-0x443b1f). quick.fnt is
    // 20 tall, so the step is 17 - and that is exactly what `GUIWindow::DrawText` does, since
    // OpenEnroth's font code is a port of this same routine.
    //
    // We still lay the lines out by hand rather than call DrawText: this wraps once instead of once
    // per frame, and it lets the clip rect cut the lines that straddle the window's top and bottom
    // edges, where DrawText's `maxY` would drop such a line whole.
    //
    // OPEN QUESTION: the lines are left-aligned here, matching `GUIFont::DrawText`, which keeps
    // centering in a separate `AlignText_Center` helper for callers that want it. But MM6's routine
    // has what looks like an ungated per-line `max(0, (rectWidth - lineWidth) / 2)` (@0x443ac1),
    // which would mean the original centers the crawl. The stack arithmetic there did not resolve
    // cleanly enough to call it, so this is left as-is until someone eyeballs the real game: the
    // tell is the short final line, "Good Luck" - centered in MM6, hard left here.
    std::string wrapped = _font->WrapText(prologue, kMm6SegueTextWidth, 0);
    for (std::string_view line : split(wrapped).by('\n'))
        _prologueLines.emplace_back(line);
    _lineSpacing = _font->GetHeight() - 3;

    _startedMs = platform->tickCount();

    _btnCreateParty = CreateButton("Mm6Segue_CreateParty", kMm6SegueCreatePartyPos, kMm6SegueButtonSize,
                                   BUTTON_TYPE_NORMAL, 0, UIMSG_Mm6Segue_CreateParty, 0, INPUT_ACTION_INVALID, "",
                                   {_createPressed});
    _btnQuickStart = CreateButton("Mm6Segue_QuickStart", kMm6SegueQuickStartPos, kMm6SegueButtonSize,
                                  BUTTON_TYPE_NORMAL, 0, UIMSG_Mm6Segue_QuickStart, 0, INPUT_ACTION_INVALID, "",
                                  {_quickPressed});

    // A zero-sized button that exists only to carry the escape hotkey (@0x452f32). MM6 also binds
    // 'C' and 'Q' to the two buttons, but we deliberately don't - hotkeys in OpenEnroth are
    // config-bound `InputAction`s, and two more of them for a single screen isn't worth it.
    CreateButton({0, 0}, {0, 0}, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_ESCAPE);
}

GUIWindow_Mm6Segue::~GUIWindow_Mm6Segue() {
    _background->release();
    _scroll->release();
    _createPressed->release();
    _quickPressed->release();
}

void GUIWindow_Mm6Segue::Update() {
    render->DrawQuad2D(_background, {0, 0});

    int scrollY = mm6SegueScrollY(platform->tickCount() - _startedMs);

    render->SetUIClipRect(kMm6SegueViewport);
    render->DrawQuad2D(_scroll, {kMm6SegueViewport.x, kMm6SegueViewport.y - scrollY});

    // Both color arguments below are the text color: the second one is what a `\f` tag resets to,
    // not a shadow color, and intro.str carries no tags anyway. The shadow is the one baked into
    // the font's glyphs, which `GUIFont` draws black. MM6 shadows differently - it draws the whole
    // crawl twice, first in color 1 (near-black at 16bpp) offset a pixel down and right
    // (@0x452e33), then in #ECE69C (@0x453132) - so ours is a drop shadow of the same shape, but
    // not of the same pixels.
    int textX = kMm6SegueViewport.x + kMm6SegueTextOrigin.x;
    int textY = kMm6SegueViewport.y + kMm6SegueTextOrigin.y - scrollY;
    for (const std::string &line : _prologueLines) {
        _font->DrawTextLine(line, colorTable.Primrose, colorTable.Primrose, {textX, textY}); // #ECE69C, @0x4530df-0x453105.
        textY += _lineSpacing;
    }

    // Text is batched and drawn under whatever clip rect is set when the batch is finally flushed,
    // so it has to be flushed here, while the clip rect is still the viewport. Otherwise the lines
    // straddling the top and bottom edges of the window would spill out of it.
    render->EndTextNew();

    render->ResetUIClipRect();
}

void GUIWindow_Mm6Segue::processMessage(UIMessageType message) {
    // Both buttons' resting faces are part of segue_bg.pcx, so a button is only ever drawn while
    // pressed. Unlike MM6's main menu, which has no pressed state at all and only ever draws a
    // hover glow, this screen really does flash its buttons (@0x42fdea / @0x42fe0e). OpenEnroth has
    // no held-button state to hold the frame for, so this is the one-frame flash that MM7's main
    // menu uses.
    switch (message) {
    case UIMSG_Mm6Segue_CreateParty:
        new OnButtonClick(kMm6SegueCreatePartyPos, {0, 0}, _btnCreateParty);
        break;
    case UIMSG_Mm6Segue_QuickStart:
        new OnButtonClick(kMm6SegueQuickStartPos, {0, 0}, _btnQuickStart);
        break;
    default:
        break;
    }
}
