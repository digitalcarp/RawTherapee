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
template <class T>
class RtTreeListExpander;

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
    Glib::RefPtr<Gtk::ColumnViewColumn> column;
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
    std::unordered_map<Gtk::ListItem*, Connections> rowToConnections;

    void setupRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void bindRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void unbindRow(const Glib::RefPtr<Gtk::ListItem>& item);
    void teardownRow(const Glib::RefPtr<Gtk::ListItem>& item);

    void onSortChanged();
    void onRowActivated(RtTreeListExpander<DirColumns>* row);

    void populateRootDirectories();
    void onFileChanged(const Glib::RefPtr<Gio::File>& file,
                       const Glib::RefPtr<Gio::File>& otherFile,
                       Gio::FileMonitor::Event event,
                       const std::weak_ptr<DirNode>& weakNode);
    void updateDir(const Glib::RefPtr<DirNode>& node);
    void processDirChanges();
    guint expandTreeToDir(const Glib::ustring& absDirPath);

#ifdef _WIN32
    Glib::RefPtr<DirColumns> createForVolume(char letter) const;
    bool updateVolumes();

    unsigned int volumes = 0;
#endif

    DirSelectionSignal dirSelectionSignal;

    std::mutex mutex;
    std::vector<Glib::RefPtr<DirNode>> updatedNodes;
    Glib::Dispatcher dispatcher;

public:
    DirBrowser ();

    void open           (const Glib::ustring& dirName, const Glib::ustring& fileName = ""); // goes to dir "dirName" and selects file "fileName"
    void selectDir      (const Glib::ustring& dir);

    DirSelectionSignal& dirSelected() { return dirSelectionSignal; }
};
