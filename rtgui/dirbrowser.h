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
#include "svgpaintable.h"

template <class T>
class RtTreeListModel;
template <class T>
class RtTreeListNode;

class DirBrowser : public Gtk::Box
{
public:
    typedef sigc::signal<void(const Glib::ustring&, const Glib::ustring&)> DirSelectionSignal;

private:

    class DirColumns : public Glib::Object {
    public:
        Glib::ustring filename;
        Glib::ustring dirname;
        Glib::RefPtr<SvgPaintableWrapper> icon;
        Glib::RefPtr<Gio::FileMonitor> monitor;

        static Glib::RefPtr<DirColumns> create(
            const Glib::ustring& filename,
            const Glib::ustring& dirname,
            const Glib::RefPtr<SvgPaintableWrapper>& icon,
            const Glib::RefPtr<Gio::FileMonitor>& monitor)
        {
            return Glib::make_refptr_for_instance<DirColumns>(
                new DirColumns(filename, dirname, icon, monitor));
        }

    private:
        DirColumns(const Glib::ustring& a_filename, const Glib::ustring& a_dirname,
                   const Glib::RefPtr<SvgPaintableWrapper>& a_icon,
                   const Glib::RefPtr<Gio::FileMonitor>& a_monitor)
            : filename(a_filename), dirname(a_dirname), icon(a_icon), monitor(a_monitor)
        {}
    };
    using DirModel = RtTreeListModel<DirColumns>;
    using DirNode = RtTreeListNode<DirColumns>;

    Gtk::ColumnView columnView;
    Gtk::ScrolledWindow scrollWindow;
    Glib::RefPtr<Gtk::ColumnViewSorter> sorter;
    Glib::RefPtr<DirModel> dirTreeListModel;

    Glib::RefPtr<SvgPaintableWrapper> openFolderSvg;
    Glib::RefPtr<SvgPaintableWrapper> closeFolderSvg;
    Glib::RefPtr<SvgPaintableWrapper> cdromSvg;
    Glib::RefPtr<SvgPaintableWrapper> floppySvg;
    Glib::RefPtr<SvgPaintableWrapper> hddSvg;
    Glib::RefPtr<SvgPaintableWrapper> networkSvg;
    Glib::RefPtr<SvgPaintableWrapper> usbSvg;

    struct Connections {
        sigc::connection expanded;

        void disconnectAll()
        {
            expanded.disconnect();
        }
    };
    std::unordered_map<Gtk::ListItem*, Connections> row_to_connections;

    void setupRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void bindRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void unbindRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void teardownRow(const Glib::RefPtr<Gtk::ListItem>& item);

    void onSortChanged();

    void populateRootDirectories();
    void onFileChanged(const Glib::RefPtr<Gio::File>& file,
                       const Glib::RefPtr<Gio::File>& other_file,
                       Gio::FileMonitor::Event event,
                       const std::weak_ptr<DirNode>& weak_node);
    void updateDir(const Glib::RefPtr<DirNode>& node);
    void processDirChanges();

#ifdef _WIN32
    Glib::RefPtr<DirColumns> createForVolume(char letter) const;
#endif



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
    std::vector<Glib::RefPtr<DirNode>> updatedNodes;
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
