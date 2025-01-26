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

#include "svgpaintable.h"

#include "rtscalable.h"

#include <glibmm/fileutils.h>
#include <gtkmm/snapshot.h>
#include <librsvg/rsvg.h>

#include <iostream>

// --- START C GObject

// Implementation taken from the GTK4 demo examples:
// https://gitlab.gnome.org/GNOME/gtk/-/blob/main/demos/gtk-demo/svgpaintable.c

struct _SvgPaintable
{
    GObject parent_instance;
    GFile *file;
    RsvgHandle *handle;
};

struct _SvgPaintableClass
{
    GObjectClass parent_class;
};

enum {
    PROP_FILE = 1,
    NUM_PROPERTIES
};

static void
svg_paintable_snapshot (GdkPaintable *paintable,
                        GdkSnapshot  *snapshot,
                        double        width,
                        double        height)
{
    SvgPaintable *self = SVG_PAINTABLE (paintable);

    cairo_t *cr;
    auto graphene_rect = GRAPHENE_RECT_INIT (0, 0,
                                             static_cast<float>(width),
                                             static_cast<float>(height));
    cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot), &graphene_rect);

    // The device scale should be set properly already by GTK
    // double device_scale = RTScalable::getScale ();
    // cairo_surface_set_device_scale (cairo_get_target (cr), device_scale, device_scale);

    GError *error = NULL;
    RsvgRectangle rsvg_rect = {0, 0, width, height};
    if (!rsvg_handle_render_document (self->handle, cr, &rsvg_rect, &error))
    {
        g_error ("%s", error->message);
    }

    cairo_destroy (cr);
}

static int
svg_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
    SvgPaintable *self = SVG_PAINTABLE (paintable);
    double width;

    if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, &width, NULL))
        return 0;

    return ceil (width);
}

static int
svg_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
    SvgPaintable *self = SVG_PAINTABLE (paintable);
    double height;

    if (!rsvg_handle_get_intrinsic_size_in_pixels (self->handle, NULL, &height))
        return 0;

    return ceil (height);
}

static void
svg_paintable_init_interface (GdkPaintableInterface *iface)
{
    iface->snapshot = svg_paintable_snapshot;
    iface->get_intrinsic_width = svg_paintable_get_intrinsic_width;
    iface->get_intrinsic_height = svg_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (SvgPaintable, svg_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                svg_paintable_init_interface))

static void
svg_paintable_init (SvgPaintable *self)
{
}

static void
svg_paintable_dispose (GObject *object)
{
    SvgPaintable *self = SVG_PAINTABLE (object);

    g_clear_object (&self->file);
    g_clear_object (&self->handle);

    G_OBJECT_CLASS (svg_paintable_parent_class)->dispose (object);
}

static void
svg_paintable_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    SvgPaintable *self = SVG_PAINTABLE (object);

    switch (prop_id)
    {
    case PROP_FILE:
        {
            gpointer ptr = g_value_get_object (value);
            GFile *file = reinterpret_cast<GFile *>(ptr);
            RsvgHandle *handle = rsvg_handle_new_from_gfile_sync (file,
                                                                  RSVG_HANDLE_FLAGS_NONE,
                                                                  NULL,
                                                                  NULL);
            rsvg_handle_set_dpi (handle, 96);

            g_set_object (&self->file, file);
            g_set_object (&self->handle, handle);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
svg_paintable_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
    SvgPaintable *self = SVG_PAINTABLE (object);

    switch (prop_id)
    {
        case PROP_FILE:
            g_value_set_object (value, self->file);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
svg_paintable_class_init (SvgPaintableClass *svg_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (svg_class);

    object_class->dispose = svg_paintable_dispose;
    object_class->set_property = svg_paintable_set_property;
    object_class->get_property = svg_paintable_get_property;

    g_object_class_install_property (object_class, PROP_FILE,
                                     g_param_spec_object ("file", "File", "File",
                                                          G_TYPE_FILE,
                                                          G_PARAM_READWRITE));
}

GdkPaintable *
svg_paintable_new (GFile *file)
{
    gpointer ptr = g_object_new (SVG_TYPE_PAINTABLE, "file", file, NULL);
    return reinterpret_cast<GdkPaintable *>(ptr);
}

// --- END C GObject

extern Glib::ustring argv0;

Glib::RefPtr<SvgPaintableWrapper>
SvgPaintableWrapper::createFromFilename(const Glib::ustring& filepath, bool cached) {
    using SvgPaintableCache = std::unordered_map<std::string, Glib::RefPtr<SvgPaintableWrapper>>;
    static SvgPaintableCache cache;

    if (cached) {
        auto it = cache.find(filepath);
        if (it != cache.end()) {
            return it->second;
        }
    }

    if (!Glib::file_test(filepath.c_str(), Glib::FileTest::EXISTS)) {
        std::cerr << "Failed to load SVG file \"" << filepath << "\"\n";
        return nullptr;
    }

    GFile* file = g_file_new_for_path(filepath.c_str());
    SvgPaintable* svg = SVG_PAINTABLE (svg_paintable_new(file));
    g_object_unref(file);

    auto wrapper = std::make_shared<SvgPaintableWrapper>(svg);
    if (cached) {
        cache.emplace(filepath, wrapper);
    }
    return wrapper;
}

Glib::RefPtr<SvgPaintableWrapper>
SvgPaintableWrapper::createFromIcon(const Glib::ustring& icon_name, bool cached) {
    Glib::RefPtr<Gdk::Display> display = Gdk::Display::get_default();
    auto theme = Gtk::IconTheme::get_for_display(display);

    Glib::RefPtr<Gtk::IconPaintable> icon = theme->lookup_icon(icon_name, 16);
    if (!icon) {
        std::cerr << "Failed to load icon \"" << icon_name << "\"\n";
        return nullptr;
    }

    std::string file = icon->get_file()->get_path();
    auto pos = file.find_last_of('.');
    if (pos > file.length()) {
        std::cerr << "Failed to parse extension for icon \"" << icon_name
            << "\" at path: " << file << "\n";
        return nullptr;
    }

    auto fext = file.substr(pos + 1);
    if (fext != "svg") {
        std::cerr << "Icon \"" << icon_name << "\" is not an SVG: " << file << "\n";
        return nullptr;
    }

    return SvgPaintableWrapper::createFromFilename(file, cached);
}

Glib::RefPtr<SvgPaintableWrapper>
SvgPaintableWrapper::createFromImage(const Glib::ustring& fname, bool cached) {
    Glib::ustring img_dir = Glib::build_filename(argv0, "images");
    Glib::ustring filepath = Glib::build_filename(img_dir, fname);

    return createFromFilename(filepath, cached);
}

SvgPaintableWrapper::~SvgPaintableWrapper() {
    if (m_gobj) {
        g_object_unref(m_gobj);
    }
}

Glib::RefPtr<Gdk::Texture> SvgPaintableWrapper::createTexture(int width, int height) {
    // MemoryTexture::DEFAULT_FORMAT is equal to CAIRO_FORMAT_ARGB32.
    // See gdkmemorytexture.h definition of GDK_MEMORY_DEFAULT
    constexpr auto default_format = GDK_MEMORY_DEFAULT;
    constexpr auto texture_format = static_cast<Gdk::MemoryTexture::Format>(default_format);
    constexpr auto cairo_format = Cairo::Surface::Format::ARGB32;
    static_assert(static_cast<int>(texture_format) == static_cast<int>(cairo_format));

    auto surface = Cairo::ImageSurface::create(cairo_format, width, height);
    auto cr = Cairo::Context::create(surface);

    GError *error = NULL;
    RsvgRectangle rsvg_rect = {0, 0, static_cast<double>(width), static_cast<double>(height)};
    if (!rsvg_handle_render_document(m_gobj->handle, cr->cobj(), &rsvg_rect, &error))
    {
        g_error("%s", error->message);
        return nullptr;
    }

    // Implementation inspired by private API gdk_texture_new_for_surface().
    // See gdktexture.c
    int stride = surface->get_stride();
    auto bytes = Glib::Bytes::create(surface->get_data(), height * stride);
    auto texture = Gdk::MemoryTexture::create(width, height, texture_format, bytes, stride);
    return texture;
}
