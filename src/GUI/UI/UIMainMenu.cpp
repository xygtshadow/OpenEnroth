#include "GUI/UI/UIMainMenu.h"

#include <array>
#include <string>
#include <vector>

#include "Engine/EngineGlobals.h"
#include "Engine/Localization.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Image.h"
#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"

#include "Io/Mouse.h"

#include "Library/Platform/Interface/Platform.h"

#include "Media/Audio/AudioPlayer.h"

// MM6's menu buttons are baked into title.pcx, and the button under the cursor is overdrawn with
// one frame of a seven-step glow ramp ("start%02d%c", letters a-d = NEW/LOAD/CREDITS/EXIT, loaded
// @0x4508f9). Frame 0 is the resting button, frame 6 the brightest.
static constexpr int kMm6GlowFrameCount = 7;
// The pulse steps every 50ms (delay double @0x4b9468) and ping-pongs 0..6 and back, so a full
// round trip is 12 steps.
static constexpr int kMm6GlowStepMs = 50;
static constexpr int kMm6GlowPeriodSteps = 2 * kMm6GlowFrameCount - 2;

// MM7 stacks the menu buttons at x=495 from y=172 with a 55px step. MM6 bakes its buttons into
// title.pcx along the top right and creates 132x44 hitboxes at x=482 from y=9 with a 62px step
// (MM6.EXE CreateButton calls @0x4507bf/0x450815/0x45086c/0x4508c5, glow frames blitted at the
// same coords by the menu loop @0x450aa8).
static Pointi mainMenuButtonPos(int index) {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return {482, 9 + 62 * index};
    return {495, 172 + 55 * index};
}

// MM6.EXE advances a single frame counter shared by all four buttons: `v += dir`, then `dir = +1`
// below 1 and `dir = -1` above 5 (@0x450b52). It is free-running - it ticks whether or not a
// button is hovered - so moving the cursor between buttons continues the pulse in phase instead of
// restarting it, which a stateless triangle wave over the tick count reproduces exactly.
int mm6MainMenuGlowFrame(int64_t tickCountMs) {
    int step = (tickCountMs / kMm6GlowStepMs) % kMm6GlowPeriodSteps;
    return step < kMm6GlowFrameCount ? step : kMm6GlowPeriodSteps - step;
}

GUIWindow_MainMenu::GUIWindow_MainMenu() :
    GUIWindow(WINDOW_MainMenu, {0, 0}, render->GetRenderDimensions()) {
    main_menu_background = assets->getImage_PCXFromIconsLOD("title.pcx");

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;

    // A button's textures are the images it can be overdrawn with while hovered: a single lit
    // backlight image in MM7, the whole glow ramp in MM6, where the texture index is the frame.
    std::array<std::vector<GraphicsImage *>, 4> textures;
    if (isMm6) {
        ui_mainmenu_new = ui_mainmenu_load = ui_mainmenu_credits = ui_mainmenu_exit = nullptr;
        for (int index = 0; index < std::ssize(textures); index++)
            for (int frame = 0; frame < kMm6GlowFrameCount; frame++)
                textures[index].push_back(assets->getImage_Alpha(fmt::format("start{:02}{:c}", frame, static_cast<char>('a' + index))));
    } else {
        ui_mainmenu_new = assets->getImage_ColorKey("title_new");
        ui_mainmenu_load = assets->getImage_ColorKey("title_load");
        ui_mainmenu_credits = assets->getImage_ColorKey("title_cred");
        ui_mainmenu_exit = assets->getImage_ColorKey("title_exit");
        textures = {{{ui_mainmenu_new}, {ui_mainmenu_load}, {ui_mainmenu_credits}, {ui_mainmenu_exit}}};
    }

    // MM6's hitboxes are slightly smaller than the 135x45 glow frames - keep the EXE's numbers.
    Sizei mm6ButtonSize = {132, 44};

    pBtnNew = CreateButton("MainMenu_NewGame", mainMenuButtonPos(0), isMm6 ? mm6ButtonSize : ui_mainmenu_new->size(), BUTTON_TYPE_NORMAL, 0,
                           UIMSG_MainMenu_ShowPartyCreationWnd, 0, INPUT_ACTION_NEW_GAME, "", textures[0]);
    pBtnLoad = CreateButton("MainMenu_LoadGame", mainMenuButtonPos(1), isMm6 ? mm6ButtonSize : ui_mainmenu_load->size(), BUTTON_TYPE_NORMAL, 0,
                            UIMSG_MainMenu_ShowLoadWindow, 1, INPUT_ACTION_LOAD_GAME, "", textures[1]);
    pBtnCredits = CreateButton("MainMenu_Credits", mainMenuButtonPos(2), isMm6 ? mm6ButtonSize : ui_mainmenu_credits->size(), BUTTON_TYPE_NORMAL, 0,
                               UIMSG_ShowCredits, 2, INPUT_ACTION_SHOW_CREDITS, "", textures[2]);
    pBtnExit = CreateButton("MainMenu_ExitGame", mainMenuButtonPos(3), isMm6 ? mm6ButtonSize : ui_mainmenu_exit->size(), BUTTON_TYPE_NORMAL, 0,
                            UIMSG_ExitToWindows, 3, INPUT_ACTION_EXIT_GAME, "", textures[3]);
}

GUIWindow_MainMenu::~GUIWindow_MainMenu() {
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // The glow ramp is only ever referenced through the buttons that own it.
        for (GUIButton *button : vButtons)
            for (GraphicsImage *texture : button->vTextures)
                texture->release();
    } else {
        ui_mainmenu_new->release();
        ui_mainmenu_load->release();
        ui_mainmenu_credits->release();
        ui_mainmenu_exit->release();
    }
    main_menu_background->release();
}

void GUIWindow_MainMenu::Update() {
    render->DrawQuad2D(main_menu_background, {0, 0});

    // The hovered button is overdrawn: MM7 with its lit backlight image, MM6 with the current frame
    // of the glow ramp (MM6.EXE hit-tests the cursor against every button rect @0x450a81 and blits
    // the frame in place @0x450aaf).
    int frame = engine->gameVersion() == GAME_VERSION_MM6 ? mm6MainMenuGlowFrame(platform->tickCount()) : 0;

    Pointi pt = mouse->position();

    for (GUIButton *pButton : vButtons)
        if (pButton->Contains(pt.x, pt.y))
            render->DrawQuad2D(pButton->vTextures[frame], mainMenuButtonPos(pButton->msg_param));
}

void GUIWindow_MainMenu::processMessage(UIMessageType message) {
    // MM6 has no pressed state at all - its menu loop only ever draws the hover glow - but it does
    // click (StartMainChoice02 is id 75 in MM6's own dsounds.bin).
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        switch (message) {
        case UIMSG_MainMenu_ShowPartyCreationWnd:
        case UIMSG_MainMenu_ShowLoadWindow:
        case UIMSG_ShowCredits:
        case UIMSG_ExitToWindows:
            pAudioPlayer->playUISound(SOUND_StartMainChoice02);
            break;
        default:
            break;
        }
        return;
    }

    // Play the sound and change visual connected to the related button.
    switch (message) {
    case UIMSG_MainMenu_ShowPartyCreationWnd:
        new OnButtonClick(mainMenuButtonPos(0), {0, 0}, pBtnNew);
        break;
    case UIMSG_MainMenu_ShowLoadWindow:
        new OnButtonClick(mainMenuButtonPos(1), {0, 0}, pBtnLoad);
        break;
    case UIMSG_ShowCredits:
        new OnButtonClick(mainMenuButtonPos(2), {0, 0}, pBtnCredits);
        break;
    case UIMSG_ExitToWindows:
        new OnButtonClick(mainMenuButtonPos(3), {0, 0}, pBtnExit);
        break;
    default:
        break;
    }
}
