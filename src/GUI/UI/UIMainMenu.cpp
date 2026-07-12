#include "GUI/UI/UIMainMenu.h"

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

// MM7 stacks the menu buttons at x=495 from y=172 with a 55px step. MM6 bakes its buttons into
// title.pcx along the top right and creates 132x44 hitboxes at x=482 from y=9 with a 62px step
// (MM6.EXE CreateButton calls @0x4507bf/0x450815/0x45086c/0x4508c5, pressed frames blitted at the
// same coords by the menu loop @0x450aa8).
static Pointi mainMenuButtonPos(int index) {
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return {482, 9 + 62 * index};
    return {495, 172 + 55 * index};
}

GUIWindow_MainMenu::GUIWindow_MainMenu() :
    GUIWindow(WINDOW_MainMenu, {0, 0}, render->GetRenderDimensions()) {
    main_menu_background = assets->getImage_PCXFromIconsLOD("title.pcx");

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    if (isMm6) {
        // MM6 has no hover highlight; start06a-d are the fully-pressed frames of the press
        // animation the EXE loads as "start%02d%c" (@0x4508f9), drawn while a button is held.
        ui_mainmenu_new = assets->getImage_Alpha("start06a");
        ui_mainmenu_load = assets->getImage_Alpha("start06b");
        ui_mainmenu_credits = assets->getImage_Alpha("start06c");
        ui_mainmenu_exit = assets->getImage_Alpha("start06d");
    } else {
        ui_mainmenu_new = assets->getImage_ColorKey("title_new");
        ui_mainmenu_load = assets->getImage_ColorKey("title_load");
        ui_mainmenu_credits = assets->getImage_ColorKey("title_cred");
        ui_mainmenu_exit = assets->getImage_ColorKey("title_exit");
    }

    // MM6's hitboxes are slightly smaller than the 135x45 pressed frames - keep the EXE's numbers.
    Sizei mm6ButtonSize = {132, 44};

    pBtnNew = CreateButton("MainMenu_NewGame", mainMenuButtonPos(0), isMm6 ? mm6ButtonSize : ui_mainmenu_new->size(), BUTTON_TYPE_NORMAL, 0,
                           UIMSG_MainMenu_ShowPartyCreationWnd, 0, INPUT_ACTION_NEW_GAME, "", {ui_mainmenu_new});
    pBtnLoad = CreateButton("MainMenu_LoadGame", mainMenuButtonPos(1), isMm6 ? mm6ButtonSize : ui_mainmenu_load->size(), BUTTON_TYPE_NORMAL, 0,
                            UIMSG_MainMenu_ShowLoadWindow, 1, INPUT_ACTION_LOAD_GAME, "", {ui_mainmenu_load});
    pBtnCredits = CreateButton("MainMenu_Credits", mainMenuButtonPos(2), isMm6 ? mm6ButtonSize : ui_mainmenu_credits->size(), BUTTON_TYPE_NORMAL, 0,
                               UIMSG_ShowCredits, 2, INPUT_ACTION_SHOW_CREDITS, "", {ui_mainmenu_credits});
    pBtnExit = CreateButton("MainMenu_ExitGame", mainMenuButtonPos(3), isMm6 ? mm6ButtonSize : ui_mainmenu_exit->size(), BUTTON_TYPE_NORMAL, 0,
                            UIMSG_ExitToWindows, 3, INPUT_ACTION_EXIT_GAME, "", {ui_mainmenu_exit});
}

GUIWindow_MainMenu::~GUIWindow_MainMenu() {
    ui_mainmenu_new->release();
    ui_mainmenu_load->release();
    ui_mainmenu_credits->release();
    ui_mainmenu_exit->release();
    main_menu_background->release();
}

void GUIWindow_MainMenu::Update() {
    render->DrawQuad2D(main_menu_background, {0, 0});

    // MM6's buttons are part of title.pcx and don't react to hover; only the click animation
    // (OnButtonClick) draws the pressed frame on top.
    if (engine->gameVersion() == GAME_VERSION_MM6)
        return;

    Pointi pt = mouse->position();

    for (GUIButton *pButton : vButtons) {
        if (pButton->Contains(pt.x, pt.y)) {
            // Backlight for the hovered button; vTextures[0] is the same lit image the button was
            // created with.
            render->DrawQuad2D(pButton->vTextures[0], mainMenuButtonPos(pButton->msg_param));
        }
    }
}

void GUIWindow_MainMenu::processMessage(UIMessageType message) {
    // Play the sound and change visual connected to the related button
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
