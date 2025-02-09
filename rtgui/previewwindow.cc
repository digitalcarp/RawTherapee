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
#include "previewwindow.h"

#include "cursormanager.h"
#include "guiutils.h"
#include "hidpi.h"
#include "imagearea.h"
#include "options.h"
#include "rtscalable.h"

#include "rtengine/procparams.h"

PreviewWindow::PreviewWindow () : previewHandler(nullptr), mainCropWin(nullptr), imageArea(nullptr), imgW(0), imgH(0),
    zoom(0.0), press_x(0), press_y(0), isMoving(false), needsUpdate(false), cursor_type(CSUndefined)

{
    set_name("PreviewWindow");
    get_style_context()->add_class("drawingarea");

    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_pressed().connect(
        sigc::mem_fun(*this, &PreviewWindow::on_button_press_event));
    click->signal_released().connect(
        sigc::mem_fun(*this, &PreviewWindow::on_button_release_event));
    add_controller(click);

    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(
        sigc::mem_fun(*this, &PreviewWindow::on_motion_notify_event));
    add_controller(motion);

    set_draw_func(sigc::mem_fun(*this, &PreviewWindow::on_draw));
    signal_resize().connect( sigc::mem_fun(*this, &PreviewWindow::on_resized) );
}

void PreviewWindow::getObservedFrameArea (int& x, int& y, int& w, int& h)
{

    if (mainCropWin) {
        int cropX, cropY, cropW, cropH;
        mainCropWin->getCropRectangle (cropX, cropY, cropW, cropH);
        // translate it to screen coordinates
        x = round(cropX * zoom);
        y = round(cropY * zoom);
        w = round(cropW * zoom);
        h = round(cropH * zoom);
    }
}

void PreviewWindow::updatePreviewImage ()
{
    if (!get_realized()) {
        needsUpdate = true;
        return;
    }

    backBuffer = nullptr;
    if (!previewHandler) return;

    int scale = RTScalable::getScaleForWidget(this);
    auto logical = hidpi::LogicalSize::forWidget(this);

    hidpi::DevicePixbuf result = previewHandler->getRoughImage(logical, scale, zoom);
    if (!result.pixbuf()) return;
    
    hidpi::ScaledDeviceSize device = result.size();
    imgW = device.width;
    imgH = device.height;

    backBuffer = Cairo::RefPtr<BackBuffer> ( new BackBuffer(
        device.width, device.height, Cairo::Surface::Format::ARGB32) );
    Cairo::RefPtr<Cairo::ImageSurface> surface = backBuffer->getSurface();
    hidpi::setDeviceScale(surface, device.device_scale);

    Cairo::RefPtr<Cairo::Context> cc = Cairo::Context::create(surface);
    cc->set_source_rgba (0., 0., 0., 0.);
    cc->set_operator (Cairo::Context::Operator::CLEAR);
    cc->paint ();
    cc->set_operator (Cairo::Context::Operator::OVER);
    cc->set_antialias(Cairo::ANTIALIAS_NONE);
    cc->set_line_join(Cairo::Context::LineJoin::MITER);

    Gdk::Cairo::set_source_pixbuf(cc, result.pixbuf(), 0, 0);
    auto pattern = hidpi::getSourceForSurface(cc);
    hidpi::setDeviceScale(pattern->get_surface(), device.device_scale);
    cc->rectangle(0, 0, device.width, device.height);
    cc->fill();

    if (previewHandler->getCropParams().enabled) {
        rtengine::procparams::CropParams cparams = previewHandler->getCropParams();
        switch (options.cropGuides) {
        case Options::CROP_GUIDE_NONE:
            cparams.guide = rtengine::procparams::CropParams::Guide::NONE;
            break;
        case Options::CROP_GUIDE_FRAME:
            cparams.guide = rtengine::procparams::CropParams::Guide::FRAME;
            break;
        default:
            break;
        }
        drawCrop (cc, 0, 0, imgW, imgH, imgW, imgH, 0, 0, zoom, cparams, true, false);
    }
}

void PreviewWindow::setPreviewHandler (PreviewHandler* ph)
{

    previewHandler = ph;

    if (previewHandler) {
        previewHandler->addPreviewImageListener (this);
    }
}

void PreviewWindow::on_resized (int width, int height)
{

    updatePreviewImage ();
    queue_draw ();
}

void PreviewWindow::on_draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    style->render_background(cr, 0, 0, width, height);

    if (!backBuffer) {
        return;
    }

    int bufferW, bufferH;
    bufferW = backBuffer->getWidth();
    bufferH = backBuffer->getHeight();

    if (!mainCropWin && imageArea) {
        mainCropWin = imageArea->getMainCropWindow ();

        if (mainCropWin) {
            mainCropWin->addCropWindowListener (this);
        }
    }

    auto deviceSize = hidpi::ScaledDeviceSize::forWidget(this);
    const int scale = deviceSize.device_scale;

    if ((deviceSize.width != bufferW && deviceSize.height != bufferH) || needsUpdate) {
        needsUpdate = false;
        updatePreviewImage ();
    }

    cr->save();

    int x_offset = static_cast<double>(deviceSize.width - bufferW) / scale / 2;
    int y_offset = static_cast<double>(deviceSize.height - bufferH) / scale / 2;
    cr->translate(x_offset, y_offset);

    backBuffer->copySurface(cr, nullptr);

    if (mainCropWin && zoom > 0.0) {
        int x, y, w, h;
        getObservedFrameArea (x, y, w, h);
        if (x>0 || y>0 || w < imgW || h < imgH) {
            const double s = scale;
            double rectX = x + 0.5 * s;
            double rectY = y + 0.5 * s;
            double rectW = std::min(w, (int)(imgW - x)) - 1 * s;
            double rectH = std::min(h, (int)(imgH - y)) - 1 * s;

            // draw a black "shadow" line
            cr->set_source_rgba (0.0, 0.0, 0.0, 0.65);
            cr->set_line_width (1 * s);
            cr->set_line_join(Cairo::Context::LineJoin::MITER);
            cr->rectangle (rectX + 1 * s, rectY + 1 * s, rectW - 2 * s, rectH - 2 * s);
            cr->stroke ();

            // draw a "frame" line. Color of frame line can be set in preferences
            cr->set_source_rgba(options.navGuideBrush[0], options.navGuideBrush[1], options.navGuideBrush[2], options.navGuideBrush[3]); //( 1.0, 1.0, 1.0, 1.0);
            cr->rectangle (rectX, rectY, rectW, rectH);
            cr->stroke ();
        }
    }

    cr->restore();

    style->render_frame (cr, 0, 0, width, height);
}

void PreviewWindow::previewImageChanged ()
{

    updatePreviewImage ();
    queue_draw ();
}

void PreviewWindow::setImageArea (ImageArea* ia)
{

    imageArea = ia;
    mainCropWin = ia->getMainCropWindow ();

    if (mainCropWin) {
        mainCropWin->addCropWindowListener (this);
    }
}

void PreviewWindow::cropPositionChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::cropWindowSizeChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::cropZoomChanged(CropWindow* w)
{
    queue_draw ();
}

void PreviewWindow::initialImageArrived()
{
}

void PreviewWindow::on_motion_notify_event (double x, double y)
{

    if (!mainCropWin) {
        return;
    }

    int fx, fy, w, h;
    getObservedFrameArea (fx, fy, w, h);
    if (fx>0 || fy>0 || w < imgW || h < imgH) {
        bool inside = x > fx - 6 && x < fx + w - 1 + 6 && y > fy - 6 && y < fy + h - 1 + 6;

        CursorShape newType;

        if (isMoving) {
            mainCropWin->remoteMove ((x - press_x) / zoom, (y - press_y) / zoom);
            press_x = x;
            press_y = y;
            newType = CSHandClosed;
        } else if (inside) {
            newType = CSHandOpen;
        } else {
            newType = CSArrow;
        }

        if (newType != cursor_type) {
            cursor_type = newType;
            CursorManager::setWidgetCursor(getToplevelWindow(this), cursor_type);
        }
    }
}

void PreviewWindow::on_button_press_event (int n_press, double x, double y)
{

    if (!mainCropWin) {
        return;
    }

    int fx, fy, w, h;
    getObservedFrameArea (fx, fy, w, h);
    if (fx>0 || fy>0 || w < imgW || h < imgH) {

        if (!isMoving) {
            isMoving = true;

            press_x = x;
            press_y = y;

            if (cursor_type != CSHandClosed) {
                cursor_type = CSHandClosed;
                CursorManager::setWidgetCursor(getToplevelWindow(this), cursor_type);
            }
        }
    }
}

void PreviewWindow::on_button_release_event (int n_press, double x, double y)
{

    if (!mainCropWin) {
        return;
    }

    if (isMoving) {
        isMoving = false;

        if (cursor_type != CSArrow) {
            cursor_type = CSArrow;
            CursorManager::setWidgetCursor(getToplevelWindow(this), cursor_type);
        }

        mainCropWin->remoteMoveReady ();
    }
}

Gtk::SizeRequestMode PreviewWindow::get_request_mode_vfunc () const
{
    return Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void PreviewWindow::measure_vfunc(
    Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
    int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = RTScalable::scalePixelSize(80);
        natural = RTScalable::scalePixelSize(120);
    } else {
        minimum = RTScalable::scalePixelSize(50);
        natural = RTScalable::scalePixelSize(100);
    }

    minimum_baseline = -1;
    natural_baseline = -1;
}
