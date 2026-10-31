#include "BalanceCurveView.h"

namespace
{
    using Pt = juce::Point<float>;

    /** Catmull-Rom → Bézier: glatter Kurvenabschnitt von pts[i] nach pts[i + 1]. */
    void appendSmoothSegment (juce::Path& path, const std::vector<Pt>& pts, size_t i)
    {
        const auto& p0 = pts[i == 0 ? 0 : i - 1];
        const auto& p1 = pts[i];
        const auto& p2 = pts[i + 1];
        const auto& p3 = pts[std::min (i + 2, pts.size() - 1)];
        path.cubicTo (p1 + (p2 - p0) / 6.0f, p2 - (p3 - p1) / 6.0f, p2);
    }

    void appendSmoothCurve (juce::Path& path, const std::vector<Pt>& pts, bool startNewSubPath)
    {
        if (pts.empty())
            return;

        if (startNewSubPath) path.startNewSubPath (pts.front());
        else                 path.lineTo (pts.front());

        for (size_t i = 0; i + 1 < pts.size(); ++i)
            appendSmoothSegment (path, pts, i);
    }

    /** Zusammenhängende Läufe gültiger Bänder (Lücken z. B. oberhalb von Nyquist). */
    template <typename IsValid>
    std::vector<std::vector<int>> findRuns (IsValid&& isValid)
    {
        std::vector<std::vector<int>> runs;
        std::vector<int> current;

        for (int b = 0; b < ms::BandLayout::numBands; ++b)
        {
            if (isValid (b))
            {
                current.push_back (b);
            }
            else if (! current.empty())
            {
                runs.push_back (std::move (current));
                current = {};
            }
        }

        if (! current.empty())
            runs.push_back (std::move (current));

        return runs;
    }

    juce::String formatFrequency (double f)
    {
        if (f < 1000.0)
            return juce::String (juce::roundToInt (f)) + " Hz";

        return juce::String (f / 1000.0, f < 10000.0 ? 2 : 1).replaceCharacter ('.', ',') + " kHz";
    }

    juce::String formatDb (float db)
    {
        const auto magnitude = juce::String (std::abs (db), 1).replaceCharacter ('.', ',');

        if (db <= -0.05f) return ms::de ("−") + magnitude;
        if (db >=  0.05f) return "+" + magnitude;
        return "0,0";
    }
}

//==============================================================================
BalanceCurveView::BalanceCurveView()
{
    live.fill (ms::SpectrumSnapshot::invalid);
    setOpaque (false);
}

void BalanceCurveView::setLive (const BandValues& values, bool hasSignal, bool isStale)
{
    live = values;
    hasLive = hasSignal;
    stale = isStale;
    repaint();
}

void BalanceCurveView::setTarget (std::shared_ptr<const ms::TargetCurve> newTarget)
{
    target = std::move (newTarget);
    updateRange();
    repaint();
}

void BalanceCurveView::setNarrowRange (bool shouldBeNarrow)
{
    narrow = shouldBeNarrow;
    repaint();
}

void BalanceCurveView::setTilt (float dbPerOctave)
{
    tilt = dbPerOctave;
    updateRange();
    repaint();
}

//==============================================================================
juce::Rectangle<float> BalanceCurveView::getPlotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (40.0f).withTrimmedBottom (24.0f).withTrimmedTop (4.0f).withTrimmedRight (8.0f);
}

float BalanceCurveView::frequencyToX (double f, juce::Rectangle<float> area) const
{
    const auto norm = std::log (f / displayMinHz) / std::log (displayMaxHz / displayMinHz);
    return area.getX() + (float) norm * area.getWidth();
}

double BalanceCurveView::xToFrequency (float x, juce::Rectangle<float> area) const
{
    const auto norm = (double) ((x - area.getX()) / area.getWidth());
    return displayMinHz * std::pow (displayMaxHz / displayMinHz, norm);
}

float BalanceCurveView::dbToY (float db, juce::Rectangle<float> area) const
{
    return juce::jmap (db, yMin, yMax, area.getBottom(), area.getY());
}

float BalanceCurveView::tiltAt (double f) const
{
    return tilt * (float) std::log2 (f / 1000.0);
}

void BalanceCurveView::updateRange()
{
    float lo = -24.0f, hi = 6.0f;

    if (target != nullptr)
    {
        lo = std::numeric_limits<float>::max();
        hi = std::numeric_limits<float>::lowest();

        for (int b = 0; b < ms::BandLayout::numBands; ++b)
        {
            const auto& p = target->bands[(size_t) b];

            if (! p.isValid())
                continue;

            const auto t = tiltAt (ms::BandLayout::getCentre (b));
            lo = juce::jmin (lo, p.p10 + t);
            hi = juce::jmax (hi, p.p90 + t);
        }

        if (lo > hi) { lo = -24.0f; hi = 6.0f; }
    }

    yMin = std::floor ((lo - 6.0f) / 6.0f) * 6.0f;
    yMax = std::ceil  ((hi + 6.0f) / 6.0f) * 6.0f;

    while (yMax - yMin < 30.0f)
    {
        yMin -= 3.0f;
        yMax += 3.0f;
    }
}

//==============================================================================
void BalanceCurveView::paint (juce::Graphics& g)
{
    const auto area = getPlotArea();

    drawGrid (g, area);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (area.toNearestInt());
        drawTarget (g, area);
        drawLive (g, area);
        drawHover (g, area);
    }

    drawEmptyHints (g, area);
}

void BalanceCurveView::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (ms::Colours::panel);
    g.fillRoundedRectangle (area, 6.0f);

    // Nebenlinien (Dekaden-Teilung)
    g.setColour (ms::Colours::gridMinor);

    for (double decade : { 10.0, 100.0, 1000.0, 10000.0 })
        for (int m = 2; m <= 9; ++m)
        {
            const double f = decade * m;

            if (f > displayMinHz && f < displayMaxHz)
                g.drawVerticalLine (juce::roundToInt (frequencyToX (f, area)), area.getY(), area.getBottom());
        }

    // dB-Linien
    g.setFont (juce::FontOptions (11.0f));

    for (float v = std::ceil (yMin / 6.0f) * 6.0f; v <= yMax + 0.01f; v += 6.0f)
    {
        const auto y = dbToY (v, area);
        g.setColour (std::abs (v) < 0.01f ? ms::Colours::outline : ms::Colours::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());

        g.setColour (ms::Colours::textDim);
        g.drawText (v > 0.0f ? "+" + juce::String ((int) v) : (v < 0.0f ? ms::de ("−") + juce::String ((int) -v) : juce::String ("0")),
                    juce::Rectangle<float> (0.0f, y - 7.0f, area.getX() - 6.0f, 14.0f), juce::Justification::centredRight);
    }

    // Hauptlinien + Beschriftung
    const std::pair<double, const char*> labels[] = { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
                                                      { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" } };

    for (const auto& [f, text] : labels)
    {
        const auto x = frequencyToX (f, area);

        if (f > displayMinHz && f < displayMaxHz)
        {
            g.setColour (ms::Colours::gridMajor);
            g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        }

        g.setColour (ms::Colours::textDim);
        const auto labelX = juce::jlimit (area.getX() + 12.0f, area.getRight() - 12.0f, x);
        g.drawText (text, juce::Rectangle<float> (labelX - 20.0f, area.getBottom() + 5.0f, 40.0f, 14.0f), juce::Justification::centred);
    }

    // Bereichsgrenzen + Namen
    const auto& regions = ms::getRegions();
    g.setFont (juce::FontOptions (11.0f).withKerningFactor (0.06f));

    for (int r = 0; r < ms::numRegions; ++r)
    {
        const auto& region = regions[(size_t) r];
        const auto x0 = frequencyToX (juce::jmax (displayMinHz, region.lowFrequency), area);
        const auto x1 = frequencyToX (juce::jmin (displayMaxHz, region.highFrequency), area);

        if (r > 0)
        {
            g.setColour (ms::Colours::outline.withAlpha (0.8f));
            g.drawVerticalLine (juce::roundToInt (x0), area.getY(), area.getBottom());
        }

        g.setColour (ms::Colours::textFaint);
        g.drawText (ms::de (region.name), juce::Rectangle<float> (x0, area.getY() + 6.0f, x1 - x0, 14.0f), juce::Justification::centred);
    }
}

void BalanceCurveView::drawTarget (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (target == nullptr)
        return;

    const auto runs = findRuns ([this] (int b) { return target->bands[(size_t) b].isValid(); });

    auto pointFor = [&] (int b, float value)
    {
        const auto f = ms::BandLayout::getCentre (b);
        return Pt (frequencyToX (f, area), dbToY (value + tiltAt (f), area));
    };

    auto drawRange = [&] (float ms::Percentiles::* lowMember, float ms::Percentiles::* highMember, float fillAlpha, bool strokeEdges)
    {
        for (const auto& run : runs)
        {
            std::vector<Pt> upper, lower;

            for (int b : run)
            {
                const auto& p = target->bands[(size_t) b];
                upper.push_back (pointFor (b, p.*highMember));
                lower.push_back (pointFor (b, p.*lowMember));
            }

            std::reverse (lower.begin(), lower.end());

            juce::Path shape;
            appendSmoothCurve (shape, upper, true);
            appendSmoothCurve (shape, lower, false);
            shape.closeSubPath();

            g.setColour (ms::Colours::target.withAlpha (fillAlpha));
            g.fillPath (shape);

            if (strokeEdges)
            {
                juce::Path edgeTop, edgeBottom;
                appendSmoothCurve (edgeTop, upper, true);
                appendSmoothCurve (edgeBottom, lower, true);
                g.setColour (ms::Colours::target.withAlpha (0.45f));
                g.strokePath (edgeTop, juce::PathStrokeType (1.0f));
                g.strokePath (edgeBottom, juce::PathStrokeType (1.0f));
            }
        }
    };

    if (! narrow)
        drawRange (&ms::Percentiles::p10, &ms::Percentiles::p90, 0.11f, true);

    drawRange (&ms::Percentiles::p25, &ms::Percentiles::p75, narrow ? 0.20f : 0.10f, narrow);

    // Median gestrichelt
    for (const auto& run : runs)
    {
        std::vector<Pt> median;

        for (int b : run)
            median.push_back (pointFor (b, target->bands[(size_t) b].p50));

        juce::Path path, dashed;
        appendSmoothCurve (path, median, true);

        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.3f).createDashedStroke (dashed, path, dashes, 2);
        g.setColour (ms::Colours::target.withAlpha (0.9f));
        g.fillPath (dashed);
    }
}

void BalanceCurveView::drawLive (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! hasLive)
        return;

    const float alpha = stale ? 0.35f : 1.0f;
    const auto runs = findRuns ([this] (int b) { return std::isfinite (live[(size_t) b]); });

    for (const auto& run : runs)
    {
        std::vector<Pt> pts;

        for (int b : run)
        {
            const auto f = ms::BandLayout::getCentre (b);
            pts.emplace_back (frequencyToX (f, area), dbToY (live[(size_t) b] + tiltAt (f), area));
        }

        juce::Path curve;
        appendSmoothCurve (curve, pts, true);

        // weiche Fläche unter der Kurve
        juce::Path fill (curve);
        fill.lineTo (pts.back().x, area.getBottom());
        fill.lineTo (pts.front().x, area.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (ms::Colours::live.withAlpha (0.10f * alpha), 0.0f, area.getY(),
                                                 ms::Colours::live.withAlpha (0.0f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);

        g.setColour (ms::Colours::live.withAlpha (0.92f * alpha));
        g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Abschnitte außerhalb des Zielbereichs
        if (target == nullptr || pts.size() < 2)
            continue;

        juce::Path outside;

        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const int b0 = run[i], b1 = run[i + 1];
            const auto dev0 = target->bands[(size_t) b0].getDeviation (live[(size_t) b0], narrow);
            const auto dev1 = target->bands[(size_t) b1].getDeviation (live[(size_t) b1], narrow);

            if (dev0 != 0.0f || dev1 != 0.0f)
            {
                outside.startNewSubPath (pts[i]);
                appendSmoothSegment (outside, pts, i);
            }
        }

        g.setColour (ms::Colours::warn.withAlpha (alpha));
        g.strokePath (outside, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void BalanceCurveView::drawHover (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! hoverX.has_value() || ! area.contains (*hoverX, area.getCentreY()))
        return;

    const auto f = xToFrequency (*hoverX, area);
    const int band = juce::jlimit (0, ms::BandLayout::numBands - 1,
                                   (int) std::floor (std::log2 (f / ms::BandLayout::minFrequency) * ms::BandLayout::bandsPerOctave));
    const auto centre = ms::BandLayout::getCentre (band);
    const auto x = frequencyToX (centre, area);

    g.setColour (ms::Colours::text.withAlpha (0.18f));
    g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());

    juce::StringArray lines;
    lines.add (formatFrequency (centre));

    const float liveValue = live[(size_t) band];
    const bool liveOk = hasLive && std::isfinite (liveValue);

    if (liveOk)
    {
        g.setColour (ms::Colours::live);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, dbToY (liveValue + tiltAt (centre), area) }));
    }

    if (target != nullptr && target->bands[(size_t) band].isValid())
    {
        const auto& p = target->bands[(size_t) band];
        g.setColour (ms::Colours::target);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, dbToY (p.p50 + tiltAt (centre), area) }));

        if (liveOk)
        {
            const auto dev = p.getDeviation (liveValue, narrow);
            lines.add (ms::de ("zum Ziel-Median: ") + formatDb (liveValue - p.p50) + " dB");
            lines.add (dev == 0.0f ? ms::de ("im Zielbereich")
                                   : (dev > 0.0f ? ms::de ("zu viel (") : ms::de ("zu wenig (")) + formatDb (dev) + " dB)");
        }
    }

    const float boxW = 190.0f, lineH = 16.0f;
    const float boxH = 10.0f + lineH * (float) lines.size();
    auto box = juce::Rectangle<float> (x + 12.0f, area.getY() + 26.0f, boxW, boxH);

    if (box.getRight() > area.getRight() - 4.0f)
        box.setX (x - 12.0f - boxW);

    g.setColour (ms::Colours::panelRaised.withAlpha (0.95f));
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (ms::Colours::outline);
    g.drawRoundedRectangle (box, 5.0f, 1.0f);

    g.setFont (juce::FontOptions (12.0f));
    auto textArea = box.reduced (9.0f, 5.0f);

    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour (i == 0 ? ms::Colours::text : ms::Colours::textDim);
        g.drawText (lines[i], textArea.removeFromTop (lineH), juce::Justification::centredLeft);
    }
}

void BalanceCurveView::drawEmptyHints (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (target == nullptr)
    {
        auto hint = area.withHeight (area.getHeight() * 0.42f).withTrimmedTop (area.getHeight() * 0.14f);
        g.setColour (ms::Colours::text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (16.0f));
        g.drawText (ms::de ("Kein Ziel geladen"), hint.removeFromTop (24.0f), juce::Justification::centred);
        g.setColour (ms::Colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (ms::de ("Referenztracks ins Fenster ziehen oder \xe2\x80\x9eReferenzen laden \xe2\x80\xa6\xe2\x80\x9c klicken"),
                    hint.removeFromTop (20.0f), juce::Justification::centred);
    }

    if (! hasLive)
    {
        g.setColour (ms::Colours::textDim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (ms::de ("Warte auf Audiosignal \xe2\x80\xa6"),
                    area.withTrimmedTop (area.getHeight() - 44.0f).withHeight (20.0f), juce::Justification::centred);
    }
}

//==============================================================================
void BalanceCurveView::mouseMove (const juce::MouseEvent& e)
{
    hoverX = (float) e.x;
    repaint();
}

void BalanceCurveView::mouseExit (const juce::MouseEvent&)
{
    hoverX.reset();
    repaint();
}
