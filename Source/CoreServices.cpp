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

#include "CoreServices.h"
#include "AccessClass.h"

#include "Processors/Events/Event.h"
#include "Processors/MessageCenter/MessageCenterEditor.h"
#include "Processors/ProcessorGraph/ProcessorGraph.h"
#include "Processors/RecordNode/RecordNode.h"
#include "UI/SNAPControlPanel.h"
#include "UI/SNAPEditorViewport.h"
#include "UI/PopupComponent.h"
#include "Processors/Merger/Merger.h"

using namespace AccessClass;

namespace CoreServices
{
void updateSignalChain (GenericProcessor* source)
{
    getProcessorGraph()->updateSettings (source);
}

void updateSignalChain (GenericEditor* source)
{
    updateSignalChain (source->getProcessor());
}

void saveRecoveryConfig()
{
    File configsDir = getSavedStateDirectory();
    if (! configsDir.getFullPathName().contains ("plugin-GUI" + File::getSeparatorString() + "Build"))
        configsDir = configsDir.getChildFile ("configs-api" + String (PLUGIN_API_VER));

    SNAPEditorViewport* ev = getSNAPEditorViewport();
    File recoveryFile = configsDir.getChildFile ("recoveryConfig.xml");
    //NOTE: Recovery config will not get saved in headless mode
    if (ev != nullptr)
        ev->saveState (recoveryFile);
}

void loadSignalChain (String path)
{
    File fileToLoad (path);

    if (fileToLoad.existsAsFile())
    {
        if (getSNAPEditorViewport() != nullptr)
            getSNAPEditorViewport()->loadState (File (path));
        else // headless mode
        {
            XmlDocument doc (fileToLoad);
            std::unique_ptr<XmlElement> xml = doc.getDocumentElement();
            AccessClass::getProcessorGraph()->loadFromXml (xml.get());
        }
    }
}

bool getAcquisitionStatus()
{
    return getSNAPControlPanel()->getAcquisitionState();
}

void setAcquisitionStatus (bool enable)
{
    const MessageManagerLock mml;
    getSNAPControlPanel()->setAcquisitionState (enable);
}

bool getRecordingStatus()
{
    return getSNAPControlPanel()->getRecordingState();
}

void setRecordingStatus (bool enable)
{
    const MessageManagerLock mml;
    getSNAPControlPanel()->setRecordingState (enable, true); // starts recording regardless of sync status
}

void sendStatusMessage (const String& text)
{
    LOGD ("CoreServices::sendStatusMessage: ", text);
    getBroadcaster()->sendActionMessage (text);
}

void sendStatusMessage (const char* text)
{
    LOGD ("CoreServices::sendStatusMessage: ", String (text));
    getBroadcaster()->sendActionMessage (text);
}

void highlightEditor (GenericEditor* ed)
{
    getSNAPEditorViewport()->makeEditorVisible (ed);
}

juce::int64 getSystemTime()
{
    return Time::currentTimeMillis();
}

juce::int64 getRecordingTime()
{
    return getSNAPControlPanel()->getRecordingTime();
}

void setRecordingParentDirectory (String dir)
{
    if (File (dir).exists())
    {
        getSNAPControlPanel()->setRecordingParentDirectory (dir);
    }
    else
    {
        sendStatusMessage (dir + " not found.");
    }
}

File getRecordingParentDirectory()
{
    return getSNAPControlPanel()->getRecordingParentDirectory();
}

void setRecordingDirectoryBaseText (String text)
{
    getSNAPControlPanel()->setRecordingDirectoryBaseText (text);
}

String getRecordingDirectoryBaseText()
{
    return getSNAPControlPanel()->getRecordingDirectoryBaseText();
}

String getRecordingDirectoryName()
{
    return getSNAPControlPanel()->getRecordingDirectoryName();
}

void createNewRecordingDirectory()
{
    getSNAPControlPanel()->createNewRecordingDirectory();
}

void setRecordingDirectoryPrependText (String text)
{
    getSNAPControlPanel()->setRecordingDirectoryPrependText (text);
}

void setRecordingDirectoryAppendText (String text)
{
    getSNAPControlPanel()->setRecordingDirectoryAppendText (text);
}

String getRecordingDirectoryPrependText()
{
    return getSNAPControlPanel()->getRecordingDirectoryPrependText();
}

String getRecordingDirectoryAppendText()
{
    return getSNAPControlPanel()->getRecordingDirectoryAppendText();
}

std::vector<RecordEngineManager*> getAvailableRecordEngines()
{
    return getSNAPControlPanel()->getAvailableRecordEngines();
}

String getDefaultRecordEngineId()
{
    return getSNAPControlPanel()->getSelectedRecordEngineId();
}

bool setDefaultRecordEngine (String id)
{
    return getSNAPControlPanel()->setSelectedRecordEngineId (id);
}

bool allRecordNodesAreSynchronized()
{
    for (auto node : getProcessorGraph()->getRecordNodes())
    {
        if (! node->isSynchronized())
            return false;
    }

    return true;
}

Array<int> getAvailableRecordNodeIds()
{
    Array<int> nodeIds;

    for (auto node : getProcessorGraph()->getRecordNodes())
    {
        nodeIds.add (node->getNodeId());
    }

    return nodeIds;
}

GenericProcessor* getProcessorById (uint16_t nodeId)
{
    if (getProcessorGraph() == nullptr)
    {
        return nullptr;
    }
    return getProcessorGraph()->getProcessorWithNodeId (nodeId);
}

std::vector<int> getPredecessorProcessorIds (GenericProcessor* node)
{
    std::vector<int> predecessor_node_ids;

    std::queue<GenericProcessor*> queued;
    queued.push (node);
    bool is_original = true;
    while (! queued.empty())
    {
        auto cur = queued.front();
        queued.pop();

        // Don't push on the original node itself, nor populate the IDs for a merger/splitter
        if (! cur->isMerger() && ! cur->isSplitter() && ! is_original)
        {
            predecessor_node_ids.push_back (cur->getNodeId());
        }
        is_original = false;

        if (cur->isMerger())
        {
            Merger* merger = (Merger*) cur;
            if (merger->getSourceNode (0) != nullptr)
            {
                queued.push (merger->getSourceNode (0));
            }
            if (merger->getSourceNode (1) != nullptr)
            {
                queued.push (merger->getSourceNode (1));
            }
        }
        else
        {
            auto parent_node = cur->getSourceNode();
            if (parent_node != nullptr)
            {
                queued.push (parent_node);
            }
        }
    }

    return predecessor_node_ids;
}

GenericProcessor* getProcessorByName (String processorName, bool onlySearchSources)
{
    if (getProcessorGraph() == nullptr)
    {
        return nullptr;
    }
    Array<GenericProcessor*> processors;
    if (onlySearchSources)
    {
        processors = getProcessorGraph()->getRootNodes();
    }
    else
    {
        processors = getProcessorGraph()->getListOfProcessors();
    }

    for (const auto& processor : processors)
    {
        if (processor->getName() == processorName)
        {
            return processor;
        }
    }

    return nullptr;
}

namespace RecordNode
{

    void setRecordingDirectory (String dir, int nodeId, bool applyToAll)
    {
        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId || applyToAll)
            {
                node->getParameter ("directory")->setNextValue (dir);
            }
        }
    }

    File getRecordingDirectory (int nodeId)
    {
        File directory;

        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                directory = node->getDataDirectory();
        }

        return directory;
    }

    float getFreeSpaceAvailable (int nodeId)
    {
        float freeSpace = -1.0f;

        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                freeSpace = node->getFreeSpaceKilobytes();
        }

        return freeSpace;
    }

    String getRecordEngineId (int nodeId)
    {
        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                return node->getEngineId();
        }
        return String ("NO_MATCHING_NODE_FOUND");
    }

    void setRecordEngine (String id, int nodeId, bool applyToAll)
    {
        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId || applyToAll)
                node->setEngine (id);
        }
    }

    int getRecordingNumber (int nodeId)
    {
        int lastRecordingNum = -1;

        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                lastRecordingNum = node->getRecordingNumber();
        }

        return lastRecordingNum;
    }

    int getExperimentNumber (int nodeId)
    {
        int experimentNumber = -1;

        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                experimentNumber = node->getExperimentNumber();
        }

        return experimentNumber;
    }

    void createNewRecordingDirectory (int nodeId)
    {
        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                node->createNewDirectory(true);
        }
    }

    bool isSynchronized (int nodeId)
    {
        for (auto* node : getProcessorGraph()->getRecordNodes())
        {
            if (node->getNodeId() == nodeId)
                return node->isSynchronized();
        }
        return false;
    }

    /* NOT YET IMPLEMENTED -- these functions are currently global only

		void setRecordingStatus(int nodeId, bool status)
		{
			for (auto* node : getProcessorGraph()->getRecordNodes())
			{
				if (node->getNodeId() == nodeId)
				{
					if (status && !getRecordingStatus(nodeId))
					{
						node->startRecording();
						return;
					}

					if (!status && getRecordingStatus(nodeId))
					{
						node->stopRecording();
						return;
					}
				}
			}
		}

		bool getRecordingStatus(int nodeId)
		{
			bool status = false;

			for (auto* node : getProcessorGraph()->getRecordNodes())
			{
				if (node->getNodeId() == nodeId)
					status = node->getRecordingStatus();
			}

			return status;
		}*/

}; // namespace RecordNode

//const char* getApplicationResource(const char* name, int& size)
//{
//	return BinaryData::getNamedResource(name, size);
//}

File getDefaultUserSaveDirectory()
{
#if defined(__APPLE__)
    const File dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SNAP");
#elif _WIN32
    const File dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("SNAP");
#else
    const File dir = File::getSpecialLocation (File::userHomeDirectory).getChildFile ("snap");
#endif
    if (! dir.isDirectory())
    {
        dir.createDirectory();
    }
    return std::move (dir);
}

File getSavedStateDirectory()
{
#if defined(__APPLE__)
    File dir = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Application Support/snap");
#elif _WIN32
    String appDir = File::getSpecialLocation (File::currentApplicationFile).getFullPathName();
    File dir;
    if (appDir.contains ("plugin-GUI\\Build\\"))
        dir = File::getSpecialLocation (File::currentApplicationFile).getParentDirectory();
    else
        dir = File::getSpecialLocation (File::windowsLocalAppData).getChildFile ("SNAP");
#else
    String appDir = File::getSpecialLocation (File::currentApplicationFile).getFullPathName();
    File dir;
    if (appDir.contains ("plugin-GUI/Build/"))
        dir = File::getSpecialLocation (File::currentApplicationFile).getParentDirectory();
    else
        dir = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("snap");
#endif
    if (! dir.isDirectory())
    {
        dir.createDirectory();
    }
    return std::move (dir);
}

String getGUIVersion()
{
#define XSTR_DEF(s) #s
#define STR_DEF(s) XSTR_DEF (s)
    return STR_DEF (JUCE_APP_VERSION);
}

UndoManager* getUndoManager()
{
    return getProcessorGraph()->getUndoManager();
}

PopupManager* getPopupManager()
{
    return getSNAPUIComponent()->getPopupManager();
}

namespace PluginInstaller
{
    bool installPlugin (String plugin, String version)
    {
        getSNAPUIComponent()->getSNAPPluginInstaller()->installPluginAndDependency (plugin, version);

        return true;
    }
} // namespace PluginInstaller

}; // namespace CoreServices
