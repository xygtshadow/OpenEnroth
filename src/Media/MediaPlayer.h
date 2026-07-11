#pragma once

#include <string>
#include <memory>

#include "Library/Vid/VidReader.h"

#include "Media/Movie.h"

// MOVIE_3DOLogo  "3dologo"
// MOVIE_NWCLogo  "new world logo"
// MOVIE_JVC      "jvc"
// MOVIE_Intro    "Intro"
// MOVIE_Emerald  "Intro Post"
// MOVIE_Death    "losegame"
// MOVIE_Outro    "end_seq1"

class VideoList;
class FFmpegLogProxy;

class MPlayer {
 public:
    MPlayer();
    virtual ~MPlayer();

    void Initialize();
    void Unload();

    // TODO(Gerark) Remove this method once we move all the Video to be played in the Fsm
    void PlayFullscreenMovie(std::string_view pMovieName);

    void OpenHouseMovie(std::string_view pMovieName, bool bLoop);
    void HouseMovieLoop();

    /**
     * Demotes the currently playing house movie to a one-shot: it finishes its current pass and
     * unloads instead of looping. MM6.EXE clears the movie loop flag this way when `MoveNPC` runs
     * inside Archibald's library (0x43ce99), so the room clip plays out and the movie-end pass
     * advances the house (see `mm6HouseMovieEndChain`).
     */
    void stopHouseMovieLooping() {
        _houseMovieLooping = false;
    }

    /**
     * @return  Whether the house movie has finished: never loaded, failed to load, or a one-shot
     *          that played out. Headless (`debug.NoVideo`) house movies never actually play, so a
     *          one-shot counts as over as soon as it's opened.
     */
    bool isHouseMovieOver() const;

    std::string_view currentHouseMovieName() const {
        return sInHouseMovie;
    }

    bool IsMoviePlaying() const;
    bool StopMovie();

    std::unique_ptr<IMovie> loadFullScreenMovie(std::string_view pFilename);

 protected:
    std::unique_ptr<FFmpegLogProxy> logProxy;
    VidReader might_list;
    VidReader magic_list;
    std::string sInHouseMovie;
    bool _houseMovieLooping = true;

    Blob LoadMovie(std::string_view video_name);
};

extern MPlayer *pMediaPlayer;
extern PMovie pMovie_Track;
