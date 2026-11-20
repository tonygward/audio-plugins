#include "AnalyzerComponent.h"

namespace Songsmith::Analyzer
{
    namespace
    {
        void styleLabel (juce::Label& l, float height, UI::Weight weight, juce::Colour colour, juce::Justification j = juce::Justification::centredLeft)
        {
            l.setFont (UI::font (height, weight));
            l.setColour (juce::Label::textColourId, colour);
            l.setJustificationType (j);
            l.setBorderSize ({ 0, 0, 0, 0 });
            l.setMinimumHorizontalScale (1.0f);
            l.setInterceptsMouseClicks (false, false);
        }
    }

    //==============================================================================
    ReadingTile::ReadingTile()
    {
        for (auto* l : { &caption, &value, &note })
        {
            addAndMakeVisible (*l);
        }
        styleLabel (caption, UI::Type::hint, UI::Weight::Medium, UI::Colours::textDim);
        styleLabel (value, UI::Type::title, UI::Weight::SemiBold, UI::Colours::text);
        styleLabel (note, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim);
    }

    void ReadingTile::setReading (const Reading& r)
    {
        reading = r;
        caption.setText (r.caption, juce::dontSendNotification);
        value.setText (r.value, juce::dontSendNotification);
        note.setText (r.note, juce::dontSendNotification);
        setTooltip (r.tooltip);
        setTitle (r.caption);
        setDescription (r.value + (r.note.isNotEmpty() ? ". " + r.note : juce::String()));
        // "Not available" is shown quieter than a real measurement.
        value.setColour (juce::Label::textColourId, r.value == "Not available" ? UI::Colours::textDim : UI::Colours::text);
    }

    void ReadingTile::paint (juce::Graphics& g)
    {
        UI::LookAndFeelBase::drawWell (g, getLocalBounds().toFloat().reduced (0.5f), UI::Colours::recess);
    }

    void ReadingTile::resized()
    {
        auto r = getLocalBounds().reduced (14, 10);
        caption.setBounds (r.removeFromTop (16));
        r.removeFromTop (4);
        value.setBounds (r.removeFromTop (28));
        r.removeFromTop (2);
        note.setBounds (r.removeFromTop (18));
    }

    //==============================================================================
    AnalyzerComponent::AnalyzerComponent()
        : runner ([this] (const juce::File& f, const FileAnalysis::Result& r) { finished (f, r); })
    {
        setLookAndFeel (&lnf);
        setOpaque (true);
        setWantsKeyboardFocus (true);
        setTitle (ANALYZER_PRODUCT_NAME);
        setDescription ("Open a finished WAV, AIFF, FLAC or MP3 file to see how loud it is and how it was recorded.");

        styleLabel (dropLabel, UI::Type::title, UI::Weight::Regular, UI::Colours::text, juce::Justification::centred);
        dropLabel.setText ("Drop a WAV, AIFF, FLAC or MP3 file here", juce::dontSendNotification);
        styleLabel (orLabel, UI::Type::control, UI::Weight::Regular, UI::Colours::textDim, juce::Justification::centred);
        orLabel.setText ("or", juce::dontSendNotification);
        styleLabel (privacyLabel, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim, juce::Justification::centred);
        privacyLabel.setText ("Your file is only read. It is never changed, copied or sent anywhere.", juce::dontSendNotification);
        styleLabel (fileNameLabel, UI::Type::heading, UI::Weight::SemiBold, UI::Colours::text);
        styleLabel (fileDetailLabel, UI::Type::control, UI::Weight::Regular, UI::Colours::textDim);
        styleLabel (statusLabel, UI::Type::control, UI::Weight::Regular, UI::Colours::textDim);
        styleLabel (footerHint, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim, juce::Justification::centredRight);
        styleLabel (versionLabel, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim);
        versionLabel.setText (juce::String ("v") + ANALYZER_VERSION, juce::dontSendNotification);
        versionLabel.setTitle ("Version");

        chooseButton.setButtonText ("Choose File...");
        chooseButton.setTooltip ("Choose a WAV, AIFF, FLAC or MP3 file from your computer. Shortcut: Ctrl+O");
        chooseButton.onClick = [this] { chooseFile(); };
        anotherButton.setButtonText ("Analyse Another File");
        anotherButton.setTooltip ("Choose another WAV, AIFF, FLAC or MP3 file. You can also drop one on this window. Shortcut: Ctrl+O");
        anotherButton.onClick = [this] { chooseFile(); };
        cancelButton.setButtonText ("Cancel");
        cancelButton.setTooltip ("Stop reading this file and go back. Shortcut: Escape");
        cancelButton.onClick = [this] { reset(); };

        progressBar.setTitle ("Progress");
        progressBar.setPercentageDisplay (true);

        for (int i = 0; i < Layout::tileColumns * Layout::tileRows; ++i)
        {
            tiles.push_back (std::make_unique<ReadingTile>());
            addChildComponent (*tiles.back());
        }

        for (auto* c : std::initializer_list<juce::Component*> { &dropLabel, &orLabel, &privacyLabel, &fileNameLabel, &fileDetailLabel, &statusLabel,
                                                                 &footerHint, &versionLabel, &chooseButton, &anotherButton, &cancelButton, &progressBar, &guidance })
            addChildComponent (*c);
        versionLabel.setVisible (true);

        chooseButton.setExplicitFocusOrder (1);
        anotherButton.setExplicitFocusOrder (1);
        cancelButton.setExplicitFocusOrder (1);

        tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 500);

        setSize (Layout::designWidth, Layout::designHeight);
        updateVisibility();
    }

    AnalyzerComponent::~AnalyzerComponent()
    {
        stopTimer();
        runner.cancel();
        setLookAndFeel (nullptr);
    }

    //==============================================================================
    void AnalyzerComponent::analyse (const juce::File& f)
    {
        footerHint.setText (extraNote, juce::dontSendNotification);
        if (f.isDirectory())
        {
            FileAnalysis::Result r;
            r.error = "That is a folder. Choose a WAV, AIFF, FLAC or MP3 file.";
            showResult (f, r);
            return;
        }
        file = f;
        progress = 0.0;
        state = State::reading;
        result = {};
        advice = {};
        runner.start (f);
        fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);
        statusLabel.setText ("Reading and measuring the whole file. A long file can take a little while.", juce::dontSendNotification);
        startTimerHz (15);
        updateVisibility();
        resized();
        repaint();
    }

    void AnalyzerComponent::finished (const juce::File& f, const FileAnalysis::Result& r)
    {
        stopTimer();
        showResult (f, r);
    }

    void AnalyzerComponent::showResult (const juce::File& f, const FileAnalysis::Result& r)
    {
        stopTimer();
        file = f;
        result = r;
        advice = FileAdvice::adviseOn (r);
        state = State::showing;

        fileNameLabel.setText (f.getFileName(), juce::dontSendNotification);
        fileDetailLabel.setText (r.opened ? describeFile (r) : "This file could not be analysed.", juce::dontSendNotification);

        const auto readings = readingsFor (r);
        for (size_t i = 0; i < tiles.size(); ++i)
            if (i < readings.size())
                tiles[i]->setReading (readings[i]);
        guidance.setAdvice (advice, extraMeasurements (advice));
        footerHint.setText (extraNote.isNotEmpty() ? extraNote : juce::String ("You can also drop another file anywhere on this window."), juce::dontSendNotification);

        updateVisibility();
        resized();
        repaint();
        anotherButton.grabKeyboardFocus();
    }

    void AnalyzerComponent::reset()
    {
        stopTimer();
        runner.cancel();
        state = State::waiting;
        file = juce::File();
        result = {};
        advice = {};
        extraNote = {};
        footerHint.setText ({}, juce::dontSendNotification);
        guidance.getView().clear();
        updateVisibility();
        resized();
        repaint();
        chooseButton.grabKeyboardFocus();
    }

    void AnalyzerComponent::timerCallback()
    {
        progress = (double) runner.getProgress();
    }

    void AnalyzerComponent::chooseFile()
    {
        if (chooser != nullptr)
            return;   // the chooser is already open
        const auto start = file.exists() ? file.getParentDirectory() : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Choose a WAV, AIFF, FLAC or MP3 file", start, "*.wav;*.aif;*.aiff;*.flac;*.mp3");
        juce::Component::SafePointer<AnalyzerComponent> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;
                                  const auto chosen = fc.getResult();
                                  safe->chooser.reset();
                                  if (chosen != juce::File())
                                  {
                                      safe->extraNote = {};
                                      safe->analyse (chosen);
                                  }
                              });
    }

    //==============================================================================
    bool AnalyzerComponent::isInterestedInFileDrag (const juce::StringArray& files) { return files.size() > 0; }

    void AnalyzerComponent::filesDropped (const juce::StringArray& files, int, int)
    {
        dragOver = false;
        if (files.isEmpty())
            return;
        extraNote = files.size() > 1 ? "You dropped " + juce::String (files.size()) + " files. Only the first one is analysed, one at a time." : juce::String();
        analyse (juce::File (files[0]));
    }

    bool AnalyzerComponent::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress ('o', juce::ModifierKeys::commandModifier, 0))
        {
            chooseFile();
            return true;
        }
        if (key == juce::KeyPress::escapeKey && state == State::reading)
        {
            reset();
            return true;
        }
        return false;
    }

    //==============================================================================
    void AnalyzerComponent::updateVisibility()
    {
        const bool waiting = state == State::waiting, reading = state == State::reading, showing = state == State::showing;
        for (auto* c : std::initializer_list<juce::Component*> { &dropLabel, &orLabel, &privacyLabel, &chooseButton })
            c->setVisible (waiting);
        for (auto* c : std::initializer_list<juce::Component*> { &statusLabel, &progressBar, &cancelButton })
            c->setVisible (reading);
        fileNameLabel.setVisible (reading || showing);
        fileDetailLabel.setVisible (showing);
        anotherButton.setVisible (showing);
        footerHint.setVisible (showing);
        guidance.setVisible (showing);
        for (size_t i = 0; i < tiles.size(); ++i)
            tiles[i]->setVisible (showing && result.opened && i < readingsFor (result).size());
    }

    void AnalyzerComponent::paint (juce::Graphics& g)
    {
        g.fillAll (UI::Colours::panel);

        const juce::Rectangle<float> bar (0.0f, 0.0f, (float) getWidth(), (float) Layout::titleBarHeight);
        UI::LookAndFeelBase::drawTitleBar (g, bar);
        UI::drawTitle (g, titleBounds.toFloat(), ANALYZER_PRODUCT_NAME, UI::Colours::text, Colours::accent);

        if (state == State::waiting)
        {
            UI::LookAndFeelBase::drawWell (g, dropPanel.toFloat().reduced (0.5f), UI::Colours::recess);
            g.setColour (dragOver ? Colours::accent : UI::Colours::edge);
            g.drawRoundedRectangle (dropPanel.toFloat().reduced (10.0f), 6.0f, dragOver ? 2.0f : 1.0f);
        }
        else if (state == State::showing)
        {
            UI::LookAndFeelBase::drawWell (g, guidanceArea.toFloat().reduced (0.5f), UI::Colours::recess);
            if (dragOver)
            {
                g.setColour (Colours::accent);
                g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 6.0f, 2.0f);
            }
        }
        else if (dragOver)
        {
            g.setColour (Colours::accent);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 6.0f, 2.0f);
        }
    }

    void AnalyzerComponent::resized()
    {
        using namespace Layout;
        const int w = getWidth();
        titleBounds = { (int) UI::titleInset, 0, w - 2 * (int) UI::titleInset, titleBarHeight };

        const int bodyTop = titleBarHeight + 16, bodyWidth = w - 2 * margin, footerTop = designHeight - 32;
        versionLabel.setBounds (margin, footerTop, 200, 20);

        // Waiting: one large drop panel with the choose button in the middle.
        dropPanel = { margin, bodyTop, bodyWidth, footerTop - bodyTop - 8 };
        {
            const int cx = dropPanel.getCentreX(), cy = dropPanel.getCentreY();
            dropLabel.setBounds (cx - 300, cy - 70, 600, 28);
            orLabel.setBounds (cx - 60, cy - 28, 120, 20);
            chooseButton.setBounds (cx - 90, cy + 0, 180, 34);
            privacyLabel.setBounds (cx - 300, cy + 52, 600, 20);
        }

        // Reading: the file's name, what is happening, a progress bar and a Cancel button.
        fileNameLabel.setBounds (margin, bodyTop, bodyWidth, 24);
        statusLabel.setBounds (margin, bodyTop + 36, bodyWidth, 20);
        progressBar.setBounds (margin, bodyTop + 68, bodyWidth, 22);
        cancelButton.setBounds (margin, bodyTop + 106, 110, buttonHeight);

        // Showing: the name, the six reading boxes, the guidance and the button to go again.
        fileDetailLabel.setBounds (margin, bodyTop + 26, bodyWidth, 20);
        const int tilesTop = bodyTop + 56;
        for (size_t i = 0; i < tiles.size(); ++i)
        {
            const int col = (int) i % tileColumns, row = (int) i / tileColumns;
            tiles[i]->setBounds (margin + col * (tileWidth + tileGap), tilesTop + row * (tileHeight + tileRowGap), tileWidth, tileHeight);
        }
        // A file that could not be read has no reading boxes, so its advice starts straight under the name.
        const bool unreadable = state == State::showing && ! result.opened;
        const int guidanceTop = unreadable ? tilesTop : tilesTop + tileRows * tileHeight + (tileRows - 1) * tileRowGap + 16;
        const int buttonTop = footerTop - 8 - buttonHeight;
        guidanceArea = { margin, guidanceTop, bodyWidth, buttonTop - 12 - guidanceTop };
        guidance.setBounds (guidanceArea.reduced (14, 10));
        anotherButton.setBounds (margin, buttonTop, 190, buttonHeight);
        footerHint.setBounds (margin + 206, buttonTop, bodyWidth - 206, buttonHeight);
    }
}
