#pragma once

#include <string>
#include <vector>

#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIHouseEnums.h"

class GUIWindow_Transport : public GUIWindow_House {
 public:
    explicit GUIWindow_Transport(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_Transport() {}

    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
    virtual std::vector<DialogueId> listDialogueOptions() override;

 protected:
    void mainDialogue();
    void transportDialogue();
    void mm6PriceFixingDialogue();

 private:
    // MM6 stables' agreement line (npctext row 136), drawn in the dialogue panel once given.
    std::string _mm6PriceFixingText;
    /**
     * @brief                               New function.
     *
     * @param schedule_id                   Index to transport_schedule.
     *
     * @return                              Number of days travel by transport will take with hireling modifiers.
     */
    int getTravelTimeTransportDays(int schedule_id);
};

bool isTravelAvailable(HouseId houseId);
