namespace ms
{

const std::array<Region, numRegions>& getRegions() noexcept
{
    static const std::array<Region, numRegions> regions {{
        { "Bass",         20.0,    200.0 },
        { "Tiefe Mitten", 200.0,   1000.0 },
        { "Hohe Mitten",  1000.0,  5000.0 },
        { "H\xc3\xb6hen", 5000.0,  BandLayout::getMaxFrequency() + 1.0 },
    }};
    return regions;
}

int getRegionForBand (int band) noexcept
{
    const auto centre = BandLayout::getCentre (band);
    const auto& regions = getRegions();

    for (int r = 0; r < numRegions; ++r)
        if (centre >= regions[(size_t) r].lowFrequency && centre < regions[(size_t) r].highFrequency)
            return r;

    return -1;
}

double kWeightingPowerGain (double frequency) noexcept
{
    const double w = juce::MathConstants<double>::twoPi * juce::jlimit (0.0, 23999.0, frequency) / 48000.0;
    const auto z1 = std::polar (1.0, -w);
    const auto z2 = z1 * z1;

    auto biquadPower = [&] (double b0, double b1, double b2, double a1, double a2)
    {
        const auto num = b0 + b1 * z1 + b2 * z2;
        const auto den = 1.0 + a1 * z1 + a2 * z2;
        return std::norm (num) / std::norm (den);
    };

    // Stufe 1: High-Shelf ("Kopfeffekt"), Stufe 2: RLB-Hochpass
    return biquadPower (1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585)
         * biquadPower (1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621);
}

} // namespace ms
