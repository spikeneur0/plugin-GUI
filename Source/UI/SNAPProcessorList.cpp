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

#include "SNAPProcessorList.h"
#include <stdio.h>

#include "../AccessClass.h"
#include "../Processors/ProcessorGraph/ProcessorGraph.h"
#include "../Processors/ProcessorManager/ProcessorManager.h"
#include "SNAPUIComponent.h"

#include "../Utils/Utils.h"
#include "LookAndFeel/CustomLookAndFeel.h"

SNAPProcessorList::SNAPProcessorList (Viewport* v) : viewport (v),
                                             isDragging (false),
                                             totalHeight (800),
                                             itemHeight (32),
                                             subItemHeight (22),
                                             xBuffer (1),
                                             yBuffer (1),
                                             hoverItem (nullptr),
                                             maximumNameOffset (0)
{
    listFontLight = FontOptions ("Inter", "Semi Bold", 16);
    listFontPlain = FontOptions ("Inter", "Regular", 14);

    ProcessorListItem* sources = new ProcessorListItem ("Sources");
    ProcessorListItem* filters = new ProcessorListItem ("Filters");
    ProcessorListItem* sinks = new ProcessorListItem ("Sinks");
    ProcessorListItem* utilities = new ProcessorListItem ("Utilities");
    ProcessorListItem* record = new ProcessorListItem ("Recording");

    baseItem = std::make_unique<ProcessorListItem> ("Processors");
    baseItem->addSubItem (sources);
    baseItem->addSubItem (filters);
    baseItem->addSubItem (sinks);
    baseItem->addSubItem (utilities);
    baseItem->addSubItem (record);

    baseItem->setParentName ("Processors");

    for (int n = 0; n < baseItem->getNumSubItems(); n++)
    {
        const String category = baseItem->getSubItem (n)->getName();
        baseItem->getSubItem (n)->setParentName (category);
        for (int m = 0; m < baseItem->getSubItem (n)->getNumSubItems(); m++)
        {
            baseItem->getSubItem (n)->getSubItem (m)->setParentName (category);
        }
    }

    openArrowPath.addTriangle (5, 10, 0, 0, 10, 0);
    closedArrowPath.addTriangle (10, 10, 0, 5, 10, 0);

    arrowButton = std::make_unique<CustomArrowButton> (0.0f, 20.0f);
    arrowButton->setToggleState (true, dontSendNotification);
    arrowButton->setClickingTogglesState (false);
    arrowButton->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (arrowButton.get());

    searchButton = std::make_unique<ShapeButton> ("Search", Colours::transparentBlack, Colours::transparentBlack, Colours::transparentBlack);
    searchButton->setOutline (findColour (ThemeColours::controlPanelText), 1.0f);
    String searchIconPath = "M15 15m-4 0a4 4 0 1 0 8 0a4 4 0 1 0 -8 0 M18.5 18.5l2.5 2.5 M4 6h16 M4 12h4 M4 18h4";
    searchButton->setShape (Drawable::parseSVGPath (searchIconPath), true, true, false);
    searchButton->setClickingTogglesState (false);
    searchButton->onClick = [this]
    {
        searchField->setVisible (true);
        searchField->grabKeyboardFocus();
    };
    addAndMakeVisible (searchButton.get());

    searchField = std::make_unique<TextEditor>();
    searchField->setTextToShowWhenEmpty ("Search...", Colours::grey);
    searchField->setFont (FontOptions ("Fira Code", "Regular", 16.0f));
    searchField->setJustification (Justification::centredLeft);
    searchField->setPopupMenuEnabled (false);
    searchField->onTextChange = [this]
    {
        searchText = searchField->getText();
        repaint();
    };
    searchField->onEscapeKey = [this]
    {
        searchField->setText ("");
        searchField->setVisible (false);
    };
    searchField->onFocusLost = [this]
    {
        searchField->setText ("");
        searchField->setVisible (false);
    };
    addChildComponent (searchField.get());
}

void SNAPProcessorList::resized()
{
    setBounds (0, 0, 195, getTotalHeight());
    searchField->setBounds (0, 0, getWidth(), itemHeight);
    searchButton->setBounds (getWidth() - 45, (itemHeight / 2) - 8, 16, 16);
    arrowButton->setBounds (getWidth() - 25, (itemHeight / 2) - 10, 20, 20);
}

void SNAPProcessorList::timerCallback()
{
    maximumNameOffset += 1;

    repaint();
}

void SNAPProcessorList::lookAndFeelChanged()
{
    searchButton->setOutline (findColour (ThemeColours::controlPanelText), 1.0f);
}

bool SNAPProcessorList::isOpen()
{
    return baseItem->isOpen();
}

void SNAPProcessorList::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::windowBackground));
    drawItems (g);
}

void SNAPProcessorList::drawItems (Graphics& g)
{
    totalHeight = yBuffer + itemHeight;

    category = baseItem->getName();

    g.setOrigin (0, yBuffer);
    drawItem (g, baseItem.get());

    if (baseItem->isOpen())
    {
        for (int n = 0; n < baseItem->getNumSubItems(); n++)
        {
            setViewport (g, baseItem->hasSubItems());
            category = baseItem->getSubItem (n)->getName();
            drawItem (g, baseItem->getSubItem (n));

            if (baseItem->getSubItem (n)->isOpen())
            {
                for (int m = 0; m < baseItem->getSubItem (n)->getNumSubItems(); m++)
                {
                    ProcessorListItem* subSubItem = baseItem->getSubItem (n)->getSubItem (m);
                    if (subSubItem->getName().containsIgnoreCase (searchText) || searchText.isEmpty())
                    {
                        setViewport (g, subSubItem->hasSubItems());
                        drawItem (g, subSubItem);
                    }
                }
            }
        }

        totalHeight += yBuffer;
        setSize (getWidth(), totalHeight);
    }
}

void SNAPProcessorList::drawItem (Graphics& g, ProcessorListItem* item)
{
    Colour c = getLookAndFeel().findColour (item->colourId);

    if (item->hasSubItems())
    {
        if (item->getName().equalsIgnoreCase ("Processors"))
        {
            // Top-level header: subtle dark background
            g.setColour (findColour (ThemeColours::controlPanelBackground));
            g.fillRoundedRectangle (2.0f, 1.0f, (float) getWidth() - 4.0f, (float) itemHeight - 2.0f, 4.0f);
        }
        else
        {
            // Category headers: subtle background with left accent bar
            g.setColour (findColour (ThemeColours::componentBackground).withAlpha (0.6f));
            g.fillRoundedRectangle (2.0f, 1.0f, (float) getWidth() - 4.0f, (float) itemHeight - 2.0f, 4.0f);

            // Left accent bar
            g.setColour (c);
            g.fillRoundedRectangle (2.0f, 3.0f, 3.0f, (float) itemHeight - 6.0f, 1.5f);
        }
    }
    else
    {
        // Sub-items: subtle hover highlight
        bool isHovered = (item == hoverItem);
        if (isHovered)
        {
            g.setColour (findColour (ThemeColours::widgetBackground).withAlpha (0.5f));
            g.fillRoundedRectangle (8.0f, 10.0f, (float) getWidth() - 12.0f, (float) subItemHeight - 1.0f, 3.0f);
        }

        // Left accent dot for sub-items
        g.setColour (c.withAlpha (isHovered ? 0.9f : 0.4f));
        g.fillEllipse (12.0f, 10.0f + ((float) subItemHeight - 1.0f) * 0.5f - 2.5f, 5.0f, 5.0f);
    }

    drawItemName (g, item);
}

void SNAPProcessorList::drawItemName (Graphics& g, ProcessorListItem* item)
{
    float offsetX, offsetY;

    if (item->getNumSubItems() == 0)
    {
        // Sub-item (individual processor)
        String name = item->getName();
        bool isHovered = (item == hoverItem);

        g.setColour (findColour (ThemeColours::defaultText).withAlpha (isHovered ? 1.0f : 0.75f));

        float scrollbarOffset = 0.0f;
        float maxWidth = getWidth();

        offsetX = 24.0f;

        g.setFont (listFontPlain);

        if (isHovered)
        {
            maxWidth = GlyphArrangement::getStringWidth (g.getCurrentFont(), name) + 5.0f;

            if (maxWidth + 30 < getWidth() - scrollbarOffset)
            {
                maximumNameOffset = 0;
                stopTimer();
            }
            else if (maximumNameOffset + getWidth() > maxWidth + 30 + scrollbarOffset)
            {
                stopTimer();
            }

            offsetX -= maximumNameOffset;
        }

        offsetY = 0.72f;

        if (item->isSelected())
        {
            g.setColour (findColour (ThemeColours::highlightedFill));
            g.drawText (">", offsetX - 12, 5, 12, itemHeight, Justification::centred, false);
            g.setColour (findColour (ThemeColours::defaultText));
        }
        g.drawText (name, offsetX, 5, maxWidth, itemHeight, Justification::left, false);
    }
    else
    {
        // Category header or root
        if (item->getName().equalsIgnoreCase ("Processors"))
        {
            g.setColour (findColour (ThemeColours::controlPanelText).withAlpha (0.8f));
            g.setFont (listFontLight);
            g.drawText (item->getName().toUpperCase(), 8.0f, 0, getWidth(), itemHeight, Justification::left, false);
        }
        else
        {
            String name = item->getName().toUpperCase();
            offsetX = 12.0f;

            g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.9f));
            g.setFont (listFontLight);
            g.drawText (name, offsetX, 0, getWidth(), itemHeight, Justification::left, false);

            // Arrow indicator
            g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.45f));

            if (item->isOpen())
            {
                g.fillPath (openArrowPath, AffineTransform::translation (getWidth() - 20, itemHeight / 2 - 5));
            }
            else
            {
                g.fillPath (closedArrowPath, AffineTransform::translation (getWidth() - 20, itemHeight / 2 - 5));
            }
        }
    }
}

void SNAPProcessorList::clearSelectionState()
{
    baseItem->setSelected (false);

    for (int n = 0; n < baseItem->getNumSubItems(); n++)
    {
        baseItem->getSubItem (n)->setSelected (false);

        for (int m = 0; m < baseItem->getSubItem (n)->getNumSubItems(); m++)
        {
            baseItem->getSubItem (n)->getSubItem (m)->setSelected (false);
        }
    }
}

ProcessorListItem* SNAPProcessorList::getListItemForYPos (int y)
{
    int bottom = (yBuffer + itemHeight);

    if (y < bottom)
    {
        return baseItem.get();
    }
    else
    {
        if (baseItem->isOpen())
        {
            for (int n = 0; n < baseItem->getNumSubItems(); n++)
            {
                bottom += (yBuffer + itemHeight);

                if (y < bottom)
                {
                    return baseItem->getSubItem (n);
                }

                if (baseItem->getSubItem (n)->isOpen())
                {
                    for (int m = 0; m < baseItem->getSubItem (n)->getNumSubItems(); m++)
                    {
                        ProcessorListItem* subSubItem = baseItem->getSubItem (n)->getSubItem (m);
                        if (subSubItem->getName().containsIgnoreCase (searchText) || searchText.isEmpty())
                        {
                            bottom += (yBuffer + subItemHeight);

                            if (y < bottom)
                            {
                                return baseItem->getSubItem (n)->getSubItem (m);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

void SNAPProcessorList::setViewport (Graphics& g, bool hasSubItems)
{
    int height;

    if (hasSubItems)
    {
        height = itemHeight;
    }
    else
    {
        height = subItemHeight;
    }

    g.setOrigin (0, yBuffer + height);

    totalHeight += yBuffer + height;
}

int SNAPProcessorList::getTotalHeight()
{
    return totalHeight;
}

void SNAPProcessorList::toggleState()
{
    ProcessorListItem* fli = getListItemForYPos (0);
    fli->reverseOpenState();
    LOGC ("Processor List - Toggling state of ", fli->getName());
    arrowButton->setToggleState (fli->isOpen(), dontSendNotification);
    searchButton->setVisible (fli->isOpen());
    AccessClass::getSNAPUIComponent()->childComponentChanged();
    repaint();
}

void SNAPProcessorList::mouseDown (const MouseEvent& e)
{
    isDragging = false;

    juce::Point<int> pos = e.getPosition();
    int xcoord = pos.getX();
    int ycoord = pos.getY();

    ProcessorListItem* listItem = getListItemForYPos (ycoord);

    if (listItem != 0)
    {
        LOGA ("Processor List Selecting: ", listItem->getName());

        if (! listItem->hasSubItems())
        {
            clearSelectionState();
            listItem->setSelected (true);
        }
    }

    if (listItem != 0)
    {
        if (xcoord < getWidth())
        {
            if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
            {
                if (listItem->getName().equalsIgnoreCase ("Sources"))
                {
                    currentColour = ProcessorColour::IDs::SOURCE_COLOUR;
                }
                else if (listItem->getName().equalsIgnoreCase ("Filters"))
                {
                    currentColour = ProcessorColour::IDs::FILTER_COLOUR;
                }
                else if (listItem->getName().equalsIgnoreCase ("Utilities"))
                {
                    currentColour = ProcessorColour::IDs::UTILITY_COLOUR;
                }
                else if (listItem->getName().equalsIgnoreCase ("Sinks"))
                {
                    currentColour = ProcessorColour::IDs::SINK_COLOUR;
                }
                else if (listItem->getName().equalsIgnoreCase ("Recording"))
                {
                    currentColour = ProcessorColour::IDs::RECORD_COLOUR;
                }
                else
                {
                    return;
                }

                int options = 0;
                options += (1 << 1); // showColourAtTop
                options += (1 << 2); // editableColour
                options += (1 << 4); // showColourSpace

                auto* colourSelector = new ColourSelector (options);
                colourSelector->setName ("background");
                colourSelector->setCurrentColour (getLookAndFeel().findColour (currentColour));
                colourSelector->addChangeListener (this);
                colourSelector->addChangeListener (AccessClass::getProcessorGraph());
                colourSelector->setColour (ColourSelector::backgroundColourId, Colours::lightgrey);
                colourSelector->setSize (250, 270);

                juce::Rectangle<int> rect = juce::Rectangle<int> (e.getScreenPosition().getX(),
                                                                  e.getScreenPosition().getY(),
                                                                  1,
                                                                  1);

                CallOutBox& myBox = CallOutBox::launchAsynchronously (std::unique_ptr<Component> (colourSelector),
                                                                      rect,
                                                                      nullptr);
            }
            else
            {
                listItem->reverseOpenState();
            }
        }

        if (listItem == baseItem.get())
        {
            arrowButton->setToggleState (listItem->isOpen(), dontSendNotification);
            searchButton->setVisible (listItem->isOpen());
            AccessClass::getSNAPUIComponent()->childComponentChanged();
        }
    }

    repaint();
}

void SNAPProcessorList::changeListenerCallback (ChangeBroadcaster* source)
{
    ColourSelector* cs = dynamic_cast<ColourSelector*> (source);

    getLookAndFeel().setColour (currentColour, cs->getCurrentColour());

    repaint();
}

void SNAPProcessorList::mouseMove (const MouseEvent& e)
{
    if (e.getMouseDownX() < getWidth() && ! (isDragging))
    {
        ProcessorListItem* listItem = getListItemForYPos (e.getMouseDownY());

        if (hoverItem != listItem) // new hover item
        {
            hoverItem = listItem;
            maximumNameOffset = 0;
            startTimer (33);
        }
    }
}

void SNAPProcessorList::mouseExit (const MouseEvent& e)
{
    hoverItem = nullptr;
    maximumNameOffset = 0;
    stopTimer();
    isDragging = false;

    repaint();
}

void SNAPProcessorList::mouseDrag (const MouseEvent& e)
{
    if (e.getMouseDownX() < getWidth() && ! (isDragging))
    {
        ProcessorListItem* listItem = getListItemForYPos (e.getMouseDownY());

        if (listItem != 0)
        {
            if (! listItem->hasSubItems())
            {
                isDragging = true;

                if (listItem->getName().isNotEmpty())
                {
                    DragAndDropContainer* const dragContainer = DragAndDropContainer::findParentDragContainerFor (this);

                    if (dragContainer != 0)
                    {
                        ScaledImage dragImage (Image (Image::ARGB, 100, 15, true));

                        LOGA ("Processor List - ", listItem->getName(), " drag start.");

                        // Draw the drag image
                        {
                            Graphics g (dragImage.getImage());
                            g.setColour (getLookAndFeel().findColour (listItem->colourId));
                            g.fillAll();
                            g.setColour (Colours::white);
                            g.setFont (FontOptions (14.0f));
                            g.drawSingleLineText (listItem->getName(), 10, 12);
                        }

                        dragImage.getImage().multiplyAllAlphas (0.6f);

                        juce::Point<int> imageOffset (20, 10);

                        Array<var> dragData;
                        dragData.add (true); // fromSNAPProcessorList
                        dragData.add (listItem->getName()); // pluginName
                        dragData.add (listItem->index); // processorIndex
                        dragData.add (listItem->pluginType); // pluginType
                        dragData.add (listItem->processorType); // processorType

                        dragContainer->startDragging (dragData, this, dragImage, true, &imageOffset);
                    }
                }
            }
        }
    }
}

void SNAPProcessorList::saveStateToXml (XmlElement* xml)
{
    XmlElement* processorListState = xml->createNewChildElement ("PROCESSORLIST");

    for (int i = 0; i < 7; i++)
    {
        XmlElement* colourState = processorListState->createNewChildElement ("COLOUR");

        int id;

        switch (i)
        {
            case 0:
                id = ProcessorColour::IDs::PROCESSOR_COLOUR;
                break;
            case 1:
                id = ProcessorColour::IDs::SOURCE_COLOUR;
                break;
            case 2:
                id = ProcessorColour::IDs::FILTER_COLOUR;
                break;
            case 3:
                id = ProcessorColour::IDs::SINK_COLOUR;
                break;
            case 4:
                id = ProcessorColour::IDs::UTILITY_COLOUR;
                break;
            case 5:
                id = ProcessorColour::IDs::RECORD_COLOUR;
                break;
            case 6:
                id = ProcessorColour::IDs::AUDIO_COLOUR;
                break;
            default:
                // do nothing
                ;
        }

        Colour c = getLookAndFeel().findColour (id);

        colourState->setAttribute ("ID", (int) id);
        colourState->setAttribute ("R", (int) c.getRed());
        colourState->setAttribute ("G", (int) c.getGreen());
        colourState->setAttribute ("B", (int) c.getBlue());
    }
}

void SNAPProcessorList::loadStateFromXml (XmlElement* xml)
{
    for (auto* xmlNode : xml->getChildIterator())
    {
        if (xmlNode->hasTagName ("PROCESSORLIST"))
        {
            for (auto* colourNode : xmlNode->getChildIterator())
            {
                int ID = colourNode->getIntAttribute ("ID");

                // Ignore the processor colour
                if (ID == ProcessorColour::IDs::PROCESSOR_COLOUR)
                    continue;

                getLookAndFeel().setColour (ID,
                                            Colour (
                                                colourNode->getIntAttribute ("R"),
                                                colourNode->getIntAttribute ("G"),
                                                colourNode->getIntAttribute ("B")));
            }
        }
    }

    repaint();

    AccessClass::getProcessorGraph()->refreshColours();
}

Array<Colour> SNAPProcessorList::getColours()
{
    Array<Colour> c;

    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::PROCESSOR_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::SOURCE_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::FILTER_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::SINK_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::UTILITY_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::RECORD_COLOUR));
    c.add (getLookAndFeel().findColour (ProcessorColour::IDs::AUDIO_COLOUR));
    return c;
}

void SNAPProcessorList::setColours (Array<Colour> c)
{
    for (int i = 0; i < c.size(); i++)
    {
        switch (i)
        {
            case 0:
                getLookAndFeel().setColour (ProcessorColour::IDs::PROCESSOR_COLOUR, c[i]);
                break;
            case 1:
                getLookAndFeel().setColour (ProcessorColour::IDs::SOURCE_COLOUR, c[i]);
                break;
            case 2:
                getLookAndFeel().setColour (ProcessorColour::IDs::FILTER_COLOUR, c[i]);
                break;
            case 3:
                getLookAndFeel().setColour (ProcessorColour::IDs::SINK_COLOUR, c[i]);
                break;
            case 4:
                getLookAndFeel().setColour (ProcessorColour::IDs::UTILITY_COLOUR, c[i]);
                break;
            case 5:
                getLookAndFeel().setColour (ProcessorColour::IDs::RECORD_COLOUR, c[i]);
                break;
            case 6:
                getLookAndFeel().setColour (ProcessorColour::IDs::AUDIO_COLOUR, c[i]);
            default:; // do nothing
        }
    }
}

void SNAPProcessorList::fillItemList()
{
    LOGD ("SNAPProcessorList::fillItemList()");

    baseItem->getSubItem (0)->clearSubItems(); //Sources
    baseItem->getSubItem (1)->clearSubItems(); //Filters
    baseItem->getSubItem (2)->clearSubItems(); //Sinks
    baseItem->getSubItem (3)->clearSubItems(); //Utilities
    baseItem->getSubItem (4)->clearSubItems(); //Record

    for (auto pluginType : ProcessorManager::getAvailablePluginTypes())
    {
        for (int i = 0; i < ProcessorManager::getNumProcessorsForPluginType (pluginType); i++)
        {
            Plugin::Description description = ProcessorManager::getPluginDescription (pluginType, i);

            LOGD ("Processor List - creating item for ", description.name);

            ProcessorListItem* item = new ProcessorListItem (description.name,
                                                             i,
                                                             description.type,
                                                             description.processorType);

            if (description.processorType == Plugin::Processor::SOURCE)

                baseItem->getSubItem (0)->addSubItem (item);

            else if (description.processorType == Plugin::Processor::FILTER)

                baseItem->getSubItem (1)->addSubItem (item);

            else if (description.processorType == Plugin::Processor::SINK)

                baseItem->getSubItem (2)->addSubItem (item);

            else if (description.processorType == Plugin::Processor::UTILITY
                     || description.processorType == Plugin::Processor::MERGER
                     || description.processorType == Plugin::Processor::SPLITTER
                     || description.processorType == Plugin::Processor::AUDIO_MONITOR)

                baseItem->getSubItem (3)->addSubItem (item);

            else if (description.processorType == Plugin::Processor::RECORD_NODE)

                baseItem->getSubItem (4)->addSubItem (item);
        }
    }

    for (int n = 0; n < baseItem->getNumSubItems(); n++)
    {
        const String category = baseItem->getSubItem (n)->getName();

        baseItem->getSubItem (n)->setParentName (category);

        for (int m = 0; m < baseItem->getSubItem (n)->getNumSubItems(); m++)
        {
            baseItem->getSubItem (n)->getSubItem (m)->setParentName (category);
        }
    }
}

Array<String> SNAPProcessorList::getItemList()
{
    Array<String> listOfProcessors;

    for (int i = 0; i < 5; i++)
    {
        int numSubItems = baseItem->getSubItem (i)->getNumSubItems();

        ProcessorListItem* subItem = baseItem->getSubItem (i);

        for (int j = 0; j < numSubItems; j++)
        {
            listOfProcessors.addIfNotAlreadyThere (subItem->getSubItem (j)->getName());
        }
    }

    return listOfProcessors;
}

Plugin::Description SNAPProcessorList::getItemDescriptionfromList (const String& name)
{
    Plugin::Description description;

    for (int i = 0; i < 5; i++)
    {
        int numSubItems = baseItem->getSubItem (i)->getNumSubItems();

        ProcessorListItem* subItem = baseItem->getSubItem (i);

        for (int j = 0; j < numSubItems; j++)
        {
            if (name.equalsIgnoreCase (subItem->getSubItem (j)->getName()))
            {
                description.fromSNAPProcessorList = true;
                description.index = subItem->getSubItem (j)->index;
                description.name = subItem->getSubItem (j)->getName();
                description.type = subItem->getSubItem (j)->pluginType;
                description.processorType = subItem->getSubItem (j)->processorType;

                break;
            }
        }
    }

    return description;
}

// ===================================================================

ProcessorListItem::ProcessorListItem (const String& name_,
                                      int index_,
                                      Plugin::Type pluginType_,
                                      Plugin::Processor::Type processorType_) : index (index_),
                                                                                pluginType (pluginType_),
                                                                                processorType (processorType_),
                                                                                selected (false),
                                                                                open (true),
                                                                                name (name_)
{
}

bool ProcessorListItem::hasSubItems()
{
    if (subItems.size() > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int ProcessorListItem::getNumSubItems()
{
    return subItems.size();
}

ProcessorListItem* ProcessorListItem::getSubItem (int index)
{
    return subItems[index];
}

void ProcessorListItem::clearSubItems()
{
    subItems.clear();
}

void ProcessorListItem::addSubItem (ProcessorListItem* newItem)
{
    subItems.add (newItem);
}

void ProcessorListItem::removeSubItem (int index)
{
    subItems.remove (index);
}

bool ProcessorListItem::isOpen()
{
    return open;
}

void ProcessorListItem::setOpen (bool t)
{
    open = t;
}

const String& ProcessorListItem::getName()
{
    return name;
}

const String& ProcessorListItem::getParentName()
{
    return parentName;
}

void ProcessorListItem::setParentName (const String& name)
{
    parentName = name;

    if (parentName.equalsIgnoreCase ("Processors"))
    {
        colourId = ProcessorColour::IDs::PROCESSOR_COLOUR;
    }
    else if (parentName.equalsIgnoreCase ("Filters"))
    {
        colourId = ProcessorColour::IDs::FILTER_COLOUR;
    }
    else if (parentName.equalsIgnoreCase ("Sinks"))
    {
        colourId = ProcessorColour::IDs::SINK_COLOUR;
    }
    else if (parentName.equalsIgnoreCase ("Sources"))
    {
        colourId = ProcessorColour::IDs::SOURCE_COLOUR;
    }
    else if (parentName.equalsIgnoreCase ("Recording"))
    {
        colourId = ProcessorColour::IDs::RECORD_COLOUR;
    }
    else if (parentName.equalsIgnoreCase ("Audio"))
    {
        colourId = ProcessorColour::IDs::AUDIO_COLOUR;
    }
    else
    {
        colourId = ProcessorColour::IDs::UTILITY_COLOUR;
    }
}
