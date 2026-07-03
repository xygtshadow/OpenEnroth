#include <memory>
#include <string>

#include "UIBranchlessDialogue.h"

#include "Engine/Engine.h"
#include "Engine/AssetsManager.h"
#include "Engine/Evt/Processor.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Objects/Decoration.h"
#include "Engine/Party.h"
#include "Engine/mm7_data.h"
#include "Engine/Graphics/Viewport.h"

#include "GUI/GUIFont.h"
#include "GUI/GUIMessageQueue.h"
#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIGame.h"

#include "Io/KeyboardInputHandler.h"

#include "Library/Color/ColorTable.h"

#include "Utility/String/Format.h"

GUIWindow_BranchlessDialogue::GUIWindow_BranchlessDialogue(EvtOpcode event) : GUIWindow(WINDOW_GreetingNPC, {0, 0}, render->GetRenderDimensions()), _event(event) {
    prev_screen_type = current_screen_type;
    keyboardInputHandler->StartTextInput(Io::TextInputType::Text, 15, this);
    current_screen_type = SCREEN_BRANCHLESS_NPC_DIALOG;

    CreateCharacterButtons();
}

GUIWindow_BranchlessDialogue::~GUIWindow_BranchlessDialogue() {
    current_screen_type = prev_screen_type;
    keyboardInputHandler->EndTextInput();
}

void GUIWindow_BranchlessDialogue::Update() {
    if (current_npc_text.length() > 0 && branchless_dialogue_str.empty())
        branchless_dialogue_str = current_npc_text;

    pGUIWindow_BranchlessDialogue->DrawDialoguePanel(branchless_dialogue_str);
    render->DrawQuad2D(game_ui_statusbar, {0, 352});

    // MM6/MM8 typed-input prompt (EVENT_InputString, e.g. MM6's riddle doors): draw the question and the answer
    // typed so far, and once the input is confirmed with Enter hand the answer back to the paused event script.
    if (event() == EVENT_InputString) {
        if (keyboard_input_status == WINDOW_INPUT_IN_PROGRESS) {
            std::string str = fmt::format("{} {}", savedEventPrompt, keyboardInputHandler->GetTextInput());
            DrawText(assets->pFontLucida.get(), {13, 357}, colorTable.White, str, frameRect);
            DrawFlashingInputCursor(assets->pFontLucida->GetLineWidth(str) + 13, 357, assets->pFontLucida.get(), frameRect);
            return;
        }
        if (keyboard_input_status == WINDOW_INPUT_CONFIRMED) {
            savedEventInput = keyboardInputHandler->GetTextInput(); // The paused event resumes on escape below.
        } else {
            savedEventID = 0; // Cancelled with Escape - close the prompt without taking either event branch.
        }
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 0, 0);
        return;
    }

    // Close branchless dialog on any keypress
    if (!keyboardInputHandler->GetTextInput().empty()) {
        keyboardInputHandler->EndTextInput();
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 0, 0);
        return;
    }

    // Also close branchless dialog on enter
    if (pGUIWindow_BranchlessDialogue->keyboard_input_status != WINDOW_INPUT_IN_PROGRESS) {
        engine->_messageQueue->addMessageCurrentFrame(UIMSG_Escape, 0, 0);
        return;
    }
}

void startBranchlessDialogue(int eventid, int entryline, EvtOpcode type) {
    if (!pGUIWindow_BranchlessDialogue) {
        pMiscTimer->setPaused(true);
        pEventTimer->setPaused(true);
        savedEventID = eventid;
        savedEventStep = entryline;
        savedDecoration = activeLevelDecoration;
        pGUIWindow_BranchlessDialogue = std::make_unique<GUIWindow_BranchlessDialogue>(type);
    }
}

void releaseBranchlessDialogue() {
    pGUIWindow_BranchlessDialogue = nullptr;
    if (savedEventID) {
        // Do not run event engine whith no event, it may happen when you close talk window
        // with NPC that only say catch phrases
        activeLevelDecoration = savedDecoration;
        eventProcessor(savedEventID, Pid(), 1, savedEventStep);
    }
    activeLevelDecoration = nullptr;
    pEventTimer->setPaused(false);
}

