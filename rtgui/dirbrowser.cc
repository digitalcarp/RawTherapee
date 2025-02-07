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
#include "dirbrowser.h"

#include <iostream>
#include <cstring>
#include <queue>
#include <unordered_set>

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#include <windows.h>
#endif

#include "guiutils.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtimage.h"
#include "treelist.h"

namespace
{

class NoopSorter : public Gtk::Sorter {
public:
    Gtk::Ordering compare_vfunc(gpointer lhs, gpointer rhs) override {
        return Gtk::Ordering::EQUAL;
    }
    Gtk::Sorter::Order get_order_vfunc() override {
        return Gtk::Sorter::Order::NONE;
    }
};

std::vector<Glib::ustring> listSubDirs (const Glib::RefPtr<Gio::File>& dir, bool addHidden)
{
    std::vector<Glib::ustring> subDirs;

    try {

        // CD-ROM with no disc inserted are reported, but do not exist.
        if (!Glib::file_test (dir->get_path (), Glib::FileTest::EXISTS)) {
            return subDirs;
        }

        auto enumerator = dir->enumerate_children ("standard::name,standard::type,standard::is-hidden");

        while (true) {
            try {
                auto file = enumerator->next_file ();
                if (!file) {
                    break;
                }
                if (file->get_file_type () != Gio::FileType::DIRECTORY) {
                    continue;
                }
                if (!addHidden && file->is_hidden ()) {
                    continue;
                }
                subDirs.push_back (file->get_name ());
            } catch (const Glib::Error& exception) {

                if (rtengine::settings->verbose) {
                    std::cerr << exception.what() << std::endl;
                }

            }
        }

    } catch (const Glib::Error& exception) {

        if (rtengine::settings->verbose) {
            std::cerr << "Failed to list subdirectories of \"" << dir->get_parse_name() << "\": " << exception.what () << std::endl;
        }

    }

    return subDirs;
}

}

DirBrowser::DirBrowser ()
{
    openFolderSvg = SvgPaintableWrapper::createFromIcon("folder-open-small");
    closeFolderSvg = SvgPaintableWrapper::createFromIcon("folder-closed-small");
    cdromSvg = SvgPaintableWrapper::createFromIcon("device-optical");
    floppySvg = SvgPaintableWrapper::createFromIcon("device-floppy");
    hddSvg = SvgPaintableWrapper::createFromIcon("device-hdd");
    networkSvg = SvgPaintableWrapper::createFromIcon("device-network");
    usbSvg = SvgPaintableWrapper::createFromIcon("device-usb");

    set_orientation(Gtk::Orientation::VERTICAL);

    scrollWindow.set_child(columnView);
    scrollWindow.set_vexpand(true);
    append(scrollWindow);

    columnView.set_reorderable(false);
    columnView.set_enable_rubberband(false);

    dirTreeListModel = RtTreeListModel<DirColumns>::create();

    auto selectionModel = Gtk::SingleSelection::create(dirTreeListModel);
    selectionModel->set_autoselect(false);
    selectionModel->set_can_unselect(true);
    columnView.set_model(selectionModel);

    auto factory = Gtk::SignalListItemFactory::create();
    factory->signal_setup().connect(sigc::mem_fun(*this, &DirBrowser::setupRow));
    factory->signal_bind().connect(sigc::mem_fun(*this, &DirBrowser::bindRow));
    factory->signal_unbind().connect(sigc::mem_fun(*this, &DirBrowser::unbindRow));
    factory->signal_teardown().connect(sigc::mem_fun(*this, &DirBrowser::teardownRow));

    column = Gtk::ColumnViewColumn::create(M("DIRBROWSER_FOLDERS"), factory);
    column->set_expand();
    // Set a noop sorter so that column headers can be clicked to choose order
    column->set_sorter(std::make_shared<NoopSorter>());
    columnView.append_column(column);
    columnView.sort_by_column(column, options.dirBrowserSortType);

    // Only ColumnViewSorter knows the SortType
    sorter = std::dynamic_pointer_cast<Gtk::ColumnViewSorter>(columnView.get_sorter());
    if (sorter) {
        sorter->property_primary_sort_order().signal_changed().connect(
            sigc::mem_fun(*this, &DirBrowser::onSortChanged));
    }

    onSortChanged();
    populateRootDirectories();

    dispatcher.connect(sigc::mem_fun(*this, &DirBrowser::processDirChanges));
}

void DirBrowser::setupRow(const Glib::RefPtr<Gtk::ListItem>& item)
{
    auto expander = Gtk::make_managed<RtTreeListExpander<DirColumns>>();
    auto box = Gtk::make_managed<Gtk::Box>();
    auto image = Gtk::make_managed<RtImage>();
    auto label = Gtk::make_managed<Gtk::Label>();
    box->append(*image);
    box->append(*label);
    expander->set_child(*box);
    item->set_child(*expander);
    // Required for expander shortcuts based on Gtk::TreeExpander API docs
    item->set_focusable(false);
}

void DirBrowser::bindRow(const Glib::RefPtr<Gtk::ListItem>& item)
{
    auto node = std::dynamic_pointer_cast<DirNode>(item->get_item());
    if (!node) return;

    Glib::RefPtr<DirColumns> col = node->data();
    if (!col) return;

    auto expander = static_cast<RtTreeListExpander<DirColumns>*>(item->get_child());
    expander->set_node(node);
    auto box = static_cast<Gtk::Box*>(expander->get_child());
    auto image = static_cast<RtImage*>(box->get_first_child());
    if (col->icon) {
        col->icon->setOnImage(image);
        image->show();
    } else {
        image->hide();
    }
    auto label = static_cast<Gtk::Label*>(box->get_last_child());
    label->set_text(col->filename);

    // Signals are disconnected in unbindRow()

    expander->signal_activate().connect(sigc::mem_fun(*this, &DirBrowser::onRowActivated));

    Connections& conn = rowToConnections[item.get()];
    conn.expanded = node->property_expanded().signal_changed().connect(
        [=]() {
            if (node->property_expanded().get_value()) {
                if (col->icon == nullptr || col->icon == closeFolderSvg) {
                    col->icon = openFolderSvg;
                    col->icon->setOnImage(image);
                    image->show();
                }

                updateDir(node);
            } else {
                if (col->icon == openFolderSvg) {
                    col->icon = closeFolderSvg;
                    col->icon->setOnImage(image);
                }
            }
        });
}

void DirBrowser::unbindRow(const Glib::RefPtr<Gtk::ListItem>& item)
{
    auto expander = static_cast<RtTreeListExpander<DirColumns>*>(item->get_child());
    expander->set_node(nullptr);

    auto it = rowToConnections.find(item.get());
    if (it == rowToConnections.end()) return;
    it->second.disconnectAll();
}

void DirBrowser::teardownRow(const Glib::RefPtr<Gtk::ListItem>& item)
{
    rowToConnections.erase(item.get());
}

void DirBrowser::onSortChanged()
{
    Gtk::SortType sort = sorter->get_primary_sort_order();
    options.dirBrowserSortType = sort;
    // ColumnView displayed arrow for sort type is opposite the expected
    // direction so ASCENDING is actually DESCENDING
    if (sort == Gtk::SortType::ASCENDING) {
        dirTreeListModel->set_sorter(
            [](const Glib::RefPtr<DirNode>& lhs, const Glib::RefPtr<DirNode>& rhs) {
                return lhs->data()->filename > rhs->data()->filename;
            });
    } else {
        dirTreeListModel->set_sorter(
            [](const Glib::RefPtr<DirNode>& lhs, const Glib::RefPtr<DirNode>& rhs) {
                return lhs->data()->filename < rhs->data()->filename;
            });
    }
}

void DirBrowser::onRowActivated(RtTreeListExpander<DirColumns>* row)
{
    Glib::RefPtr<DirNode> node = row->get_node();
    if (!node) return;

    Glib::RefPtr<DirColumns> data = node->data();
    if (Glib::file_test(data->dirname, Glib::FileTest::IS_DIR)) {
        dirSelectionSignal.emit(data->dirname, {});
        node->property_expanded().set_value(true);
    }
}

void DirBrowser::populateRootDirectories()
{
    auto transaction = dirTreeListModel->maybe_init_transaction();

#ifdef _WIN32
    volumes = GetLogicalDrives();
    for (int i = 0; i < 32; i++) {
        if ((volumes >> i) & 1) {
            dirTreeListModel->add_node(createForVolume('A' + i), nullptr);
        }
    }

    Glib::SignalTimeout::connect_seconds(
        sigc::mem_fun(*this, &DirBrowser::updateVolumes()),
        10 /*seconds*/);
#else
    dirTreeListModel->add_node(DirColumns::create("/", "/", closeFolderSvg, nullptr), nullptr);
#endif
}

void DirBrowser::onFileChanged(const Glib::RefPtr<Gio::File>& file,
                               const Glib::RefPtr<Gio::File>& otherFile,
                               Gio::FileMonitor::Event event,
                               const std::weak_ptr<DirNode>& weakNode)
{
    if (!file || event == Gio::FileMonitor::Event::ATTRIBUTE_CHANGED) return;

    auto node = weakNode.lock();
    if (!node) return;

    const std::lock_guard<std::mutex> lock(mutex);
    bool shouldEmit = updatedNodes.empty();
    updatedNodes.push_back(node);
    if (shouldEmit) {
        dispatcher.emit();
    }
}

void DirBrowser::updateDir(const Glib::RefPtr<DirNode>& node)
{
    GuiThreadSafety::assertInGuiThread();

    if (!dirTreeListModel->owns_node(node.get())) return;

    Glib::RefPtr<DirColumns> data = node->data();
    if (!data) return;

    auto transaction = dirTreeListModel->maybe_init_transaction();

    if (!Glib::file_test(data->dirname, Glib::FileTest::EXISTS) ||
            !Glib::file_test(data->dirname, Glib::FileTest::IS_DIR)) {
        dirTreeListModel->remove_node(node);
        return;
    }

    std::unordered_set<std::string> current_children;
    for (const auto& child : node->children()) {
        auto data = child->data();
        if (!data) continue;

        if (!Glib::file_test(data->dirname, Glib::FileTest::EXISTS) ||
                !Glib::file_test(data->dirname, Glib::FileTest::IS_DIR)) {
            dirTreeListModel->remove_node(child);
        } else {
            current_children.insert(data->filename.collate_key());
        }
    }

    auto dir = Gio::File::create_for_path(data->dirname);
    auto subDirs = listSubDirs(dir, options.fbShowHidden);

    for (const auto& dirname : subDirs) {
        auto it = current_children.find(dirname.collate_key());
        if (it != current_children.end()) continue;

        Glib::ustring fullname = Glib::build_filename(data->dirname, dirname);
        auto newData = DirColumns::create(dirname, fullname, closeFolderSvg, nullptr);
        auto newNode = dirTreeListModel->add_node(newData, node.get());

        Glib::RefPtr<Gio::FileMonitor> monitor = dir->monitor_directory();
        newData->monitor = dir->monitor_directory();
        // Creating a weak_ptr here must use the shared_ptr from add_node().
        // See RtTreeListModel<T> comments for why this is required.
        newData->monitor->signal_changed().connect(
            sigc::bind(sigc::mem_fun(*this, &DirBrowser::onFileChanged), std::weak_ptr(newNode)));
    }
}

void DirBrowser::processDirChanges()
{
    const std::lock_guard<std::mutex> lock(mutex);
    auto transaction = dirTreeListModel->maybe_init_transaction();
    for (const auto& node : updatedNodes) {
        updateDir(node);
    }
    updatedNodes.clear();
}

guint DirBrowser::expandTreeToDir(const Glib::ustring& absDirPath)
{
    std::vector<Glib::ustring> dirStack;
    auto path = Gio::File::create_for_path(absDirPath.c_str());
    while (path) {
        auto parent = path->get_parent();
        if (parent) {
            dirStack.push_back(parent->get_relative_path(path).c_str());
        } else {
            // This is a top level so we can use the absolute path for Windows
            // volume identifier support.
            dirStack.push_back(path->get_path().c_str());
        }
        path = parent;
    }

    auto transaction = dirTreeListModel->maybe_init_transaction();

    Glib::RefPtr<DirNode> parent = nullptr;  // Start at tree root
    while (!dirStack.empty()) {
        Glib::ustring dir = std::move(dirStack.back());
        dirStack.pop_back();

        auto pred = [&](const DirNode* node) {
            return node->data()->filename == dir;
        };
        Glib::RefPtr<DirNode> found = dirTreeListModel->find_if(parent.get(), pred);

        if (found) {
            updateDir(found);
            found->property_expanded().set_value(true);
            parent = found;
        } else {
            break;
        }
    }

    transaction.commit();

    std::optional<guint> pos = dirTreeListModel->find_pos(parent.get());
    return pos ? *pos : 0;
}

#ifdef _WIN32
Glib::RefPtr<DirColumns> DirBrowser::createForVolume(char letter) const
{
    auto volume = Glib::ustring::compose("%1:\\", letter);
    Glib::RefPtr<SvgPaintableWrapper> svg;

    int type = GetDriveType(volume);
    switch (type) {
    case DRIVE_CDROM:
        svg = cdromSvg;
        break;
    case DRIVE_REMOVABLE:
        svg = (letter - 'A' < 2) ? floppySvg : usbSvg;
        break;
    case DRIVE_REMOTE:
        svg = networkSvg;
        break;
    case DRIVE_FIXED:
        svg = hddSvg;
        break;
    default:
        break;
    }

    return DirColumns::create(volume, volume, svg, nullptr);
}

bool DirBrowser::updateVolumes()
{
    GuiThreadSafety::assertInGuiThread();

    unsigned int nvolumes  = GetLogicalDrives();
    if (nvolumes == volumes) return true;

    std::unordered_set<std::string> to_delete;

    auto transaction = dirTreeListModel->maybe_init_transaction();

    for (int i = 0; i < 32; i++) {
        if (((volumes >> i) & 1) && !((nvolumes >> i) & 1)) {
            to_delete.insert(Glib::ustring::compose("%1:\\", 'A' + i).collate_key());
        } else if (!((volumes >> i) & 1) && ((nvolumes >> i) & 1)) {
            // Volume i added
            dirTreeListModel->add_node(createForVolume('A' + i), nullptr);
        }
    }

    dirTreeListModel->remove_children_if(nullptr, [&](const DirNode* node) {
        return to_delete.count(node->data()->filename.collate_key()) != 0;
    });

    volumes = nvolumes;

    return true;
}
#endif  // _WIN32

void DirBrowser::open (const Glib::ustring& dirname, const Glib::ustring& fileName)
{
    // WARNING & TODO: One should test here if the directory/file has R/W access permission to avoid crash

    Glib::RefPtr<Gio::File> dir = Gio::File::create_for_path(dirname);

    if( !dir->query_exists()) {
        return;
    }

    Glib::ustring absDirPath = dir->get_parse_name ();

    guint pos = expandTreeToDir(absDirPath);
    columnView.scroll_to(pos, column, Gtk::ListScrollFlags::SELECT);

    Glib::ustring absFilePath;
    if (!fileName.empty()) {
        absFilePath = Glib::build_filename (absDirPath, fileName);
    }

    dirSelectionSignal (absDirPath, absFilePath);
}

void DirBrowser::selectDir (const Glib::ustring& dir)
{
    open (dir, "");
}
