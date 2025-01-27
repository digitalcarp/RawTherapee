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

#include "hidpi.h"

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <gdkmm/memorytexture.h>
#include <glibmm/refptr.h>

#include <optional>

class TextureBackBuffer {
public:
    static Cairo::RefPtr<Cairo::ImageSurface> createImageSurface(hidpi::ScaledDeviceSize size);

    TextureBackBuffer() = default;
    TextureBackBuffer(const Glib::RefPtr<Gdk::Texture>& texture, int device_scale) noexcept;
    TextureBackBuffer(const Cairo::RefPtr<Cairo::ImageSurface>& surface);

    bool update(const Cairo::RefPtr<Cairo::ImageSurface>& surface);

    const Glib::RefPtr<Gdk::Texture>& texture() const { return m_texture; }

    void setDirty() { m_texture = nullptr; }
    bool isDirty() const { return m_texture == nullptr; }
    operator bool() const { return isDirty(); }

    std::optional<hidpi::ScaledDeviceSize> getSize() const;

private:
    Glib::RefPtr<Gdk::Texture> m_texture;
    int m_device_scale = 1;
};
