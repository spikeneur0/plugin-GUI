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

#ifndef __ACCESSCLASS_H_CE1DC2DE__
#define __ACCESSCLASS_H_CE1DC2DE__

#include "../JuceLibraryCode/JuceHeader.h"
#include "TestableExport.h"

class SNAPUIComponent;
class SNAPEditorViewport;
class SNAPProcessorList;
class SNAPDataViewport;
class ProcessorGraph;
class MessageCenter;
class SNAPControlPanel;
class SNAPAudioComponent;
class SNAPGraphViewer;
class PluginManager;
class GenericProcessor;

namespace AccessClass
{

/** Sets the object's SNAPUIComponent and copies all the necessary pointers 
 * from the SNAPUIComponent. */
void setSNAPUIComponent (SNAPUIComponent*);

/** Sets the object's ProcessorGraph */
void setProcessorGraph (ProcessorGraph*);

/** Sets the object's SNAPAudioComponent */
void setSNAPAudioComponent (SNAPAudioComponent*);

/** Sets the object's SNAPControlPanel */
void setSNAPControlPanel (SNAPControlPanel*);

/** Returns a pointer to the application's SNAPEditorViewport. */
SNAPEditorViewport* getSNAPEditorViewport();

/** Returns a pointer to the application's SNAPDataViewport. */
SNAPDataViewport* getSNAPDataViewport();

/** Returns a pointer to the application's SNAPProcessorList. */
SNAPProcessorList* getSNAPProcessorList();

/** Returns a pointer to the application's ProcessorGraph. */
ProcessorGraph* getProcessorGraph();

/** Returns a pointer to the application's SNAPDataViewport. */
SNAPControlPanel* getSNAPControlPanel();

/** Returns a pointer to the application's MessageCenter. */
MessageCenter* getMessageCenter();

/** Returns a pointer to the application's SNAPUIComponent. */
SNAPUIComponent* getSNAPUIComponent();

/** Returns a pointer to the application's SNAPAudioComponent. */
SNAPAudioComponent* getSNAPAudioComponent();

/** Returns a pointer to the application's SNAPGraphViewer. */
SNAPGraphViewer* getSNAPGraphViewer();

/** Returns a pointer to the application's PluginManager. */
PluginManager* getPluginManager();

/** Returns a pointer to the application's UndoManager */
UndoManager* getUndoManager();

/** Returns a pointer to the application's ActionBroadcaster */
ActionBroadcaster* getBroadcaster();

void TESTABLE setMessageCenter (MessageCenter* mc_);

/** Clears all of the global state in AccessClass. Only for use in testing. */
void TESTABLE clearAccessClassStateForTesting();

void shutdownBroadcaster();

//Methods to access some private members of GenericProcessors.
//Like all of the AccessClass methods, this ones are meant to be
//used by various internal parts of the core GUI which need access
//to those members, while keeping them inaccessible by normal plugins

class TESTABLE ExternalProcessorAccessor
{
public:
    static MidiBuffer* getMidiBuffer (GenericProcessor* proc);
    static void injectNumSamples (GenericProcessor* proc, uint16_t dataStream, uint32_t numSamples);
};

}; // namespace AccessClass

#endif // __ACCESSCLASS_H_CE1DC2DE__
