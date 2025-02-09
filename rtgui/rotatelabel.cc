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

#include "rotatelabel.h"

#include <pangomm/layout.h>
#include <gtkmm/snapshot.h>

// This implementation is inspired by a simplified version of Gtk::Label.
// https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gtk/gtklabel.c

RotateLabel::RotateLabel() : RotateLabel("") {}

RotateLabel::RotateLabel(const Glib::ustring& text) : m_rotate90(false)
{
    m_layout = create_pango_layout(text);
    m_layout->set_wrap(Pango::WrapMode::NONE);
    // Reset so it doesn't affect measure()
    m_layout->set_width(-1);
}

void RotateLabel::set_text(const Glib::ustring& text)
{
    m_layout->set_text(text);
    // Reset so it doesn't affect measure()
    m_layout->set_width(-1);
    queue_resize();
}

void RotateLabel::rotate90(bool val)
{
    m_rotate90 = val;
    queue_resize();
}

void RotateLabel::size_allocate_vfunc(int width, int height, int baseline)
{
    if (m_rotate90) {
        m_layout->set_width(height);
    } else {
        m_layout->set_width(width);
    }
}

Gtk::SizeRequestMode RotateLabel::get_request_mode_vfunc() const
{
    return Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void RotateLabel::measure_vfunc(Gtk::Orientation orientation, int for_size,
                                int& minimum, int& natural,
                                int& minimum_baseline, int& natural_baseline) const
{
    if (for_size > 0) {
        for_size *= Pango::SCALE;
    }

    bool is_horizontal = m_rotate90 ?
        orientation == Gtk::Orientation::VERTICAL :
        orientation == Gtk::Orientation::HORIZONTAL;
    int stub = 0;

    if (is_horizontal) {
        m_layout->get_size(natural, stub);
        minimum = natural;
    } else {
        m_layout->get_size(stub, minimum);
        natural = minimum;
    }

    minimum = PANGO_PIXELS_CEIL (minimum);
    natural = PANGO_PIXELS_CEIL (natural);
    minimum_baseline = -1;
    natural_baseline = -1;
    if (minimum_baseline > 0) {
        minimum_baseline = PANGO_PIXELS_CEIL (minimum_baseline);
    }
    if (natural_baseline > 0) {
        natural_baseline = PANGO_PIXELS_CEIL (natural_baseline);
    }
}

void RotateLabel::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot)
{
    if (m_layout->get_text().empty()) return;

    if (m_rotate90) {
        snapshot->translate(Gdk::Graphene::Point(0.0, get_height()));
        snapshot->rotate(-90);
    }

    auto [x, y] = get_layout_location();
    snapshot->render_layout(get_style_context(), x, y, m_layout);
}

std::pair<double, double> RotateLabel::get_layout_location() const
{
    const int widget_width = get_width();
    const int widget_height = get_height();

    Pango::Rectangle logical;
    Pango::Rectangle stub;
    m_layout->get_pixel_extents(stub, logical);

    const double x_align = 0.5;
    const double y_align = 0.5;
    double x = 0;
    double y = 0;

    if (m_rotate90) {
        x = std::floor((x_align * (widget_height - logical.get_width()))  - logical.get_x());
        y = std::floor((y_align * (widget_width  - logical.get_height())) - logical.get_y());
    } else {
        x = std::floor((x_align * (widget_width  - logical.get_width()))  - logical.get_x());
        y = std::floor((y_align * (widget_height - logical.get_height())) - logical.get_y());
    }

    return {x, y};
}
