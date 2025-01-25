/*
 *  This file is part of RawTherapee.
 *
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
#include <cairomm/cairomm.h>

#include "rtengine/procparams.h"
#include "rtengine/rt_math.h"
#include "rtengine/utils.h"

#include "guiutils.h"

// #include "adjuster.h"
#include "multilangmgr.h"
#include "options.h"
#include "rtimage.h"
#include "rtmessagedialog.h"
#include "rtscalable.h"
// #include "toolpanel.h"

#include <assert.h>

using namespace std;

namespace
{

template <class T, class RemoveFunc>
bool removeIfThere(T parent, Gtk::Widget* widget, bool inc_ref, RemoveFunc remove)
{
    auto found = std::find(parent->get_children().begin(), parent->get_children().end(), widget);
    if (found == parent->get_children().end()) return false;

    if (inc_ref) {
        widget->reference();
    }
    std::invoke(remove);
    return true;
}

void drawCropGuides(const Cairo::RefPtr<Cairo::Context>& cr,
                    double rectx1, double recty1, double rectx2, double recty2,
                    const rtengine::procparams::CropParams& cparams)
{
    if (cparams.guide == rtengine::procparams::CropParams::Guide::NONE) return;

    cr->set_line_width (1.0);
    cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
    cr->move_to (rectx1, recty1);
    cr->line_to (rectx2, recty1);
    cr->line_to (rectx2, recty2);
    cr->line_to (rectx1, recty2);
    cr->line_to (rectx1, recty1);
    cr->stroke ();
    cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
    cr->set_dash (std::valarray<double>({4}), 0);
    cr->move_to (rectx1, recty1);
    cr->line_to (rectx2, recty1);
    cr->line_to (rectx2, recty2);
    cr->line_to (rectx1, recty2);
    cr->line_to (rectx1, recty1);
    cr->stroke ();
    cr->set_dash (std::valarray<double>(), 0);

    if (
        cparams.guide != rtengine::procparams::CropParams::Guide::RULE_OF_DIAGONALS
        && cparams.guide != rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_1
        && cparams.guide != rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_2
    ) {
        // draw guide lines
        std::vector<double> horiz_ratios;
        std::vector<double> vert_ratios;

        switch (cparams.guide) {
            case rtengine::procparams::CropParams::Guide::NONE:
            case rtengine::procparams::CropParams::Guide::FRAME:
            case rtengine::procparams::CropParams::Guide::RULE_OF_DIAGONALS:
            case rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_1:
            case rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_2: {
                break;
            }

            case rtengine::procparams::CropParams::Guide::RULE_OF_THIRDS: {
                horiz_ratios.push_back (1.0 / 3.0);
                horiz_ratios.push_back (2.0 / 3.0);
                vert_ratios.push_back (1.0 / 3.0);
                vert_ratios.push_back (2.0 / 3.0);
                break;
            }

            case rtengine::procparams::CropParams::Guide::HARMONIC_MEANS: {
                horiz_ratios.push_back (1.0 - 0.618);
                horiz_ratios.push_back (0.618);
                vert_ratios.push_back (0.618);
                vert_ratios.push_back (1.0 - 0.618);
                break;
            }

            case rtengine::procparams::CropParams::Guide::GRID: {
                // To have even distribution, normalize it a bit
                const int longSideNumLines = 10;

                int w = rectx2 - rectx1, h = recty2 - recty1;

                if (w > longSideNumLines && h > longSideNumLines) {
                    if (w > h) {
                        for (int i = 1; i < longSideNumLines; i++) {
                            vert_ratios.push_back ((double)i / longSideNumLines);
                        }

                        int shortSideNumLines = (int)round(h * (double)longSideNumLines / w);

                        for (int i = 1; i < shortSideNumLines; i++) {
                            horiz_ratios.push_back ((double)i / shortSideNumLines);
                        }
                    } else {
                        for (int i = 1; i < longSideNumLines; i++) {
                            horiz_ratios.push_back ((double)i / longSideNumLines);
                        }

                        int shortSideNumLines = (int)round(w * (double)longSideNumLines / h);

                        for (int i = 1; i < shortSideNumLines; i++) {
                            vert_ratios.push_back ((double)i / shortSideNumLines);
                        }
                    }
                }
                break;
            }

            case rtengine::procparams::CropParams::Guide::EPASSPORT: {
                /* Official measurements do not specify exact ratios, just min/max measurements within which the eyes and chin-crown distance must lie. I averaged those measurements to produce these guides.
                 * The first horizontal guide is for the crown, the second is roughly for the nostrils, the third is for the chin.
                 * http://www.homeoffice.gov.uk/agencies-public-bodies/ips/passports/information-photographers/
                 * "(...) the measurement of the face from the bottom of the chin to the crown (ie the top of the head, not the top of the hair) is between 29mm and 34mm."
                 */
                horiz_ratios.push_back (7.0 / 45.0);
                horiz_ratios.push_back (26.0 / 45.0);
                horiz_ratios.push_back (37.0 / 45.0);
                vert_ratios.push_back (0.5);
                break;
            }

            case rtengine::procparams::CropParams::Guide::CENTERED_SQUARE: {
                const double w = rectx2 - rectx1, h = recty2 - recty1;
                double ratio = w / h;
                if (ratio < 1.0) {
                    ratio = 1.0 / ratio;
                    horiz_ratios.push_back((ratio - 1.0) / (2.0 * ratio));
                    horiz_ratios.push_back(1.0 - (ratio - 1.0) / (2.0 * ratio));
                } else {
                    vert_ratios.push_back((ratio - 1.0) / (2.0 * ratio));
                    vert_ratios.push_back(1.0 - (ratio - 1.0) / (2.0 * ratio));
                }
                break;
            }
        }

        // Horizontals
        for (size_t i = 0; i < horiz_ratios.size(); i++) {
            cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
            cr->move_to (rectx1, recty1 + round((recty2 - recty1) * horiz_ratios[i]));
            cr->line_to (rectx2, recty1 + round((recty2 - recty1) * horiz_ratios[i]));
            cr->stroke ();
            cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
            std::valarray<double> ds (1);
            ds[0] = 4;
            cr->set_dash (ds, 0);
            cr->move_to (rectx1, recty1 + round((recty2 - recty1) * horiz_ratios[i]));
            cr->line_to (rectx2, recty1 + round((recty2 - recty1) * horiz_ratios[i]));
            cr->stroke ();
            ds.resize (0);
            cr->set_dash (ds, 0);
        }

        // Verticals
        for (size_t i = 0; i < vert_ratios.size(); i++) {
            cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
            cr->move_to (rectx1 + round((rectx2 - rectx1) * vert_ratios[i]), recty1);
            cr->line_to (rectx1 + round((rectx2 - rectx1) * vert_ratios[i]), recty2);
            cr->stroke ();
            cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
            std::valarray<double> ds (1);
            ds[0] = 4;
            cr->set_dash (ds, 0);
            cr->move_to (rectx1 + round((rectx2 - rectx1) * vert_ratios[i]), recty1);
            cr->line_to (rectx1 + round((rectx2 - rectx1) * vert_ratios[i]), recty2);
            cr->stroke ();
            ds.resize (0);
            cr->set_dash (ds, 0);
        }
    } else if (cparams.guide == rtengine::procparams::CropParams::Guide::RULE_OF_DIAGONALS) {
        double corners_from[4][2];
        double corners_to[4][2];
        int mindim = min(rectx2 - rectx1, recty2 - recty1);
        corners_from[0][0] = rectx1;
        corners_from[0][1] = recty1;
        corners_to[0][0]   = rectx1 + mindim;
        corners_to[0][1]   = recty1 + mindim;
        corners_from[1][0] = rectx1;
        corners_from[1][1] = recty2;
        corners_to[1][0]   = rectx1 + mindim;
        corners_to[1][1]   = recty2 - mindim;
        corners_from[2][0] = rectx2;
        corners_from[2][1] = recty1;
        corners_to[2][0]   = rectx2 - mindim;
        corners_to[2][1]   = recty1 + mindim;
        corners_from[3][0] = rectx2;
        corners_from[3][1] = recty2;
        corners_to[3][0]   = rectx2 - mindim;
        corners_to[3][1]   = recty2 - mindim;

        for (int i = 0; i < 4; i++) {
            cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
            cr->move_to (corners_from[i][0], corners_from[i][1]);
            cr->line_to (corners_to[i][0], corners_to[i][1]);
            cr->stroke ();
            cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
            std::valarray<double> ds (1);
            ds[0] = 4;
            cr->set_dash (ds, 0);
            cr->move_to (corners_from[i][0], corners_from[i][1]);
            cr->line_to (corners_to[i][0], corners_to[i][1]);
            cr->stroke ();
            ds.resize (0);
            cr->set_dash (ds, 0);
        }
    } else if (
        cparams.guide == rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_1
        || cparams.guide == rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_2
    ) {
        // main diagonal
        if(cparams.guide == rtengine::procparams::CropParams::Guide::GOLDEN_TRIANGLE_2) {
            std::swap(rectx1, rectx2);
        }

        cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
        cr->move_to (rectx1, recty1);
        cr->line_to (rectx2, recty2);
        cr->stroke ();
        cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
        cr->set_dash (std::valarray<double>({4}), 0);
        cr->move_to (rectx1, recty1);
        cr->line_to (rectx2, recty2);
        cr->stroke ();
        cr->set_dash (std::valarray<double>(), 0);

        double height = recty2 - recty1;
        double width = rectx2 - rectx1;
        double d = sqrt(height * height + width * width);
        double alpha = asin(width / d);
        double beta = asin(height / d);
        double a = sin(beta) * height;
        double b = sin(alpha) * height;

        double x = (a * b) / height;
        double y = height - (b * (d - a)) / width;
        cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
        cr->move_to (rectx1, recty2);
        cr->line_to (rectx1 + x, recty1 + y);
        cr->stroke ();
        cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
        cr->set_dash (std::valarray<double>({4}), 0);
        cr->move_to (rectx1, recty2);
        cr->line_to (rectx1 + x, recty1 + y);
        cr->stroke ();
        cr->set_dash (std::valarray<double>(), 0);

        x = width - (a * b) / height;
        y = (b * (d - a)) / width;
        cr->set_source_rgba (1.0, 1.0, 1.0, 0.618);
        cr->move_to (rectx2, recty1);
        cr->line_to (rectx1 + x, recty1 + y);
        cr->stroke ();
        cr->set_source_rgba (0.0, 0.0, 0.0, 0.618);
        cr->set_dash (std::valarray<double>({4}), 0);
        cr->move_to (rectx2, recty1);
        cr->line_to (rectx1 + x, recty1 + y);
        cr->stroke ();
        cr->set_dash (std::valarray<double>(), 0);
    }
}

}  // namespace

// IdleRegister::~IdleRegister()
// {
//     destroy();
// }
//
// void IdleRegister::add(std::function<bool ()> function, gint priority)
// {
//     const auto dispatch =
//         [](gpointer data) -> gboolean
//         {
//             DataWrapper* const data_wrapper = static_cast<DataWrapper*>(data);
//
//             if (!data_wrapper->function()) {
//                 data_wrapper->self->mutex.lock();
//                 data_wrapper->self->ids.erase(data_wrapper);
//                 data_wrapper->self->mutex.unlock();
//
//                 delete data_wrapper;
//                 return FALSE;
//             }
//
//             return TRUE;
//         };
//
//     DataWrapper* const data_wrapper = new DataWrapper{
//         this,
//         std::move(function)
//     };
//
//     mutex.lock();
//     ids[data_wrapper] = gdk_threads_add_idle_full(priority, dispatch, data_wrapper, nullptr);
//     mutex.unlock();
// }
//
// void IdleRegister::destroy()
// {
//     mutex.lock();
//     for (const auto& id : ids) {
//         g_source_remove(id.second);
//         delete id.first;
//     }
//     ids.clear();
//     mutex.unlock();
// }
//
// BlockAdjusterEvents::BlockAdjusterEvents(Adjuster* adjuster) : adj(adjuster)
// {
//     if (adj) {
//         adj->block(true);
//     }
// }
//
// BlockAdjusterEvents::~BlockAdjusterEvents()
// {
//     if (adj) {
//         adj->block(false);
//     }
// }
//
// DisableListener::DisableListener(ToolPanel* panelToDisable) : panel(panelToDisable)
// {
//     if (panel) {
//         panel->disableListener();
//     }
// }
//
// DisableListener::~DisableListener()
// {
//     if (panel) {
//         panel->enableListener();
//     }
// }
//
Glib::ustring escapeHtmlChars(const Glib::ustring &src)
{

    // Sources chars to be escaped
    static const Glib::ustring srcChar("&<>");

    // Destination strings, in the same order than the source
    static std::vector<Glib::ustring> dstChar(3);
    dstChar.at(0) = "&amp;";
    dstChar.at(1) = "&lt;";
    dstChar.at(2) = "&gt;";

    // Copying the original string, that will be modified
    Glib::ustring dst(src);

    // Iterating all chars of the copy of the source string
    for (size_t i = 0; i < dst.length();) {

        // Looking out if it's part of the characters to be escaped
        size_t pos = srcChar.find_first_of(dst.at(i), 0);

        if (pos != Glib::ustring::npos) {
            // If yes, replacing the char in the destination string
            dst.replace(i, 1, dstChar.at(pos));
            // ... and going forward  by the length of the new string
            i += dstChar.at(pos).length();
        } else {
            ++i;
        }
    }

    return dst;
}

void pack_start(Gtk::Box* box, Gtk::Widget& child, Pack pack, int /*padding*/)
{
    if (box->get_orientation() == Gtk::Orientation::HORIZONTAL) {
        if (pack == Pack::EXPAND_WIDGET) {
            child.set_halign(Gtk::Align::FILL);
            child.set_hexpand(true);
        } else {
            child.set_halign(Gtk::Align::START);
            child.set_hexpand(false);
        }
    } else {
        if (pack == Pack::EXPAND_WIDGET) {
            child.set_valign(Gtk::Align::FILL);
            child.set_vexpand(true);
        } else {
            child.set_valign(Gtk::Align::START);
            child.set_vexpand(false);
        }
    }
    box->append(child);
}

void pack_start(Gtk::Box* box, Gtk::Widget& child, bool expand, bool fill, int /*padding*/)
{
    if (box->get_orientation() == Gtk::Orientation::HORIZONTAL) {
        child.set_halign(fill ? Gtk::Align::FILL : Gtk::Align::START);
        child.set_hexpand(expand);
    } else {
        child.set_valign(fill ? Gtk::Align::FILL : Gtk::Align::START);
        child.set_vexpand(expand);
    }
    box->append(child);
}

void pack_end(Gtk::Box* box, Gtk::Widget& child, Pack pack, int /*padding*/)
{
    if (box->get_orientation() == Gtk::Orientation::HORIZONTAL) {
        if (pack == Pack::EXPAND_WIDGET) {
            child.set_halign(Gtk::Align::FILL);
            child.set_hexpand(true);
        } else {
            child.set_halign(Gtk::Align::END);
            child.set_hexpand(false);
        }
    } else {
        if (pack == Pack::EXPAND_WIDGET) {
            child.set_valign(Gtk::Align::FILL);
            child.set_vexpand(true);
        } else {
            child.set_valign(Gtk::Align::END);
            child.set_vexpand(false);
        }
    }
    box->append(child);
}

void setExpandAlignProperties(Gtk::Widget *widget, bool hExpand, bool vExpand, enum Gtk::Align hAlign, enum Gtk::Align vAlign)
{
    widget->set_hexpand(hExpand);
    widget->set_vexpand(vExpand);
    widget->set_halign(hAlign);
    widget->set_valign(vAlign);
}

Gtk::Border getPadding(const Glib::RefPtr<Gtk::StyleContext> style)
{
    Gtk::Border padding;
    if (!style) {
        return padding;
    }

    padding = style->get_padding();

    if (RTScalable::getGlobalScale() > 1.0) {
        // Scale pixel border size based on DPI and Scale
        padding.set_left(RTScalable::scalePixelSize(padding.get_left()));
        padding.set_right(RTScalable::scalePixelSize(padding.get_right()));
        padding.set_top(RTScalable::scalePixelSize(padding.get_top()));
        padding.set_bottom(RTScalable::scalePixelSize(padding.get_bottom()));
    }

    return padding;
}

bool removeIfThere(Gtk::Box* box, Gtk::Widget* w, bool increference)
{
    return removeIfThere(box, w, increference, [&]() { box->remove(*w); });
}
bool removeIfThere(Gtk::Grid* grid, Gtk::Widget* w, bool increference)
{
    return removeIfThere(grid, w, increference, [&]() { grid->remove(*w); });
}

// TODO(gtk4): Make this and callers async safe
bool confirmOverwrite (Gtk::Window& parent, const std::string& filename)
{
    bool safe = true;

    if (Glib::file_test (filename, Glib::FileTest::EXISTS)) {
        Glib::ustring msg_ = Glib::ustring ("<b>\"") + escapeHtmlChars(Glib::path_get_basename (filename)) + "\": "
                             + M("MAIN_MSG_ALREADYEXISTS") + "</b>\n" + M("MAIN_MSG_QOVERWRITE");
        auto msgd = Gtk::make_managed<RtMessageDialog>(
            msg_,
            RtMessageDialog::Type::WARNING,
            RtMessageDialog::ButtonSet::YES_NO);
        // TODO(gtk4)
        // safe = (msgd.run () == Gtk::ResponseType::YES);
        msgd->show();
    }

    return safe;
}

void drawCrop (const Cairo::RefPtr<Cairo::Context>& cr,
               double imx, double imy, double imw, double imh,
               double clipWidth, double clipHeight,
               double startx, double starty, double scale,
               const rtengine::procparams::CropParams& cparams,
               bool drawGuide, bool useBgColor, bool fullImageVisible)
{
    cr->save();

    cr->set_line_width(0.0);
    cr->rectangle(imx, imy, clipWidth, clipHeight);
    cr->clip();

    double c1x = (cparams.x - startx) * scale;
    double c1y = (cparams.y - starty) * scale;
    double c2x = (cparams.x + cparams.w - startx) * scale - (fullImageVisible ? 0.0 : 1.0);
    double c2y = (cparams.y + cparams.h - starty) * scale - (fullImageVisible ? 0.0 : 1.0);

    // crop overlay color, linked with crop windows background
    if (options.bgcolor == 0 || !useBgColor) {
        cr->set_source_rgba (options.cutOverlayBrush[0], options.cutOverlayBrush[1], options.cutOverlayBrush[2], options.cutOverlayBrush[3]);
    } else if (options.bgcolor == 1) {
        cr->set_source_rgb (0, 0, 0);
    } else if (options.bgcolor == 2) {
        cr->set_source_rgb (1, 1, 1);
    } else if (options.bgcolor == 3) {
        cr->set_source_rgb (0.467, 0.467, 0.467);
    }

    cr->rectangle (imx, imy, imw + 0.5, round(c1y) + 0.5);
    cr->rectangle (imx, round(imy + c2y) + 0.5, imw + 0.5, round(imh - c2y) + 0.5);
    cr->rectangle (imx, round(imy + c1y) + 0.5, round(c1x) + 0.5, round(c2y - c1y + 1) + 0.5);
    cr->rectangle (round(imx + c2x) + 0.5, round(imy + c1y) + 0.5, round(imw - c2x) + 0.5, round(c2y - c1y + 1) + 0.5);
    cr->fill ();

    cr->restore();

    // rectangle around the cropped area and guides
    if (cparams.guide != rtengine::procparams::CropParams::Guide::NONE && drawGuide) {
        double rectx1 = round(c1x) + imx + 0.5;
        double recty1 = round(c1y) + imy + 0.5;
        double rectx2 = round(c2x) + imx + 0.5;
        double recty2 = round(c2y) + imy + 0.5;

        if(fullImageVisible) {
            rectx2 = min(rectx2, imx + imw - 0.5);
            recty2 = min(recty2, imy + imh - 0.5);
        }

        drawCropGuides(cr, rectx1, recty1, rectx2, recty2, cparams);
    }
}

ExpanderBox::ExpanderBox()
{
    set_name ("ExpanderBox");
}

void ExpanderBox::setLevel(int level)
{
    if (level <= 1) {
        set_name("ExpanderBox");
    } else if (level == 2) {
        set_name("ExpanderBox2");
    } else if (level >= 3) {
        set_name("ExpanderBox3");
    }
}

void ExpanderBox::show_all()
{
    // ask childs to show themselves, but not us (remain unchanged)
    for (auto& child : get_children()) {
        child->show();
    }
}

void ExpanderBox::showBox()
{
    Gtk::Box::show();
}

void ExpanderBox::hideBox()
{
    Gtk::Box::hide();
}

MyExpander::MyExpander(bool useEnabled, Gtk::Widget* titleWidget) :
    inconsistentImage("power-inconsistent-small"),
    enabledImage("power-on-small"),
    disabledImage("power-off-small"),
    openedImage("expander-open-small"),
    closedImage("expander-closed-small"),
    enabled(false), inconsistent(false), flushEvent(false), expBox(nullptr),
    child(nullptr), headerWidget(nullptr), statusImage(nullptr),
    label(nullptr), useEnabled(useEnabled)
{
    setupPart1();

    if (titleWidget) {
        setExpandAlignProperties(titleWidget, true, false, Gtk::Align::FILL, Gtk::Align::FILL);
        pack_start(headerHBox, *titleWidget, Pack::EXPAND_WIDGET, 0);
        headerWidget = titleWidget;
    }

    setupPart2();
}

MyExpander::MyExpander(bool useEnabled, Glib::ustring titleLabel) :
    inconsistentImage("power-inconsistent-small"),
    enabledImage("power-on-small"),
    disabledImage("power-off-small"),
    openedImage("expander-open-small"),
    closedImage("expander-closed-small"),
    enabled(false), inconsistent(false), flushEvent(false), expBox(nullptr),
    child(nullptr), headerWidget(nullptr),
    label(nullptr), useEnabled(useEnabled)
{
    setupPart1();

    label = Gtk::manage(new Gtk::Label());
    setExpandAlignProperties(label, true, false, Gtk::Align::START, Gtk::Align::CENTER);
    label->set_markup(escapeHtmlChars(titleLabel));
    pack_start(headerHBox, *label, Pack::EXPAND_WIDGET, 0);

    setupPart2();
}

void MyExpander::setupPart1()
{
    set_orientation(Gtk::Orientation::VERTICAL);
    set_spacing(0);
    set_name("MyExpander");
    set_can_focus(false);
    setExpandAlignProperties(this, true, false, Gtk::Align::FILL, Gtk::Align::FILL);

    headerHBox = Gtk::manage(new Gtk::Box());
    headerHBox->set_can_focus(false);
    setExpandAlignProperties(headerHBox, true, false, Gtk::Align::FILL, Gtk::Align::FILL);

    if (useEnabled) {
        get_style_context()->add_class("OnOff");
        statusImage = Gtk::manage(new RtImage(disabledImage));
        imageEvBox = Gtk::manage(new Gtk::Box());
        imageEvBox->set_name("MyExpanderStatus");
        imageEvBox->append(*statusImage);
        pack_start(headerHBox, *imageEvBox, Pack::SHRINK, 0);

        auto clickController = Gtk::GestureClick::create();
        clickController->set_button(GDK_BUTTON_PRIMARY);
        clickController->signal_released().connect(
            sigc::mem_fun(*this, &MyExpander::onEnabledChange));
        imageEvBox->add_controller(clickController);

        auto motionController = Gtk::EventControllerMotion::create();
        motionController->signal_enter().connect(
            sigc::mem_fun(*this, &MyExpander::onEnterEnable), false);
        motionController->signal_leave().connect(
            sigc::mem_fun(*this, &MyExpander::onLeaveEnable), false);
        imageEvBox->add_controller(motionController);
    } else {
        get_style_context()->add_class("Fold");
        statusImage = Gtk::manage(new RtImage(openedImage));
        pack_start(headerHBox, *statusImage, Pack::SHRINK, 0);
    }

    statusImage->set_can_focus(false);
}

void MyExpander::setupPart2()
{
    titleEvBox = Gtk::manage(new Gtk::Box());
    titleEvBox->set_name("MyExpanderTitle");
    titleEvBox->append(*headerHBox);
    titleEvBox->set_can_focus(false);

    pack_start(this, *titleEvBox, Pack::EXPAND_WIDGET, 0);

    updateStyle();

    auto clickController = Gtk::GestureClick::create();
    clickController->set_button(GDK_BUTTON_PRIMARY);
    clickController->signal_released().connect(
        sigc::mem_fun(*this, &MyExpander::onToggle));
    titleEvBox->add_controller(clickController);

    auto motionController = Gtk::EventControllerMotion::create();
    motionController->signal_enter().connect(
        sigc::mem_fun(*this, &MyExpander::onEnterTitle), false);
    motionController->signal_leave().connect(
        sigc::mem_fun(*this, &MyExpander::onLeaveTitle), false);
    titleEvBox->add_controller(motionController);
}

void MyExpander::onEnterTitle(double /*x*/, double /*y*/)
{
    if (is_sensitive()) {
        titleEvBox->set_state_flags(Gtk::StateFlags::PRELIGHT);
        queue_draw();
    }
}

void MyExpander::onLeaveTitle()
{
    if (is_sensitive()) {
        titleEvBox->set_state_flags(Gtk::StateFlags::NORMAL);
        queue_draw();
    }
}

void MyExpander::onEnterEnable(double /*x*/, double /*y*/)
{
    if (is_sensitive()) {
        imageEvBox->set_state_flags(Gtk::StateFlags::PRELIGHT);
        queue_draw();
    }
}

void MyExpander::onLeaveEnable()
{
    if (is_sensitive()) {
        imageEvBox->set_state_flags(Gtk::StateFlags::NORMAL);
        queue_draw();
    }
}

void MyExpander::updateStyle()
{
    updateVScrollbars(options.hideTPVScrollbar);

//GTK318
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION < 20
    headerHBox->set_spacing(2);
    headerHBox->set_border_width(1);
    set_spacing(0);
    set_border_width(0);
#endif
//GTK318
}

void MyExpander::updateVScrollbars(bool hide)
{
    if (hide) {
        get_style_context()->remove_class("withScrollbar");
    } else {
        get_style_context()->add_class("withScrollbar");
    }
}

void MyExpander::setLevel (int level)
{
    if (expBox) {
        expBox->setLevel(level);
    }
}

void MyExpander::setLabel (Glib::ustring newLabel)
{
    if (label) {
        label->set_markup(escapeHtmlChars(newLabel));
    }
}

void MyExpander::setLabel (Gtk::Widget *newWidget)
{
    if (headerWidget) {
        removeIfThere(headerHBox, headerWidget, false);
        pack_start(headerHBox, *newWidget, Pack::EXPAND_WIDGET, 0);
    }
}

bool MyExpander::get_inconsistent()
{
    return inconsistent;
}

void MyExpander::set_inconsistent(bool isInconsistent)
{
    if (inconsistent != isInconsistent) {
        inconsistent = isInconsistent;

        if (useEnabled) {
            if (isInconsistent) {
                statusImage->set_from_icon_name(inconsistentImage);
            } else {
                if (enabled) {
                    statusImage->set_from_icon_name(enabledImage);
                    get_style_context()->add_class("enabledTool");
                } else {
                    statusImage->set_from_icon_name(disabledImage);
                    get_style_context()->remove_class("enabledTool");
                }
            }
        }

    }
}

bool MyExpander::getUseEnabled()
{
    return useEnabled;
}

bool MyExpander::getEnabled()
{
    return enabled;
}

void MyExpander::setEnabled(bool isEnabled)
{
    if (isEnabled != enabled) {
        if (useEnabled) {
            if (enabled) {
                enabled = false;

                if (!inconsistent) {
                    statusImage->set_from_icon_name(disabledImage);
                    get_style_context()->remove_class("enabledTool");
                    message.emit();
                }
            } else {
                enabled = true;

                if (!inconsistent) {
                    statusImage->set_from_icon_name(enabledImage);
                    get_style_context()->add_class("enabledTool");
                    message.emit();
                }
            }
        }
    }
}

void MyExpander::setEnabledTooltipMarkup(Glib::ustring tooltipMarkup)
{
    if (useEnabled) {
        statusImage->set_tooltip_markup(tooltipMarkup);
    }
}

void MyExpander::setEnabledTooltipText(Glib::ustring tooltipText)
{
    if (useEnabled) {
        statusImage->set_tooltip_text(tooltipText);
    }
}

void MyExpander::set_expanded( bool expanded )
{
    if (!expBox) {
        return;
    }

    if (!useEnabled) {
        if (expanded ) {
            statusImage->set_from_icon_name(openedImage);
        } else {
            statusImage->set_from_icon_name(closedImage);
        }
    }

    if (expanded) {
        expBox->showBox();
    } else {
        expBox->hideBox();
    }
}

bool MyExpander::get_expanded()
{
    return expBox ? expBox->get_visible() : false;
}

void MyExpander::add  (Gtk::Widget& widget, bool setChild)
{
    if(setChild) {
        child = &widget;
    }
    expBox = Gtk::manage (new ExpanderBox ());
    expBox->append (widget);
    pack_start(this, *expBox, Pack::SHRINK, 0);
    expBox->hideBox();
}

void MyExpander::onToggle(int /*n_press*/, double /*x*/, double /*y*/)
{
    if (flushEvent) {
        flushEvent = false;
        return;
    }

    if (!expBox) return;

    bool isVisible = expBox->is_visible();

    if (!useEnabled) {
        if (isVisible) {
            statusImage->set_from_icon_name(closedImage);
        } else {
            statusImage->set_from_icon_name(openedImage);
        }
    }

    if (isVisible) {
        expBox->hideBox();
    } else {
        expBox->showBox();
    }

    titleButtonRelease.emit();
}

// used to connect a function to the enabled_toggled signal
MyExpander::type_signal_enabled_toggled MyExpander::signal_enabled_toggled()
{
    return message;
}

// internal use ; when the user clicks on the toggle button, it calls this method that will emit an enabled_change event
void MyExpander::onEnabledChange(int /*n_press*/, double /*x*/, double /*y*/)
{
    if (enabled) {
        enabled = false;
        statusImage->set_from_icon_name(disabledImage);
        get_style_context()->remove_class("enabledTool");
    } else {
        enabled = true;
        statusImage->set_from_icon_name(enabledImage);
        get_style_context()->add_class("enabledTool");
    }

    message.emit();
    flushEvent = true;
}

// /*
//  *
//  * Derived class of some widgets to properly handle the scroll wheel ;
//  * the user has to use the Shift key to be able to change the widget's value,
//  * otherwise the mouse wheel will scroll the editor's tabs content.
//  *
//  */
// MyScrolledWindow::MyScrolledWindow()
// {
//     auto controller = Gtk::EventControllerScroll::create();
//     controller->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
//     controller->signal_scroll().connect(
//         sigc::mem_fun(*this, &MyScrolledWindow::onScroll), false);
//     add_controller(controller);
// }
//
// bool MyScrolledWindow::onScroll(double /*dx*/, double dy)
// {
//     if (!options.hideTPVScrollbar) {
//         // Let Gtk::ScrolledWindow handle it
//         return false;
//     }
//
//     Glib::RefPtr<Gtk::Adjustment> adjust = get_vadjustment();
//     Gtk::Scrollbar* vscroll = get_vscrollbar();
//
//     if (adjust && vscroll) {
//         const double upperBound = adjust->get_upper();
//         const double lowerBound = adjust->get_lower();
//         const double value = adjust->get_value();
//         const double step = adjust->get_step_increment();
//
//         double newValue = value + step * dy;
//         newValue = std::clamp(newValue, lowerBound, upperBound);
//
//         if (newValue != value) {
//             vscroll->set_value(newValue);
//         }
//     }
//
//     return true;
// }
//
// void MyScrolledWindow::measure_vfunc(Gtk::Orientation orientation, int /*for_size*/,
//                                      int& minimum, int& natural,
//                                      int& minimum_baseline, int& natural_baseline) const
// {
//     if (orientation == Gtk::Orientation::HORIZONTAL) {
//         int width = RTScalable::scalePixelSize(100);
//         minimum = width;
//         natural = width;
//     } else {
//         int height = RTScalable::scalePixelSize(50);
//         minimum = height;
//         natural = height;
//     }
//
//     // Don't use baseline alignment
//     minimum_baseline = -1;
//     natural_baseline = -1;
// }
//
// /*
//  *
//  * Derived class of some widgets to properly handle the scroll wheel ;
//  * the user has to use the Shift key to be able to change the widget's value,
//  * otherwise the mouse wheel will scroll the toolbar.
//  *
//  */
// MyScrolledToolbar::MyScrolledToolbar()
// {
//     set_policy (Gtk::PolicyType::EXTERNAL, Gtk::PolicyType::NEVER);
//     get_style_context()->add_class("scrollableToolbar");
//
//     auto controller = Gtk::EventControllerScroll::create();
//     controller->set_flags(Gtk::EventControllerScroll::Flags::HORIZONTAL);
//     controller->signal_scroll().connect(
//         sigc::mem_fun(*this, &MyScrolledToolbar::onScroll), false);
//     add_controller(controller);
// }
//
// bool MyScrolledToolbar::onScroll(double dx, double /*dy*/)
// {
//     Glib::RefPtr<Gtk::Adjustment> adjust = get_hadjustment();
//     Gtk::Scrollbar* hscroll = get_hscrollbar();
//
//     if (adjust && hscroll) {
//         const double upperBound = adjust->get_upper();
//         const double lowerBound = adjust->get_lower();
//         const double value = adjust->get_value();
//         const double step = adjust->get_step_increment() * 2;
//
//         double newValue = value + step * dx;
//         newValue = std::clamp(newValue, lowerBound, upperBound);
//
//         if (newValue != value) {
//             hscroll->set_value(newValue);
//         }
//     }
//
//     return true;
// }
//
// void MyScrolledToolbar::measure_vfunc(Gtk::Orientation orientation, int /*for_size*/,
//                                       int& minimum, int& natural,
//                                       int& minimum_baseline, int& natural_baseline) const
// {
//     if (orientation == Gtk::Orientation::HORIZONTAL) {
//         int width = RTScalable::scalePixelSize(100);
//         minimum = width;
//         natural = width;
//     } else {
//         minimum = 0;
//         natural = 0;
//
//         for (const auto& child : get_children()) {
//             PreferredSize size = child->get_preferred_size();
//             minimum = std::max(minimum, size.minimum.get_height());
//             natural = std::max(natural, size.natural.get_height());
//         }
//     }
//
//     // Don't use baseline alignment
//     minimum_baseline = -1;
//     natural_baseline = -1;
// }

MyComboBoxText::MyComboBoxText (bool has_entry) : Gtk::ComboBoxText(has_entry)
{
    minimumWidth = naturalWidth = RTScalable::scalePixelSize(70);
    Gtk::CellRendererText* cellRenderer = dynamic_cast<Gtk::CellRendererText*>(get_first_cell());
    cellRenderer->property_ellipsize() = Pango::EllipsizeMode::MIDDLE;

    controller = Gtk::EventControllerScroll::create();
    using Flags = Gtk::EventControllerScroll::Flags;
    controller->set_flags(Flags::VERTICAL | Flags::DISCRETE);
    controller->signal_scroll().connect(
        sigc::mem_fun(*this, &MyComboBoxText::onScroll), false);
    add_controller(controller);
}

bool MyComboBoxText::onScroll(double /*dx*/, double /*dy*/)
{
    // If Shift is pressed, the widget is modified
    auto state = controller->get_current_event_state() & Gdk::ModifierType::SHIFT_MASK;
    if (state != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // TODO(gtk4): What is the equivalent in GTK4?
        // Gtk::ComboBoxText::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyComboBoxText::setPreferredWidth (int minimum_width, int natural_width)
{
    if (natural_width == -1 && minimum_width == -1) {
        naturalWidth = minimumWidth = RTScalable::scalePixelSize(70);
    } else if (natural_width == -1) {
        naturalWidth =  minimumWidth = minimum_width;
    } else if (minimum_width == -1) {
        naturalWidth = natural_width;
        minimumWidth = rtengine::max(naturalWidth / 2, RTScalable::scalePixelSize(20));
        minimumWidth = rtengine::min(naturalWidth, minimumWidth);
    } else {
        naturalWidth = natural_width;
        minimumWidth = minimum_width;
    }
}

void MyComboBoxText::measure_vfunc(Gtk::Orientation orientation, int for_size,
                                   int& minimum, int& natural,
                                   int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = std::max(minimumWidth, RTScalable::scalePixelSize(10));
        natural = std::max(naturalWidth, RTScalable::scalePixelSize(10));
        // Don't use baseline alignment
        minimum_baseline = -1;
        natural_baseline = -1;
    } else {
        Gtk::ComboBox::measure_vfunc(orientation, for_size, minimum, natural,
                                     minimum_baseline, natural_baseline);
    }
}

MyComboBox::MyComboBox ()
{
    minimumWidth = naturalWidth = RTScalable::scalePixelSize(70);

    controller = Gtk::EventControllerScroll::create();
    using Flags = Gtk::EventControllerScroll::Flags;
    controller->set_flags(Flags::VERTICAL | Flags::DISCRETE);
    controller->signal_scroll().connect(
        sigc::mem_fun(*this, &MyComboBox::onScroll), false);
    add_controller(controller);
}

bool MyComboBox::onScroll(double /*dx*/, double /*dy*/)
{
    // If Shift is pressed, the widget is modified
    auto state = controller->get_current_event_state() & Gdk::ModifierType::SHIFT_MASK;
    if (state != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // TODO(gtk4): What is the equivalent in GTK4?
        // Gtk::ComboBox::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyComboBox::setPreferredWidth (int minimum_width, int natural_width)
{
    if (natural_width == -1 && minimum_width == -1) {
        naturalWidth = minimumWidth = RTScalable::scalePixelSize(70);
    } else if (natural_width == -1) {
        naturalWidth =  minimumWidth = minimum_width;
    } else if (minimum_width == -1) {
        naturalWidth = natural_width;
        minimumWidth = rtengine::max(naturalWidth / 2, RTScalable::scalePixelSize(20));
        minimumWidth = rtengine::min(naturalWidth, minimumWidth);
    } else {
        naturalWidth = natural_width;
        minimumWidth = minimum_width;
    }
}

void MyComboBox::measure_vfunc(Gtk::Orientation orientation, int for_size,
                               int& minimum, int& natural,
                               int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        minimum = std::max(minimumWidth, RTScalable::scalePixelSize(10));
        natural = std::max(naturalWidth, RTScalable::scalePixelSize(10));
        // Don't use baseline alignment
        minimum_baseline = -1;
        natural_baseline = -1;
    } else {
        Gtk::ComboBox::measure_vfunc(orientation, for_size, minimum, natural,
                                     minimum_baseline, natural_baseline);
    }
}

MySpinButton::MySpinButton ()
{
    // TODO(gtk4)
    // Gtk::Border border;
    // border.set_bottom(0);
    // border.set_top(0);
    // border.set_left(3);
    // border.set_right(3);
    // set_inner_border(border);
    set_numeric(true);
    set_wrap(false);
    // TODO(gtk4)
    // set_alignment(Gtk::Align::END);
    set_update_policy(Gtk::SpinButton::UpdatePolicy::IF_VALID); // Avoid updating text if input is not a numeric

    auto keyPress = Gtk::EventControllerKey::create();
    keyPress->signal_key_pressed().connect(
        sigc::mem_fun(*this, &MySpinButton::onKeyPress), false);
    add_controller(keyPress);

    m_controller = Gtk::EventControllerScroll::create();
    using Flags = Gtk::EventControllerScroll::Flags;
    m_controller->set_flags(Flags::VERTICAL);
    m_controller->signal_scroll().connect(
        sigc::mem_fun(*this, &MySpinButton::onScroll), false);
    add_controller(m_controller);
}

void MySpinButton::updateSize()
{
    double vMin, vMax;
    int maxAbs;
    unsigned int digits, digits2;
    unsigned int maxLen;

    get_range(vMin, vMax);

    digits = get_digits();
    maxAbs = (int)(fmax(fabs(vMin), fabs(vMax)) + 0.000001);

    if (maxAbs == 0) {
        digits2 = 1;
    } else {
        digits2 = (int)(log10(double(maxAbs)) + 0.000001);
        digits2++;
    }

    maxLen = digits + digits2 + (vMin < 0 ? 1 : 0) + (digits > 0 ? 1 : 0);
    set_width_chars(maxLen);
    set_max_width_chars(maxLen);
}

bool MySpinButton::onKeyPress(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/)
{
    double vMin, vMax;
    get_range(vMin, vMax);

    if ((keyval >= GDK_KEY_a && keyval <= GDK_KEY_z)
            || (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z)
            || keyval == GDK_KEY_equal || keyval == GDK_KEY_underscore
            || keyval == GDK_KEY_plus || (keyval == GDK_KEY_minus && vMin >= 0)) {
        return false; // Event is propagated further
    } else {
        if (keyval == GDK_KEY_comma || keyval == GDK_KEY_KP_Decimal) {
            set_text(get_text() + ".");
            set_position(get_text().length()); // When setting text, cursor position is reset at text start. Avoiding this with this code
            return true; // Event is not propagated further
        }

        // TODO(gtk4)
        // return Gtk::SpinButton::on_key_press_event(event); // Event is propagated normally
        return false;
    }
}

bool MySpinButton::onScroll(double /*dx*/, double /*dy*/)
{
    // If Shift is pressed, the widget is modified
    auto state = m_controller->get_current_event_state() & Gdk::ModifierType::SHIFT_MASK;
    if (state != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // TODO(gtk4): What is the equivalent in GTK4?
        // Gtk::SpinButton::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

MyHScale::MyHScale()
{
    auto keyPress = Gtk::EventControllerKey::create();
    keyPress->signal_key_pressed().connect(
        sigc::mem_fun(*this, &MyHScale::onKeyPress), false);
    add_controller(keyPress);

    m_controller = Gtk::EventControllerScroll::create();
    using Flags = Gtk::EventControllerScroll::Flags;
    m_controller->set_flags(Flags::VERTICAL);
    m_controller->signal_scroll().connect(
        sigc::mem_fun(*this, &MyHScale::onScroll), false);
    add_controller(m_controller);
}

bool MyHScale::onScroll(double /*dx*/, double /*dy*/)
{
    // If Shift is pressed, the widget is modified
    auto state = m_controller->get_current_event_state() & Gdk::ModifierType::SHIFT_MASK;
    if (state != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // TODO(gtk4): What is the equivalent in GTK4?
        // Gtk::Scale::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

bool MyHScale::onKeyPress(guint keyval, guint /*keycode*/, Gdk::ModifierType /*state*/)
{
    if (keyval == GDK_KEY_plus || keyval == GDK_KEY_minus) {
        return false;
    } else {
        // TODO(gtk4)
        // return Gtk::Widget::on_key_press_event(event);
        return false;
    }
}

class MyFileChooserWidget::Impl
{
public:
    Impl(const Glib::ustring &title, Gtk::FileChooser::Action action) :
        title_(title),
        action_(action)
    {
    }

    Glib::ustring title_;
    Gtk::FileChooser::Action action_;
    Glib::RefPtr<Gio::File> filename_;
    Glib::RefPtr<Gio::File> current_folder_;
    std::vector<Glib::RefPtr<Gtk::FileFilter>> file_filters_;
    Glib::RefPtr<Gtk::FileFilter> cur_filter_;
    std::vector<Glib::RefPtr<Gio::File>> shortcut_folders_;
    bool show_hidden_{false};
    sigc::signal<void()> selection_changed_;
};


MyFileChooserWidget::MyFileChooserWidget(const Glib::ustring &title, Gtk::FileChooser::Action action) :
    pimpl(new Impl(title, action))
{
}


std::unique_ptr<Gtk::Image> MyFileChooserWidget::make_folder_image()
{
    return std::unique_ptr<Gtk::Image>(new RtImage("folder-open-small"));
}

void MyFileChooserWidget::show_chooser(Gtk::Widget *parent)
{
    auto dlg = Gtk::make_managed<Gtk::FileChooserDialog>(
        *getToplevelWindow(parent), pimpl->title_, pimpl->action_);
    dlg->add_button(M("GENERAL_CANCEL"), Gtk::ResponseType::CANCEL);
    dlg->add_button(M(pimpl->action_ == Gtk::FileChooser::Action::SAVE ? "GENERAL_SAVE" : "GENERAL_OPEN"), Gtk::ResponseType::OK);
    dlg->set_file(pimpl->filename_);
    for (auto &f : pimpl->file_filters_) {
        dlg->add_filter(f);
    }
    if (pimpl->cur_filter_) {
        dlg->set_filter(pimpl->cur_filter_);
    }
    for (auto &f : pimpl->shortcut_folders_) {
        dlg->add_shortcut_folder(f);
    }
    if (!pimpl->current_folder_) {
        dlg->set_current_folder(pimpl->current_folder_);
    }
    // dlg->set_show_hidden(pimpl->show_hidden_);
    dlg->set_modal(true);
    dlg->signal_response().connect([&, dlg](int res) {
        if (res == static_cast<int>(Gtk::ResponseType::OK)) {
            pimpl->filename_ = dlg->get_file();
            pimpl->current_folder_ = dlg->get_current_folder();
            on_filename_set();
            pimpl->selection_changed_.emit();
        }
        dlg->destroy();
    });
    dlg->present();
}


void MyFileChooserWidget::on_filename_set()
{
    // Sub-classes decide if anything needs to be done.
}


sigc::signal<void()> &MyFileChooserWidget::signal_selection_changed()
{
    return pimpl->selection_changed_;
}


sigc::signal<void()> &MyFileChooserWidget::signal_file_set()
{
    return pimpl->selection_changed_;
}


std::string MyFileChooserWidget::get_filename() const
{
    return pimpl->filename_ ? pimpl->filename_->get_path() : "";
}


bool MyFileChooserWidget::set_filename(const std::string &filename)
{
    pimpl->filename_ = Gio::File::create_for_path(filename);
    on_filename_set();
    return true;
}


void MyFileChooserWidget::add_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    pimpl->file_filters_.push_back(filter);
}


void MyFileChooserWidget::remove_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    auto it = std::find(pimpl->file_filters_.begin(), pimpl->file_filters_.end(), filter);
    if (it != pimpl->file_filters_.end()) {
        pimpl->file_filters_.erase(it);
    }
}


void MyFileChooserWidget::set_filter(const Glib::RefPtr<Gtk::FileFilter> &filter)
{
    pimpl->cur_filter_ = filter;
}


std::vector<Glib::RefPtr<Gtk::FileFilter>> MyFileChooserWidget::list_filters() const
{
    return pimpl->file_filters_;
}


bool MyFileChooserWidget::set_current_folder(const std::string &filename)
{
    pimpl->current_folder_ = Gio::File::create_for_path(filename);
    if (pimpl->action_ == Gtk::FileChooser::Action::SELECT_FOLDER) {
        set_filename(filename);
    }
    return true;
}

std::string MyFileChooserWidget::get_current_folder() const
{
    return pimpl->current_folder_->get_path();
}


bool MyFileChooserWidget::add_shortcut_folder(const std::string &folder)
{
    pimpl->shortcut_folders_.push_back(Gio::File::create_for_path(folder));
    return true;
}


bool MyFileChooserWidget::remove_shortcut_folder(const std::string &folder)
{
    auto handle = Gio::File::create_for_path(folder);
    auto it = std::find(pimpl->shortcut_folders_.begin(), pimpl->shortcut_folders_.end(), handle);
    if (it != pimpl->shortcut_folders_.end()) {
        pimpl->shortcut_folders_.erase(it);
    }
    return true;
}


void MyFileChooserWidget::unselect_all()
{
    pimpl->filename_ = nullptr;
    on_filename_set();
}


void MyFileChooserWidget::unselect_filename(const std::string &filename)
{
    if (pimpl->filename_->get_path() == filename) {
        unselect_all();
    }
}


void MyFileChooserWidget::set_show_hidden(bool yes)
{
    pimpl->show_hidden_ = yes;
}


class MyFileChooserButton::Impl
{
public:
    Gtk::Box box_;
    Gtk::Label lbl_{"", Gtk::Align::START};
};

MyFileChooserButton::MyFileChooserButton(const Glib::ustring &title, Gtk::FileChooser::Action action):
    MyFileChooserWidget(title, action),
    pimpl(new Impl())
{
    pimpl->lbl_.set_ellipsize(Pango::EllipsizeMode::MIDDLE);
    pimpl->lbl_.set_justify(Gtk::Justification::LEFT);
    on_filename_set();
    pack_start(&(pimpl->box_), pimpl->lbl_, true, true);
    pack_start(&(pimpl->box_), *Gtk::manage(new Gtk::Separator(Gtk::Orientation::VERTICAL)), false, false, 5);
    pack_start(&(pimpl->box_), *Gtk::manage(make_folder_image().release()), false, false);
    set_child(pimpl->box_);
    signal_clicked().connect([this]() {
        show_chooser(this);
    });

    set_name("MyFileChooserButton");

    m_controller = Gtk::EventControllerScroll::create();
    using Flags = Gtk::EventControllerScroll::Flags;
    m_controller->set_flags(Flags::VERTICAL | Flags::DISCRETE);
    m_controller->signal_scroll().connect(
        sigc::mem_fun(*this, &MyFileChooserButton::onScroll), false);
    add_controller(m_controller);
}

void MyFileChooserButton::on_filename_set()
{
    if (Glib::file_test(get_filename(), Glib::FileTest::EXISTS)) {
        pimpl->lbl_.set_label(Glib::path_get_basename(get_filename()));
    } else {
        pimpl->lbl_.set_label(Glib::ustring("(") + M("GENERAL_NONE") + ")");
    }
}

// For an unknown reason (a bug ?), it doesn't work when action = FILE_CHOOSER_ACTION_SELECT_FOLDER !
bool MyFileChooserButton::onScroll(double /*dx*/, double /*dy*/)
{
    // If Shift is pressed, the widget is modified
    auto state = m_controller->get_current_event_state() & Gdk::ModifierType::SHIFT_MASK;
    if (state != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // TODO(gtk4): What is the equivalent in GTK4?
        // Gtk::Button::on_scroll_event(event);
        return true;
    }

    // ... otherwise the scroll event is sent back to an upper level
    return false;
}

void MyFileChooserButton::measure_vfunc(Gtk::Orientation orientation, int for_size,
                                        int& minimum, int& natural,
                                        int& minimum_baseline, int& natural_baseline) const
{
    if (orientation == Gtk::Orientation::HORIZONTAL) {
        int width = RTScalable::scalePixelSize(35);
        minimum = width;
        natural = width;
        // Don't use baseline alignment
        minimum_baseline = -1;
        natural_baseline = -1;
    } else {
        Gtk::Button::measure_vfunc(orientation, for_size, minimum, natural,
                                   minimum_baseline, natural_baseline);
    }
}

class MyFileChooserEntry::Impl
{
public:
    Gtk::Entry entry;
    ImageLabelButton file_chooser_button;
};


MyFileChooserEntry::MyFileChooserEntry(const Glib::ustring &title, Gtk::FileChooser::Action action) :
    MyFileChooserWidget(title, action),
    pimpl(new Impl())
{
    const auto on_text_changed = [this]() {
        set_filename(pimpl->entry.get_text());
    };
    pimpl->entry.get_buffer()->signal_deleted_text().connect([on_text_changed](guint, guint) { on_text_changed(); });
    pimpl->entry.get_buffer()->signal_inserted_text().connect([on_text_changed](guint, const gchar *, guint) { on_text_changed(); });

    pimpl->file_chooser_button.set_image(*Gtk::manage(make_folder_image().release()));
    pimpl->file_chooser_button.signal_clicked().connect([this]() {
        const auto &filename = get_filename();
        if (Glib::file_test(filename, Glib::FileTest::IS_DIR)) {
            set_current_folder(filename);
        }
        show_chooser(this);
    });

    pack_start(this, pimpl->entry, true, true);
    pack_start(this, pimpl->file_chooser_button, false, false);
}


Glib::ustring MyFileChooserEntry::get_placeholder_text() const
{
    return pimpl->entry.get_placeholder_text();
}


void MyFileChooserEntry::set_placeholder_text(const Glib::ustring &text)
{
    pimpl->entry.set_placeholder_text(text);
}


void MyFileChooserEntry::on_filename_set()
{
    if (pimpl->entry.get_text() != get_filename().c_str()) {
        pimpl->entry.set_text(get_filename().c_str());
    }
}


// TextOrIcon::TextOrIcon (const Glib::ustring &icon_name, const Glib::ustring &labelTx, const Glib::ustring &tooltipTx)
// {
//
//     RtImage *img = Gtk::manage(new RtImage(icon_name));
//     pack_start(*img, Pack::SHRINK, 0);
//     set_tooltip_markup("<span font_size=\"large\" font_weight=\"bold\">" + labelTx  + "</span>\n" + tooltipTx);
//
//     set_name("TextOrIcon");
//     show_all();
//
// }

class ImageAndLabel::Impl
{
public:
    RtImage* image;
    Gtk::Label* label;

    Impl(RtImage* image, Gtk::Label* label) : image(image), label(label) {}
    static std::unique_ptr<RtImage> createImage(const Glib::ustring& iconName);
};

std::unique_ptr<RtImage> ImageAndLabel::Impl::createImage(const Glib::ustring& iconName)
{
    if (iconName.empty()) {
        return nullptr;
    }
    return std::unique_ptr<RtImage>(new RtImage(iconName));
}

ImageAndLabel::ImageAndLabel(const Glib::ustring& label, const Glib::ustring& iconName) :
    ImageAndLabel(label, Gtk::manage(Impl::createImage(iconName).release()))
{
}

ImageAndLabel::ImageAndLabel(const Glib::ustring& label, RtImage *image) :
    pimpl(new Impl(image, Gtk::manage(new Gtk::Label(label))))
{
    Gtk::Grid* grid = Gtk::manage(new Gtk::Grid());
    grid->set_orientation(Gtk::Orientation::HORIZONTAL);

    if (image) {
        grid->attach_next_to(*image, Gtk::PositionType::LEFT, 1, 1);
    }

    grid->attach_next_to(*(pimpl->label), Gtk::PositionType::RIGHT, 1, 1);
    grid->set_column_spacing(4);
    grid->set_row_spacing(0);
    pack_start(this, *grid, Pack::SHRINK, 0);
}

const RtImage* ImageAndLabel::getImage() const
{
    return pimpl->image;
}

const Gtk::Label* ImageAndLabel::getLabel() const
{
    return pimpl->label;
}

ImageLabelButton::ImageLabelButton()
{
    m_box.append(m_label);
    set_child(m_box);
}

ImageLabelButton::ImageLabelButton(const Glib::ustring& text) : m_label(text)
{
    m_box.append(m_label);
    set_child(m_box);
}

void ImageLabelButton::set_image(Gtk::Image& image)
{
    m_box.prepend(image);
}

// class MyImageMenuItem::Impl
// {
// private:
//     std::unique_ptr<ImageAndLabel> widget;
//
// public:
//     Impl(const Glib::ustring &label, const Glib::ustring &iconName) :
//         widget(new ImageAndLabel(label, iconName)) {}
//     Impl(const Glib::ustring &label, RtImage *itemImage) :
//         widget(new ImageAndLabel(label, itemImage)) {}
//     ImageAndLabel* getWidget() const { return widget.get(); }
// };
//
// MyImageMenuItem::MyImageMenuItem(const Glib::ustring& label, const Glib::ustring& iconName) :
//     pimpl(new Impl(label, iconName))
// {
//     add(*(pimpl->getWidget()));
// }
//
// MyImageMenuItem::MyImageMenuItem(const Glib::ustring& label, RtImage* itemImage) :
//     pimpl(new Impl(label, itemImage))
// {
//     add(*(pimpl->getWidget()));
// }
//
// const RtImage *MyImageMenuItem::getImage () const
// {
//     return pimpl->getWidget()->getImage();
// }
//
// const Gtk::Label* MyImageMenuItem::getLabel () const
// {
//     return pimpl->getWidget()->getLabel();
// }
//
// class MyRadioImageMenuItem::Impl
// {
//     std::unique_ptr<ImageAndLabel> widget;
//
// public:
//     Impl(const Glib::ustring &label, RtImage *image) :
//         widget(new ImageAndLabel(label, image)) {}
//     ImageAndLabel* getWidget() const { return widget.get(); }
// };
//
// MyRadioImageMenuItem::MyRadioImageMenuItem(const Glib::ustring& label, RtImage *image, Gtk::RadioButton::Group& group) :
//     Gtk::RadioMenuItem(group),
//     pimpl(new Impl(label, image))
// {
//     add(*(pimpl->getWidget()));
// }
//
// const Gtk::Label* MyRadioImageMenuItem::getLabel() const
// {
//     return pimpl->getWidget()->getLabel();
// }
//
// MyProgressBar::MyProgressBar(int width) : w(rtengine::max(width, RTScalable::scalePixelSize(10))) {}
// MyProgressBar::MyProgressBar() : w(RTScalable::scalePixelSize(200)) {}
//
// void MyProgressBar::setPreferredWidth(int width)
// {
//     w = rtengine::max(width, RTScalable::scalePixelSize(10));
// }
//
// void MyProgressBar::measure_vfunc(Gtk::Orientation orientation, int for_size,
//                                   int& minimum, int& natural,
//                                   int& minimum_baseline, int& natural_baseline) const
// {
//     if (orientation == Gtk::Orientation::HORIZONTAL) {
//         int scaled = RTScalable::scalePixelSize(50);
//         minimum = std::max(w / 2, scaled);
//         natural = std::max(w, scaled);
//         // Don't use baseline alignment
//         minimum_baseline = -1;
//         natural_baseline = -1;
//     } else {
//         Gtk::ComboBox::measure_vfunc(orientation, for_size, minimum, natural,
//                                      minimum_baseline, natural_baseline);
//     }
// }
//
// BackBuffer::BackBuffer() : x(0), y(0), w(0), h(0), offset(0, 0), dirty(true) {}
// BackBuffer::BackBuffer(int width, int height, Cairo::Format format) : x(0), y(0), w(width), h(height), offset(0, 0), dirty(true)
// {
//     if (w > 0 && h > 0) {
//         surface = Cairo::ImageSurface::create(format, w, h);
//     } else {
//         w = h = 0;
//     }
// }
//
// void BackBuffer::setDestPosition(int x, int y)
// {
//     // values will be clamped when used...
//     this->x = x;
//     this->y = y;
// }
//
// void BackBuffer::setSrcOffset(int x, int y)
// {
//     // values will be clamped when used...
//     offset.set(x, y);
// }
//
// void BackBuffer::setSrcOffset(const rtengine::Coord &newOffset)
// {
//     // values will be clamped when used...
//     offset = newOffset;
// }
//
// void BackBuffer::getSrcOffset(int &x, int &y)
// {
//     // values will be clamped when used...
//     offset.get(x, y);
// }
//
// void BackBuffer::getSrcOffset(rtengine::Coord &offset)
// {
//     // values will be clamped when used...
//     offset = this->offset;
// }
//
// // Note: newW & newH must be > 0
// bool BackBuffer::setDrawRectangle(Glib::RefPtr<Gtk::Window> window, Gdk::Rectangle &rectangle, bool updateBackBufferSize)
// {
//     return setDrawRectangle(window, rectangle.get_x(), rectangle.get_y(), rectangle.get_width(), rectangle.get_height(), updateBackBufferSize);
// }
//
// // Note: newW & newH must be > 0
// bool BackBuffer::setDrawRectangle(Glib::RefPtr<Gtk::Window> window, int newX, int newY, int newW, int newH, bool updateBackBufferSize)
// {
//     assert(newW && newH);
//
//     bool newSize = (newW > 0 && w != newW) || (newH > 0 && h != newH);
//
//     x = newX;
//     y = newY;
//     if (newW > 0) {
//         w = newW;
//     }
//     if (newH > 0) {
//         h = newH;
//     }
//
//     // WARNING: we're assuming that the surface type won't change during all the execution time of RT. I guess it may be wrong when the user change the gfx card display settings!?
//     if (((updateBackBufferSize && newSize) || !surface) && window) {
//         // allocate a new Surface
//         surface.clear();  // ... don't know if this is necessary?
//         surface = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, w, h);
//         dirty = true;
//     }
//
//     return dirty;
// }
//
// // Note: newW & newH must be > 0
// bool BackBuffer::setDrawRectangle(Cairo::Format format, Gdk::Rectangle &rectangle, bool updateBackBufferSize)
// {
//     return setDrawRectangle(format, rectangle.get_x(), rectangle.get_y(), rectangle.get_width(), rectangle.get_height(), updateBackBufferSize);
// }
//
// // Note: newW & newH must be > 0
// bool BackBuffer::setDrawRectangle(Cairo::Format format, int newX, int newY, int newW, int newH, bool updateBackBufferSize)
// {
//     assert(newW && newH);
//
//     bool newSize = (newW > 0 && w != newW) || (newH > 0 && h != newH);
//
//     x = newX;
//     y = newY;
//     if (newW > 0) {
//         w = newW;
//     }
//     if (newH > 0) {
//         h = newH;
//     }
//
//     // WARNING: we're assuming that the surface type won't change during all the execution time of RT. I guess it may be wrong when the user change the gfx card display settings!?
//     if ((updateBackBufferSize && newSize) || !surface) {
//         // allocate a new Surface
//         surface.clear();  // ... don't know if this is necessary?
//         surface = Cairo::ImageSurface::create(format, w, h);
//         dirty = true;
//     }
//
//     return dirty;
// }
//
// /*
//  * Copy uint8 RGB raw data to an ImageSurface. We're assuming that the source contains enough data for the given srcX, srcY, srcW, srcH -> no error checking!
//  */
// void BackBuffer::copyRGBCharData(const unsigned char *srcData, int srcX, int srcY, int srcW, int srcH, int srcRowStride, int dstX, int dstY)
// {
//     unsigned char r, g, b;
//
//     if (!surface) {
//         return;
//     }
//
//     //printf("copyRGBCharData:    src: (X:%d Y:%d, W:%d H:%d)  /  dst: (X: %d Y:%d)\n", srcX, srcY, srcW, srcH, dstX, dstY);
//
//     unsigned char *dstData = surface->get_data();
//     int surfW = surface->get_width();
//     int surfH = surface->get_height();
//
//     if (!srcData || dstX >= surfW || dstY >= surfH || srcW <= 0 || srcH <= 0 || srcX < 0 || srcY < 0) {
//         return;
//     }
//
//     for (int i = 0; i < srcH; ++i) {
//         if (dstY + i >= surfH) {
//             break;
//         }
//
//         const unsigned char *src = srcData + i * srcRowStride;
//         unsigned char *dst = dstData + ((dstY + i) * surfW + dstX) * 4;
//
//         for (int j = 0; j < srcW; ++j) {
//             if (dstX + j >= surfW) {
//                 break;
//             }
//
//             r = *(src++);
//             g = *(src++);
//             b = *(src++);
//
//             rtengine::poke255_uc(dst, r, g, b);
//         }
//     }
//
//     surface->mark_dirty();
//
// }
//
// /*
//  * Copy the backbuffer to a Gtk::Window
//  */
// void BackBuffer::copySurface(Glib::RefPtr<Gtk::Window> window, Gdk::Rectangle *destRectangle)
// {
//     if (surface && window) {
//         // TODO: look out if window can be different on each call, and if not, store a reference to the window
//         Cairo::RefPtr<Cairo::Context> crSrc = window->create_cairo_context();
//         Cairo::RefPtr<Cairo::Surface> destSurface = crSrc->get_target();
//
//         // compute the source offset
//         int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
//         int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());
//
//         // now copy the off-screen Surface to the destination Surface
//         Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
//         crDest->set_line_width(0.);
//
//         if (destRectangle) {
//             crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
//             int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
//             int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
//             //printf("BackBuffer::copySurface / rectangle1(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             //printf("BackBuffer::copySurface / rectangle1\n");
//         } else {
//             crDest->set_source(surface, -offsetX + x, -offsetY + y);
//             //printf("BackBuffer::copySurface / rectangle2(%d, %d, %d, %d)\n", x, y, w, h);
//             crDest->rectangle(x, y, w, h);
//             //printf("BackBuffer::copySurface / rectangle2\n");
//         }
//
//         crDest->fill();
//     }
// }
//
// /*
//  * Copy the BackBuffer to another BackBuffer
//  */
// void BackBuffer::copySurface(BackBuffer *destBackBuffer, Gdk::Rectangle *destRectangle)
// {
//     if (surface && destBackBuffer) {
//         Cairo::RefPtr<Cairo::ImageSurface> destSurface = destBackBuffer->getSurface();
//
//         if (!destSurface) {
//             return;
//         }
//
//         // compute the source offset
//         int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
//         int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());
//
//         // now copy the off-screen Surface to the destination Surface
//         Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
//         crDest->set_line_width(0.);
//
//         if (destRectangle) {
//             crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
//             int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
//             int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
//             //printf("BackBuffer::copySurface / rectangle3(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             //printf("BackBuffer::copySurface / rectangle3\n");
//         } else {
//             crDest->set_source(surface, -offsetX + x, -offsetY + y);
//             //printf("BackBuffer::copySurface / rectangle4(%d, %d, %d, %d)\n", x, y, w, h);
//             crDest->rectangle(x, y, w, h);
//             //printf("BackBuffer::copySurface / rectangle4\n");
//         }
//
//         crDest->fill();
//     }
// }
//
// /*
//  * Copy the BackBuffer to another Cairo::Surface
//  */
// void BackBuffer::copySurface(Cairo::RefPtr<Cairo::ImageSurface> destSurface, Gdk::Rectangle *destRectangle)
// {
//     if (surface && destSurface) {
//         // compute the source offset
//         int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
//         int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());
//
//         // now copy the off-screen Surface to the destination Surface
//         Cairo::RefPtr<Cairo::Context> crDest = Cairo::Context::create(destSurface);
//         crDest->set_line_width(0.);
//
//         if (destRectangle) {
//             crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
//             int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
//             int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
//             //printf("BackBuffer::copySurface / rectangle5(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             //printf("BackBuffer::copySurface / rectangle5\n");
//         } else {
//             crDest->set_source(surface, -offsetX + x, -offsetY + y);
//             //printf("BackBuffer::copySurface / rectangle6(%d, %d, %d, %d)\n", x, y, w, h);
//             crDest->rectangle(x, y, w, h);
//             //printf("BackBuffer::copySurface / rectangle6\n");
//         }
//
//         crDest->fill();
//     }
// }
//
// /*
//  * Copy the BackBuffer to another Cairo::Surface
//  */
// void BackBuffer::copySurface(Cairo::RefPtr<Cairo::Context> crDest, Gdk::Rectangle *destRectangle)
// {
//     if (surface && crDest) {
//         // compute the source offset
//         int offsetX = rtengine::LIM<int>(offset.x, 0, surface->get_width());
//         int offsetY = rtengine::LIM<int>(offset.y, 0, surface->get_height());
//
//         // now copy the off-screen Surface to the destination Surface
//         // int srcSurfW = surface->get_width();
//         // int srcSurfH = surface->get_height();
//         //printf("srcSurf:  w: %d, h: %d\n", srcSurfW, srcSurfH);
//         crDest->set_line_width(0.);
//
//         if (destRectangle) {
//             crDest->set_source(surface, -offsetX + destRectangle->get_x(), -offsetY + destRectangle->get_y());
//             int w_ = destRectangle->get_width() > 0 ? destRectangle->get_width() : w;
//             int h_ = destRectangle->get_height() > 0 ? destRectangle->get_height() : h;
//             //printf("BackBuffer::copySurface / rectangle7(%d, %d, %d, %d)\n", destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             crDest->rectangle(destRectangle->get_x(), destRectangle->get_y(), w_, h_);
//             //printf("BackBuffer::copySurface / rectangle7\n");
//         } else {
//             crDest->set_source(surface, -offsetX + x, -offsetY + y);
//             //printf("BackBuffer::copySurface / rectangle8(%d, %d, %d, %d)\n", x, y, w, h);
//             crDest->rectangle(x, y, w, h);
//             //printf("BackBuffer::copySurface / rectangle8\n");
//         }
//
//         crDest->fill();
//     }
// }
//
// SpotPicker::SpotPicker(int const defaultValue, Glib::ustring const &buttonKey, Glib::ustring const &buttonTooltip, Glib::ustring const &labelKey) :
//     Gtk::Grid(),
//     _spotHalfWidth(defaultValue),
//     _spotLabel(labelSetup(labelKey)),
//     _spotSizeSetter(MyComboBoxText(selecterSetup())),
//     _spotButton(spotButtonTemplate(buttonKey, buttonTooltip))
//
// {
//     this->get_style_context()->add_class("grid-spacing");
//     setExpandAlignProperties(this, true, false, Gtk::Align::FILL, Gtk::Align::CENTER);
//
//     this->attach (_spotButton, 0, 0, 1, 1);
//     this->attach (_spotLabel, 1, 0, 1, 1);
//     this->attach (_spotSizeSetter, 2, 0, 1, 1);
//     _spotSizeSetter.signal_changed().connect( sigc::mem_fun(*this, &SpotPicker::spotSizeChanged));
// }
//
// Gtk::Label SpotPicker::labelSetup(Glib::ustring const &key) const
// {
//     Gtk::Label label(key);
//     setExpandAlignProperties(&label, false, false, Gtk::Align::START, Gtk::Align::CENTER);
//     return label;
// }
//
// MyComboBoxText SpotPicker::selecterSetup() const
// {
//     MyComboBoxText spotSize = MyComboBoxText();
//     setExpandAlignProperties(&spotSize, false, false, Gtk::Align::START, Gtk::Align::CENTER);
//
//     spotSize.append ("2");
//     if (_spotHalfWidth == 2) {
//         spotSize.set_active(0);
//     }
//
//     spotSize.append ("4");
//
//     if (_spotHalfWidth == 4) {
//         spotSize.set_active(1);
//     }
//
//     spotSize.append ("8");
//
//     if (_spotHalfWidth == 8) {
//         spotSize.set_active(2);
//     }
//
//     spotSize.append ("16");
//
//     if (_spotHalfWidth == 16) {
//         spotSize.set_active(3);
//     }
//
//     spotSize.append ("32");
//
//     if (_spotHalfWidth == 32) {
//         spotSize.set_active(4);
//     }
//     return spotSize;
// }
//
// Gtk::ToggleButton SpotPicker::spotButtonTemplate(Glib::ustring const &key, const Glib::ustring &tooltip) const
// {
//     Gtk::ToggleButton spotButton = Gtk::ToggleButton(key);
//     setExpandAlignProperties(&spotButton, true, false, Gtk::Align::FILL, Gtk::Align::CENTER);
//     spotButton.get_style_context()->add_class("independent");
//     spotButton.set_tooltip_text(tooltip);
//     spotButton.set_image_from_icon_name("color-picker-small");
//     return spotButton;
// }
//
// void SpotPicker::spotSizeChanged()
// {
//     _spotHalfWidth = atoi(_spotSizeSetter.get_active_text().c_str());
// }
//
// // OptionalRadioButtonGroup class
//
// void OptionalRadioButtonGroup::onButtonToggled(Gtk::ToggleButton *button)
// {
//     if (!button) {
//         return;
//     }
//
//     if (button->get_active()) {
//         if (active_button == button) {
//             // Same button, noting to do.
//         } else if (active_button) {
//             // Deactivate the other button.
//             active_button->set_active(false);
//         }
//         active_button = button;
//     } else {
//         if (active_button == button) {
//             // Active button got deactivated.
//             active_button = nullptr;
//         } else {
//             // No effect on other buttons.
//         }
//     }
// }
//
// Gtk::ToggleButton *OptionalRadioButtonGroup::getActiveButton() const
// {
//     return active_button;
// }
//
// void OptionalRadioButtonGroup::register_button(Gtk::ToggleButton &button)
// {
//     button.signal_toggled().connect(sigc::bind(
//         sigc::mem_fun(this, &OptionalRadioButtonGroup::onButtonToggled),
//         &button));
//     onButtonToggled(&button);
// }

RotateLabel::RotateLabel() : m_rotate90(false)
{
    m_label.set_parent(*this);
}

RotateLabel::RotateLabel(const Glib::ustring& text) : m_label(text), m_rotate90(false)
{
    m_label.set_parent(*this);
}

void RotateLabel::rotate90(bool val)
{
    m_rotate90 = val;
    queue_allocate();
}

void RotateLabel::size_allocate_vfunc(int width, int height, int baseline)
{
    if (m_rotate90) {
        Gtk::Allocation alloc(0, 0, height, width);
        m_label.size_allocate(alloc, -1);
    } else {
        Gtk::Allocation alloc(0, 0, width, height);
        m_label.size_allocate(alloc, -1);
    }
}

Gtk::SizeRequestMode RotateLabel::get_request_mode_vfunc() const
{
    return m_rotate90 ?
        Gtk::SizeRequestMode::WIDTH_FOR_HEIGHT :
        Gtk::SizeRequestMode::HEIGHT_FOR_WIDTH;
}

void RotateLabel::measure_vfunc(Gtk::Orientation orientation, int for_size,
                                int& minimum, int& natural,
                                int& minimum_baseline, int& natural_baseline) const
{
    auto rotated_orient = orientation;
    if (m_rotate90) {
        if (rotated_orient == Gtk::Orientation::HORIZONTAL) {
            rotated_orient = Gtk::Orientation::VERTICAL;
        } else {
            rotated_orient = Gtk::Orientation::HORIZONTAL;
        }
    }

    int dummy1 = 0;
    int dummy2 = 0;
    m_label.measure(rotated_orient, for_size, minimum, natural,
                    dummy1, dummy2);

    minimum_baseline = -1;
    natural_baseline = -1;
}

void RotateLabel::snapshot_vfunc(const Glib::RefPtr<Gtk::Snapshot>& snapshot)
{
    if (m_rotate90) {
        std::optional<Gdk::Graphene::Rect> bounds = compute_bounds(*this);
        double h = bounds->get_height();
        snapshot->translate(Gdk::Graphene::Point(0.0, h));
        snapshot->rotate(-90);
    }
    snapshot_child(m_label, snapshot);
}
