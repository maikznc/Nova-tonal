namespace ms
{

namespace
{
    using WeightedValues = std::vector<std::pair<float, double>>;

    /** Gewichtetes Quantil mit linearer Interpolation zwischen den Gewichts-Mittelpunkten. */
    float weightedQuantile (const WeightedValues& sorted, double totalWeight, double q)
    {
        const double target = q * totalWeight;
        double cumulative = 0.0, previousMid = 0.0;
        float previousValue = sorted.front().first;

        for (size_t i = 0; i < sorted.size(); ++i)
        {
            const auto [value, weight] = sorted[i];
            const double mid = cumulative + 0.5 * weight;

            if (mid >= target)
            {
                if (i == 0)
                    return value;

                const double t = (target - previousMid) / juce::jmax (1.0e-12, mid - previousMid);
                return (float) (previousValue + t * (value - previousValue));
            }

            previousMid = mid;
            previousValue = value;
            cumulative += weight;
        }

        return sorted.back().first;
    }

    Percentiles computePercentiles (WeightedValues& values)
    {
        Percentiles p;

        if (values.empty())
            return p;

        std::sort (values.begin(), values.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        double total = 0.0;
        for (const auto& v : values)
            total += v.second;

        p.p10 = weightedQuantile (values, total, 0.10);
        p.p25 = weightedQuantile (values, total, 0.25);
        p.p50 = weightedQuantile (values, total, 0.50);
        p.p75 = weightedQuantile (values, total, 0.75);
        p.p90 = weightedQuantile (values, total, 0.90);
        return p;
    }
}

void TargetBuilder::beginTrack (const juce::String& trackName)
{
    jassert (! current.has_value());
    current = Track { trackName, {} };
}

void TargetBuilder::addObservation (const SpectrumSnapshot& snapshot)
{
    jassert (current.has_value());

    if (current.has_value() && snapshot.valid)
        current->observations.push_back (snapshot);
}

bool TargetBuilder::endTrack()
{
    if (! current.has_value())
        return false;

    auto track = std::move (*current);
    current.reset();

    if (track.observations.empty())
        return false;

    // Relatives Gate: mittlere Lautheit (Leistungsmittel) − 10 dB
    double powerSum = 0.0;
    for (const auto& o : track.observations)
        powerSum += std::pow (10.0, o.loudness / 10.0);

    const double meanLoudness = 10.0 * std::log10 (powerSum / (double) track.observations.size());
    const double gate = meanLoudness - relativeGateDb;

    std::erase_if (track.observations, [gate] (const SpectrumSnapshot& o) { return o.loudness < gate; });

    if (track.observations.empty())
        return false;

    tracks.push_back (std::move (track));
    return true;
}

juce::StringArray TargetBuilder::getTrackNames() const
{
    juce::StringArray names;

    for (const auto& t : tracks)
        names.add (t.name);

    return names;
}

TargetCurve TargetBuilder::build (const juce::String& curveName) const
{
    TargetCurve curve;
    curve.name = curveName;
    curve.sources = getTrackNames();

    WeightedValues values;

    auto collect = [&] (auto&& getValue)
    {
        values.clear();

        for (const auto& t : tracks)
        {
            const double weight = 1.0 / (double) t.observations.size();

            for (const auto& o : t.observations)
            {
                const float v = getValue (o);

                if (std::isfinite (v))
                    values.emplace_back (v, weight);
            }
        }

        return computePercentiles (values);
    };

    for (size_t b = 0; b < curve.bands.size(); ++b)
        curve.bands[b] = collect ([b] (const SpectrumSnapshot& o) { return o.bands[b]; });

    for (size_t r = 0; r < curve.regions.size(); ++r)
        curve.regions[r] = collect ([r] (const SpectrumSnapshot& o) { return o.regions[r]; });

    curve.loudness = collect ([] (const SpectrumSnapshot& o) { return o.loudness; });
    return curve;
}

} // namespace ms
