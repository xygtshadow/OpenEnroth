#pragma once

#include <string>
#include <vector>

#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIHouseEnums.h"

class GUIWindow_Tavern : public GUIWindow_House {
 public:
    explicit GUIWindow_Tavern(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_Tavern() {}

    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
    virtual std::vector<DialogueId> listDialogueOptions() override;
    virtual void updateDialogueOnEscape() override;

 protected:
    void mainDialogue();
    void arcomageMainDialogue();
    void arcomageRulesDialogue();
    void arcomageVictoryCondDialogue();
    void arcomageResultDialogue();
    void restDialogue();
    void buyFoodDialogue();
    void mm6DrinksDialogue();
    void mm6TipDialogue();

    // The rumor currently shown in the dialogue panel after a Tip (MM6.EXE's [0x9DDEB8] text id) -
    // per-visit display state; the per-tavern rumor itself is cached in Party::_mm6TavernRumors.
    std::string _mm6RumorText;
};
