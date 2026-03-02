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

#ifndef OPENEPHYS_SPLASHSCREEN_H
#define OPENEPHYS_SPLASHSCREEN_H

#include "../../JuceLibraryCode/JuceHeader.h"

/**
    A modern splash screen displayed during application startup.

    Shows the Open Ephys logo, a progress bar, and status text
    to indicate what the application is doing during initialization.
*/
class OpenEphysSplashScreen : public Component,
                              public Timer
{
public:
    /** Constructor */
    OpenEphysSplashScreen();

    /** Destructor */
    ~OpenEphysSplashScreen() override;

    /** Paint the splash screen */
    void paint (Graphics& g) override;

    /** Update the progress bar and status text */
    void setProgress (float progress, const String& statusText);

    /** Begin the fade-out animation, then call onComplete when done */
    void fadeOut (std::function<void()> onComplete = nullptr);

    /** Show the splash screen as a top-level window */
    void show();

    /** Force a repaint so updates are visible during blocking init */
    void pumpAndRepaint();

private:
    void timerCallback() override;

    float currentProgress = 0.0f;
    float displayProgress = 0.0f;
    String statusMessage { "Initializing..." };

    float opacity = 1.0f;
    bool fadingOut = false;
    std::function<void()> fadeOutCallback;

    Image logoImage;

    std::unique_ptr<DocumentWindow> splashWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenEphysSplashScreen)
};

#endif // OPENEPHYS_SPLASHSCREEN_H
