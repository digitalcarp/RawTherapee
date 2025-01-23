/*
 *  This file is part of RawTherapee.
 *
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

#include "rtscalable.h"

// Default static parameter values
double RTScalable::s_dpi = 96.;
int RTScalable::s_scale = 1;

int RTScalable::getScaleForWindow(const Gtk::Window* window)
{
    int scale = window->get_scale_factor();
    // Default minimum value of 1 as scale is used to scale surface
    return scale > 0 ? scale : 1;
}

int RTScalable::getScaleForWidget(const Gtk::Widget* widget)
{
    int scale = widget->get_scale_factor();
    // Default minimum value of 1 as scale is used to scale surface
    return scale > 0 ? scale : 1;
}

void RTScalable::getDPInScale(const Gtk::Window* window, double &newDPI, int &newScale)
{
    if (window) {
        // TODO: Screen API removed... how do you get the DPI now?
        // const auto screen = window->get_screen();
        // newDPI = screen->get_resolution(); // Get DPI retrieved from the OS
        newDPI = s_dpi;

        // Get scale factor associated to the window
        newScale = getScaleForWindow(window);
    }
}

void RTScalable::init(const Gtk::Window* window)
{
    // Retrieve DPI and Scale paremeters from OS
    double dpi = s_dpi;
    int scale = s_scale;
    getDPInScale(window, dpi, scale);
    setDPInScale(dpi, scale);
}

void RTScalable::setDPInScale (const Gtk::Window* window)
{
    double dpi = s_dpi;
    int scale = s_scale;
    getDPInScale(window, dpi, scale);
    setDPInScale(dpi, scale);
}

void RTScalable::setDPInScale (const double newDPI, const int newScale)
{
    if (s_dpi != newDPI || s_scale != newScale) {
        s_dpi = newDPI;
        s_scale = newScale;
    }
}

double RTScalable::getDPI ()
{
    return s_dpi;
}

int RTScalable::getScale ()
{
    return s_scale;
}

double RTScalable::getGlobalScale()
{
    return (RTScalable::getDPI() / RTScalable::baseDPI);
}

int RTScalable::scalePixelSize(const int pixel_size)
{
    const double s = getGlobalScale();
    return static_cast<int>(pixel_size * s + 0.5); // Rounded scaled size
}

double RTScalable::scalePixelSize(const double pixel_size)
{
    const double s = getGlobalScale();
    return (pixel_size * s);
}
