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

#include "cairotexture.h"

// MemoryTexture::DEFAULT_FORMAT is equal to CAIRO_FORMAT_ARGB32.
// See gdkmemorytexture.h definition of GDK_MEMORY_DEFAULT
constexpr auto TEXTURE_FORMAT =
    static_cast<Gdk::MemoryTexture::Format>(GDK_MEMORY_DEFAULT);
constexpr auto CAIRO_FORMAT = Cairo::Surface::Format::ARGB32;
static_assert(static_cast<int>(TEXTURE_FORMAT) == static_cast<int>(CAIRO_FORMAT));

Cairo::RefPtr<Cairo::ImageSurface> createMemoryTextureImageSurface(int width, int height) {
    return Cairo::ImageSurface::create(CAIRO_FORMAT, width, height);
}

Glib::RefPtr<Gdk::Texture> createMemoryTexture(const Cairo::RefPtr<Cairo::ImageSurface>& surface) {
    if (surface->get_format() != CAIRO_FORMAT) return nullptr;

    // Implementation inspired by private API gdk_texture_new_for_surface().
    // See gdktexture.c
    int width = surface->get_width();
    int height = surface->get_height();
    int stride = surface->get_stride();
    auto bytes = Glib::Bytes::create(surface->get_data(), height * stride);
    auto texture = Gdk::MemoryTexture::create(width, height, TEXTURE_FORMAT, bytes, stride);
    return texture;
}
