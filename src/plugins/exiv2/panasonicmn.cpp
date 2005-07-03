// ***************************************************************** -*- C++ -*-
/*
 * Copyright (C) 2004, 2005 Andreas Huggel <ahuggel@gmx.net>
 * 
 * This program is part of the Exiv2 distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/*
  File:      panasonicmn.cpp
  Version:   $Rev: 581 $
  Author(s): Andreas Huggel (ahu) <ahuggel@gmx.net>
  History:   11-Jun-04, ahu: created
  Credits:   See header file
 */
// *****************************************************************************
#include "rcsid.hpp"
EXIV2_RCSID("@(#) $Id: panasonicmn.cpp 581 2005-06-12 05:54:57Z ahuggel $");

// *****************************************************************************
// included header files
#include "types.hpp"
#include "panasonicmn.hpp"
#include "makernote.hpp"
#include "value.hpp"

// + standard includes
#include <string>
#include <sstream>
#include <iomanip>
#include <cassert>

// Define DEBUG_MAKERNOTE to output debug information to std::cerr
#undef DEBUG_MAKERNOTE

// *****************************************************************************
// class member definitions
namespace Exiv2 {

    //! @cond IGNORE
    PanasonicMakerNote::RegisterMn::RegisterMn()
    {
        MakerNoteFactory::registerMakerNote("Panasonic", "*", createPanasonicMakerNote);
        MakerNoteFactory::registerMakerNote(
            panasonicIfdId, MakerNote::AutoPtr(new PanasonicMakerNote));

        ExifTags::registerMakerTagInfo(panasonicIfdId, tagInfo_);
    }
    //! @endcond

    // Panasonic MakerNote Tag Info
    const TagInfo PanasonicMakerNote::tagInfo_[] = {
        TagInfo(0x0001, "Quality", "Image Quality", panasonicIfdId, makerTags, unsignedShort, print0x0001),
        TagInfo(0x0002, "FirmwareVersion", "Firmware version", panasonicIfdId, makerTags, undefined, printValue),
        TagInfo(0x0003, "WhiteBalance", "White balance setting", panasonicIfdId, makerTags, unsignedShort, print0x0003),
        TagInfo(0x0004, "0x0004", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0007, "FocusMode", "Focus mode", panasonicIfdId, makerTags, unsignedShort, print0x0007),
        TagInfo(0x000f, "SpotMode", "Spot mode", panasonicIfdId, makerTags, unsignedByte, print0x000f),
        TagInfo(0x001a, "ImageStabilizer", "Image stabilizer", panasonicIfdId, makerTags, unsignedShort, print0x001a),
        TagInfo(0x001c, "Macro", "Macro mode", panasonicIfdId, makerTags, unsignedShort, print0x001c),
        TagInfo(0x001f, "ShootingMode", "Shooting mode", panasonicIfdId, makerTags, unsignedShort, print0x001f),
        TagInfo(0x0020, "Audio", "Audio", panasonicIfdId, makerTags, unsignedShort, print0x0020),
        TagInfo(0x0021, "DataDump", "Data dump", panasonicIfdId, makerTags, undefined, printValue),
        TagInfo(0x0022, "0x0022", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0023, "WhiteBalanceBias", "White balance adjustment", panasonicIfdId, makerTags, unsignedShort, print0x0023),
        TagInfo(0x0024, "FlashBias", "Flash bias", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0025, "SerialNumber", "Serial number", panasonicIfdId, makerTags, undefined, printValue),
        TagInfo(0x0026, "0x0026", "Unknown", panasonicIfdId, makerTags, undefined, printValue),
        TagInfo(0x0027, "0x0027", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0028, "ColorEffect", "Color effect", panasonicIfdId, makerTags, unsignedShort, print0x0028),
        TagInfo(0x0029, "0x0029", "Unknown", panasonicIfdId, makerTags, unsignedLong, printValue),
        TagInfo(0x002a, "0x002a", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x002b, "0x002b", "Unknown", panasonicIfdId, makerTags, unsignedLong, printValue),
        TagInfo(0x002c, "Contrast", "Contrast setting", panasonicIfdId, makerTags, unsignedShort, print0x002c),
        TagInfo(0x002d, "NoiseReduction", "Noise reduction", panasonicIfdId, makerTags, unsignedShort, print0x002d),
        TagInfo(0x002e, "0x002e", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x002f, "0x002f", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0030, "0x0030", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0031, "0x0031", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x0032, "0x0032", "Unknown", panasonicIfdId, makerTags, unsignedShort, printValue),
        TagInfo(0x4449, "0x4449", "Unknown", panasonicIfdId, makerTags, undefined, printValue),
        // End of list marker
        TagInfo(0xffff, "(UnknownPanasonicMakerNoteTag)", "Unknown PanasonicMakerNote tag", panasonicIfdId, makerTags, invalidTypeId, printValue)
    };

    PanasonicMakerNote::PanasonicMakerNote(bool alloc)
        : IfdMakerNote(panasonicIfdId, alloc, false)
    {
        byte buf[] = {
            'P', 'a', 'n', 'a', 's', 'o', 'n', 'i', 'c', 0x00, 0x00, 0x00
        };
        readHeader(buf, 12, byteOrder_);
    }

    PanasonicMakerNote::PanasonicMakerNote(const PanasonicMakerNote& rhs)
        : IfdMakerNote(rhs)
    {
    }

    int PanasonicMakerNote::readHeader(const byte* buf,
                                       long len, 
                                       ByteOrder byteOrder)
    {
        if (len < 12) return 1;

        header_.alloc(12);
        memcpy(header_.pData_, buf, header_.size_);
        // Adjust the offset of the IFD for the prefix
        adjOffset_ = 12;
        return 0;
    }

    int PanasonicMakerNote::checkHeader() const
    {
        int rc = 0;
        // Check the Panasonic prefix
        if (   header_.size_ < 12
            || std::string(reinterpret_cast<char*>(header_.pData_), 9) 
               != std::string("Panasonic", 9)) {
            rc = 2;
        }
        return rc;
    }

    PanasonicMakerNote::AutoPtr PanasonicMakerNote::create(bool alloc) const
    {
        return AutoPtr(create_(alloc));
    }

    PanasonicMakerNote* PanasonicMakerNote::create_(bool alloc) const 
    {
        AutoPtr makerNote = AutoPtr(new PanasonicMakerNote(alloc));
        assert(makerNote.get() != 0);
        makerNote->readHeader(header_.pData_, header_.size_, byteOrder_);
        return makerNote.release();
    }

    PanasonicMakerNote::AutoPtr PanasonicMakerNote::clone() const
    {
        return AutoPtr(clone_());
    }

    PanasonicMakerNote* PanasonicMakerNote::clone_() const 
    {
        return new PanasonicMakerNote(*this);
    }

    //! Quality
    const TagDetails quality[] = {
        { 0, "(start)" },
        { 2, "High" },
        { 3, "Standard" },
        { 6, "Very High" },
        { 7, "Raw" },
        { 0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x0001(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(quality).print(os, value);
    } // PanasonicMakerNote::print0x0001

    //! WhiteBalance
    const TagDetails whiteBalance[] = {
        {  0, "(start)" },
        {  1, "Auto" },
        {  2, "Daylight" },
        {  3, "Cloudy" },
        {  4, "Halogen" },
        {  5, "Manual" },
        {  8, "Flash" },
        { 10, "Black and White" },
        {  0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x0003(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(whiteBalance).print(os, value);
    } // PanasonicMakerNote::print0x0003

    //! FocusMode
    const TagDetails focusMode[] = {
        {  0, "(start)" },
        {  1, "Auto" },
        {  2, "Manual" },
        {  0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x0007(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(focusMode).print(os, value);
    } // PanasonicMakerNote::print0x0007

    std::ostream& PanasonicMakerNote::print0x000f(std::ostream& os, 
                                                  const Value& value)
    {
        if (value.count() < 2 || value.typeId() != unsignedByte) {
            return os << value;
        }
        long l0 = value.toLong(0);
        if (l0 == 1) os << "On";
        else if (l0 == 16) os << "Off";
        else os << value;
        return os;
    } // PanasonicMakerNote::print0x000f

    //! ImageStabilizer
    const TagDetails imageStabilizer[] = {
        {  0, "(start)" },
        {  2, "On, Mode 1" },
        {  3, "Off" },
        {  4, "On, Mode 2" },
        {  0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x001a(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(imageStabilizer).print(os, value);
    } // PanasonicMakerNote::print0x001a

    //! Macro
    const TagDetails macro[] = {
        { 0, "(start)" },
        { 1, "On" },
        { 2, "Off" },
        { 0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x001c(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(macro).print(os, value);
    } // PanasonicMakerNote::print0x001c

    //! ShootingMode
    const TagDetails shootingMode[] = {
        {  0, "(start)" },
        {  1, "Normal" },
        {  2, "Portrait" },
        {  3, "Scenery" },
        {  4, "Sports" },
        {  5, "Night Portrait" },
        {  6, "Program" },
        {  7, "Aperture Priority" },
        {  8, "Shutter Priority" },
        {  9, "Macro" },
        { 11, "Manual" },
        { 13, "Panning" },
        { 18, "Fireworks" },
        { 19, "Party" },
        { 20, "Snow" },
        { 21, "Night Scenery" },
        {  0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x001f(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(shootingMode).print(os, value);
    } // PanasonicMakerNote::print0x001f

    //! Audio
    const TagDetails Audio[] = {
        { 0, "(start)" },
        { 1, "Yes" },
        { 2, "No" },
        { 0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x0020(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(Audio).print(os, value);
    } // PanasonicMakerNote::print0x0020

    std::ostream& PanasonicMakerNote::print0x0023(std::ostream& os, 
                                                  const Value& value)
    {
        return os << std::fixed << std::setprecision(1) 
                  << value.toLong() / 3 << " EV";
    } // PanasonicMakerNote::print0x0023

    //! ColorEffect
    const TagDetails colorEffect[] = {
        { 0, "(start)" },
        { 1, "Off" },
        { 2, "Warm" },
        { 3, "Cool" },
        { 4, "Black and White" },
        { 5, "Sepia" },
        { 0, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x0028(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(colorEffect).print(os, value);
    } // PanasonicMakerNote::print0x0028

    //! Contrast
    const TagDetails contrast[] = {
        { -1, "(start)" },
        { 0, "Standard" },
        { 1, "Low" },
        { 2, "High" },
        { 0x100, "Low" },
        { 0x110, "Standard" },
        { 0x120, "High" },
        { -1, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x002c(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(contrast).print(os, value);
    } // PanasonicMakerNote::print0x002c

    //! NoiseReduction
    const TagDetails noiseReduction[] = {
        { -1, "(start)" },
        { 0, "Standard" },
        { 1, "Low" },
        { 2, "High" },
        { -1, "(end)" }
    };

    std::ostream& PanasonicMakerNote::print0x002d(std::ostream& os, 
                                                  const Value& value)
    {
        return TagTranslator(noiseReduction).print(os, value);
    } // PanasonicMakerNote::print0x002d

// *****************************************************************************
// free functions

    MakerNote::AutoPtr createPanasonicMakerNote(bool alloc,
                                           const byte* buf, 
                                           long len, 
                                           ByteOrder byteOrder, 
                                           long offset)
    {
        return MakerNote::AutoPtr(new PanasonicMakerNote(alloc));
    }

}                                       // namespace Exiv2