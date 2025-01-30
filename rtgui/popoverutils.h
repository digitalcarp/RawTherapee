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

#include <gtkmm/popover.h>
#include <gtkmm/popovermenu.h>

class PopoverBin : public Gtk::Widget {
public:
    PopoverBin();
    ~PopoverBin();

    void set_child(const Glib::RefPtr<Gtk::Widget>& child);
    void set_popover(const Glib::RefPtr<Gtk::Popover>& popover);
    void remove_child();
    void remove_popover();

protected:
    void size_allocate_vfunc(int width, int height, int baseline) override;
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size,
                       int& minimum, int& natural,
                       int& minimum_baseline, int& natural_baseline) const override;

private:
    Glib::RefPtr<Gtk::Widget> m_child;
    Glib::RefPtr<Gtk::Popover> m_popover;
};
