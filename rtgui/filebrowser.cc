/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *  Copyright (c) 2011 Oliver Duis <www.oliverduis.de>
 *  Copyright (c) 2011 Michael Ezra <www.michaelezra.com>
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
#include <algorithm>
#include <map>

#include <glibmm/ustring.h>

#include "filebrowser.h"

// #include "batchqueue.h"
#include "clipboard.h"
#include "inspector.h"
#include "multilangmgr.h"
#include "options.h"
#include "paramsedited.h"
#include "procparamchangers.h"
#include "rtimage.h"
#include "rtmessagedialog.h"
#include "threadutils.h"
#include "thumbnail.h"

#include "rtengine/dfmanager.h"
#include "rtengine/ffmanager.h"
#include "rtengine/procparams.h"

#define GET_SELECTED_ITEMS()        \
    if (!tbl) return;               \
    auto mselected = getSelected(); \
    if (mselected.empty()) return;

namespace
{

const char* FILEBROWSER_ACTION_GROUP = "filebrowser";
const char* SELECT_ALL_ACTION_NAME = "select-all";
const char* RENAME_ACTION_NAME = "rename";
const char* PROCESS_ACTION_NAME = "process";

Glib::ustring getActionName(const char* name) {
    return Glib::ustring::compose("%1.%2", FILEBROWSER_ACTION_GROUP, name);
}

Glib::ustring getActionName(const char* name, int target) {
    return Glib::ustring::compose("%1.%2(%3)", FILEBROWSER_ACTION_GROUP, name, target);
}

const Glib::ustring* getOriginalExtension (const ThumbBrowserEntryBase* entry)
{
    // We use the parsed extensions as a priority list,
    // i.e. what comes earlier in the list is considered an original of what comes later.
    typedef std::vector<Glib::ustring> ExtensionVector;
    typedef ExtensionVector::const_iterator ExtensionIterator;

    const ExtensionVector& originalExtensions = options.parsedExtensions;

    // Extract extension from basename
    const Glib::ustring basename = Glib::path_get_basename (entry->filename.lowercase().c_str());

    const Glib::ustring::size_type pos = basename.find_last_of ('.');
    if (pos >= basename.length () - 1) {
        return nullptr;
    }

    const Glib::ustring extension = basename.substr (pos + 1);

    // Try to find a matching original extension
    for (ExtensionIterator originalExtension = originalExtensions.begin(); originalExtension != originalExtensions.end(); ++originalExtension) {
        if (*originalExtension == extension) {
            return &*originalExtension;
        }
    }

    return nullptr;
}

ThumbBrowserEntryBase* selectOriginalEntry (ThumbBrowserEntryBase* original, ThumbBrowserEntryBase* candidate)
{
    if (original == nullptr) {
        return candidate;
    }

    // The candidate will become the new original, if it has an original extension
    // and if its extension is higher in the list than the old original.
    if (const Glib::ustring* candidateExtension = getOriginalExtension (candidate)) {
        if (const Glib::ustring* originalExtension = getOriginalExtension (original)) {
            return candidateExtension < originalExtension ? candidate : original;
        }
    }

    return original;
}

void findOriginalEntries (const std::vector<ThumbBrowserEntryBase*>& entries)
{
    // Sort all entries into buckets by basename without extension
    std::map<Glib::ustring, std::vector<ThumbBrowserEntryBase*>> byBasename;

    for (const auto entry : entries) {
        const auto basename = Glib::path_get_basename(entry->filename.lowercase().c_str());

        const auto pos = basename.find_last_of('.');
        if (pos >= basename.length() - 1) {
            entry->setOriginal(nullptr);
            continue;
        }

        const auto withoutExtension = basename.substr(0, pos);

        byBasename[withoutExtension].push_back(entry);
    }

    // Find the original image for each bucket
    for (const auto& bucket : byBasename) {
        const auto& lentries = bucket.second;
        ThumbBrowserEntryBase* original = nullptr;

        // Select the most likely original in a first pass...
        for (const auto entry : lentries) {
            original = selectOriginalEntry(original, entry);
        }

        // ...and link all other images to it in a second pass.
        for (const auto entry : lentries) {
            entry->setOriginal(entry != original ? original : nullptr);
        }
    }
}

}

FileBrowser::FileBrowser () :
    colorLabel_actionData(nullptr),
    bppcl(nullptr),
    tbl(nullptr),
    numFiltered(0),
    exportPanel(nullptr)
{
    session_id_ = 0;

    ProfileStore::getInstance()->addListener(this);

    pmenuActions = Gio::SimpleActionGroup::create();
    contextMenuModel = Gio::Menu::create();
    colorLabelMenuModel = Gio::Menu::create();
    pmenuShortcutController = Gtk::ShortcutController::create();
    pmenuShortcutController->set_scope(Gtk::ShortcutScope::LOCAL);

    auto section = Gio::Menu::create();
    auto startNewSection = [&]() {
        contextMenuModel->append_section(section);
        section = Gio::Menu::create();
    };

    {
        const char* POPUP_OPEN = "open";
        pmenuActions->add_action(POPUP_OPEN, [&]() {
            GET_SELECTED_ITEMS();
            openRequested(mselected);
        });
        section->append(M("FILEBROWSER_POPUPOPEN"), getActionName(POPUP_OPEN));
        pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Return),
            Gtk::NamedAction::create(getActionName(POPUP_OPEN))));

        if (options.inspectorWindow) {
            const char* POPUP_INSPECT = "inspect";
            pmenuActions->add_action(POPUP_INSPECT, [&]() {
                GET_SELECTED_ITEMS();
                inspectRequested(mselected);
            });
            section->append(M("FILEBROWSER_POPUPINSPECT"), getActionName(POPUP_INSPECT));
            inspectShortcut = Gtk::Shortcut::create(
                Gtk::KeyvalTrigger::create(GDK_KEY_f),
                Gtk::NamedAction::create(getActionName(POPUP_INSPECT)));
            pmenuShortcutController->add_shortcut(inspectShortcut);
        }

        pmenuActions->add_action(PROCESS_ACTION_NAME, [&]() {
            GET_SELECTED_ITEMS();
            tbl->developRequested(mselected, false);
        });
        // TODO(gtk4): Needs icon "gears"
        section->append(M("FILEBROWSER_POPUPPROCESS"), getActionName(PROCESS_ACTION_NAME));
        pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_b, Gdk::ModifierType::CONTROL_MASK),
            Gtk::NamedAction::create(getActionName(PROCESS_ACTION_NAME))));

        const char* POPUP_PROCESS_FAST = "process-fast";
        pmenuActions->add_action(POPUP_PROCESS_FAST, [&]() { activateProcessFast(); });
        section->append(M("FILEBROWSER_POPUPPROCESSFAST"), getActionName(POPUP_PROCESS_FAST));
        pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(
                GDK_KEY_b, Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::SHIFT_MASK),
            Gtk::NamedAction::create(getActionName(POPUP_PROCESS_FAST))));

        startNewSection();
        pmenuActions->add_action(SELECT_ALL_ACTION_NAME, [&]() { activateSelectAll(); });
        section->append(M("FILEBROWSER_POPUPSELECTALL"), getActionName(SELECT_ALL_ACTION_NAME));
        pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_a, Gdk::ModifierType::CONTROL_MASK),
            Gtk::NamedAction::create(getActionName(SELECT_ALL_ACTION_NAME))));
    }
    appendSortMenu(section);
    appendRankMenu(section);

    // separate Rank and Color Labels if either is not grouped
    if (!options.menuGroupRank || !options.menuGroupLabel) {
        startNewSection();
    }

    // Populates contextMenuModel and colorLabelMenuModel
    appendColorLabelMenu(section);

    startNewSection();
    appendExternalProgramMenu(section);
    startNewSection();
    appendFileOperationsMenu(section);
    if (!options.menuGroupProfileOperations) {
        startNewSection();
    }
    appendProfileOperationsMenu(section);
    startNewSection();
    appendDarkFrameMenu(section);
    appendFlatFieldMenu(section);
    startNewSection();
    appendCacheMenu(section);
    contextMenuModel->append_section(section);

    pmenu = std::make_shared<Gtk::PopoverMenu>();
    pmenu->insert_action_group("filebrowser", pmenuActions);
    pmenu->set_menu_model(contextMenuModel);
    pmenu->set_flags(Gtk::PopoverMenu::Flags::NESTED);
    pmenu->set_has_arrow(false);
    pmenu->add_controller(pmenuShortcutController);
    popoverBin.set_popover(pmenu);

    // Has to be located after creation of profileOperationsMenu
    updateProfileList();
}

FileBrowser::~FileBrowser ()
{
    ProfileStore::getInstance()->removeListener(this);
}

void FileBrowser::appendSortMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto sortSubmenu = Gio::Menu::create();

    const char* SORT_ORDER = "sort-order";
    auto orderRadioAction = Gio::SimpleAction::create_radio_integer(
        SORT_ORDER, options.sortDescending ? 1 : 0);
    orderRadioAction->signal_activate().connect(
        [&, orderRadioAction](const Glib::VariantBase& state) {
            orderRadioAction->set_state(state);
            auto order = Glib::VariantBase::cast_dynamic<Glib::Variant<gint32>>(state);
            sortOrderRequested(order.get());
        });
    pmenuActions->add_action(orderRadioAction);

    auto sortOrderSection = Gio::Menu::create();
    constexpr std::array<const char*, 2> sortOrders = {
        "SORT_ASCENDING",
        "SORT_DESCENDING"
    };
    for (int i = 0; i < 2; i++) {
        sortOrderSection->append(M(sortOrders[i]), getActionName(SORT_ORDER, i));
    }
    sortSubmenu->append_section(sortOrderSection);

    const char* SORT_METHOD = "sort-method";
    auto methodRadioAction = Gio::SimpleAction::create_radio_integer(
        SORT_METHOD, static_cast<gint32>(options.sortMethod));
    methodRadioAction->signal_activate().connect(
        [&, methodRadioAction](const Glib::VariantBase& state) {
            methodRadioAction->set_state(state);
            auto method = Glib::VariantBase::cast_dynamic<Glib::Variant<gint32>>(state);
            sortMethodRequested(method.get());
        });
    pmenuActions->add_action(methodRadioAction);

    auto sortMethodSection = Gio::Menu::create();
    constexpr std::array<const char*, Options::SORT_METHOD_COUNT> sortMethods = {
        "SORT_BY_NAME",
        "SORT_BY_DATE",
        "SORT_BY_EXIF",
        "SORT_BY_RANK",
        "SORT_BY_LABEL"
    };
    for (int i = 0; i < Options::SORT_METHOD_COUNT; i++) {
        sortMethodSection->append(M(sortMethods[i]), getActionName(SORT_METHOD, i));
    }
    sortSubmenu->append_section(sortMethodSection);

    section->append_submenu(M("FILEBROWSER_POPUPSORTBY"), sortSubmenu);
}

void FileBrowser::appendRankMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto rankMenu = Gio::Menu::create();
    {
        pmenuActions->add_action("rank0", [&]() { activateRank(0); });
        rankMenu->append(M("FILEBROWSER_POPUPUNRANK"), getActionName("rank0"));
    }
    for (int i = 1; i <= 5; i++) {
        auto actionName = Glib::ustring::compose("rank%1", i);
        pmenuActions->add_action(actionName, [&, i]() { activateRank(i); });
        rankMenu->append(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPRANK", i)),
                         getActionName(actionName.c_str()));
    }
    if (options.menuGroupRank) {
        section->append_submenu(M("FILEBROWSER_POPUPRANK"), rankMenu);
    } else {
        section->append_section(rankMenu);
    }
}

void FileBrowser::appendColorLabelMenu(Glib::RefPtr<Gio::Menu>& section)
{
    // Thumbnail context menu
    // Similar image arrays in filecatalog.cc
    constexpr int COLOR_LABEL_SIZE = 6;
    // TODO(gtk4): image rows
    // constexpr std::array<const char*, COLOR_LABEL_SIZE> activeLabelIcons = {
    //     "circle-empty-gray-small",
    //     "circle-red-small",
    //     "circle-yellow-small",
    //     "circle-green-small",
    //     "circle-blue-small",
    //     "circle-purple-small"
    // };
    // constexpr std::array<const char*, COLOR_LABEL_SIZE> inactiveLabelIcons = {
    //     "circle-empty-darkgray-small",
    //     "circle-empty-red-small",
    //     "circle-empty-yellow-small",
    //     "circle-empty-green-small",
    //     "circle-empty-blue-small",
    //     "circle-empty-purple-small"
    // };

    auto colorLabelMenu = Gio::Menu::create();
    for (int i = 0; i < COLOR_LABEL_SIZE; i++) {
        auto actionName = Glib::ustring::compose("color%1", i);
        pmenuActions->add_action(actionName, [&, i]() { activateColorLabel(i); });
        colorLabelMenu->append(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPCOLORLABEL", i)),
                               getActionName(actionName.c_str()));
    }
    if (options.menuGroupLabel) {
        // TODO(gtk4): with active icon [i]
        section->append_submenu(M("FILEBROWSER_POPUPCOLORLABEL"), colorLabelMenu);
    } else {
        // TODO(gtk4): with inactive icon [i]
        section->append_submenu(M("FILEBROWSER_POPUPCOLORLABEL"), colorLabelMenu);
        section->append_section(colorLabelMenu);
    }

    for (int i = 0; i < COLOR_LABEL_SIZE; i++) {
        auto actionName = Glib::ustring::compose("color-data%1", i);
        pmenuActions->add_action(actionName, [&, i]() { activateActionDataColorLabel(i); });
        // TODO(gtk4): with active icon [i]
        colorLabelMenu->append(M(Glib::ustring::compose("%1%2", "FILEBROWSER_POPUPCOLORLABEL", i)),
                               getActionName(actionName.c_str()));
    }
}

void FileBrowser::appendFileOperationsMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto section1 = Gio::Menu::create();

    const char* TRASH = "trash";
    trashAction = Gio::SimpleAction::create(TRASH);
    trashAction->signal_activate().connect([&](auto) {
        GET_SELECTED_ITEMS();
        toTrashRequested(mselected);
    });
    pmenuActions->add_action(trashAction);
    section1->append(M("FILEBROWSER_POPUPTRASH"), getActionName(TRASH));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_Delete),
        Gtk::NamedAction::create(getActionName(TRASH))));

    const char* UNTRASH = "untrash";
    untrashAction = Gio::SimpleAction::create(UNTRASH);
    untrashAction->signal_activate().connect([&](auto) {
        GET_SELECTED_ITEMS();
        fromTrashRequested(mselected);
    });
    pmenuActions->add_action(untrashAction);
    section1->append(M("FILEBROWSER_POPUPUNTRASH"), getActionName(UNTRASH));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_Delete, Gdk::ModifierType::SHIFT_MASK),
        Gtk::NamedAction::create(getActionName(UNTRASH))));

    auto section2 = Gio::Menu::create();

    pmenuActions->add_action(RENAME_ACTION_NAME, [&]() {
        GET_SELECTED_ITEMS();
        tbl->renameRequested(mselected);
    });
    section2->append(M("FILEBROWSER_POPUPRENAME"), getActionName(RENAME_ACTION_NAME));

    const char* REMOVE = "remove";
    pmenuActions->add_action(REMOVE, [&]() {
        GET_SELECTED_ITEMS();
        tbl->deleteRequested(mselected, false, true);
    });
    section2->append(M("FILEBROWSER_POPUPREMOVE"), getActionName(REMOVE));

    const char* REMOVE_INCL_PROC = "remove-incl-proc";
    pmenuActions->add_action(REMOVE_INCL_PROC, [&]() {
        GET_SELECTED_ITEMS();
        tbl->deleteRequested(mselected, true, true);
    });
    section2->append(M("FILEBROWSER_POPUPREMOVEINCLPROC"), getActionName(REMOVE_INCL_PROC));

    auto section3 = Gio::Menu::create();

    const char* COPY_TO = "copy-to";
    copyToAction = Gio::SimpleAction::create(COPY_TO);
    copyToAction->signal_activate().connect([&](auto) {
        GET_SELECTED_ITEMS();
        tbl->copyMoveRequested(mselected, false);
    });
    pmenuActions->add_action(copyToAction);
    section3->append(M("FILEBROWSER_POPUPCOPYTO"), getActionName(COPY_TO));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(
            GDK_KEY_c, Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::SHIFT_MASK),
        Gtk::NamedAction::create(getActionName(COPY_TO))));

    const char* MOVE_TO = "move-to";
    moveToAction = Gio::SimpleAction::create(MOVE_TO);
    moveToAction->signal_activate().connect([&](auto) {
        GET_SELECTED_ITEMS();
        tbl->copyMoveRequested(mselected, true);
    });
    pmenuActions->add_action(moveToAction);
    section3->append(M("FILEBROWSER_POPUPMOVETO"), getActionName(MOVE_TO));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(
            GDK_KEY_m, Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::SHIFT_MASK),
        Gtk::NamedAction::create(getActionName(MOVE_TO))));

    auto menu = Gio::Menu::create();
    menu->append_section(section1);
    menu->append_section(section2);
    menu->append_section(section3);

    if (options.menuGroupFileOperations) {
        section->append_submenu(M("FILEBROWSER_POPUPFILEOPERATIONS"), menu);
    } else {
        section->append_section(menu);
    }
}

void FileBrowser::appendProfileOperationsMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto menu = Gio::Menu::create();
    profileOperationsMenu = menu;

    const char* COPY = "copy-profile";
    copyProfileAction = Gio::SimpleAction::create(COPY);
    copyProfileAction->signal_activate().connect([&](auto) { copyProfile(); });
    pmenuActions->add_action(copyProfileAction);
    menu->append(M("FILEBROWSER_COPYPROFILE"), getActionName(COPY));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_c, Gdk::ModifierType::CONTROL_MASK),
        Gtk::NamedAction::create(getActionName(COPY))));

    const char* PASTE = "paste-profile";
    pasteProfileAction = Gio::SimpleAction::create(PASTE);
    pasteProfileAction->signal_activate().connect([&](auto) { pasteProfile(); });
    pmenuActions->add_action(pasteProfileAction);
    menu->append(M("FILEBROWSER_PASTEPROFILE"), getActionName(PASTE));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_v, Gdk::ModifierType::CONTROL_MASK),
        Gtk::NamedAction::create(getActionName(PASTE))));

    const char* PASTE_PARTIAL = "partial-paste-profile";
    partialPasteProfileAction = Gio::SimpleAction::create(PASTE_PARTIAL);
    partialPasteProfileAction->signal_activate().connect([&](auto) { partPasteProfile(); });
    pmenuActions->add_action(partialPasteProfileAction);
    menu->append(M("FILEBROWSER_PARTIALPASTEPROFILE"), getActionName(PASTE_PARTIAL));
    pmenuShortcutController->add_shortcut(Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(
            GDK_KEY_v, Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::SHIFT_MASK),
        Gtk::NamedAction::create(getActionName(PASTE_PARTIAL))));

    // This positioning is hard-coded in updateProfileList()
    menu->append_submenu(M("FILEBROWSER_APPLYPROFILE"), Gio::Menu::create());
    menu->append_submenu(M("FILEBROWSER_APPLYPROFILE_PARTIAL"), Gio::Menu::create());

    const char* RESET = "reset-default-profile";
    pmenuActions->add_action(RESET, [&]() { activateResetDefaultProfile(); });
    menu->append(M("FILEBROWSER_RESETDEFAULTPROFILE"), getActionName(RESET));

    const char* CLEAR = "clear-profile";
    clearProfileAction = Gio::SimpleAction::create(CLEAR);
    clearProfileAction->signal_activate().connect([&](auto) {
        GET_SELECTED_ITEMS();
        for (size_t i = 0; i < mselected.size(); i++) {
            mselected[i]->thumbnail->clearProcParams(FILEBROWSER);
        }
        redraw();
    });
    pmenuActions->add_action(clearProfileAction);
    menu->append(M("FILEBROWSER_CLEARPROFILE"), getActionName(CLEAR));

    if (options.menuGroupProfileOperations) {
        section->append_submenu(M("FILEBROWSER_POPUPPROFILEOPERATIONS"), menu);
    } else {
        section->append_section(menu);
    }
}

void FileBrowser::appendExternalProgramMenu(Glib::RefPtr<Gio::Menu>& section)
{
#if defined(_WIN32)
    const char* OPEN_DEFAULT = "open-default-viewer";
    pmenu->add_action(OPEN_DEFAULT, [&]() { openDefaultViewer(1); });
    section->append(M("FILEBROWSER_OPENDEFAULTVIEWER"), getActionName(OPEN_DEFAULT));
#endif
    // Build a list of menu items
    mMenuExtProgs.clear();
    for (const auto& action : extProgStore->getActions()) {
        if (action.target == 1 || action.target == 2) {
            mMenuExtProgs[action.getFullName()] = &action;
        }
    }
    if (mMenuExtProgs.empty()) return;

    auto menu = Gio::Menu::create();
    int itemNo = 0;
    for (auto it = mMenuExtProgs.begin(); it != mMenuExtProgs.end(); it++, itemNo++) {
        auto name = Glib::ustring::compose("external-program%1", itemNo);
        auto action = Gio::SimpleAction::create(name);
        action->signal_activate().connect([&, externalName=it->first](auto) {
            activateExternalProgram(externalName);
        });
        pmenuActions->add_action(action);
        menu->append(it->first, getActionName(name.c_str()));
    }

    if (options.menuGroupExtProg) {
        section->append_submenu(M("FILEBROWSER_EXTPROGMENU"), menu);
    } else {
        section->append_section(menu);
    }
}

void FileBrowser::appendDarkFrameMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto menu = Gio::Menu::create();

    const char* SELECT = "select-dark-frame";
    pmenuActions->add_action(SELECT, [&]() { activateSelectDarkFrame(); });
    menu->append(M("FILEBROWSER_SELECTDARKFRAME"), getActionName(SELECT));

    const char* AUTO = "auto-dark-frame";
    pmenuActions->add_action(AUTO, [&]() { activateAutoDarkFrame(); });
    menu->append(M("FILEBROWSER_AUTODARKFRAME"), getActionName(AUTO));

    const char* MOVE = "move-to-dark-frame";
    pmenuActions->add_action(MOVE, [&]() { activateMoveToDarkFrameDir(); });
    menu->append(M("FILEBROWSER_MOVETODARKFDIR"), getActionName(MOVE));

    section->append_submenu(M("FILEBROWSER_DARKFRAME"), menu);
}

void FileBrowser::appendFlatFieldMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto menu = Gio::Menu::create();

    const char* SELECT = "select-flat-field";
    pmenuActions->add_action(SELECT, [&]() { activateSelectFlatField(); });
    menu->append(M("FILEBROWSER_SELECTFLATFIELD"), getActionName(SELECT));

    const char* AUTO = "auto-flat-field";
    pmenuActions->add_action(AUTO, [&]() { activateAutoFlatField(); });
    menu->append(M("FILEBROWSER_AUTOFLATFIELD"), getActionName(AUTO));

    const char* MOVE = "move-to-flat-field";
    pmenuActions->add_action(MOVE, [&]() { activateMoveToFlatFieldDir(); });
    menu->append(M("FILEBROWSER_MOVETOFLATFIELDDIR"), getActionName(MOVE));

    section->append_submenu(M("FILEBROWSER_FLATFIELD"), menu);
}

void FileBrowser::appendCacheMenu(Glib::RefPtr<Gio::Menu>& section)
{
    auto menu = Gio::Menu::create();

    const char* CLEAR_PARTIAL = "clear-cache-partial";
    pmenuActions->add_action(CLEAR_PARTIAL, [&]() {
        GET_SELECTED_ITEMS();
        tbl->clearFromCacheRequested(mselected, false);
    });
    menu->append(M("FILEBROWSER_CACHECLEARFROMPARTIAL"), getActionName(CLEAR_PARTIAL));

    const char* CLEAR_FULL = "clear-cache-full";
    pmenuActions->add_action(CLEAR_FULL, [&]() {
        GET_SELECTED_ITEMS();
        tbl->clearFromCacheRequested(mselected, true);
    });
    menu->append(M("FILEBROWSER_CACHECLEARFROMFULL"), getActionName(CLEAR_FULL));

    section->append_submenu(M("FILEBROWSER_CACHE"), menu);
}

void FileBrowser::rightClicked(double x, double y)
{
    pmenu->set_menu_model(contextMenuModel);
    pmenu->set_pointing_to(Gdk::Rectangle(x, y, 1, 1));
    pmenu->popup();

    {
        MYREADERLOCK(l, entryRW);
        bool isTrashSensitive = true;
        for (size_t i = 0; i < selected.size(); i++) {
            if ((static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getTrashed()) {
                isTrashSensitive = false;
                break;
            }
        }

        bool isUntrashSensitive = true;
        for (size_t i = 0; i < selected.size(); i++) {
            if (!(static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getTrashed()) {
                isUntrashSensitive = false;
                break;
            }
        }

        trashAction->set_enabled(isTrashSensitive);
        untrashAction->set_enabled(isUntrashSensitive);
        pasteProfileAction->set_enabled(clipboard.hasProcParams());
        partialPasteProfileAction->set_enabled(clipboard.hasProcParams());
        copyProfileAction->set_enabled(selected.size() == 1);
        clearProfileAction->set_enabled(!selected.empty());
        copyToAction->set_enabled(!selected.empty());
        moveToAction->set_enabled(!selected.empty());
    }
}

void FileBrowser::doubleClicked (ThumbBrowserEntryBase* entry)
{

    if (tbl && entry) {
        std::vector<Thumbnail*> entries;
        entries.push_back ((static_cast<FileBrowserEntry*>(entry))->thumbnail);
        tbl->openRequested (entries);
    }
}

void FileBrowser::addEntry (FileBrowserEntry* entry)
{
    entry->setParent(this);

    const unsigned int sid = session_id();

    idle_register.add(
        [this, entry, sid]() -> bool
        {
            if (sid != session_id()) {
                delete entry;
            } else {
                addEntry_(entry);
            }

            return false;
        }
    );
}

void FileBrowser::addEntry_ (FileBrowserEntry* entry)
{
    entry->selected = false;
    entry->drawable = false;
    entry->framed = editedFiles.find(entry->filename) != editedFiles.end();

//     // add button set to the thumbbrowserentry
//     entry->addButtonSet(new FileThumbnailButtonSet(entry));
//     entry->getThumbButtonSet()->setRank(entry->thumbnail->getRank());
//     entry->getThumbButtonSet()->setColorLabel(entry->thumbnail->getColorLabel());
//     entry->getThumbButtonSet()->setInTrash(entry->thumbnail->getTrashed());
//     entry->getThumbButtonSet()->setButtonListener(this);
    entry->resize(getThumbnailHeight());
    entry->filtered = !checkFilter(entry);
    insertEntry(entry);
}

FileBrowserEntry* FileBrowser::delEntry (const Glib::ustring& fname)
{
    MYWRITERLOCK(l, entryRW);

    for (std::vector<ThumbBrowserEntryBase*>::iterator i = fd.begin(); i != fd.end(); ++i)
        if ((*i)->filename == fname) {
            ThumbBrowserEntryBase* entry = *i;
            entry->selected = false;
            fd.erase (i);
            std::vector<ThumbBrowserEntryBase*>::iterator j = std::find (selected.begin(), selected.end(), entry);

            MYWRITERLOCK_RELEASE(l);

            if (j != selected.end()) {
                if (checkFilter (*j)) {
                    numFiltered--;
                }

                selected.erase (j);
                notifySelectionListener ();
            }

            if (lastClicked == entry) {
                lastClicked = nullptr;
            }

            redraw ();

            return (static_cast<FileBrowserEntry*>(entry));
        }

    return nullptr;
}

void FileBrowser::close ()
{
    ++session_id_;

    {
        MYWRITERLOCK(l, entryRW);

        selected.clear ();
        anchor = nullptr;

        MYWRITERLOCK_RELEASE(l); // notifySelectionListener will need read access!

        notifySelectionListener ();

        MYWRITERLOCK_ACQUIRE(l);

        // The listener merges parameters with old values, so delete afterwards
        for (size_t i = 0; i < fd.size(); i++) {
            delete fd.at(i);
        }

        fd.clear ();
    }

    lastClicked = nullptr;
}

std::vector<FileBrowserEntry*> FileBrowser::getSelected()
{
    std::vector<FileBrowserEntry*> mselected;

    MYREADERLOCK(l, entryRW);

    for (size_t i = 0; i < selected.size(); i++) {
        mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
    }
    return mselected;
}

void FileBrowser::activateProcessFast()
{
    GET_SELECTED_ITEMS();
    if (exportPanel) {
        // force saving export panel settings
        exportPanel->setExportPanelListener(nullptr);
        exportPanel->FastExportPressed();
        exportPanel->setExportPanelListener(this);
    }
    tbl->developRequested(mselected, true);
}

void FileBrowser::activateSelectAll()
{
    lastClicked = nullptr;
    {
        MYWRITERLOCK(l, entryRW);

        selected.clear();

        for (size_t i = 0; i < fd.size(); ++i) {
            if (checkFilter(fd[i])) {
                fd[i]->selected = true;
                selected.push_back(fd[i]);
            }
        }
        if (!anchor && !selected.empty()) {
            anchor = selected[0];
        }
    }
    queue_draw ();
    notifySelectionListener();
}

void FileBrowser::activateRank(int rank)
{
    GET_SELECTED_ITEMS();
    rankingRequested(mselected, rank);
}

void FileBrowser::activateColorLabel(int color)
{
    GET_SELECTED_ITEMS();
    colorlabelRequested(mselected, color);
}

void FileBrowser::activateActionDataColorLabel(int color)
{
    if (!colorLabel_actionData) return;
    std::vector<FileBrowserEntry*> tbe = {static_cast<FileBrowserEntry*>(colorLabel_actionData)};
    colorlabelRequested(tbe, color);
}

void FileBrowser::activateResetDefaultProfile()
{
    GET_SELECTED_ITEMS();
    if (!mselected.empty() && bppcl) {
        bppcl->beginBatchPParamsChange(mselected.size());
    }

    for (size_t i = 0; i < mselected.size(); i++)  {
        const auto thumbnail = mselected[i]->thumbnail;
        const auto rank = thumbnail->getRank();
        const auto colorLabel = thumbnail->getColorLabel();
        const auto stage = thumbnail->getTrashed();

        thumbnail->createProcParamsForUpdate (false, true);
        thumbnail->setRank(rank);
        thumbnail->setColorLabel(colorLabel);
        thumbnail->setTrashed(stage);

        // Empty run to update the thumb
        rtengine::procparams::ProcParams params = thumbnail->getProcParams ();
        thumbnail->setProcParams (params, nullptr, FILEBROWSER, true, true);
    }

    if (!mselected.empty() && bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::activateExternalProgram(const Glib::ustring& name)
{
    GET_SELECTED_ITEMS();

    const auto pAct = mMenuExtProgs[name];

    // Build vector of all file names
    std::vector<Glib::ustring> selFileNames;

    for (size_t i = 0; i < mselected.size(); i++) {
        Glib::ustring fn = mselected[i]->thumbnail->getFileName();

        // Maybe batch processed version
        if (pAct->target == 2) {
            fn = Glib::ustring::compose("%1.%2", Thumbnail::calcAutoFileNameBase(fn),
                                        options.saveFormatBatch.format);
        }

        selFileNames.push_back(fn);
    }

    pAct->execute(selFileNames);
}

void FileBrowser::activateSelectDarkFrame()
{
    GET_SELECTED_ITEMS();

    const rtengine::procparams::ProcParams& pp = mselected[0]->thumbnail->getProcParams();

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title(M("TP_DARKFRAME_LABEL"));
    dialog->set_modal();
    if (!options.lastDarkframeDir.empty()) {
        dialog->set_initial_folder(Gio::File::create_for_path(options.lastDarkframeDir));
    }
    if(!pp.raw.dark_frame.empty()) {
        dialog->set_initial_file(Gio::File::create_for_path(pp.raw.dark_frame));
    }

    auto onResponse = [&, dialog, sel=std::move(mselected)](Glib::RefPtr<Gio::AsyncResult>& result) {
        static_assert(!std::is_reference_v<decltype(sel)>);

        Glib::RefPtr<Gio::File> file;
        try {
            file = dialog->open_finish(result);
            if (!file) return;
        } catch (const Glib::Error& err) {
            return;
        }

        auto folder = file->get_parent();
        if (folder) {
            options.lastDarkframeDir = folder->get_path();
        }

        if (bppcl) {
            bppcl->beginBatchPParamsChange(sel.size());
        }

        for (size_t i = 0; i < sel.size(); i++) {
            rtengine::procparams::ProcParams lpp = sel[i]->thumbnail->getProcParams();
            lpp.raw.dark_frame = file->get_path();
            lpp.raw.df_autoselect = false;
            sel[i]->thumbnail->setProcParams(lpp, nullptr, FILEBROWSER, false);
        }

        if (bppcl) {
            bppcl->endBatchPParamsChange();
        }
    };

    dialog->set_accept_label(M("GENERAL_APPLY"));
    dialog->open(*getToplevelWindow(this), onResponse);
}

void FileBrowser::activateAutoDarkFrame()
{
    GET_SELECTED_ITEMS();

    if (bppcl) {
        bppcl->beginBatchPParamsChange(mselected.size());
    }

    for (size_t i = 0; i < mselected.size(); i++) {
        rtengine::procparams::ProcParams pp = mselected[i]->thumbnail->getProcParams();
        pp.raw.df_autoselect = true;
        pp.raw.dark_frame.clear();
        mselected[i]->thumbnail->setProcParams(pp, nullptr, FILEBROWSER, false);
    }

    if (bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::activateMoveToDarkFrameDir()
{
    GET_SELECTED_ITEMS();

    if (!options.rtSettings.darkFramesPath.empty()) {
        if (Gio::File::create_for_path(options.rtSettings.darkFramesPath)->query_exists()) {
            for (size_t i = 0; i < mselected.size(); i++) {
                Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(mselected[i]->filename);

                if (!file) {
                    continue;
                }

                Glib::ustring destName = options.rtSettings.darkFramesPath + "/" + file->get_basename();
                Glib::RefPtr<Gio::File> dest = Gio::File::create_for_path(destName);
                file->move(dest);
            }

            // Reinit cache
            rtengine::DFManager::getInstance().init(options.rtSettings.darkFramesPath);
        } else {
            // Target directory creation failed, we clear the darkFramesPath setting
            options.rtSettings.darkFramesPath.clear();
            Glib::ustring msg_ = Glib::ustring::compose(
                M("MAIN_MSG_PATHDOESNTEXIST"),
                escapeHtmlChars(options.rtSettings.darkFramesPath));
            msg_ += "\n\n";
            msg_ += M("MAIN_MSG_OPERATIONCANCELLED");

            auto msgd = Gtk::make_managed<RtMessageDialog>(msg_,
                RtMessageDialog::Type::ERROR,
                RtMessageDialog::ButtonSet::OK);
            msgd->set_title(M("TP_DARKFRAME_LABEL"));
            msgd->show();
        }
    } else {
        auto msgd = Gtk::make_managed<RtMessageDialog>(
            M("MAIN_MSG_SETPATHFIRST") + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED"),
            RtMessageDialog::Type::ERROR,
            RtMessageDialog::ButtonSet::OK);
        msgd->set_title(M("TP_DARKFRAME_LABEL"));
        msgd->show();
    }
}

void FileBrowser::activateSelectFlatField()
{
    GET_SELECTED_ITEMS();

    const rtengine::procparams::ProcParams& pp = mselected[0]->thumbnail->getProcParams();

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title(M("TP_FLATFIELD_LABEL"));
    dialog->set_modal();
    if (!options.lastFlatfieldDir.empty()) {
        dialog->set_initial_folder(Gio::File::create_for_path(options.lastFlatfieldDir));
    }
    if(!pp.raw.ff_file.empty()) {
        dialog->set_initial_file(Gio::File::create_for_path(pp.raw.ff_file));
    }

    auto onResponse = [&, dialog, sel=std::move(mselected)](Glib::RefPtr<Gio::AsyncResult>& result) {
        static_assert(!std::is_reference_v<decltype(sel)>);

        Glib::RefPtr<Gio::File> file;
        try {
            file = dialog->open_finish(result);
            if (!file) return;
        } catch (const Glib::Error& err) {
            return;
        }

        auto folder = file->get_parent();
        if (folder) {
            options.lastFlatfieldDir = folder->get_path();
        }

        if (bppcl) {
            bppcl->beginBatchPParamsChange(sel.size());
        }

        for (size_t i = 0; i < sel.size(); i++) {
            rtengine::procparams::ProcParams lpp = sel[i]->thumbnail->getProcParams();
            lpp.raw.ff_file = file->get_path();
            lpp.raw.ff_AutoSelect = false;
            sel[i]->thumbnail->setProcParams(lpp, nullptr, FILEBROWSER, false);
        }

        if (bppcl) {
            bppcl->endBatchPParamsChange();
        }
    };

    dialog->set_accept_label(M("GENERAL_APPLY"));
    dialog->open(*getToplevelWindow(this), onResponse);
}

void FileBrowser::activateAutoFlatField()
{
    GET_SELECTED_ITEMS();

    if (bppcl) {
        bppcl->beginBatchPParamsChange(mselected.size());
    }

    for (size_t i = 0; i < mselected.size(); i++) {
        rtengine::procparams::ProcParams pp = mselected[i]->thumbnail->getProcParams();
        pp.raw.ff_AutoSelect = true;
        pp.raw.ff_file.clear();
        mselected[i]->thumbnail->setProcParams(pp, nullptr, FILEBROWSER, false);
    }

    if (bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::activateMoveToFlatFieldDir()
{
    GET_SELECTED_ITEMS();

    if (!options.rtSettings.flatFieldsPath.empty()) {
        if (Gio::File::create_for_path(options.rtSettings.flatFieldsPath)->query_exists()) {
            for (size_t i = 0; i < mselected.size(); i++) {
                Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(mselected[i]->filename);

                if (!file) {
                    continue;
                }

                Glib::ustring destName = options.rtSettings.flatFieldsPath + "/" + file->get_basename();
                Glib::RefPtr<Gio::File> dest = Gio::File::create_for_path(destName);
                file->move(dest);
            }

            // Reinit cache
            rtengine::ffm.init(options.rtSettings.flatFieldsPath);
        } else {
            // Target directory creation failed, we clear the flatFieldsPath setting
            options.rtSettings.flatFieldsPath.clear();
            Glib::ustring msg_ = Glib::ustring::compose(
                M("MAIN_MSG_PATHDOESNTEXIST"), escapeHtmlChars(options.rtSettings.flatFieldsPath));
            msg_ += "\n\n";
            msg_ += M("MAIN_MSG_OPERATIONCANCELLED");

            auto msgd = Gtk::make_managed<RtMessageDialog>(msg_,
                RtMessageDialog::Type::ERROR,
                RtMessageDialog::ButtonSet::OK);
            msgd->set_title(M("TP_DARKFRAME_LABEL"));
            msgd->show();
        }
    } else {
        auto msgd = Gtk::make_managed<RtMessageDialog>(
            M("MAIN_MSG_SETPATHFIRST") + "\n\n" + M("MAIN_MSG_OPERATIONCANCELLED"),
            RtMessageDialog::Type::ERROR,
            RtMessageDialog::ButtonSet::OK);
        msgd->set_title(M("TP_FLATFIELD_LABEL"));
        msgd->show();
    }
}

void FileBrowser::copyProfile ()
{
    MYREADERLOCK(l, entryRW);

    if (selected.size() == 1) {
        clipboard.setProcParams ((static_cast<FileBrowserEntry*>(selected[0]))->thumbnail->getProcParams());
    }
}

void FileBrowser::pasteProfile ()
{

    if (clipboard.hasProcParams()) {
        std::vector<FileBrowserEntry*> mselected;
        {
            MYREADERLOCK(l, entryRW);

            for (unsigned int i = 0; i < selected.size(); i++) {
                mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
            }
        }

        if (!tbl || mselected.empty()) {
            return;
        }

        if (!mselected.empty() && bppcl) {
            bppcl->beginBatchPParamsChange(mselected.size());
        }

        for (unsigned int i = 0; i < mselected.size(); i++) {
            // copying read only clipboard PartialProfile to a temporary one
            const rtengine::procparams::PartialProfile& cbPartProf = clipboard.getPartialProfile();
            rtengine::procparams::PartialProfile pastedPartProf(cbPartProf.pparams, cbPartProf.pedited, true);

            // applying the PartialProfile to the thumb's ProcParams
            mselected[i]->thumbnail->setProcParams (*pastedPartProf.pparams, pastedPartProf.pedited, FILEBROWSER);
            pastedPartProf.deleteInstance();
        }

        if (!mselected.empty() && bppcl) {
            bppcl->endBatchPParamsChange();
        }

        queue_draw ();
    }
}

void FileBrowser::partPasteProfile ()
{

    if (clipboard.hasProcParams()) {

        std::vector<FileBrowserEntry*> mselected;
        {
            MYREADERLOCK(l, entryRW);

            for (unsigned int i = 0; i < selected.size(); i++) {
                mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
            }
        }

        if (!tbl || mselected.empty()) {
            return;
        }

        auto toplevel = getToplevelWindow(this);
        auto partialPasteDlg = Gtk::make_managed<PartialPasteDlg>(
            M("PARTIALPASTE_DIALOGLABEL"), toplevel);

        partialPasteDlg->updateSpotWidget(clipboard.getPartialProfile().pparams);
        partialPasteDlg->signal_response().connect([&, partialPasteDlg, sel=std::move(mselected)](int response) {
            static_assert(!std::is_reference_v<decltype(sel)>);

            partialPasteDlg->destroy();
            if (response != Gtk::ResponseType::OK) return;

            if (!mselected.empty() && bppcl) {
                bppcl->beginBatchPParamsChange(mselected.size());
            }

            for (auto entry : mselected) {
                // copying read only clipboard PartialProfile to a temporary one, initialized to the thumb's ProcParams
                entry->thumbnail->createProcParamsForUpdate(false, false); // this can execute customprofilebuilder to generate param file
                const rtengine::procparams::PartialProfile& cbPartProf = clipboard.getPartialProfile();
                rtengine::procparams::PartialProfile pastedPartProf(&entry->thumbnail->getProcParams (), nullptr);

                // pushing the selected values of the clipboard PartialProfile to the temporary PartialProfile
                partialPasteDlg->applyPaste (pastedPartProf.pparams, pastedPartProf.pedited, cbPartProf.pparams, cbPartProf.pedited);

                // applying the temporary PartialProfile to the thumb's ProcParams
                entry->thumbnail->setProcParams (*pastedPartProf.pparams, pastedPartProf.pedited, FILEBROWSER);
                pastedPartProf.deleteInstance();
            }

            if (!mselected.empty() && bppcl) {
                bppcl->endBatchPParamsChange();
            }

            queue_draw ();
        });
        partialPasteDlg->show();
    }
}

#ifdef _WIN32
void FileBrowser::openDefaultViewer (int destination)
{
    bool success = true;

    {
        MYREADERLOCK(l, entryRW);

        if (selected.size() == 1) {
            success = (static_cast<FileBrowserEntry*>(selected[0]))->thumbnail->openDefaultViewer(destination);
        }
    }

    if (!success) {
        auto msgd = Gtk::make_managed<RtMessageDialog>(
            M("MAIN_MSG_IMAGEUNPROCESSED"),
            RtMessageDialog::Type::ERROR,
            RtMessageDialog::ButtonSet::OK);
        msgd->show(getToplevelWindow(this));
    }
}
#endif

bool FileBrowser::keyPressed (guint keyval, guint keycode, Gdk::ModifierType state)
{
    bool ctrl = isControlOrMetaDown(state);
    bool shift = isShiftDown(state);
    bool alt = isAltDown(state);

    if ((keyval == GDK_KEY_C || keyval == GDK_KEY_c) && ctrl && shift) {
        copyToAction->activate();
        return true;
    } else if ((keyval == GDK_KEY_M || keyval == GDK_KEY_m) && ctrl && shift) {
        moveToAction->activate();
        return true;
    } else if ((keyval == GDK_KEY_C || keyval == GDK_KEY_c || keyval == GDK_KEY_Insert) && ctrl) {
        copyProfile ();
        return true;
    } else if ((keyval == GDK_KEY_V || keyval == GDK_KEY_v) && ctrl && !shift) {
        pasteProfile ();
        return true;
    } else if (keyval == GDK_KEY_Insert && shift) {
        pasteProfile ();
        return true;
    } else if ((keyval == GDK_KEY_V || keyval == GDK_KEY_v) && ctrl && shift) {
        partPasteProfile ();
        return true;
    } else if (keyval == GDK_KEY_Delete && !shift) {
        trashAction->activate();
        return true;
    } else if (keyval == GDK_KEY_Delete && shift) {
        untrashAction->activate();
        return true;
    } else if ((keyval == GDK_KEY_B || keyval == GDK_KEY_b) && ctrl && !shift) {
        pmenuActions->activate_action(PROCESS_ACTION_NAME);
        return true;
    } else if ((keyval == GDK_KEY_B || keyval == GDK_KEY_b) && ctrl && shift) {
        activateProcessFast();
        return true;
    } else if ((keyval == GDK_KEY_A || keyval == GDK_KEY_a) && ctrl) {
        pmenuActions->activate_action(SELECT_ALL_ACTION_NAME);
        return true;
    } else if (keyval == GDK_KEY_F2 && !ctrl) {
        pmenuActions->activate_action(RENAME_ACTION_NAME);
        return true;
    } else if (keyval == GDK_KEY_F3 && !(ctrl || shift || alt)) { // open Previous image from FileBrowser perspective
        FileBrowser::openPrevImage ();
        return true;
    } else if (keyval == GDK_KEY_F4 && !(ctrl || shift || alt)) { // open Next image from FileBrowser perspective
        FileBrowser::openNextImage ();
        return true;
    } else if (keyval == GDK_KEY_Left) {
        selectPrev (1, shift);
        return true;
    } else if (keyval == GDK_KEY_Right) {
        selectNext (1, shift);
        return true;
    } else if (keyval == GDK_KEY_Up) {
        selectPrev (numOfCols, shift);
        return true;
    } else if (keyval == GDK_KEY_Down) {
        selectNext (numOfCols, shift);
        return true;
    } else if (keyval == GDK_KEY_Home) {
        selectFirst (shift);
        return true;
    } else if (keyval == GDK_KEY_End) {
        selectLast (shift);
        return true;
    } else if(keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        std::vector<FileBrowserEntry*> mselected;

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }

        openRequested(mselected);
#ifdef _WIN32
    } else if (keyval == GDK_KEY_F5) {
        int dest = 1;

        if (shift) {
            dest = 2;
        } else if (ctrl) {
            dest = 3;
        }

        openDefaultViewer (dest);
        return true;
#endif
    } else if (keyval == GDK_KEY_Page_Up) {
        scrollPage(GDK_SCROLL_UP);
        return true;
    } else if (keyval == GDK_KEY_Page_Down) {
        scrollPage(GDK_SCROLL_DOWN);
        return true;
    }

#ifdef __WIN32__
    else if (!shift && !ctrl && !alt) { // rank
        switch(keycode) {
        case 0x30:  // 0-key
            requestRanking (0);
            return true;

        case 0x31:  // 1-key
            requestRanking (1);
            return true;

        case 0x32:  // 2-key
            requestRanking (2);
            return true;

        case 0x33:  // 3-key
            requestRanking (3);
            return true;

        case 0x34:  // 4-key
            requestRanking (4);
            return true;

        case 0x35:  // 5-key
            requestRanking (5);
            return true;
        }
    } else if (shift && ctrl && !alt) { // color labels
        switch(keycode) {
        case 0x30:  // 0-key
            requestColorLabel (0);
            return true;

        case 0x31:  // 1-key
            requestColorLabel (1);
            return true;

        case 0x32:  // 2-key
            requestColorLabel (2);
            return true;

        case 0x33:  // 3-key
            requestColorLabel (3);
            return true;

        case 0x34:  // 4-key
            requestColorLabel (4);
            return true;

        case 0x35:  // 5-key
            requestColorLabel (5);
            return true;
        }
    }

#else
    else if (!shift && !ctrl && !alt) { // rank
        switch(keycode) {
        case 0x13:
            requestRanking (0);
            return true;

        case 0x0a:
            requestRanking (1);
            return true;

        case 0x0b:
            requestRanking (2);
            return true;

        case 0x0c:
            requestRanking (3);
            return true;

        case 0x0d:
            requestRanking (4);
            return true;

        case 0x0e:
            requestRanking (5);
            return true;
        }
    } else if (shift && ctrl && !alt) { // color labels
        switch(keycode) {
        case 0x13:
            requestColorLabel (0);
            return true;

        case 0x0a:
            requestColorLabel (1);
            return true;

        case 0x0b:
            requestColorLabel (2);
            return true;

        case 0x0c:
            requestColorLabel (3);
            return true;

        case 0x0d:
            requestColorLabel (4);
            return true;

        case 0x0e:
            requestColorLabel (5);
            return true;
        }
    }
#endif

    return false;
}

void FileBrowser::saveThumbnailHeight (int height)
{
    if (!options.sameThumbSize && getLocation() == THLOC_EDITOR) {
        options.thumbSizeTab = height;
    } else {
        options.thumbSize = height;
    }
}

int FileBrowser::getThumbnailHeight ()
{
    // The user could have manually forced the option to a too big value
    if (!options.sameThumbSize && getLocation() == THLOC_EDITOR) {
        return std::max(std::min(options.thumbSizeTab, 800), 10);
    } else {
        return std::max(std::min(options.thumbSize, 800), 10);
    }
}

void FileBrowser::enableTabMode(bool enable)
{
    ThumbBrowserBase::enableTabMode(enable);
    if (options.inspectorWindow) {
        if (enable) {
            pmenuShortcutController->add_shortcut(inspectShortcut);
        }
        else {
            pmenuShortcutController->remove_shortcut(inspectShortcut);
        }
    }
}

void FileBrowser::activateApplyProfile(size_t index)
{
    MYREADERLOCK(l, entryRW);

    const ProfileStoreEntry* entry = [&]() {
        const std::vector<const ProfileStoreEntry*>* entries =
            ProfileStore::getInstance()->getFileList();
        auto entry = entries->at(index);
        ProfileStore::getInstance()->releaseFileList();
        return entry;
    }();

    const rtengine::procparams::PartialProfile* partProfile =
        ProfileStore::getInstance()->getProfile(entry);

    if (partProfile->pparams && !selected.empty()) {
        if (bppcl) {
            bppcl->beginBatchPParamsChange(selected.size());
        }

        for (size_t i = 0; i < selected.size(); i++) {
            (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->setProcParams (*partProfile->pparams, partProfile->pedited, FILEBROWSER);
        }

        if (bppcl) {
            bppcl->endBatchPParamsChange();
        }

        queue_draw ();
    }
}

void FileBrowser::activateApplyPartialProfile(size_t index)
{

    {
        MYREADERLOCK(l, entryRW);

        if (!tbl || selected.empty()) {
            return;
        }
    }

    const ProfileStoreEntry* entry = [&]() {
        const std::vector<const ProfileStoreEntry*>* entries =
            ProfileStore::getInstance()->getFileList();
        auto entry = entries->at(index);
        ProfileStore::getInstance()->releaseFileList();
        return entry;
    }();

    const rtengine::procparams::PartialProfile* srcProfiles =
        ProfileStore::getInstance()->getProfile(entry);

    if (srcProfiles->pparams) {
        auto toplevel = getToplevelWindow(this);
        auto partialPasteDlg = Gtk::make_managed<PartialPasteDlg>(
            M("PARTIALPASTE_DIALOGLABEL"), toplevel);
        partialPasteDlg->updateSpotWidget(srcProfiles->pparams);

        partialPasteDlg->signal_response().connect([&, partialPasteDlg, srcProfiles](int response) {
            partialPasteDlg->destroy();
            if (response != Gtk::ResponseType::OK) return;

            MYREADERLOCK(l, entryRW);

            if (bppcl) {
                bppcl->beginBatchPParamsChange(selected.size());
            }

            for (size_t i = 0; i < selected.size(); i++) {
                selected[i]->thumbnail->createProcParamsForUpdate(false, false);  // this can execute customprofilebuilder to generate param file

                rtengine::procparams::PartialProfile dstProfile(true);
                *dstProfile.pparams = (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->getProcParams ();
                dstProfile.set(true);
                dstProfile.pedited->locallab.spots.resize(dstProfile.pparams->locallab.spots.size(), LocallabParamsEdited::LocallabSpotEdited(true));
                partialPasteDlg->applyPaste (dstProfile.pparams, dstProfile.pedited, srcProfiles->pparams, srcProfiles->pedited);
                (static_cast<FileBrowserEntry*>(selected[i]))->thumbnail->setProcParams (*dstProfile.pparams, dstProfile.pedited, FILEBROWSER);
                dstProfile.deleteInstance();
            }

            if (bppcl) {
                bppcl->endBatchPParamsChange();
            }

            redraw();
        });

        partialPasteDlg->show();
    }
}

void FileBrowser::applyFilter (const BrowserFilter& filter)
{

    this->filter = filter;

    // remove items not complying the filter from the selection
    bool selchanged = false;
    numFiltered = 0;
    {
        MYWRITERLOCK(l, entryRW);

        if (filter.showOriginal) {
            findOriginalEntries(fd);
        }

        for (size_t i = 0; i < fd.size(); i++) {
            if (checkFilter(fd[i])) {
                numFiltered++;
            } else if (fd[i]->selected) {
                fd[i]->selected = false;
                std::vector<ThumbBrowserEntryBase*>::iterator j = std::find(selected.begin(), selected.end(), fd[i]);
                selected.erase(j);

                if (lastClicked == fd[i]) {
                    lastClicked = nullptr;
                }

                selchanged = true;
            }
        }
        if (selected.empty() || (anchor && std::find(selected.begin(), selected.end(), anchor) == selected.end())) {
            anchor = nullptr;
        }
    }

    if (selchanged) {
        notifySelectionListener ();
    }

    tbl->filterApplied();
    redraw ();
}

bool FileBrowser::checkFilter (ThumbBrowserEntryBase* entryb) const   // true -> entry complies filter
{

    FileBrowserEntry* entry = static_cast<FileBrowserEntry*>(entryb);

    if (filter.showOriginal && entry->getOriginal()) {
        return false;
    }

    // return false if basic filter settings are not satisfied
    if ((!filter.showRanked[entry->thumbnail->getRank()] ) ||
            (!filter.showCLabeled[entry->thumbnail->getColorLabel()] ) ||

            ((entry->thumbnail->hasProcParams() && filter.showEdited[0]) && !filter.showEdited[1]) ||
            ((!entry->thumbnail->hasProcParams() && filter.showEdited[1]) && !filter.showEdited[0]) ||

            ((entry->thumbnail->isRecentlySaved() && filter.showRecentlySaved[0]) && !filter.showRecentlySaved[1]) ||
            ((!entry->thumbnail->isRecentlySaved() && filter.showRecentlySaved[1]) && !filter.showRecentlySaved[0]) ||

            (entry->thumbnail->getTrashed() && !filter.showTrash) ||
            (!entry->thumbnail->getTrashed() && !filter.showNotTrash)) {
        return false;
    }

    // return false if query is not satisfied
    if (!filter.vFilterStrings.empty()) {
        // check if image's FileName contains queryFileName (case insensitive)
        // TODO should we provide case-sensitive search option via preferences?
        std::string FileName = Glib::path_get_basename(entry->thumbnail->getFileName().c_str());
        std::transform(FileName.begin(), FileName.end(), FileName.begin(), ::toupper);
        int iFilenameMatch = 0;

        for (const auto& filterString : filter.vFilterStrings) {
            if (FileName.find(filterString) != std::string::npos) {
                ++iFilenameMatch;
                break;
            }
        }

        if (filter.matchEqual) {
            if (iFilenameMatch == 0) { //none of the vFilterStrings found in FileName
                return false;
            }
        } else {
            if (iFilenameMatch > 0) { // match is found for at least one of vFilterStrings in FileName
                return false;
            }
        }
    }

    if (!filter.exifFilterEnabled) {
        return true;
    }

    // check exif filter
    const CacheImageData* cfs = entry->thumbnail->getCacheImageData();
    double tol = 0.01;
    double tol2 = 1e-8;

    if (!cfs->exifValid) {
        return (!filter.exifFilter.filterCamera || filter.exifFilter.cameras.count(cfs->getCamera()) > 0)
               && (!filter.exifFilter.filterLens || filter.exifFilter.lenses.count(cfs->lens) > 0)
               && (!filter.exifFilter.filterFiletype || filter.exifFilter.filetypes.count(cfs->filetype) > 0)
               && (!filter.exifFilter.filterExpComp || filter.exifFilter.expcomp.count(cfs->expcomp) > 0);
    }

    return
        (!filter.exifFilter.filterShutter || (rtengine::FramesMetaData::shutterFromString(rtengine::FramesMetaData::shutterToString(cfs->shutter)) >= filter.exifFilter.shutterFrom - tol2 && rtengine::FramesMetaData::shutterFromString(rtengine::FramesMetaData::shutterToString(cfs->shutter)) <= filter.exifFilter.shutterTo + tol2))
        && (!filter.exifFilter.filterFNumber || (rtengine::FramesMetaData::apertureFromString(rtengine::FramesMetaData::apertureToString(cfs->fnumber)) >= filter.exifFilter.fnumberFrom - tol2 && rtengine::FramesMetaData::apertureFromString(rtengine::FramesMetaData::apertureToString(cfs->fnumber)) <= filter.exifFilter.fnumberTo + tol2))
        && (!filter.exifFilter.filterFocalLen || (cfs->focalLen >= filter.exifFilter.focalFrom - tol && cfs->focalLen <= filter.exifFilter.focalTo + tol))
        && (!filter.exifFilter.filterISO     || (cfs->iso >= filter.exifFilter.isoFrom && cfs->iso <= filter.exifFilter.isoTo))
        && (!filter.exifFilter.filterExpComp || filter.exifFilter.expcomp.count(cfs->expcomp) > 0)
        && (!filter.exifFilter.filterCamera  || filter.exifFilter.cameras.count(cfs->getCamera()) > 0)
        && (!filter.exifFilter.filterLens    || filter.exifFilter.lenses.count(cfs->lens) > 0)
        && (!filter.exifFilter.filterFiletype  || filter.exifFilter.filetypes.count(cfs->filetype) > 0);
}

void FileBrowser::toTrashRequested (std::vector<FileBrowserEntry*> tbe)
{

    for (size_t i = 0; i < tbe.size(); i++) {
        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // no need to notify listeners as item goes to trash, likely to be deleted

        if (tbe[i]->thumbnail->getTrashed()) {
            continue;
        }

        tbe[i]->thumbnail->setTrashed (true);

//         if (tbe[i]->getThumbButtonSet()) {
//             tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
//             tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
//             tbe[i]->getThumbButtonSet()->setInTrash (true);
//             tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
//         }
    }

    trash_changed().emit();
    applyFilter (filter);
}

void FileBrowser::fromTrashRequested (std::vector<FileBrowserEntry*> tbe)
{

    for (size_t i = 0; i < tbe.size(); i++) {
        // if thumbnail was marked inTrash=true then param file must be there, no need to run customprofilebuilder

        if (!tbe[i]->thumbnail->getTrashed()) {
            continue;
        }

        tbe[i]->thumbnail->setTrashed (false);

//         if (tbe[i]->getThumbButtonSet()) {
//             tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
//             tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
//             tbe[i]->getThumbButtonSet()->setInTrash (false);
//             tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
//         }
    }

    trash_changed().emit();
    applyFilter (filter);
}

void FileBrowser::sortMethodRequested (int method)
{
    options.sortMethod = Options::SortMethod(method);
    resort ();
}

void FileBrowser::sortOrderRequested (int order)
{
    options.sortDescending = !!order;
    resort ();
}

void FileBrowser::rankingRequested (std::vector<FileBrowserEntry*> tbe, int rank)
{

    if (!tbe.empty() && bppcl) {
        bppcl->beginBatchPParamsChange(tbe.size());
    }

    for (size_t i = 0; i < tbe.size(); i++) {

        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // notify listeners TODO: should do this ONLY when params changed by customprofilebuilder?
        tbe[i]->thumbnail->notifylisterners_procParamsChanged(FILEBROWSER);

        tbe[i]->thumbnail->setRank (rank);
        tbe[i]->thumbnail->updateCache (); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file
        //TODO? - should update pparams instead?

//         if (tbe[i]->getThumbButtonSet()) {
//             tbe[i]->getThumbButtonSet()->setRank (tbe[i]->thumbnail->getRank());
//         }
    }

    applyFilter (filter);

    if (!tbe.empty() && bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::colorlabelRequested (std::vector<FileBrowserEntry*> tbe, int colorlabel)
{

    if (!tbe.empty() && bppcl) {
        bppcl->beginBatchPParamsChange(tbe.size());
    }

    for (size_t i = 0; i < tbe.size(); i++) {
        // try to load the last saved parameters from the cache or from the paramfile file
        tbe[i]->thumbnail->createProcParamsForUpdate(false, false, true);  // this can execute customprofilebuilder to generate param file in "flagging" mode

        // notify listeners TODO: should do this ONLY when params changed by customprofilebuilder?
        tbe[i]->thumbnail->notifylisterners_procParamsChanged(FILEBROWSER);

        tbe[i]->thumbnail->setColorLabel (colorlabel);
        tbe[i]->thumbnail->updateCache(); // needed to save the colorlabel to disk in the procparam file(s) and the cache image data file

        //TODO? - should update pparams instead?
//         if (tbe[i]->getThumbButtonSet()) {
//             tbe[i]->getThumbButtonSet()->setColorLabel (tbe[i]->thumbnail->getColorLabel());
//         }
    }

    applyFilter (filter);

    if (!tbe.empty() && bppcl) {
        bppcl->endBatchPParamsChange();
    }
}

void FileBrowser::requestRanking(int rank)
{
    std::vector<FileBrowserEntry*> mselected;
    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }
    }

    rankingRequested (mselected, rank);
}

void FileBrowser::requestColorLabel(int colorlabel)
{
    std::vector<FileBrowserEntry*> mselected;
    {
        MYREADERLOCK(l, entryRW);

        for (size_t i = 0; i < selected.size(); i++) {
            mselected.push_back (static_cast<FileBrowserEntry*>(selected[i]));
        }
    }

    colorlabelRequested (mselected, colorlabel);
}

// void FileBrowser::buttonPressed (LWButton* button, int actionCode, void* actionData)
// {
//
//     if (actionCode >= 0 && actionCode <= 5) { // rank
//         std::vector<FileBrowserEntry*> tbe;
//         tbe.push_back (static_cast<FileBrowserEntry*>(actionData));
//         rankingRequested (tbe, actionCode);
//     } else if (actionCode == 6 && tbl) { // to processing queue
//         std::vector<FileBrowserEntry*> tbe;
//         tbe.push_back (static_cast<FileBrowserEntry*>(actionData));
//         tbl->developRequested (tbe, false); // not a fast, but a FULL mode
//     } else if (actionCode == 7) { // to trash / undelete
//         std::vector<FileBrowserEntry*> tbe;
//         FileBrowserEntry* entry = static_cast<FileBrowserEntry*>(actionData);
//         tbe.push_back (entry);
//
//         if (!entry->thumbnail->getTrashed()) {
//             toTrashRequested (tbe);
//         } else {
//             fromTrashRequested (tbe);
//         }
//     } else if (actionCode == 8 && tbl) { // color label
//         // show popup menu
//         colorLabel_actionData = actionData;// this will be reused when pmenuColorLabels is clicked
//         pmenuColorLabels->popup (3, this->eventTime);
//     }
// }

void FileBrowser::openNextImage()
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && selected.size() > 0 && !options.tabbedUI) {
        for (size_t i = 0; i < fd.size() - 1; i++) {
            if (selected[0]->thumbnail->getFileName() == fd[i]->filename) { // located 1-st image in current selection
                if (i < fd.size() && tbl) {
                    // find the first not-filtered-out (next) image
                    for (size_t k = i + 1; k < fd.size(); k++) {
                        if (!fd[k]->filtered/*checkFilter (fd[k])*/) {

                            // clear current selection
                            for (size_t j = 0; j < selected.size(); j++) {
                                selected[j]->selected = false;
                            }

                            selected.clear();

                            // set new selection
                            fd[k]->selected = true;
                            selected.push_back(fd[k]);
                            //queue_draw();

                            MYWRITERLOCK_RELEASE(l);

                            // this will require a read access
                            notifySelectionListener();

                            MYWRITERLOCK_ACQUIRE(l);

                            // scroll to the selected position, centered horizontally in the container
                            double x1, y1;
                            getScrollPosition(x1, y1);

                            double x2 = selected[0]->getStartX();
                            double y2 = selected[0]->getStartY();

                            Thumbnail* thumb = (static_cast<FileBrowserEntry*>(fd[k]))->thumbnail;
                            int tw = fd[k]->getMinimalWidth(); // thumb width

                            int ww = get_width(); // window width

                            MYWRITERLOCK_RELEASE(l);

                            // scroll only when selected[0] is outside of the displayed bounds
                            // or less than a thumbnail's width from either edge.
                            if ((x2 > x1 + ww - 1.5 * tw) || (x2 - tw / 2 < x1)) {
                                setScrollPosition(x2 - (ww - tw) / 2, y2);
                            }

                            // open the selected image
                            tbl->openRequested({thumb});

                            return;
                        }
                    }
                }
            }
        }
    }
}

void FileBrowser::openPrevImage()
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && selected.size() > 0 && !options.tabbedUI) {
        for (size_t i = 1; i < fd.size(); i++) {
            if (selected[0]->thumbnail->getFileName() == fd[i]->filename) { // located 1-st image in current selection
                if (i > 0 && tbl) {
                    // find the first not-filtered-out (previous) image
                    for (ssize_t k = (ssize_t)i - 1; k >= 0; k--) {
                        if (!fd[k]->filtered/*checkFilter (fd[k])*/) {

                            // clear current selection
                            for (size_t j = 0; j < selected.size(); j++) {
                                selected[j]->selected = false;
                            }

                            selected.clear();

                            // set new selection
                            fd[k]->selected = true;
                            selected.push_back(fd[k]);
                            //queue_draw();

                            MYWRITERLOCK_RELEASE(l);

                            // this will require a read access
                            notifySelectionListener();

                            MYWRITERLOCK_ACQUIRE(l);

                            // scroll to the selected position, centered horizontally in the container
                            double x1, y1;
                            getScrollPosition(x1, y1);

                            double x2 = selected[0]->getStartX();
                            double y2 = selected[0]->getStartY();

                            Thumbnail* thumb = (static_cast<FileBrowserEntry*>(fd[k]))->thumbnail;
                            int tw = fd[k]->getMinimalWidth(); // thumb width

                            int ww = get_width(); // window width

                            MYWRITERLOCK_RELEASE(l);

                            // scroll only when selected[0] is outside of the displayed bounds
                            // or less than a thumbnail's width from either edge.
                            if ((x2 > x1 + ww - 1.5 * tw) || (x2 - tw / 2 < x1)) {
                                setScrollPosition(x2 - (ww - tw) / 2, y2);
                            }

                            // open the selected image
                            tbl->openRequested({thumb});

                            return;
                        }
                    }
                }
            }
        }
    }
}

void FileBrowser::selectImage(const Glib::ustring& fname, bool doScroll)
{
    MYWRITERLOCK(l, entryRW);

    if (!fd.empty() && !options.tabbedUI) {
        for (size_t i = 0; i < fd.size(); i++) {
            if (fname == fd[i]->filename && !fd[i]->filtered) {
                // matching file found for sync

                // clear current selection
                for (size_t j = 0; j < selected.size(); j++) {
                    selected[j]->selected = false;
                }

                selected.clear();

                // set new selection
                fd[i]->selected = true;
                selected.push_back(fd[i]);
                queue_draw();

                MYWRITERLOCK_RELEASE(l);

                // this will require a read access
                notifySelectionListener();

                MYWRITERLOCK_ACQUIRE(l);

                // scroll to the selected position, centered horizontally in the container
                double x = selected[0]->getStartX();
                double y = selected[0]->getStartY();

                int tw = fd[i]->getMinimalWidth(); // thumb width

                int ww = get_width(); // window width

                MYWRITERLOCK_RELEASE(l);

                if (doScroll) {
                    // Center thumb
                    setScrollPosition(x - (ww - tw) / 2, y);
                }

                return;
            }
        }
    }
}

void FileBrowser::openNextPreviousEditorImage (const Glib::ustring& fname, eRTNav nextPrevious)
{

    // let FileBrowser acquire Editor's perspective
    selectImage (fname, false);

    // now switch to the requested image
    if (nextPrevious == NAV_NEXT) {
        openNextImage();
    } else if (nextPrevious == NAV_PREVIOUS) {
        openPrevImage();
    }
}

void FileBrowser::thumbRearrangementNeeded ()
{
    idle_register.add(
        [this]() -> bool
        {
            refreshThumbImages();// arrangeFiles is NOT enough
            return false;
        }
    );
}

void FileBrowser::selectionChanged ()
{

    notifySelectionListener ();
}

void FileBrowser::notifySelectionListener ()
{

    if (tbl) {
        MYREADERLOCK(l, entryRW);

        std::vector<Thumbnail*> thm;

        for (size_t i = 0; i < selected.size(); i++) {
            thm.push_back ((static_cast<FileBrowserEntry*>(selected[i]))->thumbnail);
        }

        tbl->selectionChanged (thm);
    }
}

// void FileBrowser::redrawNeeded (LWButton* button)
// {
//     GuiThreadSafety::assertInGuiThread();
//     queue_draw ();
// }

FileBrowser::type_trash_changed FileBrowser::trash_changed ()
{
    return m_trash_changed;
}


// ExportPanel interface
void FileBrowser::exportRequested ()
{
    activateProcessFast();
}

void FileBrowser::setExportPanel (ExportPanel* expanel)
{

    exportPanel = expanel;
    exportPanel->set_sensitive (false);
    exportPanel->setExportPanelListener (this);
}

void FileBrowser::storeCurrentValue()
{
}

void FileBrowser::updateProfileList()
{
    // Remove existing actions
    for (const auto& action : applyProfileActions) {
        pmenuActions->remove_action(action->property_name().get_value());
    }

    // Lock and get list
    const std::vector<const ProfileStoreEntry*>* profEntries =
        ProfileStore::getInstance()->getFileList();

    struct Menus {
        Glib::RefPtr<Gio::Menu> full;
        Glib::RefPtr<Gio::Menu> partial;
    };
    std::unordered_map<unsigned short /* folderId */, Menus> subMenuList;

    auto fullMenu = Gio::Menu::create();
    auto partialMenu = Gio::Menu::create();
    {
        Menus& menus = subMenuList[0];
        menus.full = fullMenu;
        menus.partial = partialMenu;
    }
    auto createMenus = [&](const ProfileStoreEntry* entry) {
        Menus menus = {};
        menus.full = Gio::Menu::create();
        menus.partial = Gio::Menu::create();

        Menus& parentMenus = subMenuList.at(entry->parentFolderId);
        parentMenus.full->append_submenu(entry->label, menus.full);
        parentMenus.partial->append_submenu(entry->label, menus.partial);

        subMenuList.emplace(entry->folderId, std::move(menus));
    };

    // Hardcoded value...
    constexpr unsigned short BUNDLED_PROFILES_FOLDER_ID = 1;

    for (size_t i = 0; i < profEntries->size(); i++) {
        const ProfileStoreEntry* entry = profEntries->at(i);
        if (entry->type == PSET_FOLDER) {
            // Skip bundled profiles folder
            if (options.useBundledProfiles || entry->folderId != BUNDLED_PROFILES_FOLDER_ID) {
                createMenus(entry);
            }
        } else {
            // Skip bundled profiles
            if (options.useBundledProfiles || entry->parentFolderId != BUNDLED_PROFILES_FOLDER_ID) {
                auto fullActionName = Glib::ustring::compose("apply-profile%1", i);
                auto fullAction = Gio::SimpleAction::create(fullActionName);
                fullAction->signal_activate().connect([&, i](auto) {
                    activateApplyProfile(i);
                });
                applyProfileActions.push_back(fullAction);
                pmenuActions->add_action(fullAction);

                auto partialActionName = Glib::ustring::compose("apply-profile-partial%1", i);
                auto partialAction = Gio::SimpleAction::create(partialActionName);
                partialAction->signal_activate().connect([&, i](auto) {
                    activateApplyPartialProfile(i);
                });
                applyProfileActions.push_back(partialAction);
                pmenuActions->add_action(partialAction);

                Menus& parentMenus = subMenuList.at(entry->parentFolderId);
                parentMenus.full->append(entry->label, getActionName(fullActionName.c_str()));
                parentMenus.partial->append(entry->label, getActionName(partialActionName.c_str()));
            }
        }
    }

    ProfileStore::getInstance()->releaseFileList();

    // Have to remove and re-insert in order to trigger menu redraw
    constexpr int APPLY_POSITION = 3;
    profileOperationsMenu->remove(APPLY_POSITION);
    profileOperationsMenu->remove(APPLY_POSITION);
    // Insert in reverse order to reuse the same index
    profileOperationsMenu->insert_submenu(
        APPLY_POSITION, M("FILEBROWSER_APPLYPROFILE_PARTIAL"), partialMenu);
    profileOperationsMenu->insert_submenu(
        APPLY_POSITION, M("FILEBROWSER_APPLYPROFILE"), fullMenu);
}

void FileBrowser::restoreValue()
{
}

void FileBrowser::openRequested( std::vector<FileBrowserEntry*> mselected)
{
    std::vector<Thumbnail*> entries;
    // in Single Editor Mode open only last selected image
    size_t openStart = options.tabbedUI ? 0 : ( mselected.size() > 0 ? mselected.size() - 1 : 0);

    for (size_t i = openStart; i < mselected.size(); i++) {
        entries.push_back (mselected[i]->thumbnail);
    }

    tbl->openRequested (entries);
}

void FileBrowser::inspectRequested(std::vector<FileBrowserEntry*> mselected)
{
    getInspector()->showWindow(true);
}
