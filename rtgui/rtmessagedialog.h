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

#pragma once

#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

class RtMessageDialog : public Gtk::Window {
public:
    enum class Type { INFO, WARNING, ERROR, FATAL_ERROR };
    enum class ButtonSet { NONE, OK, CLOSE, CANCEL, YES_NO, OK_CANCEL };

    RtMessageDialog();
    RtMessageDialog(const Glib::ustring& msg, Type type = Type::INFO,
                    ButtonSet buttons = ButtonSet::NONE);

    void show(Gtk::Window* parent = nullptr);

private:
    void setupLayout();
    void setupButtons(Gtk::Box& box, ButtonSet buttons);

    void onPositiveResponse();
    void onNegativeResponse();

    Gtk::Label m_message;
    Type m_type;
    ButtonSet m_button_set;
};
