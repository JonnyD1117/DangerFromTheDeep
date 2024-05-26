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

// main program
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "cfg.hpp"
#include "credits.hpp"
#include "datadirs.hpp"
#include "date.hpp"
#include "faulthandler.hpp"
#include "filehelper.hpp"
#include "game.hpp"
#include "game_editor.hpp"
#include "global_data.hpp"
#include "highscorelist.hpp"
#include "image.hpp"
#include "keys.hpp"
#include "log.hpp"
#include "model.hpp"
#include "music.hpp"
#include "mymain.cpp"
#include "system_interface.hpp"
#include "texts.hpp"
#include "texture.hpp"
#include "user_interface.hpp"
#include "vector3.hpp"
#include "widget.hpp"

#include <ctime>
#include <glu.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#ifndef WIN32
// broken under windows... TODO
#include "dftdtester/tests.hpp"
#endif

/* fixme: 2006/12/02
   How to decide about removal:
   - We have to limit RAM/Video-Ram usage:
   - Each image has a cost, that is width*height*bpp.
   - Each unref'd image has an age, a timestamp taken when unref'd lastly.
   - Compute total cost with some formula
   - Remove image with most cost, if total ram usage of unref'd images is above
   limit. So we have:
   - global limit for unref'd images, good starting point 4MB.
   - A formula to compute cost, c(s, t)  where s is size in bytes and t is age
   in milliseconds. We should have that few images to just keep a simple list
   for management. What about the cost formula?
   - Size matters
   - time is only to get rid of old, but small images.
   Try this formula:
   cost(s, t) = s + t^2/32.
   So after ca. 10 seconds the cost of an 1x1 image is as high as an fullscreen
   3MB image. The cache should have a "clean_up" function to instantly remove
   all cleaned up images.
*/

highscorelist hsl_mission, hsl_career;
#define HSL_MISSION_NAME "mission.hsc"
#define HSL_CAREER_NAME  "career.hsc"

// a dirty hack
void menu_notimplemented()
{
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(110));
    wm->add_entry(texts::get(20), std::make_unique<widget_caller_button<>>(0, 0, 0, 0, "", [&w]() { w.close(0); }));

    wm->align(0, 0);
    widget::run(w, 0, false);
}

//
// save game directory and helper functions
//
std::string savegamedirectory =
#ifdef WIN32
    "./save/";
#else
    std::string(getenv("HOME")) + "/.dangerdeep/";
#endif

auto get_savegame_name_for(const std::string& descr, std::map<std::string, std::string>& savegames) -> std::string
{
    auto num = 1;
    for (auto& savegame : savegames) {
        if (savegame.second == descr) {
            return savegamedirectory + savegame.first;
        }
        auto num2 = atoi((savegame.first.substr(5, 4)).c_str());
        if (num2 >= num) {
            num = num2 + 1;
        }
    }
    char tmp[20];
    sprintf(tmp, "save_%04u.dftd", num);
    return savegamedirectory + tmp;
}

auto is_savegame_name(const std::string& s) -> bool
{
    if (s.length() != 14) {
        return false;
    }
    if (s.substr(0, 5) != "save_") {
        return false;
    }
    if (s.substr(9, 7) != ".dftd") {
        return false;
    }
    for (int i = 5; i < 9; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }
    return true;
}

//
// loading, saving games
//
class loadsavequit_dialogue : public widget
{
    widget_edit::ptr gamename;
    widget_list::ptr gamelist;
    widget_button::ptr btnload, btnsave, btndel, btnquit, btncancel;
    const game* mygame;
    bool gamesaved{false};
    std::map<std::string, std::string> savegames;
    std::string gamefilename_to_load;
    void load();
    void save();
    void erase();
    void quit();
    void cancel()
    {
        close(0);
    } // return 0 for cancel/return, 1 for quit (if saving is enabled), 2 for
      // loaded
    void update_list();

  public:
    auto get_gamefilename_to_load() const { return gamefilename_to_load; }
    auto get_gamename() const { return gamename; }
    explicit loadsavequit_dialogue(const game* g); // give 0 to disable saving
};

loadsavequit_dialogue::loadsavequit_dialogue(const game* g)
    : widget(0, 0, 1024, 768, texts::get(177), "depthcharge.jpg")
    , mygame(g)
{
    add_child<widget_text>(40, 40, 0, 0, texts::get(178));
    gamename = add_child<widget_edit>(200, 40, 684, 40, "");

    auto wm = add_child<widget_menu>(40, 700, 180, 40, "", true);

    btnload = wm->add_entry(
        texts::get(118),
        std::make_unique<widget_caller_button<loadsavequit_dialogue&>>([](auto& ld) { ld.load(); }, *this));

    if (mygame != nullptr) {
        btnsave = wm->add_entry(
            texts::get(119),
            std::make_unique<widget_caller_button<loadsavequit_dialogue&>>([](auto& ld) { ld.save(); }, *this));
    }

    btndel = wm->add_entry(
        texts::get(179),
        std::make_unique<widget_caller_button<loadsavequit_dialogue&>>([](auto& ld) { ld.erase(); }, *this));

    if (mygame != nullptr) {
        btnquit = wm->add_entry(
            texts::get(120),
            std::make_unique<widget_caller_button<loadsavequit_dialogue&>>([](auto& ld) { ld.quit(); }, *this));
    }

    btncancel = wm->add_entry(
        texts::get(mygame != nullptr ? 121 : 20),
        std::make_unique<widget_caller_button<loadsavequit_dialogue&>>([](auto& ld) { ld.cancel(); }, *this));

    wm->adjust_buttons(944);

    struct lsqlist : public widget_list
    {
        void on_sel_change() override
        {
            auto p = get_parent();
            if (p) {
                static_cast<loadsavequit_dialogue&>(*p).get_gamename()->set_text(get_selected_entry());
            }
        }
        lsqlist(int x, int y, int w, int h)
            : widget_list(x, y, w, h)
        {
        }
        ~lsqlist() override = default;
    };

    gamelist = add_child<lsqlist>(40, 100, 944, 580);

    update_list();

    gamename->set_text(gamelist->get_selected_entry());
}

void loadsavequit_dialogue::load()
{
    gamefilename_to_load = get_savegame_name_for(gamename->get_text(), savegames);

    // fixme: ask: replace this game?
    auto w(create_dialogue_ok(texts::get(185), texts::get(180) + gamename->get_text() + texts::get(181)));

    widget::run(*w);
    close(2); // load and close
}

void loadsavequit_dialogue::save()
{
    std::string fn = get_savegame_name_for(gamename->get_text(), savegames);
    FILE* f        = fopen(fn.c_str(), "rb");
    if (f != nullptr) {
        fclose(f);
        auto w(create_dialogue_ok_cancel(texts::get(182), texts::get_replace(183, gamename->get_text())));
        int ok = widget::run(*w);
        w.reset();
        if (ok == 0) {
            return;
        }
    }

    gamesaved = true;
    mygame->save(fn, gamename->get_text());

    auto w(create_dialogue_ok(texts::get(186), texts::get(180) + gamename->get_text() + texts::get(187)));

    widget::run(*w);
    update_list();
}

void loadsavequit_dialogue::erase()
{
    auto w(create_dialogue_ok_cancel(texts::get(182), texts::get(188) + gamename->get_text() + texts::get(189)));

    int ok = widget::run(*w);
    w.reset();

    if (ok != 0) {
        std::string fn = get_savegame_name_for(gamename->get_text(), savegames);
        remove(fn.c_str());
        int s = gamelist->get_selected() - 1;
        update_list();
        if (s < 0) {
            s = 0;
        }
        gamelist->set_selected(s);
        gamename->set_text(gamelist->get_selected_entry());
    }
}

void loadsavequit_dialogue::quit()
{
    if (!gamesaved) {
        auto w(create_dialogue_ok_cancel(texts::get(182), texts::get(190)));
        int q = widget::run(*w);
        if (q != 0) {
            close(1);
        }
    } else {
        close(1);
    }
}

void loadsavequit_dialogue::update_list()
{
    savegames.clear();

    // read save games in directory
    {
        directory savegamedir(savegamedirectory);
        while (true) {
            std::string e = savegamedir.read();
            if (e.empty()) {
                break;
            }
            if (is_savegame_name(e)) {
                std::string descr = game::read_description_of_savegame(savegamedirectory + e);
                savegames[e]      = descr;
            }
        }
    }

    gamelist->clear();

    unsigned sel = 0;
    for (auto& savegame : savegames) {
        gamelist->append_entry(savegame.second);
        if (savegame.second == gamename->get_text()) {
            gamelist->set_selected(sel);
        }
        ++sel;
    }

    if (savegames.empty()) {
        btnload->disable();
        btndel->disable();
    } else {
        btnload->enable();
        btndel->enable();
    }
}

//
// show hall of fame
//
void show_halloffame(const highscorelist& hsl)
{
    widget w(0, 0, 1024, 768, texts::get(197), "krupp_docks.jpg");
    w.add_child<widget>(40, 50, 944, 640, "");
    w.add_child<widget_caller_button<widget&>>(
        (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105), [](auto& w) { w.close(1); }, w);
    hsl.show(&w);
    widget::run(w, 0, false);
}

void show_halloffame_mission()
{
    show_halloffame(hsl_mission);
}
void show_halloffame_career()
{
    show_halloffame(hsl_career);
}

//
// check if a game is good enough for the high score list
//
void check_for_highscore(const game& gm)
{
    unsigned totaltons                          = 0;
    const list<game::sink_record>& sunken_ships = gm.get_sunken_ships();
    for (const auto& sunken_ship : sunken_ships) {
        totaltons += sunken_ship.tons;
    }
    highscorelist& hsl = (/* check if game is career or mission fixme */ true) ? hsl_mission : hsl_career;
    unsigned points    = totaltons /* compute points from tons etc here fixme */;

    widget w(0, 0, 1024, 768, texts::get(197), "krupp_docks.jpg");
    w.add_child<widget>(40, 50, 944, 640, "");
    w.add_child<widget_caller_button<>>(
        (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105), [&w]() { w.close(1); });

    unsigned pos = hsl.get_listpos_for(points);
    if (hsl.is_good_enough(points)) {
        std::string txt = texts::get(199);
        if (pos == 0) {
            txt += "\n\n" + texts::get(201);
        }
        w.add_child<widget_text>(400, 200, 0, 0, txt);
        widget::run(w, 0, false);
        hsl.record(points, gm.get_player_info().name);
    } else {
        w.add_child<widget_text>(400, 200, 0, 0, texts::get(198));
        widget::run(w, 0, false);
    }
    show_halloffame(hsl);
}

//
// show results after a game ended
//
void show_results_for_game(const game& gm)
{
    widget w(0, 0, 1024, 768, texts::get(124), "sunken_destroyer.jpg");
    auto wl = w.add_child<widget_list>(64, 64, 1024 - 64 - 64, 768 - 64 - 64);
    wl->set_column_width((1024 - 2 * 64) / 4);
    w.add_child<widget_caller_button<>>(
        (1024 - 128) / 2, 768 - 32 - 16, 128, 32, texts::get(105), [&w]() { w.close(1); });
    unsigned totaltons       = 0;
    const auto& sunken_ships = gm.get_sunken_ships();
    for (const auto& sunken_ship : sunken_ships) {
        std::ostringstream oss;
        oss << texts::numeric_from_date(sunken_ship.dat) << "\t" << sunken_ship.descr << "\t\t" << sunken_ship.tons
            << " BRT";
        totaltons += sunken_ship.tons;
        wl->append_entry(oss.str());
    }
    std::ostringstream os;
    os << "total: " << totaltons;
    wl->append_entry(os.str());
    widget::run(w, 0, false);
}

// main play loop
// fixme: clean this up!!!
auto game_exec(game& gm, const std::shared_ptr<user_interface>& ui) -> game::run_state
{
    // fixme: add special ui heir: playback
    // to record videos.
    // record ship positions or at least commands!
    // and camera path (bspline) etc.
    // used for credits background etc.

    unsigned frames     = 1;
    unsigned lasttime   = SYS().millisec();
    unsigned lastframes = 1;
    double fpstime      = 0;
    double totaltime    = 0;
    double measuretime  = 5; // seconds

    ui->resume_all_sound();

    // draw one initial frame
    ui->display();

    ui->request_abort(false);
    SYS().add_input_event_handler(ui);

    while (gm.get_run_state() == game::running && !ui->abort_requested()) {
        // this time_scaling is bad. hits may get computed wrong when time
        // scaling is too high. fixme
        unsigned thistime = SYS().millisec();
        if (gm.get_freezetime_start() > 0) {
            THROW(error, "freeze_time() called without unfreeze_time() call");
        }
        lasttime += gm.process_freezetime();
        unsigned time_scale = ui->time_scaling();
        double delta_time   = (thistime - lasttime) / 1000.0; // * time_scale;
        totaltime += (thistime - lasttime) / 1000.0;
        lasttime = thistime;

        // next simulation step
        if (!ui->paused()) {
            for (unsigned j = 0; j < time_scale; ++j) {
                gm.simulate(time_scale == 1 ? delta_time : (1.0 / 30.0));
                // evaluate events of game, because they are cleared
                // by next call of game::simulate and new ones are
                // generated
                const auto& events = gm.get_events();
                for (const auto& it : events) {
                    it->evaluate(*ui);
                }
            }
        }

        // fixme: make use of game::job interface, 3600/256 = 14.25 secs job
        // period
        ui->set_time(gm.get_time());
        ui->display();
        ++frames;

        // record fps
        if (totaltime - fpstime >= measuretime) {
            fpstime = totaltime;
            log_info("fps " << (frames - lastframes) / measuretime);
            lastframes = frames;
        }

        // this also fetches input events to the handlers
        SYS().finish_frame();
    }
    SYS().remove_input_event_handler(ui);

    ui->pause_all_sound();

    return gm.get_run_state(); // if player is killed, end game (1), else show
                               // menu (0)
}

//
// start and run a game, handle load/save (game menu), show results after game's
// end, delete game
//
void run_game(std::unique_ptr<game> gm)
{
    auto gametheme = std::make_unique<widget::theme>(
        "widgetelements_game.png",
        "widgeticons_game.png",
        &*font_vtremington12,
        color(182, 146, 137),
        color(240, 217, 127),
        color(64, 64, 64));

    reset_loading_screen();

    // embrace user interface generation with right theme set!
    unique_ptr<widget::theme> tmp = widget::replace_theme(std::move(gametheme));
    auto ui                       = user_interface::create(*gm);
    gametheme                     = widget::replace_theme(std::move(tmp));

    while (true) {
        tmp                   = widget::replace_theme(std::move(gametheme));
        game::run_state state = game_exec(*gm, ui);
        gametheme             = widget::replace_theme(std::move(tmp));

        // if (state == 2) break;
        // SDL_ShowCursor(SDL_ENABLE);
        if (state != game::running) {
            //			if (state == game::mission_complete)

            if (state == game::player_killed) {
                music::instance().play_track(1, 500);
                widget w(0, 0, 1024, 768, "", "killed.jpg");
                auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(103));
                wm->add_entry(texts::get(105), std::make_unique<widget_caller_button<>>([&w]() { w.close(0); }));
                wm->align(0, 0);
                widget::run(w, 0, false);
            }

            //			if (state == game::contact_lost)

            // widget* w = widget::create_dialogue_ok_cancel("Quit game?", "");
            // int q = w->run();
            // delete w;
            // if (q == 1)
            break;
        }
        music::instance().play_track(1, 500);
        loadsavequit_dialogue dlg(gm.get());
        int q = widget::run(dlg, 0, false);
        // replace game and ui if new game was loaded
        if (q == 2) {
            // fixme: ui doesn't need to get replaced, just give pointer to
            // new game to old ui, clear ui values and messages, finished...
            // this safes time to recompute map/water/sky etc.
            // this can only work if old and new game have same type
            // of player (and thus same type of ui)
            gm.reset();
            ui = nullptr;
            gm = std::make_unique<game>(dlg.get_gamefilename_to_load());
            // embrace user interface generation with right theme set!
            tmp       = widget::replace_theme(std::move(gametheme));
            ui        = user_interface::create(*gm);
            gametheme = widget::replace_theme(std::move(tmp));
        }
        // replace ui after loading!!!!
        if (q == 1) {
            music::instance().play_track(1, 500);
            break;
        }
        if (q == 0) {
            // music::instance()._fade_out(1000);
        }

        // SDL_ShowCursor(SDL_DISABLE);
    }
    show_results_for_game(*gm);
    check_for_highscore(*gm);
}

//
// start and run a game editor, handle load/save (game menu), delete game
//
void run_game_editor(std::unique_ptr<game> gm)
{
    auto gametheme = std::make_unique<widget::theme>(
        "widgetelements_game.png",
        "widgeticons_game.png",
        &*font_vtremington12,
        color(182, 146, 137),
        color(240, 217, 127),
        color(64, 64, 64));

    reset_loading_screen();

    // embrace user interface generation with right theme set!
    unique_ptr<widget::theme> tmp = widget::replace_theme(std::move(gametheme));
    auto ui                       = user_interface::create(*gm);
    gametheme                     = widget::replace_theme(std::move(tmp));

    // game is initially running, so pause it.
    ui->toggle_pause();

    while (true) {
        tmp = widget::replace_theme(std::move(gametheme));
        // 2006-12-01 doc1972 we should do some checks of the state if game
        // exits
        /*game::run_state state =*/game_exec(*gm, ui);
        gametheme = widget::replace_theme(std::move(tmp));

        music::instance().play_track(1, 500);
        loadsavequit_dialogue dlg(gm.get());

        int q = widget::run(dlg, 0, false);

        // replace game and ui if new game was loaded
        if (q == 2) {
            // fixme: ui doesn't need to get replaced, just give pointer to new
            // game to old ui, clear ui values and messages, finished...
            // this safes time to recompute map/water/sky etc.
            // as long as class game holds a pointer to ui this is more
            // difficult or won't work.
            gm.reset();
            ui = nullptr;
            gm = std::make_unique<game_editor>(dlg.get_gamefilename_to_load());

            // embrace user interface generation with right theme set!
            tmp       = widget::replace_theme(std::move(gametheme));
            ui        = user_interface::create(*gm);
            gametheme = widget::replace_theme(std::move(tmp));
        }
        // replace ui after loading!!!!
        if (q == 1) {
            music::instance().play_track(1, 500);
            break;
        }
        if (q == 0) {
            // music::instance()._fade_out(1000);
        }
    }
}

/** choose player data
    @returns false when cancelled
*/

class widget_image_select : public widget
{
    std::list<std::string> imagenames;
    std::string extension;
    std::list<std::string>::iterator current;

  public:
    using ptr = ptr_t<widget_image_select>;
    widget_image_select(int x, int y, int w, int h, std::string ext_, std::list<std::string> imagenames_)
        : widget(x, y, w, h, "")
        , imagenames(std::move(imagenames_))
        , extension(std::move(ext_))
        , current(imagenames.begin())
    {
        if (imagenames.empty()) {
            THROW(error, "can't use widget_image_select with empty list");
        }

        background = image_store.ref(*current + extension);
        add_child<widget_text>(20, 20, 0, 0, texts::get(117));
    }
    virtual auto get_current_imagename() const -> const std::string& { return *current; }
    void next(int direction)
    {
        if (direction > 0) {
            current++;
            if (current == imagenames.end()) {
                current = imagenames.begin();
            }
        } else {
            if (current == imagenames.begin()) {
                current = imagenames.end();
            }
            current--;
        }
        background = image_store.ref(*current + extension);
        redraw();
    }
    void draw() const override
    {
        redrawme   = false;
        vector2i p = get_pos();
        int bw     = int(background->get_width());
        int bh     = int(background->get_height());
        background->draw(p.x + size.x / 2 - bw / 2, p.y + size.y / 2 - bh / 2);
    }
    virtual void select_by_nr(unsigned n)
    {
        auto nextimg = imagenames.begin();
        for (; n > 0; --n) {
            ++nextimg;
            if (nextimg == imagenames.end()) {
                nextimg = imagenames.begin();
            }
        }
        if (nextimg != current) {
            current    = nextimg;
            background = image_store.ref(*current + extension);
            redraw();
        }
    }
    virtual auto get_selected() const -> unsigned
    {
        auto it    = imagenames.begin();
        unsigned n = 0;
        while (it != current) {
            ++n;
            ++it;
        }
        return n;
    }
};

class widget_button_next : public widget_button
{
    int direction;
    widget_image_select::ptr attached_widget;

  public:
    widget_button_next(
        int x,
        int y,
        int w,
        int h,
        int dir,
        widget_image_select::ptr att,
        const std::string& text_,
        const std::string& bg_image_)
        : widget_button(x, y, w, h, text_, bg_image_)
        , direction(dir)
        , attached_widget(att)
    {
    }
    void draw() const override
    {
        redrawme   = false;
        vector2i p = get_pos();
        int bw     = int(background->get_width());
        int bh     = int(background->get_height());

        colorf col = colorf(1.0, 1.0, 1.0, 1.0);
        if (mouseover != this) {
            col = colorf(1.0, 1.0, 1.0, 0.75);
        }
        background->draw(p.x + size.x / 2 - bw / 2, p.y + size.y / 2 - bh / 2, col);
    }
    void on_release() override
    {
        pressed = false;
        attached_widget->next(direction);
    }
};

// used for easier access of data later
struct flotilla
{
    unsigned nr;
    // later: type
    std::string insignia;
    std::string base;
    // later: operation areas
    std::vector<unsigned> subnrs;
    std::string description;
};

void show_flotilla_description(const std::string& infopopupdescr)
{
    auto w(widget::create_dialogue_ok("", infopopupdescr, 1024 * 3 / 4, 768 * 3 / 4));
    w->set_background_color(color(16, 8, 0));
    widget::run(*w);
}

auto choose_player_info(game::player_info& pi, const std::string& subtype, const date& gamedate) -> bool
{
    widget w(0, 0, 1024, 768, "", "playerselection_background.jpg");
    auto w2 = w.add_child<widget>(40, 40, 500, 640, "");
    w2->add_child<widget_text>(20, 20, 0, 0, texts::get(200));
    auto wplayername = w2->add_child<widget_edit>(20, 50, 460, 30, "Heinz Mustermann");
    std::vector<flotilla> availableflotillas;

    xml_doc flotilladb(get_data_dir() + "flotillas/available.xml");
    flotilladb.load();
    xml_elem eflotillas = flotilladb.child("flotillas");
    // compute which flotillas are available by time and submarine type
    // for every flotilla present a list of submarine IDs
    for (auto flot : eflotillas.iterate("flotilla")) {
        bool avail       = false;
        std::string base = flot.attr("base");
        flotilla ft;
        for (auto tp : flot.iterate("timeperiod")) {
            date dfr = date(tp.attr("from"));
            date dut = date(tp.attr("until"));
            if (dfr <= gamedate && gamedate <= dut) {
                // flotilla is available by date
                for (auto subs : tp.iterate("subs")) {
                    if (subtype.substr(std::string("submarine_").length()) == subs.attr("type")) {
                        // submarines are available
                        std::istringstream iss(subs.child_text());
                        while (!iss.eof()) {
                            unsigned nr;
                            iss >> nr;
                            ft.subnrs.push_back(nr);
                        }
                        if (!ft.subnrs.empty()) {
                            // list of available subs is not empty
                            avail = true;
                            break;
                        }
                    }
                }
                if (avail) {
                    if (tp.has_attr("base")) {
                        base = tp.attr("base");
                    }
                    break;
                }
            }
        }
        if (avail) {
            ft.nr          = flot.attru("nr");
            ft.insignia    = flot.attr("sign");
            ft.base        = base;
            ft.description = "not available, fix me";
            for (auto desc : flot.iterate("description")) {
                if (desc.attr("lang") == texts::get_language_code()) {
                    ft.description = desc.child_text();
                    break;
                }
            }
            availableflotillas.push_back(ft);
        }
    }
    if (availableflotillas.empty()) {
        log_warning("No flotilla available with these settings");
        return false;
    }
    // remove dummy flotilla if we have others
    if (availableflotillas.size() > 1) {
        for (auto it = availableflotillas.begin(); it != availableflotillas.end(); ++it) {
            if (it->nr == 99) {
                availableflotillas.erase(it);
                break;
            }
        }
    }

    struct emblemselect : widget_image_select
    {
        widget_list::ptr flst;
        emblemselect(int x, int y, int w, int h, const std::string& ext_, const std::list<std::string>& imagenames_)
            : widget_image_select(x, y, w, h, ext_, imagenames_)
        {
        }
        void on_release() override
        {
            widget_image_select::on_release();
            if (flst != nullptr) {
                flst->set_selected(get_selected());
            }
        }
    };
    std::list<std::string> emblems;
    for (auto& availableflotilla : availableflotillas) {
        emblems.push_back(availableflotilla.insignia);
    }
    auto wemblem = w.add_child<emblemselect>(764 - 220 / 2, 572 - 32 - 300 / 2, 220, 300, ".png", emblems);

    struct flotlist : public widget_list
    {
        widget_image_select::ptr wis;
        widget_list::ptr wsns;
        widget_text::ptr baseloc;
        const std::vector<flotilla>& availableflotillas;
        widget_button::ptr infobut;
        std::string& infobutdesc;
        void on_sel_change() override
        {
            wis->select_by_nr(std::max(0, get_selected()));
            wsns->clear();
            // iterate list of available flotillas and offer sub numbers
            int s = get_selected();
            if (s >= 0) {
                const std::vector<unsigned>& l = availableflotillas[s].subnrs;
                for (unsigned int i : l) {
                    std::ostringstream oss;
                    oss << "U " << i;
                    wsns->append_entry(oss.str());
                }
                baseloc->set_text(availableflotillas[s].base);
                infobut->enable();
                infobutdesc = availableflotillas[s].description;
            } else {
                infobut->disable();
            }
        }
        flotlist(
            int x,
            int y,
            int w,
            int h,
            widget_image_select::ptr w_,
            widget_list::ptr ws,
            widget_text::ptr bl,
            const std::vector<flotilla>& af,
            widget_button::ptr ib,
            std::string& ibd)
            : widget_list(x, y, w, h)
            , wis(w_)
            , wsns(ws)
            , baseloc(bl)
            , availableflotillas(af)
            , infobut(ib)
            , infobutdesc(ibd)
        {
        }
    };
    auto wsubnumber   = w2->add_child<widget_list>(20, 420, 460, 200);
    auto baselocation = w2->add_child<widget_text>(20, 350, 0, 0, "");
    std::string infopopupdescr;
    auto infobutton = w2->add_child<widget_caller_button<std::string&>>(
        300, 320, 180, 40, texts::get(161), show_flotilla_description, infopopupdescr);
    auto wflotilla = w2->add_child<flotlist>(
        20, 110, 460, 200, wemblem, wsubnumber, baselocation, availableflotillas, infobutton, infopopupdescr);
    std::string flotname = texts::get(164);
    for (auto& availableflotilla : availableflotillas) {
        std::string fn = flotname;
        fn.replace(fn.find('#'), 1, str(availableflotilla.nr));
        wflotilla->append_entry(fn);
    }

    w2->add_child<widget_text>(20, 80, 0, 0, texts::get(175));
    wemblem->flst = wflotilla;
    w2->add_child<widget_text>(20, 320, 0, 0, texts::get(163));
    w2->add_child<widget_text>(20, 380, 0, 0, texts::get(176));

    std::list<std::string> playerphotos;
    for (unsigned i = 1; i <= 11; ++i) {
        playerphotos.push_back(std::string("player_photo") + str(i));
    }

    w.add_child<widget_text>(661 + 20, 40 + 30, 0, 0, texts::get(162));

    auto wplayerphoto = w.add_child<widget_image_select>(661, 40 + 45, 205, 300, ".jpg|png", playerphotos);

    w.add_child<widget_button_next>(661 - 35, 40 + 150, 25, 80, -1, wplayerphoto, "", "BG_btn_left.png");

    w.add_child<widget_button_next>(661 + 215, 40 + 150, 25, 80, 1, wplayerphoto, "", "BG_btn_right.png");

    auto wm = w.add_child<widget_menu>(40, 700, 0, 40, "", true);

    wm->add_entry(
        texts::get(20),
        std::make_unique<widget_caller_button<widget&>>(
            70, 700, 400, 40, "", [](auto& w) { w.close(1); }, w));

    wm->add_entry(
        texts::get(19),
        std::make_unique<widget_caller_button<widget&>>(
            540, 700, 400, 40, "", [](auto& w) { w.close(2); }, w));

    wm->adjust_buttons(944);
    int result = widget::run(w, 0, false);
    if (result == 2) {
        pi.name        = wplayername->get_text();
        pi.flotilla    = availableflotillas[std::max(0, wflotilla->get_selected())].nr;
        pi.submarineid = wsubnumber->get_selected_entry();

        pi.photo = atoi(wplayerphoto->get_current_imagename()
                            .substr(std::string("player_photo").length())
                            .c_str()); // fixme unstable
        // log_debug(player_name<<","<<player_flotilla<<","<<player_subnumber<<","<<player_photo);
        return true;
    }
    return false;
}

//
// create a custom convoy mission
//
void create_convoy_mission()
{
    widget w(0, 0, 1024, 768, texts::get(9), "scopewatcher.jpg");
    w.add_child<widget_text>(40, 60, 0, 0, texts::get(16));
    auto wsubtype = w.add_child<widget_list>(40, 90, 200, 200);
    w.add_child<widget_text>(280, 60, 0, 0, texts::get(84));
    auto wcvsize = w.add_child<widget_list>(280, 90, 200, 200);
    w.add_child<widget_text>(520, 60, 0, 0, texts::get(88));
    auto wescortsize = w.add_child<widget_list>(520, 90, 200, 200);
    w.add_child<widget_text>(760, 60, 0, 0, texts::get(90));
    auto wtimeofday = w.add_child<widget_list>(760, 90, 200, 200);
    w.add_child<widget_text>(40, 310, 0, 0, texts::get(62));
    auto wtimeperiod = w.add_child<widget_list>(40, 340, 640, 200);
    wsubtype->append_entry(texts::get(17));
    //	wsubtype->append_entry(texts::get(174));
    //	wsubtype->append_entry(texts::get(18));
    wsubtype->append_entry(texts::get(800));
    wsubtype->append_entry(texts::get(801));
    wsubtype->append_entry(texts::get(802));
    wsubtype->append_entry(texts::get(803));
    wcvsize->append_entry(texts::get(85));
    wcvsize->append_entry(texts::get(86));
    wcvsize->append_entry(texts::get(87));
    wescortsize->append_entry(texts::get(89));
    wescortsize->append_entry(texts::get(85));
    wescortsize->append_entry(texts::get(86));
    wescortsize->append_entry(texts::get(87));
    wtimeofday->append_entry(texts::get(91));
    wtimeofday->append_entry(texts::get(92));
    wtimeofday->append_entry(texts::get(93));
    wtimeofday->append_entry(texts::get(94));
    wtimeperiod->append_entry(texts::get(63));
    wtimeperiod->append_entry(texts::get(64));
    wtimeperiod->append_entry(texts::get(65));
    wtimeperiod->append_entry(texts::get(66));
    wtimeperiod->append_entry(texts::get(67));
    wtimeperiod->append_entry(texts::get(68));
    wtimeperiod->append_entry(texts::get(69));
    wtimeperiod->append_entry(texts::get(70));

    {
        auto wm = w.add_child<widget_menu>(40, 700, 0, 40, "", true);
        wm->add_entry(
            texts::get(20),
            std::make_unique<widget_caller_button<widget&>>(
                70, 700, 400, 40, "", [](auto& w) { w.close(1); }, w));
        wm->add_entry(
            texts::get(19),
            std::make_unique<widget_caller_button<widget&>>(
                540, 700, 400, 40, "", [](auto& w) { w.close(2); }, w));
        wm->adjust_buttons(944);
    }
    while (true) {
        int result = widget::run(w, 0, false);
        if (result == 2) { // start game
            std::string st;
            switch (wsubtype->get_selected()) {
                case 0:
                    st = "submarine_VIIc";
                    break;
                    //			case 1: st = "submarine_IXc40"; break;
                    //			case 2: st = "submarine_XXI"; break;
                case 1:
                    st = "submarine_IIa";
                    break;
                case 2:
                    st = "submarine_IIb";
                    break;
                case 3:
                    st = "submarine_IIc";
                    break;
                case 4:
                    st = "submarine_IId";
                    break;
            }

            // compute mission time (date)
            date datebegin;
            date dateend;
            switch (wtimeperiod->get_selected()) {
                case 0:
                    datebegin = date(1939, 9, 1);
                    dateend   = date(1940, 5, 31);
                    break;
                case 1:
                    datebegin = date(1940, 6, 1);
                    dateend   = date(1941, 3, 31);
                    break;
                case 2:
                    datebegin = date(1941, 4, 1);
                    dateend   = date(1941, 12, 31);
                    break;
                case 3:
                    datebegin = date(1942, 1, 1);
                    dateend   = date(1942, 6, 30);
                    break;
                case 4:
                    datebegin = date(1942, 7, 1);
                    dateend   = date(1942, 12, 31);
                    break;
                case 5:
                    datebegin = date(1943, 1, 1);
                    dateend   = date(1943, 5, 31);
                    break;
                case 6:
                    datebegin = date(1943, 6, 1);
                    dateend   = date(1944, 6, 30);
                    break;
                case 7:
                    datebegin = date(1944, 7, 1);
                    dateend   = date(1945, 5, 8);
                    break;
            }
            double tpr    = rnd();
            double time   = datebegin.get_time() * (1.0 - tpr) + dateend.get_time() * tpr;
            time          = floor(time / 86400) * 86400; // set to begin of day
            date gamedate = date(unsigned(time));

            // show player gui screen
            // use strings for all data, more extendable
            game::player_info pi;
            bool ok = choose_player_info(pi, st, gamedate);
            if (!ok) {
                continue;
            }

            // reset loading screen here to show user we are doing something
            // fixme: give data to game! player data. maybe combine that to a
            // struct!
            reset_loading_screen();
            run_game(std::make_unique<game>(
                st, wcvsize->get_selected(), wescortsize->get_selected(), wtimeofday->get_selected(), gamedate, pi));
        } else {
            break;
        }
    }
}

//
// choose a historical mission
//
void choose_historical_mission()
{
    std::vector<std::string> missions;

    // read missions
    unsigned nr_missions = 0;
    {
        directory missiondir(get_mission_dir());
        while (true) {
            std::string e = missiondir.read();
            if (e.empty()) {
                break;
            }
            if (e.length() > 4 && e.substr(e.length() - 4) == ".xml") {
                missions.push_back(e);
                ++nr_missions;
            }
        }
    }

    // read descriptions, set up windows
    widget w(0, 0, 1024, 768, texts::get(10), "sunderland.jpg");
    std::vector<std::string> descrs;
    struct msnlist : public widget_list
    {
        const std::vector<std::string>& descrs;
        widget_text::ptr wdescr;
        void on_sel_change() override
        {
            int sel = get_selected();
            if (sel >= 0 && sel < int(descrs.size())) {
                wdescr->set_text(descrs[sel]);
            } else {
                wdescr->set_text("");
            }
        }
        msnlist(int x, int y, int w, int h, const std::vector<std::string>& descrs_, widget_text::ptr wdescr_)
            : widget_list(x, y, w, h)
            , descrs(descrs_)
            , wdescr(wdescr_)
        {
        }
        ~msnlist() override = default;
    };
    auto wdescr   = w.add_child<widget_text>(40, 380, 1024 - 80, 300, "", true);
    auto wmission = w.add_child<msnlist>(40, 60, 1024 - 80, 300, descrs, wdescr);
    // Note:
    // Missions have the same format like savegames, except that the head xml
    // node has an additional child node <description> with multi-lingual
    // descriptions of the mission.
    for (unsigned i = 0; i < nr_missions; ++i) {
        xml_doc doc(get_mission_dir() + missions[i]);
        doc.load();
        xml_elem edftdmission = doc.child("dftd-mission");
        xml_elem edescription = edftdmission.child("description");
        for (auto elem : edescription.iterate("short")) {
            if (elem.attr("lang") == texts::get_language_code()) {
                std::string desc;
                try {
                    desc = elem.child_text();
                }
                catch (xml_error& e) {
                    desc = "NO DESCRIPTION???";
                }
                wmission->append_entry(desc);
                break;
            }
        }
        for (auto elem : edescription.iterate("long")) {
            if (elem.attr("lang") == texts::get_language_code()) {
                std::string desc;
                try {
                    desc = elem.child_text();
                }
                catch (xml_error& e) {
                    desc = "NO DESCRIPTION???";
                }
                descrs.push_back(desc);
                break;
            }
        }
    }
    wmission->on_sel_change();

    auto wm = w.add_child<widget_menu>(40, 700, 0, 40, "", true);
    wm->add_entry(
        texts::get(20),
        std::make_unique<widget_caller_button<widget&>>(
            70, 700, 400, 40, "", [](auto& w) { w.close(1); }, w));
    wm->add_entry(
        texts::get(19),
        std::make_unique<widget_caller_button<widget&>>(
            70, 700, 400, 40, "", [](auto& w) { w.close(2); }, w));
    wm->adjust_buttons(944);
    int result = widget::run(w, 0, false);
    if (result == 2) { // start game
        unique_ptr<game> gm;
        try {
            gm = std::make_unique<game>(get_mission_dir() + missions[wmission->get_selected()]);
        }
        catch (error& e) {
            log_warning("error loading game: " << e.what());
            // fixme: show dialogue!
            return;
        }
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game(std::move(gm));
    }
}

//
// choose a saved game
//
void choose_saved_game()
{
    loadsavequit_dialogue dlg(nullptr);
    int q = widget::run(dlg, 0, false);
    if (q == 0) {
        return;
    }
    if (q == 2) {
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game(std::make_unique<game>(dlg.get_gamefilename_to_load()));
    }
}

void menu_single_mission()
{
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(21));
    //	wm->add_entry(texts::get(8),
    // std::make_unique<widget_caller_button<>>(menu_notimplemented));
    wm->add_entry(texts::get(9), std::make_unique<widget_caller_button<>>(create_convoy_mission));
    wm->add_entry(texts::get(10), std::make_unique<widget_caller_button<>>(choose_historical_mission));
    wm->add_entry(texts::get(118), std::make_unique<widget_caller_button<>>(choose_saved_game));
    wm->add_entry(texts::get(11), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));
    wm->align(0, 0);
    widget::run(w, 0, false);
}

void menu_mission_editor()
{
    widget w(0, 0, 1024, 768, texts::get(222), "scopewatcher.jpg");
    w.add_child<widget_text>(40, 60, 944, 0, texts::get(223));

    /*
        w.add_child<widget_text>(40, 60, 0, 0, texts::get(16));
        auto& wsubtype = &w.add_child<widget_list>(40, 90, 200,
        200)); wsubtype->append_entry(texts::get(17));
        wsubtype->append_entry(texts::get(174));
        wsubtype->append_entry(texts::get(18));
    */

    auto wm = w.add_child<widget_menu>(40, 700, 0, 40, "", true);
    wm->add_entry(
        texts::get(20),
        std::make_unique<widget_caller_button<widget&>>(
            540, 700, 400, 40, "", [](auto& w) { w.close(1); }, w));
    wm->add_entry(
        texts::get(222),
        std::make_unique<widget_caller_button<widget&>>(
            70, 700, 400, 40, "", [](auto& w) { w.close(2); }, w));
    wm->adjust_buttons(944);
    int result = widget::run(w, 0, false);
    if (result == 2) { // start editor
                       /*
                               std::string st;
                               switch (wsubtype->get_selected()) {
                                   case 0: st = "submarine_VIIc"; break;
                                   case 1: st = "submarine_IXc40"; break;
                                   case 2: st = "submarine_XXI"; break;
                               }
                       */
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game_editor(std::make_unique<game_editor>(date(1939, 9, 1) /*st*/));
    }
}

void menu_select_language()
{
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(26));

    struct lgclist : public widget_list
    {
        void on_sel_change() override
        {
            texts::set_language(get_selected());
            cfg::instance().set("language", get_selected());
        }
        lgclist(int x, int y, int w, int h)
            : widget_list(x, y, w, h)
        {
        }
    };

    auto wlg    = w.add_child<lgclist>(0, 0, 400, 400);
    unsigned nl = texts::get_nr_of_available_languages();
    for (unsigned i = 0; i < nl; ++i) {
        wlg->append_entry(texts::get(i, texts::languages));
    }
    wlg->set_selected(texts::get_current_language_nr());

    auto wcb = w.add_child<widget_caller_button<widget&>>(
        0, 0, 400, 40, texts::get(11), [](auto& w) { w.close(0); }, w);

    wlg->align(0, 0);
    vector2i wlgp = wlg->get_pos();
    vector2i wlgs = wlg->get_size();
    wm->set_pos(vector2i(wlgp.x, wlgp.y - 60));
    wcb->set_pos(vector2i(wlgp.x, wlgp.y + wlgs.y + 20));

    widget::run(w, 0, false);
}

//
// options:
// - set resolution
// - enable bump mapping
// - enable wave foam
// - detail for map/terrain/water (wave tile detail, # of wave tiles, global
// terrain detail, wave bump map detail)
// - set fullscreen
// - invert mouse in view
// - set keys
//

void apply_mode(widget_list& wlg)
{
    auto wks    = wlg.get_selected_entry();
    auto height = atoi(wks.substr(wks.rfind('x') + 1).c_str());
    auto width  = atoi(wks.substr(0, wks.rfind('x')).c_str());

    // try to set video mode BEFORE writing to config file, so that if video
    // mode is broken, user is not forced to same mode again on restart
    try {
        auto params       = SYS().get_parameters();
        params.resolution = {width, height};
        SYS().set_parameters(params);
        cfg::instance().set("screen_res_y", height);
        cfg::instance().set("screen_res_x", width);
        glClearColor(0, 0, 0, 0);
    }
    catch (exception& e) {
        log_warning("Video mode setup failed: " << e.what());
    }
}

void menu_resolution()
{
    const auto& available_resolutions = SYS().get_available_resolutions();

    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(106));

    auto wlg = w.add_child<widget_list>(0, 0, 400, 400);

    vector2i curr_res(SYS().get_res_x(), SYS().get_res_y());
    unsigned curr_entry = 0;
    unsigned i          = 0;
    for (auto available_resolution : available_resolutions) {
        wlg->append_entry(str(available_resolution.x) + "x" + str(available_resolution.y));
        if (available_resolution == curr_res) {
            curr_entry = i;
        }
        ++i;
    }
    wlg->set_selected(curr_entry);

    auto wcb = w.add_child<widget_caller_button<widget&>>(
        0, 0, 400, 40, texts::get(20), [](auto& w) { w.close(0); }, w);
    w.add_child<widget_caller_button<widget_list&>>(516, 604, 452, 40, texts::get(106), apply_mode, *wlg);

    wlg->align(0, 0);
    vector2i wlgp = wlg->get_pos();
    vector2i wlgs = wlg->get_size();
    wm->set_pos(vector2i(wlgp.x, wlgp.y - 60));
    wcb->set_pos(vector2i(wlgp.x - 260, wlgp.y + wlgs.y + 20));
    widget::run(w, 0, false);
}

void configure_key(widget_list& wkeys)
{
    struct confkey_widget : public widget
    {
        widget_text::ptr keyname;
        key_command keynr;
        void on_key(key_code kc, key_mod km) override
        {
            if (kc == key_code::ESCAPE) {
                close(0);
                return;
            }
            cfg::instance().set_key(keynr, kc, km);
            keyname->set_text(SYS().get_key_name(kc, km));
            redraw();
        }
        confkey_widget(int x, int y, int w, int h, const std::string& text_, const std::string& backgrimg, unsigned sel)
            : widget(x, y, w, h, text_, backgrimg)
            , keynr(key_command(sel)) // fixme later use key_command in widget directly!
        {
            auto k  = cfg::instance().getkey(keynr);
            keyname = add_child<widget_text>(40, 80, 432, 40, SYS().get_key_name(k.keycode, k.keymod));
            add_child<widget_text>(40, 120, 432, 40, texts::get(217));
        }
        ~confkey_widget() override = default;
    };
    unsigned sel = wkeys.get_selected();
    confkey_widget w(256, 256, 512, 256, texts::get(216), "", sel);
    string wks = wkeys.get_selected_entry();
    wks        = wks.substr(0, wks.find('\t'));
    w.add_child<widget_text>(40, 40, 432, 32, wks);
    widget::run(w, 0, true);
    auto k = cfg::instance().getkey(key_command(sel));
    wkeys.set_entry(sel, texts::get(sel + 600) + string("\t") + SYS().get_key_name(k.keycode, k.keymod));
}

void menu_configure_keys()
{
    widget w(0, 0, 1024, 768, texts::get(214), "titlebackgr.jpg");
    auto wkeys = w.add_child<widget_list>(40, 50, 944, 640);
    wkeys->set_column_width(700);

    for (unsigned i = 600; i < 600 + unsigned(key_command::number); ++i) {
        cfg::key k = cfg::instance().getkey(key_command(i - 600));
        wkeys->append_entry(texts::get(i) + std::string("\t") + SYS().get_key_name(k.keycode, k.keymod));
    }

    // fixme: handle undefined keys!
    // fixme: check for double keys!

    w.add_child<widget_caller_button<widget&>>(
        40, 708, 452, 40, texts::get(20), [](auto& w) { w.close(0); }, w);
    w.add_child<widget_caller_button<widget_list&>>(532, 708, 452, 40, texts::get(215), configure_key, *wkeys);
    widget::run(w, 0, false);
}

void menu_opt_input()
{
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(705));

    wm->add_entry(texts::get(214), std::make_unique<widget_caller_button<>>(menu_configure_keys));
    wm->add_entry(texts::get(709),
                  std::make_unique<widget_caller_button<>>(menu_notimplemented)); // TODO(tj):

    wm->add_entry(texts::get(11), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));
    wm->align(0, 0);
    widget::run(w, 0, false);
}

void menu_opt_audio()
{
    menu_notimplemented(); // TODO(tj):
}
void menu_opt_video()
{
    unsigned wd    = 400;
    unsigned gap   = 112;
    unsigned x     = 56;
    unsigned y     = 150;
    unsigned right = x + wd + gap;

    // make widgets
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    w.add_child<widget_menu>(x, y, wd, 40, texts::get(707));
    auto resolution = std::make_unique<widget_caller_button<>>(x, y + 60, wd, 40, texts::get(106), menu_resolution);
    auto vsync = w.add_child<widget_checkbox>(right, y + 60, wd, 40, cfg::instance().getb("vsync"), texts::get(720));
    auto terrain_lod = w.add_child<widget_slider>(
        x, y + 120, wd, 80, texts::get(112), 3, 9, cfg::instance().geti("terrain_detail"), 3);
    auto tex_compress = w.add_child<widget_checkbox>(
        right, y + 120, wd, 40, cfg::instance().getb("use_compressed_textures"), texts::get(721));

    auto wfx_quality = w.add_child<widget_list>(x + (wd / 2), y + 220, wd / 2, 80);
    w.add_child<widget_text>(x, y + 220, wd / 2, 20, texts::get(713));
    auto w_postprocessing = w.add_child<widget_list>(right + (wd / 2), y + 220, wd / 2, 80);
    w.add_child<widget_text>(right, y + 220, wd / 2, 20, texts::get(714));
    auto anisotropic_level = w.add_child<widget_list>(x + (wd / 2), y + 320, wd / 2, 80);
    w.add_child<widget_text>(x, y + 320, wd / 2, 20, texts::get(722));
    auto anti_aliasing_level = w.add_child<widget_list>(right + (wd / 2), y + 320, wd / 2, 80);
    w.add_child<widget_text>(right, y + 320, wd / 2, 20, texts::get(723));
    w.add_child<widget_caller_button<widget&>>(
        x, y + 420, wd, 40, texts::get(20), [](auto& w) { w.close(0); }, w);

    // insert values
    wfx_quality->append_entry(texts::get(710));
    wfx_quality->append_entry(texts::get(711));
    wfx_quality->append_entry(texts::get(712));
    wfx_quality->set_selected(cfg::instance().geti("sfx_quality"));

    w_postprocessing->append_entry(texts::get(715));
    w_postprocessing->append_entry(texts::get(716));
    w_postprocessing->append_entry(texts::get(717));
    w_postprocessing->set_selected(cfg::instance().geti("postprocessing"));

    float max_ani = 1.0;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_ani);
    anisotropic_level->append_entry(texts::get(724));

    if (1.0F != max_ani) {
        unsigned count = 0;
        float base     = 1.0F;

        while (base < max_ani) {
            base *= 2;
            count++;

            anisotropic_level->append_entry(str(base));

            if (cfg::instance().getf("anisotropic_level") == base) {
                anisotropic_level->set_selected(count);
            }
        }
    }

    anti_aliasing_level->append_entry(texts::get(724));
    anti_aliasing_level->set_selected(0); // TODO(tj): FIXME

    widget::run(w, 0, false);

    // save settings
    cfg::instance().set("vsync", vsync->is_checked());

    cfg::instance().set("terrain_detail", terrain_lod->get_curr_value());
    cfg::instance().set("use_compressed_textures", tex_compress->is_checked());

    cfg::instance().set("sfx_quality", wfx_quality->get_selected());
    cfg::instance().set("postprocessing", w_postprocessing->get_selected());
    // TODO(tj): need to update postproc. code to use this int instead of the
    // previous two booleans.

    if (0 == anisotropic_level->get_selected()) {
        cfg::instance().set("use_ani_filtering", false);
        cfg::instance().set("anisotropic_level", 1.0F);
    } else {
        cfg::instance().set("use_ani_filtering", true);

        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_ani);
        unsigned max_list = anisotropic_level->get_listsize() - 1;
        unsigned selected = anisotropic_level->get_selected();

        for (unsigned ix = max_list; ix > selected; ix--) {
            max_ani /= 2.0F;
        }

        cfg::instance().set("anisotropic_level", max_ani);
    }

    if (0 == anti_aliasing_level->get_selected()) {
        cfg::instance().set("use_multisampling", false);
    } else {
        cfg::instance().set("use_multisampling", true);
        // TODO(tj): - implement this, kind of held back by the lack of easy of
        // detecting suitable/MAX values.
        // cfg::instance().set("multisampling_level", ??? );
    }

    // TODO(tj): something about this ? we need to convert or update existing code
    // that uses the boolean use_hqsfx
    //	cfg::instance().set("use_hqsfx", whqsfx->is_checked());
    //	glsl_shader::enable_hqsfx = whqsfx->is_checked();
}

void menu_opt_network()
{
    menu_notimplemented(); // TODO(tj):
}

void menu_options()
{
    widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");
    auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(29));

    wm->add_entry(texts::get(705), std::make_unique<widget_caller_button<>>(menu_opt_input));
    wm->add_entry(texts::get(706), std::make_unique<widget_caller_button<>>(menu_opt_audio));
    wm->add_entry(texts::get(707), std::make_unique<widget_caller_button<>>(menu_opt_video));
    wm->add_entry(texts::get(708), std::make_unique<widget_caller_button<>>(menu_opt_network));
    wm->add_entry(texts::get(11), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));
    wm->align(0, 0);
    widget::run(w, 0, false);
}

// vessel preview
class vessel_view
{
    std::vector<std::string> shipnames;
    std::vector<std::string>::iterator current;
    std::set<string> modellayouts;
    std::set<string>::iterator currentlayout;
    widget_text::ptr wdesc; ///< description of model
    widget_3dview::ptr w3d;

    auto load_model()
    {
        xml_doc doc(data_file().get_filename(*current));
        doc.load();
        string mdlname = doc.first_child().child("classification").attr("modelname");

        for (auto elem : doc.first_child().child("description").iterate("near")) {
            if (elem.attr("lang") == texts::get_language_code()) {
                wdesc->set_text_and_resize(elem.child_text());
                int y = wdesc->get_pos().y;
                wdesc->align(0, -1);
                wdesc->move_pos(vector2i(0, y));
                break;
            }
        }
        auto mdl = std::make_unique<model>(data_file().get_path(*current) + mdlname);

        // register and set default layout.
        mdl->register_layout();
        mdl->set_layout();
        modellayouts.clear();
        mdl->get_all_layout_names(modellayouts);
        currentlayout = modellayouts.begin();

        return mdl;
    }

  public:
    vessel_view(widget& parent, widget_text::ptr wdesc_)
        : current(shipnames.end())
        , wdesc(wdesc_)
    {
        color bgcol(50, 50, 150);

        shipnames = data_file().get_ship_list();
        auto tmp  = data_file().get_submarine_list();
        shipnames.insert(shipnames.end(), tmp.begin(), tmp.end());
        tmp = data_file().get_airplane_list();
        shipnames.insert(shipnames.end(), tmp.begin(), tmp.end());
        current = shipnames.begin();

        w3d = parent.add_child<widget_3dview>(20, 0, 1024 - 2 * 20, 700 - 32 - 16, load_model(), bgcol);

        vector3f lightdir = vector3f(angle(143).cos(), angle(143).sin(), angle(49.5).tan()).normal();

        w3d->set_light_dir(vector4f(lightdir.x, lightdir.y, lightdir.z, 0));
        w3d->set_light_color(color(233, 221, 171));
    }
    void next()
    {
        ++current;

        if (current == shipnames.end()) {
            current = shipnames.begin();
        }
        w3d->set_model(load_model());
        w3d->redraw();
    }
    void previous()
    {
        if (current == shipnames.begin()) {
            current = shipnames.end();
        }
        --current;

        w3d->set_model(load_model());
        w3d->redraw();
    }
    void switchlayout()
    {
        ++currentlayout;

        if (currentlayout == modellayouts.end()) {
            currentlayout = modellayouts.begin();
        }

        // registering the same layout multiple times does not hurt, no problem
        w3d->get_model()->register_layout(*currentlayout);
        w3d->get_model()->set_layout(*currentlayout);
        w3d->redraw();
    }
};

void menu_show_vessels()
{
    widget w(0, 0, 1024, 768, texts::get(24), "threesubs.jpg");
    auto wt = w.add_child<widget_text>(0, 50, 1024, 32, "", true);
    auto wm = w.add_child<widget_menu>(0, 700, 140, 32, "" /*texts::get(110)*/, true);
    vessel_view vw(w, wt);

    wm->add_entry(
        texts::get(115), std::make_unique<widget_caller_button<vessel_view&>>([](auto& vw) { vw.next(); }, vw));
    wm->add_entry(
        texts::get(116), std::make_unique<widget_caller_button<vessel_view&>>([](auto& vw) { vw.previous(); }, vw));
    // fixme: disable butten when there is only one layout
    wm->add_entry(
        texts::get(246), std::make_unique<widget_caller_button<vessel_view&>>([](auto& vw) { vw.switchlayout(); }, vw));
    wm->add_entry(texts::get(117), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));
    wm->adjust_buttons(984);

    widget::run(w, 0, false);
}

auto file_exists(const std::string& fn) -> bool
{
    ifstream in(fn.c_str(), ios::in | ios::binary);
    return in.good();
}

auto set_dir(const std::string& dir, std::string& setdir) -> bool
{
    // check if it is a directory.
    if (!is_directory(dir)) {
        return false;
    }
    // append separator if needed
    if (dir[dir.length() - 1] != '/') {
        setdir = dir + "/";
    } else {
        setdir = dir;
    }
    return true;
}

auto mymain(std::vector<std::string>& args) -> int
{
    // report critical errors (on Unix/Posix systems)
    install_segfault_handler();

    std::string highscoredirectory =
#ifdef WIN32
        "./highscores/";
#else
        // fixme: use global /var/games instead
        std::string(getenv("HOME")) + "/.dangerdeep/";
#endif

    std::string configdirectory =
#ifdef WIN32
        "./config/";
#else
        std::string(getenv("HOME")) + "/.dangerdeep/";
#endif

    // command line argument parsing
    unsigned res_x  = 0;
    unsigned res_y  = 0;
    bool fullscreen = true;
    std::string cmdmissionfilename;
    bool runeditor     = false;
    bool override_lang = false;
    bool use_sound     = true;
    date editor_start_date(1939, 9, 1);

    // parse commandline
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (*it == "--help") {
            cout << "*** Danger from the Deep ***\nusage:\n--help\t\tshow this\n"
                 << "--language \tuse the listed language CODEs from the "
                    "common.cvs file. \"en\" is the default language\n"
                 << "--res X*Y\tuse resolution X horizontal, Y "
                    "vertical.\n\t\tDefault is 1024*768. If no Y value is "
                    "given, Y=3/4*X is assumed.\n"
                 << "--nofullscreen\tdon't use fullscreen\n"
                 << "--debug\t\tdebug mode: no fullscreen, resolution 800\n"
                 << "--editor\trun mission editor directly\n"
                 << "--editordate yyyy/mm/dd\tset start date for editor\n"
                 << "--mission fn\trun mission from file fn (just the filename "
                    "in the mission directory)\n"
                 << "--nosound\tdon't use sound\n"
                 << "--datadir path\tset base directory of data, must point to "
                    "a directory with subdirs images/ textures/ objects/ and so "
                    "on. Default on Unix e.g. /usr/local/share/dangerdeep.\n"
                 << "--savegamedir path\tdirectory for savegames, default path "
                    "depends on platform\n"
                 << "--highscoredir path\tdirectory for highscores, default "
                    "path depends on platform\n"
                 << "--configdir path\tdirectory for configuration data, "
                    "default path depends on platform\n"
#if !(defined(WIN32) || (defined(__APPLE__) && defined(__MACH__)))
                 << "--vsync\tsync to vertical retrace signal (for nvidia "
                    "cards)\n"
#endif
                 << "--consolelog\tcopy log output to current console\n";
            return 0;
        } else if (*it == "--nofullscreen") {
            fullscreen = false;
        } else if (*it == "--debug") {
            fullscreen = false;
            res_x      = 800;
            res_y      = 600;
        } else if (*it == "--mission") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                cmdmissionfilename = *it2;
                ++it;
            }
        } else if (*it == "--editor") {
            runeditor = true;
        } else if (*it == "--editordate") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                editor_start_date = date(*it2);
                ++it;
            }
        } else if (*it == "--consolelog") {
            log::copy_output_to_console = true;
        } else if (*it == "--nosound") {
            use_sound = false;
        } else if (*it == "--res") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                std::string::size_type st = it2->find("*");
                if (st == std::string::npos) {
                    // no "*" found, use y=3/4*x
                    res_x = atoi(it2->c_str());
                    res_y = 3 * res_x / 4;
                } else {
                    res_x = atoi(it2->substr(0, st).c_str());
                    res_y = atoi(it2->substr(st + 1).c_str());
                }
                ++it;
            }
        } else if (*it == "--datadir") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                std::string datadir = *it2;
                // check if it is a directory.
                if (!is_directory(datadir)) {
                    cout << "ERROR: data directory is no directory!\n";
                    return -1;
                }
                // append separator if needed
                if (datadir[datadir.length() - 1] != '/') {
                    datadir += "/";
                }
                // check if there are valid files in data directory.
                bool datadirseemsok = true;
                if (!is_directory(datadir + "fonts")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "images")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "missions")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "objects")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "shaders")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "sounds")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "texts")) {
                    datadirseemsok = false;
                } else if (!is_directory(datadir + "textures")) {
                    datadirseemsok = false;
                }
                if (!datadirseemsok) {
                    cout << "ERROR: data directory is missing crucial files!\n";
                    return -1;
                }
                set_data_dir(datadir);
                cout << "data directory set to \"" << datadir << "\"\n";
                ++it;
            }
        } else if (*it == "--savegamedir") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, savegamedirectory)) {
                    cout << "ERROR: savegame directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--highscoredir") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, highscoredirectory)) {
                    cout << "ERROR: highscore directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--configdir") {
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                if (!set_dir(*it2, configdirectory)) {
                    cout << "ERROR: config directory is no directory!\n";
                    return -1;
                }
                ++it;
            }
        } else if (*it == "--language") { // included 2006/11/14 by doc1972
            auto it2 = it;
            ++it2;
            if (it2 != args.end()) {
                texts::set_language(*it2);
                override_lang = true;
                ++it;
            }
#if !(defined(WIN32) || (defined(__APPLE__) && defined(__MACH__)))
        } else if (*it == "--vsync") {
            if (putenv((char*) "__GL_SYNC_TO_VBLANK=1") < 0) {
                cout << "ERROR: vsync setting failed.\n";
            }
#endif
        } else {
            cout << "unknown parameter " << *it << ".\n";
        }
    }

    // parse configuration
    cfg& mycfg = cfg::instance();
    mycfg.register_option("screen_res_x", 1024);
    mycfg.register_option("screen_res_y", 768);
    mycfg.register_option("fullscreen", true);
    mycfg.register_option("debug", false);
    mycfg.register_option("sound", true);

    mycfg.register_option("sfx_quality", 0);
    mycfg.register_option("postprocessing", 0);

    mycfg.register_option("use_hqsfx", true); // TODO(tj): remove
    mycfg.register_option("use_ani_filtering", false);
    mycfg.register_option("anisotropic_level", 1.0F);
    mycfg.register_option("use_compressed_textures", false);
    mycfg.register_option("multisampling_level", 0);
    mycfg.register_option("use_multisampling", false);
    mycfg.register_option("bloom_enabled", false); // TODO(tj): remove
    mycfg.register_option("hdr_enabled", false);   // TODO(tj): remove
    mycfg.register_option("hint_multisampling", 0);
    mycfg.register_option("hint_fog", 0);
    mycfg.register_option("hint_mipmap", 0);
    mycfg.register_option("hint_texture_compression", 0);
    mycfg.register_option("vsync", false);
    mycfg.register_option("water_detail", 128);
    mycfg.register_option("wave_fft_res", 128);
    mycfg.register_option("wave_phases", 256);
    mycfg.register_option("wavetile_length", 256.0F);
    mycfg.register_option("wave_tidecycle_time", 10.24F);
    mycfg.register_option("usex86sse", true);
    mycfg.register_option("language", 0);
    mycfg.register_option("cpucores", 1);
    mycfg.register_option("terrain_texture_resolution", 0.1F);
    mycfg.register_option("terrain_detail", 1);

    mycfg.register_key(key_names[unsigned(key_command::ZOOM_MAP)].name, key_code::PLUS, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::UNZOOM_MAP)].name, key_code::MINUS, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_GAUGES_SCREEN)].name, key_code::F1, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_PERISCOPE_SCREEN)].name, key_code::F2, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_UZO_SCREEN)].name, key_code::F3, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_BRIDGE_SCREEN)].name, key_code::F4, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_MAP_SCREEN)].name, key_code::F5, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_TORPEDO_SCREEN)].name, key_code::F6, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_DAMAGE_CONTROL_SCREEN)].name, key_code::F7, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_LOGBOOK_SCREEN)].name, key_code::F8, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_SUCCESS_RECORDS_SCREEN)].name, key_code::F9, key_mod::none);
    mycfg.register_key(
        key_names[unsigned(key_command::SHOW_FREEVIEW_SCREEN)].name, key_code::F10, key_mod::shift | key_mod::ctrl);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_TDC_SCREEN)].name, key_code::F10, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_TDC2_SCREEN)].name, key_code::F11, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_TORPSETUP_SCREEN)].name, key_code::F12, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_VALVES_SCREEN)].name, key_code::F1, key_mod::ctrl);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_LEFT)].name, key_code::LEFT, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_HARD_LEFT)].name, key_code::LEFT, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_RIGHT)].name, key_code::RIGHT, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_HARD_RIGHT)].name, key_code::RIGHT, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_UP)].name, key_code::UP, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_HARD_UP)].name, key_code::UP, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_DOWN)].name, key_code::DOWN, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::RUDDER_HARD_DOWN)].name, key_code::DOWN, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::CENTER_RUDDERS)].name, key_code::RETURN, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_LISTEN)].name, key_code::_1, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_SLOW)].name, key_code::_2, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_HALF)].name, key_code::_3, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_FULL)].name, key_code::_4, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_FLANK)].name, key_code::_5, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_STOP)].name, key_code::_6, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_REVERSE)].name, key_code::_7, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_REVERSEHALF)].name, key_code::_8, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::THROTTLE_REVERSEFULL)].name, key_code::_9, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_1)].name, key_code::_1, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_2)].name, key_code::_2, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_3)].name, key_code::_3, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_4)].name, key_code::_4, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_5)].name, key_code::_5, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TUBE_6)].name, key_code::_6, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::SELECT_TARGET)].name, key_code::SPACE, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SCOPE_UP_DOWN)].name, key_code::_0, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::CRASH_DIVE)].name, key_code::c, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::GO_TO_SNORKEL_DEPTH)].name, key_code::d, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TOGGLE_SNORKEL)].name, key_code::f, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SET_HEADING_TO_VIEW)].name, key_code::h, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::IDENTIFY_TARGET)].name, key_code::i, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::GO_TO_PERISCOPE_DEPTH)].name, key_code::p, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::GO_TO_SURFACE)].name, key_code::s, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_TORPEDO)].name, key_code::t, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SET_VIEW_TO_HEADING)].name, key_code::v, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TOGGLE_ZOOM_OF_VIEW)].name, key_code::y, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TURN_VIEW_LEFT)].name, key_code::COMMA, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TURN_VIEW_LEFT_FAST)].name, key_code::COMMA, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::TURN_VIEW_RIGHT)].name, key_code::PERIOD, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TURN_VIEW_RIGHT_FAST)].name, key_code::PERIOD, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::TIME_SCALE_UP)].name, key_code::KP_PLUS, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TIME_SCALE_DOWN)].name, key_code::KP_MINUS, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::FIRE_DECK_GUN)].name, key_code::g, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TOGGLE_RELATIVE_BEARING)].name, key_code::r, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TOGGLE_MAN_DECK_GUN)].name, key_code::g, key_mod::shift);
    mycfg.register_key(key_names[unsigned(key_command::TOGGLE_POPUP)].name, key_code::TAB, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::SHOW_TORPEDO_CAMERA)].name, key_code::k, key_mod::none);
    mycfg.register_key(key_names[unsigned(key_command::TAKE_SCREENSHOT)].name, key_code::PRINTSCREEN, key_mod::none);

    // mycfg.register_option("invert_mouse", false);
    // mycfg.register_option("ocean_res_x", 128);
    // mycfg.register_option("ocean_res_y", 128);
    // mycfg.register_option("", );

    // randomize
    srand(time(nullptr));

    // read data files
    data_file();

    // make sure the default values are stored if there is no config file,
    // and make sure all registered values are stored in it
    if (is_file(configdirectory + "config")) {
        mycfg.load(configdirectory + "config");
    } else {
        if (!is_directory(configdirectory)) {
            make_dir(configdirectory);
        }
        mycfg.save(configdirectory + "config");
    }

    //	mycfg.save("./testconf");
    glsl_shader::enable_hqsfx = cfg::instance().getb("use_hqsfx");

    // read screen resolution from config file if no override has been set by
    // command line parameters
    if (res_x == 0) {
        res_x = cfg::instance().geti("screen_res_x");
        res_y = cfg::instance().geti("screen_res_y");
    }
    // Read language from options-file
    if (!override_lang) {
        texts::set_language(cfg::instance().geti("language"));
    }
    // fixme: also allow 1280x1024, set up gl viewport for 4:3 display
    // with black borders at top/bottom (height 2*32pixels)
    // weather conditions and earth curvature allow 30km sight at maximum.
    system_interface::parameters params;
    params.near_z                      = 1.0;
    params.far_z                       = 30000.0 + 500.0;
    params.resolution                  = {res_x, res_y};
    params.resolution2d                = {1024, 768};
    params.window_caption              = texts::get(7);
    params.fullscreen                  = fullscreen;
    params.vertical_sync               = mycfg.getb("vsync");
    texture::use_compressed_textures   = mycfg.getb("use_compressed_textures");
    texture::use_anisotropic_filtering = mycfg.getb("use_ani_filtering");
    texture::anisotropic_level         = mycfg.getf("anisotropic_level");
    system_interface::create_instance(params);
    SYS().set_screenshot_directory(savegamedirectory);

    reset_loading_screen();

    // --------------------------------------------------------------------------------
    // check for shader/glsl support

#ifndef WIN32
    tests gltest = tests();
    int problems = gltest.do_gl_tests();
    std::string warnings;

    // check for fatal errors
    if (0 == problems) {
        // non fatal errors
        if (!gltest.warn_log.empty()) {
            std::ostringstream str_problems;
            str_problems << "Warnings (missing functionality):\n"; // TODO(tj): use
                                                                   // texts::get ?

            for (const auto& it : gltest.warn_log) {
                str_problems << "  " << it.c_str() << "\n";
            }
            warnings = str_problems.str();
        }

        // fatal errors detected...
        if (!gltest.error_log.empty()) {
            std::ostringstream str_problems;

            str_problems << "Dangerdeep cannot run on this machine because the "
                            "following tests failed:\n\n";

            for (const auto& it : gltest.error_log) {
                str_problems << "  " << it.c_str() << "\n";
            }

            str_problems << "\nPress any key to quit.";
            str_problems << "\n\n" << warnings;

            glClearColor(0, 0, 1, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            SYS().prepare_2d_drawing();
            font_arial->print(0, 0, str_problems.str());
            SYS().unprepare_2d_drawing();
            SYS().finish_frame();
            bool quit = false;
            input_event_handler_custom ic;
            ic.set_handler([&quit](const input_event_handler::mouse_click_data& mc) {
                if (mc.up()) {
                    quit = true;
                }
                return true;
            });
            ic.set_handler([&quit](const input_event_handler::key_data& kd) {
                if (kd.keycode == key_code::ESCAPE) {
                    quit = true;
                }
                return true;
            });
            while (!quit) {
                quit = SYS().finish_frame();
            }
            // throw system_interface::quit_exception(-1);
        }
    }
#endif // WIN32
    // --------------------------------------------------------------------------------

    log_info("Danger from the Deep");
    log_info("Copyright (C) 2003-2011  Thorsten Jordan, Luis Barrancos and others.");
    log_info("Version " << get_program_version());

    GLfloat lambient[4]  = {0.1, 0.1, 0.1, 1};
    GLfloat ldiffuse[4]  = {1, 1, 1, 1};
    GLfloat lposition[4] = {0, 0, 1, 0};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ldiffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lposition);
    glEnable(GL_LIGHT0);

    // create and start thread for music handling.
    music::create_instance(use_sound);
    music::instance().set_sound_dir(get_sound_dir());

    music::instance().append_track("ImInTheMood.ogg");
    music::instance().append_track("Betty_Roche-Trouble_Trouble.ogg");
    music::instance().append_track("theme.ogg");
    music::instance().append_track("Auf_Feindfahrt_fast.ogg");
    music::instance().append_track("outside_underwater.ogg");
    music::instance().append_track("Auf_Feindfahrt_environmental.ogg");
    music::instance().append_track("loopable_seasurface.ogg");
    music::instance().append_track("loopable_seasurface_badweather.ogg");
    music::instance().append_track("Auf_Feindfahrt.ogg");

    add_loading_screen("Music list loaded");
    // music::instance().set_playback_mode(music::playback_mode::shuffle_track);
    music::instance().play();

    widget::set_theme(std::make_unique<widget::theme>(
        "widgetelements_menu.png",
        "widgeticons_menu.png",
        &*font_typenr16,
        color(182, 146, 137),
        color(240, 217, 127) /*color(222, 208, 195)*/,
        color(92, 72, 68)));

    std::unique_ptr<texture> metalbackground(new texture(get_image_dir() + "metalbackground.jpg"));

    // try to make directories if they do not exist
    try {
        directory savegamedir(savegamedirectory);
    }
    catch (exception& e) {
        if (!make_dir(savegamedirectory)) {
            THROW(error, "could not create save game directory.");
        }
    }

    try {
        directory configdir(configdirectory);
    }
    catch (exception& e) {
        if (!make_dir(configdirectory)) {
            THROW(error, "could not create config directory.");
        }
    }

    try {
        directory highscoredir(highscoredirectory);
    }
    catch (exception& e) {
        if (!make_dir(highscoredirectory)) {
            THROW(error, "could not create save game directory.");
        }
    }

    // read highscores
    if (!file_exists(highscoredirectory + HSL_MISSION_NAME)) {
        highscorelist().save(highscoredirectory + HSL_MISSION_NAME);
    }

    if (!file_exists(highscoredirectory + HSL_CAREER_NAME)) {
        highscorelist().save(highscoredirectory + HSL_CAREER_NAME);
    }

    hsl_mission = highscorelist(highscoredirectory + HSL_MISSION_NAME);
    hsl_career  = highscorelist(highscoredirectory + HSL_CAREER_NAME);

    // check if there was a mission given at the command line, or editor more
    // etc.
    if (runeditor) {
        // reset loading screen here to show user we are doing something
        reset_loading_screen();
        run_game_editor(unique_ptr<game>(new game_editor(editor_start_date)));
    } else if (cmdmissionfilename.length() > 0) {
        // fixme: check here that the file exists or tinyxml faults with a
        // embarassing error message
        unique_ptr<game> gm;
        bool ok = true;
        try {
            gm = std::make_unique<game>(get_mission_dir() + cmdmissionfilename);
        }
        catch (error& e) {
            log_warning("error loading mission: " << e.what());
            // fixme: show dialogue!
            ok = false;
        }
        if (ok) {
            // reset loading screen here to show user we are doing something
            reset_loading_screen();
            run_game(std::move(gm));
        }
    } else {
        int retval = 1;
        do { // loop until menu is closed.
            // main menu
            widget w(0, 0, 1024, 768, "", "titlebackgr.jpg");

#ifndef WIN32
            // opengl warnings
            if (0 != warnings.length()) {
                w.add_child<widget_text>(20, 20, 0, 0, warnings);
            }
#endif // WIN32

            // display version #
            w.add_child<widget_text>(5, 768 - 30, 0, 0, get_program_version());

            auto wm = w.add_child<widget_menu>(0, 0, 400, 40, texts::get(104));

            wm->set_entry_spacing(8);
            wm->add_entry(texts::get(21), std::make_unique<widget_caller_button<>>(menu_single_mission));

            // wm->add_entry(texts::get(23),
            // std::make_unique<widget_caller_button<>>(menu_notimplemented /*
            // career menu */));
            wm->add_entry(texts::get(222), std::make_unique<widget_caller_button<>>(menu_mission_editor));
            wm->add_entry(texts::get(24), std::make_unique<widget_caller_button<>>(menu_show_vessels));
            wm->add_entry(texts::get(25), std::make_unique<widget_caller_button<>>(show_halloffame_mission));
            wm->add_entry(texts::get(213), std::make_unique<widget_caller_button<>>(show_credits));
            wm->add_entry(
                texts::get(26), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(1); }, w));
            wm->add_entry(texts::get(29), std::make_unique<widget_caller_button<>>(menu_options));
            wm->add_entry(
                texts::get(30), std::make_unique<widget_caller_button<widget&>>([](auto& w) { w.close(0); }, w));
            wm->align(0, 0);
            retval = widget::run(w, 0, false);
            if (retval == 1) {
                menu_select_language();
            }
        } while (retval != 0);
    }

    music::instance().stop(1000);

    hsl_mission.save(highscoredirectory + HSL_MISSION_NAME);
    hsl_career.save(highscoredirectory + HSL_CAREER_NAME);
    mycfg.save(configdirectory + "config");

    widget::set_theme(unique_ptr<widget::theme>()); // clear allocated theme
    widget::image_store.cleanup();

    return 0;
}
