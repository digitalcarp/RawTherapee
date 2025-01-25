/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2025 Daniel Gao <daniel.gao.work@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "rtmessagedialog.h"

#include "multilangmgr.h"

#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>

namespace {

Glib::ustring toString(RtMessageDialog::Type type) {
    using Type = RtMessageDialog::Type;
    switch (type) {
    case Type::INFO:
        return M("MESSAGE_INFO");
    case Type::WARNING:
        return M("MESSAGE_WARNING");
    case Type::ERROR:
        return M("MESSAGE_ERROR");
    case Type::FATAL_ERROR:
        return M("MESSAGE_FATAL_ERROR");
    default:
        return "";
    }
}

}  // namespace

RtMessageDialog::RtMessageDialog() {
    setupLayout();
}

RtMessageDialog::RtMessageDialog(const Glib::ustring& msg, Type type, ButtonSet buttons)
        : m_type(type), m_button_set(buttons) {
    setupLayout();

    m_message.set_markup(msg);
}

void RtMessageDialog::show(Gtk::Window* parent) {
    if (parent) {
        set_transient_for(*parent);
    }
    present();
}

void RtMessageDialog::setupLayout() {
    set_default_size(400, 200);
    set_title("");
    set_modal();

    auto main_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    main_box->set_spacing(8);
    main_box->set_hexpand(true);
    main_box->set_vexpand(true);

    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_use_markup();
    label->set_markup(toString(m_type));
    main_box->append(*label);

    m_message.set_wrap();
    m_message.set_use_markup();

    auto msg_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    msg_box->set_hexpand(true);
    msg_box->set_vexpand(true);
    msg_box->append(m_message);

    auto scroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    scroll->set_has_frame();
    scroll->set_hexpand(true);
    scroll->set_vexpand(true);
    scroll->set_child(*msg_box);
    main_box->append(*scroll);

    auto button_box = Gtk::make_managed<Gtk::Box>();
    button_box->set_margin(8);
    button_box->set_spacing(4);
    setupButtons(*button_box, m_button_set);
    main_box->append(*button_box);

    set_child(*main_box);
}

void RtMessageDialog::setupButtons(Gtk::Box& box, RtMessageDialog::ButtonSet buttons) {
    using ButtonSet = RtMessageDialog::ButtonSet;

    Glib::ustring positive;
    Glib::ustring negative;

    switch (buttons) {
    case ButtonSet::OK:
        positive = M("GENERAL_OK");
        break;
    case ButtonSet::CLOSE:
        negative = M("GENERAL_CLOSE");
        break;
    case ButtonSet::CANCEL:
        negative = M("GENERAL_CANCEL");
        break;
    case ButtonSet::YES_NO:
        positive = M("GENERAL_YES");
        negative = M("GENERAL_NO");
        break;
    case ButtonSet::OK_CANCEL:
        positive = M("GENERAL_OK");
        negative = M("GENERAL_CANCEL");
        break;
    case ButtonSet::NONE:
    default:
        return;
    }

    auto add_positive_button = [&](RtMessageDialog* window) {
        if (!positive.empty()) {
            auto button = Gtk::make_managed<Gtk::Button>(positive);
            button->set_hexpand(true);
            button->signal_clicked().connect(
                sigc::mem_fun(*window, &RtMessageDialog::onPositiveResponse));
            box.append(*button);
        }
    };
    auto add_negative_button = [&](RtMessageDialog* window) {
        if (!negative.empty()) {
            auto button = Gtk::make_managed<Gtk::Button>(negative);
            button->set_hexpand(true);
            button->signal_clicked().connect(
                sigc::mem_fun(*window, &RtMessageDialog::onNegativeResponse));
            box.append(*button);
        }
    };

    add_positive_button(this);
    add_negative_button(this);
}

void RtMessageDialog::onPositiveResponse() {
    // TODO(gtk4): Expose way for parent to handle response
    close();
}

void RtMessageDialog::onNegativeResponse() {
    close();
}
