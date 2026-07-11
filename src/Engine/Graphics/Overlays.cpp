#include "Engine/Graphics/Overlays.h"

#include <array>
#include <string>
#include <tuple>

#include "Engine/Party.h"
#include "Engine/Time/Timer.h"
#include "Engine/mm7_data.h"
#include "Engine/Graphics/Renderer/Renderer.h"
#include "Engine/Objects/Actor.h"
#include "Engine/Tables/IconFrameTable.h"
#include "Engine/TurnEngine/TurnEngine.h"

#include "GUI/GUIWindow.h"

#include "Sprites.h"


ActiveOverlayList *pActiveOverlayList = new ActiveOverlayList;  // idb
OverlayList *pOverlayList = new OverlayList;

// Fixed screen anchor for a screen-overlay target code, from the switch in MM6.EXE 0x4358F0. Anchors with
// OVERLAY_FLAG_BUFF_OWNED are the persistent per-character buff fx that a SpellBuff owns and frees.
struct OverlayScreenAnchor {
    int16_t x = 0;
    int16_t y = 0;
    int16_t flags = 0;
};

static bool overlayScreenAnchorForTarget(int target, OverlayScreenAnchor *anchor) {
    if (target >= 100 && target <= 103) { // One-shot spell cast fx over a character portrait.
        static constexpr int16_t xs[4] = {51, 163, 276, 388};
        *anchor = {xs[target - 100], 422, 0};
        return true;
    }
    if (target == 201) {
        *anchor = {40, 326, 0};
        return true;
    }
    if (target == 202) {
        *anchor = {23, 24, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    if (target == 203) {
        *anchor = {447, 17, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    // The five persistent per-character buff fx rows near the portraits.
    if (target >= 310 && target <= 313) {
        static constexpr int16_t xs[4] = {19, 132, 245, 357};
        *anchor = {xs[target - 310], 456, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    if (target >= 320 && target <= 323) {
        static constexpr int16_t xs[4] = {31, 144, 257, 369};
        *anchor = {xs[target - 320], 468, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    if (target >= 330 && target <= 333) {
        static constexpr int16_t xs[4] = {50, 163, 276, 388};
        *anchor = {xs[target - 330], 472, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    if (target >= 340 && target <= 343) {
        static constexpr int16_t xs[4] = {69, 182, 295, 407};
        *anchor = {xs[target - 340], 468, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    if (target >= 350 && target <= 353) {
        static constexpr int16_t xs[4] = {82, 195, 308, 420};
        *anchor = {xs[target - 350], 469, OVERLAY_FLAG_BUFF_OWNED};
        return true;
    }
    return false; // Target 200 anchors at uninitialized registers in the EXE; treat it and unknowns as unanchored.
}

static int overlayListIndexForId(int uOverlayID) {
    for (int i = 0; i < static_cast<int>(pOverlayList->pOverlays.size()); i++)
        if (uOverlayID == pOverlayList->pOverlays[i].uOverlayID)
            return i;
    return static_cast<int>(pOverlayList->pOverlays.size());
}

/**
 * @return                              Current animation frame for an overlay-list index, or nullptr when there is
 *                                      nothing drawable (out-of-range index, or the "null" sprite that all MM7
 *                                      doverlay.bin entries point to).
 */
static SpriteFrame *overlaySpriteFrame(int indexToOverlayList, Duration time) {
    if (indexToOverlayList < 0 || indexToOverlayList >= static_cast<int>(pOverlayList->pOverlays.size()))
        return nullptr;
    const OverlayDesc &desc = pOverlayList->pOverlays[indexToOverlayList];
    if (!desc.uSpriteFramesetID)
        return nullptr;
    SpriteFrame *frame = pSpriteFrameTable->GetFrame(desc.uSpriteFramesetID, time);
    if (!frame || frame->spriteName == "null" || !frame->sprites[0] || !frame->sprites[0]->texture)
        return nullptr;
    if (frame->sprites[0]->texture->height() == 0 || frame->sprites[0]->texture->width() == 0)
        return nullptr;
    return frame;
}

/**
 * Draws a sprite frame in screen space, anchored like the original billboard rasterizer: `pos` is the bottom-center
 * of the drawn sprite.
 */
static void drawOverlaySprite2D(SpriteFrame *frame, float scale, Pointi pos) {
    int w = static_cast<int>(frame->sprites[0]->uWidth * scale);
    int h = static_cast<int>(frame->sprites[0]->uHeight * scale);
    if (w <= 0 || h <= 0)
        return;
    render->DrawImage(frame->sprites[0]->texture, Recti(pos.x - w / 2, pos.y - h, w, h), frame->paletteId);
}

// inlined
//----- (mm6c::0045BD50) --------------------------------------------------
void ActiveOverlayList::Reset() {
    for (unsigned int i = 0; i < 50; ++i) pOverlays[i].Reset();
}

//----- (004418B6), mm6: 0x435E00 ------------------------------------------
int ActiveOverlayList::addPidOverlay(int uOverlayID, Pid pid, Duration animLength, int fpDamageMod, int16_t flags) {
    for (unsigned int i = 0; i < 50; ++i) {
        if (this->pOverlays[i].animLength <= 0) {
            ActiveOverlay &slot = this->pOverlays[i];
            slot.Reset();
            slot.pid = pid;
            slot.indexToOverlayList = overlayListIndexForId(uOverlayID);
            if (slot.indexToOverlayList >= static_cast<int>(pOverlayList->pOverlays.size()))
                return 0; // Not in doverlay.bin - e.g. MM6's cast-fx table references 9050/9070, which its shipped data lacks.
            Duration length = animLength;
            if (!length)
                length = pSpriteFrameTable->pSpriteSFrames[pOverlayList->pOverlays[slot.indexToOverlayList].uSpriteFramesetID].animationLength;
            slot.animLength = length.ticks();
            slot.fpDamageMod = fpDamageMod;
            slot.flags = flags;
            return i + 1;
        }
    }
    return 0;
}

//----- mm6: 0x4358F0 (stubbed out in MM7.EXE at 0x4418B1) ------------------
int ActiveOverlayList::addScreenOverlay(int uOverlayID, int target, Duration animLength, int fpDamageMod) {
    // Reuse an already-active overlay on the same persistent anchor - one buff fx per anchor.
    if (target > 200) {
        for (unsigned int i = 0; i < 50; ++i) {
            if (this->pOverlays[i].animLength > 0 && this->pOverlays[i].target == target)
                return i + 1;
        }
    }

    for (unsigned int i = 0; i < 50; ++i) {
        if (this->pOverlays[i].animLength <= 0) {
            ActiveOverlay &slot = this->pOverlays[i];
            slot.Reset();
            slot.target = target;
            OverlayScreenAnchor anchor;
            if (overlayScreenAnchorForTarget(target, &anchor)) {
                slot.screenSpaceX = anchor.x;
                slot.screenSpaceY = anchor.y;
                slot.flags = anchor.flags;
            }
            slot.indexToOverlayList = overlayListIndexForId(uOverlayID);
            if (slot.indexToOverlayList >= static_cast<int>(pOverlayList->pOverlays.size()))
                return 0; // Not in doverlay.bin - e.g. MM6's cast-fx table references 9050/9070, which its shipped data lacks.
            Duration length = animLength;
            if (!length)
                length = pSpriteFrameTable->pSpriteSFrames[pOverlayList->pOverlays[slot.indexToOverlayList].uSpriteFramesetID].animationLength;
            slot.animLength = length.ticks();
            slot.fpDamageMod = fpDamageMod;
            return i + 1;
        }
    }
    return 0;
}

//----- mm6: 0x436797 -------------------------------------------------------
void ActiveOverlayList::update(Duration dt) {
    for (ActiveOverlay &slot : pOverlays) {
        if (slot.animLength <= 0)
            continue;
        if (slot.flags & OVERLAY_FLAG_BUFF_OWNED)
            continue; // Shows its first frame until the owning SpellBuff resets the slot.
        if (slot.flags & OVERLAY_FLAG_LOOPING) {
            slot.spriteFrameTime = (slot.spriteFrameTime + dt.ticks()) % slot.animLength; // % keeps the int16 from wrapping negative.
            continue;
        }
        if (slot.spriteFrameTime < slot.animLength) {
            slot.spriteFrameTime += dt.ticks();
            if (slot.spriteFrameTime >= slot.animLength) { // One-shot finished - free the slot.
                slot.animLength = 0;
                slot.pid = Pid();
            }
        }
    }
}

//----- mm6: the pid branch of 0x4361A0 -------------------------------------
void ActiveOverlayList::prepareBillboards() {
    for (const ActiveOverlay &slot : pOverlays) {
        if (slot.animLength <= 0 || slot.pid.type() != OBJECT_Actor)
            continue;
        if (slot.pid.id() < 0 || slot.pid.id() >= static_cast<int>(pActors.size()))
            continue;
        if (::uNumBillboardsToDraw >= 499)
            break;
        SpriteFrame *frame = overlaySpriteFrame(slot.indexToOverlayList, Duration::fromTicks(slot.spriteFrameTime));
        if (!frame)
            continue;
        float scale = frame->scale * (slot.fpDamageMod / 65536.0f);
        const Actor &actor = pActors[slot.pid.id()];
        Vec3f pos = actor.pos;
        pos.z += 0.8f * actor.height; // The EXE anchors overlays at 80% of the actor's height.
        if (pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayType == 0)
            pos.z -= 0.5f * scale * frame->sprites[0]->uHeight; // "center": billboards anchor at their bottom edge.
        render->AddBillboardIfVisible(frame->sprites[0], frame->paletteId, pos, {scale, scale}, BillboardFlags(),
                                      Pid(), actor.sectorId);
    }
}

//----- mm6: 0x436010 & the pid==0 branch of 0x4361A0 -----------------------
void ActiveOverlayList::drawScreenOverlays() {
    for (const ActiveOverlay &slot : pOverlays) {
        if (slot.animLength <= 0 || slot.pid || !slot.target)
            continue;
        SpriteFrame *frame = overlaySpriteFrame(slot.indexToOverlayList, Duration::fromTicks(slot.spriteFrameTime));
        if (!frame)
            continue;
        Pointi pos = Pointi(slot.screenSpaceX, slot.screenSpaceY);
        uint16_t type = pOverlayList->pOverlays[slot.indexToOverlayList].uOverlayType;
        if (type == 0 || type == 2)
            pos.y += frame->sprites[0]->uHeight / 2; // Unscaled in the EXE too.
        drawOverlaySprite2D(frame, frame->scale * (slot.fpDamageMod / 65536.0f), pos);
    }
}

//----- mm6: 0x4354E0 -------------------------------------------------------
void drawMm6PartyBuffStatusOverlays() {
    // MM6 party buff -> status overlay id + x position (y is always 254). MM6's resistance and magic-protection
    // buffs land in these engine buff slots per the established MM6 damage-type mapping (Elec->Air, Cold->Water,
    // Pois->Earth, Magic->ProtectionFromMagic).
    static constexpr std::array<std::tuple<PartyBuff, int, int>, 7> statusOverlays = {{
        {PARTY_BUFF_FEATHER_FALL, 10013, 500},
        {PARTY_BUFF_RESIST_FIRE, 10009, 521},
        {PARTY_BUFF_RESIST_AIR, 10010, 543},
        {PARTY_BUFF_WIZARD_EYE, 10015, 556},
        {PARTY_BUFF_RESIST_WATER, 10011, 567},
        {PARTY_BUFF_RESIST_EARTH, 10012, 588},
        {PARTY_BUFF_PROTECTION_FROM_MAGIC, 10014, 610},
    }};

    for (const auto &[buff, overlayId, x] : statusOverlays) {
        if (!pParty->pPartyBuffs[buff].Active())
            continue;
        SpriteFrame *frame = overlaySpriteFrame(overlayListIndexForId(overlayId), pMiscTimer->time());
        if (!frame)
            continue;
        drawOverlaySprite2D(frame, frame->scale, Pointi(x, 254));
    }
}

//----- mm6: the addScreenOverlay sites in the CastSpell dispatch 0x422C93 --
int mm6SpellCastFxOverlayId(int mm6SpellId) {
    // Native MM6 spell id -> one-shot portrait cast-fx overlay id. Buff/heal/utility spells only; attack
    // spells cast no portrait fx (MM6.EXE has no add site for them). A few spells also spawn a persistent
    // buff fx (overlays 10000-10014) which is left to the y=254 party-buff status row / tracked as residue.
    switch (mm6SpellId) {
        case 3:  return 1020;
        case 5:  return 1040;
        case 12: return 2000;
        case 14: return 2020;
        case 16: return 2040;
        case 17: return 2050;
        case 19: return 2070;
        case 21: return 2090;
        case 23: return 3000;
        case 25: return 3020;
        case 27: return 3040;
        case 36: return 4020;
        case 38: return 4040;
        case 40: return 2050;
        case 46: return 5010;
        case 47: return 5020;
        case 48: return 5030;
        case 49: return 5040;
        case 51: return 5060;
        case 53: return 5080;
        case 54: return 5090;
        case 55: return 5100;
        case 56: return 6000;
        case 57: return 6010;
        case 59: return 6030;
        case 60: return 6040;
        case 64: return 6080;
        case 67: return 7000;
        case 68: return 7010;
        case 69: return 7020;
        case 71: return 7040;
        case 72: return 7050;
        case 73: return 7060;
        case 74: return 7070;
        case 75: return 7080; // MM6.EXE also spawns 6030 over char 0 here; see spawnMm6CastFx.
        case 77: return 7100;
        case 83: return 8050;
        case 85: return 8070;
        case 88: return 8100;
        case 94: return 9050;
        case 96: return 9070;
        default: return 0;
    }
}

//----- (00458D97) --------------------------------------------------------
void OverlayList::InitializeSprites() {
    for (size_t i = 0; i < pOverlays.size(); ++i)
        pSpriteFrameTable->InitializeSprite(pOverlays[i].uSpriteFramesetID);
}

//----- (0045855F) --------------------------------------------------------
void ActiveOverlay::Reset() {
    this->target = 0;
    this->indexToOverlayList = 0;
    this->spriteFrameTime = 0;
    this->animLength = 0;
    this->screenSpaceX = 0;
    this->screenSpaceY = 0;
    this->pid = Pid();
    this->flags = 0;
    this->fpDamageMod = 65536;
}

//----- (004584B8) --------------------------------------------------------
ActiveOverlay::ActiveOverlay() { this->Reset(); }
