namespace ms
{

namespace
{
    const std::array<double, BandLayout::numBands>& getBandKWeights() noexcept
    {
        static const auto weights = []
        {
            std::array<double, BandLayout::numBands> w {};

            for (int b = 0; b < BandLayout::numBands; ++b)
                w[(size_t) b] = kWeightingPowerGain (BandLayout::getCentre (b));

            return w;
        }();

        return weights;
    }

    constexpr double tinyPower = 1.0e-20;
}

double SpectrumSnapshot::computeLoudness (const BandPowers& powers, const BandFlags& validBands) noexcept
{
    const auto& k = getBandKWeights();
    double weighted = 0.0;

    for (size_t b = 0; b < powers.size(); ++b)
        if (validBands[b])
            weighted += powers[b] * k[b];

    return -0.691 + 10.0 * std::log10 (weighted + tinyPower);
}

SpectrumSnapshot SpectrumSnapshot::fromBandPowers (const BandPowers& powers, const BandFlags& validBands)
{
    SpectrumSnapshot s;
    const double loudness = computeLoudness (powers, validBands);
    s.loudness = (float) loudness;

    if (loudness < silenceThresholdLufs)
        return s;

    // Bezug: Lautheit (ohne den BS.1770-Offset) + 10·log10(Bandanzahl),
    // damit rosa Rauschen bei ≈ 0 dB liegt.
    const double reference = (loudness + 0.691) - 10.0 * std::log10 ((double) BandLayout::numBands);

    std::array<double, numRegions> regionSum {};
    std::array<int, numRegions> regionCount {};

    for (int b = 0; b < BandLayout::numBands; ++b)
    {
        if (! validBands[(size_t) b])
            continue;

        const double p = powers[(size_t) b];
        s.bands[(size_t) b] = (float) (10.0 * std::log10 (p + tinyPower) - reference);

        const int r = getRegionForBand (b);

        if (r >= 0)
        {
            regionSum[(size_t) r] += p;
            ++regionCount[(size_t) r];
        }
    }

    // Bereichswert = mittlere Bandleistung im Bereich, gleiche Skala wie die Bänder
    for (int r = 0; r < numRegions; ++r)
        if (regionCount[(size_t) r] > 0)
            s.regions[(size_t) r] = (float) (10.0 * std::log10 (regionSum[(size_t) r] / regionCount[(size_t) r] + tinyPower) - reference);

    s.valid = true;
    return s;
}

} // namespace ms
