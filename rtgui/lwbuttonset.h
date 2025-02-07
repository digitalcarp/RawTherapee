/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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

#include <gtkmm.h>
#include <memory>
#include <vector>

class LWButton;
class LWButtonListener;
class LWButtonSet
{

protected:
    std::vector<std::unique_ptr<LWButton>> buttons;
    hidpi::LogicalCoord pos;
    hidpi::LogicalSize allocSize;

public:
    LWButtonSet ();

    void add (std::unique_ptr<LWButton>&& b);

    hidpi::LogicalSize getMinimalDimensions() const;
    hidpi::LogicalSize getAllocatedDimensions() const { return allocSize; }
    void    arrangeButtons (int x, int y, int w, int h);
    void    setColors     (const Gdk::RGBA& bg, const Gdk::RGBA& fg);
    bool    motionNotify  (int x, int y);
    bool    pressNotify   (int x, int y);
    bool    releaseNotify (int x, int y);
    void    move          (int nx, int ny);
    bool    inside        (hidpi::LogicalCoord) const;

    Glib::ustring getToolTip (int x, int y) const;

    void    setButtonListener   (LWButtonListener* bl);
    void    redraw              (const Cairo::RefPtr<Cairo::Context>& context);
};
