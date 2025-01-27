
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
#include "toolbar.h"
#include "multilangmgr.h"
#include "guiutils.h"
#include "lockablecolorpicker.h"
#include "rtimage.h"

ToolBar::ToolBar () : showColPickers(true), listener (nullptr), pickerListener(nullptr)
{

    editingMode = false;
    handimg.reset(new RtImage("hand-open"));
    editinghandimg.reset(new RtImage("crosshair-adjust"));

    handTool = Gtk::manage (new Gtk::ToggleButton ());
    handTool->set_child (*handimg);
    handTool->set_has_frame(false);

    pack_start (this, *handTool);

    wbTool = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* wbimg = Gtk::manage (new RtImage ("color-picker"));
    wbTool->set_child (*wbimg);
    wbTool->set_has_frame(false);

    pack_start (this, *wbTool);

    showcolpickersimg.reset(new RtImage("color-picker-bars"));
    hidecolpickersimg.reset(new RtImage("color-picker-hide"));

    colPickerTool = Gtk::manage (new Gtk::ToggleButton ());
    colPickerTool->set_child (*showcolpickersimg);
    colPickerTool->set_has_frame(false);

    pack_start (this, *colPickerTool);

    cropTool = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* cropimg = Gtk::manage (new RtImage ("crop"));
    cropTool->set_child (*cropimg);
    cropTool->set_has_frame(false);

    pack_start (this, *cropTool);

    straTool = Gtk::manage (new Gtk::ToggleButton ());
    Gtk::Image* straimg = Gtk::manage (new RtImage ("rotate-straighten"));
    straTool->set_child (*straimg);
    straTool->set_has_frame(false);

    pack_start (this, *straTool);

    perspTool = Gtk::manage(new Gtk::ToggleButton());
    Gtk::Image* perspimg = Gtk::manage(new RtImage("perspective-vertical-bottom"));
    perspTool->set_child(*perspimg);
    perspTool->set_has_frame(false);
    pack_start (this, *perspTool);


    handTool->set_active (true);
    current = TMHand;
    allowNoTool = false;

    handConn = handTool->signal_toggled().connect( sigc::mem_fun(*this, &ToolBar::hand_pressed));
    wbConn   = wbTool->signal_toggled().connect( sigc::mem_fun(*this, &ToolBar::wb_pressed));
    cropConn = cropTool->signal_toggled().connect( sigc::mem_fun(*this, &ToolBar::crop_pressed));
    straConn = straTool->signal_toggled().connect( sigc::mem_fun(*this, &ToolBar::stra_pressed));
    perspConn = perspTool->signal_toggled().connect( sigc::mem_fun(*this, &ToolBar::persp_pressed));

    clickController = Gtk::GestureClick::create();
    cpConn = clickController->signal_pressed().connect(
        sigc::mem_fun(*this, &ToolBar::colPicker_pressed));
    add_controller(clickController);

    handTool->set_tooltip_markup (M("TOOLBAR_TOOLTIP_HAND"));
    wbTool->set_tooltip_markup (M("TOOLBAR_TOOLTIP_WB"));
    colPickerTool->set_tooltip_markup (M("TOOLBAR_TOOLTIP_COLORPICKER"));
    cropTool->set_tooltip_markup (M("TOOLBAR_TOOLTIP_CROP"));
    straTool->set_tooltip_markup (M("TOOLBAR_TOOLTIP_STRAIGHTEN"));
    perspTool->set_tooltip_markup(M("TOOLBAR_TOOLTIP_PERSPECTIVE"));
}

//
// Selects the desired tool without notifying the listener
//
void ToolBar::setTool (ToolMode tool)
{

    bool stopEdit;

    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    stopEdit = tool == TMHand && (handTool->get_active() || (perspTool && perspTool->get_active())) && editingMode && !blockEdit;

    handTool->set_active (false);

    if (wbTool) {
        wbTool->set_active (false);
    }

    cropTool->set_active (false);
    straTool->set_active (false);
    if (colPickerTool) {
        colPickerTool->set_active (false);
    }
    if (perspTool) {
        perspTool->set_active(false);
    }

    if (tool == TMHand) {
        handTool->set_active (true);
        handTool->grab_focus(); // switch focus to the handTool button
    } else if (tool == TMSpotWB) {
        if (wbTool) {
            wbTool->set_active (true);
        }
    } else if (tool == TMCropSelect) {
        cropTool->set_active (true);
    } else if (tool == TMStraighten) {
        straTool->set_active (true);
    } else if (tool == TMColorPicker) {
        if (colPickerTool) {
            colPickerTool->set_active (true);
        }
    } else if (tool == TMPerspective) {
        if (perspTool) {
            perspTool->set_active(true);
            // Perspective is a hand tool, but has its own button.
            handTool->set_child(*handimg);
        }
    }

    current = tool;

    }

    if (stopEdit) {
        stopEditMode();

        if (listener) {
            listener->editModeSwitchedOff();
        }
    }
}

void ToolBar::startEditMode()
{
    if (!editingMode) {
        {
        ConnectionBlocker handBlocker(handConn);
        ConnectionBlocker straBlocker(straConn);
        ConnectionBlocker cropBlocker(cropConn);
        ConnectionBlocker perspBlocker(perspConn);
        ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

        if (current != TMHand) {
            if (colPickerTool) {
                colPickerTool->set_active(false);
            }
            if (wbTool) {
                wbTool->set_active (false);
            }

            cropTool->set_active (false);
            straTool->set_active (false);
            if (perspTool) {
                perspTool->set_active(false);
            }
            current = TMHand;
        }
        handTool->set_active (true);

        }

        editingMode = true;
        handTool->set_child(*editinghandimg);
    }

#ifndef NDEBUG
    else {
        printf("Editing mode already active!\n");
    }

#endif
}

void ToolBar::stopEditMode()
{
    if (editingMode) {
        editingMode = false;
        handTool->set_child(*handimg);
    }
}

void ToolBar::hand_pressed ()
{
    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    if (editingMode && !blockEdit) {
        stopEditMode();
        if (listener) {
            listener->editModeSwitchedOff ();
        }
    }

    if (colPickerTool) {
        colPickerTool->set_active(false);
    }
    if (wbTool) {
        wbTool->set_active (false);
    }

    cropTool->set_active (false);
    straTool->set_active (false);
    if (perspTool) {
        perspTool->set_active(false);
    }
    handTool->set_active (true);

    if (current != TMHand) {
        current = TMHand;
    } else if (allowNoTool) {
        current = TMNone;
        handTool->set_active(false);
    }

    }

    if (listener) {
        listener->toolSelected (current);
    }
}

void ToolBar::wb_pressed ()
{
    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    if (current != TMSpotWB) {
        if (editingMode) {
            stopEditMode();
            if (listener) {
                listener->editModeSwitchedOff ();
            }
        }
        handTool->set_active (false);
        cropTool->set_active (false);
        straTool->set_active (false);
        if (perspTool) {
            perspTool->set_active(false);
        }
        if (colPickerTool) {
            colPickerTool->set_active(false);
        }
        current = TMSpotWB;
    }

    if (wbTool) {
        wbTool->set_active (true);
    }

    }

    if (listener) {
        listener->toolSelected (TMSpotWB);
    }
}

void ToolBar::colPicker_pressed (int n_press, double x, double y)
{
    const auto button = clickController->get_current_button();
    if (button == 1) {
        {
        ConnectionBlocker handBlocker(handConn);
        ConnectionBlocker straBlocker(straConn);
        ConnectionBlocker cropBlocker(cropConn);
        ConnectionBlocker wbWasBlocked(wbTool, wbConn);

        cropTool->set_active (false);
        if (wbTool) {
            wbTool->set_active (false);
        }
        straTool->set_active (false);
        if (perspTool) {
            perspTool->set_active(false);
        }

        if (current != TMColorPicker) {
            // Disabling all other tools, enabling the Picker tool and entering the "visible pickers" mode
            if (editingMode && !blockEdit) {
                stopEditMode();
                if (listener) {
                    listener->editModeSwitchedOff ();
                }
            }
            handTool->set_active (false);
            showColorPickers(true);
            current = TMColorPicker;
            if (pickerListener) {
                pickerListener->switchPickerVisibility (showColPickers);
            }
        } else {
            // Disabling the picker tool, enabling the Hand tool and keeping the "visible pickers" mode
            handTool->set_active (true);
            //colPickerTool->set_active (false);  Done by the standard event handler
            current = TMHand;
        }

        }

        if (listener) {
            listener->toolSelected (current);
        }
    } else if (button == 3) {
        if (current == TMColorPicker) {
            // Disabling the Picker tool and entering into the "invisible pickers" mode
            ConnectionBlocker handBlocker(handConn);
            ConnectionBlocker cpWasBlocked(cpConn);
            handTool->set_active (true);
            colPickerTool->set_active (false);
            current = TMHand;
            showColorPickers(false);
        } else {
            // The Picker tool is already disabled, entering into the "invisible pickers" mode
            switchColorPickersVisibility();
        }
        if (pickerListener) {
            pickerListener->switchPickerVisibility (showColPickers);
        }
    }
}

bool ToolBar::showColorPickers(bool showCP)
{
    if (showColPickers != showCP) {
        // Inverting the state
        colPickerTool->set_child(showCP ? *showcolpickersimg : *hidecolpickersimg);
        showColPickers = showCP;
        return true;
    }

    return false;
}

void ToolBar::switchColorPickersVisibility()
{
    // Inverting the state
    showColPickers = !showColPickers;
    colPickerTool->set_child(showColPickers ? *showcolpickersimg : *hidecolpickersimg);
}

void ToolBar::crop_pressed ()
{
    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    if (editingMode) {
        stopEditMode();
        if (listener) {
            listener->editModeSwitchedOff ();
        }
    }
    handTool->set_active (false);
    if (colPickerTool) {
        colPickerTool->set_active(false);
    }
    if (wbTool) {
        wbTool->set_active (false);
    }

    straTool->set_active (false);
    if (perspTool) {
        perspTool->set_active(false);
    }
    cropTool->set_active (true);

    if (current != TMCropSelect) {
        current = TMCropSelect;
        cropTool->grab_focus ();
    } else if (allowNoTool) {
        current = TMNone;
        cropTool->set_active(false);
    }

    }

    if (listener) {
        listener->toolSelected (current);
    }
}

void ToolBar::stra_pressed ()
{
    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    if (editingMode) {
        stopEditMode();
        if (listener) {
            listener->editModeSwitchedOff ();
        }
    }
    handTool->set_active (false);
    if (colPickerTool) {
        colPickerTool->set_active(false);
    }
    if (wbTool) {
        wbTool->set_active (false);
    }

    cropTool->set_active (false);
    if (perspTool) {
        perspTool->set_active(false);
    }
    straTool->set_active (true);

    if (current != TMStraighten) {
        current = TMStraighten;
    } else if (allowNoTool) {
        current = TMNone;
        straTool->set_active(false);
    }

    }

    if (listener) {
        listener->toolSelected (current);
    }
}

void ToolBar::persp_pressed ()
{
    if (listener && !perspTool->get_active()) {
        listener->toolDeselected(TMPerspective);
        return;
    }

    // Unlike other modes, mode switching is handled by the perspective panel.
    {
    ConnectionBlocker handBlocker(handConn);
    ConnectionBlocker straBlocker(straConn);
    ConnectionBlocker cropBlocker(cropConn);
    ConnectionBlocker perspBlocker(perspConn);
    ConnectionBlocker wbWasBlocked(wbTool, wbConn), cpWasBlocked(colPickerTool, cpConn);

    if (editingMode) {
        stopEditMode();
        if (listener) {
            listener->editModeSwitchedOff();
        }
    }

    }

    if (listener) {
        listener->toolSelected(TMPerspective);
    }
}

bool ToolBar::handleShortcutKey (guint keyval, guint keycode, Gdk::ModifierType state)
{

    bool ctrl = isControlOrMetaDown(state);
    bool alt = isAltDown(state);

    if (!ctrl && !alt) {
        switch(keyval) {
        case GDK_KEY_w:
        case GDK_KEY_W:
            if(wbTool) {
                wb_pressed ();
                return true;
            }

            return false;

        case GDK_KEY_c:
        case GDK_KEY_C:
            crop_pressed ();
            return true;

        case GDK_KEY_s:
        case GDK_KEY_S:
            stra_pressed ();
            return true;

        case GDK_KEY_h:
        case GDK_KEY_H:
            hand_pressed ();
            return true;
        }
    }

    return false;
}

void ToolBar::setBatchMode()
{
    if (wbTool) {
        wbConn.disconnect();
        removeIfThere(this, wbTool, false);
        wbTool = nullptr;
    }
    if (colPickerTool) {
        cpConn.disconnect();
        removeIfThere(this, colPickerTool, false);
        colPickerTool = nullptr;
    }
    if (perspTool) {
        perspConn.disconnect();
        removeIfThere(this, perspTool, false);
        perspTool = nullptr;
    }

    allowNoTool = true;
    switch (current) {
    case TMHand:
        hand_pressed();
        break;
    case TMCropSelect:
        crop_pressed();
        break;
    case TMStraighten:
        stra_pressed();
        break;
    default:
        break;
    }
}

