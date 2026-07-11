#include <array>
#include <memory>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"

#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Tables/AutonoteTable.h"
#include "Engine/mm7_data.h"

#include "GUI/GUIButton.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/Books/AutonotesBook.h"

#include "Media/Audio/AudioPlayer.h"

GraphicsImage *ui_book_autonotes_background = nullptr;

AutonoteType autonoteBookDisplayType;

void GUIWindow_AutonotesBook::recalculateCurrentNotesTypePages() {
    _startingNotesIdx = 0;
    _currentPage = 0;
    _currentPageNotes = 0;
    _activeNotesIdx.clear();
    for (int i : pParty->_autonoteBits.indices())
        if (pParty->_autonoteBits[i] && autonoteBookDisplayType == pAutonoteTxt[i].eType && !pAutonoteTxt[i].pText.empty())
            _activeNotesIdx.push_back(i);
}

GUIWindow_AutonotesBook::GUIWindow_AutonotesBook() : GUIWindow_Book() {
    this->eWindowType = WindowType::WINDOW_AutonotesBook;

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    pChildBooksOverlay = std::make_unique<GUIWindow_BooksButtonOverlay>(isMm6 ? pBtn_Autonotes->rect.topLeft() : Pointi{527, 353}, Sizei{0, 0}, pBtn_Autonotes);
    bFlashAutonotesBook = false;

    ui_book_quest_div_bar = assets->getImage_Alpha("divbar");
    if (isMm6) {
        // MM6's autonotes book (MM6.EXE window ctor case 0x40d417 + draw 0x40e390): note_bg over the shared
        // book base, the tab+/tab-- page tabs (top = next page), and FIVE category tabs at (415, 118+35*i).
        // MM6's autonote.txt type column covers exactly these five categories (no teacher notes), and the
        // per-tab art follows the EXE's slot order: anot1/2/3 then anot5 (seer), anot4 (misc).
        ui_book_autonotes_background = assets->getImage_Solid("note_bg");
        ui_book_button1_on = assets->getImage_Alpha("tab+on");
        ui_book_button2_on = assets->getImage_Alpha("tab--on");
        ui_book_button3_on = assets->getImage_Alpha("anot1on");
        ui_book_button4_on = assets->getImage_Alpha("anot2on");
        ui_book_button5_on = assets->getImage_Alpha("anot3on");
        ui_book_button6_on = assets->getImage_Alpha("anot5on");
        ui_book_button7_on = assets->getImage_Alpha("anot4on");
        ui_book_button1_off = assets->getImage_Alpha("tab+off");
        ui_book_button2_off = assets->getImage_Alpha("tab--off");
        ui_book_button3_off = assets->getImage_Alpha("anot1off");
        ui_book_button4_off = assets->getImage_Alpha("anot2off");
        ui_book_button5_off = assets->getImage_Alpha("anot3off");
        ui_book_button6_off = assets->getImage_Alpha("anot5off");
        ui_book_button7_off = assets->getImage_Alpha("anot4off");

        pBtn_Book_1 = CreateButton({415, 13}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), INPUT_ACTION_DIALOG_RIGHT, localization->str(LSTR_SCROLL_DOWN), {ui_book_button1_on});
        pBtn_Book_2 = CreateButton({415, 48}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), INPUT_ACTION_DIALOG_LEFT, localization->str(LSTR_SCROLL_UP), {ui_book_button2_on});
        pBtn_Book_3 = CreateButton({415, 118}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_POTION), INPUT_ACTION_INVALID, localization->str(LSTR_POTION_NOTES), {ui_book_button3_on});
        pBtn_Book_4 = CreateButton({415, 153}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_FOUNTAIN), INPUT_ACTION_INVALID, localization->str(LSTR_FOUNTAIN_NOTES), {ui_book_button4_on});
        pBtn_Book_5 = CreateButton({415, 188}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_OBELISK), INPUT_ACTION_INVALID, localization->str(LSTR_OBELISK_NOTES), {ui_book_button5_on});
        pBtn_Book_6 = CreateButton({415, 223}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_SEER), INPUT_ACTION_INVALID, localization->str(LSTR_SEER_NOTES), {ui_book_button6_on});
        pBtn_Autonotes_Misc = CreateButton({415, 258}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_MISC), INPUT_ACTION_INVALID, localization->str(LSTR_MISCELLANEOUS_NOTES), {ui_book_button7_on});
        pBtn_Autonotes_Instructors = nullptr; // MM6 has no teacher notes.

        // MM6 has no teacher-notes category - land on the potion tab if the persistent selection is one
        // that MM6 cannot display.
        if (autonoteBookDisplayType == AUTONOTE_TEACHER)
            autonoteBookDisplayType = AUTONOTE_POTION_RECIPE;
    } else {
        ui_book_autonotes_background = assets->getImage_ColorKey("sbautnot");

        ui_book_button1_on = assets->getImage_Alpha("tab-an-6b");
        ui_book_button2_on = assets->getImage_Alpha("tab-an-7b");
        ui_book_button3_on = assets->getImage_Alpha("tab-an-1b");
        ui_book_button4_on = assets->getImage_Alpha("tab-an-2b");
        ui_book_button5_on = assets->getImage_Alpha("tab-an-3b");
        ui_book_button6_on = assets->getImage_Alpha("tab-an-5b");
        ui_book_button7_on = assets->getImage_Alpha("tab-an-4b");
        ui_book_button8_on = assets->getImage_Alpha("tab-an-8b");
        ui_book_button1_off = assets->getImage_Alpha("tab-an-6a");
        ui_book_button2_off = assets->getImage_Alpha("tab-an-7a");
        ui_book_button3_off = assets->getImage_Alpha("tab-an-1a");
        ui_book_button4_off = assets->getImage_Alpha("tab-an-2a");
        ui_book_button5_off = assets->getImage_Alpha("tab-an-3a");
        ui_book_button6_off = assets->getImage_Alpha("tab-an-5a");
        ui_book_button7_off = assets->getImage_Alpha("tab-an-4a");
        ui_book_button8_off = assets->getImage_Alpha("tab-an-8a");

        pBtn_Book_1 = CreateButton(pViewport.topLeft() + Pointi(398, 1), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), INPUT_ACTION_DIALOG_LEFT, localization->str(LSTR_SCROLL_DOWN), {ui_book_button1_on});
        pBtn_Book_2 = CreateButton(pViewport.topLeft() + Pointi(398, 38), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), INPUT_ACTION_DIALOG_RIGHT, localization->str(LSTR_SCROLL_UP), {ui_book_button2_on});
        pBtn_Book_3 = CreateButton(pViewport.topLeft() + Pointi(398, 113), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_POTION), INPUT_ACTION_INVALID, localization->str(LSTR_POTION_NOTES), {ui_book_button3_on});
        pBtn_Book_4 = CreateButton(pViewport.topLeft() + Pointi(399, 150), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_FOUNTAIN), INPUT_ACTION_INVALID, localization->str(LSTR_FOUNTAIN_NOTES), {ui_book_button4_on});
        pBtn_Book_5 = CreateButton(pViewport.topLeft() + Pointi(397, 188), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_OBELISK), INPUT_ACTION_INVALID, localization->str(LSTR_OBELISK_NOTES), {ui_book_button5_on});
        pBtn_Book_6 = CreateButton(pViewport.topLeft() + Pointi(397, 226), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_SEER), INPUT_ACTION_INVALID, localization->str(LSTR_SEER_NOTES), {ui_book_button6_on});
        pBtn_Autonotes_Misc = CreateButton(pViewport.topLeft() + Pointi(397, 264), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_MISC), INPUT_ACTION_INVALID, localization->str(LSTR_MISCELLANEOUS_NOTES), {ui_book_button7_on});
        pBtn_Autonotes_Instructors = CreateButton(pViewport.topLeft() + Pointi(397, 302), {50, 34}, BUTTON_TYPE_NORMAL, 0,
            UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NOTES_INSTRUCTORS), INPUT_ACTION_INVALID, localization->str(LSTR_INSTRUCTORS), {ui_book_button8_on});
    }

    recalculateCurrentNotesTypePages();
}

void GUIWindow_AutonotesBook::Update() {
    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;

    int pTextHeight;
    bool noteTypeChanged = false;

    if (isMm6) {
        // MM6.EXE 0x40e390: shared book base, note_bg at (47,22), page tabs like the quest book.
        drawMm6BookBase();
        render->DrawQuad2D(ui_book_autonotes_background, {47, 22});

        if ((_bookButtonClicked && _bookButtonAction == BOOK_NEXT_PAGE) || (_startingNotesIdx + _currentPageNotes) >= _activeNotesIdx.size()) {
            render->DrawQuad2D(ui_book_button1_on, {415, 13});
        } else {
            render->DrawQuad2D(ui_book_button1_off, {418, 13});
        }
        if ((_bookButtonClicked && _bookButtonAction == BOOK_PREV_PAGE) || !_startingNotesIdx) {
            render->DrawQuad2D(ui_book_button2_on, {415, 48});
        } else {
            render->DrawQuad2D(ui_book_button2_off, {418, 48});
        }
    } else {
        render->DrawQuad2D(ui_exit_cancel_button_background, {471, 445});
        render->DrawQuad2D(ui_book_autonotes_background, pViewport.topLeft());
        if ((_bookButtonClicked && _bookButtonAction == BOOK_PREV_PAGE) || !_startingNotesIdx) {
            render->DrawQuad2D(ui_book_button1_off, pViewport.topLeft() + Pointi(407, 2));
        } else {
            render->DrawQuad2D(ui_book_button1_on, pViewport.topLeft() + Pointi(398, 1));
        }

        if ((_bookButtonClicked && _bookButtonAction == BOOK_NEXT_PAGE) || (_startingNotesIdx + _currentPageNotes) >= _activeNotesIdx.size()) {
            render->DrawQuad2D(ui_book_button2_off, pViewport.topLeft() + Pointi(407, 38));
        } else {
            render->DrawQuad2D(ui_book_button2_on, pViewport.topLeft() + Pointi(398, 38));
        }
    }

    // The category tabs: the selected (or just-clicked) category shows its "selected" art, the others their
    // raised art; clicking a new category retunes the filter. MM7 draws eight tabs down the page edge; MM6
    // has five (no teacher notes) at (415, 118+35*i) with the raised art nudged to x=418 (MM6.EXE 0x40e446).
    struct CategoryTab {
        BookButtonAction action;
        AutonoteType type;
        GraphicsImage *on;
        GraphicsImage *off;
        Pointi posOn;
        Pointi posOff;
    };
    std::array<CategoryTab, 6> tabs;
    size_t tabCount;
    if (isMm6) {
        tabCount = 5;
        static constexpr std::array<int, 5> ys = {{118, 153, 188, 223, 258}};
        static constexpr std::array<AutonoteType, 5> types = {{AUTONOTE_POTION_RECIPE, AUTONOTE_STAT_HINT, AUTONOTE_OBELISK, AUTONOTE_SEER, AUTONOTE_MISC}};
        static constexpr std::array<BookButtonAction, 5> actions = {{BOOK_NOTES_POTION, BOOK_NOTES_FOUNTAIN, BOOK_NOTES_OBELISK, BOOK_NOTES_SEER, BOOK_NOTES_MISC}};
        std::array<GraphicsImage *, 5> ons = {{ui_book_button3_on, ui_book_button4_on, ui_book_button5_on, ui_book_button6_on, ui_book_button7_on}};
        std::array<GraphicsImage *, 5> offs = {{ui_book_button3_off, ui_book_button4_off, ui_book_button5_off, ui_book_button6_off, ui_book_button7_off}};
        for (size_t i = 0; i < 5; i++)
            tabs[i] = {actions[i], types[i], ons[i], offs[i], Pointi(415, ys[i]), Pointi(418, ys[i])};
    } else {
        tabCount = 6;
        tabs[0] = {BOOK_NOTES_POTION, AUTONOTE_POTION_RECIPE, ui_book_button3_on, ui_book_button3_off, pViewport.topLeft() + Pointi(398, 113), pViewport.topLeft() + Pointi(408, 113)};
        tabs[1] = {BOOK_NOTES_FOUNTAIN, AUTONOTE_STAT_HINT, ui_book_button4_on, ui_book_button4_off, pViewport.topLeft() + Pointi(399, 150), pViewport.topLeft() + Pointi(408, 150)};
        tabs[2] = {BOOK_NOTES_OBELISK, AUTONOTE_OBELISK, ui_book_button5_on, ui_book_button5_off, pViewport.topLeft() + Pointi(397, 188), pViewport.topLeft() + Pointi(408, 188)};
        tabs[3] = {BOOK_NOTES_SEER, AUTONOTE_SEER, ui_book_button6_on, ui_book_button6_off, pViewport.topLeft() + Pointi(397, 226), pViewport.topLeft() + Pointi(408, 226)};
        tabs[4] = {BOOK_NOTES_MISC, AUTONOTE_MISC, ui_book_button7_on, ui_book_button7_off, pViewport.topLeft() + Pointi(397, 264), pViewport.topLeft() + Pointi(408, 263)};
        tabs[5] = {BOOK_NOTES_INSTRUCTORS, AUTONOTE_TEACHER, ui_book_button8_on, ui_book_button8_off, pViewport.topLeft() + Pointi(397, 302), pViewport.topLeft() + Pointi(408, 302)};
    }

    for (size_t i = 0; i < tabCount; i++) {
        const CategoryTab &tab = tabs[i];
        if (_bookButtonClicked && _bookButtonAction == tab.action && autonoteBookDisplayType != tab.type) {
            pAudioPlayer->playUISound(SOUND_StartMainChoice02);
            autonoteBookDisplayType = tab.type;
            noteTypeChanged = true;
        }
        if ((_bookButtonClicked && _bookButtonAction == tab.action) || autonoteBookDisplayType == tab.type) {
            render->DrawQuad2D(tab.on, tab.posOn);
        } else {
            render->DrawQuad2D(tab.off, tab.posOff);
        }
    }

    // for title
    DrawTitleText(assets->pFontBookTitle.get(), 0, 22, ui_book_autonotes_title_color, localization->str(LSTR_AUTO_NOTES), 3, pViewport);

    // for other text
    Recti autonotes_frameRect(48, 70, 360, 264);

    if (_bookButtonClicked == 10) {
        if (_bookButtonAction >= BOOK_NOTES_POTION && _bookButtonAction <= BOOK_NOTES_INSTRUCTORS) {
            if (noteTypeChanged) {
                recalculateCurrentNotesTypePages();
            }
        } else {
            if (_bookButtonAction == BOOK_NEXT_PAGE && (_startingNotesIdx + _currentPageNotes) < _activeNotesIdx.size()) {
                pAudioPlayer->playUISound(SOUND_openbook);
                _startingNotesIdx += _currentPageNotes;
                _notesPerPage[_currentPage] = _currentPageNotes;
                _currentPage++;
            }
            if (_bookButtonAction == BOOK_PREV_PAGE && _startingNotesIdx) {
                pAudioPlayer->playUISound(SOUND_openbook);
                _currentPage--;
                _startingNotesIdx -= _notesPerPage[_currentPage];
            }
        }
    }

    if (_bookButtonClicked)
        _bookButtonClicked--;

    _currentPageNotes = 0;

    for (int i = _startingNotesIdx; i < _activeNotesIdx.size(); ++i) {
        _currentPageNotes++;

        DrawText(assets->pFontBookOnlyShadow.get(), {1, 0}, ui_book_autonotes_text_color, pAutonoteTxt[_activeNotesIdx[i]].pText, autonotes_frameRect);
        pTextHeight = assets->pFontBookOnlyShadow->CalcTextHeight(pAutonoteTxt[_activeNotesIdx[i]].pText, autonotes_frameRect.w, 1);
        if ((autonotes_frameRect.y + pTextHeight) > autonotes_frameRect.h) {
            break;
        }

        render->DrawQuad2D(ui_book_quest_div_bar, {100, autonotes_frameRect.y + pTextHeight + 12});
        autonotes_frameRect.y = (autonotes_frameRect.y + pTextHeight) + 24;
    }
}
