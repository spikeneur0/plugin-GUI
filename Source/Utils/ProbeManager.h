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

#ifndef __PROBEMANAGER_H__
#define __PROBEMANAGER_H__

#include "../../JuceLibraryCode/JuceHeader.h"
#include "ProbeInterfaceReader.h"

/**
    Manages loaded ProbeInterface probe definitions.

    Stores probe definitions loaded from JSON files, provides lookup
    by channel count, and persists the list of loaded probe file paths
    across sessions via XML config.
*/
class ProbeManager
{
public:
    /** Get the singleton instance */
    static ProbeManager& getInstance();

    /** Load a probe definition from a JSON file. Returns the index, or -1 on error. */
    int loadProbeFromFile (const File& jsonFile, String& errorOut);

    /** Remove a loaded probe by index */
    void removeProbe (int index);

    /** Remove all loaded probes */
    void removeAllProbes();

    /** Get the number of loaded probes */
    int getNumProbes() const { return probes.size(); }

    /** Get a loaded probe by index */
    const ProbeDefinition& getProbe (int index) const { return probes.getReference (index); }

    /** Get the file path for a loaded probe */
    const File& getProbeFile (int index) const { return probeFiles.getReference (index); }

    /** Find probes matching a given channel count */
    Array<int> findProbesForChannelCount (int channelCount) const;

    /** Save loaded probe file paths to XML config */
    void saveStateToXml (XmlElement* xml) const;

    /** Load probe file paths from XML config and re-parse them */
    void loadStateFromXml (XmlElement* xml);

    /** Show a dialog listing all currently loaded probes */
    void showProbeViewer() const;

    /** Show a dialog with probe summary after loading */
    static void showProbeSummary (const ProbeDefinition& probe);

private:
    ProbeManager() {}

    Array<ProbeDefinition> probes;
    Array<File> probeFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProbeManager);
};

#endif // __PROBEMANAGER_H__
