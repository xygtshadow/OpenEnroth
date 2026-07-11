#pragma once

#include <memory>
#include <string>

#include "GUI/GUIWindow.h"

class GraphicsImage;

enum class BookButtonAction {
    BOOK_ZOOM_IN = 0,
    BOOK_ZOOM_OUT = 1,
    BOOK_SCROLL_UP = 2,
    BOOK_SCROLL_DOWN = 3,
    BOOK_SCROLL_RIGHT = 4,
    BOOK_SCROLL_LEFT = 5,
    BOOK_NOTES_POTION = 6,
    BOOK_NOTES_FOUNTAIN = 7,
    BOOK_NOTES_OBELISK = 8,
    BOOK_NOTES_SEER = 9,
    BOOK_NOTES_MISC = 10,
    BOOK_NOTES_INSTRUCTORS = 11,
    BOOK_NEXT_PAGE = 12,
    BOOK_PREV_PAGE = 13,

    BOOK_BUTTON_FIRST = BOOK_ZOOM_IN,
    BOOK_BUTTON_LAST = BOOK_PREV_PAGE
};
using enum BookButtonAction;

class GUIWindow_Book : public GUIWindow {
 public:
    GUIWindow_Book();
    virtual ~GUIWindow_Book();

    void bookButtonClicked(BookButtonAction action);

 protected:
    /**
     * Draws MM6's shared book-screen base: the full-page `book` parchment at (8,8) and the `tabexit`
     * close tab at (360,332) - what MM6.EXE's book-screen dispatcher (0x40ebd0) puts under every book
     * before the per-book content. MM6 sessions only; MM7 books draw their own `sb*` backgrounds and
     * the right-panel exit hint instead.
     */
    void drawMm6BookBase();

    std::unique_ptr<GUIWindow> pChildBooksOverlay;

    GraphicsImage *ui_book_mm6_base{ nullptr };
    GraphicsImage *ui_book_mm6_exit_tab{ nullptr };

    GraphicsImage *ui_book_button8_off{ nullptr };
    GraphicsImage *ui_book_button8_on{ nullptr };
    GraphicsImage *ui_book_button7_off{ nullptr };
    GraphicsImage *ui_book_button7_on{ nullptr };
    GraphicsImage *ui_book_button6_off{ nullptr };
    GraphicsImage *ui_book_button6_on{ nullptr };
    GraphicsImage *ui_book_button5_off{ nullptr };
    GraphicsImage *ui_book_button5_on{ nullptr };
    GraphicsImage *ui_book_button4_off{ nullptr };
    GraphicsImage *ui_book_button4_on{ nullptr };
    GraphicsImage *ui_book_button3_off{ nullptr };
    GraphicsImage *ui_book_button3_on{ nullptr };
    GraphicsImage *ui_book_button2_off{ nullptr };
    GraphicsImage *ui_book_button2_on{ nullptr };
    GraphicsImage *ui_book_button1_off{ nullptr };
    GraphicsImage *ui_book_button1_on{ nullptr };

    GraphicsImage *ui_book_map_frame{ nullptr };
    GraphicsImage *ui_book_quest_div_bar{ nullptr };

    int _bookButtonClicked{ 0 };
    BookButtonAction _bookButtonAction;

 private:
    /**
     * @offset 0x411AAA
     */
    void initializeFonts();
};


class GUIWindow_BooksButtonOverlay : public GUIWindow {
 public:
    GUIWindow_BooksButtonOverlay(Pointi position, Sizei dimensions, GUIButton *button, std::string_view hint = {});

    virtual void Update() override;

 private:
    GUIButton *_button = nullptr;
};
