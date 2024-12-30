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

#include "rtscalable.h"
#include "hidpi.h"

#include <cairomm/surface.h>

void getDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double& x_scale, double& y_scale) {
    cairo_surface_t* cobj = surface->cobj();
    cairo_surface_get_device_scale(cobj, &x_scale, &y_scale);
}

void setDeviceScale(const Cairo::RefPtr<Cairo::Surface>& surface,
                    double x_scale, double y_scale) {
    cairo_surface_t* cobj = surface->cobj();
    cairo_surface_set_device_scale(cobj, x_scale, y_scale);
}

void getDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double& x_scale, double& y_scale) {
    cairo_surface_t* cobj = surface->cobj();
    cairo_surface_get_device_scale(cobj, &x_scale, &y_scale);
}

void setDeviceScale(const Cairo::RefPtr<Cairo::ImageSurface>& surface,
                    double x_scale, double y_scale) {
    cairo_surface_t* cobj = surface->cobj();
    cairo_surface_set_device_scale(cobj, x_scale, y_scale);
}

void setDeviceScaleToGlobal(const Cairo::RefPtr<Cairo::ImageSurface>& surface) {
    setDeviceScale(surface, RTScalable::getScale());
}
