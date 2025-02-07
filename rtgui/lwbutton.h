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

#include <cairomm/context.h>
#include <gdkmm/rgba.h>
#include <glibmm/ustring.h>

class LWButton;

class LWButtonListener
{
public:
    virtual ~LWButtonListener() = default;
    virtual void buttonPressed(LWButton* button, int actionCode, void* actionData)  = 0;
    virtual void redrawNeeded(LWButton* button) = 0;
};

class LWButton
{

public:
    enum Alignment {Left, Right, Top, Bottom, Center};
    enum State { Normal, Over, Pressed_In, Pressed_Out};

    using Icon = hidpi::ScaledImageSurface;

private:
    hidpi::LogicalCoord currPos;
    hidpi::LogicalSize size;
    Alignment halign, valign;
    Icon icon;
    double bgr, bgg, bgb;
    double fgr, fgg, fgb;
    State state;
    LWButtonListener* listener;
    int actionCode;
    void* actionData;
    Glib::ustring* toolTip;

public:
    LWButton (const Icon& i, int aCode, void* aData, Alignment ha = Left, Alignment va = Center, Glib::ustring* tooltip = nullptr);

    hidpi::LogicalSize getSize() const { return size; }
    hidpi::LogicalCoord getPosition() const { return currPos; }

    void    getAlignment        (Alignment& ha, Alignment& va) const;
    void    setPosition         (hidpi::LogicalCoord pos) { currPos = pos; }
    void    addPosition         (hidpi::LogicalCoord offset);
    bool    inside              (hidpi::LogicalCoord pos) const;
    void    setColors           (const Gdk::RGBA& bg, const Gdk::RGBA& fg);
    void    setToolTip          (Glib::ustring* tooltip);

    void setIcon(const Icon& i);
    const Icon& getIcon() const { return icon; }

    bool    motionNotify        (int x, int y);
    bool    pressNotify         (int x, int y);
    bool    releaseNotify       (int x, int y);

    Glib::ustring getToolTip (int x, int y) const;

    void    setButtonListener   (LWButtonListener* bl)
    {
        listener = bl;
    }

    void    redraw              (const Cairo::RefPtr<Cairo::Context>& context);
};
