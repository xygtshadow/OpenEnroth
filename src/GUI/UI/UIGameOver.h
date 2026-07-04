#pragma once

#include "GUI/GUIWindow.h"

class GUIWindow_GameOver : public GUIWindow {
 public:
    explicit GUIWindow_GameOver(UIMessageType releaseEvent = UIMSG_OnGameOverWindowClose, bool isLoss = false);
    virtual ~GUIWindow_GameOver();

    virtual void Update() override;

    bool toggleAndTestFinished();

 protected:
    UIMessageType _releaseEvent = UIMSG_0;
    bool _isLoss = false; // MM6 has a losing ending (the Hive reactor blast); MM7 endings are always won.
    bool _showPopUp = false;
    GraphicsImage *_winnerCert = nullptr;
};
