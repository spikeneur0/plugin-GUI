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

#include "AccessClass.h"
#include "Processors/GenericProcessor/GenericProcessor.h"
#include "Processors/MessageCenter/MessageCenter.h"
#include "Processors/ProcessorGraph/ProcessorGraph.h"

#include "UI/SNAPUIComponent.h"

namespace AccessClass
{
namespace
{

    SNAPUIComponent* ui = nullptr;
    SNAPEditorViewport* ev = nullptr;
    SNAPProcessorList* pl = nullptr;
    SNAPDataViewport* dv = nullptr;
    ProcessorGraph* pg = nullptr;
    SNAPControlPanel* cp = nullptr;
    MessageCenter* mc = nullptr;
    SNAPAudioComponent* ac = nullptr;
    SNAPGraphViewer* gv = nullptr;
    PluginManager* pm = nullptr;
    std::unique_ptr<ActionBroadcaster> bc;
} // namespace

void setSNAPUIComponent (SNAPUIComponent* ui_)
{
    if (ui != nullptr)
        return;
    ui = ui_;

    ev = ui->getSNAPEditorViewport();
    dv = ui->getSNAPDataViewport();
    pl = ui->getSNAPProcessorList();
    gv = ui->getSNAPGraphViewer();
}

void setProcessorGraph (ProcessorGraph* pg_)
{
    if (pg != nullptr)
        return;
    pg = pg_;

    pm = pg->getPluginManager();
    mc = pg->getMessageCenter();

    bc = std::make_unique<ActionBroadcaster>();
    bc->addActionListener (mc);
}

void setSNAPAudioComponent (SNAPAudioComponent* ac_)
{
    if (ac != nullptr)
        return;
    ac = ac_;
}

void setSNAPControlPanel (SNAPControlPanel* cp_)
{
    if (cp != nullptr)
        return;
    cp = cp_;
}

void shutdownBroadcaster()
{
    bc = nullptr;
}

/** Returns a pointer to the application's SNAPEditorViewport. */
SNAPEditorViewport* getSNAPEditorViewport()
{
    return ev;
}

/** Returns a pointer to the application's SNAPDataViewport. */
SNAPDataViewport* getSNAPDataViewport()
{
    return dv;
}

/** Returns a pointer to the application's SNAPProcessorList. */
SNAPProcessorList* getSNAPProcessorList()
{
    return pl;
}

/** Returns a pointer to the application's ProcessorGraph. */
ProcessorGraph* getProcessorGraph()
{
    return pg;
}

/** Returns a pointer to the application's SNAPDataViewport. */
SNAPControlPanel* getSNAPControlPanel()
{
    return cp;
}

/** Returns a pointer to the application's MessageCenter. */
MessageCenter* getMessageCenter()
{
    return mc;
}

/** Returns a pointer to the application's SNAPUIComponent. */
SNAPUIComponent* getSNAPUIComponent()
{
    return ui;
}

/** Returns a pointer to the application's SNAPAudioComponent. */
SNAPAudioComponent* getSNAPAudioComponent()
{
    return ac;
}

/** Returns a pointer to the application's SNAPGraphViewer. */
SNAPGraphViewer* getSNAPGraphViewer()
{
    return gv;
}

PluginManager* getPluginManager()
{
    return pm;
}

ActionBroadcaster* getBroadcaster()
{
    return bc.get();
}

UndoManager* getUndoManager()
{
    return pg->getUndoManager();
}

void clearAccessClassStateForTesting()
{
    ui = nullptr;
    ev = nullptr;
    pl = nullptr;
    dv = nullptr;
    pg = nullptr;
    cp = nullptr;
    mc = nullptr;
    ac = nullptr;
    gv = nullptr;
    pm = nullptr;
    bc.reset();
}

MidiBuffer* ExternalProcessorAccessor::getMidiBuffer (GenericProcessor* proc)
{
    return proc->m_currentMidiBuffer;
}

void ExternalProcessorAccessor::injectNumSamples (GenericProcessor* proc, uint16_t dataStream, uint32_t numSamples)
{
    proc->numSamplesInBlock[dataStream] = numSamples;
}

//**Set the MessageCenter for testing only**//
void setMessageCenter (MessageCenter* mc_)
{
    if (pg != nullptr)
        return;
    mc = mc_;
}

} // namespace AccessClass
