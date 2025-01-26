/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2018 Jean-Christophe FRISCH <natureh.510@gmail.com>
 *  Copyright (c) 2022 Pierre CABRERA <pierre.cab@gmail.com>
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

#include "rtimage.h"

#include <gdkmm/display.h>
#include <gtkmm/icontheme.h>

#include <iostream>

RtImage::RtImage() : Gtk::Image() {}

RtImage::RtImage(const Glib::ustring& icon_name, bool cached)
        : Gtk::Image(), m_icon_name(icon_name) {
    m_svg = SvgPaintableWrapper::createFromIcon(icon_name);
    gtk_image_set_from_paintable(gobj(), m_svg->base_gobj());
}

void RtImage::set_from_icon_name(const Glib::ustring& icon_name) {
    m_icon_name = icon_name;
    m_svg = SvgPaintableWrapper::createFromIcon(icon_name);
    gtk_image_set_from_paintable(gobj(), m_svg->base_gobj());
}
