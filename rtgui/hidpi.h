/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2024 Daniel Gao <daniel.gao.work@gmail.com>
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

namespace Cairo {
class ImageSurface;
class Surface;
}

namespace Gtk {
class Widget;
class Window;
}

struct DevicePixelDimensions {
    int width;
    int height;
    int scale;

    DevicePixelDimensions() : width(0), height(0), scale(1) {}
    DevicePixelDimensions(int phys_width, int phys_height, int hidpi_scale)
            : width(phys_width), height(phys_height), scale(hidpi_scale) {}

    static DevicePixelDimensions for_widget(const Gtk::Widget* widget, const Gtk::Window* window);
    static DevicePixelDimensions for_surface(const Cairo::RefPtr<Cairo::Surface>& surface);
};

void getDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double x_scale, double y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface, double scale) {
    setDeviceScale(surface, scale, scale);
}

void getDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double x_scale, double y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface, double scale) {
    setDeviceScale(surface, scale, scale);
}

void setDeviceScaleToGlobal(const Cairo::RefPtr<Cairo::ImageSurface>& surface);
