#include "FindFiles.h"
#include "generated/Generated.h"

enum class SortKey { name, mtime, size };

static constexpr juce::int64 millisecondsPerSecond { 1000 };
static constexpr juce::int64 secondsPerSecond { 1 };
static constexpr juce::int64 secondsPerMinute { 60 };
static constexpr juce::int64 secondsPerHour { 3600 };
static constexpr juce::int64 secondsPerDay { 86400 };
static constexpr juce::int64 secondsPerWeek { 604800 };
static constexpr int monthsPerYear { 12 };
static constexpr int noMonths { 0 };
static constexpr juce::int64 decimalUnitBase { 1000 };
static constexpr juce::int64 binaryUnitBase { 1024 };
static constexpr juce::int64 byteMultiplier { 1 };
static constexpr int decimalRadix { 10 };
static constexpr int octalRadix { 8 };
static constexpr int permissionDigits { 4 };
static constexpr int maxIntegerDigits { 24 };
static constexpr size_t dateSize { 10 };
static constexpr size_t dateTimeSize { 19 };
static constexpr size_t yearWidth { 4 };
static constexpr size_t pairWidth { 2 };
static constexpr size_t monthOffset { 5 };
static constexpr size_t dayOffset { 8 };
static constexpr size_t timeOffset { 11 };
static constexpr size_t minuteOffset { 14 };
static constexpr size_t secondOffset { 17 };
static constexpr int firstMonth { 1 };
static constexpr int lastMonth { 12 };
static constexpr int firstDay { 1 };
static constexpr int lastDay { 31 };
static constexpr int firstHour { 0 };
static constexpr int lastHour { 23 };
static constexpr int firstMinuteOrSecond { 0 };
static constexpr int lastMinuteOrSecond { 59 };

template <typename KeyType, size_t ValueCount>
static std::optional<KeyType> getKeyOfValue (std::string_view value, const std::array<std::pair<std::string_view, KeyType>, ValueCount>& table)
{
    for (const auto& [optionText, key] : table)
    {
        if (Arguments::matchesOption (value, optionText))
            return key;
    }

    return std::nullopt;
}

static juce::Result getFirstFailure (std::initializer_list<juce::Result> results)
{
    for (const auto& result : results)
    {
        if (result.failed())
            return result;
    }

    return juce::Result::ok();
}

static std::pair<juce::Result, std::optional<int>> getMaxDepth (const Arguments& arguments)
{
    if (not arguments.contains (Id::maxDepthOption))
        return { juce::Result::ok(), std::nullopt };

    const auto [depthResult, maxDepth] { arguments.getInteger (Id::maxDepthOption, 0, 0) };
    return { depthResult, maxDepth };
}

static std::pair<juce::Result, jam::Array<Paths::EntryType>> getEntryTypes (const Arguments& arguments)
{
    static constexpr std::array<std::pair<std::string_view, Paths::EntryType>, 3> entryTypeValues {
        std::pair<std::string_view, Paths::EntryType> { Id::fileValue, Paths::EntryType::file },
        std::pair<std::string_view, Paths::EntryType> { Id::dirValue, Paths::EntryType::directory },
        std::pair<std::string_view, Paths::EntryType> { Id::symlinkValue, Paths::EntryType::symlink }
    };
    jam::Array<Paths::EntryType> entryTypes;

    for (const auto& value : arguments.getValues (Id::typeOption))
    {
        const auto entryType { getKeyOfValue (value, entryTypeValues) };

        if (entryType.has_value())
            entryTypes.add (*entryType);
        else
            return { Stdio::fail (text::Diagnostics::failChoiceInvalid, Arguments::getOptionName (Id::typeOption)), jam::Array<Paths::EntryType> {} };
    }

    if (entryTypes.isEmpty())
        entryTypes.add (Paths::EntryType::file);

    return { juce::Result::ok(), std::move (entryTypes) };
}

static std::string_view getLoweredAscii (std::string_view text, juce::Span<char> buffer)
{
    const size_t length { std::min (text.size(), buffer.size()) };

    for (size_t index { 0 }; index < length; ++index)
        buffer[index] = static_cast<char> (juce::CharacterFunctions::toLowerCase (text.at (index)));

    return std::string_view { buffer.data(), length };
}

static juce::int64 getSizeMultiplier (std::string_view unitText)
{
    static constexpr size_t maxUnitLength { 8 };
    static constexpr std::array<std::pair<std::string_view, juce::int64>, 9> sizeUnits {
        std::pair<std::string_view, juce::int64> { Id::byteUnit, byteMultiplier },
        std::pair<std::string_view, juce::int64> { Id::kiloUnit, decimalUnitBase },
        std::pair<std::string_view, juce::int64> { Id::megaUnit, decimalUnitBase * decimalUnitBase },
        std::pair<std::string_view, juce::int64> { Id::gigaUnit, decimalUnitBase * decimalUnitBase * decimalUnitBase },
        std::pair<std::string_view, juce::int64> { Id::teraUnit, decimalUnitBase * decimalUnitBase * decimalUnitBase * decimalUnitBase },
        std::pair<std::string_view, juce::int64> { Id::kibiUnit, binaryUnitBase },
        std::pair<std::string_view, juce::int64> { Id::mebiUnit, binaryUnitBase * binaryUnitBase },
        std::pair<std::string_view, juce::int64> { Id::gibiUnit, binaryUnitBase * binaryUnitBase * binaryUnitBase },
        std::pair<std::string_view, juce::int64> { Id::tebiUnit, binaryUnitBase * binaryUnitBase * binaryUnitBase * binaryUnitBase }
    };

    if (unitText.size() > maxUnitLength)
        return 0;

    std::array<char, maxUnitLength> lowered {};
    return getKeyOfValue (getLoweredAscii (unitText, lowered), sizeUnits).value_or (0);
}

static std::optional<juce::int64> getSizeBytes (std::string_view digitsText)
{
    const char* const digitsEnd { digitsText.data() + digitsText.size() };
    std::uint64_t number { 0 };
    const auto [numberEnd, numberError] { std::from_chars (digitsText.data(), digitsEnd, number) };
    const auto multiplier { getSizeMultiplier (std::string_view { numberEnd, static_cast<size_t> (digitsEnd - numberEnd) }) };

    if (numberError != std::errc {} or multiplier == 0)
        return std::nullopt;

    return static_cast<juce::int64> (number) * multiplier;
}

static std::optional<juce::Range<juce::int64>> getSizeRange (std::string_view text)
{
    const bool hasPlus { not text.empty() and text.front() == static_cast<char> (Chars::plus) };
    const bool hasDash { not text.empty() and text.front() == static_cast<char> (Chars::dash) };
    const auto bytes { getSizeBytes ((hasPlus or hasDash) ? text.substr (1) : text) };

    if (not bytes.has_value())
        return std::nullopt;

    if (hasPlus)
        return juce::Range<juce::int64> { *bytes, std::numeric_limits<juce::int64>::max() };

    if (hasDash)
        return juce::Range<juce::int64> { 0, *bytes + 1 };

    return juce::Range<juce::int64> { *bytes, *bytes + 1 };
}

static std::pair<juce::Result, jam::Array<juce::Range<juce::int64>>> getSizeRanges (const Arguments& arguments)
{
    jam::Array<juce::Range<juce::int64>> sizeRanges;

    for (const auto& value : arguments.getValues (Id::sizeOption))
    {
        const auto sizeRange { getSizeRange (value) };

        if (sizeRange.has_value())
            sizeRanges.add (*sizeRange);
        else
            return { Stdio::fail (text::Diagnostics::failSizeInvalid, value), jam::Array<juce::Range<juce::int64>> {} };
    }

    return { juce::Result::ok(), std::move (sizeRanges) };
}

static std::optional<juce::int64> getEpochMilliseconds (std::string_view text)
{
    std::uint64_t seconds { 0 };
    const char* const textEnd { text.data() + text.size() };
    const auto [secondsEnd, secondsError] { std::from_chars (text.data(), textEnd, seconds) };

    if (secondsError == std::errc {} and secondsEnd == textEnd)
        return static_cast<juce::int64> (seconds) * millisecondsPerSecond;

    return std::nullopt;
}

static bool isWithin (int value, int firstValue, int lastValue)
{
    return value >= firstValue and value <= lastValue;
}

static std::optional<int> getDateField (std::string_view text, size_t offset, size_t width)
{
    const char* const fieldFirst { text.data() + offset };
    int value { 0 };
    const auto [fieldEnd, fieldError] { std::from_chars (fieldFirst, fieldFirst + width, value) };

    if (fieldError == std::errc {} and fieldEnd == fieldFirst + width)
        return value;

    return std::nullopt;
}

static std::optional<std::tuple<int, int, int>> getDateParts (std::string_view text)
{
    const auto year { getDateField (text, 0, yearWidth) };
    const auto month { getDateField (text, monthOffset, pairWidth) };
    const auto day { getDateField (text, dayOffset, pairWidth) };
    const bool isSeparated { text.at (yearWidth) == static_cast<char> (Chars::dash) and text.at (dayOffset - 1) == static_cast<char> (Chars::dash) };

    if (not (year.has_value() and month.has_value() and day.has_value() and isSeparated))
        return std::nullopt;

    if (not (isWithin (*month, firstMonth, lastMonth) and isWithin (*day, firstDay, lastDay)))
        return std::nullopt;

    return std::tuple<int, int, int> { *year, *month, *day };
}

static std::optional<std::tuple<int, int, int>> getTimeParts (std::string_view text)
{
    const auto hours { getDateField (text, timeOffset, pairWidth) };
    const auto minutes { getDateField (text, minuteOffset, pairWidth) };
    const auto seconds { getDateField (text, secondOffset, pairWidth) };
    const bool isSeparated { text.at (dateSize) == static_cast<char> (Chars::space) and text.at (minuteOffset - 1) == static_cast<char> (Chars::colon) and text.at (secondOffset - 1) == static_cast<char> (Chars::colon) };

    if (not (hours.has_value() and minutes.has_value() and seconds.has_value() and isSeparated))
        return std::nullopt;

    if (not (isWithin (*hours, firstHour, lastHour) and isWithin (*minutes, firstMinuteOrSecond, lastMinuteOrSecond) and isWithin (*seconds, firstMinuteOrSecond, lastMinuteOrSecond)))
        return std::nullopt;

    return std::tuple<int, int, int> { *hours, *minutes, *seconds };
}

static std::optional<juce::int64> getLocalMilliseconds (std::string_view text)
{
    const bool isDateOnly { text.size() == dateSize };
    const bool isDateTime { text.size() == dateTimeSize };

    if (not (isDateOnly or isDateTime))
        return std::nullopt;

    const auto date { getDateParts (text) };

    if (not date.has_value())
        return std::nullopt;

    const auto& [year, month, day] { *date };

    if (isDateOnly)
        return juce::Time (year, month - 1, day, firstHour, firstMinuteOrSecond, firstMinuteOrSecond, 0, true).toMilliseconds();

    const auto time { getTimeParts (text) };

    if (not time.has_value())
        return std::nullopt;

    const auto& [hours, minutes, seconds] { *time };
    return juce::Time (year, month - 1, day, hours, minutes, seconds, 0, true).toMilliseconds();
}

static juce::Time getMonthsEarlier (const juce::Time& time, int months)
{
    const int monthIndex { time.getYear() * monthsPerYear + time.getMonth() - months };
    return juce::Time { monthIndex / monthsPerYear, monthIndex % monthsPerYear, time.getDayOfMonth(), time.getHours(), time.getMinutes(), time.getSeconds() };
}

static std::optional<std::tuple<juce::int64, int, const char*>> getDurationTerm (const char* cursor, const char* textEnd)
{
    static constexpr std::array<std::tuple<std::string_view, juce::int64, int>, 7> durationUnits {
        std::tuple<std::string_view, juce::int64, int> { Id::secondUnit, secondsPerSecond, noMonths },
        std::tuple<std::string_view, juce::int64, int> { Id::minuteUnit, secondsPerMinute, noMonths },
        std::tuple<std::string_view, juce::int64, int> { Id::hourUnit, secondsPerHour, noMonths },
        std::tuple<std::string_view, juce::int64, int> { Id::dayUnit, secondsPerDay, noMonths },
        std::tuple<std::string_view, juce::int64, int> { Id::weekUnit, secondsPerWeek, noMonths },
        std::tuple<std::string_view, juce::int64, int> { Id::monthUnit, 0, 1 },
        std::tuple<std::string_view, juce::int64, int> { Id::yearUnit, 0, monthsPerYear }
    };
    std::uint64_t amount { 0 };
    const auto [amountEnd, amountError] { std::from_chars (cursor, textEnd, amount) };

    if (amountError != std::errc {})
        return std::nullopt;

    const char* const unitEnd { std::find_if (amountEnd, textEnd, [] (char character)
    {
        return juce::CharacterFunctions::isDigit (character);
    }) };
    static constexpr size_t maxDurationUnitLength { 16 };
    const std::string_view unitSlice { amountEnd, static_cast<size_t> (unitEnd - amountEnd) };

    if (unitSlice.size() > maxDurationUnitLength)
        return std::nullopt;

    std::array<char, maxDurationUnitLength> lowered {};
    const auto loweredUnit { getLoweredAscii (unitSlice, lowered) };

    for (const auto& [unitText, unitSeconds, unitMonths] : durationUnits)
    {
        if (Arguments::matchesOption (loweredUnit, unitText))
            return std::tuple<juce::int64, int, const char*> { static_cast<juce::int64> (amount) * unitSeconds, static_cast<int> (amount) * unitMonths, unitEnd };
    }

    return std::nullopt;
}

static std::optional<juce::int64> getDurationCutoff (std::string_view text)
{
    if (text.empty())
        return std::nullopt;

    juce::int64 seconds { 0 };
    int months { 0 };
    const char* cursor { text.data() };
    const char* const textEnd { text.data() + text.size() };

    while (cursor != textEnd)
    {
        const auto term { getDurationTerm (cursor, textEnd) };

        if (not term.has_value())
            return std::nullopt;

        const auto& [termSeconds, termMonths, termEnd] { *term };
        seconds += termSeconds;
        months += termMonths;
        cursor = termEnd;
    }

    const juce::Time earlier { juce::Time (juce::Time::getCurrentTime().toMilliseconds() - seconds * millisecondsPerSecond) };
    return getMonthsEarlier (earlier, months).toMilliseconds();
}

static std::optional<juce::int64> getCutoffMilliseconds (std::string_view text)
{
    const std::string_view epochPrefix { Id::epochPrefix };

    if (text.substr (0, epochPrefix.size()).compare (epochPrefix) == 0)
        return getEpochMilliseconds (text.substr (epochPrefix.size()));

    const auto durationCutoff { getDurationCutoff (text) };

    if (durationCutoff.has_value())
        return durationCutoff;

    return getLocalMilliseconds (text);
}

static std::pair<juce::Result, std::optional<juce::int64>> getCutoffOption (const Arguments& arguments, std::string_view option)
{
    if (not arguments.contains (option))
        return { juce::Result::ok(), std::nullopt };

    const auto cutoff { getCutoffMilliseconds (arguments.getValueView (option)) };

    if (cutoff.has_value())
        return { juce::Result::ok(), cutoff };

    return { Stdio::fail (text::Diagnostics::failTimeInvalid, arguments.getValueView (option)), std::nullopt };
}

static std::pair<juce::Result, SortKey> getSortKey (const Arguments& arguments)
{
    static constexpr std::array<std::pair<std::string_view, SortKey>, 3> sortValues {
        std::pair<std::string_view, SortKey> { Id::nameValue, SortKey::name },
        std::pair<std::string_view, SortKey> { Id::mtimeValue, SortKey::mtime },
        std::pair<std::string_view, SortKey> { Id::sizeValue, SortKey::size }
    };
    const std::string_view value { arguments.getValueView (Id::sortOption) };

    if (value.empty())
        return { juce::Result::ok(), SortKey::name };

    const auto sortKey { getKeyOfValue (value, sortValues) };

    if (sortKey.has_value())
        return { juce::Result::ok(), *sortKey };

    return { Stdio::fail (text::Diagnostics::failChoiceInvalid, Arguments::getOptionName (Id::sortOption)), SortKey::name };
}

std::pair<juce::Result, std::unique_ptr<EntryFilter>> EntryFilter::create (const Arguments& arguments)
{
    auto [typeResult, entryTypes] { getEntryTypes (arguments) };
    auto [sizeResult, sizeRanges] { getSizeRanges (arguments) };
    const auto [withinResult, withinMilliseconds] { getCutoffOption (arguments, Id::changedWithinOption) };
    const auto [beforeResult, beforeMilliseconds] { getCutoffOption (arguments, Id::changedBeforeOption) };
    const juce::Result filterResult { getFirstFailure ({ typeResult, sizeResult, withinResult, beforeResult }) };

    if (filterResult.failed())
        return { filterResult, nullptr };

    return { filterResult, std::make_unique<EntryFilter> (std::move (entryTypes), std::move (sizeRanges), withinMilliseconds, beforeMilliseconds) };
}

EntryFilter::EntryFilter (jam::Array<Paths::EntryType> newEntryTypes,
                          jam::Array<juce::Range<juce::int64>> newSizeRanges,
                          std::optional<juce::int64> newChangedWithinMilliseconds,
                          std::optional<juce::int64> newChangedBeforeMilliseconds)
    : entryTypes (std::move (newEntryTypes)),
      sizeRanges (std::move (newSizeRanges)),
      changedWithinMilliseconds (newChangedWithinMilliseconds),
      changedBeforeMilliseconds (newChangedBeforeMilliseconds)
{
}

bool EntryFilter::isMatching (const Paths::ListedEntry& listedEntry) const
{
    const auto& [start, length, entryType, metadata] { listedEntry };
    const auto& [size, modifiedMilliseconds, permissions] { metadata };
    juce::ignoreUnused (start, length, permissions);
    const bool isTypeMatch { std::find (entryTypes.begin(), entryTypes.end(), entryType) != entryTypes.end() };
    const juce::int64 fileSize { size };
    const bool isSizeMatch { sizeRanges.isEmpty() or (entryType == Paths::EntryType::file and std::all_of (sizeRanges.begin(), sizeRanges.end(), [fileSize] (const juce::Range<juce::int64>& sizeRange)
    {
        return sizeRange.contains (fileSize);
    })) };
    const bool isWithinMatch { not changedWithinMilliseconds.has_value() or modifiedMilliseconds > *changedWithinMilliseconds };
    const bool isBeforeMatch { not changedBeforeMilliseconds.has_value() or modifiedMilliseconds < *changedBeforeMilliseconds };
    return isTypeMatch and isSizeMatch and isWithinMatch and isBeforeMatch;
}

bool EntryFilter::isIncludingNonFiles() const noexcept
{
    return std::any_of (entryTypes.begin(), entryTypes.end(), [] (Paths::EntryType entryType)
    {
        return entryType != Paths::EntryType::file;
    });
}

bool EntryFilter::isFiltering() const noexcept
{
    return isIncludingNonFiles()
        or not sizeRanges.isEmpty()
        or changedWithinMilliseconds.has_value()
        or changedBeforeMilliseconds.has_value();
}

static Paths::ListedEntry getVerifiedListedEntry (const Paths::Entry& entry)
{
    const auto [listResult, listedEntry] { Paths::getListedEntry (entry) };
    jassert (listResult.wasOk());
    return listedEntry;
}

static juce::int64 getSortValue (const Paths::Entry& entry, SortKey sortKey)
{
    const auto listedEntry { getVerifiedListedEntry (entry) };
    const auto& [start, length, entryType, metadata] { listedEntry };
    const auto& [size, modifiedMilliseconds, permissions] { metadata };
    juce::ignoreUnused (start, length, entryType, permissions);

    if (sortKey == SortKey::mtime)
        return modifiedMilliseconds;

    if (sortKey == SortKey::size)
        return size;

    return 0;
}

static bool isOrderedBefore (const Paths::Entry& firstEntry, const Paths::Entry& secondEntry, SortKey sortKey)
{
    const juce::int64 firstValue { getSortValue (firstEntry, sortKey) };
    const juce::int64 secondValue { getSortValue (secondEntry, sortKey) };

    if (firstValue != secondValue)
        return firstValue < secondValue;

    return Paths::getDisplayPath (firstEntry).compare (Paths::getDisplayPath (secondEntry)) < 0;
}

static std::pair<juce::Result, jam::Array<Paths::Entry>> getMatchingEntries (const jam::Array<Paths::Entry>& entries, const EntryFilter& filter)
{
    jam::Array<Paths::Entry> matchingEntries;

    for (const auto& entry : entries)
    {
        const auto [listResult, listedEntry] { Paths::getListedEntry (entry) };

        if (listResult.failed())
            return { listResult, jam::Array<Paths::Entry> {} };

        if (filter.isMatching (listedEntry))
            matchingEntries.add (entry);
    }

    return { juce::Result::ok(), std::move (matchingEntries) };
}

static void sortEntries (jam::Array<Paths::Entry>& entries, SortKey sortKey, bool isReversed)
{
    if (sortKey != SortKey::name)
    {
        std::sort (entries.begin(), entries.end(), [sortKey] (const Paths::Entry& firstEntry, const Paths::Entry& secondEntry)
        {
            return isOrderedBefore (firstEntry, secondEntry, sortKey);
        });
    }

    if (isReversed)
        std::reverse (entries.begin(), entries.end());
}

static void appendInteger (std::string& text, juce::int64 value, int radix)
{
    std::array<char, maxIntegerDigits> digits {};
    [[maybe_unused]] const auto [end, error] { std::to_chars (digits.data(), std::next (digits.data(), static_cast<std::ptrdiff_t> (digits.size())), value, radix) };

    jam::Format::appendBytes (text, std::string_view { digits.data(), static_cast<size_t> (end - digits.data()) });
}

static std::string_view getEntryTypeName (Paths::EntryType entryType)
{
    switch (entryType)
    {
        case Paths::EntryType::file: return Arguments::getOptionName (Id::fileValue);
        case Paths::EntryType::directory: return Arguments::getOptionName (Id::dirValue);
        case Paths::EntryType::symlink: return Arguments::getOptionName (Id::symlinkValue);
    }

    return {};
}

static void appendModeString (std::string& members, int permissions)
{
    std::array<char, maxIntegerDigits> digits {};
    [[maybe_unused]] const auto [end, error] { std::to_chars (digits.data(), std::next (digits.data(), static_cast<std::ptrdiff_t> (digits.size())), permissions, octalRadix) };
    const int digitCount { static_cast<int> (end - digits.data()) };

    jam::JsonWriter::appendQuote (members, 1);
    jam::Format::appendZeros (members, permissionDigits - digitCount);
    jam::Format::appendBytes (members, std::string_view { digits.data(), static_cast<size_t> (digitCount) });
    jam::JsonWriter::appendQuote (members, 1);
}

static std::string getLinkTarget (const Paths::Entry& entry, Paths::EntryType entryType)
{
    if (entryType == Paths::EntryType::symlink)
    {
        std::error_code targetError;
        const auto target { std::filesystem::read_symlink (std::filesystem::u8path (jam::Strings::getView (Paths::getFile (entry).getFullPathName())), targetError) };
        return target.u8string();
    }

    return {};
}

static void appendEntryMembers (std::string& members, const Paths::Entry& entry, const Paths::ListedEntry& listedEntry)
{
    const auto& [start, length, entryType, metadata] { listedEntry };
    const auto& [size, modifiedMilliseconds, permissions] { metadata };
    juce::ignoreUnused (start, length);
    jam::JsonWriter::appendKey (members, Id::path);
    jam::JsonWriter::appendString (members, Paths::getDisplayPath (entry), juce::JSON::Encoding::utf8);
    jam::JsonWriter::appendKey (members, Id::type);
    jam::JsonWriter::appendString (members, getEntryTypeName (entryType), juce::JSON::Encoding::utf8);
    jam::JsonWriter::appendKey (members, Id::sizeValue);
    appendInteger (members, size, decimalRadix);
    jam::JsonWriter::appendKey (members, Id::mtimeValue);
    jam::JsonWriter::appendString (members, jam::Strings::getView (juce::Time (modifiedMilliseconds).toISO8601 (true)), juce::JSON::Encoding::utf8);
    jam::JsonWriter::appendKey (members, Id::mode);
    appendModeString (members, permissions);
    jam::JsonWriter::appendKey (members, Id::target);
    jam::JsonWriter::appendString (members, getLinkTarget (entry, entryType), juce::JSON::Encoding::utf8);
}

static void appendEntries (std::string& output, const jam::Array<Paths::Entry>& entries, bool isJson)
{
    for (const auto& entry : entries)
    {
        if (isJson)
        {
            const auto listedEntry { getVerifiedListedEntry (entry) };
            jam::JsonWriter::appendObject (output, [&entry, &listedEntry] (std::string& members)
            {
                appendEntryMembers (members, entry, listedEntry);
            });
        }
        else
        {
            jam::Format::appendBytes (output, Paths::getDisplayPath (entry));
        }

        jam::Format::appendUtf8 (output, Chars::newline);
    }
}

Stdio::Output FindFiles::run (const Arguments& arguments)
{
    const auto [depthResult, maxDepth] { getMaxDepth (arguments) };
    const auto [filterResult, filter] { EntryFilter::create (arguments) };
    const auto [sortResult, sortKey] { getSortKey (arguments) };
    const juce::Result optionsResult { getFirstFailure ({ depthResult, filterResult, sortResult }) };

    if (optionsResult.failed())
        return Stdio::getFailure (optionsResult);

    const auto positionals (arguments.getPositionals (0));
    const auto rawPaths (positionals.empty() ? juce::Span<const std::string_view> { Paths::defaultPaths } : positionals);
    const bool isListingEntries { filter->isIncludingNonFiles() };
    const bool isReadingMetadata { arguments.contains (Id::sizeOption)
                                or arguments.contains (Id::changedWithinOption)
                                or arguments.contains (Id::changedBeforeOption)
                                or sortKey != SortKey::name
                                or arguments.contains (Id::jsonOption) };
    auto [pathFailures, files, skippedCount] { Paths::getFiles (rawPaths,
                                                  arguments.getValues (Id::includeOption),
                                                  arguments.getValues (Id::excludeOption),
                                                  arguments.contains (Id::hiddenOption),
                                                  arguments.contains (Id::noIgnoreOption),
                                                  maxDepth,
                                                  isListingEntries,
                                                  isReadingMetadata) };
    auto& [failurePaths, failureResults] { pathFailures };
    juce::ignoreUnused (failurePaths);
    const bool hasFailures { not failureResults.isEmpty() };
    auto [matchResult, selected] { filter->isFiltering() ? getMatchingEntries (files, *filter) : std::pair<juce::Result, jam::Array<Paths::Entry>> { juce::Result::ok(), std::move (files) } };

    if (matchResult.failed())
        return Stdio::getFailure (matchResult);
    sortEntries (selected, sortKey, arguments.contains (Id::reverseOption));
    std::string output;

    if (not arguments.contains (Id::quietOption))
        appendEntries (output, selected, arguments.contains (Id::jsonOption));

    const int exitCode { hasFailures ? Stdio::exitError : selected.isEmpty() ? Stdio::exitMismatch : Stdio::exitSuccess };
    auto sortedFailures (Paths::getFailureResults (pathFailures));

    if (exitCode == Stdio::exitMismatch and skippedCount > 0)
        sortedFailures.add (Stdio::fail (skippedCount, text::Diagnostics::entriesNotSearched));

    return Stdio::getOutput (exitCode, Stdio::getDocuments (std::move (output)), std::move (sortedFailures));
}

juce::Result FindFiles::getListOnlyFailure (const Arguments& arguments)
{
    static constexpr std::array listOnlyOptions { Id::maxDepthOption, Id::typeOption, Id::sizeOption, Id::changedWithinOption, Id::changedBeforeOption, Id::sortOption, Id::reverseOption, Id::jsonOption };

    for (const auto option : listOnlyOptions)
    {
        if (arguments.contains (option))
            return Stdio::fail (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option));
    }

    return juce::Result::ok();
}
