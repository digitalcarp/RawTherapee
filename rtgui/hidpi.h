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
#include <glibmm/refptr.h>

namespace Cairo {
class ImageSurface;
class Surface;
}

namespace Gdk {
class Pixbuf;
}

namespace Gtk {
class Widget;
class Window;
}

namespace hidpi {

enum class PixelSpace { LOGICAL, PHYSICAL };

struct LogicalCoord {
    int x = 0;
    int y = 0;

    constexpr PixelSpace pixelSpace() const { return PixelSpace::LOGICAL; }
};

struct DeviceCoord {
    int x = 0;
    int y = 0;
    int device_scale = 1;

    constexpr PixelSpace pixelSpace() const { return PixelSpace::PHYSICAL; }
};

struct LogicalSize {
    int width = 0;
    int height = 0;

    static LogicalSize forWidget(const Gtk::Widget* widget);

    constexpr PixelSpace pixelSpace() const { return PixelSpace::LOGICAL; }
};

struct DeviceSize {
    int width = 0;
    int height = 0;
    int device_scale = 1;

    static DeviceSize forWidget(const Gtk::Widget* widget);

    constexpr PixelSpace pixelSpace() const { return PixelSpace::PHYSICAL; }
};

class DevicePixbuf {
public:
    DevicePixbuf();
    DevicePixbuf(const Glib::RefPtr<Gdk::Pixbuf>& ptr, int device_scale);

    DevicePixbuf(const DevicePixbuf& other);
    DevicePixbuf& operator=(const DevicePixbuf& other);
    DevicePixbuf(DevicePixbuf&& other);
    DevicePixbuf& operator=(DevicePixbuf&& other);

    operator bool() const { return static_cast<bool>(m_pixbuf); }

    const Glib::RefPtr<Gdk::Pixbuf>& pixbuf() const { return m_pixbuf; }
    DeviceSize size() const;

    friend void swap(DevicePixbuf& lhs, DevicePixbuf& rhs);

private:
    Glib::RefPtr<Gdk::Pixbuf> m_pixbuf;
    int m_device_scale;
};

void getDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    int x_scale, int y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface, int scale) {
    setDeviceScale(surface, scale, scale);
}

void getDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double& x_scale, double& y_scale);
void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    int x_scale, int y_scale);
inline void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface, int scale) {
    setDeviceScale(surface, scale, scale);
}

}  // namespace hidpi
