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

#include "ProbeManager.h"

ProbeManager& ProbeManager::getInstance()
{
    static ProbeManager instance;
    return instance;
}

int ProbeManager::loadProbeFromFile (const File& jsonFile, String& errorOut)
{
    auto allProbes = ProbeInterfaceReader::readAllProbes (jsonFile, errorOut);

    if (allProbes.size() == 0)
    {
        if (errorOut.isEmpty())
            errorOut = "No probes found in file";
        return -1;
    }

    int firstIndex = probes.size();

    for (auto& probe : allProbes)
    {
        if (! probe.isValid())
        {
            errorOut = probe.getValidationErrors();
            continue;
        }

        probes.add (probe);
        probeFiles.add (jsonFile);
    }

    if (probes.size() == firstIndex)
        return -1;

    return firstIndex;
}

void ProbeManager::removeProbe (int index)
{
    if (index >= 0 && index < probes.size())
    {
        probes.remove (index);
        probeFiles.remove (index);
    }
}

void ProbeManager::removeAllProbes()
{
    probes.clear();
    probeFiles.clear();
}

Array<int> ProbeManager::findProbesForChannelCount (int channelCount) const
{
    Array<int> matches;

    for (int i = 0; i < probes.size(); i++)
    {
        if (probes[i].getNumContacts() == channelCount)
            matches.add (i);
    }

    return matches;
}

void ProbeManager::saveStateToXml (XmlElement* xml) const
{
    auto* probesXml = xml->createNewChildElement ("PROBE_DEFINITIONS");

    // Save unique file paths (a file may contain multiple probes)
    StringArray savedPaths;
    for (const auto& file : probeFiles)
    {
        String path = file.getFullPathName();
        if (! savedPaths.contains (path))
        {
            savedPaths.add (path);
            auto* fileXml = probesXml->createNewChildElement ("PROBE_FILE");
            fileXml->setAttribute ("path", path);
        }
    }
}

void ProbeManager::loadStateFromXml (XmlElement* xml)
{
    auto* probesXml = xml->getChildByName ("PROBE_DEFINITIONS");
    if (probesXml == nullptr)
        return;

    removeAllProbes();

    for (auto* fileXml : probesXml->getChildIterator())
    {
        if (fileXml->hasTagName ("PROBE_FILE"))
        {
            String path = fileXml->getStringAttribute ("path");
            File file (path);

            if (file.existsAsFile())
            {
                String error;
                loadProbeFromFile (file, error);
            }
        }
    }
}

void ProbeManager::showProbeViewer() const
{
    if (probes.size() == 0)
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon,
                                           "Loaded Probes",
                                           "No probe definitions are currently loaded.\n\n"
                                           "Right-click on the graph viewer canvas and select\n"
                                           "\"Load Probe Definition...\" to load a ProbeInterface JSON file.");
        return;
    }

    String message;

    for (int i = 0; i < probes.size(); i++)
    {
        const auto& probe = probes[i];

        message += String (i + 1) + ". " + probe.name + "\n";
        message += "   Manufacturer: " + probe.manufacturer + "\n";
        message += "   Contacts: " + String (probe.getNumContacts()) + "\n";
        message += "   Shanks: " + String (probe.getNumShanks()) + "\n";
        message += "   Units: " + probe.siUnits + "\n";
        message += "   File: " + probeFiles[i].getFileName() + "\n";

        if (i < probes.size() - 1)
            message += "\n";
    }

    AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon,
                                       "Loaded Probes (" + String (probes.size()) + ")",
                                       message);
}

void ProbeManager::showProbeSummary (const ProbeDefinition& probe)
{
    String message;
    message += "Name: " + probe.name + "\n";
    message += "Manufacturer: " + probe.manufacturer + "\n";
    message += "Specification: " + probe.specification + " v" + probe.version + "\n";
    message += "Contacts: " + String (probe.getNumContacts()) + "\n";
    message += "Shanks: " + String (probe.getNumShanks()) + "\n";
    message += "Units: " + probe.siUnits + "\n";

    if (probe.contacts.size() > 0)
    {
        // Show range of positions
        float minY = probe.contacts[0].y, maxY = probe.contacts[0].y;
        float minX = probe.contacts[0].x, maxX = probe.contacts[0].x;
        for (const auto& c : probe.contacts)
        {
            minX = jmin (minX, c.x);
            maxX = jmax (maxX, c.x);
            minY = jmin (minY, c.y);
            maxY = jmax (maxY, c.y);
        }
        message += "Span: " + String (maxX - minX, 1) + " x " + String (maxY - minY, 1) + " " + probe.siUnits + "\n";
    }

    if (! probe.isValid())
    {
        message += "\nValidation warnings:\n" + probe.getValidationErrors();
    }

    AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon,
                                       "Probe Loaded Successfully",
                                       message);
}
