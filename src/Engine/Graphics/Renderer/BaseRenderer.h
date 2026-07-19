#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Renderer/UiScaleTransform.h"

#include "Renderer.h"

class BaseRenderer : public Renderer {
 public:
    inline BaseRenderer(
        std::shared_ptr<GameConfig> config,
        DecalBuilder *decal_builder,
        SpellFxRenderer *spellfx,
        std::shared_ptr<ParticleEngine> particle_engine,
        Vis *vis
    ) : Renderer(config, decal_builder, spellfx, particle_engine, vis) {
    }

    virtual bool Initialize() override;

    virtual void TransformBillboards() override;
    virtual bool AddBillboardIfVisible(Sprite* spr, int palette, const Vec3f& pos, const Vec2f& scale, BillboardFlags flags, Pid id, int sector = 0) override;

    virtual void DrawSpriteObjects() override;
    virtual void PrepareDecorationsRenderList_ODM() override;
    virtual void MakeParticleBillboardAndPush(const Particle& p) override;
    virtual float GetGamma() override;

    virtual void BillboardSphereSpellFX(SpellFX_Billboard *a1, Color diffuse) override;
    virtual void DrawMonsterPortrait(const Recti &rc, SpriteFrame *Portrait_Sprite, int Y_Offset) override;
    virtual void DrawSpecialEffectsQuad(GraphicsImage *texture, int palette) override;
    virtual void DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene() override;

    virtual std::vector<Actor*> getActorsInViewport(int pDepth) override;

    bool Reinitialize(bool firstInit) override;

    virtual Sizei GetRenderDimensions() override;
    virtual Sizei GetPresentDimensions() override;
    virtual Pointi MapToRender(Pointi position) override;
    virtual Pointi MapToPresent(Pointi position) override;
    virtual RgbaImage MakeVirtualScreenshot() override;

    /**
     * @return  Whether the renderer runs in native-resolution mode (`render_filter` == 0). In this
     *          mode `outputRender` holds the virtual 640x480 UI size rather than the window size,
     *          and mouse coordinates map between window and virtual space through the UI scale
     *          transform instead of the framebuffer letterbox math.
     *
     *          The mode is snapshotted in `updateRenderDimensions()` together with `outputRender`
     *          and `_uiTransform`, so a config change only takes effect on the next
     *          `Reinitialize()` and the three can never disagree mid-frame.
     */
    [[nodiscard]] bool isNativeResMode() const;

 protected:
    unsigned int NextBillboardIndex();
    void SortBillboards();
    void TransformBillboard(const RenderBillboard *pBillboard, int parent);

 protected:
    Sizei outputRender = {0, 0};
    Sizei outputPresent = {0, 0};
    bool _nativeResMode = false;
    UiScaleTransform _uiTransform;

 private:
    void updateRenderDimensions();
};
