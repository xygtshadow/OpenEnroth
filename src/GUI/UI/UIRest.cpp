#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/Indoor.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Objects/NPC.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/Engine.h"
#include "Engine/Time/Timer.h"

#include "GUI/GUIButton.h"
#include "GUI/UI/UIRest.h"

GraphicsImage *rest_ui_btn_4 = nullptr;
GraphicsImage *rest_ui_btn_exit = nullptr;
GraphicsImage *rest_ui_btn_3 = nullptr;
GraphicsImage *rest_ui_btn_1 = nullptr;
GraphicsImage *rest_ui_btn_2 = nullptr;
GraphicsImage *rest_ui_restmain = nullptr;

GraphicsImage *rest_ui_sky_frame_current = nullptr;
GraphicsImage *rest_ui_hourglass_frame_current = nullptr;

int foodRequiredToRest;
Duration remainingRestTime;
RestType currentRestType;

static void prepareToLoadRestUI() {
    if (current_screen_type != SCREEN_GAME) {
        pGUIWindow_CurrentMenu = nullptr;
        current_screen_type = SCREEN_GAME;
    }
    pEventTimer->setPaused(true);
    if (currentRestType != REST_HEAL) {
        // The pressed-button overlay sits on the HUD rest button: MM7 (518, 450), MM6 (525, 399) (MM6.EXE 0x41d5ae).
        new OnButtonClick(pBtn_Rest->rect.topLeft(), {0, 0}, pBtn_Rest);
    }
    remainingRestTime = Duration();
    currentRestType = REST_NONE;
}

static void calculateRequiredFood() {
    if (uCurrentlyLoadedLevelType == LEVEL_OUTDOOR) {
        foodRequiredToRest = pOutdoor->getNumFoodRequiredToRestInCurrentPos(pParty->pos);
    } else {
        foodRequiredToRest = 2;
    }

    if (PartyHasDragon()) {
        ++foodRequiredToRest;
    }

    if (CheckHiredNPCSpeciality(Porter)) {
        --foodRequiredToRest;
    }
    if (CheckHiredNPCSpeciality(QuarterMaster)) {
        foodRequiredToRest -= 2;
    }
    if (CheckHiredNPCSpeciality(Gypsy)) {
        --foodRequiredToRest;
    }
    if (foodRequiredToRest < 1) {
        foodRequiredToRest = 1;
    }
    // MM7-only: MM6 map ids collide with MM7's MapId enum.
    if (engine->gameVersion() == GAME_VERSION_MM7 &&
        engine->_currentLoadedMapId == MAP_CASTLE_HARMONDALE && pParty->_questBits[QBIT_HARMONDALE_REBUILT]) {
        foodRequiredToRest = 0;
    }
}

GUIWindow_Rest::GUIWindow_Rest()
    : GUIWindow(WINDOW_Rest, {0, 0}, render->GetRenderDimensions()) {
    prepareToLoadRestUI();
    calculateRequiredFood();

    current_screen_type = SCREEN_REST;

    hourglassLoopTimer = 0_ticks;
    rest_ui_restmain = assets->getImage_Alpha("restmain");
    rest_ui_btn_1 = assets->getImage_Alpha("restb1");
    rest_ui_btn_2 = assets->getImage_Alpha("restb2");
    rest_ui_btn_3 = assets->getImage_Alpha("restb3");
    rest_ui_btn_4 = assets->getImage_Alpha("restb4");
    rest_ui_btn_exit = assets->getImage_Alpha("restexit");

    OutdoorLocation::LoadActualSkyFrame();

    // MM6's buttons sit 3px right and 7-11px below MM7's, and the three wait buttons are 27px tall,
    // not 33 (MM6.EXE rest ctor @0x41d560: exit (283, 308) 154x37, rest & heal (27, 161) 225x37,
    // dawn (64, 243) / 1 hour (64, 275) / 5 minutes (64, 307), all 154x27).
    bool mm6 = engine->gameVersion() == GAME_VERSION_MM6;
    Sizei waitButtonSize = mm6 ? Sizei(154, 27) : Sizei(154, 33);
    pButton_RestUI_Exit = CreateButton(mm6 ? Pointi(283, 308) : Pointi(280, 297), {154, 37}, BUTTON_TYPE_NORMAL, 0, UIMSG_ExitRest, 0, INPUT_ACTION_INVALID, "", {rest_ui_btn_exit});
    pButton_RestUI_Main = CreateButton("Rest_RestAndHeal", mm6 ? Pointi(27, 161) : Pointi(24, 154), {225, 37}, BUTTON_TYPE_NORMAL, 0, UIMSG_Rest8Hour, 0, INPUT_ACTION_REST_HEAL, "", {rest_ui_btn_4});
    pButton_RestUI_WaitUntilDawn = CreateButton("Rest_WaitTillDawn", mm6 ? Pointi(64, 243) : Pointi(61, 232), waitButtonSize, BUTTON_TYPE_NORMAL, 0, UIMSG_WaitTillDawn, 0, INPUT_ACTION_REST_WAIT_TILL_DAWN, "", {rest_ui_btn_1});
    pButton_RestUI_Wait1Hour = CreateButton("Rest_Wait1Hour", mm6 ? Pointi(64, 275) : Pointi(61, 264), waitButtonSize, BUTTON_TYPE_NORMAL, 0, UIMSG_Wait1Hour, 0, INPUT_ACTION_REST_WAIT_1_HOUR, "", {rest_ui_btn_2});
    pButton_RestUI_Wait5Minutes = CreateButton(mm6 ? Pointi(64, 307) : Pointi(61, 296), waitButtonSize, BUTTON_TYPE_NORMAL, 0, UIMSG_Wait5Minutes, 0, INPUT_ACTION_REST_WAIT_5_MINUTES, "", {rest_ui_btn_3});
}

void GUIWindow_Rest::Update() {
    GUIButton tmp_button;

    int liveCharacters = 0;
    for (Character &player : pParty->pCharacters) {
        if (!player.IsDead() && !player.IsEradicated() && player.health > 0) {
            ++liveCharacters;
        }
    }

    if (liveCharacters) {
        render->DrawQuad2D(rest_ui_restmain, {8, 8});
        render->DrawQuad2D(rest_ui_sky_frame_current, {16, 26});
        if (rest_ui_hourglass_frame_current) {
            rest_ui_hourglass_frame_current->release();
            rest_ui_hourglass_frame_current = nullptr;
        }

        hourglassLoopTimer += pEventTimer->dt();
        if (hourglassLoopTimer >= Duration::fromRealtimeSeconds(4)) {
            hourglassLoopTimer = 0_ticks;
        }

        int hourglass_icon_idx = (int)floorf(((double)hourglassLoopTimer.ticks() / 512.0 * 120.0) + 0.5f) % 256 + 1;
        if (hourglass_icon_idx >= 120) {
            hourglass_icon_idx = 1;
        }

        // MM6's rest-screen draw @0x41d920 puts the hourglass at (271, 164), centers the rest & heal
        // label in (27, 161, 171, 37), and right-aligns the food count to 392 at y=170 ("\r392%d").
        bool mm6 = engine->gameVersion() == GAME_VERSION_MM6;
        rest_ui_hourglass_frame_current = assets->getImage_ColorKey(fmt::format("hglas{:03}", hourglass_icon_idx));
        render->DrawQuad2D(rest_ui_hourglass_frame_current, mm6 ? Pointi(271, 164) : Pointi(267, 159));

        // MM6 draws every rest-screen string with color 0 (all ten text calls in the draw fn @0x41d920),
        // which the MM6 text drawer resolves to the font's own FONTPAL palette: white body over a black
        // shadow - not MM7's dark-on-parchment Diesel/StarkWhite.
        Color textColor = mm6 ? colorTable.White : colorTable.Diesel;
        Color shadowColor = mm6 ? colorTable.Black : colorTable.StarkWhite;

        tmp_button.rect = mm6 ? Recti(27, 161, 171, 37) : Recti(24, 154, 171, 37);
        tmp_button.pParent = pButton_RestUI_WaitUntilDawn->pParent;
        tmp_button.DrawLabel(localization->str(LSTR_REST_HEAL_8_HOURS), assets->pFontCreate.get(), textColor, shadowColor);
        tmp_button.pParent = 0;

        auto str1 = fmt::format("\r{}{}", mm6 ? 392 : 408, foodRequiredToRest);
        GUIWindow::DrawText(assets->pFontCreate.get(), {0, mm6 ? 170 : 164}, textColor, str1, pGUIWindow_CurrentMenu->frameRect, 0, shadowColor);

        pButton_RestUI_WaitUntilDawn->DrawLabel(localization->str(LSTR_WAIT_UNTIL_DAWN), assets->pFontCreate.get(), textColor, shadowColor);
        pButton_RestUI_Wait1Hour->DrawLabel(localization->str(LSTR_WAIT_1_HOUR), assets->pFontCreate.get(), textColor, shadowColor);
        pButton_RestUI_Wait5Minutes->DrawLabel(localization->str(LSTR_WAIT_5_MINUTES), assets->pFontCreate.get(), textColor, shadowColor);
        pButton_RestUI_Exit->DrawLabel(localization->str(LSTR_EXIT_REST), assets->pFontCreate.get(), textColor, shadowColor);
        // MM6 centers "Wait without healing" in (48, 210, 185, 22) (MM6.EXE 0x41dd45).
        tmp_button.rect = mm6 ? Recti(48, 210, 185, 22) : Recti(45, 199, 185, 30);

        tmp_button.pParent = pButton_RestUI_WaitUntilDawn->pParent;
        tmp_button.DrawLabel(localization->str(LSTR_WAIT_WITHOUT_HEALING), assets->pFontCreate.get(), textColor, shadowColor);
        tmp_button.pParent = 0;

        CivilTime time = pParty->GetPlayingTime().toCivilTime();

        std::string str2 = fmt::format("{}:{:02} {}", time.hourAmPm, time.minute, localization->amPm(time.isPm));
        DrawText(assets->pFontCreate.get(), {368, 168}, textColor, str2, pGUIWindow_CurrentMenu->frameRect, 0, shadowColor);
        std::string str3 = fmt::format("{}\r190{}", localization->str(LSTR_DAY_CAPITALIZED), time.day);
        DrawText(assets->pFontCreate.get(), {350, 190}, textColor, str3, pGUIWindow_CurrentMenu->frameRect, 0, shadowColor);
        std::string str4 = fmt::format("{}\r190{}", localization->str(LSTR_MONTH), time.month);
        DrawText(assets->pFontCreate.get(), {350, 222}, textColor, str4, pGUIWindow_CurrentMenu->frameRect, 0, shadowColor);
        std::string str5 = fmt::format("{}\r190{}", localization->str(LSTR_YEAR), time.year);
        DrawText(assets->pFontCreate.get(), {350, 254}, textColor, str5, pGUIWindow_CurrentMenu->frameRect, 0, shadowColor);
        if (currentRestType != REST_NONE) {
            Party::restOneFrame();
        }
    } else {
        new OnCancel(pButton_RestUI_Exit->rect.topLeft(), {0, 0}, pButton_RestUI_Exit, localization->str(LSTR_EXIT_REST));
    }
}
