#pragma once

#include <cstdint>
#include <functional>

#include "GUI/GUIWindow.h"

/**
 * @param tickCountMs               Milliseconds since startup.
 * @return                          Frame of the MM6 main menu glow ramp to draw on the button
 *                                  under the cursor, in `[0, 6]`.
 */
int mm6MainMenuGlowFrame(int64_t tickCountMs);

class GUIWindow_MainMenu : public GUIWindow {
 public:
    GUIWindow_MainMenu();
    virtual ~GUIWindow_MainMenu();

    virtual void Update() override;

    void processMessage(UIMessageType messageType);

 protected:
    GUIButton *pBtnExit;
    GUIButton *pBtnCredits;
    GUIButton *pBtnLoad;
    GUIButton *pBtnNew;

    GraphicsImage *main_menu_background;

    GraphicsImage *ui_mainmenu_new;
    GraphicsImage *ui_mainmenu_load;
    GraphicsImage *ui_mainmenu_credits;
    GraphicsImage *ui_mainmenu_exit;
};
