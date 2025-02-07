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
#include "lwbuttonset.h"
#include "lwbutton.h"
#include "rtscalable.h"

LWButtonSet::LWButtonSet() = default;

void LWButtonSet::add(std::unique_ptr<LWButton>&& b)
{
    buttons.push_back(std::move(b));
}

hidpi::LogicalSize LWButtonSet::getMinimalDimensions () const
{
    hidpi::LogicalSize size(0, 0);

    for (const auto& entry : buttons) {
        hidpi::LogicalSize buttonSize = entry->getSize();
        size.width += buttonSize.width;
        size.height = std::max(buttonSize.height, size.height);
    }

    return size;
}

void LWButtonSet::arrangeButtons (int x, int y, int w, int h)
{
    if (x == pos.x && y == pos.y && w == allocSize.width && (h == -1 || h == allocSize.height)) {
        return;
    }

    hidpi::LogicalSize min = getMinimalDimensions();
    if (w < 0) {
        w = min.width;
    }
    if (h < 0) {
        h = min.height;
    }

    int begx = x;
    int endx = x + w - 1;

    for (const auto& button : buttons) {
        hidpi::LogicalCoord bPos;
        hidpi::LogicalSize bSize = button->getSize();

        LWButton::Alignment halign, valign;
        button->getAlignment (halign, valign);

        if (halign == LWButton::Left) {
            bPos.x = begx;
            begx += bSize.width;
        } else if (halign == LWButton::Right) {
            bPos.x = endx - bSize.width;
            endx -= bSize.width;
        }

        if (valign == LWButton::Top) {
            bPos.y = y;
        } else if (valign == LWButton::Bottom) {
            bPos.y = y + h - bSize.height - 1;
        } else if (valign == LWButton::Center) {
            bPos.y = y + (h - bSize.height) / 2;
        }

        button->setPosition(bPos);
    }

    pos.x = x;
    pos.y = y;
    allocSize.width = w;
    allocSize.height = h;
}

void LWButtonSet::move (int nx, int ny)
{
    for (const auto& entry : buttons) {
        entry->addPosition(hidpi::LogicalCoord(nx, ny) - pos);
    }
    pos.x = nx;
    pos.y = ny;
}

void LWButtonSet::redraw (const Cairo::RefPtr<Cairo::Context>& context)
{
    for (const auto& entry : buttons) {
        entry->redraw(context);
    }
}

bool LWButtonSet::motionNotify (int x, int y)
{
    bool res = false;
    for (const auto& entry : buttons) {
        res = entry->motionNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::pressNotify (int x, int y)
{
    bool res = false;
    for (const auto& entry : buttons) {
        res = entry->pressNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::releaseNotify (int x, int y)
{
    bool res = false;
    for (const auto& entry : buttons) {
        res = entry->releaseNotify(x, y) || res;
    }
    return res;
}

bool LWButtonSet::inside (hidpi::LogicalCoord pos) const
{

    for (const auto& entry : buttons) {
        if (entry->inside(pos)) {
            return true;
        }
    }
    return false;
}

void LWButtonSet::setButtonListener (LWButtonListener* bl)
{
    for (const auto& entry : buttons) {
        entry->setButtonListener(bl);
    }
}

void LWButtonSet::setColors (const Gdk::RGBA& bg, const Gdk::RGBA& fg)
{
    for (const auto& entry : buttons) {
        entry->setColors(bg, fg);
    }
}

Glib::ustring LWButtonSet::getToolTip (int x, int y) const
{
    for (const auto& entry : buttons) {
        const auto ttip = entry->getToolTip(x, y);

        if (!ttip.empty()) {
            return ttip;
        }
    }
    return {};
}
