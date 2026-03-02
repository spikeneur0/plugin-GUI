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

#include "SNAPUIComponent.h"

#include <stdio.h>

#include "../Audio/SNAPAudioComponent.h"
#include "../AutoUpdater.h"
#include "../SNAPMainWindow.h"
#include "../Processors/MessageCenter/MessageCenter.h"
#include "../Processors/ProcessorGraph/ProcessorGraph.h"
#include "ConsoleViewer.h"
#include "SNAPControlPanel.h"
#include "SNAPDataViewport.h"
#include "SNAPEditorViewport.h"
#include "SNAPGraphViewer.h"
#include "InfoLabel.h"
#include "MessageCenterButton.h"
#include "SNAPProcessorList.h"

SNAPUIComponent::SNAPUIComponent (SNAPMainWindow* mainWindow_,
                          ProcessorGraph* processorGraph_,
                          SNAPAudioComponent* audioComponent_,
                          SNAPControlPanel* controlPanel_,
                          ConsoleViewer* consoleViewer_,
                          CustomLookAndFeel* customLookAndFeel_)
    : mainWindow (mainWindow_),
      processorGraph (processorGraph_),
      audio (audioComponent_),
      controlPanel (controlPanel_),
      customLookAndFeel (customLookAndFeel_)
{
    messageCenterEditor = (MessageCenterEditor*) processorGraph->getMessageCenter()->createEditor();
    LOGD ("Created message center editor.");

    infoLabel = std::make_unique<InfoLabel>();
    LOGD ("Created info label.");

    graphViewer = std::make_unique<SNAPGraphViewer>();
    LOGD ("Created graph viewer.");

    consoleViewer.reset (consoleViewer_);

    dataViewport = std::make_unique<SNAPDataViewport>();
    addChildComponent (dataViewport.get());
    LOGD ("Created data viewport.");

    signalChainTabComponent = std::make_unique<SignalChainTabComponent>();
    addAndMakeVisible (signalChainTabComponent.get());

    editorViewport = new SNAPEditorViewport (signalChainTabComponent.get());

    LOGD ("Created editor viewport.");

    showHideSNAPEditorViewportButton = std::make_unique<ShowHideEditorViewportButton>();
    showHideSNAPEditorViewportButton->addListener (this);
    showHideSNAPEditorViewportButton->setToggleState (true, dontSendNotification);
    addAndMakeVisible (showHideSNAPEditorViewportButton.get());

    addAndMakeVisible (controlPanel);

    LOGD ("Created control panel.");

    processorList = std::make_unique<SNAPProcessorList> (&processorListViewport);
    processorListViewport.setViewedComponent (processorList.get(), false);
    processorListViewport.setScrollBarsShown (false, false);
    addAndMakeVisible (&processorListViewport);

    messageCenterButton.addListener (this);
    addAndMakeVisible (messageCenterEditor);
    addAndMakeVisible (&messageCenterButton);

    processorList->setVisible (true);
    processorList->setBounds (0, 0, 195, processorList->getTotalHeight());
    LOGD ("Created processor list.");

    setBounds (0, 0, 500, 400);

    AccessClass::setSNAPUIComponent (this);

    getSNAPProcessorList()->fillItemList();

    addInfoTab();
    addGraphTab();
    addConsoleTab();

    popupManager = std::make_unique<PopupManager>();

    bubbleMsgComponent = std::make_unique<BubbleMessageComponent> (500);
    bubbleMsgComponent->setAllowedPlacement (BubbleComponent::above | BubbleComponent::below);
    addChildComponent (bubbleMsgComponent.get());
}

SNAPUIComponent::~SNAPUIComponent()
{
    //dataViewport->removeTab(0); // get rid of tab for InfoLabel

    if (pluginInstaller)
    {
        pluginInstaller->setVisible (false);
        delete pluginInstaller;
    }

    if (consoleWindow)
    {
        consoleWindow->removeListener (this);
    }

    // setLookAndFeel(nullptr);
}

/** Returns a pointer to the SNAPEditorViewport. */
SNAPEditorViewport* SNAPUIComponent::getSNAPEditorViewport()
{
    return editorViewport;
}

/** Returns a pointer to the SNAPProcessorList. */
SNAPProcessorList* SNAPUIComponent::getSNAPProcessorList()
{
    return processorList.get();
}

/** Returns a pointer to the SNAPDataViewport. */
SNAPDataViewport* SNAPUIComponent::getSNAPDataViewport()
{
    return dataViewport.get();
}

/** Returns a pointer to the ProcessorGraph. */
ProcessorGraph* SNAPUIComponent::getProcessorGraph()
{
    return processorGraph;
}

/** Returns a pointer to the SNAPGraphViewer. */
SNAPGraphViewer* SNAPUIComponent::getSNAPGraphViewer()
{
    return graphViewer.get();
}

/** Returns a pointer to the SNAPControlPanel. */
SNAPControlPanel* SNAPUIComponent::getSNAPControlPanel()
{
    return controlPanel;
}

/** Returns a pointer to the SNAPUIComponent. */
SNAPUIComponent* SNAPUIComponent::getSNAPUIComponent()
{
    return this;
}

/** Returns a pointer to the SNAPAudioComponent. */
SNAPAudioComponent* SNAPUIComponent::getSNAPAudioComponent()
{
    return audio;
}

SNAPPluginInstaller* SNAPUIComponent::getSNAPPluginInstaller()
{
    if (pluginInstaller == nullptr)
    {
        pluginInstaller = new SNAPPluginInstaller (false);
    }
    return pluginInstaller;
}

PopupManager* SNAPUIComponent::getPopupManager()
{
    return popupManager.get();
}

void SNAPUIComponent::buttonClicked (Button* button)
{
    if (button == &messageCenterButton)
    {
        messageCenterButton.switchState();

        messageCenterIsCollapsed = ! messageCenterIsCollapsed;

        resized();
    }
    else if (button == showHideSNAPEditorViewportButton.get())
    {
        resized();
    }
}

void SNAPUIComponent::resized()
{
    int w = getWidth();
    int h = getHeight();

    if (showHideSNAPEditorViewportButton != nullptr)
    {
        showHideSNAPEditorViewportButton->setBounds (w - 230, h - 40, 225, 35);
    }

    if (signalChainTabComponent != nullptr)
    {
        if (showHideSNAPEditorViewportButton->getToggleState() && ! signalChainTabComponent->isVisible())
        {
            signalChainTabComponent->setVisible (true);
        }

        else if (! showHideSNAPEditorViewportButton->getToggleState() && signalChainTabComponent->isVisible())
        {
            signalChainTabComponent->setVisible (false);
        }

        signalChainTabComponent->setBounds (6, h - 200, w - 11, 160);
    }

    if (controlPanel != nullptr)
    {
        int controlPanelWidth;
        int addHeight = 0;
        int leftBound;

        if (! editorViewport->isSignalChainLocked())
        {
            if (w >= 460)
            {
                leftBound = 202;
                controlPanelWidth = w - 210;
            }
            else
            {
                leftBound = w - 258;
                controlPanelWidth = w - leftBound;
            }
        }
        else
        {
            leftBound = 6;
            controlPanelWidth = w - 12;
        }

        if (controlPanelWidth < 750)
        {
            addHeight = 750 - controlPanelWidth;

            if (addHeight > 32)
                addHeight = 32;
        }

        if (controlPanelWidth < 570)
        {
            addHeight = 32 + 570 - controlPanelWidth;

            if (addHeight > 64)
                addHeight = 64;
        }

        if (controlPanel->isOpen())
            controlPanel->setBounds (leftBound, 6, controlPanelWidth, 64 + addHeight);
        else
            controlPanel->setBounds (leftBound, 6, controlPanelWidth, 32 + addHeight);
    }

    if (processorList != nullptr)
    {
        if (editorViewport->isSignalChainLocked())
        {
            processorList->setVisible (false);
        }

        else
        {
            processorList->setVisible (true);

            if (processorList->isOpen())
            {
                if (showHideSNAPEditorViewportButton->getToggleState())
                    processorListViewport.setBounds (5, 5, 195, h - 210);
                else
                    processorListViewport.setBounds (5, 5, 195, h - 50);

                processorListViewport.setScrollBarsShown (false, false, true, false);
            }
            else
            {
                processorListViewport.setBounds (5, 5, 195, 34);
                processorListViewport.setScrollBarsShown (false, false);
                processorListViewport.setViewPosition (0, 0);
            }

            if (w < 460)
                processorListViewport.setBounds (5 - 460 + getWidth(), 5, 195, processorList->getHeight());
        }
    }

    if (dataViewport != nullptr)
    {
        int left, top, width, height;
        left = 6;
        top = 40;

        if (processorList->isOpen() && ! editorViewport->isSignalChainLocked())
            left = processorListViewport.getX() + processorListViewport.getWidth() + 2;
        else
            left = 6;

        top = controlPanel->getHeight() + 8;

        if (showHideSNAPEditorViewportButton->getToggleState())
            height = h - top - 205;
        else
            height = h - top - 45;

        width = w - left - 5;

        dataViewport->setBounds (left, top, width, height);

        if (h < 200)
            dataViewport->setVisible (false);
        else
            dataViewport->setVisible (true);
    }

    if (messageCenterEditor != nullptr)
    {
        if (messageCenterIsCollapsed)
        {
            messageCenterEditor->collapse();
            messageCenterEditor->setBounds (6, h - 35, w - 241, 30);
        }
        else
        {
            messageCenterEditor->expand();
            messageCenterEditor->setBounds (6, 6, w - 241, getHeight() - 11);
        }
    }

    //if (messageCenterIsCollapsed)
    // {
    messageCenterButton.setBounds ((w - 241) / 2, h - 35, 30, 30);
    // } else {
    //     messageCenterButton.setBounds((w-241)/2,h-305,30,30);
    // }

    // for debugging purposes:
    if (false)
    {
        dataViewport->setVisible (false);
        editorViewport->setVisible (false);
        processorList->setVisible (false);
        messageCenterEditor->setVisible (false);
        controlPanel->setVisible (false);
        showHideSNAPEditorViewportButton->setVisible (false);
    }
}

void SNAPUIComponent::disableCallbacks()
{
    //sendActionMessage("Data acquisition terminated.");
    controlPanel->disableCallbacks();
}

void SNAPUIComponent::disableSNAPDataViewport()
{
    dataViewport->disableConnectionToSNAPEditorViewport();
}

void SNAPUIComponent::childComponentChanged()
{
    resized();
}

void SNAPUIComponent::setTheme (ColourTheme t)
{
    customLookAndFeel->setTheme (t);

    mainWindow->currentTheme = t;
    mainWindow->repaintWindow();

    getProcessorGraph()->refreshColours();

    processorList->repaint();
}

ColourTheme SNAPUIComponent::getTheme()
{
    return mainWindow->currentTheme;
}

void SNAPUIComponent::addInfoTab()
{
    if (! infoTabIsOpen)
    {
        dataViewport->addTab ("Info", infoLabel.get(), 0);
        infoTabIsOpen = true;
    }
}

void SNAPUIComponent::addGraphTab()
{
    if (! graphViewerIsOpen)
    {
        dataViewport->addTab ("Graph", graphViewer->getGraphViewport(), 1);
        graphViewerIsOpen = true;
    }
}

void SNAPUIComponent::addConsoleTab()
{
    if (consoleViewer != nullptr && ! consoleOpenInTab)
    {
        if (consoleOpenInWindow)
        {
            consoleWindow->closeButtonPressed();
        }

        dataViewport->addTab ("Console", consoleViewer.get(), 2);
        consoleOpenInTab = true;
    }
}

void SNAPUIComponent::openConsoleWindow()
{
    if (consoleWindow == nullptr)
    {
        consoleWindow = std::make_unique<DataWindow> (nullptr, "SNAP Console");
        consoleWindow->addListener (this);
        consoleWindow->setLookAndFeel (customLookAndFeel);
        consoleWindow->setBackgroundColour (findColour (ThemeColours::windowBackground));
        consoleWindow->setSize (700, 700);
    }

    if (consoleOpenInTab)
    {
        dataViewport->removeTab (2);
        consoleOpenInTab = false;
    }

    consoleWindow->setContentNonOwned (consoleViewer.get(), false);
    consoleWindow->setVisible (true);
    consoleWindow->toFront (true);
    consoleOpenInWindow = true;
}

void SNAPUIComponent::paintOverChildren (Graphics& g)
{
    if (isBusy)
    {
        g.setColour (Colours::white.withAlpha (0.4f));
        g.fillAll();
    }
}

void SNAPUIComponent::setUIBusy (bool busy)
{
    isBusy = busy;
    repaint();
}

void SNAPUIComponent::checkForPluginUpdates()
{
    // Run the check on a background thread to avoid blocking the UI
    Thread::launch ([this]()
                    {
                        int numUpdates = SNAPPluginInstaller::checkForPluginUpdates();

                        if (numUpdates > 0)
                        {
                            MessageManager::callAsync ([this, numUpdates]()
                            {
                                String message = String (numUpdates) + " plugin update" 
                                                 + (numUpdates > 1 ? "s" : "") + " available";

                                AttributedString s;
                                s.setText (message);
                                s.setColour (findColour (ThemeColours::defaultText));
                                s.setJustification (Justification::left);
                                s.setWordWrap (AttributedString::WordWrap::byWord);
                                s.setFont (FontOptions ("Inter", "Regular", 16.0f));

                                bubbleMsgComponent->showAt ({5, 5, 195, 32}, s, 4000);
                            });
                        } });
}

void SNAPUIComponent::showBubbleMessage (Component* component, const String& message)
{
    AttributedString s;
    s.setText (message);
    s.setColour (findColour (ThemeColours::defaultText));
    s.setJustification (Justification::left);
    s.setWordWrap (AttributedString::WordWrap::byWord);
    s.setFont (FontOptions ("Inter", "Regular", 16.0f));

    bubbleMsgComponent->showAt (component, s, 3000);
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// MENU BAR METHODS

StringArray SNAPUIComponent::getMenuBarNames()
{
    const char* const names[] = { "File", "Edit", "View", "Help", 0 };

    return StringArray (names);
}

PopupMenu SNAPUIComponent::getMenuForIndex (int menuIndex, const String& menuName)
{
    ApplicationCommandManager* commandManager = &(mainWindow->commandManager);

    PopupMenu menu;

    if (menuIndex == 0)
    {
        menu.addCommandItem (commandManager, openSignalChain);
        menu.addSeparator();
        menu.addCommandItem (commandManager, saveSignalChain);
        menu.addCommandItem (commandManager, saveSignalChainAs);
        menu.addSeparator();
        menu.addCommandItem (commandManager, reloadOnStartup);
        menu.addSeparator();
        menu.addCommandItem (commandManager, toggleHttpServer);
        menu.addSeparator();
        menu.addCommandItem (commandManager, openDefaultConfigWindow);
        menu.addSeparator();
        menu.addCommandItem (commandManager, openSNAPPluginInstaller);

#if ! JUCE_MAC
        menu.addSeparator();
        menu.addCommandItem (commandManager, StandardApplicationCommandIDs::quit);
#endif
    }
    else if (menuIndex == 1)
    {
        menu.addCommandItem (commandManager, undo);
        menu.addCommandItem (commandManager, redo);
        menu.addSeparator();
        menu.addCommandItem (commandManager, copySignalChain);
        menu.addCommandItem (commandManager, pasteSignalChain);
        menu.addSeparator();
        menu.addCommandItem (commandManager, clearSignalChain);
        menu.addSeparator();
        menu.addCommandItem (commandManager, lockSignalChain);
    }
    else if (menuIndex == 2)
    {
        PopupMenu clockModeMenu;
        clockModeMenu.addCommandItem (commandManager, setClockModeDefault);
        clockModeMenu.addCommandItem (commandManager, setClockModeHHMMSS);

        PopupMenu clockReferenceTimeMenu;
        clockReferenceTimeMenu.addCommandItem (commandManager, setClockReferenceTimeCumulative);
        clockReferenceTimeMenu.addCommandItem (commandManager, setClockReferenceTimeAcqStart);

        PopupMenu themeMenu;
        themeMenu.addCommandItem (commandManager, setColourThemeLight);
        themeMenu.addCommandItem (commandManager, setColourThemeMedium);
        themeMenu.addCommandItem (commandManager, setColourThemeDark);

        menu.addCommandItem (commandManager, toggleSNAPProcessorList);
        menu.addCommandItem (commandManager, toggleSignalChain);
        menu.addCommandItem (commandManager, toggleFileInfo);
        menu.addCommandItem (commandManager, toggleInfoTab);
        menu.addCommandItem (commandManager, toggleSNAPGraphViewer);
        menu.addCommandItem (commandManager, toggleConsoleViewer);
        menu.addCommandItem (commandManager, showMessageWindow);
        menu.addSeparator();
        menu.addSubMenu ("Clock display mode", clockModeMenu);
        menu.addSubMenu ("Clock reference time", clockReferenceTimeMenu);
        menu.addSeparator();
        menu.addSubMenu ("Theme", themeMenu);
        menu.addSeparator();

#if JUCE_WINDOWS
        PopupMenu rendererMenu;
        rendererMenu.addCommandItem (commandManager, setSoftwareRenderer);
        rendererMenu.addCommandItem (commandManager, setDirect2DRenderer);
        menu.addSubMenu ("Renderer", rendererMenu);
        menu.addSeparator();
#endif
        menu.addCommandItem (commandManager, resizeWindow);
    }
    else if (menuIndex == 3)
    {
        menu.addCommandItem (commandManager, showHelp);
        menu.addSeparator();
        menu.addCommandItem (commandManager, checkForUpdates);
    }

    return menu;
}

void SNAPUIComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    //
}

// ApplicationCommandTarget methods

ApplicationCommandTarget* SNAPUIComponent::getNextCommandTarget()
{
    // this will return the next parent component that is an ApplicationCommandTarget (in this
    // case, there probably isn't one, but it's best to use this method anyway).
    return findFirstTargetParentComponent();
}

void SNAPUIComponent::getAllCommands (Array<CommandID>& commands)
{
    const CommandID ids[] = { openSignalChain,
                              saveSignalChain,
                              saveSignalChainAs,
                              loadPluginSettings,
                              savePluginSettings,
                              reloadOnStartup,
                              undo,
                              redo,
                              copySignalChain,
                              pasteSignalChain,
                              clearSignalChain,
                              lockSignalChain,
                              toggleSNAPProcessorList,
                              toggleSignalChain,
                              toggleHttpServer,
                              toggleFileInfo,
                              toggleInfoTab,
                              toggleSNAPGraphViewer,
                              toggleConsoleViewer,
                              showMessageWindow,
                              setClockModeDefault,
                              setClockModeHHMMSS,
                              setClockReferenceTimeCumulative,
                              setClockReferenceTimeAcqStart,
                              showHelp,
                              checkForUpdates,
                              resizeWindow,
                              openSNAPPluginInstaller,
                              openDefaultConfigWindow,
                              setColourThemeLight,
                              setColourThemeMedium,
                              setColourThemeDark,
                              setSoftwareRenderer,
                              setDirect2DRenderer };

    commands.addArray (ids, numElementsInArray (ids));
}

void SNAPUIComponent::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    bool acquisitionStarted = getSNAPAudioComponent()->callbacksAreActive();

    int renderer = 0;

    if (auto peer = getPeer())
    {
        renderer = peer->getCurrentRenderingEngine();
    }

    switch (commandID)
    {
        case openSignalChain:
            result.setInfo ("Open...", "Open a saved signal chain.", "General", 0);
            result.addDefaultKeypress ('O', ModifierKeys::commandModifier);
            result.setActive (! acquisitionStarted);
            break;

        case saveSignalChain:
            result.setInfo ("Save", "Save the current signal chain.", "General", 0);
            result.addDefaultKeypress ('S', ModifierKeys::commandModifier);
            break;

        case saveSignalChainAs:
            result.setInfo ("Save as...", "Save the current signal chain with a new name.", "General", 0);
            result.addDefaultKeypress ('S', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
            break;

        case loadPluginSettings:
            result.setInfo ("Load plugin settings...", "Load saved plugin settings.", "General", 0);
            result.setActive (! acquisitionStarted);
            break;

        case savePluginSettings:
            result.setInfo ("Save plugin settings...", "Save the settings of the selected plugin.", "General", 0);
            break;

        case reloadOnStartup:
            result.setInfo ("Reload on startup", "Load the last used configuration on startup.", "General", 0);
            result.setActive (! acquisitionStarted);
            result.setTicked (mainWindow->shouldReloadOnStartup);
            break;

        case toggleHttpServer:
            result.setInfo ("Enable HTTP Server", "Enable the HTTP server on port 37497.", "General", 0);
            result.setActive (! acquisitionStarted);
            result.setTicked (mainWindow->shouldEnableHttpServer);
            break;

        case undo:
        {
            result.setInfo ("Undo", "Undo the last action.", "General", 0);
            result.addDefaultKeypress ('Z', ModifierKeys::commandModifier);
            bool undoDisabled = acquisitionStarted && AccessClass::getUndoManager()->getUndoDescription().contains ("Disabled during acquisition");
            result.setActive (! undoDisabled && AccessClass::getUndoManager()->canUndo() && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;
        }

        case redo:
        {
            result.setInfo ("Redo", "Undo the last action.", "General", 0);
            result.addDefaultKeypress ('Z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
            bool redoDisabled = acquisitionStarted && AccessClass::getUndoManager()->getRedoDescription().contains ("Disabled during acquisition");
            result.setActive (! redoDisabled && AccessClass::getUndoManager()->canRedo() && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;
        }

        case copySignalChain:
            result.setInfo ("Copy", "Copy selected processors.", "General", 0);
            result.addDefaultKeypress ('C', ModifierKeys::commandModifier);
            result.setActive (! acquisitionStarted && getSNAPEditorViewport()->editorIsSelected() && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;

        case pasteSignalChain:
            result.setInfo ("Paste", "Paste processors.", "General", 0);
            result.addDefaultKeypress ('V', ModifierKeys::commandModifier);
            result.setActive (! acquisitionStarted && getSNAPEditorViewport()->canPaste() && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;

        case clearSignalChain:
            result.setInfo ("Clear signal chain", "Clear the current signal chain.", "General", 0);
            result.addDefaultKeypress (KeyPress::backspaceKey, ModifierKeys::commandModifier);
            result.setActive (! getSNAPEditorViewport()->isSignalChainEmpty() && ! acquisitionStarted && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;

        case lockSignalChain:
            result.setInfo ("Lock signal chain", "Disable signal chain edits.", "General", 0);
            result.addDefaultKeypress ('L', ModifierKeys::commandModifier);
            result.setTicked (getSNAPEditorViewport()->isSignalChainLocked());
            break;

        case toggleSNAPProcessorList:
            result.setInfo ("Processor List", "Show/hide Processor List.", "General", 0);
            result.addDefaultKeypress ('P', ModifierKeys::shiftModifier);
            result.setActive (! editorViewport->isSignalChainLocked());
            result.setTicked (processorList->isOpen());
            break;

        case toggleSignalChain:
            result.setInfo ("Signal Chain", "Show/hide Signal Chain.", "General", 0);
            result.addDefaultKeypress ('S', ModifierKeys::shiftModifier);
            result.setTicked (showHideSNAPEditorViewportButton->getToggleState());
            break;

        case toggleFileInfo:
            result.setInfo ("File Info", "Show/hide File Info.", "General", 0);
            result.addDefaultKeypress ('F', ModifierKeys::shiftModifier);
            result.setTicked (controlPanel->isOpen());
            break;

        case toggleInfoTab:
            result.setInfo ("Info Tab", "Show/hide Info Tab.", "General", 0);
            result.addDefaultKeypress ('I', ModifierKeys::shiftModifier);
            result.setTicked (infoTabIsOpen);
            break;

        case toggleSNAPGraphViewer:
            result.setInfo ("Graph Viewer", "Show/hide Graph Viewer.", "General", 0);
            result.addDefaultKeypress ('G', ModifierKeys::shiftModifier);
            result.setTicked (graphViewerIsOpen);
            break;

        case toggleConsoleViewer:
            result.setInfo ("Console", "Show/hide built-in console.", "General", 0);
            result.addDefaultKeypress ('C', ModifierKeys::shiftModifier);
            result.setTicked (consoleOpenInTab || consoleOpenInWindow);
#ifdef DEBUG
            result.setActive (false);
#endif
            break;

        case showMessageWindow:
            result.setInfo ("Message Window", "Show Message Window.", "General", 0);
            result.addDefaultKeypress ('M', ModifierKeys::shiftModifier);
            result.setTicked (false);
            break;

        case setClockModeDefault:
            result.setInfo ("Default", "Set clock mode to default.", "General", 0);
            result.setTicked (controlPanel->clock->getMode() == Clock::DEFAULT);
            break;

        case setClockModeHHMMSS:
            result.setInfo ("HH:MM:SS", "Set clock mode to HH:MM:SS.", "General", 0);
            result.setTicked (controlPanel->clock->getMode() == Clock::HHMMSS);
            break;

        case setClockReferenceTimeCumulative:
            result.setInfo ("Cumulative", "Set clock reference time to cumulative.", "General", 0);
            result.setTicked (controlPanel->clock->getReferenceTime() == Clock::CUMULATIVE);
            break;

        case setClockReferenceTimeAcqStart:
            result.setInfo ("Acquisition start", "Set clock to reset when acquisition starts.", "General", 0);
            result.setTicked (controlPanel->clock->getReferenceTime() == Clock::ACQUISITION_START);
            break;

        case setColourThemeLight:
            result.setInfo ("Light", "Set colour theme Light.", "General", 0);
            result.setTicked (getTheme() == ColourTheme::LIGHT);
            break;

        case setColourThemeMedium:
            result.setInfo ("Medium", "Set colour theme default.", "General", 0);
            result.setTicked (getTheme() == ColourTheme::MEDIUM);
            break;

        case setColourThemeDark:
            result.setInfo ("Dark", "Set colour theme dark.", "General", 0);
            result.setTicked (getTheme() == ColourTheme::DARK);
            break;

        case openSNAPPluginInstaller:
            result.setInfo ("Plugin Installer", "Launch the plugin installer.", "General", 0);
            result.addDefaultKeypress ('P', ModifierKeys::commandModifier);
            break;

        case openDefaultConfigWindow:
            result.setInfo ("Load a default config", "Load a default configuration", "General", 0);
            result.addDefaultKeypress ('D', ModifierKeys::commandModifier);
            result.setActive (! acquisitionStarted && ! getSNAPEditorViewport()->isSignalChainLocked());
            break;

        case showHelp:
            result.setInfo ("Online documentation...", "Launch the GUI's documentation website in a browser.", "General", 0);
            result.setActive (true);
            break;

        case checkForUpdates:
            result.setInfo ("Check for updates...", "Checks if a newer version of the GUI is available", "General", 0);
            result.setActive (true);
            break;

        case resizeWindow:
            result.setInfo ("Reset window bounds", "Reset window bounds", "General", 0);
            break;

        case setSoftwareRenderer:
            result.setInfo ("Software (CPU)", "Use the software renderer.", "General", 0);
            result.setTicked (renderer == 0);
            break;

        case setDirect2DRenderer:
            result.setInfo ("Direct2D (GPU)", "Use the Direct2D renderer.", "General", 0);
            result.setTicked (renderer == 1);
            break;

        default:
            break;
    };
}

bool SNAPUIComponent::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case openSignalChain:
        {
            FileChooser fc ("Choose a settings file to load...",
                            CoreServices::getDefaultUserSaveDirectory(),
                            "*",
                            true);

            if (fc.browseForFileToOpen())
            {
                currentConfigFile = fc.getResult();
                getSNAPEditorViewport()->loadState (currentConfigFile);
            }
            else
            {
                sendActionMessage ("No file selected.");
            }

            break;
        }
        case saveSignalChain:
        {
            if (currentConfigFile.exists())
            {
                sendActionMessage (getSNAPEditorViewport()->saveState (currentConfigFile));
            }
            else
            {
                FileChooser fc ("Choose the file name...",
                                CoreServices::getDefaultUserSaveDirectory(),
                                ".xml",
                                true);

                if (fc.browseForFileToSave (true))
                {
                    currentConfigFile = fc.getResult();
                    LOGD (currentConfigFile.getFileName());
                    sendActionMessage (getSNAPEditorViewport()->saveState (currentConfigFile));
                }
                else
                {
                    sendActionMessage ("No file chosen.");
                }
            }

            break;
        }

        case saveSignalChainAs:
        {
            FileChooser fc ("Choose the file name...",
                            CoreServices::getDefaultUserSaveDirectory(),
                            "*",
                            true);

            if (fc.browseForFileToSave (true))
            {
                currentConfigFile = fc.getResult();
                LOGD (currentConfigFile.getFileName());
                sendActionMessage (getSNAPEditorViewport()->saveState (currentConfigFile));
            }
            else
            {
                sendActionMessage ("No file chosen.");
            }

            break;
        }

        case loadPluginSettings:
        {
            FileChooser fc ("Choose a settings file to load...",
                            CoreServices::getDefaultUserSaveDirectory(),
                            "*",
                            true);

            if (fc.browseForFileToOpen())
            {
                currentConfigFile = fc.getResult();
                sendActionMessage (getSNAPEditorViewport()->loadPluginState (currentConfigFile));
            }
            else
            {
                sendActionMessage ("No file selected.");
            }

            break;
        }
        case savePluginSettings:
        {
            FileChooser fc ("Choose the file name...",
                            CoreServices::getDefaultUserSaveDirectory(),
                            "*",
                            true);

            if (fc.browseForFileToSave (true))
            {
                currentConfigFile = fc.getResult();
                LOGD (currentConfigFile.getFileName());
                sendActionMessage (getSNAPEditorViewport()->savePluginState (currentConfigFile));
            }
            else
            {
                sendActionMessage ("No file chosen.");
            }

            break;
        }

        case reloadOnStartup:
        {
            mainWindow->shouldReloadOnStartup = ! mainWindow->shouldReloadOnStartup;
        }
        break;

        case toggleHttpServer:

            mainWindow->shouldEnableHttpServer = ! mainWindow->shouldEnableHttpServer;

            if (mainWindow->shouldEnableHttpServer)
            {
                mainWindow->enableHttpServer();
            }
            else
            {
                mainWindow->disableHttpServer();
            }
            break;

        case undo:
        {
            AccessClass::getProcessorGraph()->getUndoManager()->undo();
            break;
        }

        case redo:
        {
            AccessClass::getProcessorGraph()->getUndoManager()->redo();
            break;
        }

        case copySignalChain:
        {
            getSNAPEditorViewport()->copySelectedEditors();
            break;
        }

        case pasteSignalChain:
        {
            getSNAPEditorViewport()->paste();
            break;
        }

        case clearSignalChain:
        {
            getSNAPEditorViewport()->clearSignalChain();
            break;
        }

        case lockSignalChain:
        {
            if (getSNAPEditorViewport()->isSignalChainLocked())
            {
                getSNAPEditorViewport()->lockSignalChain (false);
                resized();
                //processorList->unlock();
            }
            else
            {
                getSNAPEditorViewport()->lockSignalChain (true);
                resized();
                //processorList->lock();
            }

            break;
        }

        case showHelp:
        {
            URL url = URL ("https://open-ephys.github.io/gui-docs/");
            url.launchInDefaultBrowser();
            break;
        }

        case checkForUpdates:
        {
            LatestVersionCheckerAndUpdater::getInstance()->checkForNewVersion (false, mainWindow);
            break;
        }

        case toggleSNAPProcessorList:
            processorList->toggleState();
            break;

        case toggleFileInfo:
            controlPanel->toggleState();
            break;

        case toggleInfoTab:
            if (infoTabIsOpen)
                dataViewport->removeTab (0);
            else
            {
                dataViewport->addTab ("Info", infoLabel.get(), 0);
                infoTabIsOpen = true;
            }

            break;

        case toggleSNAPGraphViewer:
            if (graphViewerIsOpen)
                dataViewport->removeTab (1);
            else
            {
                dataViewport->addTab ("Graph", graphViewer->getGraphViewport(), 1);
                graphViewerIsOpen = true;
            }

            break;

        case toggleConsoleViewer:
        {
            if (consoleViewer == nullptr)
                break;

            if (consoleOpenInTab)
                dataViewport->removeTab (2);
            else if (consoleOpenInWindow)
            {
                consoleWindow->closeButtonPressed();
            }
            else
            {
                addConsoleTab();
            }

            break;
        }

        case showMessageWindow:
            messageWindow = std::make_unique<MessageWindow>();

            break;

        case toggleSignalChain:
            showHideSNAPEditorViewportButton->setToggleState (! showHideSNAPEditorViewportButton->getToggleState(), sendNotification);
            break;

        case resizeWindow:
            mainWindow->centreWithSize (1200, 800);
            break;

        case setClockModeDefault:
            controlPanel->clock->setMode (Clock::DEFAULT);
            break;

        case setClockModeHHMMSS:
            controlPanel->clock->setMode (Clock::HHMMSS);
            break;

        case setClockReferenceTimeCumulative:
            controlPanel->clock->setReferenceTime (Clock::CUMULATIVE);
            break;

        case setClockReferenceTimeAcqStart:
            controlPanel->clock->setReferenceTime (Clock::ACQUISITION_START);
            break;

        case setColourThemeLight:
            setTheme (ColourTheme::LIGHT);
            break;

        case setColourThemeMedium:
            setTheme (ColourTheme::MEDIUM);
            break;

        case setColourThemeDark:
            setTheme (ColourTheme::DARK);
            break;

        case setSoftwareRenderer:
        {
            setRenderingEngine (0);
            break;
        }

        case setDirect2DRenderer:
        {
            setRenderingEngine (1);
            break;
        }

        case openSNAPPluginInstaller:
        {
            if (pluginInstaller != nullptr)
            {
                delete pluginInstaller;
            }

            pluginInstaller = new SNAPPluginInstaller();
            pluginInstaller->setVisible (true);
            pluginInstaller->toFront (true);
            break;
        }

        case openDefaultConfigWindow:
        {
            defaultConfigWindow = std::make_unique<DefaultConfigWindow>();
            break;
        }

        default:
            break;
    }

    return true;
}

void SNAPUIComponent::setRenderingEngine (int index)
{
#if JUCE_WINDOWS
    for (int i = 0; i < TopLevelWindow::getNumTopLevelWindows(); i++)
    {
        if (TopLevelWindow* window = TopLevelWindow::getTopLevelWindow (i))
        {
            if (auto* peer = window->getPeer())
            {
                peer->setCurrentRenderingEngine (index);
                window->repaint();
            }
        }
    }
#endif
}

void SNAPUIComponent::saveStateToXml (XmlElement* xml)
{
    XmlElement* uiComponentState = xml->createNewChildElement ("UICOMPONENT");
    uiComponentState->setAttribute ("isSNAPProcessorListOpen", processorList->isOpen());
    uiComponentState->setAttribute ("isSNAPEditorViewportOpen", showHideSNAPEditorViewportButton->getToggleState());
    uiComponentState->setAttribute ("consoleOpenInWindow", consoleOpenInWindow);

    if (consoleOpenInWindow)
    {
        uiComponentState->setAttribute ("consoleWindowBounds", consoleWindow->getWindowStateAsString());
    }
}

void SNAPUIComponent::loadStateFromXml (XmlElement* xml)
{
    for (auto* xmlNode : xml->getChildWithTagNameIterator ("UICOMPONENT"))
    {
        bool isSNAPProcessorListOpen = xmlNode->getBoolAttribute ("isSNAPProcessorListOpen");
        bool isSNAPEditorViewportOpen = xmlNode->getBoolAttribute ("isSNAPEditorViewportOpen");

        if (! isSNAPProcessorListOpen)
        {
            processorList->toggleState();
        }

        showHideSNAPEditorViewportButton->setToggleState (isSNAPEditorViewportOpen, sendNotification);

        bool consoleWindowState = xmlNode->getBoolAttribute ("consoleOpenInWindow");

        if (consoleWindowState)
        {
            openConsoleWindow();
            consoleWindow->restoreWindowStateFromString (xmlNode->getStringAttribute ("consoleWindowBounds"));
        }
    }
}

Array<String> SNAPUIComponent::getRecentlyUsedFilenames()
{
    return controlPanel->getRecentlyUsedFilenames();
}

void SNAPUIComponent::setRecentlyUsedFilenames (const Array<String>& filenames)
{
    controlPanel->setRecentlyUsedFilenames (filenames);
}

void SNAPUIComponent::windowClosed (const String& windowName)
{
    if (windowName == consoleWindow->getName())
    {
        consoleOpenInWindow = false;
    }
}

Component* SNAPUIComponent::findComponentByIDRecursive (Component* parent, const String& componentID)
{
    if (! parent)
        return nullptr;

    // Check if the current component matches the ID
    if (parent->getComponentID() == componentID)
    {
        return parent;
    }

    // Recursively search in child components
    for (auto* child : parent->getChildren())
    {
        Component* found = findComponentByIDRecursive (child, componentID);
        if (found)
        {
            return found;
        }
    }

    // Not found in this branch of the hierarchy
    return nullptr;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

ShowHideEditorViewportButton::ShowHideEditorViewportButton() : ToggleButton()
{
    buttonFont = FontOptions ("Inter", "Medium", 14);
    setTooltip ("Show/hide signal chain");

    arrow = std::make_unique<CustomArrowButton> (MathConstants<float>::pi / 2);

    arrow->setBounds (195, 7, 22, 22);
    arrow->addListener (this);
    addAndMakeVisible (arrow.get());
}

void ShowHideEditorViewportButton::buttonClicked (Button* button)
{
    setToggleState (! getToggleState(), sendNotification);
}

void ShowHideEditorViewportButton::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::controlPanelBackground));

    g.setColour (findColour (ThemeColours::outline).withAlpha (0.25f));
    g.fillRect (0, 0, getWidth(), 1);

    g.setColour (findColour (ThemeColours::controlPanelText).withAlpha (0.7f));
    g.setFont (buttonFont);
    g.drawText ("SIGNAL CHAIN", 10, 0, getWidth(), getHeight(), Justification::left, false);

    arrow->setToggleState (! getToggleState(), dontSendNotification);
}
