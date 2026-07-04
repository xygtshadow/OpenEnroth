#include "PortraitFrameTable.h"

#include "Engine/Random/Random.h"

PortraitFrameTable *pPortraitFrameTable = nullptr;

//----- (00494AED) --------------------------------------------------------
int PortraitFrameTable::animationId(PortraitId portrait) {
    for (size_t i = 0; i < this->pFrames.size(); i++)
        if (this->pFrames[i].portrait == portrait)
            return i;
    return 0;
}

Duration PortraitFrameTable::animationDuration(PortraitId portrait) {
    int index = animationId(portrait);
    if (index == 0)
        return 0_ticks;
    return this->pFrames[index].animationLength;
}

//----- (00494B10) --------------------------------------------------------
int PortraitFrameTable::animationFrameIndex(int animationId, Duration frameTime) {
    if (this->pFrames[animationId].flags & FRAME_HAS_MORE && this->pFrames[animationId].animationLength) {
        // Processing animated character expressions - e.g., PORTRAIT_YES & PORTRAIT_NO.
        Duration time = frameTime % this->pFrames[animationId].animationLength;

        while (true) {
            Duration frameTime = this->pFrames[animationId].frameLength;
            if (time < frameTime)
                break;
            // MM6's dpft.bin has expressions whose declared animation length exceeds the sum of
            // their frame lengths - stay on the last frame instead of running into the frames of
            // the next portrait. MM7 data never takes this branch.
            if (animationId + 1 >= static_cast<int>(this->pFrames.size()) || this->pFrames[animationId + 1].portrait != PORTRAIT_INVALID)
                break;
            time -= frameTime;
            ++animationId;
        }
    }
    return pFrames[animationId].textureIndex;
}
