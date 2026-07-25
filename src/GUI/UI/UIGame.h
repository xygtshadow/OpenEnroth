#pragma once

#include <string_view>

#include "GUI/GUIWindow.h"

void GameUI_LoadPlayerPortraitsAndVoices();
void GameUI_ReloadPlayerPortraits(int player_id, int face_id);
void GameUI_WritePointedObjectStatusString();
void GameUI_OnPlayerPortraitLeftClick(int uPlayerID);  // idb
void buttonbox(int x, int y, std::string_view text, int col);
void GameUI_handleHintMessage(UIMessageType type, int param);

class GUIWindow_GameMenu : public GUIWindow {
 public:
    GUIWindow_GameMenu();
    virtual ~GUIWindow_GameMenu() {}

    virtual void Update() override;
};

class GUIWindow_GameOptions : public GUIWindow {
 public:
    GUIWindow_GameOptions();
    virtual ~GUIWindow_GameOptions() {}

    virtual void Update() override;

 private:
    void createVolumeSliderButtons(int row, UIMessageType message);
    void drawVolumeSliderThumb(int row, int level);
};

class GUIWindow_GameKeyBindings : public GUIWindow {
 public:
    GUIWindow_GameKeyBindings();
    virtual ~GUIWindow_GameKeyBindings() {}

    void Update() override;
};



class GUIWindow_GameVideoOptions : public GUIWindow {
 public:
    GUIWindow_GameVideoOptions();
    virtual ~GUIWindow_GameVideoOptions() {}

    virtual void Update() override;
};

class GraphicsImage;
extern GraphicsImage *game_ui_statusbar;
extern GraphicsImage *game_ui_rightframe;
extern GraphicsImage *game_ui_topframe;
extern GraphicsImage *game_ui_leftframe;
extern GraphicsImage *game_ui_bottomframe;

extern GraphicsImage *game_ui_monster_hp_green;
extern GraphicsImage *game_ui_monster_hp_yellow;
extern GraphicsImage *game_ui_monster_hp_red;
extern GraphicsImage *game_ui_monster_hp_background;
extern GraphicsImage *game_ui_monster_hp_border_left;
extern GraphicsImage *game_ui_monster_hp_border_right;

extern GraphicsImage *game_ui_minimap_frame;    // 5079D8
extern GraphicsImage *game_ui_minimap_compass;  // 5079B4
extern std::array<GraphicsImage *, 8> game_ui_minimap_dirs;

extern GraphicsImage *game_ui_menu_quit;
extern GraphicsImage *game_ui_menu_resume;
extern GraphicsImage *game_ui_menu_controls;
extern GraphicsImage *game_ui_menu_save;
extern GraphicsImage *game_ui_menu_load;
extern GraphicsImage *game_ui_menu_new;
extern GraphicsImage *game_ui_menu_options;

extern GraphicsImage *game_ui_tome_storyline;
extern GraphicsImage *game_ui_tome_calendar;
extern GraphicsImage *game_ui_tome_maps;
extern GraphicsImage *game_ui_tome_autonotes;
extern GraphicsImage *game_ui_tome_quests;

extern GraphicsImage *game_ui_btn_rest;
extern GraphicsImage *game_ui_btn_cast;
extern GraphicsImage *game_ui_btn_zoomin;
extern GraphicsImage *game_ui_btn_zoomout;
extern GraphicsImage *game_ui_btn_quickref;
extern GraphicsImage *game_ui_btn_settings;

extern GraphicsImage *game_ui_dialogue_background;

extern std::array<GraphicsImage *, 5> game_ui_options_controls;

extern GraphicsImage *game_ui_evtnpc;  // 50795C

extern std::array<std::array<GraphicsImage *, 56>, 4> game_ui_player_faces;
extern GraphicsImage *game_ui_player_face_eradicated;
extern GraphicsImage *game_ui_player_face_dead;

extern GraphicsImage *game_ui_player_selection_frame;  // 50C98C
extern GraphicsImage *game_ui_player_alert_yellow;     // 5079C8
extern GraphicsImage *game_ui_player_alert_red;        // 5079CC
extern GraphicsImage *game_ui_player_alert_green;      // 5079D0

extern GraphicsImage *game_ui_bar_red;
extern GraphicsImage *game_ui_bar_yellow;
extern GraphicsImage *game_ui_bar_green;
extern GraphicsImage *game_ui_bar_blue;

// MM6 in-game HUD skin (see MM6.EXE HUD draw cluster @0x417dc0/0x417df0/0x486900).
extern std::array<GraphicsImage *, 4> game_ui_mm6_tapestries;  // TAP1..TAP4, the top-right arch backgrounds.
extern GraphicsImage *game_ui_mm6_border5;   // Viewport top-left corner patch.
extern GraphicsImage *game_ui_mm6_border6;   // Viewport top-right corner patch.
extern GraphicsImage *game_ui_mm6_facemask;  // Oval mask drawn over each portrait.
extern GraphicsImage *game_ui_mm6_buttyes;   // buttyes1, the unpressed confirm button on dialogue panels.

/**
 * @return The MM6 top-right tapestry (sky-through-the-arch) for an in-game hour, per MM6.EXE 0x417960:
 *         night for [21, 5), dawn at 5, day for [6, 20), dusk at 20.
 */
GraphicsImage *mm6TapestryForHour(int hour);

/**
 * @return X position of the MM6 compass ribbon for a party view yaw in TrigLUT units, per MM6.EXE 0x417df0.
 *         The ribbon is drawn at y=10 clipped to x [536, 578).
 */
int mm6CompassRibbonX(int viewYaw);

extern GraphicsImage *game_ui_playerbuff_pain_reflection;
extern GraphicsImage *game_ui_playerbuff_hammerhands;
extern GraphicsImage *game_ui_playerbuff_preservation;
extern GraphicsImage *game_ui_playerbuff_bless;

extern int game_ui_wizardEye;
extern int game_ui_torchLight;

extern bool bFlashHistoryBook;
extern bool bFlashAutonotesBook;
extern bool bFlashQuestBook;

struct OptionsMenuSkin {
    OptionsMenuSkin();
    void Release();

    GraphicsImage *uTextureID_Background;       // 507C60
    GraphicsImage *uTextureID_TurnSpeed[3];     // 507C64
    GraphicsImage *uTextureID_ArrowLeft;        // 507C70
    GraphicsImage *uTextureID_ArrowRight;       // 507C74
    GraphicsImage *uTextureID_unused_0;         // 507C78
    GraphicsImage *uTextureID_unused_1;         // 507C7C
    GraphicsImage *uTextureID_unused_2;         // 507C80
    GraphicsImage *uTextureID_FlipOnExit;       // 507C84
    GraphicsImage *uTextureID_SoundLevels[10];  // 507C88
    GraphicsImage *uTextureID_AlwaysRun;        // 507CB0
    GraphicsImage *uTextureID_WalkSound;        // 507CB4
    GraphicsImage *uTextureID_ShowDamage;       // 507CB8

    // MM6's Controls screen has no Always Run / Flip on Exit rows, so the four MM7 option0X ticks above
    // stay null in an MM6 session (MM6's icons.lod has no such art). What it does have instead is a
    // Graphics Detail row and a single shared checkmark. See MM6.EXE 0x42d583 (loader) / 0x40f550 (draw).
    GraphicsImage *uTextureID_Mm6GraphicsDetail[3];  // con_High, con_Med, con_Low - indexed by detail level.
    GraphicsImage *uTextureID_Mm6Checkmark;          // con_X, drawn on every checked option row.
};
extern OptionsMenuSkin options_menu_skin;  // 507C60

/**
 * Geometry of one volume slider on the Controls screen. MM6 and MM7 plates differ, and both the button
 * rects and the click handlers are driven from here so that the two can't drift apart.
 */
struct VolumeSliderSkin {
    Pointi leftArrow;   // Top-left of the 16x16 left arrow button.
    Pointi rightArrow;  // Top-left of the 16x16 right arrow button.
    Recti bar;          // The clickable bar between the arrows.
    Pointi thumb;       // Top-left of the level indicator at level 0.
    int step;           // Horizontal pixels per volume level.
};

/**
 * @param row                           Slider row: 0 sound, 1 music, 2 voice.
 * @return                              Slider geometry for the current game version.
 */
VolumeSliderSkin volumeSliderSkin(int row);

/**
 * @return                              Gamma slider geometry on the video options screen, same contract
 *                                      as volumeSliderSkin(): both the buttons and the click handler
 *                                      read it, so the two can't drift.
 */
VolumeSliderSkin gammaSliderSkin();

/**
 * Mouse look sensitivity slider on MM6's key-binding screen. mm6-extra's own - vanilla MM6 has no such
 * screen, and MM7's optkb art leaves no free strip for one, so the setting stays ini-only in MM7.
 *
 * @return                              Slider geometry, same contract as gammaSliderSkin().
 */
VolumeSliderSkin mouseSensitivitySliderSkin();

/**
 * @param stop                          Slider stop, clamped into range.
 * @return                              Sensitivity value at that stop.
 */
float mouseSensitivityForStop(int stop);

/**
 * @return                              Stop nearest the configured sensitivity. A hand-edited ini value
 *                                      that sits between stops renders at the nearest one and is left
 *                                      alone until the player actually clicks the slider.
 */
int mouseSensitivityStop();

/**
 * @return                              Number of key-binding rows on one page of the controls menu.
 *                                      MM7's optkb art has exactly 14 engraved slots. MM6 has no art
 *                                      for the screen at all, so OE draws its own panel there and can
 *                                      fit all 30 configurable actions - the two strafe rows that MM7's
 *                                      art has no room for included - across its two pages.
 */
int keyBindingPageSize();

// The OE-drawn settings panel used for MM6's key-binding and video screens. MM6 shipped with neither,
// and its icons.lod has no art for either (optkb*, optvid, opvdH-*), so these draw a panel in MM6's
// stone-and-gold palette and use MM6's own small art - con_X ticks, con_ArrL/con_ArrR arrows and the
// convol* indicators - wherever it exists.
void drawMm6SettingsPanel(std::string_view title);
void drawMm6SettingsButton(const Recti &rect, std::string_view text, bool highlighted);
void drawMm6SettingsCheckRow(const Recti &rect, std::string_view text, bool checked);
