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

#include "ProbeInterfaceReader.h"

// =========================================================================
// ProbeDefinition validation
// =========================================================================

String ProbeDefinition::getValidationErrors() const
{
    StringArray errors;

    if (contacts.size() == 0)
        errors.add ("Probe has no contacts");

    // Check device_channel_indices for duplicates
    std::set<int> seenIndices;
    for (const auto& c : contacts)
    {
        if (seenIndices.count (c.deviceChannelIndex) > 0)
            errors.add ("Duplicate device_channel_index: " + String (c.deviceChannelIndex));
        seenIndices.insert (c.deviceChannelIndex);
    }

    // Check device_channel_indices range
    for (const auto& c : contacts)
    {
        if (c.deviceChannelIndex < 0)
            errors.add ("Negative device_channel_index: " + String (c.deviceChannelIndex)
                        + " at contact " + String (c.contactIndex));
    }

    // Check that max index doesn't exceed contact count
    int maxIdx = -1;
    for (const auto& c : contacts)
        maxIdx = jmax (maxIdx, c.deviceChannelIndex);

    if (maxIdx >= contacts.size())
        errors.add ("device_channel_index " + String (maxIdx)
                     + " out of range [0, " + String (contacts.size() - 1) + "]");

    return errors.joinIntoString ("\n");
}

// =========================================================================
// JSON parsing helpers
// =========================================================================

static ProbeDefinition parseProbeJson (const var& probeVar, const String& spec, const String& version, String& errorOut)
{
    ProbeDefinition probe;
    probe.specification = spec;
    probe.version = version;

    if (! probeVar.isObject())
    {
        errorOut = "Probe entry is not a JSON object";
        return probe;
    }

    auto* probeObj = probeVar.getDynamicObject();
    if (probeObj == nullptr)
    {
        errorOut = "Failed to parse probe object";
        return probe;
    }

    // Parse basic fields
    probe.ndim = probeVar.getProperty ("ndim", 2);
    probe.siUnits = probeVar.getProperty ("si_units", "um").toString();

    // Parse annotations
    var annotations = probeVar.getProperty ("annotations", var());
    if (annotations.isObject())
    {
        probe.name = annotations.getProperty ("name", "Unknown Probe").toString();
        probe.manufacturer = annotations.getProperty ("manufacturer", "Unknown").toString();
    }

    // Parse contact_positions (required)
    var positions = probeVar.getProperty ("contact_positions", var());
    if (! positions.isArray())
    {
        errorOut = "Missing or invalid contact_positions array";
        return probe;
    }

    int numContacts = positions.getArray()->size();

    // Parse device_channel_indices (required)
    var deviceIndices = probeVar.getProperty ("device_channel_indices", var());
    if (! deviceIndices.isArray())
    {
        errorOut = "Missing or invalid device_channel_indices array";
        return probe;
    }

    if (deviceIndices.getArray()->size() != numContacts)
    {
        errorOut = "device_channel_indices length (" + String (deviceIndices.getArray()->size())
                   + ") does not match contact_positions length (" + String (numContacts) + ")";
        return probe;
    }

    // Parse optional arrays
    var contactIds = probeVar.getProperty ("contact_ids", var());
    var shankIds = probeVar.getProperty ("shank_ids", var());

    if (shankIds.isArray() && shankIds.getArray()->size() != numContacts)
    {
        errorOut = "shank_ids length (" + String (shankIds.getArray()->size())
                   + ") does not match contact count (" + String (numContacts) + ")";
        return probe;
    }

    // Parse contact shape
    String contactShape = "circle";
    float shapeParam = 5.0f;

    var shapes = probeVar.getProperty ("contact_shapes", var());
    if (shapes.isString())
        contactShape = shapes.toString();

    var shapeParams = probeVar.getProperty ("contact_shape_params", var());
    if (shapeParams.isObject())
    {
        if (contactShape == "circle")
            shapeParam = (float) shapeParams.getProperty ("radius", 5.0);
        else
            shapeParam = (float) shapeParams.getProperty ("width", 10.0);
    }

    // Build contact list
    for (int i = 0; i < numContacts; i++)
    {
        ProbeContact contact;
        contact.contactIndex = i;

        // Position
        var pos = positions.getArray()->getUnchecked (i);
        if (pos.isArray() && pos.getArray()->size() >= 2)
        {
            contact.x = (float) pos.getArray()->getUnchecked (0);
            contact.y = (float) pos.getArray()->getUnchecked (1);
        }

        // Device channel index (THE CRITICAL MAPPING)
        contact.deviceChannelIndex = (int) deviceIndices.getArray()->getUnchecked (i);

        // Contact ID
        if (contactIds.isArray() && i < contactIds.getArray()->size())
            contact.contactId = contactIds.getArray()->getUnchecked (i).toString();
        else
            contact.contactId = "e" + String (i);

        // Shank ID
        if (shankIds.isArray() && i < shankIds.getArray()->size())
            contact.shankId = shankIds.getArray()->getUnchecked (i).toString();
        else
            contact.shankId = "0";

        contact.contactShape = contactShape;
        contact.shapeParam = shapeParam;

        probe.contacts.add (contact);
    }

    // Parse planar contour (optional)
    var contour = probeVar.getProperty ("probe_planar_contour", var());
    if (contour.isArray())
    {
        for (int i = 0; i < contour.getArray()->size(); i++)
        {
            var pt = contour.getArray()->getUnchecked (i);
            if (pt.isArray() && pt.getArray()->size() >= 2)
            {
                float cx = (float) pt.getArray()->getUnchecked (0);
                float cy = (float) pt.getArray()->getUnchecked (1);
                probe.planarContour.add (Point<float> (cx, cy));
            }
        }
    }

    // Validate
    String validationErrors = probe.getValidationErrors();
    if (validationErrors.isNotEmpty())
        errorOut = validationErrors;

    return probe;
}

// =========================================================================
// Public API
// =========================================================================

Array<ProbeDefinition> ProbeInterfaceReader::readAllProbesFromString (const String& json, String& errorOut)
{
    Array<ProbeDefinition> probes;

    var parsed = JSON::parse (json);
    if (parsed.isVoid())
    {
        errorOut = "Failed to parse JSON";
        return probes;
    }

    if (! parsed.isObject())
    {
        errorOut = "JSON root is not an object";
        return probes;
    }

    String spec = parsed.getProperty ("specification", "").toString();
    String version = parsed.getProperty ("version", "").toString();

    if (spec != "probeinterface")
    {
        errorOut = "Not a ProbeInterface file (specification: \"" + spec + "\")";
        return probes;
    }

    var probesArray = parsed.getProperty ("probes", var());
    if (! probesArray.isArray())
    {
        errorOut = "Missing or invalid 'probes' array";
        return probes;
    }

    for (int i = 0; i < probesArray.getArray()->size(); i++)
    {
        String probeError;
        auto probe = parseProbeJson (probesArray.getArray()->getUnchecked (i), spec, version, probeError);

        if (probeError.isNotEmpty())
        {
            errorOut = "Probe " + String (i) + ": " + probeError;
            if (probe.contacts.size() == 0)
                continue;
        }

        probes.add (probe);
    }

    return probes;
}

Array<ProbeDefinition> ProbeInterfaceReader::readAllProbes (const File& jsonFile, String& errorOut)
{
    if (! jsonFile.existsAsFile())
    {
        errorOut = "File not found: " + jsonFile.getFullPathName();
        return {};
    }

    return readAllProbesFromString (jsonFile.loadFileAsString(), errorOut);
}

ProbeDefinition ProbeInterfaceReader::readFromString (const String& json, String& errorOut)
{
    auto probes = readAllProbesFromString (json, errorOut);

    if (probes.size() > 0)
        return probes[0];

    return ProbeDefinition();
}

ProbeDefinition ProbeInterfaceReader::readFromFile (const File& jsonFile, String& errorOut)
{
    if (! jsonFile.existsAsFile())
    {
        errorOut = "File not found: " + jsonFile.getFullPathName();
        return ProbeDefinition();
    }

    return readFromString (jsonFile.loadFileAsString(), errorOut);
}

bool ProbeInterfaceReader::validateChannelMapping (const ProbeDefinition& probe, int expectedChannelCount, String& errorOut)
{
    if (probe.getNumContacts() != expectedChannelCount)
    {
        errorOut = "Probe has " + String (probe.getNumContacts())
                   + " contacts but expected " + String (expectedChannelCount) + " channels";
        return false;
    }

    // Check all indices are in valid range
    for (const auto& c : probe.contacts)
    {
        if (c.deviceChannelIndex < 0 || c.deviceChannelIndex >= expectedChannelCount)
        {
            errorOut = "device_channel_index " + String (c.deviceChannelIndex)
                       + " out of range [0, " + String (expectedChannelCount - 1) + "]";
            return false;
        }
    }

    // Check for duplicates
    std::set<int> seen;
    for (const auto& c : probe.contacts)
    {
        if (seen.count (c.deviceChannelIndex) > 0)
        {
            errorOut = "Duplicate device_channel_index: " + String (c.deviceChannelIndex);
            return false;
        }
        seen.insert (c.deviceChannelIndex);
    }

    return true;
}
