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

#include "svgpaintable.h"

#include <gtkmm.h>

enum CursorShape {
    CSAddColPicker,
    CSArrow,
    CSCropSelect,
    CSCrosshair,
    CSEmpty,
    CSHandClosed,
    CSHandOpen,
    CSMove,
    CSMove1DH,
    CSMove1DV,
    CSMove2D,
    CSMoveLeft,
    CSMoveRight,
    CSMoveRotate,
    CSPlus,
    CSResizeBottomLeft,
    CSResizeBottomRight,
    CSResizeDiagonal,
    CSResizeHeight,
    CSResizeTopLeft,
    CSResizeTopRight,
    CSResizeWidth,
    CSSpotWB,
    CSStraighten,
    CSUndefined,
    CSWait
};

class CursorManager
{
private:
    struct CursorInfo {
        Glib::RefPtr<Gdk::Cursor> cursor;
        Glib::RefPtr<SvgPaintableWrapper> svg;
        // Cached texture for cursor
        Glib::RefPtr<Gdk::Texture> texture;
        // % offset from middle in range (-1, 1);
        double hotspot_x = 0.0;
        double hotspot_y = 0.0;

        Glib::RefPtr<Gdk::Texture>
        generateTexture(int cursor_size, double scale,
                        int& out_width, int& out_height,
                        int& out_hotspot_x, int& out_hotspot_y);
    };
    std::unordered_map<CursorShape, CursorInfo> m_cursor_info;

    Glib::RefPtr<CursorInfo> cAdd;
    Glib::RefPtr<CursorInfo> cAddPicker;
    Glib::RefPtr<CursorInfo> cCropDraw;
    Glib::RefPtr<CursorInfo> cCrosshair;
    Glib::RefPtr<CursorInfo> cHandClosed;
    Glib::RefPtr<CursorInfo> cHandOpen;
    Glib::RefPtr<CursorInfo> cEmpty;
    Glib::RefPtr<CursorInfo> cMoveBL;
    Glib::RefPtr<CursorInfo> cMoveBR;
    Glib::RefPtr<CursorInfo> cMoveL;
    Glib::RefPtr<CursorInfo> cMoveR;
    Glib::RefPtr<CursorInfo> cMoveTL;
    Glib::RefPtr<CursorInfo> cMoveTR;
    Glib::RefPtr<CursorInfo> cMoveX;
    Glib::RefPtr<CursorInfo> cMoveY;
    Glib::RefPtr<CursorInfo> cMoveXY;
    Glib::RefPtr<CursorInfo> cRotate;
    Glib::RefPtr<CursorInfo> cWB;
    Glib::RefPtr<CursorInfo> cWait;

    Glib::RefPtr<Gdk::Display> display;
    Glib::RefPtr<Gtk::Window> window;

    void setCursor (CursorShape shape);
    void setCursor (const Glib::RefPtr<Gtk::Window>& window, CursorShape shape);

    Glib::RefPtr<CursorInfo>
    createCursor(const Glib::ustring& name, const Glib::ustring& fallback,
                 double hotspot_x = 0.0, double hotspot_y = 0.0);

public:
    void init                         (const Glib::RefPtr<Gtk::Window>& mainWindow);
    static void setWidgetCursor       (const Glib::RefPtr<Gtk::Window>& window, CursorShape shape);
    static void setCursorOfMainWindow (Gtk::Window* window, CursorShape shape);
};

extern CursorManager mainWindowCursorManager;
extern CursorManager editWindowCursorManager;
