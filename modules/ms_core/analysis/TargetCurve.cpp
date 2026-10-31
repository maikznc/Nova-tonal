namespace ms
{

namespace
{
    constexpr const char* formatId = "masteringsuite-target-curve";
    constexpr int formatVersion = 1;

    juce::var toJsonNumber (float v)
    {
        if (! std::isfinite (v))
            return {};   // null

        return std::round ((double) v * 100.0) / 100.0;
    }

    float fromJsonNumber (const juce::var& v)
    {
        if (v.isDouble() || v.isInt() || v.isInt64())
            return (float) (double) v;

        return SpectrumSnapshot::invalid;
    }

    template <size_t N>
    juce::var percentileArraysToVar (const std::array<Percentiles, N>& values)
    {
        auto* obj = new juce::DynamicObject();

        auto addColumn = [&] (const char* key, float Percentiles::* member)
        {
            juce::Array<juce::var> column;

            for (const auto& p : values)
                column.add (toJsonNumber (p.*member));

            obj->setProperty (key, column);
        };

        addColumn ("p10", &Percentiles::p10);
        addColumn ("p25", &Percentiles::p25);
        addColumn ("p50", &Percentiles::p50);
        addColumn ("p75", &Percentiles::p75);
        addColumn ("p90", &Percentiles::p90);
        return juce::var (obj);
    }

    template <size_t N>
    bool percentileArraysFromVar (const juce::var& v, std::array<Percentiles, N>& values)
    {
        auto readColumn = [&] (const char* key, float Percentiles::* member)
        {
            const auto* column = v[key].getArray();

            if (column == nullptr || column->size() != (int) N)
                return false;

            for (size_t i = 0; i < N; ++i)
                values[i].*member = fromJsonNumber (column->getReference ((int) i));

            return true;
        };

        return readColumn ("p10", &Percentiles::p10) && readColumn ("p25", &Percentiles::p25)
            && readColumn ("p50", &Percentiles::p50) && readColumn ("p75", &Percentiles::p75)
            && readColumn ("p90", &Percentiles::p90);
    }

    juce::var percentilesToVar (const Percentiles& p)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("p10", toJsonNumber (p.p10));
        obj->setProperty ("p25", toJsonNumber (p.p25));
        obj->setProperty ("p50", toJsonNumber (p.p50));
        obj->setProperty ("p75", toJsonNumber (p.p75));
        obj->setProperty ("p90", toJsonNumber (p.p90));
        return juce::var (obj);
    }

    Percentiles percentilesFromVar (const juce::var& v)
    {
        Percentiles p;
        p.p10 = fromJsonNumber (v["p10"]);
        p.p25 = fromJsonNumber (v["p25"]);
        p.p50 = fromJsonNumber (v["p50"]);
        p.p75 = fromJsonNumber (v["p75"]);
        p.p90 = fromJsonNumber (v["p90"]);
        return p;
    }
}

bool TargetCurve::isValid() const noexcept
{
    int validBands = 0;

    for (const auto& b : bands)
        if (b.isValid())
            ++validBands;

    return validBands >= BandLayout::numBands / 2;
}

juce::var TargetCurve::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("format", formatId);
    obj->setProperty ("version", formatVersion);
    obj->setProperty ("name", name);

    juce::Array<juce::var> sourceList;
    for (const auto& s : sources)
        sourceList.add (s);
    obj->setProperty ("sources", sourceList);

    auto* layout = new juce::DynamicObject();
    layout->setProperty ("minFrequency", BandLayout::minFrequency);
    layout->setProperty ("bandsPerOctave", BandLayout::bandsPerOctave);
    layout->setProperty ("numBands", BandLayout::numBands);
    obj->setProperty ("bandLayout", juce::var (layout));

    obj->setProperty ("bands", percentileArraysToVar (bands));
    obj->setProperty ("regions", percentileArraysToVar (regions));
    obj->setProperty ("loudness", percentilesToVar (loudness));
    return juce::var (obj);
}

std::optional<TargetCurve> TargetCurve::fromVar (const juce::var& v)
{
    if (v["format"].toString() != formatId || (int) v["version"] > formatVersion)
        return std::nullopt;

    const auto& layout = v["bandLayout"];

    if ((int) layout["numBands"] != BandLayout::numBands
        || (int) layout["bandsPerOctave"] != BandLayout::bandsPerOctave
        || std::abs ((double) layout["minFrequency"] - BandLayout::minFrequency) > 1.0e-6)
        return std::nullopt;

    TargetCurve t;
    t.name = v["name"].toString();

    if (const auto* s = v["sources"].getArray())
        for (const auto& item : *s)
            t.sources.add (item.toString());

    if (! percentileArraysFromVar (v["bands"], t.bands) || ! percentileArraysFromVar (v["regions"], t.regions))
        return std::nullopt;

    t.loudness = percentilesFromVar (v["loudness"]);

    if (! t.isValid())
        return std::nullopt;

    return t;
}

juce::String TargetCurve::toJson() const
{
    return juce::JSON::toString (toVar(), juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::singleLine));
}

std::optional<TargetCurve> TargetCurve::fromJson (const juce::String& json)
{
    juce::var parsed;

    if (juce::JSON::parse (json, parsed).failed())
        return std::nullopt;

    return fromVar (parsed);
}

bool TargetCurve::saveToFile (const juce::File& file) const
{
    if (! file.getParentDirectory().createDirectory())
        return false;

    return file.replaceWithText (juce::JSON::toString (toVar()));
}

std::optional<TargetCurve> TargetCurve::loadFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    return fromJson (file.loadFileAsString());
}

juce::File TargetCurve::getUserTargetsFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif

    return base.getChildFile ("NOVA").getChildFile ("Targets");
}

} // namespace ms
