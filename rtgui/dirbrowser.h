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

#include <mutex>

#include <gtkmm.h>
#include <giomm.h>

#include "guiutils.h"

class DirBrowser : public Gtk::Box
{
public:
    typedef sigc::signal<void(const Glib::ustring&, const Glib::ustring&)> DirSelectionSignal;

private:

    Glib::RefPtr<Gtk::TreeStore> dirTreeModel;

    struct DirTreeColumns : public Gtk::TreeModelColumnRecord {
    public:
        Gtk::TreeModelColumn<Glib::ustring> filename;
        Gtk::TreeModelColumn<Glib::ustring> icon_name;
        Gtk::TreeModelColumn<Glib::ustring> dirname;
        Gtk::TreeModelColumn<Glib::RefPtr<Gio::FileMonitor> > monitor;

        DirTreeColumns()
        {
            add(icon_name);
            add(filename);
            add(dirname);
            add(monitor);
        }
    };

    DirTreeColumns dtColumns;
    Gtk::TreeViewColumn tvc;
    Gtk::CellRendererText crt;


    Gtk::TreeView *dirtree;
    Gtk::ScrolledWindow *scrolledwindow4;
    DirSelectionSignal dirSelectionSignal;

    std::mutex mutex;
    std::vector<Gtk::TreeIter<Gtk::TreeRow>> updatedDirs;
    Glib::Dispatcher dispatcher;
    Glib::Dispatcher winDispatcher;

    void fillRoot ();

    Glib::ustring openfolder;
    Glib::ustring closedfolder;
    Glib::ustring icdrom;
    Glib::ustring ifloppy;
    Glib::ustring ihdd;
    Glib::ustring inetwork;
    Glib::ustring iremovable;

    bool expandSuccess;

#ifdef _WIN32
    unsigned int volumes;
    void addRoot (char letter);
public:
    void requestUpdateVolumes ();
private:
#endif
    void addDir (const Gtk::TreeModel::iterator& iter, const Glib::ustring& dirname);
    Gtk::TreePath expandToDir (const Glib::ustring& dirName);
    void updateVolumes ();
    void updateDirs();
    void updateDir (Gtk::TreeIter<Gtk::TreeRow>& iter);

public:
    DirBrowser ();

    void fillDirTree ();
    void on_sort_column_changed() const;
    void row_expanded   (const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);
    void row_collapsed  (const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);
    void row_activated  (const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column);
    void file_changed   (const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& other_file, Gio::FileMonitor::Event event_type, const Gtk::TreeModel::iterator& iter, const Glib::ustring& dirName);
    void open           (const Glib::ustring& dirName, const Glib::ustring& fileName = ""); // goes to dir "dirName" and selects file "fileName"
    void selectDir      (Glib::ustring dir);

    DirSelectionSignal dirSelected () const;
};

inline DirBrowser::DirSelectionSignal DirBrowser::dirSelected () const
{
    return dirSelectionSignal;
}
