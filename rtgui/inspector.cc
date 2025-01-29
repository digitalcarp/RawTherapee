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
#include "inspector.h"
#include "guiutils.h"
#include <gtkmm.h>
#include "cursormanager.h"
#include "guiutils.h"
#include "multilangmgr.h"
#include "options.h"
#include "pathutils.h"
#include "rtscalable.h"
#include "rtengine/previewimage.h"
#include "rtengine/rt_math.h"

InspectorBuffer::InspectorBuffer(const Glib::ustring &imagePath) : currTransform(0), fromRaw(false)
{
    if (!imagePath.empty() && Glib::file_test(imagePath, Glib::FileTest::EXISTS) && !Glib::file_test(imagePath, Glib::FileTest::IS_DIR)) {
        imgPath = imagePath;

        // generate thumbnail image
        Glib::ustring ext = getExtension (imagePath);

        if (ext.empty()) {
            imgPath.clear();
            return;
        }

        rtengine::PreviewImage pi(imagePath, ext, rtengine::PreviewImage::PIM_EmbeddedOrRaw);
        Cairo::RefPtr<Cairo::ImageSurface> imageSurface = pi.getImage();

        if (imageSurface) {
            imgBuffer.setSurface(imageSurface);
            fromRaw = true;
        } else {
            imgPath.clear();
        }
    }
}

/*
InspectorBuffer::~InspectorBuffer() {
}
*/

//int InspectorBuffer::infoFromImage (const Glib::ustring& fname)
//{
//
//    rtengine::FramesMetaData* idata = rtengine::FramesMetaData::fromFile (fname, nullptr, true);
//
//    if (!idata) {
//        return 0;
//    }
//
//    int deg = 0;
//
//    if (idata->hasExif()) {
//        if      (idata->getOrientation() == "Rotate 90 CW" ) {
//            deg = 90;
//        } else if (idata->getOrientation() == "Rotate 180"   ) {
//            deg = 180;
//        } else if (idata->getOrientation() == "Rotate 270 CW") {
//            deg = 270;
//        }
//    }
//
//    delete idata;
//    return deg;
//}

Inspector::Inspector () : currImage(nullptr), scaled(false), scale(1.0), zoomScale(1.0), zoomScaleBegin(1.0), active(false), pinned(false), dirty(false), fullscreen(true), keyDown(false), windowShowing(false)
{
    set_name("Inspector");

    if (!options.inspectorWindow) {
        window = nullptr;
    }
    else {
        window = new Gtk::Window();
        window->set_name("InspectorWindow");
        window->set_title("RawTherapee " + M("INSPECTOR_WINDOW_TITLE"));
        window->set_visible(false);

        auto keyController = Gtk::EventControllerKey::create();
        keyController->signal_key_pressed().connect(
            sigc::mem_fun(*this, &Inspector::on_key_press), false);
        keyController->signal_key_released().connect(
            sigc::mem_fun(*this, &Inspector::on_key_release));
        window->add_controller(keyController);

        window->signal_hide().connect(sigc::mem_fun(*this, &Inspector::on_window_hide));
        window->property_fullscreened().signal_changed().connect(
            sigc::mem_fun(*this, &Inspector::onFullscreenChange));

        auto clickController = Gtk::GestureClick::create();
        clickController->signal_pressed().connect(
            sigc::mem_fun(*this, &Inspector::on_button_press_event));
        window->add_controller(clickController);

        auto motionController = Gtk::EventControllerMotion::create();
        motionController->signal_motion().connect(
            sigc::mem_fun(*this, &Inspector::on_motion_notify_event));
        window->add_controller(motionController);

        scrollController = Gtk::EventControllerScroll::create();
        scrollController->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
        scrollController->signal_scroll().connect(
            sigc::mem_fun(*this, &Inspector::on_scroll_event), false);
        window->add_controller(scrollController);

        gestureZoom = Gtk::GestureZoom::create();
        gestureZoom->signal_begin().connect(sigc::mem_fun(*this, &Inspector::on_zoom_begin));
        gestureZoom->signal_scale_changed().connect(
            sigc::mem_fun(*this, &Inspector::on_zoom_scale_changed));
        window->add_controller(gestureZoom);

        window->set_child(*this);
        window->set_size_request(500, 500);
        window->fullscreen();
        initialized = false; // delay init to avoid flickering on some systems
        active = true; // always track inspected thumbnails
    }

    set_draw_func(sigc::mem_fun(*this, &Inspector::on_draw));
}

Inspector::~Inspector()
{
    deleteBuffers();
    if (window)
        delete window;
}

void Inspector::showWindow(bool pinned, bool scaled)
{
    if (!window || windowShowing)
        return;

    // initialize when shown first
    if (!initialized) {
        initialized = true;
    }

    // show inspector window
    this->scaled = scaled;
    window->set_visible(true);
    this->pinned = pinned;
    windowShowing = true;

    // update content when becoming visible
    switchImage(next_image_path);
    mouseMove(next_image_pos, 0);
}

void Inspector::hideWindow()
{
    if (!window) {
        return;
    }
    window->set_visible(false);
}

void Inspector::on_key_release(guint keyval, guint keycode, Gdk::ModifierType state)
{
    keyDown = false;

    if (!window) return;

    if (!pinned) {
        switch (keyval) {
        case GDK_KEY_f:
        case GDK_KEY_F:
            zoomScale = 1.0;
            window->set_visible(false);
            return;
        }
    }
}

bool Inspector::on_key_press(guint keyval, guint keycode, Gdk::ModifierType state)
{
    if (!window)
        return false;

    if (keyDown) {
        return true;
    }

    keyDown = true;

    switch (keyval) {
    case GDK_KEY_z:
    case GDK_KEY_F:
        // show image unscaled in 100% view
        if (pinned || scaled)
            zoomScale = 1.0; // reset if not key hold
        scaled = false;
        queue_draw();
        return true;
    case GDK_KEY_f:
        // show image scaled to window size
        if (pinned || !scaled)
            zoomScale = 1.0; // reset if not key hold
        scaled = true;
        queue_draw();
        return true;
    case GDK_KEY_F11:
        // toggle fullscreen
        if (fullscreen)
            window->unfullscreen();
        else
            window->fullscreen();
        fullscreen = !fullscreen;
        return true;
    case GDK_KEY_Escape:
        // hide window
        zoomScale = 1.0;
        window->set_visible(false);
        return true;
    }

    keyDown = false;

    return false;
}

void Inspector::on_window_hide()
{
    windowShowing = false;
}

void Inspector::onFullscreenChange()
{
    fullscreen = window->property_fullscreened().get_value();
}

void Inspector::on_button_press_event(int n_press, double x, double y)
{
    if (!window) return;

    if (n_press == 1) {
        button_pos.set(x, y);
        if (!pinned) {
            // pin window with mouse click
            pinned = true;
        }
    }
}

void Inspector::on_motion_notify_event(double x, double y)
{
    if (!currImage || !window) return;

    int deviceScale = get_scale_factor();
    double delta_x = (button_pos.x - x) * deviceScale;
    double delta_y = (button_pos.y - y) * deviceScale;
    int imW = currImage->imgBuffer.getWidth();
    int imH = currImage->imgBuffer.getHeight();

    moveCenter(delta_x, delta_y, imW, imH, deviceScale);
    button_pos.set(x, y);

    if (!dirty) {
        dirty = true;
        queue_draw();
    }
}

bool Inspector::on_scroll_event(double dx, double dy)
{
    if (!currImage || !window)
        return false;

    pinned = true;

    bool alt = isAltDown(scrollController->get_current_event_state());
    int deviceScale = get_scale_factor();
    int imW = currImage->imgBuffer.getWidth();
    int imH = currImage->imgBuffer.getHeight();

    double delta_x = 0.0;
    double delta_y = 0.0;
    if (scrollController->get_unit() == Gdk::ScrollUnit::SURFACE) {
        delta_x = dx;
        delta_y = dy;
    } else {
        delta_x = 5.0 * dx * deviceScale * imH / 100;
        delta_y = 5.0 * dy * deviceScale * imH / 100;
    }

    if ((options.zoomOnScroll && !alt) || (!options.zoomOnScroll && alt)) {
        // zoom
        auto event = scrollController->get_current_event();
        double x = 0.0;
        double y = 0.0;
        event->get_position(x, y);
        beginZoom(x, y);
        if (std::fabs(delta_y) > std::fabs(delta_x))
            on_zoom_scale_changed(1.0 - (double)delta_y / imH / deviceScale);
        else
            on_zoom_scale_changed(1.0 - (double)delta_x / imW / deviceScale);
        return true;
    }

    // scroll
    moveCenter(delta_x, delta_y, imW, imH, deviceScale);

    if (!dirty) {
        dirty = true;
        queue_draw();
    }

    return true;
}

void Inspector::moveCenter(double delta_x, double delta_y, int imW, int imH, int deviceScale)
{
    rtengine::Coord margin; // limit to image size
    margin.x = rtengine::min<int>(window->get_width() * deviceScale / scale, imW) / 2;
    margin.y = rtengine::min<int>(window->get_height() * deviceScale / scale, imH) / 2;
    center.set(rtengine::LIM<double>(center.x + delta_x / scale, margin.x, imW - margin.x),
               rtengine::LIM<double>(center.y + delta_y / scale, margin.y, imH - margin.y));
}

void Inspector::beginZoom(double x, double y)
{
    if (!currImage || !window)
        return;

    int deviceScale = get_scale_factor();
    int imW = currImage->imgBuffer.getWidth();
    int imH = currImage->imgBuffer.getHeight();

    // limit center to image size
    moveCenter(0, 0, imW, imH, deviceScale);

    // store center and current position for zooming
    double cur_scale = zoomScale;
    if (scaled) {
        double winW = get_width() * deviceScale;
        double winH = get_height() * deviceScale;
        int imW = rtengine::max<int>(currImage->imgBuffer.getWidth(), 1);
        int imH = rtengine::max<int>(currImage->imgBuffer.getHeight(), 1);
        cur_scale *= rtengine::min<double>(winW / imW, winH / imH);
    }
    dcenterBegin.x = (x - get_width() / 2.) / cur_scale * deviceScale;
    dcenterBegin.y = (y - get_height() / 2.) / cur_scale * deviceScale;
    centerBegin = center;
    zoomScaleBegin = zoomScale;

}

void Inspector::on_zoom_begin(Gdk::EventSequence *s)
{
    double x, y;
    pinned = true;
    if (gestureZoom->get_point(s, x, y))
        beginZoom(x, y);
}

void Inspector::on_zoom_scale_changed(double zscale)
{
    if (!currImage || !window)
        return;

    zoomScale = rtengine::LIM<double>(zoomScaleBegin * zscale, 0.01, 16.0);
    double dcenterRatio = 1.0 - zoomScaleBegin / zoomScale;
    center.x = centerBegin.x + dcenterBegin.x * dcenterRatio;
    center.y = centerBegin.y + dcenterBegin.y * dcenterRatio;

    if (!dirty) {
        dirty = true;
        queue_draw();
    }
}

void Inspector::on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height)
{
    dirty = false;

    if (!active) {
        active = true;
    }

    // cleanup the region
    if (currImage && currImage->imgBuffer.surfaceCreated()) {
        // this will eventually create/update the off-screen pixmap

        // compute the displayed area
        rtengine::Coord2D availableSize;
        rtengine::Coord2D topLeft;
        rtengine::Coord topLeftInt;
        rtengine::Coord2D dest(0, 0);
        int deviceScale = window? get_scale_factor(): 1;
        availableSize.x = width * deviceScale;
        availableSize.y = height * deviceScale;
        int imW = rtengine::max<int>(currImage->imgBuffer.getWidth(), 1);
        int imH = rtengine::max<int>(currImage->imgBuffer.getHeight(), 1);
        scale = rtengine::min(1., rtengine::min<double>(availableSize.x / imW, availableSize.y / imH));
        if (scaled) {
            // reduce size of image to fit into window, no further zoom down
            zoomScale = rtengine::max<double>(zoomScale, 1.0);
            scale *= zoomScale;
        }
        else {
            // limit zoom to fill at least complete window or 1:1
            zoomScale = rtengine::max<double>(zoomScale, rtengine::min<double>(1.0, scale));
            scale = zoomScale;
        }
        availableSize.x /= scale;
        availableSize.y /= scale;

        if (imW < availableSize.x) {
            // center the image in the available space along X
            topLeft.x = 0;
            dest.x = (availableSize.x - imW) / 2.;
        } else {
            // partial image display
            // double clamp
            topLeft.x = center.x + availableSize.x / 2.;
            topLeft.x = rtengine::min<double>(topLeft.x, imW);
            topLeft.x -= availableSize.x;
            topLeft.x = rtengine::max<double>(topLeft.x, 0);
        }

        if (imH < availableSize.y) {
            // center the image in the available space along Y
            topLeft.y = 0;
            dest.y = (availableSize.y - imH) / 2.;
        } else {
            // partial image display
            // double clamp
            topLeft.y = center.y + availableSize.y / 2.;
            topLeft.y = rtengine::min<double>(topLeft.y, imH);
            topLeft.y -= availableSize.y;
            topLeft.y = rtengine::max<double>(topLeft.y, 0);
        }
        //printf("center: %d, %d   (img: %d, %d)  (availableSize: %d, %d)  (topLeft: %d, %d)\n", center.x, center.y, imW, imH, availableSize.x, availableSize.y, topLeft.x, topLeft.y);

        topLeftInt.x = floor(topLeft.x);
        topLeftInt.y = floor(topLeft.y);

        // define the destination area
        currImage->imgBuffer.setDrawRectangle(
            getToplevelWindow(this),
            dest.x, dest.y,
            rtengine::min<int>(ceil(availableSize.x + (topLeft.x - topLeftInt.x) - 2 * dest.x), imW),
            rtengine::min<int>(ceil(availableSize.y + (topLeft.y - topLeftInt.y) - 2 * dest.y), imH),
            false);
        currImage->imgBuffer.setSrcOffset(topLeftInt.x, topLeftInt.y);

        if (!currImage->imgBuffer.surfaceCreated()) {
            return;
        }

        // Draw!

        Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

        if (!window) {
            // draw the background
            style->render_background(cr, 0, 0, width, height);
        }

        bool scaledImage = scale != 1.0;
        if (!window || (deviceScale == 1 && !scaledImage)) {
            // standard drawing
            currImage->imgBuffer.copySurface(cr);
        }
        else {
            // consider device scale and image scale
            if (deviceScale > 1) {
#ifdef __APPLE__
                // use full device resolution and let it scale the image (macOS)
                cairo_surface_set_device_scale(cr->get_target()->cobj(), scale, scale);
                scaledImage = false;
#else
                cr->scale(1. / deviceScale, 1. / deviceScale);
#endif
            }
            int viewW = rtengine::min<int>(imW, ceil(availableSize.x + (topLeft.x - topLeftInt.x)));
            int viewH = rtengine::min<int>(imH, ceil(availableSize.y + (topLeft.y - topLeftInt.y)));
            Glib::RefPtr<Gdk::Pixbuf> crop = Gdk::Pixbuf::create(currImage->imgBuffer.getSurface(), topLeftInt.x, topLeftInt.y, viewW, viewH);
            if (!scaledImage) {
                Gdk::Cairo::set_source_pixbuf(cr, crop, dest.x, dest.y);
            }
            else {
                double dx = scale * (dest.x + topLeftInt.x - topLeft.x);
                double dy = scale * (dest.y + topLeftInt.y - topLeft.y);
                // scale crop as the device does not seem to support it (Linux)
                crop = crop->scale_simple(round(viewW*scale), round(viewH*scale), Gdk::InterpType::BILINEAR);
                Gdk::Cairo::set_source_pixbuf(cr, crop, dx, dy);
            }
            cr->paint();
        }

        if (!window) {
            // draw the frame
            style->render_frame (cr, 0, 0, width, height);
        }
    }
}

void Inspector::mouseMove (rtengine::Coord2D pos, int transform)
{
    if (!active) {
        return;
    }

    next_image_pos = pos;

    // skip actual update of content when not visible
    if (window && !window->get_visible())
        return;

    if (currImage) {
        center.set(rtengine::LIM01(pos.x)*double(currImage->imgBuffer.getWidth()), rtengine::LIM01(pos.y)*double(currImage->imgBuffer.getHeight()));
    } else {
        center.set(0, 0);
    }

    queue_draw();
}

void Inspector::switchImage (const Glib::ustring &fullPath)
{
    if (!active) {
        return;
    }

    if (delayconn.connected()) {
        delayconn.disconnect();
    }

    next_image_path = fullPath;

    // skip actual update of content when not visible
    if (window && !window->get_visible())
        return;

    if (!options.inspectorDelay) {
        doSwitchImage();
    } else {
        delayconn = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Inspector::doSwitchImage), options.inspectorDelay);
    }
}


bool Inspector::doSwitchImage()
{
    Glib::ustring fullPath = next_image_path;

    // we first check the size of the list, it may have been changed in Preference
    if (images.size() > size_t(options.maxInspectorBuffers)) {
        // deleting the last entries
        for (size_t i = images.size() - 1; i > size_t(options.maxInspectorBuffers - 1); --i) {
            delete images.at(i);
            images.at(i) = nullptr;
        }

        // resizing down
        images.resize(options.maxInspectorBuffers);
    }

    if (fullPath.empty()) {
        currImage = nullptr;
        queue_draw();
    } else {
        bool found = false;

        for (size_t i = 0; i < images.size(); ++i) {
            if (images.at(i) != nullptr && images.at(i)->imgPath == fullPath) {
                currImage = images.at(i);

                // rolling the list 1 step to the beginning
                for (size_t j = i; j < images.size() - 1; ++j) {
                    images.at(j) = images.at(j + 1);
                }

                images.at(images.size() - 1) = currImage; // move the last used image to the tail
                found = true;
                break;
            }
        }

        if (!found) {
            if (images.size() == size_t(options.maxInspectorBuffers)) {
                // The list is full, delete the first entry
                delete images.at(0);
                images.erase(images.begin());
            }

            // Loading a new image
            InspectorBuffer *iBuffer = new InspectorBuffer(fullPath);

            // and add it to the tail
            if (!iBuffer->imgPath.empty()) {
                images.push_back(iBuffer);
                currImage = images.at(images.size() - 1);
            } else {
                delete iBuffer;
                currImage = nullptr;
            }
        }
    }

    return true;
}

void Inspector::deleteBuffers ()
{
    for (size_t i = 0; i < images.size(); ++i) {
        if (images.at(i) != nullptr) {
            delete images.at(i);
            images.at(i) = nullptr;
        }
    }

    images.resize(0);
    currImage = nullptr;
}

void Inspector::flushBuffers ()
{
    if (!active) {
        return;
    }

    deleteBuffers();
}

void Inspector::setActive(bool state)
{
    if (!state) {
        flushBuffers();
    }

    if (!window)
        active = state;
}


Gtk::SizeRequestMode Inspector::get_request_mode_vfunc () const
{
    return Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void Inspector::measure_vfunc(Gtk::Orientation orientation, int for_size,
                              int& minimum, int& natural,
                              int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = RTScalable::scalePixelSize(50);
        natural = RTScalable::scalePixelSize(200);
    } else {
        minimum = RTScalable::scalePixelSize(50);
        natural = RTScalable::scalePixelSize(300);
    }

    minimum_baseline = -1;
    natural_baseline = -1;
}
