#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Engine/Objects/MonsterEnums.h"

#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIHouseEnums.h"

/**
 * Runs one bounty-hunt interaction with the given town hall: regenerates the monthly bounty if
 * it's stale, pays out a completed hunt, and returns the raw (unformatted) reply text plus the
 * monster the text talks about. Shared between the town-hall house dialogue and, in MM6, the
 * proprietors' NPC topic 399 (MM6.EXE @0x4A30B0).
 */
std::pair<std::string, MonsterId> bountyHuntInteraction(HouseId townHall);

/**
 * @return   The reply from `bountyHuntInteraction` with the monster name / reward formatted in.
 */
std::string bountyHuntReplyText(const std::string &rawText, MonsterId monsterId);

class GUIWindow_TownHall : public GUIWindow_House {
 public:
    explicit GUIWindow_TownHall(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_TownHall() {}

    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
    virtual std::vector<DialogueId> listDialogueOptions() override;

    /**
     * @return   Text to show after the player has clicked on the "Bounty Hunt" dialogue option.
     */
    std::string bountyHuntingText();

    static MonsterId randomMonsterForHunting(HouseId townhall);

 protected:
    void mainDialogue();
    void bountyHuntDialogue();
    void payFineDialogue();

 private:
    /**
     * Handler for the "Bounty Hunt" dialogue option in a town hall.
     *
     * Regenerates bounty if needed, gives gold for a completed bounty hunt, and updates the current reply message to
     * be retrieved later with a call to `bountyHuntingText`.
     */
    void bountyHuntingDialogueOptionClicked();

    std::string _bountyHuntText = "";
    MonsterId _bountyHuntMonsterId = MONSTER_INVALID;
};
