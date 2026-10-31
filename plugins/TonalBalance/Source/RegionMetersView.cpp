#include "RegionMetersView.h"

namespace
{
    juce::String formatSignedDb (float db)
    {
        const auto magnitude = juce::String (std::abs (db), 1).replaceCharacter ('.', ',');

        if (db <= -0.05f) return ms::de ("−") + magnitude;
        if (db >=  0.05f) return "+" + magnitude;
        return "0,0";
    }

    juce::String formatRange (double lo, double hi)
    {
        auto f = [] (double v) { return v < 1000.0 ? juce::String (juce::roundToInt (v)) : juce::String (juce::roundToInt (v / 1000.0)) + "k"; };
        return f (lo) + ms::de ("–") + f (juce::jmin (hi, 20000.0)) + " Hz";
    }
}

RegionMetersView::RegionMetersView()
{
    live.fill (ms::SpectrumSnapshot::invalid);
}

void RegionMetersView::setLive (const RegionValues& values, bool hasSignal, bool isStale)
{
    live = values;
    hasLive = hasSignal;
    stale = isStale;
    repaint();
}

void RegionMetersView::setTarget (std::shared_ptr<const ms::TargetCurve> newTarget)
{
    target = std::move (newTarget);
    repaint();
}

void RegionMetersView::setNarrowRange (bool shouldBeNarrow)
{
    narrow = shouldBeNarrow;
    repaint();
}

void RegionMetersView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (ms::Colours::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    bounds = bounds.reduced (10.0f, 8.0f);
    const float columnWidth = bounds.getWidth() / (float) ms::numRegions;

    for (int r = 0; r < ms::numRegions; ++r)
        drawMeter (g, bounds.removeFromLeft (columnWidth).reduced (5.0f, 0.0f), r);
}

void RegionMetersView::drawMeter (juce::Graphics& g, juce::Rectangle<float> area, int r)
{
    const auto& region = ms::getRegions()[(size_t) r];

    // Kopf: Name + Frequenzbereich
    g.setColour (ms::Colours::text);
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (ms::de (region.name), area.removeFromTop (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    g.setColour (ms::Colours::textFaint);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText (formatRange (region.lowFrequency, region.highFrequency), area.removeFromTop (14.0f), juce::Justification::centred);

    // Fuß: Abweichung + Urteil
    auto footer = area.removeFromBottom (36.0f);
    area.removeFromBottom (6.0f);
    area.removeFromTop (8.0f);

    auto track = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 34.0f), area.getHeight());
    g.setColour (ms::Colours::panelRaised);
    g.fillRoundedRectangle (track, 4.0f);

    auto valueToY = [&] (float db) { return juce::jmap (juce::jlimit (-scaleDb, scaleDb, db), -scaleDb, scaleDb, track.getBottom() - 2.0f, track.getY() + 2.0f); };

    // Skalenstriche alle 3 dB
    for (float v = -scaleDb + 3.0f; v < scaleDb; v += 3.0f)
    {
        g.setColour (std::abs (v) < 0.01f ? ms::Colours::textFaint : ms::Colours::gridMajor);
        const auto y = valueToY (v);
        g.drawHorizontalLine (juce::roundToInt (y), track.getX() - (std::abs (v) < 0.01f ? 6.0f : 3.0f), track.getX());
    }

    const auto& p = target != nullptr ? target->regions[(size_t) r] : ms::Percentiles {};
    const bool hasTarget = p.isValid();
    const bool hasValue = hasLive && std::isfinite (live[(size_t) r]);

    if (hasTarget)
    {
        const auto yHi = valueToY (p.getUpper (narrow) - p.p50);
        const auto yLo = valueToY (p.getLower (narrow) - p.p50);
        const auto rangeRect = juce::Rectangle<float> (track.getX(), yHi, track.getWidth(), yLo - yHi);
        g.setColour (ms::Colours::target.withAlpha (0.22f));
        g.fillRect (rangeRect);
        g.setColour (ms::Colours::target.withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (yHi), track.getX(), track.getRight());
        g.drawHorizontalLine (juce::roundToInt (yLo), track.getX(), track.getRight());

        const float dashes[] = { 3.0f, 3.0f };
        g.setColour (ms::Colours::target.withAlpha (0.9f));
        g.drawDashedLine ({ track.getX(), valueToY (0.0f), track.getRight(), valueToY (0.0f) }, dashes, 2, 1.0f);
    }

    footer.removeFromTop (2.0f);

    if (hasTarget && hasValue)
    {
        const float deviation = live[(size_t) r] - p.p50;
        const float outside = p.getDeviation (live[(size_t) r], narrow);
        const auto colour = (outside == 0.0f ? ms::Colours::ok : ms::Colours::warn).withMultipliedAlpha (stale ? 0.4f : 1.0f);

        const auto y0 = valueToY (0.0f), y = valueToY (deviation);
        g.setColour (colour.withAlpha (0.28f * (stale ? 0.4f : 1.0f)));
        g.fillRect (juce::Rectangle<float> (track.getCentreX() - 3.0f, juce::jmin (y0, y), 6.0f, std::abs (y - y0)));

        g.setColour (colour);
        g.fillRoundedRectangle (juce::Rectangle<float> (track.getX() - 3.0f, y - 2.5f, track.getWidth() + 6.0f, 5.0f), 2.0f);

        // Pfeil, wenn außerhalb der Skala
        if (std::abs (deviation) > scaleDb)
        {
            const float dir = deviation > 0.0f ? -1.0f : 1.0f;
            juce::Path arrow;
            arrow.addTriangle (track.getCentreX() - 5.0f, y + dir * 6.0f, track.getCentreX() + 5.0f, y + dir * 6.0f, track.getCentreX(), y + dir * 12.0f);
            g.fillPath (arrow);
        }

        g.setFont (juce::FontOptions (15.0f).withStyle ("Bold"));
        g.drawFittedText (formatSignedDb (deviation), footer.removeFromTop (18.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (colour.withMultipliedAlpha (0.85f));
        g.drawText (outside == 0.0f ? ms::de ("im Ziel") : (outside > 0.0f ? ms::de ("zu viel") : ms::de ("zu wenig")),
                    footer.removeFromTop (14.0f), juce::Justification::centred);
    }
    else
    {
        g.setColour (ms::Colours::textFaint);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (ms::de ("–"), footer.removeFromTop (18.0f), juce::Justification::centred);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (hasTarget ? ms::de ("kein Signal") : ms::de ("kein Ziel"), footer.removeFromTop (14.0f), juce::Justification::centred);
    }
}
