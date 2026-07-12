#pragma once

#include <memory>

#include "GUI/GUIWindow.h"

class GraphicsImage;

bool PlayerCreation_Choose4Skills();
bool PartyCreationUI_Loop();

/**
 * Builds the party for MM6's Quick Start, the prologue screen's second button: the same default
 * party and starting inventory that the creation screen produces when it's opened and immediately
 * confirmed - but without ever showing it. So, unlike everything else declared here, this shows no
 * UI at all; it lives in this header because it shares the creation screen's setup and item grant.
 * See the definition for what MM6.EXE does and why the two constructions agree.
 *
 * @offset 0x42fe0e
 */
void mm6QuickStartParty();

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
