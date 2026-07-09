#pragma once

#include <memory>

#include "GUI/GUIWindow.h"

class GraphicsImage;

bool PlayerCreation_Choose4Skills();
bool PartyCreationUI_Loop();

class GUIWindow_PartyCreation : public GUIWindow {
 public:
    GUIWindow_PartyCreation();
    virtual ~GUIWindow_PartyCreation();

    virtual void Update() override;

 protected:
    void initializeMm6();
    void updateMm6();

    GraphicsImage *main_menu_background = nullptr;
    std::unique_ptr<GUIFont> ui_partycreation_font;
};
