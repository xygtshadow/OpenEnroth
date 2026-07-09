#pragma once

#include <vector>

#include "GUI/UI/UIHouses.h"

#include "Engine/Data/AwardEnums.h"

/**
 * @return                              Whether @p houseId is one of MM6's magic-guild houses
 *                                      (2dEvents rows 119-140, an Initiate + an Adept house per
 *                                      guild organization). Only meaningful in MM6 sessions.
 */
bool mm6IsMagicGuildHouse(HouseId houseId);

/**
 * @return                              The MM6 membership award bit gating @p houseId (the word-pair
 *                                      table @0x4C3CB8; awards.txt rows 64-80 "Joined the ...").
 *                                      Joining happens at recruiter NPC topics 381-397, see
 *                                      NPCTopics.cpp.
 */
AwardId mm6MagicGuildMembershipAward(HouseId houseId);

class GUIWindow_MagicGuild : public GUIWindow_House {
 public:
    explicit GUIWindow_MagicGuild(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_MagicGuild() {}

    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
    virtual std::vector<DialogueId> listDialogueOptions() override;
    virtual void houseScreenClick() override;

 protected:
    /**
     * @offset 0x4BC8D5
     */
    void generateSpellBooksForGuild();

    /**
     * MM6's shelf restock (MM6.EXE generator @0x4a4320): 12 identified spellbooks, each
     * item 300 + school * 11 + rand % N, where N is the per-house spell range (@0x4C48B0) and the
     * combined Element/Self guilds roll the school per slot.
     */
    void generateSpellBooksForGuildMm6();

    void mainDialogue();
    void mm6MainDialogue();
    void mm6LearnSelectedSkill(Skill skill);
    void buyBooksDialogue();

    /**
     * @return                          The house id keying this guild's shelf & restock-clock slots
     *                                  in the party arrays. The arrays are keyed by MM7's magic-guild
     *                                  house ids 139-170, so MM6's guild houses 119-140 map onto
     *                                  slots houseId + 20 (the save format is untouched).
     */
    HouseId guildStorageId() const;
};
