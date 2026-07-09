#pragma once

#include <vector>

#include "GUI/UI/UIHouses.h"
#include "GUI/UI/UIHouseEnums.h"

#include "Engine/Objects/CharacterEnums.h"

class Character;

/**
 * MM6.EXE class-can-learn table @0x4C2694: 6 base classes x 31 MM6 skill slots. The fighter/thief
 * guilds and the magic guilds all filter their taught skills through it.
 */
bool mm6ClassCanLearn(Class classType, Skill skill);

/**
 * MM6 guild skill-learning price: trunc(base * 2dEvents price multiplier), merchant-discounted with
 * a floor of a third of the undiscounted price. Base is 100 for "Merc Guild" rows, 250 for
 * "Thieves Guild" rows (MM6.EXE 0x49c4cd) and 500 for the magic guilds (0x49b854).
 */
int mm6SkillLearnPrice(const Character *player, HouseId houseId, int base);

/**
 * MM6's membership skill-teaching guilds: the fighter guilds (2dEvents "Merc Guild", houses
 * 141-146) and the thief guilds ("Thieves Guild", houses 147-152). A member of the guild's
 * organization (a per-house award bit; joining happens through recruiter NPC topics, see
 * NPCTopics.cpp) is offered the house's five taught skills at novice level; everyone else -
 * and every unmapped MM6 house type that falls through to HOUSE_TYPE_MERCENARY_GUILD, MM6's
 * catch-all "plain house" - gets no options. MM7 data never produces this house type.
 *
 * The MM7.EXE decompile this file used to hold (offset 0x4B6478) was MM7's dead copy of the
 * same MM6 handler (MM6.EXE 0x49c420).
 */
class GUIWindow_MercenaryGuild : public GUIWindow_House {
 public:
    explicit GUIWindow_MercenaryGuild(HouseId houseId) : GUIWindow_House(houseId) {}
    virtual ~GUIWindow_MercenaryGuild() {}

    virtual void houseDialogueOptionSelected(DialogueId option) override;
    virtual void houseSpecificDialogue() override;
    virtual std::vector<DialogueId> listDialogueOptions() override;

 private:
    void mm6LearnSkillsDialogue();
    void mm6LearnSelectedSkill(Skill skill);
};
