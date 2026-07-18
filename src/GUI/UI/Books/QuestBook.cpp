#include <memory>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"

#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Image.h"
#include "Engine/mm7_data.h"

#include "Engine/Tables/QuestTable.h"

#include "GUI/GUIButton.h"
#include "GUI/UI/UIGame.h"
#include "GUI/UI/Books/QuestBook.h"

#include "Media/Audio/AudioPlayer.h"

GraphicsImage *ui_book_quests_background = nullptr;

GUIWindow_QuestBook::GUIWindow_QuestBook() {
    this->eWindowType = WindowType::WINDOW_QuestBook;

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    // MM6 draws the pressed sprite at (495,270), 7px below the button rect (MM6.EXE open handler 0x42e996).
    pChildBooksOverlay = std::make_unique<GUIWindow_BooksButtonOverlay>(isMm6 ? Pointi{495, 270} : Pointi{493, 355}, Sizei{0, 0}, pBtn_Quests);
    bFlashQuestBook = false;

    ui_book_quest_div_bar = assets->getImage_Alpha("divbar");
    if (isMm6) {
        // MM6's quest book (MM6.EXE window ctor case 0x40d293 + draw 0x40e0e0): its own quest_bg parchment
        // over the shared book base, and the tab+/tab-- page tabs on the right edge at (415,13)/(415,48).
        // The TOP tab is next-page in MM6 (message 0x47 param 0 sets the page-advance latch).
        ui_book_quests_background = assets->getImage_Solid("quest_bg");
        ui_book_button1_on = assets->getImage_Alpha("tab+on");
        ui_book_button2_on = assets->getImage_Alpha("tab--on");
        ui_book_button1_off = assets->getImage_Alpha("tab+off");
        ui_book_button2_off = assets->getImage_Alpha("tab--off");
        pBtn_Book_1 = CreateButton({415, 13}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
                                   UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), INPUT_ACTION_DIALOG_RIGHT, localization->str(LSTR_SCROLL_DOWN), {ui_book_button1_on});
        pBtn_Book_2 = CreateButton({415, 48}, {50, 34}, BUTTON_TYPE_NORMAL, 0,
                                   UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), INPUT_ACTION_DIALOG_LEFT, localization->str(LSTR_SCROLL_UP), {ui_book_button2_on});
    } else {
        ui_book_quests_background = assets->getImage_Solid("sbquiknot");
        ui_book_button1_on = assets->getImage_Alpha("tab-an-6b");
        ui_book_button2_on = assets->getImage_Alpha("tab-an-7b");
        ui_book_button1_off = assets->getImage_Alpha("tab-an-6a");
        ui_book_button2_off = assets->getImage_Alpha("tab-an-7a");
        pBtn_Book_1 = CreateButton(pViewport.topLeft() + Pointi(398, 1), ui_book_button1_on->size(), BUTTON_TYPE_NORMAL, 0,
                                   UIMSG_ClickBooksBtn, std::to_underlying(BOOK_PREV_PAGE), INPUT_ACTION_DIALOG_LEFT, localization->str(LSTR_SCROLL_UP), {ui_book_button1_on});
        pBtn_Book_2 = CreateButton(pViewport.topLeft() + Pointi(398, 38), ui_book_button2_on->size(), BUTTON_TYPE_NORMAL, 0,
                                   UIMSG_ClickBooksBtn, std::to_underlying(BOOK_NEXT_PAGE), INPUT_ACTION_DIALOG_RIGHT, localization->str(LSTR_SCROLL_DOWN), {ui_book_button2_on});
    }

    for (auto i : pQuestTable.indices()) {
        if (pParty->_questBits[i] && !pQuestTable[i].empty()) {
            _activeQuestsIdx.push_back(i);
        }
    }
}

void GUIWindow_QuestBook::Update() {
    int pTextHeight;
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x40e0e0: shared book base, quest_bg at (47,22), then the two page tabs - the raised
        // ("off") art at (418,y) while the flip is available, the flat ("on") art at (415,y) when it is
        // not (or while the click animates). Top tab flips forward.
        drawMm6BookBase();
        render->DrawQuad2D(ui_book_quests_background, {47, 22});

        if ((_bookButtonClicked && _bookButtonAction == BOOK_NEXT_PAGE) || (_startingQuestIdx + _currentPageQuests) >= _activeQuestsIdx.size()) {
            render->DrawQuad2D(ui_book_button1_on, {415, 13});
        } else {
            render->DrawQuad2D(ui_book_button1_off, {418, 13});
        }
        if ((_bookButtonClicked && _bookButtonAction == BOOK_PREV_PAGE) || !_startingQuestIdx) {
            render->DrawQuad2D(ui_book_button2_on, {415, 48});
        } else {
            render->DrawQuad2D(ui_book_button2_off, {418, 48});
        }
    } else {
        render->DrawQuad2D(ui_exit_cancel_button_background, {471, 445});
        render->DrawQuad2D(ui_book_quests_background, pViewport.topLeft());

        if ((_bookButtonClicked && _bookButtonAction == BOOK_PREV_PAGE) || !_startingQuestIdx) {
            render->DrawQuad2D(ui_book_button1_off, pViewport.topLeft() + Pointi(407, 2));
        } else {
            render->DrawQuad2D(ui_book_button1_on, pViewport.topLeft() + Pointi(398, 1));
        }

        if ((_bookButtonClicked && _bookButtonAction == BOOK_NEXT_PAGE) || (_startingQuestIdx + _currentPageQuests) >= _activeQuestsIdx.size()) {
            render->DrawQuad2D(ui_book_button2_off, pViewport.topLeft() + Pointi(407, 38));
        } else {
            render->DrawQuad2D(ui_book_button2_on, pViewport.topLeft() + Pointi(398, 38));
        }
    }

    // for title
    DrawTitleText(assets->pFontBookTitle.get(), 0, 22, ui_book_quests_title_color, localization->str(LSTR_CURRENT_QUESTS), 3, pViewport);

    // for other text
    Recti questbook_window(48, 70, 360, 264);

    if (_bookButtonClicked == 10 && _bookButtonAction == BOOK_NEXT_PAGE && (_startingQuestIdx + _currentPageQuests) < _activeQuestsIdx.size()) {
        pAudioPlayer->playUISound(SOUND_openbook);
        _startingQuestIdx += _currentPageQuests;
        _questsPerPage[_currentPage] = _currentPageQuests;
        _currentPage++;
    }

    if (_bookButtonClicked == 10 && _bookButtonAction == BOOK_PREV_PAGE && _startingQuestIdx) {
        pAudioPlayer->playUISound(SOUND_openbook);
        _currentPage--;
        _startingQuestIdx -= _questsPerPage[_currentPage];
    }

    if (_bookButtonClicked)
        _bookButtonClicked--;

    _currentPageQuests = 0;

    for (int i = _startingQuestIdx; i < _activeQuestsIdx.size(); ++i) {
        _currentPageQuests++;

        DrawText(assets->pFontBookOnlyShadow.get(), {1, 0}, ui_book_quests_text_color, pQuestTable[_activeQuestsIdx[i]], questbook_window);
        pTextHeight = assets->pFontBookOnlyShadow->CalcTextHeight(pQuestTable[_activeQuestsIdx[i]], questbook_window.w, 1);
        if ((questbook_window.y + pTextHeight) > questbook_window.h) {
            break;
        }

        render->DrawQuad2D(ui_book_quest_div_bar, {100, (questbook_window.y + pTextHeight) + 12});
        questbook_window.y = (questbook_window.y + pTextHeight) + 24;
    }
}
