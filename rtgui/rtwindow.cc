/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>, Oliver Duis <www.oliverduis.de>
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

#include "rtwindow.h"

#include "options.h"
#include "rtimage.h"

#include <gtkmm.h>

// #include "cachemanager.h"
// #include "preferences.h"
// #include "iccprofilecreator.h"
// #include "cursormanager.h"
// #include "editwindow.h"
// #include "thumbnail.h"
// #include "whitebalance.h"
// #include "rtengine/settings.h"
// #include "batchqueuepanel.h"
// #include "batchqueueentry.h"
// #include "editorpanel.h"
// #include "filepanel.h"
// #include "filmsimulation.h"

Glib::RefPtr<Gtk::CssProvider> cssForced;
Glib::RefPtr<Gtk::CssProvider> cssRT;

RtWindow::RtWindow ()
    : mainNB (nullptr)
    , bpanel (nullptr)
    , splash (nullptr)
    , btn_fullscreen (nullptr)
    , iFullscreen (nullptr)
    , iFullscreen_exit (nullptr)
    , epanel (nullptr)
    , fpanel (nullptr)
{
    cacheMgr->init ();
    ProfilePanel::init (this);

    // ------- loading theme files

    Glib::RefPtr<Gdk::Screen> screen = Gdk::Screen::get_default();

    if (screen) {
        // Setting default theme and icon theme (bases for custom themes)
        Gtk::Settings::get_for_screen (screen)->property_gtk_theme_name() = "Adwaita";
        Gtk::Settings::get_for_screen (screen)->property_gtk_application_prefer_dark_theme() = true;
        Gtk::Settings::get_for_screen (screen)->property_gtk_icon_theme_name() = "rawtherapee";

        // Initialize RTScalable for Hi-DPI support
        RTScalable::init(this);

        // Look for theme and set it
        // Check if the current theme name in options exists, otherwise set it to default one (i.e. "RawTherapee.css")
        auto filename = Glib::build_filename(argv0, "themes", options.theme + ".css");
        if (!Glib::file_test(filename, Glib::FileTest::EXISTS)) {
            options.theme = "RawTherapee";
            filename = Glib::build_filename(argv0, "themes", options.theme + ".css");
        }

        cssRT = Gtk::CssProvider::create();

        try {
            cssRT->load_from_path (filename);
            Gtk::StyleContext::add_provider_for_screen (screen, cssRT, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        } catch (Glib::Error &err) {
            printf ("Error: Can't load css file \"%s\"\nMessage: %s\n", filename.c_str(), err.what().c_str());
        } catch (...) {
            printf ("Error: Can't load css file \"%s\"\n", filename.c_str());
        }

        // Set the font face and size
        Glib::ustring css;

        if (options.fontFamily != "default") { // Set font and size according to user choice
            // Set font and size in css from options
            css = Glib::ustring::compose ("* { font-family: %1; font-size: %2pt}",
                options.fontFamily,
                options.fontSize); // Font size is in "pt" in options
        } else { // Set font and size according to default values
            // Retrieve default style values from Gtk::Settings
            const auto defaultSettings = Gtk::Settings::get_default();
            Glib::ustring defaultFont;
            defaultSettings->get_property("gtk-font-name", defaultFont);
            const Pango::FontDescription defaultFontDesc = Pango::FontDescription(defaultFont);

            // Set font and size in css
            auto defaultFontFamily = defaultFontDesc.get_family();
            const int defaultFontSize = defaultFontDesc.get_size() / Pango::SCALE; // Font size is managed in ()"pt" * Pango::SCALE) by Pango (also refer to notes in rtscalable.h)
#if defined(__APPLE__)
            // Default MacOS font (i.e. "") is not correctly handled
            // in Gtk css. Replacing it by "-apple-system" to avoid this
            if (defaultFontFamily == ".AppleSystemUIFont") {
                defaultFontFamily = "-apple-system";
            }
#endif
            css = Glib::ustring::compose ("* { font-family: %1; font-size: %2pt}",
                defaultFontFamily,
                defaultFontSize);
        }

        // Load custom CSS for font
        if (!css.empty()) {
            if (rtengine::settings->verbose) {
                printf("CSS:\n%s\n\n", css.c_str());
            }

            try {
                cssForced = Gtk::CssProvider::create();
                cssForced->load_from_data (css);

                Gtk::StyleContext::add_provider_for_screen (screen, cssForced, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            } catch (Glib::Error &err) {
                printf ("Error: \"%s\"\n", err.what().c_str());
            } catch (...) {
                printf ("Error: Can't load the desired font correctly\n");
            }
        }
    }

    // ------- end loading theme files

    // Initialize FileBrowserEntry icons
    FileBrowserEntry::init();

    // For UNIX system, set app icon
#ifndef _WIN32
    try {
        set_default_icon_name("rawtherapee");
    } catch (const Glib::Error& ex) {
        printf ("%s\n", ex.what().c_str());
    }
#endif

    versionStr = "RawTherapee " + versionString;

    set_title_decorated ("");
    set_resizable (true);
    set_decorated (true);
    set_default_size (options.windowWidth, options.windowHeight);
    set_modal (false);

    on_delete_has_run = false;
    is_fullscreen = false;
    is_minimized = false;
    property_destroy_with_parent().set_value (false);
    signal_window_state_event().connect ( sigc::mem_fun (*this, &RtWindow::on_window_state_event) );
    onConfEventConn = signal_configure_event().connect ( sigc::mem_fun (*this, &RtWindow::on_configure_event) );
    signal_key_press_event().connect ( sigc::mem_fun (*this, &RtWindow::keyPressed) );
    signal_key_release_event().connect(sigc::mem_fun(*this, &RtWindow::keyReleased));

    if (simpleEditor) {
        epanel = Gtk::manage ( new EditorPanel (nullptr) );
        epanel->setParent (this);
        epanel->setParentWindow (this);
        add (*epanel);
        show_all ();

        pldBridge = nullptr; // No progress listener

        CacheManager* cm = CacheManager::getInstance();
        Thumbnail* thm = cm->getEntry ( argv1 );

        if (thm) {
            int error;
            rtengine::InitialImage *ii = rtengine::InitialImage::load (argv1, thm->getType() == FT_Raw, &error, nullptr);
            epanel->open ( thm, ii );
        }
    } else {
        mainNB = Gtk::manage (new Gtk::Notebook ());
        mainNB->set_name ("MainNotebook");
        mainNB->set_scrollable (true);
        mainNB->signal_switch_page().connect_notify ( sigc::mem_fun (*this, &RtWindow::on_mainNB_switch_page) );

        // Editor panel
        fpanel =  new FilePanel () ;
        fpanel->setParent (this);

        // decorate tab
        Gtk::Grid* fpanelLabelGrid = Gtk::manage (new Gtk::Grid ());
        setExpandAlignProperties (fpanelLabelGrid, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        Gtk::Label* fpl = Gtk::manage (new Gtk::Label ( Glib::ustring (" ") + M ("MAIN_FRAME_EDITOR") ));

        if (options.mainNBVertical) {
            mainNB->set_tab_pos (Gtk::PositionType::LEFT);
            // fpl->set_angle (90);
            RTImage* folderIcon = Gtk::manage (new RTImage ("folder-closed", Gtk::IconSize::LARGE));
            fpanelLabelGrid->attach_next_to (*folderIcon, Gtk::PositionType::TOP, 1, 1);
            fpanelLabelGrid->attach_next_to (*fpl, Gtk::PositionType::TOP, 1, 1);
        } else {
            RTImage* folderIcon = Gtk::manage (new RTImage ("folder-closed", Gtk::IconSize::LARGE));
            fpanelLabelGrid->attach_next_to (*folderIcon, Gtk::PositionType::RIGHT, 1, 1);
            fpanelLabelGrid->attach_next_to (*fpl, Gtk::PositionType::RIGHT, 1, 1);
        }

        fpanelLabelGrid->set_tooltip_markup (M ("MAIN_FRAME_FILEBROWSER_TOOLTIP"));
        fpanelLabelGrid->show_all ();
        mainNB->append_page (*fpanel, *fpanelLabelGrid);


        // Batch Queue panel
        bpanel = Gtk::manage ( new BatchQueuePanel (fpanel->fileCatalog) );

        // decorate tab, the label is unimportant since its updated in batchqueuepanel anyway
        Gtk::Label* lbq = Gtk::manage ( new Gtk::Label (M ("MAIN_FRAME_QUEUE")) );

        if (options.mainNBVertical) {
            // lbq->set_angle (90);
        }

        mainNB->append_page (*bpanel, *lbq);


        if (isSingleTabMode()) {
            createSetmEditor();
        }

        mainNB->set_current_page (mainNB->page_num (*fpanel));

        //Gtk::Box* mainBox = Gtk::manage (new Gtk::Box(Gtk::Orientation::VERTICAL));
        //mainBox->pack_start (*mainNB);

        // filling bottom box
        iFullscreen = new RTImage ("fullscreen-enter", Gtk::IconSize::LARGE);
        iFullscreen_exit = new RTImage ("fullscreen-leave", Gtk::IconSize::LARGE);

        Gtk::Button* iccProfileCreator = Gtk::manage (new Gtk::Button ());
        setExpandAlignProperties (iccProfileCreator, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        iccProfileCreator->set_relief(Gtk::RELIEF_NONE);
        iccProfileCreator->set_image (*Gtk::manage (new RTImage ("gamut-plus", Gtk::IconSize::LARGE)));
        iccProfileCreator->set_tooltip_markup (M ("MAIN_BUTTON_ICCPROFCREATOR"));
        iccProfileCreator->signal_clicked().connect ( sigc::mem_fun (*this, &RtWindow::showICCProfileCreator) );

        Gtk::Button* helpBtn = Gtk::manage (new Gtk::Button ());
        setExpandAlignProperties (helpBtn, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        helpBtn->set_relief(Gtk::RELIEF_NONE);
        helpBtn->set_image (*Gtk::manage (new RTImage("questionmark", Gtk::IconSize::LARGE)));
        helpBtn->set_tooltip_markup (M ("GENERAL_HELP"));
        helpBtn->signal_clicked().connect (sigc::mem_fun (*this, &RtWindow::showRawPedia));

        Gtk::Button* preferences = Gtk::manage (new Gtk::Button ());
        setExpandAlignProperties (preferences, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        preferences->set_relief(Gtk::RELIEF_NONE);
        preferences->set_image (*Gtk::manage (new RTImage ("preferences", Gtk::IconSize::LARGE)));
        preferences->set_tooltip_markup (M ("MAIN_BUTTON_PREFERENCES"));
        preferences->signal_clicked().connect ( sigc::mem_fun (*this, &RtWindow::showPreferences) );

        btn_fullscreen = Gtk::manage ( new Gtk::Button());
        setExpandAlignProperties (btn_fullscreen, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        btn_fullscreen->set_relief(Gtk::RELIEF_NONE);
        btn_fullscreen->set_tooltip_markup (M ("MAIN_BUTTON_FULLSCREEN"));
        btn_fullscreen->set_image (*iFullscreen);
        btn_fullscreen->signal_clicked().connect ( sigc::mem_fun (*this, &RtWindow::toggle_fullscreen) );
        setExpandAlignProperties (&prProgBar, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
        prProgBar.set_show_text (true);

        Gtk::Grid* actionGrid = Gtk::manage (new Gtk::Grid ());
        actionGrid->set_row_spacing (2);
        actionGrid->set_column_spacing (2);

        setExpandAlignProperties (actionGrid, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);

        if (options.mainNBVertical) {
            prProgBar.set_orientation (Gtk::Orientation::VERTICAL);
            prProgBar.set_inverted (true);
            actionGrid->set_orientation (Gtk::Orientation::VERTICAL);
            actionGrid->attach_next_to (prProgBar, Gtk::PositionType::BOTTOM, 1, 1);
            actionGrid->attach_next_to (*iccProfileCreator, Gtk::PositionType::BOTTOM, 1, 1);
            actionGrid->attach_next_to (*helpBtn, Gtk::PositionType::BOTTOM, 1, 1);
            actionGrid->attach_next_to (*preferences, Gtk::PositionType::BOTTOM, 1, 1);
            actionGrid->attach_next_to (*btn_fullscreen, Gtk::PositionType::BOTTOM, 1, 1);
            mainNB->set_action_widget (actionGrid, Gtk::PACK_END);
        } else {
            prProgBar.set_orientation (Gtk::Orientation::HORIZONTAL);
            actionGrid->set_orientation (Gtk::Orientation::HORIZONTAL);
            actionGrid->attach_next_to (prProgBar, Gtk::PositionType::RIGHT, 1, 1);
            actionGrid->attach_next_to (*iccProfileCreator, Gtk::PositionType::RIGHT, 1, 1);
            actionGrid->attach_next_to (*helpBtn, Gtk::PositionType::RIGHT, 1, 1);
            actionGrid->attach_next_to (*preferences, Gtk::PositionType::RIGHT, 1, 1);
            actionGrid->attach_next_to (*btn_fullscreen, Gtk::PositionType::RIGHT, 1, 1);
            mainNB->set_action_widget (actionGrid, Gtk::PACK_END);
        }

        actionGrid->show_all();

        pldBridge = new PLDBridge (static_cast<rtengine::ProgressListener*> (this));

        add (*mainNB);
        show_all ();

        bpanel->init (this);

        if (!argv1.empty() && !remote) {
            Thumbnail* thm = cacheMgr->getEntry (argv1);

            if (thm) {
                fpanel->fileCatalog->openRequested ({thm});
            }
        }
    }
}

RtWindow::~RtWindow()
{
    if (!simpleEditor) {
        delete pldBridge;
    }

    pldBridge = nullptr;

    delete fpanel;
    delete iFullscreen;
    delete iFullscreen_exit;
}

void RtWindow::on_realize ()
{
    Gtk::Window::on_realize ();

    if ( fpanel ) {
        fpanel->setAspect();
    }

    if (simpleEditor) {
        epanel->setAspect();
    }

    mainWindowCursorManager.init (get_window());

    // Display release notes only if new major version.
    bool waitForSplash = false;
    if (options.is_new_version()) {
        // Update the version parameter with the right value
        options.version = versionString;

        splash = new Splash (*this);
        splash->set_transient_for (*this);
        splash->signal_delete_event().connect ( sigc::mem_fun (*this, &RtWindow::splashClosed) );

        if (splash->hasReleaseNotes()) {
            waitForSplash = true;
            splash->showReleaseNotes();
            splash->show ();
        } else {
            delete splash;
            splash = nullptr;
        }
    }

    if (!waitForSplash) {
        showErrors();
    }
}

void RtWindow::showErrors()
{
    // alerting users if the default raw and image profiles are missing
    if (options.is_defProfRawMissing()) {
        options.defProfRaw = DEFPROFILE_RAW;
        Gtk::MessageDialog msgd (*this, Glib::ustring::compose (M ("OPTIONS_DEFRAW_MISSING"), escapeHtmlChars(options.defProfRaw)), true, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
        msgd.run ();
    }
    if (options.is_bundledDefProfRawMissing()) {
        Gtk::MessageDialog msgd (*this, Glib::ustring::compose (M ("OPTIONS_BUNDLED_MISSING"), escapeHtmlChars(options.defProfRaw)), true, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
        msgd.run ();
        options.defProfRaw = DEFPROFILE_INTERNAL;
    }

    if (options.is_defProfImgMissing()) {
        options.defProfImg = DEFPROFILE_IMG;
        Gtk::MessageDialog msgd (*this, Glib::ustring::compose (M ("OPTIONS_DEFIMG_MISSING"), escapeHtmlChars(options.defProfImg)), true, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
        msgd.run ();
    }
    if (options.is_bundledDefProfImgMissing()) {
        Gtk::MessageDialog msgd (*this, Glib::ustring::compose (M ("OPTIONS_BUNDLED_MISSING"), escapeHtmlChars(options.defProfImg)), true, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
        msgd.run ();
        options.defProfImg = DEFPROFILE_INTERNAL;
    }
}

bool RtWindow::on_configure_event (GdkEventConfigure* event)
{
    if (!options.windowMaximized && !is_fullscreen && !is_minimized) {
        get_size (options.windowWidth, options.windowHeight);
        get_position (options.windowX, options.windowY);
    }

    // With update the RTScalable on scale or resolution change
    RTScalable::setDPInScale(this);

    return Gtk::Widget::on_configure_event (event);
}

bool RtWindow::on_window_state_event (GdkEventWindowState* event)
{
    // Retrieve RT window states
    options.windowMaximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
    is_minimized = event->new_window_state & GDK_WINDOW_STATE_ICONIFIED;
    is_fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

    return Gtk::Widget::on_window_state_event (event);
}

void RtWindow::on_mainNB_switch_page (Gtk::Widget* widget, guint page_num)
{
    if (!on_delete_has_run) {
        if (isEditorPanel (page_num)) {
            if (isSingleTabMode() && epanel) {
                MoveFileBrowserToEditor();
            }

            EditorPanel *ep = static_cast<EditorPanel*> (mainNB->get_nth_page (page_num));
            ep->setAspect();

            if (!isSingleTabMode()) {
                if (filesEdited.size() > 0) {
                    set_title_decorated (ep->getFileName());
                }
            }
        } else {
            // in single tab mode with command line filename epanel does not exist yet
            if (isSingleTabMode() && epanel) {
                // Save profile on leaving the editor panel
                epanel->saveProfile();

                // Moving the FileBrowser only if the user has switched to the FileBrowser tab
                if (mainNB->get_nth_page (page_num) == fpanel) {
                    MoveFileBrowserToMain();
                }
            }
        }
    }
}

void RtWindow::addEditorPanel (EditorPanel* ep, const std::string &name)
{
    if (options.multiDisplayMode > 0) {
        EditWindow * wndEdit = EditWindow::getInstance (this);
        wndEdit->addEditorPanel (ep, name);
        wndEdit->show_all();
        wndEdit->restoreWindow(); // Need to be called after RtWindow creation to work with all OS Windows Manager
        ep->setAspect();
        wndEdit->toFront();
    } else {
        ep->setParent (this);
        ep->setParentWindow (this);
        ep->setExternalEditorChangedSignal(&externalEditorChangedSignal);

        // construct closeable tab for the image
        Gtk::Grid* titleGrid = Gtk::manage (new Gtk::Grid ());
        titleGrid->set_tooltip_markup (name);
        RTImage *closebimg = Gtk::manage (new RTImage ("cancel-small", Gtk::IconSize::LARGE));
        Gtk::Button* closeb = Gtk::manage (new Gtk::Button ());
        closeb->set_name ("CloseButton");
        closeb->add (*closebimg);
        closeb->set_relief (Gtk::RELIEF_NONE);
        closeb->set_focus_on_click (false);
        closeb->signal_clicked().connect ( sigc::bind (sigc::mem_fun (*this, &RtWindow::remEditorPanel), ep));

        if (!EditWindow::isMultiDisplayEnabled()) {
            titleGrid->attach_next_to (*Gtk::manage (new RTImage ("aperture", Gtk::IconSize::LARGE)), Gtk::PositionType::RIGHT, 1, 1);
        }
        titleGrid->attach_next_to (*Gtk::manage (new Gtk::Label (Glib::path_get_basename (name))), Gtk::PositionType::RIGHT, 1, 1);
        titleGrid->attach_next_to (*closeb, Gtk::PositionType::RIGHT, 1, 1);
        titleGrid->show_all ();
//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
        titleGrid->set_column_spacing (2);
#endif
//GTK318

        mainNB->append_page (*ep, *titleGrid);
        //ep->setAspect ();
        mainNB->set_current_page (mainNB->page_num (*ep));
        mainNB->set_tab_reorderable (*ep, true);

        set_title_decorated (name);
        epanels[ name ] = ep;
        filesEdited.insert ( name );
        fpanel->refreshEditedState (filesEdited);
        ep->tbTopPanel_1_visible (false); //hide the toggle Top Panel button
    }
}

void RtWindow::remEditorPanel (EditorPanel* ep)
{
    if (ep->getIsProcessing()) {
        return;    // Will crash if destroyed while loading
    }

    if (options.multiDisplayMode > 0) {
        EditWindow * wndEdit = EditWindow::getInstance (this);
        wndEdit->remEditorPanel (ep);
    } else {
        bool queueHadFocus = (mainNB->get_current_page() == mainNB->page_num (*bpanel));
        ep->setExternalEditorChangedSignal(nullptr);
        epanels.erase (ep->getFileName());
        filesEdited.erase (ep->getFileName ());
        fpanel->refreshEditedState (filesEdited);

        mainNB->remove_page (*ep);

        if (!isEditorPanel (mainNB->get_current_page())) {
            if (!queueHadFocus) {
                mainNB->set_current_page (mainNB->page_num (*fpanel));
            }

            set_title_decorated ("");
        } else {
            const EditorPanel* lep = static_cast<EditorPanel*> (mainNB->get_nth_page (mainNB->get_current_page()));
            set_title_decorated (lep->getFileName());
        }

        // TODO: ask what to do: close & apply, close & apply selection, close & revert, cancel
    }
}

bool RtWindow::selectEditorPanel (const std::string &name)
{
    if (options.multiDisplayMode > 0) {
        EditWindow * wndEdit = EditWindow::getInstance (this);

        if (wndEdit->selectEditorPanel (name)) {
            set_title_decorated (name);
            wndEdit->toFront();
            return true;
        }
    } else {
        std::map<Glib::ustring, EditorPanel*>::iterator iep = epanels.find (name);

        if (iep != epanels.end()) {
            mainNB->set_current_page (mainNB->page_num (*iep->second));
            set_title_decorated (name);
            return true;
        } else {
            //set_title_decorated(name);
            //printf("RtWindow::selectEditorPanel - plain set\n");
        }
    }

    return false;
}

bool RtWindow::keyPressed (GdkEventKey* event)
{

    bool ctrl = event->state & GDK_CONTROL_MASK;
    //bool shift = event->state & GDK_SHIFT_MASK;

    bool try_quit = false;
#if defined(__APPLE__)
    bool apple_cmd = event->state & GDK_MOD2_MASK;

    if (event->keyval == GDK_KEY_q && apple_cmd) {
        try_quit = true;
    }

#else

    if (event->keyval == GDK_KEY_q && ctrl) {
        try_quit = true;
    }

#endif

    if (try_quit) {
        if (!on_delete_event (nullptr)) {
            gtk_main_quit();
        }
    }

    if (event->keyval == GDK_KEY_F11) {
        toggle_fullscreen();
    }

    if (simpleEditor)
        // in simpleEditor mode, there's no other tab that can handle pressed keys, so we can send the event to editor panel then return
    {
        return epanel->handleShortcutKey (event);
    };

    if (ctrl) {
        switch (event->keyval) {
            case GDK_KEY_F2: // file browser panel
                mainNB->set_current_page (mainNB->page_num (*fpanel));
                return true;

            case GDK_KEY_F3: // batch queue panel
                mainNB->set_current_page (mainNB->page_num (*bpanel));
                return true;

            case GDK_KEY_F4: //single tab mode, editor panel
                if (isSingleTabMode() && epanel) {
                    mainNB->set_current_page (mainNB->page_num (*epanel));
                }

                return true;

            case GDK_KEY_w: //multi-tab mode, close editor panel
                if (!isSingleTabMode() &&
                        mainNB->get_current_page() != mainNB->page_num (*fpanel) &&
                        mainNB->get_current_page() != mainNB->page_num (*bpanel)) {

                    EditorPanel* ep = static_cast<EditorPanel*> (mainNB->get_nth_page (mainNB->get_current_page()));
                    remEditorPanel (ep);
                    return true;
                }
        }
    }

    if (mainNB->get_current_page() == mainNB->page_num (*fpanel)) {
        return fpanel->handleShortcutKey (event);
    } else if (mainNB->get_current_page() == mainNB->page_num (*bpanel)) {
        return bpanel->handleShortcutKey (event);
    } else {
        EditorPanel* ep = static_cast<EditorPanel*> (mainNB->get_nth_page (mainNB->get_current_page()));
        return ep->handleShortcutKey (event);
    }

    return false;
}

bool RtWindow::keyReleased(GdkEventKey *event)
{
    if (fpanel && mainNB->get_current_page() == mainNB->page_num(*fpanel)) {
        return fpanel->handleShortcutKeyRelease(event);
    }
    return false;
}

void RtWindow::addBatchQueueJob (BatchQueueEntry* bqe, bool head)
{

    std::vector<BatchQueueEntry*> entries;
    entries.push_back (bqe);
    bpanel->addBatchQueueJobs (entries, head);
    fpanel->queue_draw ();
}

void RtWindow::addBatchQueueJobs(const std::vector<BatchQueueEntry*>& entries)
{
    bpanel->addBatchQueueJobs (entries, false);
    fpanel->queue_draw ();
}

bool RtWindow::on_delete_event (GdkEventAny* event)
{

    if (on_delete_has_run) {
        // on Mac OSX we can get multiple events
        return false;
    }

    // Check if any editor is still processing, and do NOT quit if so. Otherwise crashes and inconsistent caches
    bool isProcessing = false;
    EditWindow* editWindow = nullptr;

    if (isSingleTabMode() || simpleEditor) {
        isProcessing = epanel->getIsProcessing();
    } else if (options.multiDisplayMode > 0) {
        editWindow = EditWindow::getInstance (this);
        isProcessing = editWindow->isProcessing();
    } else {
        int pageCount = mainNB->get_n_pages();

        for (int i = 0; i < pageCount && !isProcessing; i++) {
            if (isEditorPanel (i)) {
                isProcessing |= (static_cast<EditorPanel*> (mainNB->get_nth_page (i)))->getIsProcessing();
            }
        }
    }

    if (isProcessing) {
        return true;
    }

    if ( fpanel ) {
        fpanel->saveOptions ();
    }

    if ( bpanel ) {
        bpanel->saveOptions ();
    }

    if ((isSingleTabMode() || simpleEditor) && epanel->isRealized()) {
        epanel->saveProfile();
        epanel->writeOptions ();
    } else {
        if (options.multiDisplayMode > 0 && editWindow) {
            editWindow->closeOpenEditors();
            editWindow->writeOptions();
        } else if (epanels.size()) {
            // Storing the options of the last EditorPanel before Gtk destroys everything
            // Look at the active panel first, if any, otherwise look at the first one (sorted on the filename)

            int page = mainNB->get_current_page();
            Gtk::Widget *w = mainNB->get_nth_page (page);
            bool optionsWritten = false;

            for (std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin(); i != epanels.end(); ++i) {
                if (i->second == w) {
                    i->second->writeOptions();
                    optionsWritten = true;
                }
            }

            if (!optionsWritten) {
                // fallback solution: save the options of the first editor panel
                std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin();
                i->second->writeOptions();
            }
        }
    }

    cacheMgr->closeCache ();  // also makes cleanup if too large
    ProfilePanel::cleanup();
    ClutComboBox::cleanup();
    BatchQueueEntry::savedAsIcon.reset();
    FileBrowserEntry::editedIcon.reset();
    FileBrowserEntry::recentlySavedIcon.reset();
    FileBrowserEntry::enqueuedIcon.reset();
    FileBrowserEntry::hdr.reset();
    FileBrowserEntry::ps.reset();

    if (!options.windowMaximized && !is_fullscreen && !is_minimized) {
        get_size (options.windowWidth, options.windowHeight);
        get_position (options.windowX, options.windowY);
    }

    // Retrieve window monitor ID
    options.windowMonitor = 0;
    const auto display = get_screen()->get_display();
    const int monitor_nb = display->get_n_monitors();

    for (int id = 0; id < monitor_nb; id++) {
        if (display->get_monitor_at_window(get_window()) == display->get_monitor(id)) {
            options.windowMonitor = id;
            break;
        }
    }

    try {
        Options::save ();
    } catch (Options::Error &e) {
        Gtk::MessageDialog msgd (getToplevelWindow (this), e.get_msg(), true, Gtk::MessageType::WARNING, Gtk::ButtonsType::CLOSE, true);
        msgd.run();
    }

    hide();

    on_delete_has_run = true;
    return false;
}


void RtWindow::writeToolExpandedStatus (std::vector<int> &tpOpen)
{
    if ((isSingleTabMode() || gimpPlugin) && epanel->isRealized()) {
        epanel->writeToolExpandedStatus (tpOpen);
    } else {
        // Storing the options of the last EditorPanel before Gtk destroys everything
        // Look at the active panel first, if any, otherwise look at the first one (sorted on the filename)
        if (epanels.size()) {
            int page = mainNB->get_current_page();
            Gtk::Widget *w = mainNB->get_nth_page (page);
            bool optionsWritten = false;

            for (std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin(); i != epanels.end(); ++i) {
                if (i->second == w) {
                    i->second->writeToolExpandedStatus (tpOpen);
                    optionsWritten = true;
                }
            }

            if (!optionsWritten) {
                // fallback solution: save the options of the first editor panel
                std::map<Glib::ustring, EditorPanel*>::iterator i = epanels.begin();
                i->second->writeToolExpandedStatus (tpOpen);
            }
        }
    }
}


void RtWindow::showRawPedia()
{
    GError* gerror = nullptr;
    gtk_show_uri(nullptr, "https://rawpedia.rawtherapee.com/", GDK_CURRENT_TIME, &gerror);
}

void RtWindow::showICCProfileCreator ()
{
    ICCProfileCreator *iccpc = new ICCProfileCreator (this);
    iccpc->run ();
    delete iccpc;

    fpanel->optionsChanged ();

    if (epanel) {
        epanel->defaultMonitorProfileChanged (options.rtSettings.monitorProfile, options.rtSettings.autoMonitorProfile);
    }

    for (const auto &p : epanels) {
        p.second->defaultMonitorProfileChanged (options.rtSettings.monitorProfile, options.rtSettings.autoMonitorProfile);
    }
}

void RtWindow::showPreferences ()
{
    Preferences *pref = new Preferences (this);
    pref->run ();
    delete pref;

    fpanel->optionsChanged ();

    if (epanel) {
        epanel->defaultMonitorProfileChanged (options.rtSettings.monitorProfile, options.rtSettings.autoMonitorProfile);
    }

    for (const auto &p : epanels) {
        p.second->defaultMonitorProfileChanged (options.rtSettings.monitorProfile, options.rtSettings.autoMonitorProfile);
    }
}

void RtWindow::setProgress(double p)
{
    prProgBar.set_fraction(p);
}

void RtWindow::setProgressStr(const Glib::ustring& str)
{
    if (!options.mainNBVertical) {
        prProgBar.set_text(str);
    }
}

void RtWindow::setProgressState(bool inProcessing)
{
    if (inProcessing) {
        prProgBar.show();
    } else {
        prProgBar.hide();
    }
}

void RtWindow::error(const Glib::ustring& descr)
{
    prProgBar.set_text(descr);
}

void RtWindow::toggle_fullscreen ()
{
    onConfEventConn.block(true); // Avoid getting size and position while window is getting fullscreen

    if (is_fullscreen) {
        unfullscreen();

        if (btn_fullscreen) {
            btn_fullscreen->set_tooltip_markup (M ("MAIN_BUTTON_FULLSCREEN"));
            btn_fullscreen->set_image (*iFullscreen);
        }
    } else {
        fullscreen();

        if (btn_fullscreen) {
            btn_fullscreen->set_tooltip_markup (M ("MAIN_BUTTON_UNFULLSCREEN"));
            btn_fullscreen->set_image (*iFullscreen_exit);
        }
    }

    onConfEventConn.block(false);
}

void RtWindow::SetEditorCurrent()
{
    mainNB->set_current_page (mainNB->page_num (*epanel));
}

void RtWindow::SetMainCurrent()
{
    mainNB->set_current_page (mainNB->page_num (*fpanel));
}

void RtWindow::MoveFileBrowserToMain()
{
    if ( fpanel->ribbonPane->get_children().empty()) {
        FileCatalog *fCatalog = fpanel->fileCatalog;
        epanel->catalogPane->remove (*fCatalog);
        fpanel->ribbonPane->add (*fCatalog);
        fCatalog->enableTabMode (false);
        fCatalog->tbLeftPanel_1_visible (true);
        fCatalog->tbRightPanel_1_visible (true);
    }
}

void RtWindow::MoveFileBrowserToEditor()
{
    if (epanel->catalogPane->get_children().empty() ) {
        FileCatalog *fCatalog = fpanel->fileCatalog;
        fpanel->ribbonPane->remove (*fCatalog);
        fCatalog->disableInspector();
        epanel->catalogPane->add (*fCatalog);
        epanel->showTopPanel (options.editorFilmStripOpened);
        fCatalog->enableTabMode (true);
        fCatalog->refreshHeight();
        fCatalog->tbLeftPanel_1_visible (false);
        fCatalog->tbRightPanel_1_visible (false);
    }
}

void RtWindow::updateExternalEditorWidget(int selectedIndex, const std::vector<ExternalEditor> & editors)
{
    if (epanel) {
        epanel->updateExternalEditorWidget(selectedIndex, editors);
    }

    for (auto panel : epanels) {
        panel.second->updateExternalEditorWidget(selectedIndex, editors);
    }

    if (options.multiDisplayMode > 0) {
        EditWindow::getInstance(this)
            ->updateExternalEditorWidget(selectedIndex, editors);
    }
}

void RtWindow::updateProfiles (const Glib::ustring &printerProfile, rtengine::RenderingIntent printerIntent, bool printerBPC)
{
    if (epanel) {
        epanel->updateProfiles (printerProfile, printerIntent, printerBPC);
    }

    for (auto panel : epanels) {
        panel.second->updateProfiles (printerProfile, printerIntent, printerBPC);
    }
}

void RtWindow::updateTPVScrollbar (bool hide)
{
    fpanel->updateTPVScrollbar (hide);

    if (epanel) {
        epanel->updateTPVScrollbar (hide);
    }

    for (auto panel : epanels) {
        panel.second->updateTPVScrollbar (hide);
    }
}

void RtWindow::updateFBQueryTB (bool singleRow)
{
    fpanel->fileCatalog->updateFBQueryTB (singleRow);
}

void RtWindow::updateFBToolBarVisibility (bool showFilmStripToolBar)
{
    fpanel->fileCatalog->updateFBToolBarVisibility (showFilmStripToolBar);
}

void RtWindow::updateShowtooltipVisibility (bool showtooltip)
{
    if (epanel) {
        epanel->updateShowtooltipVisibility (showtooltip);
    }

    for (auto panel : epanels) {
        panel.second->updateShowtooltipVisibility (showtooltip);
    }
}

void RtWindow::updateHistogramPosition (int oldPosition, int newPosition)
{
    if (epanel) {
        epanel->updateHistogramPosition (oldPosition, newPosition);
    }

    for (auto panel : epanels) {
        panel.second->updateHistogramPosition (oldPosition, newPosition);
    }
}

void RtWindow::updateToolPanelToolLocations(
    const std::vector<Glib::ustring> &favorites, bool cloneFavoriteTools)
{
    if (fpanel) {
        fpanel->updateToolPanelToolLocations(favorites, cloneFavoriteTools);
    }

    if (epanel) {
        epanel->updateToolPanelToolLocations(favorites, cloneFavoriteTools);
    }

    for (const auto &panel : epanels) {
        panel.second->updateToolPanelToolLocations(favorites, cloneFavoriteTools);
    }

    if (options.multiDisplayMode > 0) {
        EditWindow::getInstance(this)
            ->updateToolPanelToolLocations(favorites, cloneFavoriteTools);
    }
}

bool RtWindow::splashClosed (GdkEventAny* event)
{
    delete splash;
    splash = nullptr;
    showErrors();
    return true;
}

void RtWindow::setWindowSize ()
{
    onConfEventConn.block(true); // Avoid getting size and position while window is being moved, maximized, ...

    Gdk::Rectangle lMonitorRect;
    const auto display = get_screen()->get_display();
    display->get_monitor (std::min (options.windowMonitor, display->get_n_monitors() - 1))->get_geometry(lMonitorRect);

#ifdef __APPLE__
    // Get macOS menu bar height
    Gdk::Rectangle lWorkAreaRect;
    display->get_monitor (std::min (options.windowMonitor, display->get_n_monitors() - 1))->get_workarea(lWorkAreaRect);
    const int macMenuBarHeight = lWorkAreaRect.get_y();

    // Place RT window to saved one in options file
    if (options.windowX <= lMonitorRect.get_x() + lMonitorRect.get_width()
            && options.windowX >= 0
            && options.windowY <= lMonitorRect.get_y() + lMonitorRect.get_height() - macMenuBarHeight
            && options.windowY >= 0) {
        move (options.windowX, options.windowY + macMenuBarHeight);
    } else {
        move (lMonitorRect.get_x(), lMonitorRect.get_y() + macMenuBarHeight);
    }
#else
    // Place RT window to saved one in options file
    if (options.windowX <= lMonitorRect.get_x() + lMonitorRect.get_width()
            && options.windowX >= 0
            && options.windowY <= lMonitorRect.get_y() + lMonitorRect.get_height()
            && options.windowY >= 0) {
        move (options.windowX, options.windowY);
    } else {
        move (lMonitorRect.get_x(), lMonitorRect.get_y());
    }
#endif

    // Maximize RT window according to options file
    if (options.windowMaximized) {
        maximize();
    } else {
        unmaximize();
        resize (options.windowWidth, options.windowHeight);
    }

    onConfEventConn.block(false);
}

void RtWindow::get_position(int& x, int& y) const
{
    // Call native function
    Gtk::Window::get_position (x, y);

    // Retrieve display (concatenation of all monitors) size
    int width = 0, height = 0;
    const auto display = get_screen()->get_display();
    const int nbMonitors = display->get_n_monitors();

    for (int i = 0; i < nbMonitors; i++) {
        Gdk::Rectangle lMonitorRect;
        display->get_monitor(i)->get_geometry(lMonitorRect);
        width = std::max(width, lMonitorRect.get_x() + lMonitorRect.get_width());
        height = std::max(height, lMonitorRect.get_y() + lMonitorRect.get_height());
    }

    // Saturate position at monitor limits to avoid unexpected behavior (fixes #6233)
    x = std::min(width, std::max(0, x));
    y = std::min(height, std::max(0, y));
}

void RtWindow::set_title_decorated (Glib::ustring fname)
{
    Glib::ustring subtitle;

    if (!fname.empty()) {
        subtitle = " - " + fname;
    }

    set_title (versionStr + subtitle);
}

void RtWindow::closeOpenEditors()
{
    std::map<Glib::ustring, EditorPanel*>::const_iterator itr;
    itr = epanels.begin();

    while (itr != epanels.end()) {
        remEditorPanel ((*itr).second);
        itr = epanels.begin();
    }
}

bool RtWindow::isEditorPanel (Widget* panel)
{
    return (panel != bpanel) && (panel != fpanel);
}

bool RtWindow::isEditorPanel (guint pageNum)
{
    return isEditorPanel (mainNB->get_nth_page (pageNum));
}

void RtWindow::setEditorMode (bool tabbedUI)
{
    MoveFileBrowserToMain();
    closeOpenEditors();
    SetMainCurrent();

    if (tabbedUI) {
        mainNB->remove_page (*epanel);
        epanel = nullptr;
        set_title_decorated ("");
    } else {
        createSetmEditor();
        epanel->show_all();
        set_title_decorated ("");
    }
}

void RtWindow::createSetmEditor()
{
    // Editor panel, single-tab mode only
    epanel = Gtk::manage ( new EditorPanel (fpanel) );
    epanel->setParent (this);
    epanel->setParentWindow (this);

    // decorate tab
    Gtk::Grid* const editorLabelGrid = Gtk::manage (new Gtk::Grid ());
    setExpandAlignProperties (editorLabelGrid, false, false, Gtk::Align::CENTER, Gtk::Align::CENTER);
    Gtk::Label* const el = Gtk::manage (new Gtk::Label ( Glib::ustring (" ") + M ("MAIN_FRAME_EDITOR") ));

    const auto pos = options.mainNBVertical ? Gtk::PositionType::TOP : Gtk::PositionType::RIGHT;

    if (options.mainNBVertical) {
        // el->set_angle (90);
    }

    editorLabelGrid->attach_next_to (*Gtk::manage (new RTImage ("aperture", Gtk::IconSize::LARGE)), pos, 1, 1);
    editorLabelGrid->attach_next_to (*el, pos, 1, 1);

    editorLabelGrid->set_tooltip_markup (M ("MAIN_FRAME_EDITOR_TOOLTIP"));
    editorLabelGrid->show_all ();
    epanel->tbTopPanel_1_visible (true); //show the toggle Top Panel button
    mainNB->append_page (*epanel, *editorLabelGrid);

}

bool RtWindow::isSingleTabMode() const
{
    return !options.tabbedUI && ! (options.multiDisplayMode > 0);
}
