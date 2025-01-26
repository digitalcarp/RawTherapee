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

#include <vector>

#include <gtkmm.h>

#include "guiutils.h"
#include "partialpastedlg.h"
#include "pparamschangelistener.h"
#include "profilechangelistener.h"

#include "rtengine/profilestore.h"
#include "rtengine/noncopyable.h"

class ProfileStoreComboBox;

namespace rtengine
{

class ProcEvent;

namespace procparams
{

class ProcParams;

class PartialProfile;

}

}
class RtImage;

class ProfilePanel final :
    public Gtk::Grid,
    public PParamsChangeListener,
    public ProfileStoreListener,
    public rtengine::NonCopyable
{

private:

    rtengine::procparams::PartialProfile* storedPProfile;
    Glib::ustring storedValue;
    Glib::ustring lastFilename;
    Glib::ustring imagePath;
    const Glib::ustring modeOn, modeOff;
    RtImage* const profileFillImage;
    Gtk::ToggleButton* fillMode;
    Gtk::TreeIter<Gtk::TreeRow> currRow;
    ProfileStoreEntry *lastSavedPSE;
    ProfileStoreEntry *customPSE;

    // Temporaries for async dialog handling
    Gtk::FileDialog* fileDialog;
    Gdk::ModifierType fileDialogState;
    const rtengine::procparams::PartialProfile* profileToSave;
    std::unique_ptr<rtengine::procparams::ProcParams> pasteProcParams;
    std::unique_ptr<ParamsEdited> pasteParamsEdited;

    void          profileFillModeToggled ();
    bool          isCustomSelected ();
    bool          isLastSavedSelected ();
    Gtk::TreeIter<Gtk::TreeRow> getCustomRow ();
    Gtk::TreeIter<Gtk::TreeRow> getLastSavedRow ();
    Gtk::TreeIter<Gtk::TreeRow> addCustomRow ();
    Gtk::TreeIter<Gtk::TreeRow> addLastSavedRow ();

    void choosePartialSaveFile(int response);
    void createSaveFileDialog();
    void copyPartialProfile(int response);
    void loadPartialProfile(int response);
    void pastePartialProfileWithClipboardEdited(int response);
    void pastePartialProfile(int response);

protected:

    static PartialPasteDlg* partialProfileDlg;
    ModButton* save;
    ModButton* load;
    ModButton* copy;
    ModButton* paste;
    ProfileStoreComboBox* profiles;
    rtengine::procparams::PartialProfile* custom;
    rtengine::procparams::PartialProfile* lastsaved;
    ProfileChangeListener* tpc;
    bool dontupdate;
    sigc::connection changeconn;
    static Gtk::Window* parent;
    void changeTo (const rtengine::procparams::PartialProfile* newpp, Glib::ustring profname);

public:

    explicit ProfilePanel ();
    ~ProfilePanel () override;

    void setProfileChangeListener (ProfileChangeListener* ppl)
    {
        tpc = ppl;
    }

    static void init (Gtk::Window* parentWindow);
    static void cleanup ();
    void storeCurrentValue() override;
    void updateProfileList () override;
    void restoreValue() override;

    void initProfile (const Glib::ustring& profileFullPath, rtengine::procparams::ProcParams* lastSaved);
    void setInitialFileName (const Glib::ustring& filename);

    // PParamsChangeListener interface
    void procParamsChanged(
        const rtengine::procparams::ProcParams* params,
        const rtengine::ProcEvent& ev,
        const Glib::ustring& descr,
        const ParamsEdited* paramsEdited = nullptr
    ) override;
    void clearParamChanges() override;

    // gui callbacks
    void save_clicked (Gdk::ModifierType state);
    void load_clicked (Gdk::ModifierType state);
    void copy_clicked (Gdk::ModifierType state);
    void paste_clicked (Gdk::ModifierType state);
    void selection_changed ();
    void writeOptions();

    void onSaveFileResponse(Glib::RefPtr<Gio::AsyncResult>& result);
    void onLoadFileResponse(Glib::RefPtr<Gio::AsyncResult>& result);
};
