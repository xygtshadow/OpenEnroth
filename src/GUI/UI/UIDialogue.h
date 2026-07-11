#pragma once

#include <string>

#include "GUI/GUIWindow.h"

#include "Utility/IndexedArray.h"

class Actor;
class Character;

enum class DialogWindowType {
    /** This one doesn't seem to be used on MM7, only creates the profession details & hire/fire topics. */
    DIALOG_WINDOW_HIRE_FIRE_SHORT = 1,

    /** Creates all appropriate dialog options, including scripted ones. */
    DIALOG_WINDOW_FULL = 3,
};
using enum DialogWindowType;

// The kind of right-panel menu an MM6 street NPC offers, computed at window open from the NPC's
// fame/reputation requirements and greet state (MM6.EXE 0x43BF39 - the window "page").
enum class Mm6StreetDialoguePage {
    // Gates passed (also: hired, no requirements, or threatened before - a threatened NPC talks
    // forever): the profession small talk / join / news menu.
    MM6_STREET_PAGE_TALK,
    // Party fame (total experience / 1000) doesn't exceed the NPC's requirement: no options at all.
    MM6_STREET_PAGE_FAME_REFUSAL,
    // The reputation gate failed, or the NPC was begged/bribed before: Beg / Threaten / Bribe.
    MM6_STREET_PAGE_BTB,
};
using enum Mm6StreetDialoguePage;

class GUIWindow_Dialogue : public GUIWindow {
 public:
    explicit GUIWindow_Dialogue(DialogWindowType type);
    virtual ~GUIWindow_Dialogue();

    void setDisplayedDialogueType(DialogueId type) {
        _displayedDialogue = type;
    }

    DialogueId getDisplayedDialogueType() {
        return _displayedDialogue;
    }

    Mm6StreetDialoguePage mm6StreetPage() const {
        return _mm6StreetPage;
    }

    virtual void Update() override;

 protected:
    DialogueId _displayedDialogue = DIALOGUE_MAIN;;
    // MM6: which street-dialogue menu this window offers (fame/reputation gates, see the enum).
    Mm6StreetDialoguePage _mm6StreetPage = MM6_STREET_PAGE_TALK;
};

void initializeNPCDialogue(int npcId, int bPlayerSaysHello, Actor *actor = nullptr);

void selectNPCDialogueOption(DialogueId option);

/**
 * MM6's Diplomacy bonus (MM6.EXE 0x4852D0), the "D" in every beg/threaten/bribe formula:
 * (Diplomacy skill level, +4 with a hired Counselor, +8 Barrister, +4 Negotiator) x 2/3/4 by
 * Novice/Expert/Master mastery.
 */
int mm6DiplomacyBonus(const Character &character);

/**
 * MM6's bribe price for the active character: max(10, (100 - diplomacy bonus) x (bribes paid so
 * far + 1) / 2) (MM6.EXE 0x4A4092). Every bribe paid anywhere makes the next one pricier.
 */
int mm6BribeCost();

extern int speakingNpcId;
extern const IndexedArray<std::string, PartyAlignment_Good, PartyAlignment_Evil> dialogueBackgroundResourceByAlignment;
