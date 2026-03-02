/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __PROBEINTERFACEREADER_H__
#define __PROBEINTERFACEREADER_H__

#include "../../JuceLibraryCode/JuceHeader.h"

/**
    A single contact on a probe, with its position and device channel mapping.
*/
struct ProbeContact
{
    int contactIndex;          ///< Index in the contact_positions array
    String contactId;          ///< From contact_ids (e.g., "e0")
    float x = 0.0f;           ///< X position in si_units
    float y = 0.0f;           ///< Y position in si_units
    int deviceChannelIndex;    ///< THE CRITICAL MAPPING: which device channel this contact is wired to
    String shankId;            ///< Which shank this contact belongs to
    String contactShape;       ///< "circle", "square", "rect"
    float shapeParam = 5.0f;  ///< Radius for circle, width for square
};

/**
    A parsed probe definition from ProbeInterface JSON format.
*/
struct ProbeDefinition
{
    String name;
    String manufacturer;
    String specification;       ///< Should be "probeinterface"
    String version;
    int ndim = 2;
    String siUnits;             ///< Typically "um"
    Array<ProbeContact> contacts;
    Array<Point<float>> planarContour;

    int getNumContacts() const { return contacts.size(); }

    int getNumShanks() const
    {
        StringArray shanks;
        for (const auto& c : contacts)
            shanks.addIfNotAlreadyThere (c.shankId);
        return jmax (1, shanks.size());
    }

    /** Returns true if the definition passes all validation checks. */
    bool isValid() const { return getValidationErrors().isEmpty(); }

    /** Returns a newline-separated list of validation errors, or empty if valid. */
    String getValidationErrors() const;
};

/**
    Parses ProbeInterface JSON files into ProbeDefinition structs.

    The ProbeInterface format (https://probeinterface.readthedocs.io/) is a
    standard JSON schema for describing probe geometry, contact positions,
    and device_channel_indices mapping.

    CRITICAL: device_channel_indices mapping must be validated carefully.
    Wrong mapping = every channel mislabeled.
*/
namespace ProbeInterfaceReader
{
    /** Read a single probe (first probe) from a JSON file. */
    ProbeDefinition readFromFile (const File& jsonFile, String& errorOut);

    /** Read a single probe from a JSON string. */
    ProbeDefinition readFromString (const String& json, String& errorOut);

    /** Read all probes from a multi-probe JSON file. */
    Array<ProbeDefinition> readAllProbes (const File& jsonFile, String& errorOut);

    /** Read all probes from a JSON string. */
    Array<ProbeDefinition> readAllProbesFromString (const String& json, String& errorOut);

    /** Validate that a probe's channel mapping is compatible with an expected channel count. */
    bool validateChannelMapping (const ProbeDefinition& probe, int expectedChannelCount, String& errorOut);
};

#endif // __PROBEINTERFACEREADER_H__
