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

#include "SplashScreen.h"

#include "../../JuceLibraryCode/BinaryData.h"

OpenEphysSplashScreen::OpenEphysSplashScreen()
{
    // Load the brain image from BinaryData
    logoImage = ImageCache::getFromMemory (BinaryData::splash_brain_png,
                                           BinaryData::splash_brain_pngSize);

    setSize (560, 400);
    setOpaque (false);
}

OpenEphysSplashScreen::~OpenEphysSplashScreen()
{
    stopTimer();
}

void OpenEphysSplashScreen::show()
{
    splashWindow = std::make_unique<DocumentWindow> (
        "", Colour (15, 15, 15), 0, false);

    splashWindow->setContentNonOwned (this, true);
    splashWindow->setUsingNativeTitleBar (false);
    splashWindow->setTitleBarHeight (0);
    splashWindow->setDropShadowEnabled (true);
    splashWindow->setResizable (false, false);
    splashWindow->centreWithSize (getWidth(), getHeight());
    splashWindow->setVisible (true);
    splashWindow->toFront (true);
    splashWindow->setAlwaysOnTop (true);

    // Force initial paint
    pumpAndRepaint();
}

void OpenEphysSplashScreen::pumpAndRepaint()
{
    repaint();

    if (auto* mm = MessageManager::getInstanceWithoutCreating())
        mm->runDispatchLoopUntil (10);
}

void OpenEphysSplashScreen::setProgress (float progress, const String& statusText)
{
    currentProgress = jlimit (0.0f, 1.0f, progress);
    displayProgress = currentProgress;
    statusMessage = statusText;
    pumpAndRepaint();
}

void OpenEphysSplashScreen::fadeOut (std::function<void()> onComplete)
{
    fadingOut = true;
    fadeOutCallback = onComplete;
    startTimerHz (60);
}

void OpenEphysSplashScreen::timerCallback()
{
    if (fadingOut)
    {
        opacity -= 0.05f;

        if (opacity <= 0.0f)
        {
            opacity = 0.0f;
            stopTimer();

            if (splashWindow)
                splashWindow->setVisible (false);

            if (fadeOutCallback)
                fadeOutCallback();
        }

        repaint();
    }
}

void OpenEphysSplashScreen::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark background
    g.setOpacity (opacity);
    g.fillAll (Colour (15, 15, 15));

    // Draw the logo centered in the upper portion
    if (logoImage.isValid())
    {
        float logoMaxWidth = bounds.getWidth() * 0.5f;
        float logoMaxHeight = bounds.getHeight() * 0.45f;

        float scale = jmin (logoMaxWidth / logoImage.getWidth(),
                            logoMaxHeight / logoImage.getHeight());

        int drawWidth = (int) (logoImage.getWidth() * scale);
        int drawHeight = (int) (logoImage.getHeight() * scale);
        int drawX = (int) ((bounds.getWidth() - drawWidth) / 2.0f);
        int drawY = 40;

        g.setOpacity (opacity);
        g.drawImage (logoImage,
                     drawX, drawY, drawWidth, drawHeight,
                     0, 0,
                     logoImage.getWidth(), logoImage.getHeight());
    }

    // Title text
    float textY = bounds.getHeight() * 0.52f;

    g.setColour (Colour (230, 230, 230).withAlpha (opacity));
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("SNAP",
                bounds.withY (textY).withHeight (32.0f),
                Justification::centred, false);

    // Version text
    g.setColour (Colour (140, 140, 140).withAlpha (opacity));
    g.setFont (FontOptions (13.0f));
    g.drawText ("v" + JUCEApplication::getInstance()->getApplicationVersion(),
                bounds.withY (textY + 30.0f).withHeight (20.0f),
                Justification::centred, false);

    // Progress bar
    float barX = bounds.getWidth() * 0.15f;
    float barWidth = bounds.getWidth() * 0.7f;
    float barY = bounds.getHeight() * 0.75f;
    float barHeight = 6.0f;

    // Bar background
    g.setColour (Colour (40, 40, 45).withAlpha (opacity));
    g.fillRoundedRectangle (barX, barY, barWidth, barHeight, 3.0f);

    // Bar fill with warm-to-cool gradient
    if (displayProgress > 0.0f)
    {
        float fillWidth = barWidth * displayProgress;

        ColourGradient gradient (Colour (244, 148, 32), barX, barY,           // warm orange
                                 Colour (32, 178, 170), barX + barWidth, barY, // cool teal
                                 false);
        gradient.addColour (0.5, Colour (200, 100, 80)); // warm mid-tone

        g.setGradientFill (gradient);
        g.setOpacity (opacity);
        g.fillRoundedRectangle (barX, barY, fillWidth, barHeight, 3.0f);
    }

    // Progress percentage
    g.setColour (Colour (180, 180, 180).withAlpha (opacity));
    g.setFont (FontOptions (12.0f));
    g.drawText (String ((int) (displayProgress * 100)) + "%",
                barX + barWidth + 8, barY - 4, 40, 14,
                Justification::centredLeft, false);

    // Status text
    g.setColour (Colour (120, 120, 130).withAlpha (opacity));
    g.setFont (FontOptions (12.0f));
    g.drawText (statusMessage,
                bounds.withY (barY + 16.0f).withHeight (20.0f),
                Justification::centred, false);
}
