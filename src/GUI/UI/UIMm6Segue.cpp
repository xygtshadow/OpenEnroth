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
// (`add eax, 0x20880` @0x452f89 for the initial blit, `add edx, 0x20880` @0x453171 for the scrolling
// one), and at 16bpp on a 640-wide buffer that's 0x20880 / 2 = 104 * 640 + 64.
static constexpr Recti kMm6SegueViewport = {64, 104, 512, 320};

// MM6 draws the prologue text into its own copy of seg_scrl.pcx and scrolls the result, so on
// screen the text scrolls with the bitmap it's drawn into.
//
// The text pass is a call to the shared text routine @0x443a40. That routine's signature is
// f(ecx = font, edx = left, a1 = top, a2 = width, a3 = height, a4 = color, a5 = str, a6 = buf,
// a7 = pitch) - `right = left + width - 1` @0x443a65 and `bottom = top + height - 1` @0x443a6d are
// what pin the four rect arguments. The segue hands it `edx = 0x14` @0x453128, `a1 = ebx` @0x45312f,
// `a2 = imgWidth - 0x14` @0x45311d and `a3 = imgHeight - 0x14` @0x453125.
//
// Careful: neither of those two 0x14s is the origin the call site makes them look like.
//
// The 0x14 in `edx` is the *total* horizontal margin, because the routine HALVES it: `mov eax,
// [esp+0x10]; sar eax, 1` @0x443ac1-0x443ac7 stashes left >> 1, and that stash is what gets added
// back as each line's x (`add edx, esi` @0x443af9, on top of the clamped centering below). So MM6's
// per-line x is clamp0((492 - lineWidth) / 2) + 10, i.e. an inset of 10, not 20 - 10 to the left of
// the rect and 10 to the right of it. The art agrees: the crawl's widest line is 486px, and 10 + 3
// puts it at [13, 499] in the 512-wide bitmap - symmetric.
//
// And the top is the literal 0 in `ebx` (`xor ebx, ebx` @0x452bed, never reassigned), used raw: the
// routine's first destination row is `buf + top * pitch * 2` (@0x443aa6-0x443abc), and `buf` is the
// start of seg_scrl.pcx's pixels - the same pointer the scroll blit @0x453171 walks from row 0. So
// the crawl starts flush with the top of the scroll bitmap. The `sub eax, 0x14` @0x453125 that looks
// like a matching top inset is the rect's *height* - a bottom margin.
static constexpr Pointi kMm6SegueTextOrigin = {10, 0};
// The horizontal margin the routine splits in two - MM6's `edx` argument, before the halving. The
// rect's width is the image width less all of it (`add ecx, -0x14` @0x45311d).
static constexpr int kMm6SegueTextMargin = 20;
static constexpr int kMm6SegueTextWidth = kMm6SegueViewport.w - kMm6SegueTextMargin;
static_assert(kMm6SegueTextOrigin.x == kMm6SegueTextMargin / 2); // `sar eax, 1` @0x443ac5.

// MM6.EXE creates the two buttons @0x452ec7 and @0x452ef1 (both are call sites of CreateButton,
// which itself lives @0x41a170). Their resting faces are painted into segue_bg.pcx, so the only
// button art the screen ships is the pressed frames - creat_dn and quick_dn, which are 218x40
// against these 217x41 hitboxes: a pixel wider and a pixel shorter.
//
// The two click handlers (@0x42fdea Create Party, @0x42fe0e Quick Start) flash that frame by
// spawning a transient window through the window factory @0x419320 - the same function the segue
// itself calls to create its 640x480 window (@0x452eb9). They call it as f(x, y, 0, 0, type = 0x5a,
// button, 0), reading the origin out of the button object itself (`mov ecx, [eax]` / `mov edx,
// [eax + 4]`), and 0x5a = 90 is OpenEnroth's `WINDOW_PressedButton2` - i.e. `OnButtonClick`. So the
// frame is drawn at the hitbox origin, these are the draw positions as well as the hitboxes, and
// the `new OnButtonClick(...)` in processMessage() below is a structural match for what MM6 does.
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

int mm6SegueTextWidth() {
    return kMm6SegueTextWidth;
}

Pointi mm6SegueLinePos(int lineIndex, int lineOffsetX, int lineSpacing, int scrollY) {
    return {kMm6SegueViewport.x + kMm6SegueTextOrigin.x + lineOffsetX,
            kMm6SegueViewport.y + kMm6SegueTextOrigin.y + lineIndex * lineSpacing - scrollY};
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
    // And each line is centered in the crawl's rect. That same routine measures the line (call
    // @0x443acf) and starts it at `rectLeft / 2 + max(0, (rectWidth - lineWidth) / 2)` - the halved
    // difference, clamped at zero by a branchless `sar eax, 1; sets dl; dec edx; and edx, eax`
    // (@0x443ae4-0x443af7), added to the halved left edge (see kMm6SegueTextOrigin). It is
    // unconditional: the `test esi, esi` just above it is the strtok loop's condition (`esi` is the
    // line pointer), not an alignment flag. `GUIFont::AlignText_Center` is exactly the clamped
    // half-difference, so that's what we call, and kMm6SegueTextOrigin.x carries the halved left.
    //
    // Careful - this looks like it contradicts our own font code, and it doesn't. `GUIFont::DrawText`
    // is a port of this very routine, but it left-aligns and leaves centering to `AlignText_Center`
    // for callers that ask for it, so matching DrawText here would mean *not* matching MM6. Don't
    // "fix" it back. The art corroborates the disassembly: composited offline at the pan's resting
    // position (`scrollY` = 580), centering lands the crawl's last line, "Good Luck", above the New
    // Sorpigal gate's archway between its two pillars - the composition's focal point - while
    // left-aligned it sits in the corner of the window over a tree.
    //
    // The offsets are a pure function of the line and the font, both fixed here, so they're measured
    // once alongside the wrapping and `Update()` stays pure drawing.
    std::string wrapped = _font->WrapText(prologue, kMm6SegueTextWidth, 0);
    for (std::string_view line : split(wrapped).by('\n'))
        _prologueLines.push_back({std::string(line), _font->AlignText_Center(kMm6SegueTextWidth, line)});
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
    // crawl twice, first in color 1 (near-black at 16bpp) offset a pixel *down only* (@0x452e33),
    // then in #ECE69C (@0x453132) - so ours is a drop shadow of the same shape, but not of the same
    // pixels. Down only, not down and right: the shadow pass passes `edx = 0x15` (@0x452e2c) against
    // the text pass's 0x14, but the routine halves `edx` (see kMm6SegueTextOrigin) and 0x15 >> 1 ==
    // 0x14 >> 1 == 10, so the x offset collapses. Only `top` differs - 1 (@0x452e2a) against 0.
    for (int i = 0; i < std::ssize(_prologueLines); i++) {
        // #ECE69C, @0x4530df-0x453105. `offsetX` centers the line, see the constructor.
        const PrologueLine &line = _prologueLines[i];
        _font->DrawTextLine(line.text, colorTable.Primrose, colorTable.Primrose,
                            mm6SegueLinePos(i, line.offsetX, _lineSpacing, scrollY));
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
