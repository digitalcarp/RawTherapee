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
#include "splash.h"

#include <glib/gstdio.h>

#include "multilangmgr.h"
#include "svgpaintable.h"

extern Glib::ustring creditsPath;
extern Glib::ustring licensePath;
extern Glib::ustring versionString;

Splash::Splash()
{
    set_title(M("GENERAL_ABOUT"));

    releaseNotesSW = nullptr;

    auto box = Gtk::manage(new Gtk::Box());
    nb = Gtk::manage (new Gtk::Notebook ());
    nb->set_name ("AboutNotebook");
    nb->set_hexpand (true);
    box->prepend (*nb);
    set_child (*box);

    Glib::RefPtr<Gtk::CssProvider> localCSS = Gtk::CssProvider::create();
    localCSS->load_from_data ("textview { font-family: monospace; }");

    // Tab 1: the image
    auto splashBox = Gtk::manage(new Gtk::Box(Gtk::Orientation::VERTICAL));
    auto svg = SvgPaintableWrapper::createFromImage("splash.svg");
    splashImage = Gtk::manage(new Gtk::Picture());
    gtk_picture_set_paintable(splashImage->gobj(), svg->base_gobj());
    splashImage->set_halign(Gtk::Align::CENTER);
    splashImage->set_valign(Gtk::Align::CENTER);
    splashBox->prepend(*splashImage);
    splashBox->append(*Gtk::manage(new Gtk::Label(versionString)));
    nb->append_page(*splashBox, M("ABOUT_TAB_SPLASH"));

    // Tab 2: the information about the current version
    std::string buildFileName = Glib::build_filename (creditsPath, "AboutThisBuild.txt");

    if ( Glib::file_test(buildFileName, (Glib::FileTest::EXISTS)) ) {
        FILE *f = g_fopen (buildFileName.c_str (), "rt");

        if (f != nullptr) {
            char* buffer = new char[1024];
            std::ostringstream ostr;

            while (fgets (buffer, 1024, f)) {
                ostr << buffer;
            }

            delete [] buffer;
            fclose (f);

            Glib::RefPtr<Gtk::TextBuffer> textBuffer = Gtk::TextBuffer::create();
            textBuffer->set_text((Glib::ustring)(ostr.str()));

            Gtk::ScrolledWindow *buildSW = Gtk::manage (new Gtk::ScrolledWindow());
            Gtk::TextView *buildTV = Gtk::manage (new Gtk::TextView (textBuffer));
            buildTV->get_style_context()->add_provider(localCSS, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            buildTV->set_editable(false);
            buildTV->set_left_margin (10);
            buildTV->set_right_margin (5);
            buildSW->set_child(*buildTV);
            nb->append_page (*buildSW, M("ABOUT_TAB_BUILD"));
        }
    }

    // Tab 3: the credits
    std::string creditsFileName = Glib::build_filename (creditsPath, "AUTHORS.txt");

    if ( Glib::file_test(creditsFileName, (Glib::FileTest::EXISTS)) ) {
        FILE *f = g_fopen (creditsFileName.c_str (), "rt");

        if (f != nullptr) {
            char* buffer = new char[1024];
            std::ostringstream ostr;

            while (fgets (buffer, 1024, f)) {
                ostr << buffer;
            }

            delete [] buffer;
            fclose (f);

            Glib::RefPtr<Gtk::TextBuffer> textBuffer = Gtk::TextBuffer::create();
            textBuffer->set_text((Glib::ustring)(ostr.str()));

            Gtk::ScrolledWindow *creditsSW = Gtk::manage (new Gtk::ScrolledWindow());
            Gtk::TextView *creditsTV = Gtk::manage (new Gtk::TextView (textBuffer));
            creditsTV->set_left_margin (10);
            creditsTV->set_right_margin (5);
            creditsTV->set_wrap_mode(Gtk::WrapMode::WORD);
            creditsTV->set_editable(false);
            creditsSW->set_child(*creditsTV);
            nb->append_page (*creditsSW, M("ABOUT_TAB_CREDITS"));
        }
    }

    // Tab 4: the license
    std::string licenseFileName = Glib::build_filename (licensePath, "LICENSE");

    if ( Glib::file_test(licenseFileName, (Glib::FileTest::EXISTS)) ) {
        FILE *f = g_fopen (licenseFileName.c_str (), "rt");

        if (f != nullptr) {
            char* buffer = new char[1024];
            std::ostringstream ostr;

            while (fgets (buffer, 1024, f)) {
                ostr << buffer;
            }

            delete [] buffer;
            fclose (f);

            Glib::RefPtr<Gtk::TextBuffer> textBuffer = Gtk::TextBuffer::create();
            textBuffer->set_text((Glib::ustring)(ostr.str()));

            Gtk::ScrolledWindow *licenseSW = Gtk::manage (new Gtk::ScrolledWindow());
            Gtk::TextView *licenseTV = Gtk::manage (new Gtk::TextView (textBuffer));

            // set monospace font to enhance readability of formatted text
            licenseTV->set_left_margin (10);
            licenseTV->set_right_margin (5);
            licenseTV->set_editable(false);
            licenseSW->set_child(*licenseTV);
            nb->append_page (*licenseSW, M("ABOUT_TAB_LICENSE"));
        }
    }

    // Tab 5: the Release Notes
    std::string releaseNotesFileName = Glib::build_filename (creditsPath, "RELEASE_NOTES.txt");

    if ( Glib::file_test(releaseNotesFileName, (Glib::FileTest::EXISTS)) ) {
        FILE *f = g_fopen (releaseNotesFileName.c_str (), "rt");

        if (f != nullptr) {
            char* buffer = new char[1024];
            std::ostringstream ostr;

            while (fgets (buffer, 1024, f)) {
                ostr << buffer;
            }

            delete [] buffer;
            fclose (f);

            Glib::RefPtr<Gtk::TextBuffer> textBuffer = Gtk::TextBuffer::create();
            textBuffer->set_text((Glib::ustring)(ostr.str()));

            releaseNotesSW = Gtk::manage (new Gtk::ScrolledWindow());
            Gtk::TextView *releaseNotesTV = Gtk::manage (new Gtk::TextView (textBuffer));

            releaseNotesTV->set_left_margin (10);
            releaseNotesTV->set_right_margin (3);
            releaseNotesTV->set_editable(false);
            releaseNotesTV->set_wrap_mode(Gtk::WrapMode::WORD);
            releaseNotesSW->set_child(*releaseNotesTV);
            nb->append_page (*releaseNotesSW, M("ABOUT_TAB_RELEASENOTES"));
        }
    }

    set_resizable (true);
    nb->set_current_page (0);
}

bool Splash::on_timer ()
{
    hide ();
    return false;
}

void Splash::showReleaseNotes()
{
    if (releaseNotesSW) {
        nb->set_current_page(nb->page_num(*releaseNotesSW));
    }
}
