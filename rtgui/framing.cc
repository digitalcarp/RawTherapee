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
 * 
 *  2024-2024 Daniel Gao <daniel.gao.work@gmail.com>
 */

#include "framing.h"

#include "aspectratios.h"
#include "resize.h"

#include <array>

namespace {

// Framing method combo box data
constexpr int INDEX_STANDARD = 0;
constexpr int INDEX_BBOX = 1;
constexpr int INDEX_FIXED = 2;
constexpr std::array<const char*, 3> FRAMING_METHODS = {
    "TP_FRAMING_METHOD_STANDARD",
    "TP_FRAMING_METHOD_BBOX",
    "TP_FRAMING_METHOD_FIXED"
};

// Orientation combo box data
constexpr int INDEX_AS_IMAGE = 0;
constexpr int INDEX_LANDSCAPE = 1;
constexpr int INDEX_PORTRAIT = 2;
constexpr std::array<const char*, 3> ORIENTATION = {
    "GENERAL_ASIMAGE",
    "GENERAL_LANDSCAPE",
    "GENERAL_PORTRAIT"
};

// Border sizing method combo box data
constexpr int INDEX_SIZE_RELATIVE = 0;
constexpr int INDEX_SIZE_ABSOLUTE = 1;
constexpr std::array<const char*, 2> BORDER_SIZE_METHODS = {
    "TP_FRAMING_BORDER_SIZE_RELATIVE",
    "TP_FRAMING_BORDER_SIZE_ABSOLUTE"
};

// Relative sizing basis combo box data
constexpr int INDEX_BASIS_AUTO = 0;
constexpr int INDEX_BASIS_WIDTH = 1;
constexpr int INDEX_BASIS_HEIGHT = 2;
constexpr int INDEX_BASIS_LONG = 3;
constexpr int INDEX_BASIS_SHORT = 4;
constexpr std::array<const char*, 5> BORDER_SIZE_BASIS = {
    "TP_FRAMING_BASIS_AUTO",
    "TP_FRAMING_BASIS_WIDTH",
    "TP_FRAMING_BASIS_HEIGHT",
    "TP_FRAMING_BASIS_LONG_SIDE",
    "TP_FRAMING_BASIS_SHORT_SIDE"
};

constexpr int INITIAL_IMG_WIDTH = 800;
constexpr int INITIAL_IMG_HEIGHT = 600;

constexpr int ROW_SPACING = 4;
constexpr float FRAME_LABEL_ALIGN_X = 0.025;
constexpr float FRAME_LABEL_ALIGN_Y = 0.5;

Gtk::Label* createGridLabel(const char* text)
{
    Gtk::Label* label = Gtk::manage(new Gtk::Label(M(text)));
    label->set_halign(Gtk::ALIGN_START);
    return label;
}

MySpinButton* createSpinButton()
{
    MySpinButton* button = Gtk::manage(new MySpinButton());
    button->set_width_chars(5);
    button->set_digits(0);
    button->set_increments(1, 100);
    setExpandAlignProperties(button, false, false, Gtk::ALIGN_END, Gtk::ALIGN_CENTER);
    return button;
}

}  // namespace

const Glib::ustring Framing::TOOL_NAME = "framing";

class Framing::AspectRatios
{
public:
    static constexpr int INDEX_CURRENT = 0;

    AspectRatios() :
        ratios{{M("GENERAL_CURRENT")}}
    {
        fillAspectRatios(ratios);
    }

    void fillCombo(MyComboBoxText* combo)
    {
        for (const auto& aspectRatio : ratios) {
            combo->append(aspectRatio.label);
        }
        combo->set_active(INDEX_CURRENT);
    }

private:
    std::vector<AspectRatio> ratios;
};

Framing::DimensionGui::DimensionGui(Gtk::Box* parent, const char* text)
{
    box = Gtk::manage(new Gtk::Box());
    Gtk::Label* label = Gtk::manage(new Gtk::Label(M(text)));
    setExpandAlignProperties(label, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    value = createSpinButton();
    box->pack_start(*label);
    box->pack_start(*value);
    parent->pack_start(*box);
}

void Framing::DimensionGui::connect(Framing& framing, CallbackFunc callback)
{
    connection = value->signal_value_changed().connect(sigc::mem_fun(framing, callback), true);
}

Framing::Framing() :
    FoldableToolPanel(this, TOOL_NAME, M("TP_FRAMING_LABEL"), false, true),
    aspectRatioData(new AspectRatios),
    imgWidth(INITIAL_IMG_WIDTH),
    imgHeight(INITIAL_IMG_HEIGHT)
{
    setupFramingMethodGui();
    pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)));
    setupBorderSizeGui();
    pack_start(*Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL)));
    setupBorderColorsGui();
}

Framing::~Framing() {
    idleRegister.destroy();
}

void Framing::setupFramingMethodGui()
{
    Gtk::Grid* combos = Gtk::manage(new Gtk::Grid());
    combos->set_row_spacing(ROW_SPACING);

    framingMethod = Gtk::manage(new MyComboBoxText());
    for (auto label : FRAMING_METHODS) {
        framingMethod->append(M(label));
    }
    framingMethod->set_active(INDEX_STANDARD);
    framingMethod->set_hexpand();
    framingMethod->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*createGridLabel("TP_FRAMING_METHOD"), 0, 0);
    combos->attach(*framingMethod, 1, 0);

    aspectRatioLabel = createGridLabel("TP_FRAMING_ASPECT_RATIO");
    aspectRatio = Gtk::manage(new MyComboBoxText());
    aspectRatioData->fillCombo(aspectRatio);
    aspectRatio->set_hexpand();
    aspectRatio->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*aspectRatioLabel, 0, 1);
    combos->attach(*aspectRatio, 1, 1);

    orientationLabel = createGridLabel("TP_FRAMING_ORIENTATION");
    orientation = Gtk::manage(new MyComboBoxText());
    for (auto label : ORIENTATION) {
        orientation->append(M(label));
    }
    orientation->set_active(INDEX_AS_IMAGE);
    orientation->set_hexpand();
    orientation->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*orientationLabel, 0, 2);
    combos->attach(*orientation, 1, 2);
    pack_start(*combos);

    width = DimensionGui(this, "TP_FRAMING_FRAMED_WIDTH");
    width.setRange(Resize::MIN_SIZE, Resize::MAX_SCALE * imgWidth);
    width.setValue(imgWidth);
    height = DimensionGui(this, "TP_FRAMING_FRAMED_HEIGHT");
    height.setRange(Resize::MIN_SIZE, Resize::MAX_SCALE * imgHeight);
    height.setValue(imgHeight);

    allowUpscaling = Gtk::manage(new Gtk::CheckButton(M("TP_FRAMING_ALLOW_UPSCALING")));
    pack_start(*allowUpscaling);

    updateFramingMethodGui();

    framingMethodChanged = framingMethod->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onFramingMethodChanged));
    aspectRatioChanged = aspectRatio->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onAspectRatioChanged));
    orientationChanged = orientation->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onOrientationChanged));
    width.connect(*this, &Framing::onWidthChanged);
    height.connect(*this, &Framing::onHeightChanged);
    allowUpscalingConnection = allowUpscaling->signal_toggled().connect(
        sigc::mem_fun(*this, &Framing::onAllowUpscalingToggled));
}

void Framing::setupBorderSizeGui()
{
    Gtk::Grid* combos = Gtk::manage(new Gtk::Grid());
    combos->set_row_spacing(ROW_SPACING);

    borderSizeMethod = Gtk::manage(new MyComboBoxText());
    for (auto label : BORDER_SIZE_METHODS) {
        borderSizeMethod->append(M(label));
    }
    borderSizeMethod->set_active(INDEX_SIZE_RELATIVE);
    borderSizeMethod->set_hexpand();
    borderSizeMethod->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*createGridLabel("TP_FRAMING_BORDER_SIZE_METHOD"), 0, 0);
    combos->attach(*borderSizeMethod, 1, 0);

    basisLabel = createGridLabel("TP_FRAMING_BASIS");
    basis = Gtk::manage(new MyComboBoxText());
    for (auto label : BORDER_SIZE_BASIS) {
        basis->append(M(label));
    }
    basis->set_active(INDEX_BASIS_AUTO);
    basis->set_hexpand();
    basis->set_halign(Gtk::ALIGN_FILL);

    combos->attach(*basisLabel, 0, 1);
    combos->attach(*basis, 1, 1);

    pack_start(*combos);

    relativeBorderSize = Gtk::manage(new Adjuster(M("TP_FRAMING_BORDER_SIZE"), 0, 1, 0.01, 0.1));
    pack_start(*relativeBorderSize);

    minSizeFrame = Gtk::manage(new Gtk::Frame());
    minSizeFrame->set_label_align(FRAME_LABEL_ALIGN_X, FRAME_LABEL_ALIGN_Y);
    minSizeEnabled = Gtk::manage(new Gtk::CheckButton(M("TP_FRAMING_LIMIT_MINIMUM")));
    minSizeFrame->set_label_widget(*minSizeEnabled);

    minSizeFrameContent = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));

    minWidth = DimensionGui(minSizeFrameContent, "TP_FRAMING_MIN_WIDTH");
    minWidth.setRange(0, imgWidth);
    minWidth.setValue(0);
    minHeight = DimensionGui(minSizeFrameContent, "TP_FRAMING_MIN_HEIGHT");
    minHeight.setRange(0, imgHeight);
    minHeight.setValue(0);

    minSizeFrame->add(*minSizeFrameContent);
    pack_start(*minSizeFrame);

    absWidth = DimensionGui(this, "TP_FRAMING_ABSOLUTE_WIDTH");
    absWidth.setRange(0, imgWidth);
    absWidth.setValue(0);
    absHeight = DimensionGui(this, "TP_FRAMING_ABSOLUTE_HEIGHT");
    absHeight.setRange(0, imgHeight);
    absHeight.setValue(0);

    updateBorderSizeGui();

    borderSizeMethodChanged = borderSizeMethod->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onBorderSizeMethodChanged));
    basisChanged = basis->signal_changed().connect(
        sigc::mem_fun(*this, &Framing::onBasisChanged));
    relativeBorderSize->setAdjusterListener(this);
    minSizeEnabledConnection = minSizeEnabled->signal_toggled().connect(
        sigc::mem_fun(*this, &Framing::onMinSizeToggled));
    minWidth.connect(*this, &Framing::onMinWidthChanged);
    minHeight.connect(*this, &Framing::onMinHeightChanged);
    absWidth.connect(*this, &Framing::onAbsWidthChanged);
    absHeight.connect(*this, &Framing::onAbsHeightChanged);
}

void Framing::setupBorderColorsGui()
{
    Gtk::Frame* const frame = Gtk::manage(new Gtk::Frame());

    Gtk::Label* const label = Gtk::manage(new Gtk::Label(M("TP_FRAMING_BORDER_COLOR")));
    frame->set_label_align(FRAME_LABEL_ALIGN_X, FRAME_LABEL_ALIGN_Y);
    frame->set_label_widget(*label);

    Gtk::Box* const box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    redAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_RED"), 0, 255, 1, 255));
    box->add(*redAdj);
    greenAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_GREEN"), 0, 255, 1, 255));
    box->add(*greenAdj);
    blueAdj = Gtk::manage(new Adjuster(M("TP_FRAMING_BLUE"), 0, 255, 1, 255));
    box->add(*blueAdj);

    frame->add(*box);
    pack_start(*frame);

    redAdj->setAdjusterListener(this);
    greenAdj->setAdjusterListener(this);
    blueAdj->setAdjusterListener(this);
}

void Framing::read(const rtengine::procparams::ProcParams* pp, const ParamsEdited* pedited)
{
}

void Framing::write(rtengine::procparams::ProcParams* pp, ParamsEdited* pedited)
{
}

void Framing::setDefaults(const rtengine::procparams::ProcParams* defParams, const ParamsEdited* pedited)
{
}

void Framing::setBatchMode(bool batchMode)
{
    ToolPanel::setBatchMode(batchMode);
}

void Framing::update(bool isCropped, int croppedWidth, int croppedHeight,
                     int originalWidth, int originalHeight)
{
    if (originalWidth && originalHeight) {
        imgWidth = originalWidth;
        imgHeight = originalHeight;
    }

    setDimensions();
}

void Framing::setDimensions()
{
    idleRegister.add([this]() -> bool {
        width.value->set_range(Resize::MIN_SIZE, Resize::MAX_SCALE * imgWidth);
        height.value->set_range(Resize::MIN_SIZE, Resize::MAX_SCALE * imgHeight);

        return false;
    });
}

void Framing::updateFramingMethodGui()
{
    if (batchMode) return;

    int activeRow = framingMethod->get_active_row_number();
    if (activeRow == INDEX_STANDARD) {
        aspectRatioLabel->show();
        aspectRatio->show();
        orientationLabel->show();
        orientation->show();
        width.hide();
        height.hide();
        allowUpscaling->hide();
    } else if (activeRow == INDEX_BBOX) {
        aspectRatioLabel->show();
        aspectRatio->show();
        orientationLabel->show();
        orientation->show();
        width.show();
        height.show();
        allowUpscaling->hide();
    } else if (activeRow == INDEX_FIXED) {
        aspectRatioLabel->hide();
        aspectRatio->hide();
        orientationLabel->hide();
        orientation->hide();
        width.show();
        height.show();
        allowUpscaling->show();
    }
}

void Framing::updateBorderSizeGui()
{
    if (batchMode) return;

    int activeRow = borderSizeMethod->get_active_row_number();
    if (activeRow == INDEX_SIZE_RELATIVE) {
        basisLabel->show();
        basis->show();
        relativeBorderSize->show();
        minSizeFrame->show();
        absWidth.hide();
        absHeight.hide();
    } else if (activeRow == INDEX_SIZE_ABSOLUTE) {
        basisLabel->hide();
        basis->hide();
        relativeBorderSize->hide();
        minSizeFrame->hide();
        absWidth.show();
        absHeight.show();
    }

    minSizeFrameContent->set_sensitive(minSizeEnabled->get_active());
}

void Framing::adjusterChanged(Adjuster* adj, double newVal)
{
}

void Framing::onFramingMethodChanged()
{
    updateFramingMethodGui();
}

void Framing::onAspectRatioChanged()
{
}

void Framing::onOrientationChanged()
{
}

void Framing::onWidthChanged()
{
}

void Framing::onHeightChanged()
{
}

void Framing::onAllowUpscalingToggled()
{
}

void Framing::onBorderSizeMethodChanged()
{
    updateBorderSizeGui();
}

void Framing::onBasisChanged()
{
}

void Framing::onMinSizeToggled()
{
    updateBorderSizeGui();
}

void Framing::onMinWidthChanged()
{
}

void Framing::onMinHeightChanged()
{
}

void Framing::onAbsWidthChanged()
{
}

void Framing::onAbsHeightChanged()
{
}