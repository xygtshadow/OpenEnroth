#include "UIStatusBar.h"

#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineGlobals.h"
#include "Engine/Localization.h"
#include "Engine/mm7_data.h"

#include "Engine/Graphics/Renderer/Renderer.h"

#include "Library/Color/ColorTable.h"

#include "GUI/GUIFont.h"

#include "GUI/UI/UIGame.h"

const std::string &StatusBar::get() {
    if (_eventStatusExpireTime) {
        return _eventStatusString;
    } else {
        return _statusString;
    }
}

// MM6.EXE draws status text at (AlignText_Center(450) + 11, 355) (draw fn @0x418e50); timed event messages are
// tinted pale yellow (255,255,155) while the mouseover/permanent string is drawn with color 0, which means the
// lucida font's own FONTPAL palette - white body (index 255) over a black shadow (index 1).
static int statusTextY() {
    return engine->gameVersion() == GAME_VERSION_MM6 ? 355 : 357;
}

void StatusBar::draw() {
    render->DrawQuad2D(game_ui_statusbar, {0, 352});

    const std::string &status = get();
    if (status.length() > 0) {
        Color color = uGameUIFontMain;
        Color shadow = uGameUIFontShadow;
        if (engine->gameVersion() == GAME_VERSION_MM6) {
            color = _eventStatusExpireTime ? colorTable.PaleCanary : colorTable.White;
            shadow = colorTable.Black;
        }
        GUIWindow::DrawText(assets->pFontLucida.get(), { assets->pFontLucida->AlignText_Center(450, status) + 11, statusTextY()}, color, status, pPrimaryWindow->frameRect, 0, shadow);
    }
}

void StatusBar::drawForced(std::string_view str, Color color) {
    render->DrawQuad2D(game_ui_statusbar, {0, 352});
    GUIWindow::DrawText(assets->pFontLucida.get(), { assets->pFontLucida->AlignText_Center(450, str) + 11, statusTextY()}, color, str, pPrimaryWindow->frameRect);
}

void StatusBar::update() {
    // Was also checking that event timer is not stopped
    if (_eventStatusExpireTime && platform->tickCount() >= _eventStatusExpireTime) {
        _eventStatusExpireTime = 0;
    }
}

void StatusBar::setPermanent(std::string_view str) {
    if (str.length() > 0) {
        if (_eventStatusExpireTime == 0) {
            _statusString = str;
        }
    }
}

void StatusBar::clearPermanent() {
    _statusString.clear();
}

void StatusBar::clearAll() {
    _statusString.clear();
    clearEvent();
}

void StatusBar::setEvent(std::string_view str) {
    _eventStatusString = str;
    _eventStatusExpireTime = platform->tickCount() + EVENT_DURATION;
}

void StatusBar::setEventShort(std::string_view str) {
    _eventStatusString = str;
    _eventStatusExpireTime = platform->tickCount() + EVENT_DURATION_SHORT;
}

void StatusBar::clearEvent() {
    _eventStatusExpireTime = 0;
}

void StatusBar::nothingHere() {
    if (_eventStatusExpireTime == 0) {
        setEvent(LSTR_NOTHING_HERE);
    }
}
