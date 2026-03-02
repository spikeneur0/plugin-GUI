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

#ifndef __CANVASANNOTATION_H__
#define __CANVASANNOTATION_H__

#include "../../JuceLibraryCode/JuceHeader.h"

class SNAPGraphViewer;

/**
    A draggable, editable sticky-note annotation for the graph viewer canvas.

    Researchers can place these on the signal chain canvas to document
    their workflows with explanatory text.

    @see SNAPGraphViewer
*/
class CanvasAnnotation : public Component
{
public:
    /** Constructor */
    CanvasAnnotation (const String& text = "Double-click to edit",
                      Colour colour = Colour (0xFFFFFDE7));

    /** Destructor */
    ~CanvasAnnotation() override {}

    /** Paint the annotation */
    void paint (Graphics& g) override;

    /** Handle mouse down for dragging and context menu */
    void mouseDown (const MouseEvent& e) override;

    /** Handle mouse drag for repositioning */
    void mouseDrag (const MouseEvent& e) override;

    /** Handle double-click to enter edit mode */
    void mouseDoubleClick (const MouseEvent& e) override;

    /** Save annotation state to XML */
    void saveToXml (XmlElement* xml) const;

    /** Load annotation state from XML */
    static CanvasAnnotation* loadFromXml (XmlElement* xml);

    /** Get the annotation text */
    String getText() const { return annotationText; }

    /** Set the annotation text */
    void setText (const String& text);

    /** Set the note colour */
    void setNoteColour (Colour c);

    /** Get the note colour */
    Colour getNoteColour() const { return noteColour; }

    /** Owner graph viewer (set after creation) */
    SNAPGraphViewer* graphViewer = nullptr;

private:
    TextEditor editor;
    String annotationText;
    Colour noteColour;

    ComponentDragger dragger;
    bool isEditing = false;

    void startEditing();
    void stopEditing();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasAnnotation);
};

#endif // __CANVASANNOTATION_H__
