#include "GUI/UI/UISpellbook.h"

#include <string>

#include "Engine/AssetsManager.h"
#include "Engine/Engine.h"
#include "Engine/Objects/CharacterEnumFunctions.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Random/Random.h"
#include "Engine/Spells/Spells.h"
#include "Engine/Spells/SpellEnumFunctions.h"
#include "Engine/Localization.h"
#include "Engine/Party.h"
#include "Engine/Time/Timer.h"

#include "GUI/GUIButton.h"
#include "GUI/GUIFont.h"

#include "Io/Mouse.h"

#include "Media/Audio/AudioPlayer.h"

static constexpr IndexedArray<const char *, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> spellbook_texture_filename_suffices = {
    {MAGIC_SCHOOL_FIRE,   "f"},
    {MAGIC_SCHOOL_AIR,    "a"},
    {MAGIC_SCHOOL_WATER,  "w"},
    {MAGIC_SCHOOL_EARTH,  "e"},
    {MAGIC_SCHOOL_SPIRIT, "s"},
    {MAGIC_SCHOOL_MIND,   "m"},
    {MAGIC_SCHOOL_BODY,   "b"},
    {MAGIC_SCHOOL_LIGHT,  "l"},
    {MAGIC_SCHOOL_DARK,   "d"}
};

static constexpr IndexedArray<std::array<unsigned char, 12>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> pSpellbookSpellIndices = {{
    {MAGIC_SCHOOL_FIRE,     {0, 3, 1, 8, 11, 7, 4, 10, 6, 2, 5, 9}},
    {MAGIC_SCHOOL_AIR,      {0, 11, 2, 9, 6, 8, 5, 10, 3, 7, 1, 4}},
    {MAGIC_SCHOOL_WATER,    {0, 4, 8, 9, 1, 10, 3, 11, 7, 6, 2, 5}},
    {MAGIC_SCHOOL_EARTH,    {0, 7, 10, 8, 2, 11, 1, 5, 3, 6, 4, 9}},
    {MAGIC_SCHOOL_SPIRIT,   {0, 5, 10, 11, 7, 2, 8, 1, 4, 9, 3, 6}},
    {MAGIC_SCHOOL_MIND,     {0, 5, 9, 8, 3, 7, 6, 4, 1, 11, 2, 10}},
    {MAGIC_SCHOOL_BODY,     {0, 1, 6, 9, 3, 5, 8, 11, 7, 10, 4, 2}},
    {MAGIC_SCHOOL_LIGHT,    {0, 1, 10, 11, 9, 4, 3, 6, 5, 7, 8, 2}},
    {MAGIC_SCHOOL_DARK,     {0, 9, 3, 7, 1, 5, 2, 10, 11, 8, 6, 4}}
}};

static constexpr IndexedArray<const char *, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> texNames = {
    {MAGIC_SCHOOL_FIRE,   "SBFB00"},
    {MAGIC_SCHOOL_AIR,    "SBAB00"},
    {MAGIC_SCHOOL_WATER,  "SBWB00"},
    {MAGIC_SCHOOL_EARTH,  "SBEB00"},
    {MAGIC_SCHOOL_SPIRIT, "SBSB00"},
    {MAGIC_SCHOOL_MIND,   "SBMB00"},
    {MAGIC_SCHOOL_BODY,   "SBBB00"},
    {MAGIC_SCHOOL_LIGHT,  "SBLB00"},
    {MAGIC_SCHOOL_DARK,   "SBDB00"}
};

static constexpr IndexedArray<std::array<int, 2>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> texture_tab_coord1 = {{
    {MAGIC_SCHOOL_FIRE,     {406, 9}},
    {MAGIC_SCHOOL_AIR,      {406, 46}},
    {MAGIC_SCHOOL_WATER,    {406, 84}},
    {MAGIC_SCHOOL_EARTH,    {406, 121}},
    {MAGIC_SCHOOL_SPIRIT,   {407, 158}},
    {MAGIC_SCHOOL_MIND,     {405, 196}},
    {MAGIC_SCHOOL_BODY,     {405, 234}},
    {MAGIC_SCHOOL_LIGHT,    {405, 272}},
    {MAGIC_SCHOOL_DARK,     {405, 309}}
}};

static constexpr IndexedArray<std::array<int, 2>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> texture_tab_coord0 = {{
    {MAGIC_SCHOOL_FIRE,     {415, 10}},
    {MAGIC_SCHOOL_AIR,      {415, 46}},
    {MAGIC_SCHOOL_WATER,    {415, 83}},
    {MAGIC_SCHOOL_EARTH,    {415, 121}},
    {MAGIC_SCHOOL_SPIRIT,   {415, 158}},
    {MAGIC_SCHOOL_MIND,     {416, 196}},
    {MAGIC_SCHOOL_BODY,     {416, 234}},
    {MAGIC_SCHOOL_LIGHT,    {416, 271}},
    {MAGIC_SCHOOL_DARK,     {416, 307}}
}};

// MM6 composes each school page from twelve patch images "{prefix}{slot:03}" - slot 0 is the school
// emblem, slots 1..11 are the school's eleven spells in native id order - drawn at fixed per-slot
// positions, with the spell NAME rendered under each icon (spell.fnt). Prefix table from MM6.EXE
// 0x4bc3d8 (engine school order); icon positions from the tables at 0x4bc75c/0x4bc90c and the name
// anchors from 0x4bc3fc/0x4bc5ac (the EXE draws the name in a 100px window at (nameX+6, nameY-5)).
static constexpr IndexedArray<const char *, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> kMm6SchoolPrefixes = {
    {MAGIC_SCHOOL_FIRE,   "fire"},
    {MAGIC_SCHOOL_AIR,    "air"},
    {MAGIC_SCHOOL_WATER,  "wtr"},
    {MAGIC_SCHOOL_EARTH,  "earth"},
    {MAGIC_SCHOOL_SPIRIT, "sprt"},
    {MAGIC_SCHOOL_MIND,   "mind"},
    {MAGIC_SCHOOL_BODY,   "body"},
    {MAGIC_SCHOOL_LIGHT,  "lite"},
    {MAGIC_SCHOOL_DARK,   "dark"}
};

static constexpr IndexedArray<std::array<Pointi, 12>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> kMm6SpellIconPos = {{
    {MAGIC_SCHOOL_FIRE,   {{{48, 18}, {198, 32}, {313, 32}, {68, 107}, {183, 107}, {328, 107}, {68, 182}, {198, 182}, {312, 182}, {56, 254}, {198, 257}, {328, 257}}}},
    {MAGIC_SCHOOL_AIR,    {{{48, 18}, {198, 32}, {328, 31}, {67, 104}, {198, 103}, {328, 107}, {68, 182}, {185, 182}, {300, 182}, {66, 255}, {181, 252}, {328, 255}}}},
    {MAGIC_SCHOOL_WATER,  {{{48, 18}, {197, 32}, {297, 31}, {64, 107}, {193, 105}, {325, 107}, {50, 182}, {194, 182}, {325, 182}, {68, 257}, {188, 257}, {325, 253}}}},
    {MAGIC_SCHOOL_EARTH,  {{{48, 18}, {198, 31}, {302, 32}, {46, 104}, {162, 104}, {328, 107}, {58, 182}, {198, 181}, {327, 182}, {68, 256}, {197, 257}, {308, 257}}}},
    {MAGIC_SCHOOL_SPIRIT, {{{48, 18}, {195, 27}, {328, 32}, {64, 103}, {198, 107}, {328, 107}, {68, 180}, {198, 182}, {328, 182}, {68, 257}, {181, 253}, {328, 257}}}},
    {MAGIC_SCHOOL_MIND,   {{{48, 18}, {198, 32}, {328, 32}, {68, 107}, {186, 106}, {328, 107}, {68, 182}, {180, 182}, {315, 179}, {68, 257}, {197, 258}, {316, 257}}}},
    {MAGIC_SCHOOL_BODY,   {{{48, 18}, {191, 31}, {328, 32}, {68, 107}, {198, 107}, {328, 107}, {68, 182}, {195, 179}, {315, 182}, {68, 257}, {197, 257}, {315, 250}}}},
    {MAGIC_SCHOOL_LIGHT,  {{{48, 18}, {193, 32}, {321, 32}, {68, 107}, {198, 107}, {322, 107}, {61, 180}, {188, 182}, {327, 182}, {68, 257}, {198, 257}, {328, 257}}}},
    {MAGIC_SCHOOL_DARK,   {{{48, 18}, {198, 32}, {314, 32}, {61, 107}, {183, 107}, {328, 107}, {68, 182}, {195, 182}, {324, 182}, {48, 255}, {188, 257}, {328, 257}}}}
}};

static constexpr IndexedArray<std::array<Pointi, 12>, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> kMm6SpellNamePos = {{
    {MAGIC_SCHOOL_FIRE,   {{{46, 89}, {176, 89}, {307, 89}, {46, 161}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_AIR,    {{{46, 89}, {176, 89}, {307, 89}, {46, 161}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_WATER,  {{{46, 89}, {176, 89}, {307, 89}, {46, 161}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_EARTH,  {{{46, 89}, {176, 89}, {307, 89}, {46, 161}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_SPIRIT, {{{46, 89}, {176, 89}, {307, 89}, {46, 164}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_MIND,   {{{46, 89}, {176, 89}, {307, 89}, {46, 164}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_BODY,   {{{46, 89}, {176, 89}, {307, 89}, {46, 161}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 314}}}},
    {MAGIC_SCHOOL_LIGHT,  {{{46, 89}, {176, 89}, {307, 89}, {46, 164}, {176, 164}, {307, 164}, {46, 240}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 310}}}},
    {MAGIC_SCHOOL_DARK,   {{{46, 89}, {176, 89}, {307, 89}, {46, 164}, {176, 164}, {307, 164}, {46, 236}, {176, 240}, {307, 240}, {46, 314}, {176, 314}, {307, 310}}}}
}};

SpellId spellbookSelectedSpell;

GUIWindow_Spellbook::GUIWindow_Spellbook() : GUIWindow(WINDOW_SpellBook, {0, 0}, render->GetRenderDimensions()) {
    current_screen_type = SCREEN_SPELL_BOOK;
    pEventTimer->setPaused(true);

    initializeTextures();
    openSpellbook();

    // Sound 48 is absent in MM7
    pAudioPlayer->playUISound(SOUND_48);
}

void GUIWindow_Spellbook::openSpellbookPage(MagicSchool page) {
    onCloseSpellBookPage();
    pParty->activeCharacter().lastOpenedSpellbookPage = page;
    openSpellbook();
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x42cc4f: the page-flip pair is sounds 203/204, one below MM7's 204/205
        // (203 doubles as MM6's TABSPELL/emblem feedback sound).
        pAudioPlayer->playUISound(vrng->randomBool() ? SOUND_TurnPage1 : SOUND_fizzle);
    } else {
        pAudioPlayer->playUISound(vrng->randomBool() ? SOUND_TurnPage2 : SOUND_TurnPage1);
    }
}

void GUIWindow_Spellbook::openSpellbook() {
    int pageSpells = 0;
    const Character &player = pParty->activeCharacter();

    loadSpellbook();

    bool isMm6 = engine->gameVersion() == GAME_VERSION_MM6;
    MagicSchool chapter = player.lastOpenedSpellbookPage;
    for (SpellId spell : spellsForMagicSchool(chapter)) {
        if (!player.bHaveSpell[spell] && !engine->config->debug.AllMagic.value())
            continue;

        int index = spellIndexInMagicSchool(spell);
        Pointi iconPos = isMm6 ? kMm6SpellIconPos[chapter][index + 1]
                               : Pointi(pViewport.x + pIconPos[chapter][pSpellbookSpellIndices[chapter][index + 1]].Xpos,
                                        pViewport.y + pIconPos[chapter][pSpellbookSpellIndices[chapter][index + 1]].Ypos);
        CreateButton(fmt::format("SpellBook_Spell{}", index), iconPos,
                     SBPageSSpellsTextureList[index + 1]->size(), BUTTON_TYPE_NORMAL, UIMSG_Spellbook_ShowHightlightedSpellInfo,
                     UIMSG_SelectSpell, std::to_underlying(spell));
        pageSpells++;
    }

    CreateButton({0, 0}, {0, 0}, BUTTON_TYPE_NORMAL, 0, UIMSG_SpellBook_PressTab, 0, INPUT_ACTION_NEXT_CHAR);
    if (pageSpells) {
        setKeyboardControlGroup(pageSpells, true, 0, 0);
    }

    static constexpr IndexedArray<Pointi, MAGIC_SCHOOL_FIRST, MAGIC_SCHOOL_LAST> buttonPositions = {
        {MAGIC_SCHOOL_FIRE,     {399, 10}},
        {MAGIC_SCHOOL_AIR,      {399, 46}},
        {MAGIC_SCHOOL_WATER,    {399, 83}},
        {MAGIC_SCHOOL_EARTH,    {399, 121}},
        {MAGIC_SCHOOL_SPIRIT,   {399, 158}},
        {MAGIC_SCHOOL_MIND,     {400, 196}},
        {MAGIC_SCHOOL_BODY,     {400, 234}},
        {MAGIC_SCHOOL_LIGHT,    {400, 271}},
        {MAGIC_SCHOOL_DARK,     {400, 307}},
    };

    for (MagicSchool school : allMagicSchools())
        if (player.pActiveSkills[skillForMagicSchool(school)] || engine->config->debug.AllMagic.value())
            CreateButton(fmt::format("SpellBook_School{}", std::to_underlying(school)),
                         isMm6 ? Pointi(410, 13 + 35 * std::to_underlying(school)) : buttonPositions[school],
                         isMm6 ? Sizei(50, 34) : Sizei(50, 36), BUTTON_TYPE_NORMAL, 0, UIMSG_OpenSpellbookPage,
                         std::to_underlying(school), INPUT_ACTION_INVALID, localization->spellSchoolName(school));

    if (isMm6) {
        // MM6.EXE 0x40ce30: the quickspell (TABSPELL) and close (TABEXIT) tabs at the page's bottom edge,
        // plus a click region on the school emblem (48,18,126x82) that clears the installed quickspell
        // (same message as TABSPELL with param 1, handler 0x42ca93).
        pBtn_InstallRemoveSpell = CreateButton({301, 332}, ui_spellbook_btn_quckspell->size(), BUTTON_TYPE_NORMAL, UIMSG_HintSelectRemoveQuickSpellBtn,
                                               UIMSG_ClickInstallRemoveQuickSpellBtn, 0, INPUT_ACTION_INVALID, "");
        pBtn_CloseBook = CreateButton({360, 332}, ui_spellbook_btn_close->size(), BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                      localization->str(LSTR_EXIT_DIALOGUE));
        CreateButton("SpellBook_RemoveQuickSpell", {48, 18}, {126, 82}, BUTTON_TYPE_NORMAL, 0,
                     UIMSG_ClickInstallRemoveQuickSpellBtn, 1);
    } else {
        pBtn_InstallRemoveSpell = CreateButton({476, 450}, ui_spellbook_btn_quckspell->size(), BUTTON_TYPE_NORMAL, UIMSG_HintSelectRemoveQuickSpellBtn,
                                               UIMSG_ClickInstallRemoveQuickSpellBtn, 0, INPUT_ACTION_INVALID, "", {ui_spellbook_btn_quckspell_click});
        pBtn_CloseBook = CreateButton({561, 450}, ui_spellbook_btn_close->size(), BUTTON_TYPE_NORMAL, 0, UIMSG_Escape, 0, INPUT_ACTION_INVALID,
                                      localization->str(LSTR_EXIT_DIALOGUE), {ui_spellbook_btn_close_click});
    }
}

void GUIWindow_Spellbook::Update() {
    const Character &player = pParty->activeCharacter();
    int pX_coord, pY_coord;

    drawCurrentSchoolBackground();

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x40ddc0: school tabs run down (414, 13+35*i) - the current page's tab sits flush at
        // x=414, the others recessed at x=421 - and each KNOWN spell draws its patch plus its name in a
        // 100px window under the icon.
        for (MagicSchool page : allMagicSchools()) {
            if (!player.pActiveSkills[skillForMagicSchool(page)] && !engine->config->debug.AllMagic.value())
                continue;
            bool current = player.lastOpenedSpellbookPage == page;
            render->DrawQuad2D(ui_spellbook_school_tabs[page][current ? 1 : 0],
                               {current ? 414 : 421, 13 + 35 * std::to_underlying(page)});
        }

        MagicSchool page = player.lastOpenedSpellbookPage;
        for (SpellId spell : spellsForMagicSchool(page)) {
            if (!player.bHaveSpell[spell] && !engine->config->debug.AllMagic.value())
                continue;
            int slot = spellIndexInMagicSchool(spell) + 1;
            if (SBPageSSpellsTextureList[slot])
                render->DrawQuad2D(SBPageSSpellsTextureList[slot], kMm6SpellIconPos[page][slot]);

            Pointi namePos = kMm6SpellNamePos[page][slot];
            Recti nameWindow(namePos.x + 6, namePos.y - 5, 100, 344);
            // MM6.EXE 0x40ddc0 composes the name shades from the pixel-format globals: near-black
            // (the 16-bit literal 1) normally, blue (15,15,255) for the installed quickspell, red
            // (255,15,15) for the selected slot (a second click casts it), and magenta (235,15,255)
            // when the selected slot IS the quickspell.
            Color nameColor = Color(0, 0, 8);
            if (spellbookSelectedSpell == spell)
                nameColor = (player.uQuickSpell == spell) ? Color(235, 15, 255) : Color(255, 15, 15);
            else if (player.uQuickSpell == spell)
                nameColor = Color(15, 15, 255);
            DrawTitleText(assets->pFontBookLloyds.get(), 0, 0, nameColor, pSpellStats->pInfos[spell].name, 3, nameWindow);
        }
        return;
    }

    for (MagicSchool page : allMagicSchools()) {
        Skill skill = skillForMagicSchool(page);

        if (player.pActiveSkills[skill] || engine->config->debug.AllMagic.value()) {
            auto pPageTexture = ui_spellbook_school_tabs[page][0];
            if (player.lastOpenedSpellbookPage == page) {
                pPageTexture = ui_spellbook_school_tabs[page][1];
                pX_coord = texture_tab_coord1[page][0];
                pY_coord = texture_tab_coord1[page][1];
            } else {
                pPageTexture = ui_spellbook_school_tabs[page][0];
                pX_coord = texture_tab_coord0[page][0];
                pY_coord = texture_tab_coord0[page][1];
            }
            render->DrawQuad2D(pPageTexture, {pX_coord, pY_coord});

            Pointi mousePos = mouse->position();

            for (SpellId spell : spellsForMagicSchool(player.lastOpenedSpellbookPage)) {
                int index = spellIndexInMagicSchool(spell);
                if (player.bHaveSpell[spell] || engine->config->debug.AllMagic.value()) {
                    // this should check if player knows spell
                    if (SBPageSSpellsTextureList[index + 1]) {
                        GraphicsImage *pTexture = (spellbookSelectedSpell == spell) ? SBPageCSpellsTextureList[index + 1] : SBPageSSpellsTextureList[index + 1];
                        if (pTexture) {
                            SpellBookIconPos &iconPos = pIconPos[player.lastOpenedSpellbookPage][pSpellbookSpellIndices[player.lastOpenedSpellbookPage][index + 1]];

                            pX_coord = pViewport.x + iconPos.Xpos;
                            pY_coord = pViewport.y + iconPos.Ypos;

                            Recti iconRect = Recti(pX_coord, pY_coord, pTexture->width(), pTexture->height());
                            if (iconRect.contains(mousePos)) { // mouseover highlight
                                if (SBPageCSpellsTextureList[index + 1]) {
                                    render->DrawQuad2D(SBPageCSpellsTextureList[index + 1], {pX_coord, pY_coord});
                                }
                            } else {
                                render->DrawQuad2D(pTexture, {pX_coord, pY_coord});
                            }
                        }
                    }
                }
            }
        }
    }
}

GUIWindow_Spellbook::~GUIWindow_Spellbook() {
    onCloseSpellBookPage();
    onCloseSpellBook();
}

void GUIWindow_Spellbook::loadSpellbook() {
    MagicSchool page = pParty->activeCharacter().lastOpenedSpellbookPage;
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x40ccca zeroes the selected-slot flag (0x4cb214) on every page load - opening
        // the book or flipping a page always starts with nothing selected, quickspell included.
        spellbookSelectedSpell = SPELL_NONE;
    } else if (pParty->activeCharacter().uQuickSpell != SPELL_NONE && magicSchoolForSpell(pParty->activeCharacter().uQuickSpell) == page) {
        spellbookSelectedSpell = pParty->activeCharacter().uQuickSpell;
    } else {
        spellbookSelectedSpell = SPELL_NONE;
    }

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6's patches sit in slot order (slot i = the school's i-th spell, native id order) and there is
        // no highlight variant - the quickspell shows through the name color instead. The slot-0 emblem
        // draws unconditionally.
        ui_mm6_spellbook_emblem = assets->getImage_Alpha(fmt::format("{}000", kMm6SchoolPrefixes[page]));
        for (SpellId spell : spellsForMagicSchool(page)) {
            if (pParty->activeCharacter().bHaveSpell[spell] || engine->config->debug.AllMagic.value()) {
                int index = spellIndexInMagicSchool(spell);
                SBPageSSpellsTextureList[index + 1] = assets->getImage_Alpha(fmt::format("{}{:03}", kMm6SchoolPrefixes[page], index + 1));
                SBPageCSpellsTextureList[index + 1] = nullptr;
            }
        }
        return;
    }

    for (SpellId spell : spellsForMagicSchool(page)) {
        if (pParty->activeCharacter().bHaveSpell[spell] || engine->config->debug.AllMagic.value()) {
            int index = spellIndexInMagicSchool(spell);
            std::string pContainer;

            pContainer = fmt::format("SB{}S{:02}", spellbook_texture_filename_suffices[page], pSpellbookSpellIndices[page][index + 1]);
            SBPageSSpellsTextureList[index + 1] = assets->getImage_Solid(pContainer);

            pContainer = fmt::format("SB{}C{:02}", spellbook_texture_filename_suffices[page], pSpellbookSpellIndices[page][index + 1]);
            SBPageCSpellsTextureList[index + 1] = assets->getImage_Solid(pContainer);
        }
    }
}

void GUIWindow_Spellbook::drawCurrentSchoolBackground() {
    MagicSchool page = MAGIC_SCHOOL_FIRE;
    if (pParty->hasActiveCharacter()) {
        page = pParty->activeCharacter().lastOpenedSpellbookPage;
    }
    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6.EXE 0x40ddc0: the shared book parchment + page mask, the TABSPELL/TABEXIT tabs at the page's
        // bottom edge, and the school emblem ({prefix}000) at its slot-0 position.
        render->DrawQuad2D(ui_mm6_spellbook_base, {8, 8});
        render->DrawQuad2D(ui_mm6_spellbook_pagemask, {8, 8});
        render->DrawQuad2D(ui_spellbook_btn_quckspell, {301, 332});
        render->DrawQuad2D(ui_spellbook_btn_close, {360, 332});
        if (ui_mm6_spellbook_emblem)
            render->DrawQuad2D(ui_mm6_spellbook_emblem, kMm6SpellIconPos[page][0]);
        return;
    }
    render->DrawQuad2D(ui_spellbook_school_backgrounds[page], {8, 8});

    render->DrawQuad2D(ui_spellbook_btn_quckspell, {476, 450});
    render->DrawQuad2D(ui_spellbook_btn_close, {561, 450});
}

void GUIWindow_Spellbook::initializeTextures() {
    pAudioPlayer->playUISound(SOUND_openbook);

    if (engine->gameVersion() == GAME_VERSION_MM6) {
        // MM6's spellbook (MM6.EXE ctor 0x40ce30 + draw 0x40ddc0) sits on the shared book parchment with a
        // page mask, and its close/quickspell buttons are the TABEXIT/TABSPELL tabs at the page's bottom
        // edge. The school tab art shares MM7's names. There are no per-school backgrounds and no
        // pressed-state button art.
        ui_mm6_spellbook_base = assets->getImage_Solid("book");
        ui_mm6_spellbook_pagemask = assets->getImage_Alpha("pagemask");
        ui_spellbook_btn_close = assets->getImage_Alpha("tabexit");
        ui_spellbook_btn_close_click = nullptr;
        ui_spellbook_btn_quckspell = assets->getImage_Alpha("tabspell");
        ui_spellbook_btn_quckspell_click = nullptr;
        if (!assets->pFontBookLloyds)
            assets->pFontBookLloyds = GUIFont::LoadFont("spell.fnt");
    } else {
        ui_spellbook_btn_close = assets->getImage_Solid("ib-m5-u");
        ui_spellbook_btn_close_click = assets->getImage_Solid("ib-m5-d");
        ui_spellbook_btn_quckspell = assets->getImage_Solid("ib-m6-u");
        ui_spellbook_btn_quckspell_click = assets->getImage_Solid("ib-m6-d");
    }

    for (MagicSchool page : allMagicSchools()) {
        // IndexedArray members do not zero-initialize - write every slot (MM6 has no school backgrounds).
        ui_spellbook_school_backgrounds[page] = engine->gameVersion() == GAME_VERSION_MM6
            ? nullptr
            : assets->getImage_ColorKey(texNames[page]);
        ui_spellbook_school_tabs[page][0] = assets->getImage_Alpha(fmt::format("tab{}a", std::to_underlying(page) + 1));
        ui_spellbook_school_tabs[page][1] = assets->getImage_Alpha(fmt::format("tab{}b", std::to_underlying(page) + 1));
    }
}

void GUIWindow_Spellbook::onCloseSpellBook() {
    if (ui_mm6_spellbook_base) {
        ui_mm6_spellbook_base->release();
        ui_mm6_spellbook_base = nullptr;
    }
    if (ui_mm6_spellbook_pagemask) {
        ui_mm6_spellbook_pagemask->release();
        ui_mm6_spellbook_pagemask = nullptr;
    }
    if (ui_spellbook_btn_close) {
        ui_spellbook_btn_close->release();
        ui_spellbook_btn_close = nullptr;
    }
    if (ui_spellbook_btn_close_click) {
        ui_spellbook_btn_close_click->release();
        ui_spellbook_btn_close_click = nullptr;
    }

    if (ui_spellbook_btn_quckspell) {
        ui_spellbook_btn_quckspell->release();
        ui_spellbook_btn_quckspell = nullptr;
    }
    if (ui_spellbook_btn_quckspell_click) {
        ui_spellbook_btn_quckspell_click->release();
        ui_spellbook_btn_quckspell_click = nullptr;
    }

    for (MagicSchool page : allMagicSchools()) {
        if (ui_spellbook_school_backgrounds[page]) {
            ui_spellbook_school_backgrounds[page]->release();
            ui_spellbook_school_backgrounds[page] = nullptr;
        }

        if (ui_spellbook_school_tabs[page][0]) {
            ui_spellbook_school_tabs[page][0]->release();
            ui_spellbook_school_tabs[page][0] = nullptr;
        }
        if (ui_spellbook_school_tabs[page][1]) {
            ui_spellbook_school_tabs[page][1]->release();
            ui_spellbook_school_tabs[page][1] = nullptr;
        }
    }

    pAudioPlayer->playUISound(SOUND_closebook);
}

void GUIWindow_Spellbook::onCloseSpellBookPage() {
    if (ui_mm6_spellbook_emblem) {
        ui_mm6_spellbook_emblem->release();
        ui_mm6_spellbook_emblem = nullptr;
    }
    for (unsigned int i = 1; i <= 11; i++) {
        if (SBPageCSpellsTextureList[i]) {
            SBPageCSpellsTextureList[i]->release();
            SBPageCSpellsTextureList[i] = nullptr;
        }
        if (SBPageSSpellsTextureList[i]) {
            SBPageSSpellsTextureList[i]->release();
            SBPageSSpellsTextureList[i] = nullptr;
        }
    }

    if (pGUIWindow_CurrentMenu)
        pGUIWindow_CurrentMenu->DeleteButtons();
}
