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

#include <gtkmm.h>
#include "rtsurface.h"

class SplashImage final :
    public Gtk::DrawingArea
{

private:
    std::shared_ptr<RTSurface> surface;
    Glib::RefPtr<Pango::Layout> version;

    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

public:
    SplashImage();
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void measure_vfunc(Gtk::Orientation orientation, int for_size, int& minimum, int& natural,
                       int& minimum_baseline, int& natural_baseline) const override;
};

class Splash final : public Gtk::Dialog
{

private:
    SplashImage* splashImage;
    Gtk::Notebook* nb;
    Gtk::ScrolledWindow* releaseNotesSW;

public:
    explicit Splash (Gtk::Window& parent);

    bool hasReleaseNotes()
    {
        return releaseNotesSW != nullptr;
    };
    void showReleaseNotes();
    bool on_timer ();
    void closePressed();
};
