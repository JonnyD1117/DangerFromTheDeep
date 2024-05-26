/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "music.hpp"

#include "error.hpp"
#include "log.hpp"
#include "random_generator.hpp"
#include "system_interface.hpp"
#include "xml.hpp"

#include <SDL.h>
#include <SDL_mixer.h>
#include <cmath>
#include <fstream>
#include <sstream>

using namespace std;

#define SOUND_SPEC_FILENAME "sound-categories.data"
#define SFX_CHANNELS_TOTAL  8
#define SFX_CHANNEL_MACHINE 0

bool music::use_music = true;

music::music(bool useit, unsigned sample_rate_)
    : sample_rate(sample_rate_)
{
    use_music = useit;
    if (use_music) {
        player = std::make_unique<::thread>("music___", [this]() { player_code(); });
    }
}

music::~music()
{
    player->request_abort();
    command_queue.wakeup_receiver();
}

void music::start_play_track(unsigned nr, unsigned fadeintime)
{
    if (nr < playlist.size()) {
        current_track = nr;
        int err       = -1;
        if (fadeintime > 0) {
            err = Mix_FadeInMusic(musiclist[current_track].get(), 1, fadeintime);
        } else {
            err = Mix_PlayMusic(musiclist[current_track].get(), 1);
        }
        if (err < 0) {
            THROW(error, "music playing failed.");
        }
        stopped = false;
    }
}

void music::callback_track_finished()
{
    // note! we CAN NOT protect the instance variable with a mutex here, because
    // we can't make a static variable for this, because we need to initialize
    // SDL before using SDL_mutexes...
    instance().track_finished();
}

void music::player_code()
{
    if (!use_music) {
        return;
    }

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        log_warning("Unable to to initialize SDL Audio: " << SDL_GetError());
        use_music = false;
        return;
    }

    int audio_rate        = int(sample_rate);
    uint16_t audio_format = AUDIO_S16SYS;
    int audio_channels    = 2;
    int audio_buffers     = 4096;

    log_info("Audio initialize " << audio_channels << " chnls " << audio_rate << "hz");

    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0) {
        log_warning("Unable to initialize audio: " << Mix_GetError());
        use_music = false;
        return;
    }

    Mix_HookMusicFinished(callback_track_finished);

    // allocate channels
    if (Mix_AllocateChannels(SFX_CHANNELS_TOTAL) < int(SFX_CHANNELS_TOTAL)) {
        THROW(error, "could not allocate enough channels");
    }

    // reserve sfx channels for environmental noises (engine, sonar)
    if (Mix_ReserveChannels(nr_reserved_channels) < int(nr_reserved_channels)) {
        THROW(error, "could not reserve enough channels");
    }

#if 0
    // load sfx files
    // fixme: later implement a cache!
    try {
        xml_doc spec(sound_dir + SOUND_SPEC_FILENAME);
        spec.load();
        xml_elem sf = spec.first_child();
        xml_elem mc = sf.child("machines");
        for (auto m : mc) {
            string mn = m.get_name();
            auto& v = sfx_machines[mn];
            // the levels/level attributes are ignored yet...fixme
            for (auto e : m) {
                if (e.get_name() != "throttle")
                    THROW(xml_error, "illegal child of \"machine\"", e.doc_name());
                string fn = e.attr("file");
                mix_chunk_ptr chk(Mix_LoadWAV((sound_dir + fn).c_str()), Mix_FreeChunk);
                if (chk == nullptr) {
                    THROW(file_read_error, sound_dir + fn);
                }
                v.push_back(std::move(chk));
            }
        }
        xml_elem ef = sf.child("effects");
        for (auto e : ef) {
            string en = e.get_name();
            string fn = e.attr("file");
            auto& v = sfx_events[en];
            mix_chunk_ptr chk(Mix_LoadWAV((sound_dir + fn).c_str()), Mix_FreeChunk);
            if (chk == nullptr) {
                THROW(file_read_error, sound_dir + fn);
            }
            v.push_back(std::move(chk));
        }
    }
    catch (...) {
        destructor();
        throw;
    }
#endif

    while (!player->abort_requested()) {
        command_queue.process_messages();
    }

    if (!use_music) {
        return;
    }

    // halt
    if (Mix_PlayingMusic() != 0) {
        Mix_HaltMusic();
        stopped = true;
    }

    // clear playlist
    musiclist.clear();

    sfx_events.clear();
    sfx_machines.clear();

    Mix_CloseAudio();
}

// -------------------- commands --------------------

auto music::append_track(const std::string& filename) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_append_track(*this, filename)));
}

auto music::set_playback_mode(playback_mode pbm) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_set_playback_mode(*this, pbm)));
}

auto music::play(unsigned fadein) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_play(*this, fadein)));
}

auto music::stop(unsigned fadeout) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_stop(*this, fadeout)));
}

auto music::pause() -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_pause(*this)));
}

auto music::resume() -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_resume(*this)));
}

auto music::set_music_position(float pos) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_set_music_position(*this, pos)));
}

auto music::play_track(unsigned nr, unsigned fadeouttime, unsigned fadeintime) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_play_track(*this, nr, fadeouttime, fadeintime)));
}

auto music::track_finished() -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_track_finished(*this)), false);
}

auto music::get_playlist() -> std::vector<std::string>
{
    std::vector<std::string> myplaylist;
    command_queue.send(std::unique_ptr<message>(new command_get_playlist(*this, myplaylist)));
    return myplaylist;
}

auto music::get_current_track() -> unsigned
{
    unsigned track = 0;
    command_queue.send(std::unique_ptr<message>(new command_get_current_track(*this, track)));
    return track;
}

auto music::is_playing() -> bool
{
    bool isply = false;
    command_queue.send(std::unique_ptr<message>(new command_is_playing(*this, isply)));
    return isply;
}

auto music::play_sfx(const std::string& category, const vector3& listener, angle listener_dir, const vector3& noise_pos)
    -> bool
{
    return command_queue.send(
        std::unique_ptr<message>(new command_play_sfx(*this, category, listener, listener_dir, noise_pos)));
}

auto music::play_sfx_machine(const std::string& name, unsigned throttle) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_play_sfx_machine(*this, name, throttle)));
}

auto music::pause_sfx(bool on) -> bool
{
    return command_queue.send(std::unique_ptr<message>(new command_pause_sfx(*this, on)));
}

// -------------------- command exec --------------------

void music::exec_append_track(const std::string& filename)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }

    mix_music_ptr mmp(Mix_LoadMUS((sound_dir + filename).c_str()), Mix_FreeMusic);

    if (mmp.get() == nullptr) {
        log_warning("Failed to load track: " << sound_dir + filename << ", " << Mix_GetError());
        THROW(file_read_error, filename);
    }
    playlist.push_back(filename);
    musiclist.push_back(std::move(mmp));
}

void music::exec_set_playback_mode(playback_mode pbm_)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    pbm = pbm_;
}

void music::exec_play(unsigned fadein)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    if (Mix_PlayingMusic() == 0) {
        start_play_track(current_track, fadein);
    } else {
        THROW(error, "music still playing, can't execute play()");
    }
}

void music::exec_stop(unsigned fadeout)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    if (Mix_PausedMusic() != 0) {
        Mix_ResumeMusic();
    }
    if (Mix_PlayingMusic() != 0) {
        stopped = true;
        if (fadeout > 0) {
            Mix_FadeOutMusic(int(fadeout));
        } else {
            Mix_HaltMusic();
        }
    } else {
        THROW(error, "music not playing, can't execute stop()");
    }
}

void music::exec_pause()
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    if ((Mix_PlayingMusic() != 0) && (Mix_PausedMusic() == 0)) {
        Mix_PauseMusic();
    }
}

void music::exec_resume()
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    Mix_ResumeMusic();
}

void music::exec_set_music_position(float pos)
{
    Mix_RewindMusic();
    if (Mix_SetMusicPosition(pos) == -1) {
        THROW(error, "music set position failed");
    }
}

void music::exec_play_track(unsigned nr, unsigned fadeouttime, unsigned fadeintime)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    try {
        exec_stop(fadeouttime);
    }
    catch (exception&) {
        // couldn't stop, so music was already stopped?
        current_track = nr;
        exec_play(fadeintime);
        return;
    }
    usersel_next_track = int(nr);
    usersel_fadein     = fadeintime;
}

void music::exec_track_finished()
{
    if (usersel_next_track >= 0) {
        current_track = unsigned(usersel_next_track);
        exec_play(usersel_fadein);
        usersel_next_track = -1;
        usersel_fadein     = 0;
        return;
    }
    if (!stopped) {
        switch (pbm) {
            case playback_mode::loop_list:
                ++current_track;
                if (current_track >= playlist.size()) {
                    current_track = 0;
                }
                break;
            case playback_mode::loop_track:
                break;
            case playback_mode::shuffle_track:
                current_track = rndgen.get(unsigned(playlist.size()));
                break;
        }
        start_play_track(current_track, 0);
    }
}

void music::exec_get_playlist(std::vector<std::string>& playlist_)
{
    playlist_ = playlist;
}

void music::exec_get_current_track(unsigned& track) const
{
    track = current_track;
}

void music::exec_is_playing(bool& /*isply*/)
{
    if ((Mix_PlayingMusic() == 0) || (Mix_PausedMusic() != 0)) {
        THROW(error, "music not playing");
    }
}

void music::exec_play_sfx(
    const std::string& category,
    const vector3& listener,
    angle listener_dir,
    const vector3& noise_pos)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    auto it = sfx_events.find(category);
    if (it == sfx_events.end()) {
        throw invalid_argument(string("unknown category for sfx: ") + category);
    }
    // chose random sound of category
    auto snr  = rndgen.get(unsigned(it->second.size()));
    auto& chk = it->second[snr];

    double distanceFromPlayer = listener.distance(noise_pos);
    // calculate max hearing distance (meters)
    double hearing_range     = 0.0;
    double hearing_increment = 0.0;

    // fixme: this depends on wether the noise is inside the sub or not!

    // audible range varies depending on whether player/noise source is
    // submerged or not
    //
    // 255 == max dist value for Mix_SetPosition()
    if (listener.z < 0) {
        // player submerged
        if (noise_pos.z < 0) {
            hearing_range     = 20000.0;
            hearing_increment = 78.43; // hearing_range / 255;
        } else {
            hearing_range     = 30000.0;
            hearing_increment = 117.64; // hearing_range / 255;
        }
    } else {
        // player on the surface
        if (noise_pos.z < 0) {
            hearing_range     = 5000.0;
            hearing_increment = 19.60; // hearing_range / 255;
        } else {
            hearing_range     = 10000.0;
            hearing_increment = 39.21; // hearing_range / 255;
        }
    }

    // is sound within audible range
    if (distanceFromPlayer <= hearing_range) {
        angle bearingFromPlayer(noise_pos.xy() - listener.xy());
        bearingFromPlayer = bearingFromPlayer + listener_dir;

        // dist for Mix_SetPosition() is calculated as a value between 0 and 255
        // with 0 being the loudest (closest)
        int dist = static_cast<int>(distanceFromPlayer / hearing_increment);

        int channel_num = Mix_PlayChannel(-1, chk.get(), 0);
        if (channel_num < 0) {
            THROW(error, "unable to play sfx"); // Mix_GetError() here...
        }
        if (Mix_SetPosition(channel_num, static_cast<short>(bearingFromPlayer.value()), dist) == 0) {
            THROW(error, "Mix_SetPosition() failed");
        }
    }
}

void music::exec_play_sfx_machine(const std::string& name, unsigned throttle)
{
    if (!use_music) {
        throw std::invalid_argument("no music support");
    }
    auto it = sfx_machines.find(name);
    if (it == sfx_machines.end()) {
        throw invalid_argument(string("unknown machine name: ") + name);
    }
    auto nrthr   = unsigned(it->second.size());
    unsigned thr = throttle * (nrthr + 1) / 100;
    if (thr == 0) {
        // stop machine
        if (Mix_Playing(SFX_CHANNEL_MACHINE) != 0) {
            Mix_HaltChannel(SFX_CHANNEL_MACHINE);
        }
        current_machine_sfx = nullptr;
        return;
    }
    thr = std::min(thr - 1, nrthr - 1);

    auto& chk = it->second[thr];
    if (chk.get() == current_machine_sfx) {
        // nothing to do, already playing
        return;
    }
    // stop old sfx
    if (Mix_Playing(SFX_CHANNEL_MACHINE) != 0) {
        Mix_HaltChannel(SFX_CHANNEL_MACHINE);
    }
    current_machine_sfx = nullptr;
    if (Mix_PlayChannel(SFX_CHANNEL_MACHINE, chk.get(), -1) < 0) {
        THROW(error, "can't play channel");
    }
    current_machine_sfx = chk.get();
}

void music::exec_pause_sfx(bool on)
{
    if (on) {
        Mix_Pause(-1);
    } else {
        Mix_Resume(-1);
    }
}
