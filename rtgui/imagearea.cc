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
#include "imagearea.h"

#include <ctime>
#include <cmath>

#include "rtengine/refreshmap.h"
#include "rtengine/procparams.h"

#include "cropwindow.h"
#include "hidpi.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtscalable.h"

ImageArea::ImageArea (ImageAreaPanel* p) : parent(p), fullImageWidth(0), fullImageHeight(0)
{

    cropgl = nullptr;
    pmlistener = nullptr;
    pmhlistener = nullptr;
    focusGrabber = nullptr;
    flawnOverWindow = nullptr;
    mainCropWindow = nullptr;
    previewHandler = nullptr;
    showClippedH = false;
    showClippedS = false;
    listener = nullptr;

    zoomPanel = Gtk::manage (new ZoomPanel (this));
    indClippedPanel = Gtk::manage (new IndicateClippedPanel (this));
    previewModePanel =  Gtk::manage (new PreviewModePanel (this));
    previewModePanel->get_style_context()->add_class("narrowbuttonbox");

    set_draw_func(sigc::mem_fun(*this, &ImageArea::on_draw));
    signal_resize().connect( sigc::mem_fun(*this, &ImageArea::on_resized) );

    dirty = false;
    ipc = nullptr;
    iLinkedImageArea = nullptr;

    clickController = Gtk::GestureClick::create();
    clickController->signal_pressed().connect(
        sigc::mem_fun(*this, &ImageArea::on_button_press_event));
    clickController->signal_released().connect(
        sigc::mem_fun(*this, &ImageArea::on_button_release_event));
    add_controller(clickController);

    motionController = Gtk::EventControllerMotion::create();
    motionController->signal_leave().connect(
        sigc::mem_fun(*this, &ImageArea::on_leave_notify_event));
    add_controller(motionController);

    scrollController = Gtk::EventControllerScroll::create();
    scrollController->signal_scroll().connect(
        sigc::mem_fun(*this, &ImageArea::on_scroll_event), false);
    add_controller(scrollController);
}

ImageArea::~ImageArea ()
{

    for (auto cropWin : cropWins) {
        delete cropWin;
    }

    cropWins.clear ();

    if (mainCropWindow) {
        delete mainCropWindow;
    }
}

void ImageArea::on_realize()
{
    Gtk::DrawingArea::on_realize();

    Cairo::FontOptions cfo;
    cfo.set_antialias (Cairo::ANTIALIAS_SUBPIXEL);
    get_pango_context ()->set_cairo_font_options (cfo);
}

void ImageArea::on_resized (int width, int height)
{
    if (ipc && width > 1) { // sometimes on_resize is called in some init state, causing wrong sizes
        if (!mainCropWindow) {
            mainCropWindow = new CropWindow (this, false, false);
            mainCropWindow->setDecorated (false);
            mainCropWindow->setFitZoomEnabled (true);
            mainCropWindow->addCropWindowListener (this);
            mainCropWindow->setCropGUIListener (cropgl);
            mainCropWindow->setPointerMotionListener (pmlistener);
            mainCropWindow->setPointerMotionHListener (pmhlistener);

            int deviceScale = RTScalable::getScaleForWidget(this);
            // Needs to be before setSize()
            mainCropWindow->cropHandler.setDeviceScale(deviceScale);

            mainCropWindow->setPosition (0, 0);
            mainCropWindow->setSize (width, height);  // this execute the refresh itself
            mainCropWindow->enable();  // start processing !
        } else {
            mainCropWindow->setSize (width, height);  // this execute the refresh itself
        }

        parent->syncBeforeAfterViews();
    }
}

rtengine::StagedImageProcessor* ImageArea::getImProcCoordinator() const
{
    return ipc;
}

void ImageArea::setImProcCoordinator(rtengine::StagedImageProcessor* ipc_)
{
    if( !ipc_ ) {
        focusGrabber = nullptr;

        for (auto cropWin : cropWins) {
            delete cropWin;
        }

        cropWins.clear();

        mainCropWindow->deleteColorPickers ();
        mainCropWindow->setObservedCropWin (nullptr);
    }

    ipc = ipc_;

}

void ImageArea::setPreviewHandler (PreviewHandler* ph)
{

    previewHandler = ph;
}

void ImageArea::setInfoText (Glib::ustring&& text)
{
    infotext = std::move(text);
    updateInfoTextBackBuffer();
}

void ImageArea::updateInfoTextBackBuffer()
{
    backBufferDeviceScale = RTScalable::getScaleForWidget(this);

    Glib::RefPtr<Pango::Context> context = get_pango_context () ;
    Pango::FontDescription fontd = context->get_font_description();

    // update font
    fontd.set_weight (Pango::Weight::BOLD);
    const int fontSize = 10; // pt
    // Non-absolute size is defined in "Pango units" and shall be multiplied by
    // Pango::SCALE from "pt":
    fontd.set_size (fontSize * Pango::SCALE);
    context->set_font_description (fontd);

    // create text layout
    Glib::RefPtr<Pango::Layout> ilayout = create_pango_layout("");
    ilayout->set_markup(infotext);

    // get size of the text block
    int iw, ih;
    ilayout->get_pixel_size (iw, ih);

    int bufferWidth = (iw + 16) * backBufferDeviceScale;
    int bufferHeight = (ih + 16) * backBufferDeviceScale;
    int bufferOffset = 8;

    // create BackBuffer
    iBackBuffer.setDrawRectangle(Cairo::Surface::Format::ARGB32, 0, 0, bufferWidth, bufferHeight, true);
    iBackBuffer.setDestPosition(bufferOffset, bufferOffset);
    hidpi::setDeviceScale(iBackBuffer.getSurface(), backBufferDeviceScale);

    Cairo::RefPtr<Cairo::Context> cr = iBackBuffer.getContext();

    // cleaning the back buffer (make it full transparent)
    cr->set_source_rgba (0., 0., 0., 0.);
    cr->set_operator (Cairo::Context::Operator::CLEAR);
    cr->paint ();
    cr->set_operator (Cairo::Context::Operator::OVER);

    // paint transparent black background
    cr->set_source_rgba (0., 0., 0., 0.5);
    cr->paint ();

    // paint text
    cr->set_source_rgb (1.0, 1.0, 1.0);
    cr->move_to (8, 8);
    ilayout->add_to_cairo_context (cr);
    cr->fill ();

}

void ImageArea::infoEnabled (bool e)
{

    if (options.showInfo != e) {
        options.showInfo = e;
        queue_draw ();
    }
}

CropWindow* ImageArea::getCropWindow (int x, int y)
{

    CropWindow* cw = mainCropWindow;

    for (auto cropWin : cropWins) {
        if (cropWin->isInside (x, y)) {
            return cropWin;
        }
    }

    return cw;
}

void ImageArea::redraw ()
{
    // dirty prevents multiple updates queued up
    if (!dirty) {
        dirty = true;
        queue_draw ();
    }
}

void ImageArea::switchPickerVisibility (bool isVisible)
{
    redraw();
}

void ImageArea::on_draw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    dirty = false;

    int deviceScale = RTScalable::getScaleForWidget(this);

    if (mainCropWindow) {
        if (deviceScale != mainCropWindow->cropHandler.getDeviceScale()) {
            for (const auto& win : cropWins) {
                win->cropHandler.setDeviceScale(deviceScale);
            }
            mainCropWindow->setSize(width, height);
        }

        mainCropWindow->expose (cr);
    }

    for (std::list<CropWindow*>::reverse_iterator i = cropWins.rbegin(); i != cropWins.rend(); ++i) {
        (*i)->expose (cr);
    }

    if (options.showInfo && !infotext.empty()) {
        if (deviceScale != backBufferDeviceScale) {
            updateInfoTextBackBuffer();
        }
        iBackBuffer.copySurface(cr);
    }
}


void ImageArea::on_motion_notify_event (double x, double y)
{
    lastMouseX = x;
    lastMouseY = y;
    Gdk::ModifierType eventState = motionController->get_current_event_state();
    int state = static_cast<int>(eventState);

    if (focusGrabber) {
        focusGrabber->pointerMoved (state, x, y);
    } else {
        CropWindow* cw = getCropWindow (x, y);

        if (cw) {
            if (cw != flawnOverWindow) {
                if (flawnOverWindow) {
                    flawnOverWindow->flawnOver(false);
                }

                cw->flawnOver(true);
                flawnOverWindow = cw;
            }

            cw->pointerMoved (state, x, y);
        } else if (flawnOverWindow) {
            flawnOverWindow->flawnOver(false);
            flawnOverWindow = nullptr;
        }
    }
}

void ImageArea::on_button_press_event (int n_press, double x, double y)
{
    unsigned int button = clickController->get_button();
    Gdk::ModifierType eventState = clickController->get_current_event_state();
    int state = static_cast<int>(eventState);

    if (focusGrabber) {
        focusGrabber->buttonPress (button, n_press, state, x, y);
    } else {
        CropWindow* cw = getCropWindow (x, y);

        if (cw) {
            cw->buttonPress (button, n_press, state, x, y);
        }
    }
}

bool ImageArea::on_scroll_event (double dx, double dy)
{
    auto event = scrollController->get_current_event();
    Gdk::ModifierType eventState = scrollController->get_current_event_state();
    int state = static_cast<int>(eventState);
    double x = -1;
    double y = -1;
    bool success = event->get_position(x, y);

//    printf("ImageArea::on_scroll_event / delta_x=%.5f, delta_y=%.5f, direction=%d, type=%d, send_event=%d\n",
//            event->delta_x, event->delta_y, (int)event->direction, (int)event->type, event->send_event);

    CropWindow* cw = success ? getCropWindow (x, y) : nullptr;
    if (cw) {
        cw->scroll (state, event->get_direction(), x, y, dx, dy);
    }

    return true;
}

void ImageArea::on_button_release_event (int n_press, double x, double y)
{
    unsigned int button = clickController->get_button();
    Gdk::ModifierType eventState = clickController->get_current_event_state();
    int state = static_cast<int>(eventState);

    if (focusGrabber) {
        focusGrabber->buttonRelease (button, n_press, state, x, y);
    } else {
        CropWindow* cw = getCropWindow (x, y);

        if (cw) {
            cw->buttonRelease (button, n_press, state, x, y);
        }
    }
}

void ImageArea::on_leave_notify_event()
{
    if (flawnOverWindow) {
        flawnOverWindow->flawnOver(false);
        flawnOverWindow = nullptr;
    }

    if (focusGrabber) {
        focusGrabber->flawnOver(false);
        focusGrabber->leaveNotify ();
    } else {
        CropWindow* cw = getCropWindow (lastMouseX, lastMouseY);

        if (cw) {
            cw->flawnOver(false);
            cw->leaveNotify ();
        }
    }
}

void ImageArea::subscribe(EditSubscriber *subscriber)
{
    EditDataProvider::subscribe(subscriber);

    mainCropWindow->setEditSubscriber(subscriber);
    for (auto cropWin : cropWins) {
        cropWin->setEditSubscriber(subscriber);
    }

    if (listener && listener->getToolBar()) {
        listener->getToolBar()->startEditMode ();
    }

    if (subscriber && subscriber->getEditingType() == ET_OBJECTS) {
        // In this case, no need to reprocess the image, so we redraw the image to display the geometry
        queue_draw();
    }
}

void ImageArea::unsubscribe()
{
    bool wasObjectType = false;
    EditSubscriber*  oldSubscriber = EditDataProvider::getCurrSubscriber();

    if (oldSubscriber && oldSubscriber->getEditingType() == ET_OBJECTS) {
        wasObjectType = true;
    }

    EditDataProvider::unsubscribe();

    // Ask the Crops to free-up edit mode buffers
    mainCropWindow->setEditSubscriber(nullptr);
    for (auto cropWin : cropWins) {
        cropWin->setEditSubscriber(nullptr);
    }

    setToolHand();

    if (listener && listener->getToolBar()) {
        listener->getToolBar()->stopEditMode ();
    }

    if (wasObjectType) {
        queue_draw();
    }
}

void ImageArea::getImageSize (int &w, int&h)
{
    if (ipc) {
        w = ipc->getFullWidth();
        h = ipc->getFullHeight();
    } else {
        w = h = 0;
    }
}

void ImageArea::getPreviewCenterPos(int &x, int &y)
{
    if (mainCropWindow) {
        // Getting crop window size
        int cW, cH;
        mainCropWindow->getSize(cW, cH);

        // Converting center coord of crop window to image coord
        const int cX = cW / 2;
        const int cY = cH / 2;
        mainCropWindow->screenCoordToImage(cX, cY, x, y);
    } else {
        x = y = 0;
    }
}

void ImageArea::getPreviewSize(int &w, int &h)
{
    if (mainCropWindow) {
        int tmpW, tmpH;
        mainCropWindow->getSize(tmpW, tmpH);
        w = mainCropWindow->scaleValueToImage(tmpW);
        h = mainCropWindow->scaleValueToImage(tmpH);
    } else {
        w = h = 0;
    }
}

void ImageArea::grabFocus (CropWindow* cw)
{

    focusGrabber = cw;

    if (cw && cw != mainCropWindow) {
        cropWindowSelected (cw);
    }
}

void ImageArea::unGrabFocus ()
{

    focusGrabber = nullptr;
}

void ImageArea::addCropWindow ()
{
    if (!mainCropWindow) {
        return;    // if called but no image is loaded, it would crash
    }

    CropWindow* cw = new CropWindow (this, true, true);
    cw->zoom11(false);
    cw->setCropGUIListener (cropgl);
    cw->setPointerMotionListener (pmlistener);
    cw->setPointerMotionHListener (pmhlistener);
    int lastWidth = options.detailWindowWidth;
    int lastHeight = options.detailWindowHeight;

    if (lastWidth < lastHeight) {
        lastHeight = lastWidth;
    }

    if (lastHeight < lastWidth) {
        lastWidth = lastHeight;
    }

    if(!cropWins.empty()) {
        CropWindow *lastCrop;
        lastCrop = cropWins.front();

        if(lastCrop) {
            lastCrop->getSize(lastWidth, lastHeight);
        }
    }

    cropWins.push_front (cw);

    // Position the new crop window this way: start from top right going down to bottom. When bottom is reached, continue top left going down......
    int N = cropWins.size() - 1;
    int cropwidth, cropheight;

    if(lastWidth <= 0) { // this is only the case for very first start of RT 4.1 or when options file is deleted
        cropwidth = 200;
        cropheight = 200;
    } else {
        cropwidth = lastWidth;
        cropheight = lastHeight;
    }

    int deviceScale = RTScalable::getScaleForWidget(this);
    // Needs to be before setSize()
    cw->cropHandler.setDeviceScale(deviceScale);

    cw->setSize (cropwidth, cropheight);
    int x, y;
    int maxRows = get_height() / cropheight;

    if(maxRows == 0) {
        maxRows = 1;
    }

    int col = N / maxRows;

    if(col % 2) { // from left side
        col = col / 2;
        x = col * cropwidth;

        if(x >= get_width() - 50) {
            x = get_width() - 50;
        }
    } else {    // from right side
        col /= 2;
        col++;
        x = get_width() - col * cropwidth;

        if(x <= 0) {
            x = 0;
        }
    }

    y = cropheight * (N % maxRows);
    cw->setPosition (x, y);
    cw->setEditSubscriber (getCurrSubscriber());
    cw->enable(); // start processing!

    {
    int anchorX = 0;
    int anchorY = 0;
    mainCropWindow->getCropAnchorPosition(anchorX, anchorY);
    cw->setCropAnchorPosition(anchorX, anchorY);
    }

    mainCropWindow->setObservedCropWin (cropWins.front());

    if(!ipc->getHighQualComputed()) {
        ipc->startProcessing(M_HIGHQUAL);
        ipc->setHighQualComputed();
    }
}


void ImageArea::cropWindowSelected (CropWindow* cw)
{

    std::list<CropWindow*>::iterator i = std::find (cropWins.begin(), cropWins.end(), cw);

    if (i != cropWins.end()) {
        cropWins.erase (i);
    }

    cropWins.push_front (cw);
    mainCropWindow->setObservedCropWin (cropWins.front());
}

void ImageArea::cropWindowClosed (CropWindow* cw)
{

    focusGrabber = nullptr;
    std::list<CropWindow*>::iterator i = std::find (cropWins.begin(), cropWins.end(), cw);

    if (i != cropWins.end()) {
        cropWins.erase (i);
    }

    if (!cropWins.empty()) {
        mainCropWindow->setObservedCropWin (cropWins.front());
    } else {
        mainCropWindow->setObservedCropWin (nullptr);
    }

    queue_draw ();
}

void ImageArea::straightenReady (double rotDeg)
{

    if (listener) {
        listener->rotateSelectionReady (rotDeg);
    }
}

void ImageArea::spotWBSelected (int x, int y)
{

    if (listener) {
        listener->spotWBselected (x, y);
    }
}

void ImageArea::sharpMaskSelected (bool sharpMask)
{

    if (listener) {
        listener->sharpMaskSelected (sharpMask);
    }
}

void ImageArea::getScrollImageSize (int& w, int& h)
{

    if (mainCropWindow && ipc) {
        w = ipc->getFullWidth();
        h = ipc->getFullHeight();
    } else {
        w = h = 0;
    }
}

void ImageArea::getScrollPosition (int& x, int& y)
{

    if (mainCropWindow) {
        mainCropWindow->getCropAnchorPosition (x, y);
    } else {
        x = y = 0;
    }
}

void ImageArea::setScrollPosition (int x, int y)
{

    if (mainCropWindow) {
        mainCropWindow->delCropWindowListener (this);
        mainCropWindow->setCropAnchorPosition (x, y);
        mainCropWindow->addCropWindowListener (this);
    }
}

void ImageArea::cropPositionChanged (CropWindow* cw)
{

    syncBeforeAfterViews ();
}

void ImageArea::cropWindowSizeChanged (CropWindow* cw)
{

    syncBeforeAfterViews ();
}

void ImageArea::cropZoomChanged (CropWindow* cw)
{

    if (cw == mainCropWindow) {
        parent->zoomChanged ();
        syncBeforeAfterViews ();
        zoomPanel->refreshZoomLabel ();
    }
}

double ImageArea::getZoom ()
{

    if (mainCropWindow) {
        return mainCropWindow->getZoom ();
    } else {
        return 1.0;
    }
}

// Called by imageAreaPanel before/after views
void ImageArea::setZoom (double zoom)
{

    if (mainCropWindow) {
        mainCropWindow->setZoom (zoom);
    }

    zoomPanel->refreshZoomLabel ();
}

void ImageArea::initialImageArrived ()
{
    if (mainCropWindow) {
        ImageSize size = mainCropWindow->cropHandler.getFullImageSize();
        if(options.prevdemo != PD_Sidecar || !options.rememberZoomAndPan ||
                size.width != fullImageWidth || size.height != fullImageHeight) {
            if (options.cropAutoFit || options.bgcolor != 0) {
                mainCropWindow->zoomFitCrop();
            } else {
                mainCropWindow->zoomFit();
            }
        } else if ((options.cropAutoFit || options.bgcolor != 0) && mainCropWindow->cropHandler.cropParams->enabled) {
            mainCropWindow->zoomFitCrop();
        }
        fullImageWidth = size.width;
        fullImageHeight = size.height;
    }
}

void ImageArea::syncBeforeAfterViews ()
{
    parent->syncBeforeAfterViews ();
}

void ImageArea::setCropGUIListener (CropGUIListener* l)
{

    cropgl = l;

    for (auto cropWin : cropWins) {
        cropWin->setCropGUIListener (cropgl);
    }

    if (mainCropWindow) {
        mainCropWindow->setCropGUIListener (cropgl);
    }
}

void ImageArea::setPointerMotionListener (PointerMotionListener* pml)
{

    pmlistener = pml;

    for (auto cropWin : cropWins) {
        cropWin->setPointerMotionListener (pml);
    }

    if (mainCropWindow) {
        mainCropWindow->setPointerMotionListener (pml);
    }
}

void ImageArea::setPointerMotionHListener (PointerMotionListener* pml)
{

    pmhlistener = pml;

    for (auto cropWin : cropWins) {
        cropWin->setPointerMotionHListener (pml);
    }

    if (mainCropWindow) {
        mainCropWindow->setPointerMotionHListener (pml);
    }
}

ToolMode ImageArea::getToolMode ()
{

    if (listener && listener->getToolBar()) {
        return listener->getToolBar()->getTool ();
    } else {
        return TMHand;
    }
}

bool ImageArea::showColorPickers ()
{

    if (listener && listener->getToolBar()) {
        return listener->getToolBar()->showColorPickers ();
    } else {
        return false;
    }
}

void ImageArea::setToolHand ()
{

    if (listener && listener->getToolBar()) {
        listener->getToolBar()->setTool (TMHand);
    }
}

int ImageArea::getSpotWBRectSize  ()
{

    if (listener) {
        return listener->getSpotWBRectSize ();
    } else {
        return 1;
    }
}

Gtk::SizeRequestMode ImageArea::get_request_mode_vfunc () const
{
    return Gtk::SizeRequestMode::CONSTANT_SIZE;
}

void ImageArea::measure_vfunc(
    Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
    int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = RTScalable::scalePixelSize(100);
        natural = RTScalable::scalePixelSize(400);
    } else {
        minimum = RTScalable::scalePixelSize(50);
        natural = RTScalable::scalePixelSize(300);
    }

    minimum_baseline = -1;
    natural_baseline = -1;
}
