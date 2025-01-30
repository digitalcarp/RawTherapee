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

#include "popoverutils.h"

PopoverBin::PopoverBin() : m_child(nullptr), m_popover(nullptr)
{
    signal_destroy().connect([&]() {
        remove_child();
        remove_popover();
    });
}

PopoverBin::~PopoverBin()
{
    remove_child();
    remove_popover();
}

void PopoverBin::set_child(const Glib::RefPtr<Gtk::Widget>& child)
{
    remove_child();
    child->insert_at_start(*this);
    m_child = child;
}

void PopoverBin::set_popover(const Glib::RefPtr<Gtk::Popover>& popover)
{
    remove_popover();
    popover->insert_at_end(*this);
    m_popover = popover;
}

void PopoverBin::remove_child()
{
    if (m_child) {
        m_child->unparent();
        m_child = nullptr;
    }
}

void PopoverBin::remove_popover()
{
    if (m_popover) {
        m_popover->unparent();
        m_popover = nullptr;
    }
}

void PopoverBin::size_allocate_vfunc(int width, int height, int baseline)
{
    if (m_child) {
        Gtk::Allocation alloc(0, 0, width, height);
        m_child->size_allocate(alloc, baseline);
    }
    if (m_popover) {
        m_popover->present();
    }
}

Gtk::SizeRequestMode PopoverBin::get_request_mode_vfunc() const
{
    return m_child ? m_child->get_request_mode() : Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void PopoverBin::measure_vfunc(Gtk::Orientation orientation, int for_size,
                               int& minimum, int& natural,
                               int& minimum_baseline, int& natural_baseline) const
{
    if (m_child) {
        m_child->measure(orientation, for_size, minimum, natural,
                         minimum_baseline, natural_baseline);
    } else {
        minimum = 10;
        natural = 10;
        minimum_baseline = -1;
        natural_baseline = -1;
    }
}
