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

// OpenGL based widgets
// (C)+(W) by Thorsten Jordan. See LICENSE

#include "widget.hpp"

#include "datadirs.hpp"
#include "filehelper.hpp"
#include "model.hpp"
#include "primitives.hpp"
#include "system_interface.hpp"
#include "texture.hpp"

#include <SDL_image.h>
#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

std::unique_ptr<widget::theme> widget::globaltheme;
widget::ptr widget::focussed;
widget::ptr widget::mouseover;
int widget::oldmx = 0;
int widget::oldmy = 0;
int widget::oldmb = 0;
/// Widget related image store. We don't use the global store here, as background images for widgets are not used
/// elsewhere, and because of dependencies.
shared_object_store<image> widget::image_store(get_image_dir());

std::vector<widget::ptr> widget::widget_stack;

std::string widget::text_ok     = "Ok"; // fixme: let user read them from text database and set it here!
std::string widget::text_cancel = "Cancel";

/* fixme: when new widget is opened that fills the whole screen, unref
   the images of previously open widgets to avoid wasting system and/or
   video ram!
*/

auto widget::theme::frame_size() const -> int
{
    return frame[0]->get_height();
}

auto widget::theme::icon_size() const -> int
{
    return icons[0]->get_height();
}

widget::theme::theme(
    const char* elements_filename,
    const char* icons_filename,
    const font* fnt,
    color tc,
    color tsc,
    color tdc)
    : myfont(fnt)
    , textcol(tc)
    , textselectcol(tsc)
    , textdisabledcol(tdc)
{
    {
        sdl_image tmp(get_texture_dir() + elements_filename);
        int fw  = tmp->h;
        backg   = std::make_unique<texture>(tmp, 0, 0, fw, fw);
        skbackg = std::make_unique<texture>(tmp, fw, 0, fw, fw);
        for (int i = 0; i < 8; ++i) {
            frame[i] = std::make_unique<texture>(tmp, (i + 2) * fw, 0, fw, fw);
        }
        for (int i = 0; i < 8; ++i) {
            frameinv[i] = std::make_unique<texture>(tmp, (i + 10) * fw, 0, fw, fw);
        }
        sbarbackg = std::make_unique<texture>(tmp, (2 + 2 * 8) * fw, 0, fw, fw);
        sbarsurf  = std::make_unique<texture>(tmp, (2 + 2 * 8 + 1) * fw, 0, fw, fw);
    }
    {
        sdl_image tmp(get_texture_dir() + icons_filename);
        int fw = tmp->h;
        for (int i = 0; i < 4; ++i) {
            icons[i] = std::make_unique<texture>(tmp, i * fw, 0, fw, fw);
        }
    }
}

auto widget::replace_theme(std::unique_ptr<widget::theme> t) -> std::unique_ptr<widget::theme>
{
    std::unique_ptr<theme> r = std::move(globaltheme);
    globaltheme              = std::move(t);
    return r;
}

widget::widget(int x, int y, int w, int h, std::string text_, const std::string& backgrimg)
    : pos(x, y)
    , size(w, h)
    , text(std::move(text_))
    , background_image_name(backgrimg)
    , background(image_store.ref(backgrimg)) // fixme das könnte das problem sein, wenn name leer!
{
    // note: when backgrimg is empty, the cache automatically returns a NULL
    // pointer.
}

widget::~widget()
{
    if (focussed == this) {
        focussed = nullptr;
    }
    if (mouseover == this) {
        mouseover = nullptr;
    }
}

void widget::add_child_near_last_child_generic(std::unique_ptr<widget>&& w, int distance, unsigned direction)
{
    if (distance < 0) {
        distance = globaltheme->frame_size() * -distance;
    }
    if (children.empty()) {
        // place near top of window, handle title bar
        vector2i cpos = vector2i(distance, distance) + get_pos();
        if (text.length() > 0) {
            cpos.y += globaltheme->frame_size() * 2 + globaltheme->myfont->get_height();
        }
        w->move_pos(cpos);
        w->set_parent(this);
        children.push_back(std::move(w));
        return;
    }
    const auto& lc = children.back();
    vector2i lcp   = lc->get_pos();
    switch (direction) {
        case 0: // above
            lcp.y -= distance + w->get_size().y;
            break;
        case 1: // right
            lcp.x += distance + lc->get_size().x;
            break;
        case 2: // below
        default:
            lcp.y += distance + lc->get_size().y;
            break;
        case 3: // left
            lcp.x -= distance + w->get_size().x;
            break;
    }
    w->move_pos(lcp);
    w->set_parent(this);
    children.push_back(std::move(w));
}

void widget::clip_to_children_area()
{
    // no children? nothing to clip.
    if (children.empty()) {
        return;
    }
    auto it       = children.begin();
    vector2i pmin = (*it)->get_pos();
    vector2i pmax = (*it)->get_pos() + (*it)->get_size();
    for (++it; it != children.end(); ++it) {
        pmin = pmin.min((*it)->get_pos());
        pmax = pmax.max((*it)->get_pos() + (*it)->get_size());
    }
    // now add border size.
    int bs = globaltheme->frame_size() * 2;
    pmin.x -= bs;
    pmin.y -= bs;
    pmax.x += bs;
    pmax.y += bs;
    // now handle possible title bar
    if (text.length() > 0) {
        pmin.y -= globaltheme->frame_size() * 2 + globaltheme->myfont->get_height();
    }
    // change size
    pos = pmin; // do not call set_pos/move_pos, as that moves children as well.
    set_size(pmax - pmin);
}

void widget::move_pos(const vector2i& p)
{
    pos += p;
    for (auto& it : children) {
        it->move_pos(p);
    }
}

void widget::align(int h, int v)
{
    vector2i sz;
    if (auto p = get_parent(); p) {
        sz = p->get_size();
    } else {
        sz = vector2i(SYS().get_res_2d());
    }
    set_pos(vector2i(
        (h < 0) ? 0 : ((h > 0) ? (sz.x - size.x) : ((sz.x - size.x) / 2)),
        (v < 0) ? 0 : ((v > 0) ? (sz.y - size.y) : ((sz.y - size.y) / 2))));
}

void widget::draw() const
{
    redrawme   = false;
    vector2i p = get_pos();
    draw_area(p.x, p.y, size.x, size.y, /*fixme: replace by property?*/ true);
    int fw = globaltheme->frame_size();
    // draw titlebar only when there is a title
    if (text.length() > 0) {
        draw_rect(p.x + fw, p.y + fw, size.x - 2 * fw, globaltheme->myfont->get_height(), false);
        color tcol = is_enabled() ? globaltheme->textcol : globaltheme->textdisabledcol;
        globaltheme->myfont->print_hc(p.x + size.x / 2, p.y + globaltheme->frame_size(), text, tcol, true);
    }
    // fixme: if childs have the same size as parent, don't draw parent?
    // i.e. do not use stacking then...
    // maybe make chooseable via argument of run()
    for (const auto& child : children) {
        child->draw();
    }
}

auto widget::compute_focus(int mx, int my) -> bool
{
    focussed.reset();
    // if the widget is disabled, it can't get the focus and neither one of its
    // children.
    if (!is_enabled()) {
        return false;
    }
    if (is_mouse_over(mx, my)) {
        for (auto& child : children) {
            if (child->compute_focus(mx, my)) {
                return true;
            }
        }
        focussed = this;
        return true;
    }
    return false;
}

auto widget::compute_mouseover(int mx, int my) -> bool
{
    mouseover.reset();
    if (is_mouse_over(mx, my)) {
        for (auto& child : children) {
            if (child->compute_mouseover(mx, my)) {
                return true;
            }
        }
        mouseover = this;
        return true;
    }
    return false;
}

auto widget::is_enabled() const -> bool
{
    bool e = enabled;
    if (auto p = get_parent(); p) {
        e = e && p->is_enabled();
    }
    return e;
}

void widget::enable()
{
    enabled = true;
}

void widget::disable()
{
    enabled = false;
}

void widget::redraw()
{
    redrawme = true;
    if (auto p = get_parent(); p) {
        p->redraw();
    }
}

void widget::on_key(key_code kc, key_mod km)
{
    // we can't handle it, so pass it to the parent
    if (auto p = get_parent(); p) {
        p->on_key(kc, km);
    }
}

void widget::on_text(const std::string& t)
{
    // we can't handle it, so pass it to the parent
    if (auto p = get_parent(); p) {
        p->on_text(t);
    }
}

void widget::on_wheel(input_action wd)
{
    // we can't handle it, so pass it to the parent
    if (auto p = get_parent(); p) {
        p->on_wheel(wd);
    }
}

void widget::draw_frame(int x, int y, int w, int h, bool out)
{
    auto* frelem = (out ? globaltheme->frame : globaltheme->frameinv);
    int fw       = globaltheme->frame_size();
    frelem[0]->draw(x, y);
    frelem[1]->draw(x + fw, y, w - 2 * fw, fw);
    frelem[2]->draw(x + w - fw, y);
    frelem[3]->draw(x + w - fw, y + fw, fw, h - 2 * fw);
    frelem[4]->draw(x + w - fw, y + h - fw);
    frelem[5]->draw(x + fw, y + h - fw, w - 2 * fw, fw);
    frelem[6]->draw(x, y + h - fw);
    frelem[7]->draw(x, y + fw, fw, h - 2 * fw);
}

void widget::draw_rect(int x, int y, int w, int h, bool out)
{
    if (out) {
        globaltheme->backg->draw(x, y, w, h);
    } else {
        globaltheme->skbackg->draw(x, y, w, h);
    }
}

void widget::draw_line(int x1, int y1, int x2, int y2)
{
    primitives::line(vector2f(x1, y1), vector2f(x2, y2), globaltheme->textcol);
}

void widget::draw_area(int x, int y, int w, int h, bool out) const
{
    int fw = globaltheme->frame_size();
    draw_rect(x + fw, y + fw, w - 2 * fw, h - 2 * fw, out);
    if (background != nullptr) {
        int bw = int(background->get_width());
        int bh = int(background->get_height());
        background->draw(x + (w - bw) / 2, y + (h - bh) / 2);
    } else if (background_color != color()) {
        primitives::quad(vector2f(x, y), vector2f(x + w, y + h), background_color).render();
    }
    draw_frame(x, y, w, h, out);
}

void widget::draw_area_col(int x, int y, int w, int h, bool out, color c) const
{
    primitives::quad(vector2f(x, y + h), vector2f(x + w, y), c);
    draw_frame(x, y, w, h, out);
}

auto widget::is_mouse_over(int mx, int my) const -> bool
{
    vector2i p = get_pos();
    return (mx >= p.x && my >= p.y && mx < p.x + size.x && my < p.y + size.y);
}

auto widget::create_dialogue_ok(const std::string& title, const std::string& text, int w, int h)
    -> std::unique_ptr<widget>
{
    unsigned res_x = SYS().get_res_x_2d();
    unsigned res_y = SYS().get_res_y_2d();
    int x          = w != 0 ? (res_x - w) / 2 : res_x / 4;
    int y          = h != 0 ? (res_y - h) / 2 : res_y / 4;
    if (w == 0) {
        w = res_x / 2;
    }
    if (h == 0) {
        h = res_y / 2;
    }
    auto wi(std::make_unique<widget>(x, y, w, h, title));
    wi->add_child<widget_text>(32, 64, w - 64, h - 128, text);
    int fw   = globaltheme->frame_size();
    int fh   = int(globaltheme->myfont->get_height());
    int butw = 4 * fh + 2 * fw;
    wi->add_child<widget_caller_button<widget&>>(
        w / 2 - butw / 2, h - 64, butw, fh + 4 * fw, text_ok, [](auto& w) { w.close(1); }, *wi);
    return wi;
}

auto widget::create_dialogue_ok_cancel(const std::string& title, const std::string& text, int w, int h)
    -> std::unique_ptr<widget>
{
    unsigned res_x = SYS().get_res_x_2d();
    unsigned res_y = SYS().get_res_y_2d();
    int x          = w != 0 ? (res_x - w) / 2 : res_x / 4;
    int y          = h != 0 ? (res_y - h) / 2 : res_y / 4;
    if (w == 0) {
        w = res_x / 2;
    }
    if (h == 0) {
        h = res_y / 2;
    }
    auto wi(std::make_unique<widget>(x, y, w, h, title));
    wi->add_child<widget_text>(32, 64, w - 64, h - 128, text);
    int fw   = globaltheme->frame_size();
    int fh   = int(globaltheme->myfont->get_height());
    int butw = 4 * fh + 2 * fw;
    wi->add_child<widget_caller_button<widget&>>(
        w / 4 - butw / 2, h - 64, butw, fh + 4 * fw, text_ok, [](auto& w) { w.close(1); }, *wi);
    wi->add_child<widget_caller_button<widget&>>(
        3 * w / 4 - butw / 2, h - 64, butw, fh + 4 * fw, text_cancel, [](auto& w) { w.close(0); }, *wi);
    return wi;
}

auto widget::run(widget& w, unsigned timeout, bool do_stacking) -> int
{
    glClearColor(0, 0, 0, 0);
    auto myparent = w.get_parent(); // store parent info and unlink chain to parent
    w.set_parent({});
    if (myparent) {
        myparent->disable();
    }
    w.closeme = false;
    // we should encapsulate the code from here in a try call, to make changes
    // reversible on error. but in case of errors, we can't handle them well
    // here, so no matter.
    widget_stack.push_back(&w);
    unsigned endtime = SYS().millisec() + timeout;
    focussed         = w;
    // draw it initially
    w.redraw();
    auto event_handler = std::make_shared<input_event_handler_custom>();
    event_handler->set_handler([&w](const input_event_handler::key_data& k) { return handle_key_event(w, k); });
    event_handler->set_handler(
        [&w](const input_event_handler::mouse_click_data& m) { return handle_mouse_button_event(w, m); });
    event_handler->set_handler(
        [&w](const input_event_handler::mouse_motion_data& m) { return handle_mouse_motion_event(w, m); });
    event_handler->set_handler(
        [&w](const input_event_handler::mouse_wheel_data& m) { return handle_mouse_wheel_event(w, m); });
    event_handler->set_handler([&w](const std::string& t) { return handle_text_input_event(w, t); });
    SYS().add_input_event_handler(event_handler);
    while (!w.was_closed()) {
        unsigned time = SYS().millisec();
        if (timeout != 0 && time > endtime) {
            break;
        }

        if (w.redrawme) {
            glClear(GL_COLOR_BUFFER_BIT);
            SYS().prepare_2d_drawing();
            if (do_stacking) {
                for (auto& it : widget_stack) {
                    it->draw();
                }
            } else {
                w.draw();
            }
            SYS().unprepare_2d_drawing();
        }
        SYS().finish_frame(); // fixme how to get information that program should quit out of here?!
    }
    widget_stack.pop_back();
    if (myparent) {
        myparent->enable();
    }
    w.set_parent(myparent);
    return w.retval;
}

auto widget::handle_key_event(widget& w, const input_event_handler::key_data& k) -> bool
{
    // any key press or release could potentially change the scene, so redraw
    w.redraw();
    if (k.down() && focussed && focussed->is_enabled()) {
        focussed->on_key(k.keycode, k.mod);
        return true;
    }
    return false;
}

auto widget::handle_mouse_button_event(widget& w, const input_event_handler::mouse_click_data& m) -> bool
{
    // any click or release could potentially change the scene, so redraw
    w.redraw();
    if (m.down()) {
        w.compute_focus(m.position_2d);
        if (focussed) {
            focussed->on_click(m.position_2d, m.button);
            return true;
        }
    } else if (m.up() && m.left()) {
        if (focussed) {
            focussed->on_release();
            return true;
        }
    }
    return false;
}

auto widget::handle_mouse_motion_event(widget& w, const input_event_handler::mouse_motion_data& m) -> bool
{
    // any mouse motion with pressed keys could potentially change the scene, so
    // redraw, without keys pressed a redraw is needed when mouseover changes
    auto current_mouseover = mouseover;
    w.compute_mouseover(m.position_2d);
    if (current_mouseover != mouseover) {
        w.redraw();
    }
    if (m.buttons_pressed.any()) {
        w.redraw();
        if (focussed) {
            focussed->on_drag(m.position_2d, m.relative_motion_2d, m.buttons_pressed);
            return true;
        }
    }
    return false;
}

auto widget::handle_mouse_wheel_event(widget& w, const input_event_handler::mouse_wheel_data& m) -> bool
{
    // any mouse wheel event could potentially change the scene, so redraw
    w.redraw();
    if (focussed) {
        focussed->on_wheel(m.action);
        return true;
    }
    return false;
}

auto widget::handle_text_input_event(widget& w, const std::string& t) -> bool
{
    // any text input could potentially change the scene, so redraw
    w.redraw();
    if (focussed && focussed->is_enabled()) {
        focussed->on_text(t);
        return true;
    }
    return false;
}

void widget::close(int val)
{
    retval  = val;
    closeme = true;
}

void widget::open()
{
    retval  = -1;
    closeme = false;
}

widget_menu::widget_menu(int x, int y, int w, int h, const std::string& text_, bool horizontal_)
    : widget(x, y, 0, 0, text_)
    , horizontal(horizontal_)
    , entryw(w)
    , entryh(h)
    , entryspacing(16)
{
    if (text.length() > 0) {
        size.x = entryw;
        size.y = entryh;
    }
}

auto widget_menu::add_entry(const std::string& s, std::unique_ptr<widget_button>&& wb) -> widget_button::ptr
{
    int x;
    int y;
    int w;
    int h;
    unsigned mult = children.size();
    if (text.length() > 0) {
        ++mult;
    }
    if (horizontal) {
        x = mult * (entryw + entryspacing);
        y = 0;
        w = entryw;
        h = entryh;
        size.x += entryw;
        size.y = entryh;
        if (mult > 0) {
            size.x += entryspacing;
        }
    } else {
        x      = 0;
        y      = mult * (entryh + entryspacing);
        w      = entryw;
        h      = entryh;
        size.x = entryw;
        size.y += entryh;
        if (mult > 0) {
            size.y += entryspacing;
        }
    }
    if (wb != nullptr) {
        wb->set_size(vector2i(w, h));
        wb->set_pos(vector2i(x, y));
        wb->set_text(s);
    } else {
        wb = std::make_unique<widget_button>(x, y, w, h, s);
    }
    wb->set_parent({});
    wb->move_pos(pos);
    widget_button::ptr result = wb;
    children.push_back(std::move(wb));
    return result;
}

auto widget_menu::get_selected() const -> int
{
    int sel = 0;
    for (const auto& child : children) {
        if ((dynamic_cast<widget_button*>(child.get()))->is_pressed()) {
            return sel;
        }
        ++sel;
    }
    return -1;
}

void widget_menu::draw() const
{
    vector2i p = get_pos();
    // draw title bar if there is text
    if (text.length() > 0) {
        draw_area(p.x, p.y, entryw, entryh, true);
        draw_area(p.x, p.y, entryw, entryh, false);
        globaltheme->myfont->print_c(p.x + entryw / 2, p.y + entryh / 2, text, globaltheme->textcol, true);
    }
    for (const auto& child : children) {
        child->draw();
    }
}

void widget_menu::adjust_buttons(unsigned totalsize)
{
    // fixme: if there's not enough space for all buttons, nothing is adjusted.
    // thats bad - but one can't do anything anyway
    if (horizontal) {
        int textw   = 0;
        int fw      = globaltheme->frame_size();
        int nrbut   = int(children.size());
        int longest = 0;
        for (auto& child : children) {
            int w = int(globaltheme->myfont->get_size(child->get_text()).x);
            textw += w;
            if (w > longest) {
                longest = w;
            }
        }
        int framew    = 2 * fw * nrbut;
        int spaceleft = int(totalsize) - ((longest + 2 * fw) * nrbut + framew + (nrbut - 1) * entryspacing);
        if (spaceleft > 0) { // equi distant buttons
            size.x     = int(totalsize);
            int spc    = spaceleft / nrbut;
            int runpos = 0;
            for (auto& child : children) {
                int mytextw = longest + 2 * fw;
                child->set_pos(pos + vector2i(runpos, 0));
                child->set_size(vector2i(mytextw + 2 * fw + spc, entryh));
                runpos += mytextw + 2 * fw + spc + entryspacing;
            }
        } else {
            spaceleft = int(totalsize) - (textw + framew + (nrbut - 1) * entryspacing);
            if (spaceleft > 0) { // space left?
                size.x     = int(totalsize);
                int spc    = spaceleft / nrbut;
                int runpos = 0;
                for (auto& child : children) {
                    int mytextw = int(globaltheme->myfont->get_size(child->get_text()).x);
                    child->set_pos(pos + vector2i(runpos, 0));
                    child->set_size(vector2i(mytextw + 2 * fw + spc, entryh));
                    runpos += mytextw + 2 * fw + spc + entryspacing;
                }
            }
        }
    } else {
        // fixme: todo
    }
}

void widget_text::draw() const
{
    vector2i p = get_pos();
    if (sunken) {
        draw_area(p.x, p.y, size.x, size.y, false);
        int fw = globaltheme->frame_size();
        globaltheme->myfont->print_wrapped(
            p.x + 2 * fw, p.y + 2 * fw, size.x - 4 * fw, 0, text, globaltheme->textcol, true);
    } else {
        globaltheme->myfont->print_wrapped(p.x, p.y, size.x, 0, text, globaltheme->textcol, true);
    }
}

void widget_text::set_text_and_resize(const std::string& s)
{
    vector2i sz = globaltheme->myfont->get_size(s);
    if (sunken) {
        int fw = globaltheme->frame_size();
        sz.x += 4 * fw;
        sz.y += 4 * fw;
    }
    set_size(sz);
    set_text(s);
}

void widget_checkbox::draw() const
{
    vector2i p = get_pos();
    draw_rect(p.x, p.y, size.x, size.y, true);
    int fw   = globaltheme->frame_size();
    int icni = checked ? 3 : 2;
    globaltheme->icons[icni]->draw(p.x, p.y + (get_size().y - globaltheme->icons[icni]->get_height()) / 2);
    globaltheme->myfont->print_vc(
        p.x + globaltheme->icons[icni]->get_width() + fw,
        p.y + size.y / 2,
        text,
        is_enabled() ? globaltheme->textcol : globaltheme->textdisabledcol,
        true);
}

void widget_checkbox::on_click(vector2i /*position*/, mouse_button /*btn*/)
{
    checked = !checked;
    on_change();
}

void widget_button::draw() const
{
    vector2i p = get_pos();
    bool mover = is_enabled() && mouseover == this;
    draw_area(p.x, p.y, size.x, size.y, !mover);
    color col =
        (is_enabled() ? (mouseover == this ? globaltheme->textselectcol : globaltheme->textcol)
                      : globaltheme->textdisabledcol);
    globaltheme->myfont->print_c(p.x + size.x / 2, p.y + size.y / 2, text, col, true);
}

void widget_button::on_click(vector2i /*position*/, mouse_button /*btn*/)
{
    pressed = true;
    on_change();
}

void widget_button::on_release()
{
    pressed = false;
    on_change();
}

widget_scrollbar::widget_scrollbar(int x, int y, int w, int h)
    : widget(x, y, w, h, "")
    , scrollbarpos(0)
    , scrollbarmaxpos(0)
{
}

void widget_scrollbar::set_nr_of_positions(unsigned s)
{
    scrollbarmaxpos = s;
    if (scrollbarmaxpos == 0) {
        scrollbarpos = 0;
    } else if (scrollbarpos >= scrollbarmaxpos) {
        scrollbarpos = scrollbarmaxpos - 1;
    }
    compute_scrollbarpixelpos();
}

auto widget_scrollbar::get_current_position() const -> unsigned
{
    return scrollbarpos;
}

void widget_scrollbar::set_current_position(unsigned p)
{
    if (p < scrollbarmaxpos) {
        scrollbarpos = p;
        compute_scrollbarpixelpos();
    }
}

auto widget_scrollbar::get_max_scrollbarsize() const -> unsigned
{
    return size.y - globaltheme->icons[0]->get_height() - globaltheme->icons[1]->get_height()
           - 4 * globaltheme->frame_size();
}

auto widget_scrollbar::get_scrollbarsize() const -> unsigned
{
    unsigned msbs = get_max_scrollbarsize();
    if (scrollbarmaxpos == 0) {
        return msbs;
    }
    return msbs / 2 + msbs / (1 + scrollbarmaxpos);
}

void widget_scrollbar::compute_scrollbarpixelpos()
{
    if (scrollbarmaxpos <= 1) {
        scrollbarpixelpos = 0;
    } else {
        scrollbarpixelpos = (get_max_scrollbarsize() - get_scrollbarsize()) * scrollbarpos / (scrollbarmaxpos - 1);
    }
}

void widget_scrollbar::draw_area(int x, int y, int w, int h, bool out) const
{
    /* this is:  draw_rect_scrollbar(x+fw, y+fw, w-2*fw, h-2*fw, out); */
    if (out) {
        globaltheme->sbarsurf->draw(x, y, w, h);
    } else {
        globaltheme->sbarbackg->draw(x, y, w, h);
    }

    // scrollbar has no background ...

    draw_frame(x, y, w, h, out);
}

void widget_scrollbar::draw() const
{
    vector2i p = get_pos();
    int fw     = globaltheme->frame_size();
    draw_frame(
        p.x, p.y, globaltheme->icons[0]->get_width() + 2 * fw, globaltheme->icons[0]->get_height() + 2 * fw, true);
    draw_frame(
        p.x,
        p.y + size.y - globaltheme->icons[1]->get_height() - 2 * fw,
        globaltheme->icons[1]->get_width() + 2 * fw,
        globaltheme->icons[1]->get_height() + 2 * fw,
        true);
    globaltheme->icons[0]->draw(p.x + fw, p.y + fw);
    globaltheme->icons[1]->draw(p.x + fw, p.y + size.y - globaltheme->icons[1]->get_height() - fw);
    draw_area(
        p.x,
        p.y + globaltheme->icons[0]->get_height() + 2 * fw,
        globaltheme->icons[0]->get_width() + 2 * fw,
        get_max_scrollbarsize(),
        false);
    draw_area(
        p.x,
        p.y + globaltheme->icons[0]->get_height() + 2 * fw + scrollbarpixelpos,
        globaltheme->icons[0]->get_width() + 2 * fw,
        get_scrollbarsize(),
        true);
}

void widget_scrollbar::on_click(vector2i position, mouse_button /*btn*/)
{
    unsigned oldpos = scrollbarpos;
    vector2i p      = get_pos();
    if (position.y < int(p.y + globaltheme->icons[0]->get_height() + 4)) {
        if (scrollbarpos > 0) {
            --scrollbarpos;
            compute_scrollbarpixelpos();
        }
    } else if (position.y >= int(p.y + size.y - globaltheme->icons[1]->get_height() - 4)) {
        if (scrollbarpos + 1 < scrollbarmaxpos) {
            ++scrollbarpos;
            compute_scrollbarpixelpos();
        }
    }
    if (oldpos != scrollbarpos) {
        on_scroll();
    }
}

void widget_scrollbar::on_drag(vector2i position, vector2i motion, mouse_button_state btnstate)
{
    unsigned oldpos = scrollbarpos;
    vector2i p      = get_pos();
    if ((position.y >= int(p.y + globaltheme->icons[0]->get_height() + 4))
        && (position.y < int(p.y + size.y - globaltheme->icons[1]->get_height() - 4))) {
        if (btnstate.any() && motion.y != 0) {
            if (scrollbarmaxpos > 1) {
                int msbp = get_max_scrollbarsize() - get_scrollbarsize();
                int sbpp = scrollbarpixelpos;
                sbpp += motion.y;
                if (sbpp < 0) {
                    sbpp = 0;
                } else if (sbpp > msbp) {
                    sbpp = msbp;
                }
                scrollbarpixelpos = sbpp;
                scrollbarpos      = scrollbarpixelpos * (scrollbarmaxpos - 1) / msbp;
            }
        }
        if (oldpos != scrollbarpos) {
            on_scroll();
        }
    }
}

void widget_scrollbar::on_wheel(input_action wd)
{
    unsigned oldpos = scrollbarpos;
    if (wd == input_action::up) {
        if (scrollbarpos > 0) {
            --scrollbarpos;
            compute_scrollbarpixelpos();
        }
    } else if (wd == input_action::down) {
        if (scrollbarpos + 1 < scrollbarmaxpos) {
            ++scrollbarpos;
            compute_scrollbarpixelpos();
        }
    }
    if (oldpos != scrollbarpos) {
        on_scroll();
    }
}

widget_list::widget_list(int x, int y, int w, int h)
    : widget(x, y, w, h, "")
    , listpos(0)
    , selected(-1)
    , columnwidth(-1)
{
    struct wls : public widget_scrollbar
    {
        unsigned& p;
        void on_scroll() override { p = get_current_position(); }
        wls(unsigned& p_, int x, int y, int w, int h)
            : widget_scrollbar(x, y, w, h)
            , p(p_)
        {
        }
        ~wls() override = default;
        ;
    };
    int fw      = globaltheme->frame_size();
    myscrollbar = add_child<wls>(
        listpos,
        size.x - 3 * fw - globaltheme->icons[0]->get_width(),
        fw,
        globaltheme->icons[0]->get_width() + 2 * fw,
        size.y - 2 * fw);
}

void widget_list::delete_entry(unsigned n)
{
    auto it = entries.begin() + n;
    if (it != entries.end()) {
        entries.erase(it);
    }
    unsigned es = entries.size();
    if (es == 0) {
        selected = -1; // remove selection
    } else if (es == 1) {
        set_selected(0); // set to first entry
    } else {
        on_sel_change();
    }
    unsigned ve = get_nr_of_visible_entries();
    if (es > ve) {
        myscrollbar->set_nr_of_positions(es - ve + 1);
    }
}

void widget_list::insert_entry(unsigned n, const std::string& s)
{
    auto it = entries.begin() + n;
    if (it != entries.end()) {
        entries.insert(it, s);
    } else {
        entries.push_back(s);
    }
    unsigned es = entries.size();
    if (es == 1) {
        set_selected(0); // set to first entry
    } else {
        on_sel_change();
    }
    unsigned ve = get_nr_of_visible_entries();
    if (es > ve) {
        myscrollbar->set_nr_of_positions(es - ve + 1);
    }
}

void widget_list::append_entry(const std::string& s)
{
    entries.push_back(s);
    unsigned es = entries.size();
    if (es == 1) {
        set_selected(0); // set to first entry
    } else {
        on_sel_change();
    }
    unsigned ve = get_nr_of_visible_entries();
    if (es > ve) {
        myscrollbar->set_nr_of_positions(es - ve + 1);
    }
}

void widget_list::set_entry(unsigned n, const std::string& s)
{
    if (n < entries.size()) {
        entries[n] = s;
    }
}

void widget_list::sort_entries()
{
    std::sort(entries.begin(), entries.end());
    on_sel_change();
}

void widget_list::make_entries_unique()
{
    entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
    unsigned es = entries.size();
    if (es == 1) {
        set_selected(0); // set to first entry
    } else {
        on_sel_change();
    }
    unsigned ve = get_nr_of_visible_entries();
    if (es > ve) {
        myscrollbar->set_nr_of_positions(es - ve + 1);
    }
}

auto widget_list::get_entry(unsigned n) const -> std::string
{
    if (n < entries.size()) {
        return entries[n];
    }
    return "";
}

auto widget_list::get_listsize() const -> unsigned
{
    return entries.size();
}

auto widget_list::get_selected() const -> int
{
    return selected;
}

void widget_list::set_selected(unsigned n)
{
    if (n < entries.size()) {
        selected    = int(n);
        unsigned ve = get_nr_of_visible_entries();
        if (listpos > n || n >= listpos + ve) {
            listpos = n;
            myscrollbar->set_current_position(n - ve);
        }
        on_sel_change();
    }
}

auto widget_list::get_selected_entry() const -> std::string
{
    if (selected >= 0) {
        return get_entry(selected);
    }
    return "";
}

auto widget_list::get_nr_of_visible_entries() const -> int
{
    return std::min(
        int(entries.size()), int((size.y - 2 * globaltheme->frame_size()) / globaltheme->myfont->get_height()));
}

void widget_list::clear()
{
    listpos  = 0;
    selected = -1;
    entries.clear();
    on_sel_change();
}

void widget_list::draw() const
{
    vector2i p = get_pos();
    draw_area(p.x, p.y, size.x, size.y, false);
    int fw                = globaltheme->frame_size();
    auto maxp             = get_nr_of_visible_entries();
    bool scrollbarvisible = (int(entries.size()) > maxp);
    for (auto lp = 0; lp < maxp; ++lp) {
        color tcol = !is_enabled()                     ? globaltheme->textdisabledcol
                     : (selected == int(lp + listpos)) ? globaltheme->textselectcol
                                                       : globaltheme->textcol;
        if (selected == int(lp + listpos)) {
            int width = size.x - 2 * fw;
            if (scrollbarvisible) {
                width -= 3 * fw + globaltheme->icons[0]->get_width();
            }
            globaltheme->backg->draw(
                p.x + fw, p.y + fw + lp * globaltheme->myfont->get_height(), width, globaltheme->myfont->get_height());
        }
        // optionally split string into columns
        if (columnwidth < 0) {
            globaltheme->myfont->print(
                p.x + fw, p.y + fw + lp * globaltheme->myfont->get_height(), entries[listpos + lp], tcol, true);
        } else {
            std::string tmp = entries[listpos + lp];
            unsigned col    = 0;
            while (true) {
                std::string::size_type tp = tmp.find('\t');
                std::string ct            = tmp.substr(0, tp);
                globaltheme->myfont->print(
                    p.x + fw + col * unsigned(columnwidth),
                    p.y + fw + lp * globaltheme->myfont->get_height(),
                    ct,
                    tcol,
                    true);
                if (tp == std::string::npos) {
                    break;
                }
                tmp = tmp.substr(tp + 1);
                ++col;
            }
        }
    }
    if (int(entries.size()) > maxp) {
        myscrollbar->draw();
    }
}

void widget_list::on_click(vector2i position, mouse_button btn)
{
    vector2i p = get_pos();
    if (btn == mouse_button::left) {
        if (myscrollbar->is_mouse_over(position)) {
            myscrollbar->on_click(position, btn);
        } else {
            int oldselected = selected;
            int fw          = globaltheme->frame_size();
            int sp          = std::max(0, (position.y - p.y - fw) / int(globaltheme->myfont->get_height()));
            selected        = std::min(int(entries.size()) - 1, int(listpos) + sp);
            if (oldselected != selected) {
                on_sel_change();
            }
        }
    }
}

void widget_list::on_drag(vector2i position, vector2i /*motion*/, mouse_button_state btnstate)
{
    auto btn = mouse_button::left;
    if (!btnstate.left()) {
        if (btnstate.right()) {
            btn = mouse_button::right;
        } else if (btnstate.middle()) {
            btn = mouse_button::middle;
        }
    }
    on_click(position, btn);
}

void widget_list::on_wheel(input_action wd)
{
    myscrollbar->on_wheel(wd);
}

void widget_list::set_column_width(int cw)
{
    columnwidth = cw;
}

void widget_edit::draw() const
{
    bool editing = focussed == this;
    vector2i p   = get_pos();
    draw_area(p.x, p.y, size.x, size.y, false);
    int fw   = globaltheme->frame_size();
    color cc = is_enabled() ? (editing ? globaltheme->textcol.more_contrast(3) : globaltheme->textcol)
                            : globaltheme->textdisabledcol;
    globaltheme->myfont->print_vc(p.x + fw, p.y + size.y / 2, text, cc, true);
    if (editing) {
        uint32_t tm = SYS().millisec();
        if ((tm / 500 & 1) != 0U) {
            vector2i sz = globaltheme->myfont->get_size(text.substr(0, cursorpos));
            vector2f xy(p.x + fw + sz.x, p.y + size.y / 8);
            vector2f wh_m1(std::max(fw / 2, 2) - 1, size.y * 3 / 4 - 1);
            primitives::quad(xy, xy + wh_m1, globaltheme->textcol.more_contrast(5)).render();
        }
    }
}

auto widget_edit::cursor_left() const -> unsigned
{
    return font::character_left(text, cursorpos);
}

auto widget_edit::cursor_right() const -> unsigned
{
    return font::character_right(text, cursorpos);
}

void widget_edit::on_key(key_code kc, key_mod /*km*/)
{
    unsigned l = text.length();
    // 	printf("get char? %i unicode %i\n", c, ks.unicode);
    // How to detect multibyte characters:
    // All parts of a multibyte (UTF8 coded) character have their highest bit
    // set (0x80). The first byte of the multibyte characters has its second
    // highest bit set (0x40). So multibyte charactes are sequences 0xC0 | x,
    // 0x80 | x, ...
    if (kc == key_code::LEFT && cursorpos > 0) {
        cursorpos = cursor_left();
    } else if (kc == key_code::RIGHT && cursorpos < l) {
        cursorpos = cursor_right();
    } else if (kc == key_code::HOME) {
        cursorpos = 0;
    } else if (kc == key_code::END) {
        cursorpos = l;
    } else if (kc == key_code::RETURN) {
        on_enter();
    } else if (kc == key_code::DELETE && cursorpos < l) {
        unsigned clen = cursor_right() - cursorpos;
        text.erase(cursorpos, clen);
        on_change();
    } else if (kc == key_code::BACKSPACE && cursorpos > 0) {
        unsigned clpos = cursor_left();
        text           = text.erase(clpos, cursorpos - clpos);
        cursorpos      = clpos;
        on_change();
    }
}

void widget_edit::on_text(const std::string& new_text)
{
    unsigned stxw  = globaltheme->myfont->get_size(new_text).x;
    unsigned textw = globaltheme->myfont->get_size(text).x;
    unsigned l     = text.length();
    if (int(textw + stxw + 8) < size.x) {
        if (cursorpos < l) {
            text.insert(cursorpos, new_text);
        } else {
            text += new_text;
        }
        cursorpos += new_text.length(); // fixme what is with UTF8 texts longer than one char
        on_change();
    }
}

widget_fileselector::widget_fileselector(int x, int y, int w, int h, const std::string& text_)
    : widget(x, y, w, h, text_)
{
    current_path     = add_child<widget_text>(120, 40, size.x - 140, 32, get_current_directory());
    current_dir      = add_child<filelist>(120, 80, size.x - 140, size.y - 136);
    current_filename = add_child<widget_edit>(120, size.y - 52, size.x - 140, 32, "");
    add_child<widget_text>(20, 40, 80, 32, "Path:");
    add_child<widget_caller_button<widget&>>(
        20, 80, 80, 32, text_ok, [](auto& w) { w.close(1); }, *this);
    add_child<widget_caller_button<widget&>>(
        20, 120, 80, 32, text_cancel, [](auto& w) { w.close(0); }, *this);

    read_current_dir();
}

void widget_fileselector::read_current_dir()
{
    current_dir->clear();
    directory dir(current_path->get_text());
    std::set<std::string> dirs;
    std::set<std::string> files;
    while (true) {
        std::string e = dir.read();
        if (e.empty()) {
            break;
        }
        if (e[0] == '.') {
            continue; // avoid . .. and hidden files
        }
        if (is_directory(current_path->get_text() + e)) {
            dirs.insert(e);
        } else {
            files.insert(e);
        }
    }
    nr_dirs  = dirs.size() + 1;
    nr_files = files.size();
    current_dir->append_entry("[..]");
    for (const auto& dir : dirs) {
        current_dir->append_entry(std::string("[") + dir + std::string("]"));
    }
    for (const auto& file : files) {
        current_dir->append_entry(file);
    }
}

void widget_fileselector::listclick()
{
    int n = current_dir->get_selected();
    if (n < 0 || unsigned(n) > nr_dirs + nr_files) {
        return;
    }
    std::string p       = current_path->get_text();
    std::string filesep = p.substr(p.length() - 1, 1);
    if (n == 0) {
        std::string::size_type st = p.rfind(filesep, p.length() - 2);
        if (st != std::string::npos) {
            p = p.substr(0, st) + filesep;
        }
        current_path->set_text(p);
        read_current_dir();
    } else if (unsigned(n) < nr_dirs) {
        std::string p = current_path->get_text();
        std::string d = current_dir->get_selected_entry();
        d             = d.substr(1, d.length() - 2); // remove [ ] characters
        p += d + filesep;
        current_path->set_text(p);
        read_current_dir();
    } else {
        current_filename->set_text(current_dir->get_selected_entry());
    }
}

widget_3dview::widget_3dview(int x, int y, int w, int h, std::unique_ptr<model>&& mdl_, color bgcol)
    : widget(x, y, w, h, "")
    , mdl(std::move(mdl_))
    , backgrcol(bgcol)
    , lightdir(0, 0, 1, 0)
    , lightcol(color::white())
{
    translation.z = 100;
    if (mdl != nullptr) {
        translation.z = mdl->get_boundbox_size().length() / 1.2;
    }
}

void widget_3dview::set_model(std::unique_ptr<model>&& mdl_)
{
    mdl = std::move(mdl_);
    if (mdl != nullptr) {
        translation.z = mdl->get_boundbox_size().length() / 1.2;
    } else {
        translation.z = 100;
    }
}

void widget_3dview::on_wheel(input_action wd)
{
    if (wd == input_action::up) {
        translation.z += 2;
    } else if (wd == input_action::down) {
        translation.z -= 2;
    }
}

void widget_3dview::on_drag(vector2i /*position*/, vector2i motion, mouse_button_state btnstate)
{
    if (btnstate.left()) {
        z_angle += motion.x * 0.5;
        x_angle += motion.y * 0.5;
    }
    if (btnstate.right()) {
        translation += vector2(motion).xy0() * 0.1;
    }
}

void widget_3dview::draw() const
{
    if (mdl == nullptr) {
        return;
    }

    vector3f bb = mdl->get_boundbox_size();
    float bbl   = bb.length();
    float zfar  = translation.z + bbl * 0.5;

    SYS().unprepare_2d_drawing();
    glFlush();
    unsigned vpx = SYS().get_res_area_2d_x();
    unsigned vpy = SYS().get_res_area_2d_y();
    unsigned vpw = SYS().get_res_area_2d_w();
    unsigned vph = SYS().get_res_area_2d_h();
    glViewport(vpx, vpy, vpw, vph);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    SYS().gl_perspective_fovx(70.0 /*fovx*/, float(size.x) / size.y, 1.0F, zfar);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float clr[4];
    backgrcol.store_rgba(clr);
    glClearColor(clr[0], clr[1], clr[2], clr[3]);
    glClear(GL_DEPTH_BUFFER_BIT /* | GL_COLOR_BUFFER_BIT*/);
    glLightfv(GL_LIGHT0, GL_POSITION, &lightdir.x);
    GLfloat diffcolor[4];
    lightcol.store_rgba(diffcolor);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffcolor);
    glLightfv(GL_LIGHT0, GL_SPECULAR, diffcolor);
    GLfloat ambcolor[4] = {0.1F, 0.1F, 0.1F, 1};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambcolor);

    glFogf(GL_FOG_DENSITY, 0.0005);
    glFogf(GL_FOG_START, 10000 * 0.75);
    glFogf(GL_FOG_END, 10000);

    glTranslatef(-translation.x, -translation.y, -translation.z);
    glRotatef(-80, 1, 0, 0);
    glRotatef(z_angle.value(), 0, 0, 1);
    glRotatef(x_angle.value(), 1, 0, 0);
    primitives::line(vector3f(-bb.x * 0.5, 0.0, -bb.z * 0.5), vector3f(bb.x * 0.5, 0.0, -bb.z * 0.5), color::black())
        .render();
    primitives::line(vector3f(0.0, -bb.y * 0.5, -bb.z * 0.5), vector3f(0.0, bb.y * 0.5, -bb.z * 0.5), color::black())
        .render();
    mdl->display();

    SYS().prepare_2d_drawing();
}

widget_slider::widget_slider(
    int x,
    int y,
    int w,
    int h,
    const std::string& text_,
    int minv,
    int maxv,
    int currv,
    int descrstep_)
    : widget(x, y, w, h, text_)
    , minvalue(minv)
    , maxvalue(maxv)
    , currvalue(currv)
    , descrstep(descrstep_)
{
    size.x = std::max(size.x, int(4));
    size.y = std::max(size.y, int(4));
    set_values(minv, maxv, currv, descrstep_);
}

void widget_slider::draw() const
{
    // draw sunken area that just has the sunken border height*2, so you see
    // only borders. draw the slider as raised area with height of 8*border or
    // so. draw vertical lines below the slider each n values, so that we have
    // at least 32pix between each value or 16 or so. draw descriptions
    // (min...maxval) every n pixels/positions, so that there is at least n
    // pixel space between each description

    // draw description if there is one
    color tcol  = is_enabled() ? globaltheme->textcol : globaltheme->textdisabledcol;
    unsigned h2 = globaltheme->myfont->get_height();
    unsigned h0 = 0;
    if (text.length() > 0) {
        globaltheme->myfont->print(pos.x, pos.y, text, tcol, true);
        h0 = globaltheme->myfont->get_size(text).y;
    }

    // draw slider bar
    unsigned h1      = size.y - h0 - h2;
    unsigned barh    = globaltheme->frame[0]->get_height() * 2;
    unsigned sliderw = h2;
    unsigned baroff  = h1 / 2 - barh;
    draw_area(pos.x, pos.y + h0 + baroff, size.x, barh, false);

    // draw marker texts and lines.
    for (int i = minvalue; i <= maxvalue; i += descrstep) {
        std::string vals = std::to_string(i);
        unsigned offset  = (size.x - sliderw) * (i - minvalue) / (maxvalue - minvalue);
        int valw         = globaltheme->myfont->get_size(vals).x;
        globaltheme->myfont->print(pos.x + sliderw / 2 + offset - valw / 2, pos.y + h0 + h1, vals, tcol, true);
        draw_line(
            pos.x + sliderw / 2 + offset, pos.y + h0 + baroff + barh, pos.x + sliderw / 2 + offset, pos.y + h0 + h1);
        // last descriptions should be aligned right and printed, so maybe skip
        // second last one.
        if (i < maxvalue && i + descrstep > maxvalue) {
            i = maxvalue - descrstep;
        }
    }

    // draw slider knob
    unsigned offset = (size.x - sliderw) * (currvalue - minvalue) / (maxvalue - minvalue);
    draw_area_col(pos.x + offset, pos.y + h0, sliderw, h1 - barh, true, globaltheme->textdisabledcol);
    draw_line(
        pos.x + sliderw / 2 + offset,
        pos.y + h0 + barh / 2,
        pos.x + sliderw / 2 + offset,
        pos.y + h0 + h1 - barh * 3 / 2);
}

void widget_slider::on_key(key_code kc, key_mod /*km*/)
{
    // move with cursor
    if (kc == key_code::LEFT && currvalue > minvalue) {
        --currvalue;
        on_change();
    } else if (kc == key_code::RIGHT && currvalue < maxvalue) {
        ++currvalue;
        on_change();
    }
}

void widget_slider::on_click(vector2i position, mouse_button btn)
{
    // set slider...
    if (btn == mouse_button::left) { // fixme rather state!
        int sliderpos = std::min(std::max(position.x, pos.x), position.x + size.x) - position.x;
        currvalue     = (sliderpos * (maxvalue - minvalue) + size.x / 2) / size.x + minvalue;
        on_change();
    }
}

void widget_slider::on_drag(vector2i position, vector2i /*motion*/, mouse_button_state btnstate)
{
    // move slider...
    if (btnstate.left()) {
        int sliderpos = std::min(std::max(position.x, pos.x), position.x + size.x) - position.x;
        currvalue     = (sliderpos * (maxvalue - minvalue) + size.x / 2) / size.x + minvalue;
        on_change();
    }
}

void widget_slider::set_values(int minv, int maxv, int currv, int descrstep_)
{
    minvalue  = minv;
    maxvalue  = std::max(minvalue + 1, maxv);
    currvalue = std::max(currv, minvalue);
    currvalue = std::min(currvalue, maxvalue);
    descrstep = std::max(int(1), descrstep_);
}
