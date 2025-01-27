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

#include "texturebackbuffer.h"

#include "cairotexture.h"

Cairo::RefPtr<Cairo::ImageSurface>
TextureBackBuffer::createImageSurface(hidpi::ScaledDeviceSize size)
{
    auto surface = createMemoryTextureImageSurface(size.width, size.height);
    hidpi::setDeviceScale(surface, size.device_scale);
    return surface;
}

TextureBackBuffer::TextureBackBuffer(const Glib::RefPtr<Gdk::Texture>& texture, int device_scale) noexcept
    : m_texture(texture), m_device_scale(device_scale)
{
    m_device_scale = m_texture ? device_scale : 1;
}

TextureBackBuffer::TextureBackBuffer(const Cairo::RefPtr<Cairo::ImageSurface>& surface)
{
    update(surface);
}

bool TextureBackBuffer::update(const Cairo::RefPtr<Cairo::ImageSurface>& surface)
{
    m_texture = createMemoryTexture(surface);
    if (m_texture) {
        m_device_scale = static_cast<int>(surface->get_device_scale());
        return true;
    } else {
        m_device_scale = 1;
        return false;
    }
}

std::optional<hidpi::ScaledDeviceSize> TextureBackBuffer::getSize() const
{
    std::optional<hidpi::ScaledDeviceSize> result;
    if (!m_texture) return result;

    result = { m_texture->get_width(), m_texture->get_height(), m_device_scale };
    return result;
}
