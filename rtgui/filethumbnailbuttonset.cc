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
#include "filethumbnailbuttonset.h"

#include "multilangmgr.h"
#include "lwbutton.h"
#include "svgpaintable.h"

bool FileThumbnailButtonSet::iconsLoaded = false;

hidpi::ScaledImageSurface FileThumbnailButtonSet::rankIcon = nullptr;
hidpi::ScaledImageSurface FileThumbnailButtonSet::gRankIcon = nullptr;
hidpi::ScaledImageSurface FileThumbnailButtonSet::unRankIcon = nullptr;
hidpi::ScaledImageSurface FileThumbnailButtonSet::trashIcon = nullptr;
hidpi::ScaledImageSurface FileThumbnailButtonSet::unTrashIcon = nullptr;
hidpi::ScaledImageSurface FileThumbnailButtonSet::processIcon = nullptr;
std::array<hidpi::ScaledImageSurface, 6> FileThumbnailButtonSet::colorLabelIcon;

Glib::ustring FileThumbnailButtonSet::processToolTip;
Glib::ustring FileThumbnailButtonSet::unrankToolTip;
Glib::ustring FileThumbnailButtonSet::trashToolTip;
Glib::ustring FileThumbnailButtonSet::untrashToolTip;
Glib::ustring FileThumbnailButtonSet::colorLabelToolTip;
std::array<Glib::ustring, 5> FileThumbnailButtonSet::rankToolTip;

FileThumbnailButtonSet::FileThumbnailButtonSet (FileBrowserEntry* myEntry, double device_scale)
{
    auto loadIcon = [&](const char* name) {
        return SvgPaintableWrapper::createFromIcon(name)->createSurface(
            SvgPaintableWrapper::IconSize::SMALL, device_scale);
    };

    if (!iconsLoaded) {
        unRankIcon  = loadIcon("star-hollow-narrow");
        rankIcon    = loadIcon("star-gold-narrow");
        gRankIcon   = loadIcon("star-narrow");
        trashIcon   = loadIcon("trash-small");
        unTrashIcon = loadIcon("trash-remove-small");
        processIcon = loadIcon("gears-small");
        colorLabelIcon[0] = loadIcon("circle-empty-gray-small");
        colorLabelIcon[1] = loadIcon("circle-red-small");
        colorLabelIcon[2] = loadIcon("circle-yellow-small");
        colorLabelIcon[3] = loadIcon("circle-green-small");
        colorLabelIcon[4] = loadIcon("circle-blue-small");
        colorLabelIcon[5] = loadIcon("circle-purple-small");

        processToolTip = M("FILEBROWSER_POPUPPROCESS");
        unrankToolTip = M("FILEBROWSER_UNRANK_TOOLTIP");
        trashToolTip = M("FILEBROWSER_POPUPTRASH");
        untrashToolTip = M("FILEBROWSER_POPUPUNTRASH");
        colorLabelToolTip = M("FILEBROWSER_COLORLABEL_TOOLTIP");
        rankToolTip[0] = M("FILEBROWSER_RANK1_TOOLTIP");
        rankToolTip[1] = M("FILEBROWSER_RANK2_TOOLTIP");
        rankToolTip[2] = M("FILEBROWSER_RANK3_TOOLTIP");
        rankToolTip[3] = M("FILEBROWSER_RANK4_TOOLTIP");
        rankToolTip[4] = M("FILEBROWSER_RANK5_TOOLTIP");

        iconsLoaded = true;
    }

    add(std::make_unique<LWButton>(processIcon, 6, myEntry, LWButton::Left, LWButton::Center, &processToolTip));
    add(std::make_unique<LWButton>(unRankIcon, 0, myEntry, LWButton::Left, LWButton::Center, &unrankToolTip));

    for (int i = 0; i < 5; i++) {
        add(std::make_unique<LWButton>(rankIcon, i + 1, myEntry, LWButton::Left, LWButton::Center, &rankToolTip[i]));
    }

    add(std::make_unique<LWButton>(trashIcon, 7, myEntry, LWButton::Right, LWButton::Center, &trashToolTip));
    add(std::make_unique<LWButton>(colorLabelIcon[0], 8, myEntry, LWButton::Right, LWButton::Center, &colorLabelToolTip));
}

void FileThumbnailButtonSet::setRank (int stars)
{

    for (int i = 1; i <= 5; i++) {
        buttons[i + 1]->setIcon(i <= stars ? rankIcon : gRankIcon);
    }
}

void FileThumbnailButtonSet::setColorLabel (int colorLabel)
{

    if (colorLabel >= 0 && colorLabel <= 5) {
        buttons[8]->setIcon(colorLabelIcon[colorLabel]);
    }
}

void FileThumbnailButtonSet::setInTrash (bool inTrash)
{

    buttons[7]->setIcon(inTrash ? unTrashIcon : trashIcon);
    buttons[7]->setToolTip(inTrash ? &untrashToolTip : &trashToolTip);
}
