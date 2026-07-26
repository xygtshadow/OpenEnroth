#pragma once

#include <string>
#include <vector>

#include "Engine/Tables/NPCTable.h"
#include "GUI/GUIDialogues.h"

std::string npcDialogueOptionString(DialogueId topic, NPCData *npcData);

std::vector<DialogueId> prepareScriptedNPCDialogueTopics(NPCData *npcData);

/**
 * Builds the dialogue options for an NPC occupying a house.
 *
 * MM7 house NPCs offer their scripted topics (and Join) only. MM6's occupants lead with the
 * profession small talk / Join / News trio - each created only when the matching npcdata column is
 * non-zero - ahead of the scripted topics, exactly like its street dialogue (MM6.EXE 0x499b3a).
 */
std::vector<DialogueId> prepareHouseNPCDialogueTopics(NPCData *npcData);

DialogueId handleScriptedNPCTopicSelection(DialogueId topic, NPCData *npcData);
std::vector<DialogueId> listNPCDialogueOptions(DialogueId topic);

void selectSpecialNPCTopicSelection(DialogueId topic, NPCData* npcData);

extern int gold_transaction_amount;
