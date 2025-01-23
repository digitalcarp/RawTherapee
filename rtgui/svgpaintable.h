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

#include <gdk/gdk.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>

// --- START C GObject

// Implementation taken from the GTK4 demo examples:
// https://gitlab.gnome.org/GNOME/gtk/-/blob/main/demos/gtk-demo/svgpaintable.h

G_BEGIN_DECLS

#define SVG_TYPE_PAINTABLE (svg_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SvgPaintable, svg_paintable, SVG, PAINTABLE, GObject)

GdkPaintable* svg_paintable_new (GFile* file);

G_END_DECLS

// --- END C GObject

class SvgPaintableWrapper {
public:
    static Glib::RefPtr<SvgPaintableWrapper>
    createFromFilename(const Glib::ustring& filepath, bool cached = true);
    static Glib::RefPtr<SvgPaintableWrapper> createFromImage(const Glib::ustring& fname);

    // This takes ownership of the pointer so the caller does not need to
    // call g_object_unref().
    SvgPaintableWrapper(SvgPaintable* gobj) : m_gobj(gobj) {}
    ~SvgPaintableWrapper();

    SvgPaintable* gobj() const { return m_gobj; }
    GdkPaintable* base_gobj() const { return GDK_PAINTABLE(m_gobj); }

private:
    SvgPaintable* m_gobj;
};
