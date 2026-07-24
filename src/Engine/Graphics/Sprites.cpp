#include "Engine/Graphics/Sprites.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>

#include "Application/Paths/GameVersion.h"

#include "Engine/Engine.h"
#include "Engine/OurMath.h"
#include "Engine/Objects/DecorationList.h"
#include "Engine/Graphics/PaletteManager.h"
#include "Engine/Graphics/Image.h"
#include "Engine/Resources/LodSpriteCache.h"
#include "Engine/Seasons.h"

#include "Library/Logger/Logger.h"
#include "Library/LodFormats/LodFormats.h"

#include "Utility/String/Ascii.h"

SpriteFrameTable *pSpriteFrameTable;

void Sprite::Release() {
    delete this->sprite_header;
    this->sprite_header = nullptr;
    this->texture->release();
    this->texture = nullptr;
    this->pName = "null";
}

//----- (0044D4F6) --------------------------------------------------------
void SpriteFrameTable::ResetLoadedFlags() {
    for (SpriteFrame &spriteFrame : pSpriteSFrames)
        spriteFrame.flags &= ~SPRITE_FRAME_LOADED;
}

// Loads one directional frame of an object/overlay sprite, tolerating absence for MM6.
//
// MM6 stores fewer directional frames for some objects and projectiles than MM7's 8-octant model
// expects (e.g. arrow sprites `arra`/`frara` have only frames 0 and 4), so the per-octant name the
// engine builds can be absent from the MM6 sprites LOD. Warn and leave the frame empty for MM6 instead
// of asserting, so engine bring-up can proceed; keep the strict assert for MM7 to catch real regressions.
static Sprite *loadSpriteFrame(const std::string &spriteName) {
    Sprite *sprite = pSprites_LOD->loadSprite(spriteName);
    if (!sprite && engine->gameVersion() == GAME_VERSION_MM6)
        logger->warning("Sprite {} not loaded!", spriteName);
    else
        assert(sprite);
    return sprite;
}

//----- (0044D513) --------------------------------------------------------
void SpriteFrameTable::InitializeSprite(signed int uSpriteID) {
    std::string spriteName;

    if (uSpriteID <= pSpriteSFrames.size()) {
        if (uSpriteID >= 0) {
            unsigned iter_uSpriteID = uSpriteID;
            //if (iter_uSpriteID == 603) assert(false);

            SpriteFrameFlags uFlags = pSpriteSFrames[iter_uSpriteID].flags;

            if (!(uFlags & SPRITE_FRAME_LOADED)) {
                pSpriteSFrames[iter_uSpriteID].flags |= SPRITE_FRAME_LOADED;

                while (1) {
                    if (ascii::noCaseEquals(pSpriteSFrames[iter_uSpriteID].textureName, "null")) {
                        // Both games use the literal texture name "null" to mean "this frame draws nothing".
                        // It's what the death frame of every monster that leaves no corpse carries, and what
                        // sprite-less spell frames carry - and neither game's sprites.lod holds a sprite by
                        // that name. Leave the frame empty; looking it up could only fail.
                        pSpriteSFrames[iter_uSpriteID].sprites.fill(nullptr);
                    } else if (uFlags & SPRITE_FRAME_IMAGE1) {
                        Sprite *sprite = pSprites_LOD->loadSprite(pSpriteSFrames[iter_uSpriteID].textureName);
                        if (sprite == nullptr)
                            logger->warning("Sprite {} not loaded!", pSpriteSFrames[iter_uSpriteID].textureName);
                        for (unsigned i = 0; i < 8; ++i)
                            pSpriteSFrames[iter_uSpriteID].sprites[i] = sprite;
                    } else if (uFlags & SPRITE_FRAME_IMAGES3) {
                        for (unsigned i = 0; i < 8; ++i) {
                            switch (i) {
                                case 3:
                                case 4:
                                case 5:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "4";
                                    break;
                                case 2:
                                case 6:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "2";
                                    break;
                                case 0:
                                case 1:
                                case 7:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "0";
                                    break;
                            }
                            Sprite *sprite = loadSpriteFrame(spriteName);
                            pSpriteSFrames[iter_uSpriteID].sprites[i] = sprite;
                        }

                    } else if (uFlags & SPRITE_FRAME_FIDGET) {
                        for (unsigned i = 0; i < 8; ++i) {
                            switch (i) {
                                case 0:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "0";
                                    break;
                                case 4:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName;
                                    spriteName.erase(spriteName.size() - 3, 3);
                                    spriteName = spriteName + "stA4";
                                    break;
                                case 3:
                                case 5:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName;
                                    spriteName.erase(spriteName.size() - 3, 3);
                                    spriteName = spriteName + "stA3";
                                    break;
                                case 2:
                                case 6:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "2";
                                    break;
                                case 1:
                                case 7:
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "1";
                                    break;
                            }
                            Sprite *sprite = loadSpriteFrame(spriteName);
                            pSpriteSFrames[iter_uSpriteID].sprites[i] = sprite;
                        }
                    } else {
                        for (unsigned i = 0; i < 8; ++i) {
                            if (pSpriteSFrames[iter_uSpriteID].flags & mirrorFlagForOctant(i)) {
                                switch (i) {
                                    case 1:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "7";
                                        break;
                                    case 2:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "6";
                                        break;
                                    case 3:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "5";
                                        break;
                                    case 4:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "4";
                                        break;
                                    case 5:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "3";
                                        break;
                                    case 6:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "2";
                                        break;
                                    case 7:
                                        spriteName = pSpriteSFrames[iter_uSpriteID].textureName + "1";
                                        break;
                                }
                            } else {
                                // Names of 7+ chars usually already have the octant code attached, but MM6 also has
                                // 7-char monster sprite names without one (e.g. "pmansta" stored as "pmansta0".."pmansta4"
                                // in sprites.lod), so probe with the code appended and fall back to the raw name.
                                spriteName = fmt::format("{}{}", pSpriteSFrames[iter_uSpriteID].textureName, i);
                                if (pSpriteSFrames[iter_uSpriteID].textureName.size() >= 7 && !pSprites_LOD->loadSprite(spriteName))
                                    spriteName = pSpriteSFrames[iter_uSpriteID].textureName;
                            }

                            Sprite *sprite = loadSpriteFrame(spriteName);
                            pSpriteSFrames[iter_uSpriteID].sprites[i] = sprite;
                        }
                    }

                    // MM6's dsft.bin fills per-frame palette ids just like MM7's, and they are the
                    // ONLY palette source that can recolor monster tiers - the A/B/C variants of a
                    // monster share the same textures and differ solely in the frame-table palette
                    // (e.g. bat walk frames carry pal159/160/161 over the same bhwlk* sprites), so
                    // the sprite headers' own palette ids can't stand in for them. The few frames
                    // left at 0 are "null"-texture frames and sprites absent from MM6's own
                    // sprites.lod - nothing loadable draws unpaletted.
                    if (!(pSpriteSFrames[iter_uSpriteID].flags & SPRITE_FRAME_HAS_MORE)) {
                        return;
                    }
                    ++iter_uSpriteID;
                }
            }
        }
    }
}

//----- (0044D813) --------------------------------------------------------
int SpriteFrameTable::FastFindSprite(std::string_view pSpriteName) {
    auto cmp = [this] (uint16_t index, std::string_view name) {
        return ascii::noCaseLess(pSpriteSFrames[index].spriteName, name);
    };

    auto pos = std::lower_bound(pSpriteEFrames.begin(), pSpriteEFrames.end(), pSpriteName, cmp);
    if (pos == pSpriteEFrames.end())
        return 0;

    return ascii::noCaseEquals(pSpriteSFrames[*pos].spriteName, pSpriteName) ? *pos : 0;
}

//----- (0044D8D0) --------------------------------------------------------
SpriteFrame *SpriteFrameTable::GetFrame(int uSpriteID, Duration uTime) {
    SpriteFrame *first = &pSpriteSFrames[uSpriteID];
    SpriteFrame *v4 = first;
    if (!(v4->flags & SPRITE_FRAME_HAS_MORE) || !v4->animationLength)
        return v4;

    // uAnimLength / uAnimTime = actual number of frames in sprite
    for (Duration t = uTime % v4->animationLength; t >= v4->frameLength; ++v4)
        t -= v4->frameLength;

    // Some framesets are missing trailing frames (MM7's dragon breath is short two), and MM6 even
    // has framesets with no loadable sprites at all (light05/light07 animate over proje23* textures
    // absent from MM6's sprites.lod). Fall back to the closest earlier frame of the SAME frameset -
    // walking past its start would return a frame of whatever unrelated animation precedes it, or
    // read out of bounds. If the whole frameset is empty, return its first frame; the draw paths
    // null-check sprites[].
    while (v4 > first && v4->sprites[0] == nullptr)
        --v4;

    return v4;
}

//----- (0044D91F) --------------------------------------------------------
SpriteFrame *SpriteFrameTable::GetFrameReversed(int uSpriteID, Duration time) {
    SpriteFrame *sprite = &pSpriteSFrames[uSpriteID];
    if (!(sprite->flags & SPRITE_FRAME_HAS_MORE) || !sprite->animationLength)
        return sprite;

    for (Duration t = sprite->animationLength - time % sprite->animationLength; t >= sprite->frameLength; ++sprite)
        t -= sprite->frameLength;

    return sprite;
}

SpriteFrame *LevelDecorationChangeSeason(const DecorationDesc *desc, Duration t, int month) {
    int spriteId = spriteIdForSeason(desc->uSpriteID, month);
    if (spriteId != desc->uSpriteID)
        pSpriteFrameTable->InitializeSprite(spriteId);
    return pSpriteFrameTable->GetFrame(spriteId, t);
}
