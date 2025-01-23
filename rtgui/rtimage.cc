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

#include "svgpaintable.h"

#include <gdkmm/display.h>
#include <gtkmm/icontheme.h>

#include <iostream>

RtImage::RtImage() : Gtk::Image() {}

RtImage::RtImage(const Glib::ustring& icon_name, bool cached)
        : Gtk::Image(), m_icon_name(icon_name) {
    Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
    auto theme = Gtk::IconTheme::get_for_display(display);

    Glib::RefPtr<Gtk::IconPaintable> icon = theme->lookup_icon(icon_name, 16);
    if (!icon) {
        std::cerr << "Failed to load icon \"" << icon_name << "\"\n";
        return;
    }

    std::string file = icon->get_file()->get_path();
    auto pos = file.find_last_of('.');
    if (pos > file.length()) {
        std::cerr << "Failed to parse extension for icon \"" << icon_name
            << "\" at path: " << file << "\n";
        return;
    }

    auto fext = file.substr(pos + 1);
    if (fext != "svg") {
        std::cerr << "Icon \"" << icon_name << "\" is not an SVG: " << file << "\n";
        return;
    }

    Glib::RefPtr<SvgPaintableWrapper> svg = SvgPaintableWrapper::createFromFilename(file, cached);
    gtk_image_set_from_paintable(gobj(), svg->base_gobj());
}
