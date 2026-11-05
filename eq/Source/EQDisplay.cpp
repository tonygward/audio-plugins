#include "EQDisplay.h"
#include "EQLookAndFeel.h"
#include "SvfFilter.h"
#include "NoteNames.h"

namespace
{
    constexpr float rightGutter  = 36.0f;   // dB labels
    constexpr float bottomGutter = 20.0f;   // frequency labels
    constexpr float gainPad      = 12.0f;   // keeps the outer gain lines inside the plot

    juce::String formatFreq (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String (hz, hz >= 100.0f ? 0 : 1) + " Hz";
    }

    juce::String formatGain (float db)
    {
        return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
    }
}

EQDisplay::EQDisplay (SongsmithEQAudioProcessor& p) : processor (p), panel (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);

    addChildComponent (panel);
    panel.onBandRemoved = [this] { setSelectedBand (-1); };
    panel.onUserMove = [this] (juce::Point<int> p) { movePanelTo (p); };

    processor.analyser.activeViewers.fetch_add (1);
    startTimerHz (60);
}

EQDisplay::~EQDisplay()
{
    processor.analyser.activeViewers.fetch_sub (1);
    endWheelEdit();
    if (processor.edits.isEditing())
        processor.edits.endEdit();
}

//==============================================================================
bool EQDisplay::isUsed (int band) const     { return band >= 0 && processor.isBandUsed (band); }
bool EQDisplay::isEnabled (int band) const  { return processor.getBandValue (band, EQ::Param::Enabled) >= 0.5f; }

EQ::FilterType EQDisplay::getType (int band) const
{
    return EQ::typeFromIndex ((int) std::lround (processor.getBandValue (band, EQ::Param::Type)));
}

int EQDisplay::getSoloBand() const
{
    const int b = processor.soloBand.load();
    return isUsed (b) ? b : -1;
}

// Curve view: 0 = all bands (the combined curve as before), otherwise one channel
// (1 Left, 2 Right, 3 Mid, 4 Side): Stereo bands plus the bands set to that channel.
// It's a per-channel view of each band's own response, not a full L/R <-> M/S
// cross-model, so e.g. a Mid band is not shown in the Left view.
bool EQDisplay::isInCurveView (int band) const
{
    const int view = processor.curveView.load();
    const int mode = (int) std::lround (processor.getBandValue (band, EQ::Param::Stereo));
    return view == 0 || mode == 0 || mode == view || processor.getMainBusNumInputChannels() < 2;
}

int EQDisplay::getSelectedBand() const
{
    const int b = processor.selectedBand.load();
    return isUsed (b) ? b : -1;
}

void EQDisplay::setSelectedBand (int band)
{
    processor.selectedBand.store (isUsed (band) ? band : -1);
    positionPanel();
    repaint();
}

// Repaint whenever anything that affects the drawing changes
// (UI edits, host automation, undo/redo, state restore).
void EQDisplay::timerCallback()
{
    // Grows to fit on the first tick (a fixed count once overran the vector).
    if (lastValues.empty())
        lastValues.reserve ((size_t) EQ::numBands * (EQ::numBandParams + 1) + 16);

    bool changed = false;
    size_t i = 0;
    auto check = [&] (float v)
    {
        if (i == lastValues.size()) { lastValues.push_back (v); changed = true; }
        else if (lastValues[i] != v) { lastValues[i] = v; changed = true; }
        ++i;
    };

    for (int b = 0; b < EQ::numBands; ++b)
    {
        for (int p = 0; p < EQ::numBandParams; ++p)
            check (processor.getBandValue (b, (EQ::Param) p));
        check (processor.isBandUsed (b) ? 1.0f : 0.0f);
    }
    check ((float) processor.selectedBand.load());
    check ((float) getSoloBand());
    check ((float) processor.curveView.load());
    check (processor.showPianoStrip.load() ? 1.0f : 0.0f);
    check (processor.getGainScale());
    for (int b = 0; b < EQ::numBands; ++b)
        check (isUsed (b) ? getLiveDynamicDb (b) : 0.0f);
    check (processor.analyserTilt.load());
    check (processor.analyserTopDb.load());
    check (processor.analyserBottomDb.load());
    check (processor.analyser.isFrozen() ? 1.0f : 0.0f);
    check (processor.analyser.isPeakHold() ? 1.0f : 0.0f);
    check (processor.displayRangeDb.load());
    check ((float) getWidth());
    check ((float) getHeight());
    check ((float) getDisplaySampleRate());
    check ((float) processor.getOversamplingFactor());

    if (wheelBand >= 0 && juce::Time::getMillisecondCounter() - lastWheelTime > 400)
        endWheelEdit();

    // Analyser at 30 fps.
    if (++analyserTick % 2 == 0)
    {
        const bool on = processor.analyser.mode.load() != (int) SpectrumAnalyser::Mode::Off;
        if (processor.analyser.update (juce::Time::getMillisecondCounterHiRes() * 0.001) && on)
            repaint();

        if (processor.sidechainView.load() > 0 && processor.isSidechainConnected()
            && processor.sidechainAnalyser.update (juce::Time::getMillisecondCounterHiRes() * 0.001))
            repaint();
    }

    if (changed)
    {
        curves.valid = false;

        if (processor.selectedBand.load() >= 0 && getSelectedBand() < 0)
            processor.selectedBand.store (-1);   // selected band removed (e.g. by undo)

        positionPanel();
        repaint();
    }
}

void EQDisplay::resized()
{
    curves.valid = false;
    positionPanel();
}

//==============================================================================
juce::Rectangle<float> EQDisplay::getPlotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromRight (rightGutter);
    area.removeFromBottom (bottomGutter);
    return area;
}

float EQDisplay::getRange() const { return processor.displayRangeDb.load(); }

float EQDisplay::freqToX (float freq) const
{
    const auto area = getPlotArea();
    const float norm = std::log (freq / EQ::minFreq) / std::log (EQ::maxFreq / EQ::minFreq);
    return area.getX() + norm * area.getWidth();
}

float EQDisplay::xToFreq (float x) const
{
    const auto area = getPlotArea();
    const float norm = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
    return EQ::minFreq * std::pow (EQ::maxFreq / EQ::minFreq, norm);
}

float EQDisplay::gainToY (float gainDb) const
{
    const auto area = getPlotArea();
    const float r = getRange();
    return juce::jmap (gainDb, r, -r, area.getY() + gainPad, area.getBottom() - gainPad);
}

float EQDisplay::yToGain (float y) const
{
    const auto area = getPlotArea();
    const float r = getRange();
    return juce::jlimit (-r, r, juce::jmap (y, area.getY() + gainPad, area.getBottom() - gainPad, r, -r));
}

float EQDisplay::displayToStoredGain (float displayGainDb) const
{
    const float scale = processor.getGainScale();
    if (std::abs (scale) < 0.01f)
        return 0.0f;
    return juce::jlimit (EQ::minGain, EQ::maxGain, displayGainDb / scale);
}

juce::Point<float> EQDisplay::getNodePosition (int band, bool clampToView) const
{
    const float freq = processor.getBandValue (band, EQ::Param::Freq);
    // Nodes sit on the *scaled* gain (what you hear), see the gain_scale parameter.
    float gain = EQ::typeHasGain (getType (band)) ? processor.getBandValue (band, EQ::Param::Gain) * processor.getGainScale() : 0.0f;

    if (clampToView)
        gain = juce::jlimit (-getRange(), getRange(), gain);

    return { freqToX (freq), gainToY (gain) };
}

int EQDisplay::hitTestNode (juce::Point<float> pos) const
{
    const float radius = hitRadius;
    const int selected = getSelectedBand();

    // Overlapping nodes: the selected one wins, otherwise the nearest.
    if (selected >= 0 && getNodePosition (selected).getDistanceFrom (pos) <= radius)
        return selected;

    int best = -1;
    float bestDist = radius;

    for (int b = 0; b < EQ::numBands; ++b)
    {
        if (! isUsed (b))
            continue;

        const float d = getNodePosition (b).getDistanceFrom (pos);
        if (d <= bestDist)
        {
            bestDist = d;
            best = b;
        }
    }
    return best;
}

double EQDisplay::getDisplaySampleRate() const
{
    const double sr = processor.getSampleRate();
    return sr > 0.0 ? sr : 48000.0;
}

//==============================================================================
void EQDisplay::paint (juce::Graphics& g)
{
    physicalScale = juce::jlimit (0.25f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto area = getPlotArea();

    g.fillAll (EQColours::background);
    g.setGradientFill (juce::ColourGradient (EQColours::plotTop, 0.0f, area.getY(),
                                             EQColours::plotBottom, 0.0f, area.getBottom(), false));
    g.fillRect (area);

    drawGrid (g, area);

    if (processor.showPianoStrip.load())
        drawPianoStrip (g, area);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (area.toNearestInt());

        if (processor.analyser.mode.load() != (int) SpectrumAnalyser::Mode::Off)
            drawAnalyser (g, area);

        if (processor.sidechainView.load() > 0 && processor.isSidechainConnected())
            drawSidechain (g, area);

        if (const int solo = getSoloBand(); solo >= 0)
            drawSoloRegion (g, area, solo);

        if (! curves.valid || std::abs (curvesScale - physicalScale) > 1.0e-4f)
        {
            computeCurves (area);
            curvesScale = physicalScale;
        }
        drawCurves (g, area);
    }

    drawNodes (g);

    if (processor.getNumUsedBands() == 0)
        drawEmptyHint (g, area);

    const int tipBand = draggingBand >= 0 ? draggingBand : (wheelBand >= 0 ? wheelBand : hoveredBand);
    if (isUsed (tipBand))
        drawTooltip (g, area, tipBand);

    g.setColour (EQColours::panelEdge);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

void EQDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto labelFont = juce::Font (EQColours::numbers (EQColours::Type::caption));
    g.setFont (labelFont);
    float lastLabelRight = area.getX() - 100.0f;   // labels that would overlap are dropped, never shortened

    for (float decade = 10.0f; decade <= 10000.0f; decade *= 10.0f)
    {
        for (int m = 1; m <= 9; ++m)
        {
            const float f = decade * (float) m;
            if (f < EQ::minFreq || f > EQ::maxFreq)
                continue;

            const float x = freqToX (f);
            const bool labelled = (m == 1 || m == 2 || m == 5);
            g.setColour (labelled ? EQColours::gridMajor : EQColours::gridMinor);
            verticalLine (g, x, area.getY(), area.getBottom());

            if (labelled && f > EQ::minFreq)
            {
                const juce::String text = f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + " kHz"
                                                       : juce::String (juce::roundToInt (f));
                const float w = juce::GlyphArrangement::getStringWidth (labelFont, text) + 2.0f;
                const auto box = juce::Rectangle<float> (x - w * 0.5f, area.getBottom() + 2.0f, w, bottomGutter - 4.0f)
                                     .withX (juce::jmin (x - w * 0.5f, area.getRight() - w));   // 20 kHz stays inside
                if (box.getX() >= lastLabelRight + 6.0f)
                {
                    g.setColour (EQColours::label);
                    g.drawText (text, box, juce::Justification::centred, false);
                    lastLabelRight = box.getRight();
                }
            }
        }
    }

    g.setColour (EQColours::gridMajor);
    verticalLine (g, freqToX (20000.0f) - hairline(), area.getY(), area.getBottom());

    const int range = juce::roundToInt (getRange());
    const int step = range <= 12 ? 3 : 6;

    for (int db = -range; db <= range; db += step)
    {
        const float y = gainToY ((float) db);
        g.setColour (db == 0 ? EQColours::zeroLine : EQColours::gridMajor);
        horizontalLine (g, y, area.getX(), area.getRight());

        const juce::String text = db > 0 ? "+" + juce::String (db) : juce::String (db);
        g.setColour (db == 0 ? EQColours::text : EQColours::label);
        g.drawText (text, juce::Rectangle<float> (area.getRight() + 2.0f, y - 8.0f, rightGutter - 6.0f, 16.0f),
                    juce::Justification::centredRight, false);
    }
}

// Spectrum analyser, drawn behind the EQ curves on its own dBFS scale.
// A +4.5 dB/octave tilt (pivot 1 kHz) makes pink noise / typical mixes read flat.
void EQDisplay::drawAnalyser (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float topDb = processor.analyserTopDb.load(), bottomDb = processor.analyserBottomDb.load();
    const float tilt = processor.analyserTilt.load();

    const auto& an = processor.analyser;
    const double sr = getDisplaySampleRate();
    const float yTop = area.getY() + gainPad, yBottom = area.getBottom();
    auto toY = [&] (float db) { return juce::jlimit (yTop, yBottom, juce::jmap (db, topDb, bottomDb, yTop, yBottom)); };

    // One point per physical pixel, or per two from 1.5x scaling up (the prompt allows either;
    // two keeps the paint time at 4K / Retina no slower than before). Each point still
    // averages one design unit of spectrum.
    const float sampleStep = (physicalScale >= 1.5f ? 2.0f : 1.0f) / physicalScale;

    juce::Path fill, peakLine;
    fill.startNewSubPath (area.getX(), yBottom);
    bool anything = false, anyPeak = false;

    for (float x = area.getX(); x <= area.getRight(); x += sampleStep)
    {
        const double f0 = xToFreq (x - 0.5f), f1 = xToFreq (x + 0.5f);
        const float t = SpectrumAnalyser::tiltDb (f0, f1, tilt);
        float db = an.getLevelDb (f0, f1, sr);

        if (db > SpectrumAnalyser::floorDb + 1.0f)
        {
            anything = true;
            db += t;
        }
        fill.lineTo (x, toY (db));

        if (an.isPeakHold())
        {
            const float peak = an.getPeakLevelDb (f0, f1, sr);
            anyPeak = anyPeak || peak > SpectrumAnalyser::floorDb + 1.0f;
            if (x == area.getX()) peakLine.startNewSubPath (x, toY (peak + t));
            else                  peakLine.lineTo (x, toY (peak + t));
        }
    }

    // dBFS labels on the left edge, every 15 dB inside the range.
    g.setFont (EQColours::numbers (EQColours::Type::caption));
    g.setColour (EQColours::label);   // 4.5:1 on the plot; lighter weight and size set it apart
    for (int db = (int) std::floor (topDb / 15.0f) * 15 - 15; db > bottomDb + 5.0f; db -= 15)
    {
        const float y = toY ((float) db);
        g.drawText (juce::String (db), juce::Rectangle<float> (area.getX() + 4.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    if (an.isFrozen())
    {
        g.setColour (juce::Colour (0xff5aa9ff));
        g.setFont (EQColours::font (EQColours::Type::control, EQColours::Weight::SemiBold));
        g.drawText ("Frozen", juce::Rectangle<float> (area.getX() + 44.0f, area.getY() + 6.0f, 80.0f, 16.0f),
                    juce::Justification::centredLeft, false);
    }

    if (anything)
    {
        juce::Path line (fill);
        fill.lineTo (area.getRight(), yBottom);
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0x40a8b4c8), 0.0f, yTop,
                                                 juce::Colour (0x10a8b4c8), 0.0f, yBottom, false));
        g.fillPath (fill);
        g.setColour (juce::Colour (0x60c0c8d8));
        g.strokePath (line, juce::PathStrokeType (stroke (1.0f)));
    }

    if (anyPeak)
    {
        g.setColour (juce::Colour (0x90e4e7ec));
        g.strokePath (peakLine, juce::PathStrokeType (stroke (0.8f)));
    }
}

// Sidechain spectrum (orange) on the analyser's dBFS scale and tilt. With view 2 the
// overlap ("collision") regions are shaded: frequencies where both this track's
// spectrum (pre or post, whatever the analyser shows) and the sidechain are above
// -60 dBFS and within 6 dB of each other, i.e. where they compete for the same space.
void EQDisplay::drawSidechain (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float topDb = processor.analyserTopDb.load(), bottomDb = processor.analyserBottomDb.load();
    const float tiltDbPerOct = processor.analyserTilt.load();
    constexpr float overlapFloorDb = -60.0f, overlapWithinDb = 6.0f;

    const auto& sc = processor.sidechainAnalyser;
    const auto& main = processor.analyser;
    const bool mainOn = main.mode.load() != (int) SpectrumAnalyser::Mode::Off;
    const bool showOverlap = processor.sidechainView.load() >= 2 && mainOn;

    const double sr = getDisplaySampleRate();
    const float yTop = area.getY() + gainPad, yBottom = area.getBottom();
    const juce::Colour colour (0xffff9f43);

    juce::Path line;
    bool started = false, anything = false;

    const float sampleStep = (physicalScale >= 1.5f ? 2.0f : 1.0f) / physicalScale;   // as drawAnalyser
    for (float x = area.getX(); x <= area.getRight(); x += sampleStep)
    {
        const double f0 = xToFreq (x - 0.5f), f1 = xToFreq (x + 0.5f);
        const float tilt = tiltDbPerOct * (float) std::log2 (std::sqrt (f0 * f1) / 1000.0);
        const float scDb = sc.getLevelDb (f0, f1, sr);
        anything = anything || scDb > SpectrumAnalyser::floorDb + 1.0f;

        const float y = juce::jlimit (yTop, yBottom, juce::jmap (scDb + tilt, topDb, bottomDb, yTop, yBottom));
        if (! started) { line.startNewSubPath (x, y); started = true; }
        else           line.lineTo (x, y);

        if (showOverlap)
        {
            const float mainDb = main.getLevelDb (f0, f1, sr);
            if (scDb > overlapFloorDb && mainDb > overlapFloorDb && std::abs (scDb - mainDb) <= overlapWithinDb)
            {
                const float yLow = juce::jlimit (yTop, yBottom, juce::jmap (juce::jmin (scDb, mainDb) + tilt, topDb, bottomDb, yTop, yBottom));
                g.setColour (juce::Colour (0x38ff4d4d));
                g.fillRect (x - 0.5f, yLow, 1.0f, yBottom - yLow);
            }
        }
    }

    if (! anything)
        return;

    g.setColour (colour.withAlpha (0.85f));
    g.strokePath (line, juce::PathStrokeType (stroke (1.3f)));
}

void EQDisplay::computeCurves (juce::Rectangle<float> area)
{
    // Filters are designed at the oversampled rate when oversampling is on (both the
    // zero-latency engine and the linear-phase FIR), so the curve is drawn from that too.
    const double sr = getDisplaySampleRate() * processor.getOversamplingFactor();
    auto& c = curves;

    // Same coefficient function as the audio engine -> the curve is the actual response.
    std::vector<EQ::FilterCoeffs> coeffs;
    c.bands.clear();
    c.enabled.clear();
    c.inView.clear();

    for (int b = 0; b < EQ::numBands; ++b)
    {
        if (! isUsed (b))
            continue;

        const auto type = getType (b);
        coeffs.push_back (EQ::makeFilter (type, sr,
                                          processor.getBandValue (b, EQ::Param::Freq),
                                          processor.getBandValue (b, EQ::Param::Q),
                                          EQ::typeHasGain (type) ? processor.getBandValue (b, EQ::Param::Gain) * processor.getGainScale() : 0.0f,
                                          (int) std::lround (processor.getBandValue (b, EQ::Param::Slope))));
        c.bands.push_back (b);
        c.enabled.push_back (isEnabled (b));
        c.inView.push_back (isInCurveView (b));
    }

    // Sample every pixel, plus each band's exact centre frequency so that narrow
    // notches / high-Q peaks are never missed between pixels.
    c.xs.clear();
    const float sampleStep = (physicalScale >= 1.5f ? 2.0f : 1.0f) / physicalScale;   // per physical pixel, per two from 1.5x
    for (float x = area.getX(); x <= area.getRight(); x += sampleStep)
        c.xs.push_back (x);
    for (int b : c.bands)
    {
        const float x = freqToX (processor.getBandValue (b, EQ::Param::Freq));
        if (x > area.getX() && x < area.getRight())
            c.xs.push_back (x);
    }
    std::sort (c.xs.begin(), c.xs.end());

    const size_t n = c.xs.size();
    std::vector<double> freqs (n);
    for (size_t i = 0; i < n; ++i)
        freqs[i] = xToFreq (c.xs[i]);

    c.total.assign (n, 0.0f);
    c.single.assign (c.bands.size(), std::vector<float> (n));

    for (size_t k = 0; k < c.bands.size(); ++k)
        for (size_t i = 0; i < n; ++i)
        {
            const float db = (float) EQ::getMagnitudeDb (coeffs[k], freqs[i], sr);
            c.single[k][i] = db;
            if (c.enabled[k] && c.inView[k])
                c.total[i] += db;   // sum in dB = product of magnitudes
        }

    // Dynamic bands: the response at the live gain (static curve + what the dynamics add).
    c.live.assign (c.bands.size(), {});
    for (size_t k = 0; k < c.bands.size(); ++k)
    {
        const int b = c.bands[k];
        const float dyn = getLiveDynamicDb (b);
        if (dyn == 0.0f || ! c.enabled[k])
            continue;

        const auto type = getType (b);
        const auto live = EQ::makeFilter (type, sr, processor.getBandValue (b, EQ::Param::Freq), processor.getBandValue (b, EQ::Param::Q),
                                          processor.getBandValue (b, EQ::Param::Gain) * processor.getGainScale() + dyn,
                                          (int) std::lround (processor.getBandValue (b, EQ::Param::Slope)));
        c.live[k].resize (n);
        for (size_t i = 0; i < n; ++i)
            c.live[k][i] = (float) EQ::getMagnitudeDb (live, freqs[i], sr);
    }

    c.valid = true;
}

// Live dynamic gain of a band, or 0 when its dynamics are off (or it has no gain).
float EQDisplay::getLiveDynamicDb (int band) const
{
    if (processor.getBandValue (band, EQ::Param::DynOn) < 0.5f || ! EQ::typeHasGain (getType (band)))
        return 0.0f;
    const float db = processor.dynamicGainDb[(size_t) band].load();
    return std::abs (db) < 0.05f ? 0.0f : std::round (db * 10.0f) / 10.0f;   // 0.1 dB steps: fewer redraws
}

void EQDisplay::drawCurves (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& c = curves;
    const int selected = getSelectedBand();
    const size_t n = c.xs.size();
    if (n < 2)
        return;

    const float yMin = area.getY() - 50.0f, yMax = area.getBottom() + 50.0f;
    const float zeroY = gainToY (0.0f);

    auto makePath = [&] (const std::vector<float>& db)
    {
        juce::Path p;
        for (size_t i = 0; i < n; ++i)
        {
            const float y = juce::jlimit (yMin, yMax, gainToY (db[i]));
            if (i == 0) p.startNewSubPath (c.xs[i], y); else p.lineTo (c.xs[i], y);
        }
        return p;
    };

    for (size_t k = 0; k < c.bands.size(); ++k)
    {
        const int band = c.bands[k];
        const bool enabled = c.enabled[k];
        const auto colour = EQColours::band (band);

        if (band == selected)
        {
            // Selected band: filled secondary curve.
            const auto line = makePath (c.single[k]);
            juce::Path fill (line);
            fill.lineTo (c.xs.back(), zeroY);
            fill.lineTo (c.xs.front(), zeroY);
            fill.closeSubPath();

            g.setColour (colour.withAlpha (enabled ? 0.22f : 0.08f));
            g.fillPath (fill);
            g.setColour (colour.withAlpha (enabled ? 0.85f : 0.4f));

            if (enabled)
                g.strokePath (line, juce::PathStrokeType (stroke (1.4f)));
            else
            {
                juce::Path dashed;
                const float dashes[] = { 4.0f, 4.0f };
                juce::PathStrokeType (stroke (1.2f)).createDashedStroke (dashed, line, dashes, 2);
                g.fillPath (dashed);
            }
        }
        else if (enabled)
        {
            g.setColour (colour.withAlpha ((band == hoveredBand ? 0.6f : 0.22f) * (c.inView[k] ? 1.0f : 0.35f)));
            g.strokePath (makePath (c.single[k]), juce::PathStrokeType (stroke (1.0f)));
        }
    }

    // Dynamic bands: shade between the static curve and the live one.
    for (size_t k = 0; k < c.bands.size(); ++k)
    {
        if (c.live[k].empty())
            continue;

        juce::Path between;
        for (size_t i = 0; i < n; ++i)
        {
            const float y = juce::jlimit (yMin, yMax, gainToY (c.single[k][i]));
            if (i == 0) between.startNewSubPath (c.xs[i], y); else between.lineTo (c.xs[i], y);
        }
        for (size_t i = n; i-- > 0;)
            between.lineTo (c.xs[i], juce::jlimit (yMin, yMax, gainToY (c.live[k][i])));
        between.closeSubPath();

        const auto colour = EQColours::band (c.bands[k]);
        g.setColour (colour.withAlpha (0.3f));
        g.fillPath (between);
        g.setColour (colour.withAlpha (0.7f));
        g.strokePath (makePath (c.live[k]), juce::PathStrokeType (stroke (1.0f)));
    }

    // Combined response of all enabled bands.
    const auto totalPath = makePath (c.total);
    g.setColour (EQColours::totalCurve.withAlpha (0.22f));
    g.strokePath (totalPath, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (EQColours::totalCurve);
    g.strokePath (totalPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EQDisplay::drawNodes (juce::Graphics& g)
{
    const int selected = getSelectedBand();
    const int solo = getSoloBand();

    auto drawNode = [&] (int b)
    {
        // While soloing, every other node is dimmed.
        const bool dim = solo >= 0 && b != solo;
        if (dim)
            g.beginTransparencyLayer (0.3f);

        const auto pos = getNodePosition (b);
        const auto colour = EQColours::band (b);
        const bool isSel = (b == selected);
        const bool hot = (b == hoveredBand || b == draggingBand);
        const bool enabled = isEnabled (b);
        const float r = nodeRadius + (hot ? 2.0f : 0.0f);
        const auto circle = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos);

        if (hot || isSel)
        {
            g.setColour (colour.withAlpha (0.22f));
            g.fillEllipse (circle.expanded (r));
        }

        if (enabled)
        {
            g.setColour (colour);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (EQColours::background);
            g.fillEllipse (circle);
            g.setColour (colour.withAlpha (0.7f));
            g.drawEllipse (circle.reduced (1.0f), 1.5f);
        }

        g.setColour (isSel ? juce::Colours::white : juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (circle, isSel ? 2.0f : 1.0f);

        g.setColour (enabled ? juce::Colours::black.withAlpha (0.8f) : colour);
        g.setFont (EQColours::numbers (EQColours::Type::hint, EQColours::Weight::SemiBold));
        g.drawText (juce::String (b + 1), circle.expanded (2.0f), juce::Justification::centred, false);

        // Dynamic gain indicator: a line from the node to where the live gain sits now.
        if (const float dyn = getLiveDynamicDb (b); processor.getBandValue (b, EQ::Param::DynOn) >= 0.5f && EQ::typeHasGain (getType (b)))
        {
            const float yLive = gainToY (juce::jlimit (-getRange(), getRange(),
                                                       processor.getBandValue (b, EQ::Param::Gain) * processor.getGainScale() + dyn));
            g.setColour (colour);
            g.drawLine (pos.x, pos.y, pos.x, yLive, 2.0f);
            g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ pos.x, yLive }), 1.5f);
            // No letter on the node: the line and ring show it's dynamic, and the hover readout says so.
        }

        // Stereo mode (Left, Right, Mid, Side) in a small pill under the node.
        if (const int mode = (int) std::lround (processor.getBandValue (b, EQ::Param::Stereo)); mode != 0)
        {
            const auto pillFont = juce::Font (EQColours::font (EQColours::Type::hint, EQColours::Weight::Medium));
            const auto& text = EQ::getStereoModeNames()[mode];
            const float w = juce::GlyphArrangement::getStringWidth (pillFont, text) + 10.0f, h = 16.0f;
            const auto pill = juce::Rectangle<float> (w, h).withCentre ({ pos.x, circle.getBottom() + 4.0f + h * 0.5f });
            g.setColour (EQColours::background.withAlpha (0.92f));
            g.fillRoundedRectangle (pill, h * 0.5f);
            g.setColour (colour);
            g.drawRoundedRectangle (pill.reduced (0.5f), h * 0.5f, 1.0f);
            g.setFont (pillFont);
            g.drawText (text, pill, juce::Justification::centred, false);
        }

        if (dim)
            g.endTransparencyLayer();
    };

    for (int b = 0; b < EQ::numBands; ++b)
        if (isUsed (b) && b != selected)
            drawNode (b);

    if (selected >= 0)
        drawNode (selected);   // selected node on top
}

void EQDisplay::drawTooltip (juce::Graphics& g, juce::Rectangle<float> area, int band)
{
    const auto type = getType (band);
    juce::String line1 = "Band " + juce::String (band + 1) + "  " + EQ::getTypeNames()[(int) type];
    if (EQ::typeHasSlope (type))
        line1 << "  " << EQ::getSlopeNames()[(int) std::lround (processor.getBandValue (band, EQ::Param::Slope))];
    if (EQ::typeHasGain (type) && processor.getBandValue (band, EQ::Param::DynOn) >= 0.5f)
        line1 << "  Dynamic";
    if (! isEnabled (band))
        line1 << "  (bypassed)";
    if (const int mode = (int) std::lround (processor.getBandValue (band, EQ::Param::Stereo)); mode != 0)
        line1 << "  [" << EQ::getStereoModeNames()[mode] << "]";

    const float bandFreq = processor.getBandValue (band, EQ::Param::Freq);
    juce::String line2 = formatFreq (bandFreq) + " " + juce::String::charToString (0x00b7) + " " + EQ::Notes::formatNote (bandFreq);
    if (EQ::typeHasGain (type))
    {
        const float stored = processor.getBandValue (band, EQ::Param::Gain), scale = processor.getGainScale();
        line2 << "   " << formatGain (stored);
        if (scale != 1.0f)
            line2 << " (at " << juce::String (juce::roundToInt (scale * 100.0f)) << "% scale: " << formatGain (stored * scale) << ")";
    }
    line2 << "   Q " << juce::String (processor.getBandValue (band, EQ::Param::Q), 2);

    const auto font = juce::Font (EQColours::numbers (EQColours::Type::control));
    const float w = juce::jmax (juce::GlyphArrangement::getStringWidth (font, line1),
                                juce::GlyphArrangement::getStringWidth (font, line2)) + 20.0f;
    const float h = 42.0f;

    const auto pos = getNodePosition (band);
    float y = pos.y - nodeRadius - h - 14.0f;
    if (y < area.getY() + 4.0f)
        y = pos.y + nodeRadius + 14.0f;

    auto box = juce::Rectangle<float> (w, h).withCentre ({ pos.x, y + h * 0.5f }).constrainedWithin (area.reduced (4.0f));

    g.setColour (juce::Colour (0xe8101216));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (EQColours::band (band).withAlpha (0.8f));
    g.drawRoundedRectangle (box, 5.0f, 1.0f);

    g.setFont (font);
    auto text = box.reduced (8.0f, 3.0f);
    g.setColour (EQColours::band (band));
    g.drawText (line1, text.removeFromTop (text.getHeight() * 0.5f), juce::Justification::centred, false);
    g.setColour (EQColours::text);
    g.drawText (line2, text, juce::Justification::centred, false);
}

// The audition filter's pass region, shaded with an opacity that follows its magnitude.
void EQDisplay::drawSoloRegion (juce::Graphics& g, juce::Rectangle<float> area, int band)
{
    const double sr = getDisplaySampleRate();
    EQ::SvfCoeffs c[EQ::numSoloSections];
    EQ::makeSoloFilter (getType (band), sr, processor.getBandValue (band, EQ::Param::Freq),
                        processor.getBandValue (band, EQ::Param::Q), c);

    const auto colour = EQColours::band (band);
    for (float x = area.getX(); x < area.getRight(); x += 2.0f)
    {
        double mag = 1.0;
        for (auto& s : c)
            mag *= EQ::getMagnitude (s, xToFreq (x + 1.0f), sr);

        if (mag > 0.01)
        {
            g.setColour (colour.withAlpha ((float) (0.28 * mag)));
            g.fillRect (x, area.getY(), 2.0f, area.getHeight());
        }
    }

    g.setColour (colour);
    g.setFont (EQColours::font (EQColours::Type::control, EQColours::Weight::SemiBold));
    g.drawText ("Solo: Band " + juce::String (band + 1), area.reduced (10.0f, 8.0f).removeFromTop (18.0f),
                juce::Justification::centredLeft, false);
}

// A thin keyboard along the bottom of the plot, on the log-frequency axis: each key
// spans +-half a semitone around its note. The key nearest the selected band is lit
// in that band's colour, and every C is labelled.
void EQDisplay::drawPianoStrip (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr float stripHeight = 17.0f;
    const auto strip = area.withTop (area.getBottom() - stripHeight);
    const int first = EQ::Notes::freqToNote (EQ::minFreq).midi, last = EQ::Notes::freqToNote (EQ::maxFreq).midi;

    const int selected = getSelectedBand();
    const int selectedMidi = selected >= 0 ? EQ::Notes::freqToNote (processor.getBandValue (selected, EQ::Param::Freq)).midi : -1000;

    g.setColour (juce::Colour (0xffd8dce3));
    g.fillRect (strip);

    const double halfStep = std::pow (2.0, 1.0 / 24.0);
    g.setFont (EQColours::font (EQColours::Type::caption, EQColours::Weight::Medium));

    for (int pass = 0; pass < 2; ++pass)   // white keys first, then the black keys on top
        for (int m = first; m <= last; ++m)
        {
            const int pc = ((m % 12) + 12) % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            if (black != (pass == 1))
                continue;

            const double f = EQ::Notes::midiToFreq (m);
            const float x0 = juce::jmax (strip.getX(), freqToX ((float) (f / halfStep)));
            const float x1 = juce::jmin (strip.getRight(), freqToX ((float) (f * halfStep)));
            if (x1 <= x0)
                continue;

            auto key = juce::Rectangle<float> (x0, strip.getY(), x1 - x0, black ? stripHeight * 0.6f : stripHeight);
            if (m == selectedMidi)
                g.setColour (EQColours::band (selected));
            else
                g.setColour (black ? juce::Colour (0xff23272e) : juce::Colour (0xffd8dce3));
            g.fillRect (key);

            if (! black)
            {
                g.setColour (juce::Colour (0x40000000));
                verticalLine (g, x0, strip.getY(), strip.getBottom());
            }

            if (pc == 0 && x1 - x0 >= 3.0f && m % 12 == 0)
            {
                g.setColour (juce::Colour (0xff3a3f48));
                g.drawText (EQ::Notes::noteName (m), juce::Rectangle<float> (x0 + 1.0f, strip.getY() + 2.0f, 24.0f, stripHeight - 2.0f),
                            juce::Justification::bottomLeft, false);
            }
        }

    g.setColour (EQColours::panelEdge);
    g.drawRect (strip, 1.0f);
}

void EQDisplay::showDisplayMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Display");
    menu.addItem (1, "Show piano strip", true, processor.showPianoStrip.load());

    juce::Component::SafePointer<EQDisplay> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (juce::Rectangle<int> (1, 1) + juce::Desktop::getMousePosition()),
                        [safe] (int result)
                        {
                            if (safe == nullptr || result != 1)
                                return;
                            safe->processor.showPianoStrip.store (! safe->processor.showPianoStrip.load());
                            safe->processor.notifyNonParameterStateChanged();
                            safe->repaint();
                        });
}

void EQDisplay::drawEmptyHint (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (EQColours::label);
    g.setFont (EQColours::font (EQColours::Type::heading));
    g.drawText ("Double-click to add a band", area.withTrimmedTop (area.getHeight() * 0.3f).withHeight (24.0f),
                juce::Justification::centred, false);
}

//==============================================================================
void EQDisplay::positionPanel()
{
    const int b = getSelectedBand();

    if (b < 0)
    {
        panelPlaced = false;   // closing the panel forgets where it was dragged: it opens beside its band again
        panel.setBand (-1);
        panel.setVisible (false);
        return;
    }

    panel.setBand (b);

    const int w = BandPanel::panelWidth, h = panel.getPreferredHeight();

    if (panelPlaced)   // where the user put it, even when another band is chosen
    {
        applyPanelBounds (fitPanelInside ({ juce::roundToInt (panelPos.x), juce::roundToInt (panelPos.y), w, h }));
        return;
    }

    // Don't move the panel out from under the mouse while it is being used, but do keep all of it
    // (for example the Dynamic row that has just opened) inside the graph.
    if (panel.isVisible() && panel.isMouseOverOrDragging (true))
    {
        applyPanelBounds (fitPanelInside (panel.getBounds().withSize (w, h)));
        return;
    }

    const auto area = getPlotArea();
    const auto pos = getNodePosition (b);

    const float x = juce::jlimit (area.getX() + 6.0f, juce::jmax (area.getX() + 6.0f, area.getRight() - (float) w - 6.0f), pos.x - (float) w * 0.5f);
    const bool nodeInLowerPart = pos.y > area.getBottom() - (float) h - 30.0f;
    const float y = nodeInLowerPart ? area.getY() + 6.0f : area.getBottom() - (float) h - 6.0f;

    applyPanelBounds (fitPanelInside (juce::Rectangle<float> (x, y, (float) w, (float) h).toNearestInt()));
}

juce::Rectangle<float> EQDisplay::getPanelLimits() const
{
    return getPlotArea().reduced (6.0f);
}

// The nearest place to "wanted" where the whole panel is inside the graph. If the graph is too small
// for the panel, the top-left corner stays visible so its first controls remain reachable.
juce::Rectangle<int> EQDisplay::fitPanelInside (juce::Rectangle<int> wanted) const
{
    const auto limit = getPanelLimits().toNearestInt();
    const int x = juce::jlimit (limit.getX(), juce::jmax (limit.getX(), limit.getRight() - wanted.getWidth()), wanted.getX());
    const int y = juce::jlimit (limit.getY(), juce::jmax (limit.getY(), limit.getBottom() - wanted.getHeight()), wanted.getY());
    return wanted.withPosition (x, y);
}

void EQDisplay::applyPanelBounds (juce::Rectangle<int> bounds)
{
    const juce::ScopedValueSetter<bool> guard (adjustingPanel, true);
    panel.setBounds (bounds);
    panel.setVisible (true);
    if (panelPlaced)
        panelPos = bounds.getPosition().toFloat();   // stay where the panel ended up, also after a nudge
}

void EQDisplay::movePanelTo (juce::Point<int> topLeft)
{
    if (getSelectedBand() < 0)
        return;
    panelPlaced = true;
    applyPanelBounds (fitPanelInside (panel.getBounds().withPosition (topLeft)));
}

// The panel grew or shrank by itself (the Dynamic row opening or closing): move it just enough to
// keep all of it inside the graph, whatever the mouse is doing.
void EQDisplay::childBoundsChanged (juce::Component* child)
{
    if (child == &panel && ! adjustingPanel && panel.isVisible())
        applyPanelBounds (fitPanelInside (panel.getBounds()));
}

void EQDisplay::showContextMenu (int band)
{
    juce::PopupMenu menu;
    const auto current = getType (band);
    const bool enabled = isEnabled (band);

    menu.addSectionHeader ("Band " + juce::String (band + 1));
    for (int t = 0; t < EQ::numFilterTypes; ++t)
        menu.addItem (100 + t, EQ::getTypeNames()[t], true, (int) current == t,
                      EQ::FilterShapes::makeDrawable (EQ::typeFromIndex (t), EQColours::text,
                                                      (float) EQ::FilterShapes::iconWidth, (float) EQ::FilterShapes::iconHeight));

    {
        juce::PopupMenu slopes;
        const int slope = (int) std::lround (processor.getBandValue (band, EQ::Param::Slope));
        for (int s = 0; s < EQ::numSlopes; ++s)
            slopes.addItem (200 + s, EQ::getSlopeNames()[s], true, slope == s);
        menu.addSubMenu ("Slope", slopes, EQ::typeHasSlope (current));

        juce::PopupMenu modes;
        const int stereo = (int) std::lround (processor.getBandValue (band, EQ::Param::Stereo));
        for (int m = 0; m < EQ::numStereoModes; ++m)
            modes.addItem (300 + m, EQ::getStereoModeNames()[m], true, stereo == m);
        menu.addSubMenu ("Stereo mode", modes, processor.getMainBusNumInputChannels() >= 2);
    }
    menu.addSeparator();
    menu.addItem (4, "Solo (audition)", true, processor.soloBand.load() == band);
    menu.addItem (5, "Dynamic", EQ::typeHasGain (current), processor.getBandValue (band, EQ::Param::DynOn) >= 0.5f);
    menu.addItem (1, enabled ? "Bypass" : "Enable");
    menu.addItem (2, "Reset gain and Q");
    menu.addItem (3, "Delete band");

    juce::Component::SafePointer<EQDisplay> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (juce::Rectangle<int> (1, 1) + juce::Desktop::getMousePosition()),
                        [safe, band, enabled] (int result)
                        {
                            if (safe == nullptr || result == 0 || ! safe->isUsed (band))
                                return;

                            auto& edits = safe->processor.edits;

                            if (result >= 300)       edits.setStereoMode (band, result - 300);
                            else if (result >= 200)  edits.setSlope (band, result - 200);
                            else if (result >= 100)  edits.setType (band, EQ::typeFromIndex (result - 100));
                            else if (result == 1)    edits.setEnabled (band, ! enabled);
                            else if (result == 2)    edits.resetBand (band);
                            else if (result == 3)  { edits.removeBand (band); safe->setSelectedBand (-1); }
                            else if (result == 5)
                                edits.setDynamic (band, safe->processor.getBandValue (band, EQ::Param::DynOn) < 0.5f);
                            else if (result == 4)
                            {
                                auto& solo = safe->processor.soloBand;
                                solo.store (solo.load() == band ? -1 : band);   // not a parameter: no undo step
                                safe->repaint();
                            }
                        });
}

//==============================================================================
void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int hit = hitTestNode (e.position);
    if (hit != hoveredBand)
    {
        hoveredBand = hit;
        setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EQDisplay::mouseExit (const juce::MouseEvent&)
{
    if (hoveredBand >= 0)
    {
        hoveredBand = -1;
        repaint();
    }
}

void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    endWheelEdit();

    const int hit = hitTestNode (e.position);

    if (hit < 0)
    {
        if (! e.mods.isPopupMenu())
        {
            setSelectedBand (-1);
            if (processor.analyser.isPeakHold())   // click the analyser area: reset peak hold
            {
                processor.analyser.resetPeaks();
                repaint();
            }
        }
        else
        {
            showDisplayMenu();
        }
        return;
    }

    setSelectedBand (hit);

    if (e.mods.isPopupMenu())
    {
        showContextMenu (hit);
        return;
    }

    // Alt-click / Alt-drag: audition this band until the mouse is released.
    if (e.mods.isAltDown())
    {
        momentarySolo = true;
        soloBeforeMomentary = processor.soloBand.load();
        processor.soloBand.store (hit);
    }

    draggingBand = hit;
    dragFine = e.mods.isShiftDown();
    gainTouched = false;
    anchorMouse = e.position;
    anchorNode = getNodePosition (hit, false);

    processor.edits.beginEdit (hit, "Move band");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    repaint();
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    // Shift = fine adjustment. Re-anchor when it toggles so the node doesn't jump.
    const bool fine = e.mods.isShiftDown();
    if (fine != dragFine)
    {
        dragFine = fine;
        anchorMouse = e.position;
        anchorNode = getNodePosition (draggingBand, false);
    }

    const auto delta = (e.position - anchorMouse) * (fine ? 0.1f : 1.0f);
    const auto target = anchorNode + delta;

    // Ctrl (Cmd on macOS) snaps the frequency to the nearest semitone (A4 = 440 Hz).
    float freq = xToFreq (target.x);
    if (e.mods.isCommandDown())
        freq = juce::jlimit (EQ::minFreq, EQ::maxFreq, (float) EQ::Notes::snapToSemitone (freq));
    processor.edits.setParam (EQ::Param::Freq, freq);

    // Only touch gain once the mouse has actually moved vertically, so a purely
    // horizontal drag never clips a gain that lies outside the visible range.
    if (std::abs (e.position.y - anchorMouse.y) >= 1.0f)
        gainTouched = true;

    if (gainTouched && EQ::typeHasGain (getType (draggingBand)))
    {
        // The node shows the scaled gain; store the unscaled value. At ~0 % scale every
        // gain draws at 0 dB, so a vertical drag has nothing to act on.
        if (std::abs (processor.getGainScale()) > 0.01f)
            processor.edits.setParam (EQ::Param::Gain, displayToStoredGain (yToGain (target.y)));
    }

    positionPanel();
    repaint();
}

void EQDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    processor.edits.endEdit();
    draggingBand = -1;

    if (momentarySolo)
    {
        momentarySolo = false;
        processor.soloBand.store (soloBeforeMomentary);
    }

    hoveredBand = hitTestNode (e.position);
    setMouseCursor (hoveredBand >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const int hit = hitTestNode (e.position);

    if (hit >= 0)
    {
        processor.edits.resetBand (hit);
    }
    else if (getPlotArea().contains (e.position))
    {
        addBandAt (xToFreq (e.position.x), yToGain (e.position.y), e.mods.isCommandDown());
    }
    repaint();
}

int EQDisplay::addBandAt (float freq, float displayGainDb, bool forceBell)
{
    // Near the edges a cut (task 29); a cut ignores the vertical position and sits on 0 dB.
    const auto type = EQ::typeForNewBand (freq, forceBell);
    const float gain = EQ::typeHasGain (type) ? displayToStoredGain (displayGainDb) : 0.0f;
    const int band = processor.edits.addBand (freq, gain, type);
    if (band >= 0)
        setSelectedBand (band);
    return band;
}

void EQDisplay::endWheelEdit()
{
    if (wheelBand >= 0)
    {
        if (processor.edits.isEditing() && processor.edits.getEditBand() == wheelBand)
            processor.edits.endEdit();
        wheelBand = -1;
        repaint();
    }
}

void EQDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (draggingBand >= 0)
        return;

    int band = hitTestNode (e.position);
    if (band < 0)
        band = getSelectedBand();
    if (band < 0)
        return;

    const float delta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    if (delta == 0.0f)
        return;

    // A burst of wheel events becomes one undo step (closed after 400 ms idle).
    if (wheelBand != band)
    {
        endWheelEdit();
        wheelBand = band;
        processor.edits.beginEdit (band, "Change Q");
    }
    lastWheelTime = juce::Time::getMillisecondCounter();

    const float speed = e.mods.isShiftDown() ? 0.2f : 1.5f;
    const float q = processor.getBandValue (band, EQ::Param::Q) * std::exp (delta * speed);
    processor.edits.setParam (EQ::Param::Q, juce::jlimit (EQ::minQ, EQ::maxQ, q));
    repaint();
}

bool EQDisplay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        const int b = getSelectedBand();
        if (b >= 0)
        {
            processor.edits.removeBand (b);
            setSelectedBand (-1);
        }
        return true;
    }
    return false;   // let the editor handle undo/redo etc.
}

//==============================================================================
// Grid lines on whole physical pixels (1 px up to 1.75x scaling, then 2, ...), so they stay
// sharp at 125 % / 150 % / 175 % instead of smearing across two pixels.
void EQDisplay::verticalLine (juce::Graphics& g, float x, float top, float bottom) const
{
    g.fillRect (juce::Rectangle<float> (snapToPixel (x), top, hairline(), bottom - top));
}

void EQDisplay::horizontalLine (juce::Graphics& g, float y, float left, float right) const
{
    g.fillRect (juce::Rectangle<float> (left, snapToPixel (y), right - left, hairline()));
}
