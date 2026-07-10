#pragma once

#include <string>
#include <vector>

#include "Engine/Tables/HouseTable.h"
#include "Engine/MapEnums.h"

#include "GUI/GUIWindow.h"

#include "Utility/IndexedArray.h"

#include "UIHouseEnums.h"

// Right hand side dialogue writing constants
constexpr int SIDE_TEXT_BOX_WIDTH = 144;
constexpr int SIDE_TEXT_BOX_POS_X = 484;
constexpr int SIDE_TEXT_BOX_POS_Z = 334;
constexpr int SIDE_TEXT_BOX_POS_Y = 113;
constexpr int SIDE_TEXT_BOX_BODY_TEXT_HEIGHT = 174;
constexpr int SIDE_TEXT_BOX_BODY_TEXT_OFFSET = 138;
constexpr int SIDE_TEXT_BOX_MAX_SPACING = 32;

// MM6 dialogue-skin layout, reversed from MM6.EXE (street dialogue draw @0x43ab90, house dialogue
// draw @0x497ebf, transition draws @0x43a300/0x43a630): dialogue screens keep the regular HUD and
// blit an "evpan###" marble panel (152x353) over the right column, with the portrait / transition
// picture directly on it (MM6 has no evtnpc portrait frame). The confirm/cancel buttons
// (buttyes*/buttesc*, 61x28, blit x offsets 0x3CC/0x41C/0x46C = 486/526/566) sit on the panel's
// bottom row; the row's y comes from a runtime global the disassembly doesn't pin down, placed
// here 7px above the panel's bottom edge, mirroring the side margins.
constexpr Pointi MM6_DIALOGUE_PANEL_POS = {481, 0};         // evpan###, 152x353.
constexpr Pointi MM6_DIALOGUE_PORTRAIT_POS = {525, 34};     // npc###, 63x73.
constexpr Pointi MM6_DIALOGUE_YES_BUTTON_POS = {486, 318};  // buttyes*.
constexpr Pointi MM6_DIALOGUE_ESC_BUTTON_POS = {566, 318};  // buttesc* when paired with a yes-button.
constexpr Pointi MM6_DIALOGUE_ESC_CENTERED_POS = {526, 318};  // buttesc* when it is the only button.
constexpr Sizei MM6_DIALOGUE_BUTTON_SIZE = {61, 28};

void BackToHouseMenu();

/**
 * @offset 0x4BCACC
 */
void selectProprietorDialogueOption(DialogueId option);

/**
 * @offset 0x44606A
 */
void prepareHouse(HouseId house);

void createHouseUI(HouseId houseId);

/**
 * @offset 0x44622E
 */
bool enterHouse(HouseId uHouseID);

// House id the last successful enterHouse() actually opened - differs from the requested id when
// the entry is redirected (throne room -> jail, MM6's King's Library Tanir's-Bell chain).
extern HouseId enteredHouseId;

bool houseDialogPressEscape();

/**
 * @offset 0x4B1E92
 */
void playHouseSound(HouseId houseID, HouseSoundType type);

void selectHouseNPCDialogueOption(DialogueId topic);

/**
 * @offset 0x4B4224
 */
void updateHouseNPCTopics(int npc);

/**
 * Type of NPC you can have dialogue with inside house.
 */
enum class HouseNpcType {
    HOUSE_PROPRIETOR, // default resident in non-simple houses (shop owner, temple priest etc.).
    HOUSE_NPC,        // regular NPC, have description in NPCs table, @npc field is points to it.
    HOUSE_TRANSITION  // transition point to different map, @targetMapID contains ID of target map.
};
using enum HouseNpcType;

struct HouseNpcDesc {
    HouseNpcType type;
    std::string label = "";
    GraphicsImage *icon = nullptr;
    GUIButton *button = nullptr;
    MapId targetMapID = MAP_INVALID;
    NPCData *npc = nullptr;
};

class GUIWindow_House : public GUIWindow {
 public:
    explicit GUIWindow_House(HouseId houseId);
    virtual ~GUIWindow_House();

    virtual void Update() override;

    HouseType buildingType() const {
        return houseTable[houseId()].uType;
    }

    HouseId houseId() const {
        return _houseId;
    }

    DialogueId getCurrentDialogue() const {
        return _currentDialogue;
    }

    void setCurrentDialogue(DialogueId dialogue) {
        _currentDialogue = dialogue;
    }

    void houseDialogManager();
    void houseNPCDialogue();
    void initializeProprietorDialogue();
    void initializeNPCDialogue(int npc);
    void initializeNPCDialogueButtons(std::vector<DialogueId> optionList);
    void learnSelectedSkill(Skill skill);
    void reinitDialogueWindow();
    bool checkIfPlayerCanInteract();

    void drawOptions(std::vector<std::string> &optionsText, Color selectColor,
                     int topOptionShift = 0, bool denseSpacing = false) const;

    virtual void houseDialogueOptionSelected(DialogueId option);
    virtual void houseSpecificDialogue();
    virtual std::vector<DialogueId> listDialogueOptions();
    virtual void updateDialogueOnEscape();
    virtual void houseScreenClick();
    virtual void playHouseGoodbyeSpeech();

 private:
    void drawNpcHouseNameAndTitle(NPCData *npcData);
    void drawNpcHouseGreetingMessage(NPCData *npcData);
    void drawNpcHouseDialogueOptions(NPCData *npcData) const;
    void drawNpcHouseDialogueResponse();

 protected:
    void learnSkillsDialogue(Color selectColor);

    HouseId _houseId = HOUSE_INVALID;
    DialogueId _currentDialogue = DIALOGUE_NULL;
    int _savedButtonsNum{};
    bool _transactionPerformed = false;
};

// Originally was a packed struct.
struct HouseAnimDescr {
    std::string video_name;
    int uDialoguePanelId; // MM6: the "evpan###" dialogue-panel bitmap index. Unused by MM7 (leftover data).
    int house_npc_id;
    HouseType uBuildingType; // Originally was 1 byte.
    uint8_t uRoomSoundId;
    uint16_t padding_e;
};

extern GraphicsImage *_591428_endcap;

extern std::array<const HouseAnimDescr, 196> pAnimatedRooms;
extern std::array<const HouseAnimDescr, 119> pAnimatedRoomsMm6;

/**
 * Version-aware accessor for the animated-rooms table (MM7 0x4E5F70 / MM6.EXE 0x4BE888
 * "HouseMovies"). MM6 records carry the house FLC animation name, the evpan dialogue-panel index
 * (in `uDialoguePanelId`), the proprietor portrait id and the room sound id; indexing MM7's table with MM6
 * animation ids used to yield garbage sound/portrait ids. Out-of-range ids resolve to entry 0.
 */
const HouseAnimDescr &houseAnimDescr(int animId);

/**
 * Version-aware name of a transition picture for a 2dEvents exit-pic id / event transition
 * (MM7 `pHouse_ExitPictures`, MM6.EXE name table @0x4BEFF8). Out-of-range ids resolve to entry 0.
 */
const char *houseExitPictureName(unsigned picId);

extern const IndexedArray<int, HOUSE_TYPE_WEAPON_SHOP, HOUSE_TYPE_DARK_GUILD> itemAmountInShop;

extern std::vector<HouseNpcDesc> houseNpcs;
extern int currentHouseNpc;
