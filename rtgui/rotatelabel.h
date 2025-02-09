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
#include <gtkmm/widget.h>

// Label that can be rotated 90 degrees
class RotateLabel : public Gtk::Widget
{
public:
    RotateLabel();
    explicit RotateLabel(const Glib::ustring& text);

    void set_text(const Glib::ustring& text);
    void rotate90(bool val = true);

protected:
    void size_allocate_vfunc(int width, int height, int baseline) override;
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                       int& minimum_baseline, int& natural_baseline) const override;
    bool grab_focus_vfunc() override { return false; }
    void snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot) override;

private:
    std::pair<double, double> get_layout_location() const;

    Glib::RefPtr<Pango::Layout> m_layout;
    bool m_rotate90;
};
