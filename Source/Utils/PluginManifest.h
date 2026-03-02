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

#ifndef __PLUGINMANIFEST_H__
#define __PLUGINMANIFEST_H__

#include "../../JuceLibraryCode/JuceHeader.h"
#include "../Processors/PluginManager/OpenEphysPlugin.h"

class PluginManager;

/**
    Records which plugins (and versions) are loaded when saving a signal chain,
    and detects missing or version-mismatched plugins when loading.

    This enables reproducible experimental configurations by warning users
    when their plugin environment differs from the saved configuration.
*/
namespace PluginManifest
{
    /** Status of a manifest entry when validated against loaded plugins */
    enum class EntryStatus
    {
        MATCHED,          ///< Plugin loaded with same name and version
        VERSION_MISMATCH, ///< Plugin loaded but different version
        MISSING           ///< Plugin not loaded at all
    };

    /** A single entry from a plugin manifest */
    struct ManifestEntry
    {
        String name;
        String version;
        String type;
        int apiVersion;
        EntryStatus status = EntryStatus::MATCHED;
        String loadedVersion; ///< The version actually loaded (for mismatch display)
    };

    /** Generate a PLUGIN_MANIFEST element and add it to the given XML root.
        Iterates all currently loaded plugins via PluginManager. */
    void generate (XmlElement* xml);

    /** Validate a PLUGIN_MANIFEST in the given XML against currently loaded plugins.
        Returns an array of entries with their status set.
        Returns an empty array if no manifest is present. */
    Array<ManifestEntry> validate (XmlElement* xml);

    /** Show a warning dialog if there are any mismatches or missing plugins.
        Returns true if the user chose to continue loading, false to cancel. */
    bool showValidationDialog (const Array<ManifestEntry>& entries);

    /** Show a read-only dialog listing all currently loaded plugins. */
    void showManifestViewer();

    /** Get the plugin type as a human-readable string */
    String getTypeString (Plugin::Type type);
};

#endif // __PLUGINMANIFEST_H__
