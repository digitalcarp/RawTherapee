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

#include <cairomm/refptr.h>
#include <cairomm/surface.h>
#include <gdkmm/memorytexture.h>
#include <glibmm/refptr.h>

Cairo::RefPtr<Cairo::ImageSurface> createMemoryTextureImageSurface(int width, int height);
Glib::RefPtr<Gdk::Texture> createMemoryTexture(const Cairo::RefPtr<Cairo::ImageSurface>& surface);
