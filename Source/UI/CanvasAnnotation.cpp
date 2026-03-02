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

#include "CanvasAnnotation.h"
#include "SNAPGraphViewer.h"

CanvasAnnotation::CanvasAnnotation (const String& text, Colour colour)
    : annotationText (text), noteColour (colour)
{
    setSize (180, 50);

    editor.setMultiLine (true, true);
    editor.setReturnKeyStartsNewLine (true);
    editor.setFont (FontOptions ("Inter", "Regular", 13.0f));
    editor.setJustification (Justification::topLeft);
    editor.setPopupMenuEnabled (false);

    editor.onEscapeKey = [this] { stopEditing(); };
    editor.onFocusLost = [this] { stopEditing(); };

    addChildComponent (editor);
}

void CanvasAnnotation::paint (Graphics& g)
{
    if (isEditing)
        return;

    // Background
    g.setColour (noteColour);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    // Border
    g.setColour (Colours::black.withAlpha (0.15f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);

    // Text
    g.setColour (Colour (0xFF3C3C3C));
    g.setFont (FontOptions ("Inter", "Regular", 13.0f));
    g.drawFittedText (annotationText,
                      getLocalBounds().reduced (8, 6),
                      Justification::topLeft,
                      20);
}

void CanvasAnnotation::mouseDown (const MouseEvent& e)
{
    if (isEditing)
        return;

    if (e.mods.isRightButtonDown())
    {
        PopupMenu menu;
        menu.addItem (1, "Delete Note");

        PopupMenu colourMenu;
        colourMenu.addItem (10, "Yellow");
        colourMenu.addItem (11, "Blue");
        colourMenu.addItem (12, "Green");
        colourMenu.addItem (13, "Pink");
        colourMenu.addItem (14, "White");
        menu.addSubMenu ("Change Colour", colourMenu);

        menu.showMenuAsync (PopupMenu::Options(), [this] (int result)
        {
            if (result == 1)
            {
                if (graphViewer != nullptr)
                    graphViewer->removeAnnotation (this);
            }
            else if (result >= 10 && result <= 14)
            {
                switch (result)
                {
                    case 10: setNoteColour (Colour (0xFFFFFDE7)); break; // Yellow
                    case 11: setNoteColour (Colour (0xFFE3F2FD)); break; // Blue
                    case 12: setNoteColour (Colour (0xFFE8F5E9)); break; // Green
                    case 13: setNoteColour (Colour (0xFFFCE4EC)); break; // Pink
                    case 14: setNoteColour (Colour (0xFFF5F5F5)); break; // White
                }
            }
        });

        return;
    }

    dragger.startDraggingComponent (this, e);
}

void CanvasAnnotation::mouseDrag (const MouseEvent& e)
{
    if (isEditing)
        return;

    dragger.dragComponent (this, e, nullptr);
}

void CanvasAnnotation::mouseDoubleClick (const MouseEvent& e)
{
    startEditing();
}

void CanvasAnnotation::startEditing()
{
    if (isEditing)
        return;

    isEditing = true;

    editor.setBounds (getLocalBounds().reduced (2));
    editor.setText (annotationText, false);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();

    repaint();
}

void CanvasAnnotation::stopEditing()
{
    if (! isEditing)
        return;

    isEditing = false;

    annotationText = editor.getText();
    if (annotationText.isEmpty())
        annotationText = "Double-click to edit";

    editor.setVisible (false);

    // Resize to fit text
    Font f (FontOptions ("Inter", "Regular", 13.0f));
    int textWidth = jmax (120, (int) GlyphArrangement::getStringWidth (f, annotationText) + 24);
    int numLines = jmax (1, (int) std::ceil ((float) textWidth / (float) (getWidth() - 16)));
    int textHeight = jmax (40, numLines * 18 + 16);
    setSize (jmax (getWidth(), jmin (textWidth, 300)), textHeight);

    repaint();
}

void CanvasAnnotation::setText (const String& text)
{
    annotationText = text;
    repaint();
}

void CanvasAnnotation::setNoteColour (Colour c)
{
    noteColour = c;
    repaint();
}

void CanvasAnnotation::saveToXml (XmlElement* xml) const
{
    XmlElement* noteXml = xml->createNewChildElement ("NOTE");
    noteXml->setAttribute ("text", annotationText);
    noteXml->setAttribute ("x", getX());
    noteXml->setAttribute ("y", getY());
    noteXml->setAttribute ("width", getWidth());
    noteXml->setAttribute ("height", getHeight());
    noteXml->setAttribute ("color", noteColour.toString());
}

CanvasAnnotation* CanvasAnnotation::loadFromXml (XmlElement* xml)
{
    String text = xml->getStringAttribute ("text", "Double-click to edit");
    Colour colour = Colour::fromString (xml->getStringAttribute ("color", "FFFFFDE7"));

    auto* note = new CanvasAnnotation (text, colour);
    note->setBounds (xml->getIntAttribute ("x", 100),
                     xml->getIntAttribute ("y", 100),
                     xml->getIntAttribute ("width", 180),
                     xml->getIntAttribute ("height", 50));

    return note;
}
