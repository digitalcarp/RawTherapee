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
#include "zoompanel.h"
#include "multilangmgr.h"
#include "imagearea.h"
#include "rtimage.h"

ZoomPanel::ZoomPanel (ImageArea* iarea) : iarea(iarea)
{
    set_name ("EditorZoomPanel");

    Gtk::Image* imageOut = Gtk::manage (new RtImage ("magnifier-minus"));
    Gtk::Image* imageIn = Gtk::manage (new RtImage ("magnifier-plus"));
    Gtk::Image* image11 = Gtk::manage ( new RtImage ("magnifier-1to1"));
    Gtk::Image* imageFit = Gtk::manage (new RtImage ("magnifier-fit"));
    Gtk::Image* imageFitCrop = Gtk::manage (new RtImage ("magnifier-crop"));

    zoomOut = Gtk::manage (new Gtk::Button());
    zoomOut->set_child (*imageOut);
    zoomOut->set_has_frame(false);
    setExpandAlignProperties(zoomOut, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    zoomIn = Gtk::manage (new Gtk::Button());
    zoomIn->set_child (*imageIn);
    zoomIn->set_has_frame(false);
    setExpandAlignProperties(zoomIn, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    zoomFit = Gtk::manage (new Gtk::Button());
    zoomFit->set_child (*imageFit);
    zoomFit->set_has_frame(false);
    setExpandAlignProperties(zoomFit, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    zoomFitCrop = Gtk::manage (new Gtk::Button());
    zoomFitCrop->set_child (*imageFitCrop);
    zoomFitCrop->set_has_frame(false);
    setExpandAlignProperties(zoomFitCrop, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    zoom11 = Gtk::manage (new Gtk::Button());
    zoom11->set_child (*image11);
    zoom11->set_has_frame(false);
    setExpandAlignProperties(zoom11, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);

    attach_next_to (*zoomOut, Gtk::PositionType::RIGHT, 1, 1);
    attach_next_to (*zoomIn, Gtk::PositionType::RIGHT, 1, 1);
    attach_next_to (*zoomFit, Gtk::PositionType::RIGHT, 1, 1);
    attach_next_to (*zoomFitCrop, Gtk::PositionType::RIGHT, 1, 1);
    attach_next_to (*zoom11, Gtk::PositionType::RIGHT, 1, 1);

    zoomLabel = Gtk::manage (new Gtk::Label ());
    setExpandAlignProperties(zoomLabel, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    attach_next_to (*zoomLabel, Gtk::PositionType::RIGHT, 1, 1);

    Gtk::Image* imageCrop = Gtk::manage (new RtImage ("window-add"));
    newCrop = Gtk::manage (new Gtk::Button());
    newCrop->set_child (*imageCrop);
    newCrop->set_has_frame(false);
    setExpandAlignProperties(newCrop, false, false, Gtk::Align::CENTER, Gtk::Align::FILL);
    attach_next_to (*newCrop, Gtk::PositionType::RIGHT, 1, 1);

    zoomIn->signal_clicked().connect ( sigc::mem_fun(*this, &ZoomPanel::zoomInClicked) );
    zoomOut->signal_clicked().connect( sigc::mem_fun(*this, &ZoomPanel::zoomOutClicked) );
    zoomFit->signal_clicked().connect( sigc::mem_fun(*this, &ZoomPanel::zoomFitClicked) );
    zoomFitCrop->signal_clicked().connect( sigc::mem_fun(*this, &ZoomPanel::zoomFitCropClicked) );
    zoom11->signal_clicked().connect ( sigc::mem_fun(*this, &ZoomPanel::zoom11Clicked) );
    newCrop->signal_clicked().connect ( sigc::mem_fun(*this, &ZoomPanel::newCropClicked) );

    zoomIn->set_tooltip_markup (M("ZOOMPANEL_ZOOMIN"));
    zoomOut->set_tooltip_markup (M("ZOOMPANEL_ZOOMOUT"));
    zoom11->set_tooltip_markup (M("ZOOMPANEL_ZOOM100"));
    zoomFit->set_tooltip_markup (M("ZOOMPANEL_ZOOMFITSCREEN"));
    zoomFitCrop->set_tooltip_markup (M("ZOOMPANEL_ZOOMFITCROPSCREEN"));
    newCrop->set_tooltip_markup (M("ZOOMPANEL_NEWCROPWINDOW"));

    zoomLabel->set_text (M("ZOOMPANEL_100"));
}

void ZoomPanel::zoomInClicked ()
{

    if (iarea->mainCropWindow) {
        iarea->mainCropWindow->zoomIn ();
    }
}

void ZoomPanel::zoomOutClicked ()
{

    if (iarea->mainCropWindow) {
        iarea->mainCropWindow->zoomOut ();
    }
}

void ZoomPanel::zoomFitClicked ()
{

    if (iarea->mainCropWindow) {
        iarea->mainCropWindow->zoomFit ();
    }
}

void ZoomPanel::zoomFitCropClicked ()
{

    if (iarea->mainCropWindow) {
        iarea->mainCropWindow->zoomFitCrop ();
    }
}

void ZoomPanel::zoom11Clicked ()
{

    if (iarea->mainCropWindow) {
        iarea->mainCropWindow->zoom11 ();
    }
}

void ZoomPanel::refreshZoomLabel ()
{

    if (iarea->mainCropWindow) {
        int z = (int)(iarea->mainCropWindow->getZoom () * 100);

        if (z < 100) {
            zoomLabel->set_text (Glib::ustring::compose(" %1%%", z));
        } else {
            zoomLabel->set_text (Glib::ustring::compose("%1%%", z));
        }
    }
}

void ZoomPanel::newCropClicked ()
{

    iarea->addCropWindow ();
}
