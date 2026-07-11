#include "GUI/UI/Books/CalendarBook.h"

#include <memory>
#include <string>

#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/AssetsManager.h"
#include "Engine/MapInfo.h"
#include "Engine/Engine.h"
#include "Engine/mm7_data.h"

#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Viewport.h"

#include "GUI/GUIButton.h"

GraphicsImage *ui_book_calendar_background = nullptr;

GraphicsImage *ui_book_calendar_moon_new = nullptr;
GraphicsImage *ui_book_calendar_moon_4 = nullptr;
GraphicsImage *ui_book_calendar_moon_2 = nullptr;
GraphicsImage *ui_book_calendar_moon_2_2 = nullptr;
GraphicsImage *ui_book_calendar_moon_full = nullptr;

// 4E1B18
static std::array<int, 28> pDayMoonPhase = {
        0, 0, 0,
        1, 1, 1, 1,
        2, 2, 2,
        3, 3, 3, 3,
        4, 4, 4,
        3, 3, 3, 3,
        2, 2, 2,
        1, 1, 1, 1
};


GUIWindow_CalendarBook::GUIWindow_CalendarBook() : GUIWindow_Book() {
    this->eWindowType = WindowType::WINDOW_CalendarBook;

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    pChildBooksOverlay = std::make_unique<GUIWindow_BooksButtonOverlay>(isMm6 ? pBtn_Calendar->rect.topLeft() : Pointi{570, 354}, Sizei{0, 0}, pBtn_Calendar);

    if (isMm6) {
        // MM6's calendar (MM6.EXE window ctor case 0x40da9b + draw 0x40ed20): time_bg over the shared book
        // base, and the current moon-phase image drawn at (266,198). The moon images share MM7's names and
        // are blitted solid (0x40b000).
        ui_book_calendar_background = assets->getImage_Solid("time_bg");
        ui_book_calendar_moon_new = assets->getImage_Solid("moon_new");
        ui_book_calendar_moon_4 = assets->getImage_Solid("moon_4");
        ui_book_calendar_moon_2 = assets->getImage_Solid("moon_2");
        ui_book_calendar_moon_2_2 = assets->getImage_Solid("moon_2");
        ui_book_calendar_moon_full = assets->getImage_Solid("moon_ful");
    } else {
        ui_book_calendar_background = assets->getImage_ColorKey("sbdate-time");
        ui_book_calendar_moon_new = assets->getImage_ColorKey("moon_new");
        ui_book_calendar_moon_4 = assets->getImage_ColorKey("moon_4");
        ui_book_calendar_moon_2 = assets->getImage_ColorKey("moon_2");
        ui_book_calendar_moon_2_2 = assets->getImage_ColorKey("moon_2");
        ui_book_calendar_moon_full = assets->getImage_ColorKey("moon_ful");
    }
}

/**
 * @offset 0x413D3C
 */
static std::string getDayPart(int hour) {
    assert(hour >= 0 && hour < 24);

    if (hour > 5 && hour < 20) {
        return localization->str(LSTR_DAY_CAPITALIZED);
    } else if (hour == 5) {
        return localization->str(LSTR_DAWN);
    } else if (hour == 20) {
        return localization->str(LSTR_DUSK);
    } else {
        return localization->str(LSTR_NIGHT);
    }
}

void GUIWindow_CalendarBook::Update() {
    CivilTime time = pParty->GetPlayingTime().toCivilTime();
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x40ed20: shared book base, time_bg at (47,22), and the moon-phase image at (266,198).
        // The text lines below are identical between the games - MM7 inherited MM6's calendar layout but
        // dropped the moon image.
        drawMm6BookBase();
        render->DrawQuad2D(ui_book_calendar_background, {47, 22});
        GraphicsImage *moons[5] = {ui_book_calendar_moon_new, ui_book_calendar_moon_4, ui_book_calendar_moon_2,
                                   ui_book_calendar_moon_2_2, ui_book_calendar_moon_full};
        render->DrawQuad2D(moons[pDayMoonPhase[time.day - 1]], {266, 198});
    } else {
        render->DrawQuad2D(ui_exit_cancel_button_background, {471, 445});
        render->DrawQuad2D(ui_book_calendar_background, pViewport.topLeft());
    }

    DrawTitleText(assets->pFontBookTitle.get(), 0, 22, ui_book_calendar_title_color, localization->str(LSTR_TIME_IN_ERATHIA), 3, pViewport);

    std::string str = fmt::format("{}\t100:\t110{}:{:02} {} - {}", localization->str(LSTR_TIME), time.hourAmPm,
                                  time.minute, localization->amPm(time.isPm), getDayPart(time.hour));
    DrawText(assets->pFontBookCalendar.get(), {70, 55}, ui_book_calendar_time_color, str, pViewport);

    str = fmt::format("{}\t100:\t110{} - {}", localization->str(LSTR_DAY_CAPITALIZED), time.day,
                      localization->dayName(time.dayOfWeek - 1));
    DrawText(assets->pFontBookCalendar.get(), {70, 2 * assets->pFontBookCalendar->GetHeight() + 49}, ui_book_calendar_day_color, str, pViewport);
    str = fmt::format("{}\t100:\t110{} - {}", localization->str(LSTR_MONTH), time.month,
                      localization->monthName(time.month - 1));
    DrawText(assets->pFontBookCalendar.get(), {70, 4 * assets->pFontBookCalendar->GetHeight() + 43}, ui_book_calendar_month_color, str, pViewport);

    str = fmt::format("{}\t100:\t110{}", localization->str(LSTR_YEAR), time.year);
    DrawText(assets->pFontBookCalendar.get(), {70, 6 * assets->pFontBookCalendar->GetHeight() + 37}, ui_book_calendar_year_color, str, pViewport);

    str = fmt::format("{}\t100:\t110{}", localization->str(LSTR_MOON), localization->moonPhaseName(pDayMoonPhase[time.day - 1]));
    DrawText(assets->pFontBookCalendar.get(), {70, 8 * assets->pFontBookCalendar->GetHeight() + 31}, ui_book_calendar_moon_color, str, pViewport);

    std::string pMapName = "Unknown";
    if (engine->_currentLoadedMapId != MAP_INVALID) {
        pMapName = pMapStats->pInfos[engine->_currentLoadedMapId].name;
    }

    str = fmt::format("{}\t100:\t110{}", localization->str(LSTR_LOCATION), pMapName);
    DrawText(assets->pFontBookCalendar.get(), {70, 10 * assets->pFontBookCalendar->GetHeight() + 25}, ui_book_calendar_location_color, str, pViewport);
}
