#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "PluginProcessor.h"
#include "BandPanel.h"
#include "FrequencyGuide.h"

// Interactive frequency-response display for the 24-band EQ.
//
//  Double-click empty space ... add a band: Low Cut below 40 Hz, High Cut above 12 kHz,
//                               Bell elsewhere (Ctrl/Cmd: always a Bell)
//  Click node .................. select
//  Drag node ................... frequency (x) / gain (y); Shift = fine
//  Wheel over node ............. Q (Shift = fine)
//  Double-click node ........... reset gain and Q
//  Right-click node ............ context menu
//  Alt-click / Alt-drag node ... solo (audition) the band while held
//  Ctrl (Cmd) while dragging ... snap the frequency to the nearest semitone
//  Right-click empty space ..... display menu (piano strip)
//  Delete / Backspace .......... remove selected band
class EQDisplay : public juce::Component, public juce::TooltipClient, private juce::Timer
{
public:
    explicit EQDisplay (SongsmithEQAudioProcessor&);
    ~EQDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void setSelectedBand (int band);
    int getSelectedBand() const;

    // The Instrument Frequency Guide (EQ-32): see-through listening regions behind the curve and the
    // analyser, with a plain word on each. Drawing only. Hovering one (away from a band) explains it.
    juce::String getTooltip() override;
    juce::String getGuideTooltipAtX (float x) const;                    // "" when the guide is off or nothing is there
    struct GuideLabel { int region; juce::Rectangle<float> bounds; };   // where each word is drawn
    std::vector<GuideLabel> getGuideLabels (juce::Rectangle<float> area) const;
    juce::Rectangle<float> getPlotAreaForGuide() const { return getPlotArea(); }

private:
    void timerCallback() override;

    juce::Rectangle<float> getPlotArea() const;
    float getRange() const;
    float freqToX (float freq) const;
    float xToFreq (float x) const;
    float gainToY (float gainDb) const;
    float yToGain (float y) const;

    bool isUsed (int band) const;
    bool isEnabled (int band) const;
    EQ::FilterType getType (int band) const;
    juce::Point<float> getNodePosition (int band, bool clampToView = true) const;
    int hitTestNode (juce::Point<float> pos) const;
    double getDisplaySampleRate() const;

    void drawGrid (juce::Graphics&, juce::Rectangle<float> area);
    void drawAnalyser (juce::Graphics&, juce::Rectangle<float> area);
    void drawSidechain (juce::Graphics&, juce::Rectangle<float> area);
    void computeCurves (juce::Rectangle<float> area);
    void drawCurves (juce::Graphics&, juce::Rectangle<float> area);
    void drawNodes (juce::Graphics&);
    void drawTooltip (juce::Graphics&, juce::Rectangle<float> area, int band);
    void drawEmptyHint (juce::Graphics&, juce::Rectangle<float> area);
    void drawPianoStrip (juce::Graphics&, juce::Rectangle<float> area);
    void drawGuide (juce::Graphics&, juce::Rectangle<float> area);
    void showDisplayMenu();
    void drawSoloRegion (juce::Graphics&, juce::Rectangle<float> area, int band);
    int getSoloBand() const;
    bool isInCurveView (int band) const;
    float getLiveDynamicDb (int band) const;
    float displayToStoredGain (float displayGainDb) const;   // undo the gain scale

    void showContextMenu (int band);
    void positionPanel();
    void endWheelEdit();

    SongsmithEQAudioProcessor& processor;
    BandPanel panel;

    std::vector<float> lastValues;

    // EQ response cache: recomputed only when parameters or size change, so the
    // analyser can repaint at 30 fps cheaply.
    struct CurveCache
    {
        bool valid = false;
        std::vector<float> xs, total;
        std::vector<int> bands;                   // used bands, in order
        std::vector<bool> enabled;
        std::vector<bool> inView;                 // counted in the main curve for the current curve view
        std::vector<std::vector<float>> single;   // per entry in 'bands'
        std::vector<std::vector<float>> live;     // dynamic bands: response at the live gain (else empty)
    } curves;

    int analyserTick = 0;

    int hoveredBand  = -1;
    float lastMouseX = -1.0f;       // for the guide's tooltip
    int draggingBand = -1;
    bool dragFine = false, gainTouched = false;
    bool momentarySolo = false;     // Alt-drag audition in progress
    int soloBeforeMomentary = -1;   // latched solo to restore on release
    juce::Point<float> anchorMouse, anchorNode;

    int wheelBand = -1;
    juce::uint32 lastWheelTime = 0;

    static constexpr float nodeRadius = 9.0f;   // fits a two-digit band number at 12 px
    static constexpr float hitRadius  = 14.0f;  // click target: a 28 x 28 design-unit circle (at least 24 x 24)

    // High-DPI drawing. 'physicalScale' is physical pixels per design unit for the current paint
    // (UI size x layout step x the display's own scaling), read from the Graphics context.
    float physicalScale = 1.0f;
    float curvesScale = 0.0f;                                  // physicalScale the curve cache was built for
    float onePixel() const noexcept { return 1.0f / physicalScale; }
    float stroke (float designWidth) const noexcept { return juce::jmax (designWidth, onePixel()); }
    float snapToPixel (float v) const noexcept { return std::round (v * physicalScale) / physicalScale; }
    float hairline() const noexcept { return juce::jmax (1.0f, std::floor (physicalScale + 0.25f)) / physicalScale; }
    void verticalLine (juce::Graphics&, float x, float top, float bottom) const;
    void horizontalLine (juce::Graphics&, float y, float left, float right) const;

public:
    // For tests: the band whose node is under 'pos' (display coordinates), or -1, and where a
    // band's node is drawn.
    int findNodeAt (juce::Point<float> pos) const { return hitTestNode (pos); }

    // Adds a band as a double-click on empty graph space does: a Low Cut below
    // EQ::lowCutZoneHz, a High Cut above EQ::highCutZoneHz, else a Bell at the clicked
    // (displayed) gain. forceBell: Ctrl/Cmd held. Returns the band, or -1 when all are used.
    int addBandAt (float freq, float displayGainDb, bool forceBell);
    juce::Point<float> getNodeCentre (int band) const { return getNodePosition (band); }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDisplay)
};
