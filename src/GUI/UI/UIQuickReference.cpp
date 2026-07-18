#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/Time/Timer.h"
#include "Engine/Engine.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"
#include "GUI/UI/UIQuickReference.h"
#include "GUI/UI/UIGame.h"

#include "Media/Audio/AudioPlayer.h"

GraphicsImage *ui_game_quickref_background = nullptr;
static GraphicsImage *mm6_quickref_exit_button = nullptr;  // buttexi1, blitted every frame (MM6.EXE 0x417387).

GUIWindow_QuickReference::GUIWindow_QuickReference() : GUIWindow(WINDOW_QuickReference, {0, 0}, render->GetRenderDimensions()) {
    // 004304E7 Game_EventLoop --- part
    pEventTimer->setPaused(true);
    current_screen_type = SCREEN_QUICK_REFERENCE;

    // paperdoll_dbrds[2] = assets->GetImage_16BitAlpha(L"BUTTEXI1");

    if (!ui_game_quickref_background)
        ui_game_quickref_background = assets->getImage_ColorKey("quikref");

    if (engine->gameVersion() == GAME_VERSION_MM6 && !mm6_quickref_exit_button)
        mm6_quickref_exit_button = assets->getImage_Solid("buttexi1");

    pBtn_ExitCancel = CreateButton({0x187u, 0x13Cu}, {0x4Bu, 0x21u}, BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0,
                                   INPUT_ACTION_INVALID, localization->str(LSTR_EXIT_DIALOGUE), {ui_buttdesc2});
}

void GUIWindow_QuickReference::Update() {
    // -----------------------------------
    // 004156F0 GUI_UpdateWindows --- part
    // {
    //     GameUI_QuickRef_Draw();
    // }

    //----- (0041A57E) --------------------------------------------------------
    // void GameUI_QuickRef_Draw()

    Color pTextColor;
    int pFontHeight = assets->pFontArrus->GetHeight() + 1;

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    if (isMm6) {
        // MM6's quikref bitmap is only the box frame (its interior is the transparency key), and it
        // goes at (0,0) over the standard char-screen base: LEATHER filling the viewport with the
        // corner patches on top (MM6.EXE 0x416884..0x4168d3). MM7 bakes the whole screen into one
        // opaque bitmap drawn at (8,8).
        render->DrawQuad2D(ui_leather_mm7, {8, 8});
        render->DrawQuad2D(game_ui_mm6_border5, {7, 8});
        render->DrawQuad2D(game_ui_mm6_border6, {461, 8});
        render->DrawQuad2D(ui_game_quickref_background, {0, 0});
    } else {
        render->DrawQuad2D(ui_game_quickref_background, {8, 8});
    }

    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, 18}, colorTable.White, localization->str(LSTR_NAME), 60, 0);
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, 47}, colorTable.White, localization->str(LSTR_LEVEL), 60, 0);
    int pY = pFontHeight + 47;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_CLASS), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_HP), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_SP), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_AC), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_ATTACK), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_DMG), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_SHOOT), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_DMG), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_SKILLS), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_POINTS), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_COND), 60, 0);
    pY += pFontHeight;
    pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {22, pY}, colorTable.White, localization->str(LSTR_QSPELL), 60, 0);

    int pX = isMm6 ? 91 : 89;  // MM6 character columns are at 91+94*i (MM6.EXE 0x416919).
    for (Character &player : pParty->pCharacters) {
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, 18}, ui_character_header_text_color, player.name, 84, 0);

        pTextColor = (player.GetActualLevel() <= player.GetBaseLevel()) ? player.GetExperienceDisplayColor() : ui_character_bonus_text_color;
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, 47}, pTextColor, fmt::format("{}", player.GetActualLevel()), 84, 0);

        pY = pFontHeight + 47;
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, localization->className(player.classType), 84, 0);
        pY += pFontHeight;

        pTextColor = UI_GetHealthManaAndOtherQualitiesStringColor(player.health, player.GetMaxHealth());
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, pTextColor, fmt::format("{}", player.health), 84, 0);
        pY += pFontHeight;

        pTextColor = UI_GetHealthManaAndOtherQualitiesStringColor(player.mana, player.GetMaxMana());
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, pTextColor, fmt::format("{}", player.mana), 84, 0);
        pY += pFontHeight;

        pTextColor = UI_GetHealthManaAndOtherQualitiesStringColor(player.GetActualAC(), player.GetBaseAC());
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, pTextColor, fmt::format("{}", player.GetActualAC()), 84, 0);
        pY += pFontHeight;

        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, fmt::format("{:+}", player.GetActualAttack(false)), 84, 0);
        pY += pFontHeight;

        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, player.GetMeleeDamageString(), 84, 0);
        pY += pFontHeight;

        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, fmt::format("{:+}", player.GetRangedAttack()), 84, 0);
        pY += pFontHeight;

        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, player.GetRangedDamageString(), 84, 0);
        pY += pFontHeight;

        int pSkillsCount = 0;
        for (Skill j : allVisibleSkills()) {
            if (player.pActiveSkills[j]) {
                ++pSkillsCount;
            }
        }
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, fmt::format("{}", pSkillsCount), 84, 0);
        pY += pFontHeight;

        pTextColor = player.uSkillPoints ? ui_character_bonus_text_color : ui_character_default_text_color;
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, pTextColor, fmt::format("{}", player.uSkillPoints), 84, 0);
        pY += pFontHeight;

        pTextColor = GetConditionDrawColor(player.GetMajorConditionIdx());
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, pTextColor, localization->characterConditionName(player.GetMajorConditionIdx()), 84, 0);
        pY += pFontHeight;

        std::string pText = (player.uQuickSpell != SPELL_NONE) ? pSpellStats->pInfos[player.uQuickSpell].pShortName : localization->str(LSTR_NONE);
        pGUIWindow_CurrentMenu->DrawTextInRect(assets->pFontArrus.get(), {pX, pY}, colorTable.White, pText, 84, 0);

        pX += 94;
    }

    if (isMm6) {
        // MM6 is positive = good (MM6.EXE 0x4172DC): bad color below zero, plain up to
        // Respectable, good color from +200 up.
        int reputation = pParty->GetPartyReputation();
        if (reputation < 0) {
            pTextColor = ui_character_bonus_text_color_neg;
        } else {
            pTextColor = (reputation < 200) ? ui_character_default_text_color : ui_character_bonus_text_color;
        }
    } else if (pParty->GetPartyReputation() >= 0) {
        pTextColor = (pParty->GetPartyReputation() <= 5) ? ui_character_default_text_color : ui_character_bonus_text_color_neg;
    } else {
        pTextColor = ui_character_bonus_text_color;
    }

    std::string rep = fmt::format("{}: {::}{}\f00000", localization->str(LSTR_REPUTATION), pTextColor.tag(), GetReputationString(pParty->GetPartyReputation()));
    GUIWindow::DrawText(assets->pFontArrus.get(), {22, 323}, colorTable.White, rep, pGUIWindow_CurrentMenu->frameRect);
    std::string fame = fmt::format("\r261{}: {}", localization->str(LSTR_FAME), pParty->getPartyFame());
    GUIWindow::DrawText(assets->pFontArrus.get(), {0, 323}, colorTable.White, fame, pGUIWindow_CurrentMenu->frameRect);

    // MM6 blits the exit button art every frame after the text (MM6.EXE 0x417387); MM7 has it
    // baked into the quikref bitmap instead.
    if (isMm6)
        render->DrawQuad2D(mm6_quickref_exit_button, {391, 316});
}
