#include "ItemEnumFunctions.h"

#include <cassert>
#include <string>

#include "Engine/Localization.h"
#include "Engine/Tables/ItemTable.h"

std::string displayNameForDamageType(DamageType damageType, Localization *localization) {
    switch (damageType) {
    case DAMAGE_FIRE:       return localization->spellSchoolName(MAGIC_SCHOOL_FIRE);
    case DAMAGE_AIR:        return localization->spellSchoolName(MAGIC_SCHOOL_AIR);
    case DAMAGE_WATER:      return localization->spellSchoolName(MAGIC_SCHOOL_WATER);
    case DAMAGE_EARTH:      return localization->spellSchoolName(MAGIC_SCHOOL_EARTH);
    case DAMAGE_PHYSICAL:   return localization->str(LSTR_PHYSICAL);
    case DAMAGE_MAGIC:      return localization->str(LSTR_MAGIC);
    case DAMAGE_SPIRIT:     return localization->spellSchoolName(MAGIC_SCHOOL_SPIRIT);
    case DAMAGE_MIND:       return localization->spellSchoolName(MAGIC_SCHOOL_MIND);
    case DAMAGE_BODY:       return localization->spellSchoolName(MAGIC_SCHOOL_BODY);
    case DAMAGE_LIGHT:      return localization->spellSchoolName(MAGIC_SCHOOL_LIGHT);
    case DAMAGE_DARK:       return localization->spellSchoolName(MAGIC_SCHOOL_DARK);
    case DAMAGE_ENERGY:     return localization->str(LSTR_ENERGY);
    default:
        assert(false);
        return {};
    }
}

// Spell bindings come from items.txt ("S<n>" in the Mod1 column), resolved into ItemData::spellId
// by ItemTable::LoadItems. For MM7 scrolls and spellbooks the data agrees with the legacy hardcoded
// maps that used to live here on all 99+99 entries; MM7 wand rows carry stale MM6 values, so LoadItems
// applies a hardcoded MM7 wand map instead. Reading the binding from the parsed table makes these
// functions work for MM6 item ids too (MM6 scrolls/books/wands live at different ids).
SpellId spellForSpellbook(ItemId spellbook) {
    assert(pItemTable->items[spellbook].type == ITEM_TYPE_BOOK);
    return pItemTable->items[spellbook].spellId;
}

SpellId spellForScroll(ItemId scroll) {
    assert(pItemTable->items[scroll].type == ITEM_TYPE_SPELL_SCROLL);
    return pItemTable->items[scroll].spellId;
}

SpellId spellForWand(ItemId wand) {
    assert(pItemTable->items[wand].type == ITEM_TYPE_WAND);
    return pItemTable->items[wand].spellId;
}
