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

#include "PluginManifest.h"
#include "../AccessClass.h"
#include "../Processors/PluginManager/PluginManager.h"

static String pluginTypeToString (Plugin::Type type)
{
    switch (type)
    {
        case Plugin::BUILT_IN:      return "BUILT_IN";
        case Plugin::PROCESSOR:     return "PROCESSOR";
        case Plugin::RECORD_ENGINE: return "RECORD_ENGINE";
        case Plugin::FILE_SOURCE:   return "FILE_SOURCE";
        case Plugin::DATA_THREAD:   return "DATA_THREAD";
        default:                    return "UNKNOWN";
    }
}

String PluginManifest::getTypeString (Plugin::Type type)
{
    return pluginTypeToString (type);
}

void PluginManifest::generate (XmlElement* xml)
{
    auto* pm = AccessClass::getPluginManager();
    if (pm == nullptr)
        return;

    auto* manifest = xml->createNewChildElement ("PLUGIN_MANIFEST");

    manifest->setAttribute ("snap_version", ProjectInfo::versionString);
    manifest->setAttribute ("api_version", PLUGIN_API_VER);

    Time now = Time::getCurrentTime();
    manifest->setAttribute ("timestamp", now.toISO8601 (true));

    // Built-in processors
    {
        auto* entry = manifest->createNewChildElement ("PLUGIN");
        entry->setAttribute ("name", "Built-in Processors");
        entry->setAttribute ("version", String (ProjectInfo::versionString));
        entry->setAttribute ("type", "BUILT_IN");
        entry->setAttribute ("api_version", PLUGIN_API_VER);
    }

    // Processor plugins
    for (int i = 0; i < pm->getNumProcessors(); i++)
    {
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::PROCESSOR, i);
        Plugin::ProcessorInfo info = pm->getProcessorInfo (i);

        auto* entry = manifest->createNewChildElement ("PLUGIN");
        entry->setAttribute ("name", String (info.name));
        entry->setAttribute ("version", pm->getLibraryVersion (libIdx));
        entry->setAttribute ("type", "PROCESSOR");
        entry->setAttribute ("api_version", PLUGIN_API_VER);
        entry->setAttribute ("library", pm->getLibraryName (libIdx));
    }

    // DataThread plugins
    for (int i = 0; i < pm->getNumDataThreads(); i++)
    {
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::DATA_THREAD, i);
        Plugin::DataThreadInfo info = pm->getDataThreadInfo (i);

        auto* entry = manifest->createNewChildElement ("PLUGIN");
        entry->setAttribute ("name", String (info.name));
        entry->setAttribute ("version", pm->getLibraryVersion (libIdx));
        entry->setAttribute ("type", "DATA_THREAD");
        entry->setAttribute ("api_version", PLUGIN_API_VER);
        entry->setAttribute ("library", pm->getLibraryName (libIdx));
    }

    // RecordEngine plugins
    for (int i = 0; i < pm->getNumRecordEngines(); i++)
    {
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::RECORD_ENGINE, i);
        Plugin::RecordEngineInfo info = pm->getRecordEngineInfo (i);

        auto* entry = manifest->createNewChildElement ("PLUGIN");
        entry->setAttribute ("name", String (info.name));
        entry->setAttribute ("version", pm->getLibraryVersion (libIdx));
        entry->setAttribute ("type", "RECORD_ENGINE");
        entry->setAttribute ("api_version", PLUGIN_API_VER);
        entry->setAttribute ("library", pm->getLibraryName (libIdx));
    }

    // FileSource plugins
    for (int i = 0; i < pm->getNumFileSources(); i++)
    {
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::FILE_SOURCE, i);
        Plugin::FileSourceInfo info = pm->getFileSourceInfo (i);

        auto* entry = manifest->createNewChildElement ("PLUGIN");
        entry->setAttribute ("name", String (info.name));
        entry->setAttribute ("version", pm->getLibraryVersion (libIdx));
        entry->setAttribute ("type", "FILE_SOURCE");
        entry->setAttribute ("api_version", PLUGIN_API_VER);
        entry->setAttribute ("library", pm->getLibraryName (libIdx));
    }
}

Array<PluginManifest::ManifestEntry> PluginManifest::validate (XmlElement* xml)
{
    Array<ManifestEntry> results;

    auto* manifestXml = xml->getChildByName ("PLUGIN_MANIFEST");
    if (manifestXml == nullptr)
        return results;

    auto* pm = AccessClass::getPluginManager();
    if (pm == nullptr)
        return results;

    // Build lookup of currently loaded plugins: name -> version
    std::map<String, String> loadedPlugins;

    loadedPlugins["Built-in Processors"] = String (ProjectInfo::versionString);

    for (int i = 0; i < pm->getNumProcessors(); i++)
    {
        Plugin::ProcessorInfo info = pm->getProcessorInfo (i);
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::PROCESSOR, i);
        loadedPlugins[String (info.name)] = pm->getLibraryVersion (libIdx);
    }

    for (int i = 0; i < pm->getNumDataThreads(); i++)
    {
        Plugin::DataThreadInfo info = pm->getDataThreadInfo (i);
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::DATA_THREAD, i);
        loadedPlugins[String (info.name)] = pm->getLibraryVersion (libIdx);
    }

    for (int i = 0; i < pm->getNumRecordEngines(); i++)
    {
        Plugin::RecordEngineInfo info = pm->getRecordEngineInfo (i);
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::RECORD_ENGINE, i);
        loadedPlugins[String (info.name)] = pm->getLibraryVersion (libIdx);
    }

    for (int i = 0; i < pm->getNumFileSources(); i++)
    {
        Plugin::FileSourceInfo info = pm->getFileSourceInfo (i);
        int libIdx = pm->getLibraryIndexFromPlugin (Plugin::FILE_SOURCE, i);
        loadedPlugins[String (info.name)] = pm->getLibraryVersion (libIdx);
    }

    // Check each manifest entry against loaded plugins
    for (auto* pluginXml : manifestXml->getChildIterator())
    {
        if (! pluginXml->hasTagName ("PLUGIN"))
            continue;

        ManifestEntry entry;
        entry.name = pluginXml->getStringAttribute ("name");
        entry.version = pluginXml->getStringAttribute ("version");
        entry.type = pluginXml->getStringAttribute ("type");
        entry.apiVersion = pluginXml->getIntAttribute ("api_version", 0);

        auto it = loadedPlugins.find (entry.name);

        if (it == loadedPlugins.end())
        {
            entry.status = EntryStatus::MISSING;
            entry.loadedVersion = "";
        }
        else if (it->second != entry.version && entry.version.isNotEmpty())
        {
            entry.status = EntryStatus::VERSION_MISMATCH;
            entry.loadedVersion = it->second;
        }
        else
        {
            entry.status = EntryStatus::MATCHED;
            entry.loadedVersion = it->second;
        }

        results.add (entry);
    }

    return results;
}

bool PluginManifest::showValidationDialog (const Array<ManifestEntry>& entries)
{
    // Check if there are any issues
    bool hasIssues = false;
    for (const auto& entry : entries)
    {
        if (entry.status != EntryStatus::MATCHED)
        {
            hasIssues = true;
            break;
        }
    }

    if (! hasIssues)
        return true;

    // Build the message
    String message = "The following plugins differ from the saved configuration:\n\n";

    for (const auto& entry : entries)
    {
        if (entry.status == EntryStatus::MISSING)
        {
            message += "[MISSING]  " + entry.name + " v" + entry.version + " (" + entry.type + ")\n";
        }
        else if (entry.status == EntryStatus::VERSION_MISMATCH)
        {
            message += "[MISMATCH] " + entry.name + ": saved v" + entry.version
                       + " / loaded v" + entry.loadedVersion + "\n";
        }
    }

    message += "\nContinue loading anyway?";

    return AlertWindow::showOkCancelBox (AlertWindow::WarningIcon,
                                          "Plugin Manifest Warning",
                                          message,
                                          "Continue Anyway",
                                          "Cancel Load");
}

void PluginManifest::showManifestViewer()
{
    auto* pm = AccessClass::getPluginManager();
    if (pm == nullptr)
        return;

    String message;
    message += "SNAP Version: " + String (ProjectInfo::versionString) + "\n";
    message += "Plugin API Version: " + String (PLUGIN_API_VER) + "\n\n";

    message += "--- Built-in Processors ---\n";
    message += "  Merger, Splitter, File Reader, Record Node,\n";
    message += "  Audio Monitor, Event Translator\n\n";

    if (pm->getNumProcessors() > 0)
    {
        message += "--- Processor Plugins ---\n";
        for (int i = 0; i < pm->getNumProcessors(); i++)
        {
            Plugin::ProcessorInfo info = pm->getProcessorInfo (i);
            int libIdx = pm->getLibraryIndexFromPlugin (Plugin::PROCESSOR, i);
            message += "  " + String (info.name) + "  v" + pm->getLibraryVersion (libIdx)
                       + "  [" + pm->getLibraryName (libIdx) + "]\n";
        }
        message += "\n";
    }

    if (pm->getNumDataThreads() > 0)
    {
        message += "--- Data Thread Plugins ---\n";
        for (int i = 0; i < pm->getNumDataThreads(); i++)
        {
            Plugin::DataThreadInfo info = pm->getDataThreadInfo (i);
            int libIdx = pm->getLibraryIndexFromPlugin (Plugin::DATA_THREAD, i);
            message += "  " + String (info.name) + "  v" + pm->getLibraryVersion (libIdx)
                       + "  [" + pm->getLibraryName (libIdx) + "]\n";
        }
        message += "\n";
    }

    if (pm->getNumRecordEngines() > 0)
    {
        message += "--- Record Engine Plugins ---\n";
        for (int i = 0; i < pm->getNumRecordEngines(); i++)
        {
            Plugin::RecordEngineInfo info = pm->getRecordEngineInfo (i);
            int libIdx = pm->getLibraryIndexFromPlugin (Plugin::RECORD_ENGINE, i);
            message += "  " + String (info.name) + "  v" + pm->getLibraryVersion (libIdx)
                       + "  [" + pm->getLibraryName (libIdx) + "]\n";
        }
        message += "\n";
    }

    if (pm->getNumFileSources() > 0)
    {
        message += "--- File Source Plugins ---\n";
        for (int i = 0; i < pm->getNumFileSources(); i++)
        {
            Plugin::FileSourceInfo info = pm->getFileSourceInfo (i);
            int libIdx = pm->getLibraryIndexFromPlugin (Plugin::FILE_SOURCE, i);
            message += "  " + String (info.name) + "  v" + pm->getLibraryVersion (libIdx)
                       + "  [" + pm->getLibraryName (libIdx) + "]\n";
        }
    }

    AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon,
                                       "Plugin Manifest",
                                       message);
}
