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
#include "cursormanager.h"

CursorManager mainWindowCursorManager;
CursorManager editWindowCursorManager;

Glib::RefPtr<Gdk::Texture>
CursorManager::CursorInfo::generateTexture(int cursor_size, double scale,
                                           int& out_width, int& out_height,
                                           int& out_hotspot_x, int& out_hotspot_y)
{
    out_width = cursor_size * scale;
    out_height = cursor_size * scale;

    out_hotspot_x = out_width / 2.0 * hotspot_x;
    out_hotspot_y = out_height / 2.0 * hotspot_y;

    // If the cached texture is already correctly sized, just return it again
    if (texture && texture->get_width() == out_width && texture->get_height() == out_height) {
        return texture;
    }

    texture = svg->createTexture(out_width, out_height);
    return texture;
}

void CursorManager::init (const Glib::RefPtr<Gtk::Window>& mainWindow)
{

    display = Gdk::Display::get_default ();
#ifndef NDEBUG

    if (!display) {
        printf("Error: no default display!\n");
    }

#endif

    cAdd        = createCursor("crosshair-hicontrast", "copy");
    cAddPicker  = createCursor("color-picker-add-hicontrast", "copy", -0.666, 0.75);
    cCropDraw   = createCursor("crop-point-hicontrast", "crosshair", -0.75, 0.75);
    cCrosshair  = createCursor("crosshair-hicontrast", "crosshair");
    cEmpty      = createCursor("empty", "none");
    cHandClosed = createCursor("hand-closed-hicontrast", "grabbing");
    cHandOpen   = createCursor("hand-open-hicontrast", "grab");
    cMoveBL     = createCursor("node-move-sw-ne-hicontrast", "nesw-resize");
    cMoveBR     = createCursor("node-move-nw-se-hicontrast", "nwse-resize");
    cMoveL      = createCursor("node-move-x-hicontrast", "ew-resize");
    cMoveR      = createCursor("node-move-x-hicontrast", "ew-resize");
    cMoveTL     = createCursor("node-move-nw-se-hicontrast", "nwse-resize");
    cMoveTR     = createCursor("node-move-sw-ne-hicontrast", "nesw-resize");
    cMoveX      = createCursor("node-move-x-hicontrast", "ns-resize");
    cMoveXY     = createCursor("node-move-xy-hicontrast", "all-scroll");
    cMoveY      = createCursor("node-move-y-hicontrast", "ns-resize");
    cRotate     = createCursor("rotate-aroundnode-hicontrast", "all-scroll");
    cWB         = createCursor("color-picker-hicontrast", "copy", -0.666, 0.75);
    cWait       = createCursor("gears", "progress");

    window = mainWindow;
}

Glib::RefPtr<CursorManager::CursorInfo>
CursorManager::createCursor(const Glib::ustring& name, const Glib::ustring& fallback,
                            double hotspot_x, double hotspot_y)
{
    auto info = std::make_shared<CursorInfo>();
    info->hotspot_x = hotspot_x;
    info->hotspot_y = hotspot_y;

    auto fallback_cursor = Gdk::Cursor::create(fallback);

    info->svg = SvgPaintableWrapper::createFromIcon(name);
    if (!info->svg) {
        info->cursor = fallback_cursor;
    } else {
        info->cursor = Gdk::Cursor::create_from_slot(
            sigc::mem_fun(*info, &CursorInfo::generateTexture),
            fallback_cursor);
    }

    return info;
}

/* Set the cursor of the given window */
void CursorManager::setCursor (const Glib::RefPtr<Gtk::Window>& window, CursorShape shape)
{
    switch (shape)
    {
        case CursorShape::CSAddColPicker:
            window->set_cursor(cAddPicker->cursor);
            break;
        case CursorShape::CSArrow:
            window->set_cursor(); // set_cursor without any arguments to select system default
            break;
        case CursorShape::CSCropSelect:
            window->set_cursor(cCropDraw->cursor);
            break;
        case CursorShape::CSCrosshair:
            window->set_cursor(cCrosshair->cursor);
            break;
        case CursorShape::CSEmpty:
            window->set_cursor(cEmpty->cursor);
            break;
        case CursorShape::CSHandClosed:
            window->set_cursor(cHandClosed->cursor);
            break;
        case CursorShape::CSHandOpen:
            window->set_cursor(cHandOpen->cursor);
            break;
        case CursorShape::CSMove:
            window->set_cursor(cHandClosed->cursor);
            break;
        case CursorShape::CSMove1DH:
            window->set_cursor(cMoveX->cursor);
            break;
        case CursorShape::CSMove1DV:
            window->set_cursor(cMoveY->cursor);
            break;
        case CursorShape::CSMove2D:
            window->set_cursor(cMoveXY->cursor);
            break;
        case CursorShape::CSMoveLeft:
            window->set_cursor(cMoveL->cursor);
            break;
        case CursorShape::CSMoveRight:
            window->set_cursor(cMoveR->cursor);
            break;
        case CursorShape::CSMoveRotate:
            window->set_cursor(cRotate->cursor);
            break;
        case CursorShape::CSPlus:
            window->set_cursor(cAdd->cursor);
            break;
        case CursorShape::CSResizeBottomLeft:
            window->set_cursor(cMoveBL->cursor);
            break;
        case CursorShape::CSResizeBottomRight:
            window->set_cursor(cMoveBR->cursor);
            break;
        case CursorShape::CSResizeDiagonal:
            window->set_cursor(cMoveXY->cursor);
            break;
        case CursorShape::CSResizeHeight:
            window->set_cursor(cMoveY->cursor);
            break;
        case CursorShape::CSResizeTopLeft:
            window->set_cursor(cMoveTL->cursor);
            break;
        case CursorShape::CSResizeTopRight:
            window->set_cursor(cMoveTR->cursor);
            break;
        case CursorShape::CSResizeWidth:
            window->set_cursor(cMoveX->cursor);
            break;
        case CursorShape::CSSpotWB:
            window->set_cursor(cWB->cursor);
            break;
        case CursorShape::CSStraighten:
            window->set_cursor(cRotate->cursor);
            break;
        case CursorShape::CSUndefined:
            break;
        case CursorShape::CSWait:
            window->set_cursor(cWait->cursor);
            break;
        default:
            window->set_cursor(cCrosshair->cursor);
    }
}

void CursorManager::setWidgetCursor (const Glib::RefPtr<Gtk::Window>& window, CursorShape shape)
{
    if (window->get_display() == mainWindowCursorManager.display) {
        mainWindowCursorManager.setCursor(window, shape);
    } else if (window->get_display() == editWindowCursorManager.display) {
        editWindowCursorManager.setCursor(window, shape);
    }

#ifndef NDEBUG
    else {
        printf("CursorManager::setWidgetCursor  /  Error: Display not found!\n");
    }

#endif
}

void CursorManager::setCursorOfMainWindow (const Glib::RefPtr<Gtk::Window>& window, CursorShape shape)
{
    if (window->get_display() == mainWindowCursorManager.display) {
        mainWindowCursorManager.setCursor(shape);
    } else if (window->get_display() == editWindowCursorManager.display) {
        editWindowCursorManager.setCursor(shape);
    }

#ifndef NDEBUG
    else {
        printf("CursorManager::setCursorOfMainWindow  /  Error: Display not found!\n");
    }

#endif
}

/* Set the cursor of the main window */
void CursorManager::setCursor (CursorShape shape)
{
    setCursor (window, shape);
}

