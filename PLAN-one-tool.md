# PLAN: one tool

**RFC:** none — objective from ARCHITECT's prompts and rulings (Rulings below).
**Date:** 2026-11-26 (third update: Stage 7C written in full, Ruling 25)
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM

## Context

The first plan hand-rolled what JAM already does: a page renderer, themes, a wrapper, a pager, a YAML reader. ARCHITECT stopped it. This plan deletes that code and builds the seam between blocks that exist.

Steps 0 to 43 are done. Their code is on disk and is not compiled. Step 45 (the one build) is their proof. Step 44 of the second update is void: Ruling 25 deleted `JsonProgram`, which it changed.

Stage 7C is the rest of the JSON work. ARCHITECT ruled (Ruling 25) that every JSON type other than `JsonDocument` and its established pattern is garbage, and the deletion is done (Step 44b). What remains is the cause that made those types: in the JSON family a scalar is a token inside its container, not an element ("The child element exists only for a container token", `jam_JsonDocument.h:244-252`; `getChildByToken` returns nullptr for it, `jam_JsonDocument.cpp:132-137`). A value has no node, so each consumer built a carrier to point at one. MANIFESTO E, the verdict: "If the data does not fit the implementation, the parse is wrong or the implementation is wrong." Stage 7D gives the correction: `JsonDocument` follows the Document pattern (Ruling 27).

## Overview

- `jam::Document` holds the one line model. Every document calls it.
- A page is a `juce::Component` painted by a LookAndFeel through `juce::Graphics`. `TerminalGraphicsContext` turns it into bytes.
- ling builds and carries the rendering library. saw is `cat`. With ling's libraries it renders as `bat`.
- `JsonDocument` is the complete AST of JSON: every value is an element. A jq program is a `JsonDocument` family with a linked tree (`Id::to`, `Id::in`). A run creates its result values as elements of a value-stream `JsonDocument`. `JsonValidator` holds every rule. `JsonWriter` renders elements. No other JSON type exists.

## Rulings (verbatim; they override every other source except CONTRACT)

| # | ARCHITECT | Effect |
|---|---|---|
| 1 | *"we dont need, dont want yaml document AT ALL. we have markdown and css for customizing syntax. NO NEW SEMANTICS ALLOWED"* | A theme is a style sheet. No tmTheme file and no `.sublime-syntax` file is read (SPEC.md:842) |
| 2 | *"i said feature parity. not byte-to-byte translation."* | The plain lanes are byte-equal with `cat`. The rendered page compares features |
| 3 | *"TerminalPage"* | The page renderer |
| 4 | *"LANGUAGE LIBRARY: owned by ling and ling only"*, *"saw `bat` is only cat"* | saw holds no language and no renderer |
| 5 | *"ling build shareable gui dynamic library seam"*, *"should be language agnostic, pure `rendering`, `style` etc"* | Stage 4 |
| 6 | *"saw must stay LEAN and SLIM. substitution failure is not an error"* | No library: plain bytes, exit 0, no message |
| 7 | *"mcp lane NEVER touch TUI (gui) dll"* | The MCP path has no loader call |
| 8 | *"line building must be at Document base. any document need one only call base."*, *"no more scattered addLine, at base Document"* | Stage 2 |
| 9 | *"just move shareable id to core."* | Step 5 |
| 10 | *"StyleObject (LookAndFeel) is the sole consumer for css/markdown"* | A document sets ids. Only the LookAndFeel reads a colour or a metric |
| 11 | *"TUI contract is ANALOGOUS to juce_gui. TUI DX must be juce native gui api, with juce::Component, juce::LookAndFeel, juce::Graphics etc, the only difference is LowLevelGraphicsContext."* | Gutter, header, grid, rule, highlight and pager are Components and LookAndFeel calls. No page code writes SGR or box glyphs |
| 12 | *"this touches probably all jam project, be careful"* | Blast-radius check in each JAM step |
| 13 | *"no audit until we get runtime validation for all related project which consume the modules, audit only wholesale when there are no garbage, no redundancy, no handroll method, and using jam SSOT OOTB"* | Stage 9 |
| 14 | *"no build until all steps completed, build only to profile to ensure we meet the feature parity without performance lost"* | Stage 8 |
| 15 | *"NO FUCKING HANDROLL ANYTHING"*, *"building blocks exists, we only need to build the seam"* | No step adds a method that the inventory holds |
| 16 | *"~/Documents/Poems/dev/tit/ probably have plan to create something like GitDocument, if not probably can"* | Stage 5 |
| 17 | *"no significant changes on callee, probably no change at all"* (EVE) | EVE source does not change |
| 18 | *"only use juce::String when we need to call juce native API (args), otherwise string manipulations always use jam::Format, jam::Strings, jam::Utf, jam::simd"* | String rule below. Every step |
| 19 | *"Parse once, validate. invalid show message, no ops. valid? build complete AST, memoized, cached / 0 rules applied / consumer 100% trust validator, no garbage defensive programming, if it was failed, you validator is garbage"* | The Document contract. Stages 7B and 7C |
| 20 | *"there is no pattern to invent, just follow established pattern verbatim. Document with validator is THE ESTABLISHED PATTERN. THAT'S THE CONTRACT"* | Stage 7B and 7C copy the JsonDocument, MermaidDiagram and CssDocument forms (inventory) |
| 21 | *"JsonFilter itself is smell. if jq program is json itself it only need to be registered as Rules, Vocabulary, Tokens etc, Validator is registrable"* | The class `JsonFilter` and `JsonFilterValidator` go. The jq program is a registered family of `JsonDocument`; its rules join `JsonValidator` |
| 22 | *"possibly jq program could be simply written as LUT, cast table generated. downstream all Validator logic filtered by the LUT"* | The jq lexemes, byte classes, precedences, associations, constructs, builtin names and builtin definitions are cast rows. Scanner, build, rules and run read the generated tables |
| 23 | *"feature parity means we want familiarity for args, why always complicate with byte-to-byte translation. FACT: our contract, our topology, our architecture are more consistent, more performant, more deterministic than jq itself."*, *"I SAID NOT FUCKING BYTE-TO-BYTE TRANSLATION"* | `saw json` takes jq's arguments and filter language. Its messages are saw's text rows. No jq message text, no jq grammar name and no jq structure is a target |
| 24 | *"we have so many language we could parse, all with consistent deterministic pattern. there is no excuses."* | The jq family uses the same Document parse, token and validator path as each other JAM grammar |
| 25 | *"every single Json<Object> which is not JsonDocument and its established pattern: Validator, Writer, etc ARE FUCKING GARBAGE NEED TO BE DELETED."*, *"if you keep thinking there must be a struct as JsonGarbage, you have already failed"* | Deleted: `JsonValue`, `JsonProgram`, `JsonAutomaton`, `JsonBuiltins`, `JsonMatches`, `JsonDates`, `JsonMath`, `JsonNumber`, `JsonBoundary`, and every struct they declared (`JsonEnvironment`, `JsonFailure`, `JsonResult`). Kept: `JsonDocument`, `JsonValidator`, `JsonWriter`, `JsonLanguageDocument`, `JsonLanguageValidator`. No step adds a JSON type or a struct |
| 26 | *"Json is the AST, the model. why always, a second model, second this, second that … if there's always need to create struct for json, meaning JsonDocument never complete. the mental model of Document cant hold THE COMPLETE AST, meaning the design still garbage and violate contract"*, *"WHY THE FUCK WE NEED TO COPY jq? jq architecture IS FUCKING GARBAGE. i dont give a fuck how jq structured."* | Stage 7C: `JsonDocument` is the complete AST, in the Document pattern (Ruling 27). The filter tree is the program. jq's structure is never a reference |
| 27 | *"Rules, Vocabulary and Token are Document contract"*, *"we have so many OOTB language parser all using the exact consistent pattern adherence to /contract … why now json change must have its own pattern and break all consumer?"*, *"what's wrong with just implement verbatim per codebase established pattern?"*, *"DONT BREAK OTHER PROJECT, means if genuinely no need to change consumer contract, only internal correction, DONT FUCKING TOUCH OTHER PROJECT."*, *"compareNumbers << foreign semantics"* | Stage 7D: `JsonDocument` follows the Document pattern of the sibling parsers and keeps the public readers that its consumers call. Steps 44d and 44e are void. No consumer source changes |

## Language / Framework Constraints

- LANGUAGE.md C++/JUCE: one object, one header and one `.cpp`. 30 lines for a function, 3 branches.
- A new identifier, text, colour, metric, lexeme, operator, builtin name or builtin definition is a row in `jam/cast/*.md`, `ling/cast/*.md` or `saw/cast/*.md`. `generated/` is written by cast in the build. No hand edit inside a generated region (`BEGIN_/END_` pairs, `user-modules-info.md:1438-1445`).
- A new JAM file is a row in `user-modules-info.md` `## submodule` (`:185`). A deleted JAM file loses its rows.
- Slim rule: saw links `jam_syntax_tree` (deps `juce_core`, `jam_core`, `user-modules-info.md:152`) and no GUI module. MCP and every verb except `page` load no library.
- Two JUCE copies in one process: established. `liblanguage.dylib` links `liblanguageCore.a`, which holds `juce_core` and `juce_events`, with hidden visibility (`language.cast:6176-6183`, `core.cast:839-855`), inside ling.
- Windows: the same mechanism, `language.dll` beside `liblanguage.dylib` (`files.md:30-31`).
- Clean room: never open `jq/src`, `bat/src`, or a grammar file. Running the installed `jq` or `bat` as a witness, and reading their tests and docs, is permitted. A witness run shows user-visible output only. It never decides an internal design (Ruling 26).
- String rule (Ruling 18). Text is a `std::string_view` over its source. `juce::String` appears only as the argument or the result of a JUCE call, or as the value of an existing `juce::String` property; a result is read at once with `jam::Strings::getView` (`jam_Strings.h:646`). Work on text uses these and nothing else:
  - append: `jam::Format::appendBytes`, `appendByte`, `appendUtf8`, `appendDecimal`, `appendNumber`, `appendHex`, `appendQuoted` (`jam_Format.h:408-736`); trim: `jam::Format::trim (view)` (`:887`); number: `jam::Format::getNumber` (`:811`);
  - lists and splits: `jam::Strings::fromTokens`, `fromLines`, `getView` (`jam_Strings.h:462-471`);
  - UTF-8: `jam::Utf8::isValid`, `getCodepointCount`, `setCodepoints`, `setBytes`, `getPrefix`, `getEncodedBytes` (`jam_Utf8.h:9-14`);
  - search and count: `jam::simd::findByte`, `findBytePair`, `findFingerprint`, `findNonPlainByte`, `findControlByte`, `findNonPrintableAscii`, `countByte` (`jam_Simd.h:283-489`), `jam::LiteralSet`;
  - line and column of an offset: `Document::getLineNumber`, `getColumnNumber`, `getByteOffset` (`jam_Document.h:1180-1199`, `.cpp:293-309`).
  - Not used: `juce::String` methods, `juce::StringArray`, `std::to_string`, `std::stoi`, `std::string` `find`, `find_last_of`, `substr` copies, `operator+`, `push_back`, `toStdString`, `resize` fills, and byte or codepoint loops.
- Agents: at most 3 at one time. Engineer and Pathfinder `haiku`, Auditor `opus`. No ling call in a brief. Watchdog 15 minutes. Dry run before each `--apply`. No git by agents except read-only `status`, `log`, `diff`, `show` by the main session and Pathfinder.

## Framework Inventory

Rows of Stages 1 to 7B keep their citations of the session that read them. Rows marked (7C) were read in this session.

### 1. API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Add a line element | jam_core | `Document::addChild (parent, after, id)` | `jam_Document.h:1346`, `.cpp:637-649` | 6 |
| Line index | jam_core | `Document::Index` | `jam_DocumentIndex.cpp:7-134` | 6 |
| Segments of cells | jam_document | `ReflowDocument::getCellSegments` | `jam_ReflowDocument.cpp:600` | 7-10 |
| File as a page | jam_markdown_graphics | `FilePreview::setFile`: code block plus metadata | `jam_FilePreview.cpp:54-80` | 11 |
| Header rows | jam_markdown | `MarkdownDocument::addProperties` | `jam_MarkdownDocument.cpp:1442-1453` | 11, 18 |
| Code block cells | jam_markdown | `MarkdownDocument::getCells` | `:1346-1380` | 12 |
| Join documents | jam_core | `Document::appendChildren` | `jam_Document.cpp:689-720` | 18 |
| Coloured cells | jam_core | `AttributedString::append (view, style)` | `jam_MarkdownDocument.cpp:1206-1213`, `:1374` | 12 |
| CSS to LookAndFeel | jam_style | `StyleManager::setAppearance` | `jam_StyleManager.cpp:174-202` | 13, 16, 18 |
| Theme | jam_style | `StyleManager (fetchers, styleSheets)` | `jam_StyleManager.cpp:14-18` | 18 |
| Syntax colour ids | jam_style | `markdownSyntax…ColourId` | `cast/style.md:168-172`, `style.css:79-88` | 12 |
| Fill of a line | jam_gui | `TextEditor::paintFills` | `jam_TextEditor.cpp:2994-3005` | 12 |
| Reader | jam_gui | `TextEditor::setDocument`, reader mode, `getKeyInputs` | `jam_TextEditor.h:212`, `.cpp:925-928`, `:1752-1779` | 15-17 |
| Component beside the text | jam_gui | `TextEditorStatusBar` | `jam_TextEditorStatusBar.h:20-60` | 14 |
| Gutter API | juce_gui_extra | `CodeEditorComponent::setLineNumbersShown`, `getGutterSize`, `lineNumberTextId` | `juce_CodeEditorComponent.cpp:494-536`, `.h:310` | 14 |
| Wrap API | juce_gui_basics | `TextEditor::setMultiLine (…, shouldWordWrap)` | `juce_TextEditor.h:87` | 15 |
| Page | jam_tui | `TerminalPage` | `jam_TerminalPage.{h,cpp}` | 16-18 |
| Paint to cells | jam_terminal | `TerminalGraphicsContext` | `jam_TerminalGraphicsContext.h:8-53` | 16 |
| Grid lines | jam_terminal | a fill thinner than one cell joins its neighbours | `.cpp:102-117`, `:197-242` | 14, 16 |
| Bytes of a frame | jam_terminal | `TerminalBuffer::writeToStream (output, laf)` | `jam_TerminalBuffer.cpp:502-515` | 16 |
| Window in a terminal | jam_gui | `TerminalWindow`, `TerminalComponentPeer` | `jam_TerminalWindow.h:9-45`, `jam_TerminalComponentPeer.cpp:4-177` | 17 |
| Terminal input and size | jam_terminal | `TerminalDisplay`, `TerminalKeyboard::applyKeyPresses` | `jam_TerminalDisplay.h:21-75`, `jam_TerminalKeyboard.h:152` | 17 |
| C boundary | jam_syntax_tree | `jam_LanguageInterface.h`, `LanguageLibrary` | `jam_LanguageInterface.h:96-132`, `jam_LanguageLibrary.cpp:5-26` | 19 |
| Tokens | jam_syntax_tree | `LanguageDocument` (*"The tokenizer is CodeDocument's. The library supplies the vocabulary and the keyword set"*) | `jam_LanguageDocument.h:11`, `jam_CodeDocument.cpp:42-82` | 22, 26 |
| Carried library build | ling | `language-bundle`, `cast --pack` | `cmake.cast:504-534`, `project-info.md:255-284` | 20 |
| Unpack | ling | `Processor::getOrCreateLanguages` (every zip) | `Processor.cpp:513-562` | 20, 21 |
| Entry export | ling | `ling_get_language` | `language.cast:6130-6161` | 20, 22 |
| Inflate | juce_core | `juce::GZIPDecompressorInputStream` | `juce_GZIPDecompressorInputStream.h:53` | 24 |
| Line diff | saw | `LineDiff::applyToChanges` | `PageChanges.cpp:49-63` | 25 |
| Escape sequences | jam_terminal | `AnsiSequence::getToken` | `jam_AnsiSequence.h:7-18` | 27 |
| UTF-16 text | juce_core | `juce::String::createStringFromData`, read with `jam::Strings::getView`, as `Document::parse` does (`jam_Document.cpp:942`) | `juce_String.cpp:1974-2007` | 27 |
| Text | jam_core | `jam::Format`, `jam::Strings`, `jam::Utf8`, `jam::simd` (String rule) | `jam_Format.h`, `jam_Strings.h`, `jam_Utf8.h`, `jam_Simd.h` | all |
| Plain page | saw | `getPlainOutput`, `PageRanges` | `Page.cpp:436-452`, `PageRanges.cpp` | 28 |
| Calendar | juce_core | `juce::Time` | used at `jam_FilePreview.cpp:18` | 44h |
| Parse, then validate, then hand out | jam_core | `Document::getValid<DocumentType, ValidatorType>`: nullptr and a `debug::Log` line on failure (7C) | `jam_Document.h:1242-1270` | 40, 43, 44d |
| Rule set (7C) | jam_core | `Document::Validator`: `getRules`, `isValid`, static `isValid (rules, document)`, `getFailures` | `jam_Document.h:836-872`, `.cpp:167-195` | 41, 44f |
| Scan driver | jam_core | `Document::parse()`: calls `getToken` per segment, then `build` | `jam_Document.cpp:1418-1450` | 40 |
| Owned source with line starts | jam_core | `Document::addSource (std::string&&)` / `(std::string_view)` call `addLineOffsets` | `jam_Document.cpp:246-280` | 40 |
| Line and column | jam_core | `getLineNumber`, `getColumnNumber`, `getByteOffset` | `jam_Document.h:1180-1199`, `.cpp:293-309` | 41, 44d |
| Memoized document (7C) | jam_core | `SharedDocuments::getOrCreate<DocumentType> (Identifier, factory)`; a failed creation is not stored | `jam_Document.h:1663-1678` | 43, 44g |
| Element and token properties (7C) | jam_core | `Element::add<ValueType> (key, …)`, `Token::add<ValueType> (key, …)`; `Element::Value` holds `bool`, `double`, `juce::String`, `Tokens`, `Element*`, `Elements`, `Integers` | `jam_Document.h:173-178`, `:437-442`, `:190-205` | 44d |
| A child element (7C) | jam_core | `Document::addChild (parent, id)`, `addChild (parent, after, id)`, `addChild (parent, after, child)`, `removeChild` | `jam_Document.h:1337-1364`, `.cpp:610-675` | 44d, 44g |
| A span on an element (7C) | jam_core | `Document::addRange (element, start, end, key = Id::range)` | `jam_Document.h:1430`, `.cpp:871-874` | 44d |
| A link between elements (7C) | jam_core | `Document::addLinkEndpoint (element, key, linked)`: adds `linked` to the `Elements` of `key` | `jam_Document.h:1621`, `.cpp:282-292` | 44f, 44g |
| A child by id (7C) | jam_core | `Element::getChildByID (element, id)` | `jam_Document.h:275-286` | 44g |
| Walk with a prune (7C) | jam_core | `Element::applyFunctionRecursively (function)`: a `false` return prunes the walk | `jam_Document.h:383-396` | 44g |
| Parts of the Document pattern (7C) | jam_core | `Element`, `Token`, `Index`, `Vocabulary`, `Cursor`, `Validator`, `Writer`, `SwapWriter`, `SwapReader`, `SharedDocuments` | ling `documentSymbol` of `jam_Document.h` | 44g |
| JSON scan dispatch (7C) | jam_core | `JsonDocument::getToken`: `LookupTable<int, Construction, numByteClasses>` by byte class, with an expectation table | `jam_JsonDocument.cpp:573-621` | 44d |
| JSON builders (7C) | jam_core | `addContainer`, `addClose`, `addString` (decodes an escaped string once into `Id::text`), `addNumber`, `addLiteral`, `closeContainer`, `addMemberIndex` (`Id::index`, last value of a repeated key wins) | `jam_JsonDocument.cpp:427-545`, `:788-831` | 44d |
| JSON build end (7C) | jam_core | `JsonDocument::build`: unclosed, root, UTF-8 failures; the root holds the pending tokens | `jam_JsonDocument.cpp:623-648` | 44d |
| Value stream (7C) | jam_core | `readValues (text, isFinal, firstLine, firstColumn, isPartial)`, `getValueStream`, `getValues`, `getConsumedLength`, `getOpenDepth`, `getValueResult`, `scanValues`, `finishValues`, `rollBack`, `commitValue` | `jam_JsonDocument.cpp:650-664`, `:711-760`, `:846-890` | 44d, 44i |
| JSON readers (7C) | jam_core | `getValue`, `get (object, name)`, `contains`, `getChildByToken`, `getBody`, `getString`, `getDecodedBody`, `getBoolean` | `jam_JsonDocument.cpp:95-157`, `:205-244`; `.h:215-279`, `:530` | 44d, 44e |
| Failure record (7C) | jam_core | `JsonDocument::setFailure (rule, offset, failRow)`; `getRuleResult (rule)`: rule, separator, text, line, colon, column | `jam_JsonDocument.cpp:251-256`, `:139-151` | 44d |
| Filter entry (7C) | jam_core | `JsonDocument::readFilter (text)`: `std::pair<juce::Result, std::unique_ptr<JsonDocument>>`; `getOrCreate (name, getText)` for a module | `jam_JsonDocument.cpp:667-692` | 44f, 44g |
| Filter construct (7C) | jam_core | `addConstruct (parent, construct)`: an `Id::value` element with `Id::type` = `map::JsonFilterType`; `addWrapper`, `addAbsent`; `getType`, `getChild`, `getChildCount` | `jam_JsonDocument.cpp:1236-1256`, `:2279-2302` | 44g |
| Filter dispatch tables (7C) | jam_core | `getPrimaries`, `getSuffixes`, `getMemberKeys` (by lexeme), `getLinks` (by `map::JsonFilterType`), `static constexpr LookupTable` of member function pointers | `jam_JsonDocument.cpp:1605-1722` | 44g |
| Filter links (7C) | jam_core | `addLinks` and its walk: a `variable`, `call`, `variablePattern` or `breakLabel` reference lists its binding under `Id::to`; the binding lists it under `Id::in`; function keys `name/arity` | `jam_JsonDocument.cpp:2365-2543`, `getFunctionKey` `:2340-2347` | 44f, 44g |
| Rules of JSON (7C) | jam_core | `JsonValidator::addRules (Rules&)` (six JSON rules), `addRules (Rules&, externalNames)` (`Id::link`, `Id::object`), `addModuleRules` (`Id::body`), `getRules` (built once), `isValidRule` | `jam_JsonValidator.cpp:5-123` | 44f |
| JSON text of a document (7C) | jam_core | `JsonWriter::getText`, `appendDocument`, `appendValue` dispatch by token type (`Function::Map`); the `Layout` family of templates; `appendEscaped` (one `findFingerprint` per run) | `jam_JsonWriter.cpp:10-47`; `jam_JsonWriter.h:74-177`, `:525-544` | 44e |
| JSON language elements (7C) | jam_core | `JsonLanguageDocument::build`, `addMemberKeys` (a `Id::key` element with `Id::nameTokens` per member), `addFlatTokens`, `setElementIndices` | `jam_JsonLanguageDocument.cpp:24-152` | 44e |
| Token kinds of JSON | jam_core | `map::JsonTokenType`: object 0, array 1, string 2, number 3, boolean 4, null 5, text 6 (7C) | `cast/core.md:220-236` | 44c, 44d |
| Construct kinds of jq | jam_core | `map::JsonFilterType` (cast `core.md:238-286`), keys up to `absent` | `generated/jam_Core.h` | 44g |
| Spelling table | cast | Bimap `name | key | value` → `struct X : jam::Bimap<int>` with `enum value` | `mermaid.md:1900-2012` → `jam_Mermaid.h:1371-1579`; `syntax_tree.md:94-205` → `jam_SyntaxTree.h:140-355` | 38, 44c |
| Byte table | cast | `key | value` → `inline constexpr jam::LookupTable<int, int, 256>` | `syntax_tree.md:318-361` → `jam_SyntaxTree.h:561-600`; spell row `jam/cast/spell.md:1381-1391`; shape `code.cast:297-304` | 38 |
| Kind table | cast | `name | key` → Bimap with `enum value` | `mermaid.md:1447-1469` → `jam_Mermaid.h:1580-1618`; `web.md:62-99` → `jam_Web.h:70-112` | 38 |
| String rows (7C) | cast | `## identifiers`: `type | name | value | description`, with `@id` and `@string` rows | `cast/core.md:23-37` | 44c |
| Longest operator match | jam_mermaid_diagram | `MermaidDiagram::getOperators`: ordinals sorted longest-first once, first spelling at the cursor | `jam_MermaidDiagram.cpp:1008-1058` | 40 |

### 2. Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| A document adds its lines, the editor lays them out | `jam_TextEditor.h:212-218` | `MarkdownDocument::addLines`; `TerminalPage::setDocument`; EVE `EVEView.cpp:427`, `:610` | 6-12 |
| A child Component of the editor with its own `ColourIds`, read from the LookAndFeel | `jam_TextEditorStatusBar.h:26-37` | `jam_TextEditor.cpp:29`, `:2019-2028` | 14 |
| A LookAndFeel method draws, an override styles | `StyleCustom::drawPaneEdge`, `getPaneEdgeSize` (`jam_StyleCustom.cpp:153-163`) | `StyleMarkdown`, `StyleTheme` | 13, 14 |
| A metric is a row and a CSS property | `metadataLabelWidth` (`cast/markdown_graphics.md:110`, `style.css:165`) | `StyleMarkdown::getMetrics` | 13 |
| Exported entry function, entries by name | `language.cast:6137-6161` | `LanguageLibrary.cpp:13-21` | 19, 20, 22 |
| A carried manifest folder with `spell.md` and its cast template | `ling/language/core/` | `cmake.cast:507-533` | 20, 22 |
| One object owns a dynamic library and its functions | `LanguageLibrary` | `LibraryLanguage.h:139`, `Processor.cpp:601` | 19 |
| Bytes of a page inside a GUI scope | `ling/help/HelpFrames.cpp:17-26`, `:39` | saw `help/HelpFrames.cpp:20` | 18 |
| `parse`/`read` = `getValid<D, V> (getDocument<D> (text))` | `jam_JsonLanguageDocument.cpp:5-8` (7C) | `jam_MermaidDiagram.cpp:22-25`, `jam_Css.cpp:90-93`, `jam_LanguageDocument.cpp:12-24`; `getValid` callers `jam_Xml.cpp:12,26,33,43`, `jam_GitDocument.cpp:11`, `jam_MarkdownDocument.cpp:12,38,55,65`, `jam_CodeDocument.cpp:18`, `jam_Html.cpp:18,28` | 40, 43 |
| The scan records one failure and stops; `build` sets structure failures; each rule reads the record (7C) | `jam_JsonDocument.cpp:613-618`, `:623-648`, `:139-151`; `jam_JsonValidator.cpp:5-13`, `:106-109` | the jq family (`failAt`, `jam_JsonDocument.cpp:1350-1354`) | 40, 41, 44d |
| A family ordinal selects the vocabulary and the tree construction | `jam_MermaidDiagram.cpp:124-163`, `:425-470`; member `family` `jam_MermaidDiagram.h:3648` | `CodeDocument::getVocabularyByFamily` `jam_CodeDocument.cpp:234-262`; the jq family (`isFilter`, `jam_JsonDocument.cpp:575-576`, `:625-631`) | 40 |
| The build marks an element; a rule lists the marked elements | CSS: `jam_Css.h:905`; `jam_CssValidator.cpp:31-52` | `jam_MermaidValidator.cpp:25-64`, `jam_XmlValidator.cpp:32-62`; `JsonValidator` `Id::link` (`jam_JsonValidator.cpp:18-50`, 7C) | 41, 44f |
| Rule set built once in a function-local static (7C) | `jam_JsonValidator.cpp:111-123` | `jam_MermaidValidator.cpp:67-79`, `jam_CssValidator.cpp:71-82` | 41, 44f |
| Memoized document from a text function (7C) | `JsonDocument::getOrCreate (name, getText)` `jam_JsonDocument.cpp:682-692`; `CssDocument::getOrCreate` `jam_Css.cpp:124-130` | `jam_MarkdownDocument.cpp:5-14`; 7 callers (`jam_Xml.cpp:7`, `jam_ConfigDocument.cpp:8,16`, `jam_MarkdownDocument.cpp:7`, `jam_Css.cpp:115,126`, `jam_Html.cpp:7`) | 43, 44g |
| A dispatch table of member function pointers keyed by a construct (7C) | `JsonDocument::getLinks` `jam_JsonDocument.cpp:1634-1682`, called `:2365-2368` | `getPrimaries` `:1605-1632`; `MarkdownDocument::getLineConstruction` `jam_MarkdownDocument.cpp:964-991` | 44g |
| A typed element: `Id::value` with `Id::type` (7C) | `JsonDocument::addConstruct` `jam_JsonDocument.cpp:1236-1241` | every filter construct | 44g |
| A reference lists its target under `Id::to` (7C) | `JsonDocument::addReference` `jam_JsonDocument.cpp:2515-2528` | `addVariableLink`, `addCallLink`, `addBreakLink`, `addPatternVariableLink` `:2487-2513` | 44g |
| A member key on an element as `Id::nameTokens` (7C) | `JsonLanguageDocument::addMemberKeys` `jam_JsonLanguageDocument.cpp:69-88` | the language document of JSON | 44d |
| A decoded text on a token as `Id::text` (7C) | `JsonDocument::addString` `jam_JsonDocument.cpp:485-491`; `addValue (std::string_view)` `:702-708` | `getDecodedBody` `:210-216` | 44d |
| A cast table → a spell row → a generated header | `jam/cast/spell.md:1336-1391` | `syntax_tree:CppKeyword`, `syntax_tree:code` | 38, 44c |
| A failure text is a text row | `text::English::failUnclosed` at `jam_JsonDocument.cpp:636` (7C); `text::English::failNoRoot` at `jam_MermaidValidator.cpp:13` | `jam_XmlValidator.cpp:25,29`; `jam_JsonValidator.cpp:42`, `:70`, `:95` (7C) | 41, 44d |

### 3. Names

| Family | Members | Intent | Step |
|---|---|---|---|
| `Document::add…` | `addChild`, `addRange` (`jam_Document.h:1337-1430`) | `addLine`, `addColumn`, `addFill`: insert into the tree | 6 |
| `MarkdownDocument::add…` | `addProperties`, `addTextLines`, `addObjectLine` | `addCodeBlock` | 11 |
| `TextEditor<Part>` | `TextEditorStatusBar` | `TextEditorGutter` | 14 |
| JUCE code editor | `setLineNumbersShown`, `getGutterSize`, `lineNumberTextId`, `lineNumberBackgroundId` | same names on `TextEditor`, `StyleCustom`, and colour rows `textEditorLineNumberTextColourId`, `textEditorLineNumberBackgroundColourId` | 13, 14 |
| `StyleCustom::draw…`, `get…Size` | `drawPaneEdge`, `getPaneEdgeSize` | `drawGutter`, `getGutterSize` | 13 |
| Language boundary | `LanguageInterface`, `LanguageLibrary`, `LanguageEntry`, `LanguageCallbacks` | `PageInterface`, `PageLibrary`, `PageEntry`, `PageCallbacks` | 19 |
| `Terminal…` in jam_tui | `TerminalPage` | `TerminalReader` (editor mode `map::EditorMode::reader`) | 17 |
| Carried libraries of ling | `language`, `languageCore` (`files.md:23-31`) | `page`, `pageLibrary`, `pageLibraryWindows` | 20 |
| Documents | `AnsiDocument`, `CodeDocument`, `ReflowDocument` | `GitDocument`, `GitValidator`; module `jam_git` (tit plan `:224`) | 24 |
| Ids | `Id::number` (`core.md:141`), `Id::change`, `Id::syntax`, `Id::info` (`markdown.md:57`) | no new id for the code page | 11, 12 |
| JSON kind tables in `core.md` | `JsonTokenType` (`:220`), `JsonFilterType` (`:238`), `JsonFilterLexeme`, `JsonFilterByte`, `JsonFilterPrecedence`, `JsonFilterAssociation`, `JsonFilterOperator` | `JsonFilterBuiltin` (Bimap `name | key | value`; value = the function key `name/arity` of `getFunctionKey`, `jam_JsonDocument.cpp:2340-2347`) | 38, 44c |
| Byte and value lookup tables | `code`, `markup` (`syntax_tree.md:318`), `css` (`web.md:243`), `markdown` (`markdown.md:444`), `jsonFilter`, `jsonFilterPrecedence`, `jsonFilterAssociation`, `jsonFilterConstruct` | `JsonFilterDefinition`: the builtins written in the filter language, a Bimap `name | key | value` in the order of the definitions (form of `JsonFilterBuiltin`; a hash-map keeps no order, and the scope walk binds each definition after the ones before it) | 38, 44c |
| Ids of a JSON value element (7C) | `Id::object`, `Id::array`, `Id::string`, `Id::number`, `Id::null` (`core.md:44`, `:142`, `:168`, `:141`, `:140`) | `Id::boolean` (new `@id` row, the `JsonTokenType` name `core.md:233`); `Id::error` (new `@id` row: the error value of a run). No other new id | 44c |
| Properties of a JSON value element (7C) | `Id::range` (`core.md:156`), `Id::text` (`:175`), `Id::number`, `Id::value` (`:188`), `Id::nameTokens` (`:138`), `Id::index` (`:114`) | span, decoded or computed text, number, boolean, member key, member order | 44d |
| `JsonDocument` readers | `parse`, `read`, `readValues`, `readFilter`, `isValid`, `fromString`, `getOrCreate` (`jam_JsonDocument.h:25-118`) | no new reader name; the readers of values take `const Document::Element&` (Step 44d) | 44d |
| `JsonDocument` value readers (7C) | JUCE `juce::var` (`juce_Variant.h:147-157`, `:290`, `:292`): `isVoid`, `isBool`, `isDouble`, `isString`, `isObject`, `isArray`, `getProperty`, `hasProperty`; `juce::NamedValueSet::getName`. ARCHITECT: *"our Document built upon juce::XmlElement/Document DX parity"* | `isObject`, `isArray`, `isString`, `isDouble`, `isBool`, `isVoid` (in place of `isContainer`); `getProperty`, `hasProperty` (in place of `get`, `contains`); `getName` (member key); `getValue`, `getValues`, `getString`, `getBody`, `getDecodedBody`, `getBoolean`, `getNumber` take `const Element&`; `getChildByToken` goes | 44d |
| `JsonDocument` creators (7C) | `addValue (double)`, `addValue (std::string_view)` (`jam_JsonDocument.h:55-56`), `addConstruct`, `addWrapper`, `addAbsent` | `addValue` overloads for a boolean, a null, an array and an object; each returns the new `Element&` | 44g |
| `apply` with a callable (NAMES Rule 1) | `applyFunctionRecursively` (`jam_Document.h:395-396`), `applyToRecords`, `applyToChanges` | `applyToResults (filter, input, function)`: the run; `function` receives each result element | 44g |
| `JsonDocument` construct tables (7C) | `getPrimaries`, `getSuffixes`, `getMemberKeys`, `getLinks` (`jam_JsonDocument.h:658-698`) | `getResults`: the table keyed by `map::JsonFilterType`; `getBuiltins`: the table keyed by `JsonFilterBuiltin` | 44g |
| Validator rule keys | `Id::root`, `Id::structure`, `Id::string`, `Id::number`, `Id::value`, `Id::depth`, `Id::link`, `Id::object`, `Id::body` (`jam_JsonValidator.cpp:7-12`, `:18`, `:52`, `:83`) | no new rule key | 44f |

### 4. Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| "Uniform signature in the type" | `Function::Map<int, pointerCapacity, void (MarkdownDocument&, Element&)>` | `jam_MarkdownDocument.cpp:964-991`; `jam_JsonWriter.cpp:31-46` (7C) |
| "No fake-carrier structs" | plain arguments; the element is the value | `jam_LanguageInterface.h:132`; `JsonDocument::addReference (Element&, const std::string&)` `jam_JsonDocument.cpp:2515` (7C) |
| "No out-parameters" | `std::pair<juce::Result, …>` | `JsonDocument::readFilter` `jam_JsonDocument.cpp:667-680` (7C); `saw/Source/Json.cpp:40-54` (7C) |
| String rule | `Format::appendBytes` runs over a view, one SIMD call for each run | `jam_JsonWriter.h:525-544` (7C) |
| 3-branch rule | `static constexpr LookupTable` of member function pointers | `jam_JsonDocument.cpp:579-594`, `:1634-1682` (7C) |
| "No non-owning naked pointers as members" | the dependency comes at the call | `MarkdownGraphics::addTextLine (…, style, drawableCache)` |
| Owner of a library | `std::shared_ptr<const LanguageLibrary>` that owns `juce::DynamicLibrary` | `jam_LanguageLibrary.cpp:21`, `:83` |
| "Use the framework API fully" (validation) | the consumer calls `getValid`, or `Validator {}.isValid` once, and trusts the tree | `jam_JsonDocument.cpp:667-692` (7C) |
| Magic values | each table value is a cast row; each failure text is a `text::English` row | `jam_JsonDocument.cpp:636`, `:640`, `:646` (7C) |
| A run is a creation (7C) | MANIFESTO E: *"Creation can use multiple passes, look-ahead, reverse steps, work buffers, and memory allocation"*; *"Transient state only — calculation buffers, intermediate values, anything that lives and dies within a single operation"* | `MANIFESTO.md:254`, `:288` |
| A value passed along is transit (7C) | MANIFESTO E: *"No copy — read the data in its location"*; *"A scalar, a handle, or a view passed by value does not copy the data it names"* | `MANIFESTO.md:258-267`, `:280` |

### 5. Absence

| Need | Query | Result |
|---|---|---|
| Git reader in JAM | JAM doxygen `index.xml`: only mermaid `Git…`; tit plan `:224` | none. `jam_git` is the second-consumer extraction |
| Gutter in `jam::TextEditor` | JAM doxygen `index.xml`, `<name>setLineNumbersShown</name>` | empty. The JUCE family is the pattern |
| A host that opens `TerminalWindow` | Pathfinder text search over 12 project `Source` folders | no match. The wiring of Step 17 is new; its parts exist |
| Code cells with a token colour | `getCells` read whole (`:1346-1380`) | one style for the block |
| A published path of ling's libraries | `ling/cast/files.md`, `Source/HELP.md:317-323` | `<cache>/language/<hash>` is written; no program other than ling reads it |
| UTF-16 reader in JAM | JAM doxygen `index.xml`, names with `utf16` | only `OpenTypeWriter::getUtf16ByteLength`, `addUtf16Text` (writers) |
| A cast table with a precedence, associativity or order column | Pathfinder read of `syntax_tree.md` and `mermaid.md` headings | none. The plain `key | value` lookup table is the form |
| A cast column that generates a callable | same read | none. Dispatch tables of member function pointers stay in code, as `jam_JsonDocument.cpp:1634-1682` |
| An element for a JSON scalar (7C) | `getChildByToken` read (`jam_JsonDocument.cpp:132-137`), doc `jam_JsonDocument.h:241-254` | none: only a container token has an element. Step 44d corrects the parse |
| An evaluator part of the Document pattern (7C) | ling `documentSymbol` of `jam_Document.h`, every struct | none. The run is a creation of a value-stream `JsonDocument` (Step 44g) |
| Ids `boolean`, `error`, `items` (7C) | `saw find` over `jam/cast/core.md` `## identifiers` rows | absent. Step 44c adds `boolean` and `error`; `items` is not needed |

**Facts corrected by a read (7C):** `jam_JsonDocument.cpp` is 2,647 lines and holds the JSON family and the jq family (ling `documentSymbol`). The value-stream family writes jq message literals: `failNumeric` to `failEscape` (`jam_JsonDocument.h:416-429`) and `" at EOF"`, `" at line "`, `", column "` (`jam_JsonDocument.cpp:746-748`); Ruling 23 makes them text rows. `JsonValidator.cpp:35` still names the deleted `JsonBuiltins::find`. `JsonWriter.h:165-171` takes a `Number` type with `isNan` and `appendLiteral` (the deleted `JsonNumber`). `JsonDocument` keeps the link walk state as members `bindings` and `scopeMarks` (`jam_JsonDocument.h:680-681`); `bindings` holds `Element*`, which breaks CODING.md "No non-owning naked pointers as members".

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering the whole sprint — never per step.

Common to every step: COUNSELOR reads every changed line, and compares it with the brief, the inventory rows and the rulings. The brief pastes the sibling lines verbatim. A name outside table 3 or a cast row fails the step. Page steps fail when code outside a LookAndFeel reads a colour or a metric, or when code outside `TerminalEncoder` writes SGR. A step fails when its diff holds an item of the *Not used* list of the String rule. Stage 7B and 7C steps fail when a table value stays in code, when a jq message text or a jq grammar name appears, when a consumer checks a validated tree, when a failure of the jq family is thrown or caught (one catch boundary only: the run's call of the JAM regular expression API, whose documented contract throws, `jam_RegExp.h:181`, `:217`, `jam_RegExpParser.h:27-30`; the run turns that throw into its jq error), when a JSON type other than the five kept ones appears, when a struct carries a value, or when a value is copied in transit.

**Blast-radius check of each JAM step:** COUNSELOR reads the use sites of each changed symbol (ling `findReferences` in the main session, doxygen index of each consumer). A public signature that a consumer calls does not change, except where this plan names the change and its consumers. Consumers: `cast`, `end`, `eve`, `ggwp`, `ling`, `sandbox`, `scout`, `stamp`, `tit`, `whatdbg`, `whelmed`.

## Steps

Steps 0 to 43 and 44a, 44b are done. Their text stays here because it is the specification of the code on disk.

### Stage 0

**Step 0: Plan file.** Done. Scope: `PLAN-one-tool.md`. Action: write the plan text.

### Stage 1 — Delete first (done)

**Step 1: JAM garbage.** Scope: `jam_core/yaml/` (4 files), `jam_syntax_tree/document/jam_Syntax{Definition,Language,Theme}.{h,cpp}`, the `JAM_USING_TERMINAL_DISPLAY` blocks (`jam_terminal.h:187-193`, `jam_terminal.cpp:106-113`), and the slim branches that only those blocks use. Action: `rm`, and remove the blocks. `TerminalDisplay` keeps the input handle argument (Step 17). Validation: no include, row or macro names them.

**Step 2: saw page garbage.** Scope: `Source/{Decorations,PageColours,PageChanges,Pager}.{h,cpp}`, `Source/PageText.{h,cpp}`, `Source/themes/`, `project-info.md:190,191,222`, the cast rows of themes, styles and paging that name a tmTheme, the `page` rows of `test/cases.md` that expect our own decorations. Action: `rm`, and delete rows. `Page.cpp:139-434` goes with them. Validation: `Page.cpp` holds the plain lane only.

**Step 3: Terminal encoder.** Scope: `jam_TerminalEncoder.{h,cpp}`. Action: read every caller of the `int` branch of `TerminalEncoder::Colour` (`.cpp:11-15`). The branch that only the deleted `PageColours` used goes. Validation: the caller list.

### Stage 2 — One line at the Document base (done)

**Step 4: Read before the edit.** COUNSELOR read the eight builders and the `TextBlock`, `Index`, `MarkdownGraphics` readers at file:line, and wrote the substitution of each builder into the briefs.

**Step 5: Ids.** Scope: `cast/style.md:29`, `cast/graphics.md:42`, `cast/core.md`. Action: rows `advances` and `fill` move to `cast/core.md`.

**Step 6: Base.** Scope: `jam_Document.{h,cpp}`. Action:
- `Element* addLine (Element& parent, Element* after, Tokens segments)`: tokens, empty row.
- `Element* addLine (Element& parent, Element* after, AttributedString cells, Tokens segments)`: the first form, and cells, one full row span, advances, images.
- `Element* addColumn (Element& line, AttributedString cells, Tokens segments, double left, double width)`.
- `Element* addFill (Element& line, double left, double width, double height, int colourId)`.
- Validation: `AttributedString` precedes `Document` (`jam_core.h:115,117`); no new include.

**Step 7: Reflow.** Scope: `jam_ReflowDocument.cpp:628-636`. Action: base call, then `span`.

**Step 8: Terminal.** Scope: `jam_AnsiDocument.{h,cpp}` (`:62-105`, `:463-565`), `jam_TerminalHistory.cpp:8-37`. Action: `AnsiDocument::getSegments (cells)` replaces `addSegments`. Five sites call the base. `addImages` fills the list that the base added.

**Step 9: Editor.** Scope: `jam_TextEditor.cpp:1822-1831`. Action: base call.

**Step 10: Markdown.** Scope: `jam_MarkdownDocument.cpp:1025-1056`, `:1179-1204`. Action: base call and `addColumn` for table cells. Validation of 7 to 10: each line has the same properties as before; no `add<…> (Id::advances)`, `Id::images` or `Id::fill` child remains outside the base.

### Stage 3 — The page is a Component (done)

**Step 11: `MarkdownDocument::addCodeBlock`.** Scope: `jam_MarkdownDocument.{h,cpp}`, `jam_FilePreview.cpp:9`, `:54-80`. Action: sibling of `addProperties (:1442)`; the element shape of the parser (`jam_MarkdownBlockParser.cpp:923-932`, `jam_MarkdownInlineParser.cpp:64-70`); tokens (`Id::syntax`), first line number (`Id::number`), changes (`Id::change`). `FilePreview` calls it.

**Step 12: Cells and line ids.** Scope: `jam_MarkdownDocument.cpp:1058-1085`, `:1346-1380`. Action: a code block with `Id::syntax` appends one span for each token and each gap; the token type is a colour id; each code line takes `Id::number`, `Id::change`, and `addFill` when highlighted.

**Step 13: LookAndFeel.** Scope: `jam_StyleCustom.{h,cpp}`, `jam_StyleMarkdown.{h,cpp}`, `cast/style.md`, `cast/markdown_graphics.md`, `style.css`. Action: `getGutterSize (int numLines)`, `drawGutter (juce::Graphics&, juce::Component&)`; colour rows; metric row `gutterSize`; CSS properties of the same names.

**Step 14: `TextEditorGutter`.** Scope: new `jam_TextEditorGutter.{h,cpp}`, `jam_TextEditor.{h,cpp}`, `user-modules-info.md`. Action: `setLineNumbersShown (bool)`, default false; `resized()` trims the viewport by `getGutterSize`; the gutter paints through `drawGutter`.

**Step 15: Wrap and tabs.** Scope: `jam_TextEditor.{h,cpp}`. Action: `setMultiLine (bool shouldBeMultiLine, bool shouldWordWrap)`, as `juce_TextEditor.h:87`; the tab width is a metric.

**Step 16: `TerminalPage`.** Scope: `jam_TerminalPage.{h,cpp}`. Action: the page sets its width; `writeToStream` sizes the page to its content and paints it (`paintEntireComponent`); rules are thematic breaks (`MarkdownGraphics::addRule`). Validation: help frames of saw, ling, scout, cast and stamp are the same bytes (Step 46).

**Step 17: `TerminalReader`.** Scope: new `jam_tui/page/jam_TerminalReader.{h,cpp}`, `user-modules-info.md`. Action: owns a `TerminalDisplay`, a `TerminalWindow` with the page, and the message loop; `q`, Escape and Ctrl+C end it; other keys are the editor's reader table (`cast/terminal.md:1994-2275`).

### Stage 4 — ling owns the seam (done)

**Step 18: Page content.** Scope: `jam_tui/page/jam_TerminalPage.{h,cpp}`. Action: `addFile`: one `MarkdownDocument` per file, joined with `Document::appendChildren`; the token type to colour id table is a cast row table in `cast/markdown_graphics.md`.

**Step 19: Interface and loader.** Scope: new `jam_syntax_tree/document/jam_PageInterface.h`, `jam_PageLibrary.{h,cpp}`. Action: siblings of `jam_LanguageInterface.h` and `LanguageLibrary`; entries `create`, `add`, `write`, `show`, `destroy`; `PageLibrary::getPageLibrary (file, entryName)` gives nullptr when the file does not open.

**Step 20: The carried library.** Scope: new `ling/language/page/{spell.md, page.cast}`, `ling/cast/{cmake.cast,files.md,spell.md}`, `ling/project-info.md`, `ling/Source/HELP.md`. Action: third block of `language-bundle` (`cmake.cast:524-533`); export `ling_get_page`; `page.zip` beside `cLang.zip`.

**Step 21: Address.** Scope: `ling/Source/HELP.md`, `saw/cast/files.md`. Action: saw finds a library as SPEC.md:840 states.

**Step 22: Languages.** Scope: `jam/cast/syntax_tree.md`, new `ling/language/tokens/{spell.md, tokens.cast}`. Action: one row set per language; one token library; one entry per language and one list entry. Validation: the row count equals the 183 names of SPEC.md:843.

### Stage 5 — GitDocument (done)

**Step 23: Read.** COUNSELOR read `GitIndex.cpp`, `GitObject.cpp`, `GitPack.cpp` and the tit plan `:140-230`.

**Step 24: `jam_git`.** Scope: new module `jam_git` (`document/jam_GitDocument.{h,cpp}`, `jam_GitValidator.{h,cpp}`); `saw/Source/Git*.{h,cpp}` deleted. Action: `GitDocument::parse` of the index bytes; blobs from loose objects and packs; `juce::ByteOrder`; `juce::GZIPDecompressorInputStream`.

**Step 25: Changes.** Scope: `saw/Source/Page.cpp`. Action: `LineDiff::applyToChanges` gives `map::LineChange` rows of `cast/core.md`.

### Stage 6 — saw is cat (done)

**Step 26: Loader in the page verb.** Scope: `saw/Source/Page.{h,cpp}`, `project-info.md`. Action: rendered output on the CLI only, when a decoration, colour, wrap or paging option applies (SPEC.md:839); `PageLibrary` nullptr gives the plain lane; MCP has no loader call; `cast/signing.md` row `com.apple.security.cs.disable-library-validation`.

**Step 27: Text to text.** Scope: `saw/Source/NonPrintable.{h,cpp}`, `Page.cpp`, `jam_terminal.h:81-94`, `jam_terminal.cpp`. Action: UTF-16 through `juce::String::createStringFromData`; `--strip-ansi` through `AnsiSequence::getToken`; `-A` and `--sanitize` through `jam::simd::findFingerprint`, `jam::Utf8::setCodepoints`, `jam::Format`.

**Step 28: Options.** Scope: `PageOptions`, `PageConfiguration`, `PageRanges`, help. Action: every option of `Source/help/page.md` maps to the plain lane, to a page entry, or to a style sheet; `--theme <name>` is a style sheet (SPEC.md:842); `--list-languages` reads the list entry of Step 22.

### Stage 7 — JSON: one job, one function (done; superseded in part by Ruling 25)

**Step 29: Read.** COUNSELOR read each pair of the JSON inventory at file:line.

**Step 30: One owner for each job.** Scope: the JSON files of that time and `saw/Source/Json*.cpp`. Action: one function per job; `std::map` tables become `LookupTable` or `HashMap`; byte loops become `jam::simd`. The files that held `JsonValue`, `JsonProgram`, `JsonAutomaton`, `JsonBuiltins`, `JsonMatches`, `JsonDates`, `JsonMath` and `JsonNumber` are deleted (Step 44b).

**Step 31: Platform calls.** Done on files that Step 44b deleted. Step 44h carries the calendar rule: calendar arithmetic through `juce::Time` where a read proves equal results.

**Step 32: Strings of the first plan.** Scope: every file that the first plan added or changed in saw and JAM. Action: the String rule replaces each *Not used* item.

**Step 33: SPEC and help.** `SPEC.md` page lane, §5.10, §8; `Source/help/page.md`, `Source/HELP.md`, ling `HELP.md`.

**Step 34: Cases and benchmark rows.** `test/cases.md` `page` rows; `matrix-page.md`, `matrix-json.md`.

### Stage 7B — The jq program is a registered JsonDocument family (done)

**Step 35: Plan file and RFC.** Done. `RFC-one-tool.md` is deleted (ARCHITECT: *"delete the RFC"*).

**Step 36: SPEC §5.9.** Done. Scope: `SPEC.md:801-821`, `Source/help/json.md`. `saw json` takes the arguments and the filter language of `jq` 1.8.2 (Ruling 23). Its messages are saw's text rows. `--run-tests`: a program of a failure case passes when the compile fails; the message text is not compared.

**Step 37: Read before the edit.** Done.

**Step 38: Rows.** Done. Scope: `jam/cast/core.md`, `jam/cast/spell.md`. `JsonFilterLexeme`, `JsonFilterOperator`, `JsonFilterPrecedence`, `JsonFilterByte`, `JsonFilterAssociation`, `jsonFilter`, `jsonFilterPrecedence`, `jsonFilterAssociation`, `jsonFilterConstruct`; spell rows in the form of `spell.md:1336-1391`. `JsonFilterType` holds `parameter` and `absent` (`core.md:285-286`).

**Step 39: Delete first.** Done. `jam_JsonFilter.{h,cpp}`, `jam_JsonFilterValidator.{h,cpp}` and their rows are deleted.

**Step 40: The jq family of `JsonDocument`.** Done. `readFilter (std::string_view text)` (`jam_JsonDocument.cpp:667-680`): the document of the jq family (`isFilter`), its line starts (`addLineOffsets (0)`), `Document::parse`, then `JsonValidator {}.isValid` once; the pair returns the document only when the result is ok. The scan (`scanFilter`, `:906-1233`) and the build (`parseTopLevel` to `parseObjectPatternMember`, `:1356-2267`; `addFilterTree`, `:2269-2277`) read the Step 38 tables. A failure is `setFailure` with a `text::English` row (`failAt`, `:1350-1354`). No throw, no catch.

**Step 41: Rules.** Done. The build links each reference to its binding in one walk (`addLinks`, `jam_JsonDocument.cpp:2365-2543`): the reference lists the binding under `Id::to`, the binding lists the reference under `Id::in` (`addLinkEndpoint`, `jam_Document.cpp:282-292`). `JsonValidator::addRules (Rules&, externalNames)` adds `Id::link` and `Id::object` (`jam_JsonValidator.cpp:16-79`); `addModuleRules` adds `Id::body` (`:81-103`). Each failure text holds the line and the column (`getLineNumber`, `getColumnNumber`). The builtin test of `Id::link` (`:35`, `JsonBuiltins::find`) names a deleted type; Step 44f corrects it.

**Step 42: The consumer trusts the tree.** Done on `JsonProgram`, which Step 44b deleted. Each function parameter is a `parameter` element (`map::JsonFilterType::parameter`): children 0 to n-2 of its definition hold the parameters, and the last child is the body (`addDefinitionLinks`, `jam_JsonDocument.cpp:2394-2414`).

**Step 43: One result, one parse.** Done. A module file is read once: `JsonDocument::getOrCreate (name, getText)` (`jam_JsonDocument.cpp:682-692`), keyed by the full path. saw `getCompileFailure` takes the `juce::Result` (`Json.cpp:56-61`).

**Step 44 (second update): Literal constants.** Void. Ruling 25 deleted `JsonProgram`, which it changed. Step 44g reads a filter literal from the filter element.

### Stage 7C — JsonDocument is the complete AST; nothing else holds a JSON value

ARCHITECT, verbatim: Rulings 19, 25 and 26. Also: *"just because existing does not mean it correct. why dedicated struct? what is JsonValue? a document?"*, *"second value Model? why we need second value model? … why not SSOT?"*, *"why there is jq tree? why there is always struct need to be created? why is JsonDocument not the SSOT, complete AST"*, *"what scalars? we dont parse vectorised? with simd?"*.

Contract (MANIFESTO S; MANIFESTO E, Complete at Creation, `MANIFESTO.md:248-288`; CODING.md "No fake-carrier structs", "No non-owning naked pointers as members"):
- `JsonDocument` builds the complete AST, in the Document pattern (Ruling 27). The build makes each fact once: the type (the element id), the span, the decoded text, the number, the boolean, the member key, and the member order.
- A value is that element, read in place (`const Document::Element&`). A value passed along is transit: no copy, no wrapper, no index carrier, no cache.
- A computed value is a creation: a new element of the run document (a value-stream `JsonDocument`).
- The filter is a `JsonDocument` of the jq family, complete and linked (Step 41). The run reads that tree in place. No second compiled form exists.
- Feature parity is the user surface only (Ruling 23). jq's structure is never a reference (Ruling 26).

**Step 44a: Read.** Done this session. The reads are the (7C) rows of the inventory: `Document` element, token, link, range, validator and part API; the JSON build (`jam_JsonDocument.cpp:427-901`); the readers (`:89-157`); the value stream (`:650-760`); the filter construct helpers, tables, tree and links (`:1235-1360`, `:1604-1722`, `:2268-2545`); `JsonValidator` whole; `JsonWriter.h` whole and `JsonWriter.cpp:1-60`; `JsonLanguageDocument.cpp` whole; the `getChildByToken` references (66, ling `findReferences`); saw `Json.h`, `Json.cpp`, `JsonInputs.h`, `JsonOutput.h`, `JsonRunner.h`, `JsonRunner.cpp`, `JsonVariables.h`, `JsonTests.h`, `JsonTestReport.h`, `JsonOptions.h`; the `core.md` ids.

**Step 44b: Delete first.** Done (Ruling 25). `rm` of `jam_core/json/jam_Json{Value,Program,Automaton,Builtins,Matches,Dates,Math,Number}.{h,cpp}`; their rows of `user-modules-info.md`; the class `JsonBoundary` (`jam_JsonDocument.{h,cpp}`).

**Step 44c: Rows.** Scope: `jam/cast/core.md`, `jam/cast/spell.md`. Action:
- `## identifiers`: `@id` rows `boolean` (`boolean`) and `error` (`error`), in the form of `core.md:35-37`.
- `## english`: one row for each text of `jam_JsonDocument.h:416-429` (`failNumeric` to `failEscape`) and for the end, line and column words of `getValueResult` (`jam_JsonDocument.cpp:746-748`). Each row states saw's message in saw's words (Ruling 23), as `failUnclosed`, `failRootNotContainer` (`jam_JsonDocument.cpp:636`, `:640`). A row for each run failure of Step 44g (a type error, an index error, a path error, a break with no label).
- `## JsonFilterBuiltin` (`name | key | value`): one row for each builtin that the run computes in code (Step 44g). `value` is its function key `name/arity` (`getFunctionKey`, `jam_JsonDocument.cpp:2340-2347`). Sibling: `mermaid.md:1900-2012`.
- `## JsonFilterDefinition` (`name | key | value`, the Bimap form of `JsonFilterBuiltin`): one row for each builtin that the filter language defines, in the order of the definitions (the scope walk binds a definition after the ones before it); `value` is the `def` text, `|` written `\|` (`core.md:395`). The definitions come from the jq manual (docs are permitted; `jq/src` is not) and each matches the manual examples and the installed jq witness. A name that is a C++ keyword or a Windows SDK macro name takes a form that is not (`logicalNot`, `logicalWhile`, `minimum`, `maximum`).
- spell rows in the form of `spell.md:1336-1391`.
- Validation: no row holds a jq message text; each builtin of the jq 1.8.2 manual is in exactly one of the two tables.

**Step 44d: The complete AST.** Void (Ruling 27, Stage 7D): its element per value is COUNSELOR's inference, not ARCHITECT's words. Scope: `jam_JsonDocument.{h,cpp}`. Action:
- Each value gets an element at creation. `addContainer` (`:427-451`), `addString` (`:472-497`), `addNumber` (`:499-519`) and `addLiteral` (`:521-545`) call `addChild` on the open container element, or on the root (`:431-437`). The element id is a lookup by `map::JsonTokenType`: `Id::object`, `Id::array`, `Id::string`, `Id::number`, `Id::boolean`, `Id::null` (form `getToken` tables, `:579-610`).
- The element facts, each made once: `Id::range` (`addRange`, `jam_Document.cpp:871-874`) over the value bytes, set for a container in `closeContainer` (`:788-803`); `Id::text` on that range token when the string holds an escape (the decode of `:485-491`, moved from the pending token); `Id::number` (`double`, `Format::getNumber`) on a number; `Id::value` (`bool`) on a boolean.
- A member key is a JSON string, so it is an element (correction of the first form, which put the key as a token on the value: that left the key the one string with no element, and a JSON Schema `propertyNames` instance, `jam_JsonSchema.cpp:518-535`, had no element to evaluate; CAROL.md Instruction Hierarchy, CONTRACT over PROJECT DECISIONS). An object's children are its key elements (`Id::string`, range over the quoted key, `Id::text` when escaped), in member order; each key element has one child, the member's value: the `juce::NamedValue` pair, name with its value (`juce_Variant.h:173-178`, `getObjectElements`). `addString` holds a key token in `pending` until its value arrives; then the key element is made from that token and the value element is made under it. `getName (key)` is the decoded key; `getProperty` returns the key's value.
- The container `Id::tokens` property goes: the element tree holds every value, so a second list of the same spans is shadow state (MANIFESTO S). `addMemberIndex` reads the keys from the child elements; `Id::index` holds the members (`Document::Elements`) sorted by key, the last of a repeated key kept, so a lookup is a binary search.
- The link walk state goes from the members: `bindings` and `scopeMarks` (`jam_JsonDocument.h:680-681`) become locals of `addFilterTree` (`:2269-2277`), passed by reference to the walk (CODING.md "No non-owning naked pointers as members").
- Readers take elements and the JUCE `juce::var` names (inventory table 3): `getValue()` returns the root's first child; `getValues()` returns the root (its children are the values); `getProperty (object, name)` returns `const Document::Element&` and `hasProperty` tests it (binary search on `Id::index`, else the scan of the children, the last value of a repeated key wins); `getName (member)` gives the decoded key; `isObject`, `isArray`, `isString`, `isDouble`, `isBool`, `isVoid` test the id; `getString`, `getBody`, `getDecodedBody`, `getBoolean`, `getNumber` take `const Document::Element&`. A number's value is made by `getNumber (std::string_view literal)` through `Format::getDecimalDouble` (`jam_Format.h:581`), bounded by the literal. `getChildByToken` goes.
- The value-stream messages are the rows of Step 44c. `getValueResult` uses the form of `getRuleResult` (`:139-151`) with `firstLine` and `firstColumn`.
- `build` (`:623-648`): the root rule reads the root's first child. The JSON rules keep their keys.
- `read (sourceText, stored)` (`jam_JsonDocument.h:45`): the stored image holds the same elements (`SwapWriter`, `SwapReader`). COUNSELOR reads its callers before the brief (blast radius).
- Validation: `getChildByToken` has no reference; no container holds `Id::tokens`; each value of a parsed document is an element with its facts; no member holds an `Element*`.

**Step 44e: Readers of JSON trust the tree.** Void (Ruling 27, Stage 7D): its element per value is COUNSELOR's inference, not ARCHITECT's words. Scope: the 66 `getChildByToken` references and every caller of the Step 44d readers: `jam_JsonWriter.{h,cpp}`, `jam_JsonLanguageDocument.{h,cpp}`, `jam_JsonLanguageValidator.{h,cpp}`, `jam_model_context_protocol/schema/jam_JsonPointer.cpp`, `jam_JsonSchema.cpp`, `server/jam_ModelContextServer.{h,cpp}`, saw `Source/Patch.cpp`, `PatchValidator.cpp`, `ToolServer.cpp`, `Undo.cpp`, `UndoCache.cpp`, and each caller in the consumer projects (blast radius). COUNSELOR writes the caller list with ling `findReferences` in the main session before the briefs. Action:
- Each caller reads elements. A test for nullptr on a validated tree goes (Ruling 19). A shape that a caller assumed (a member that must exist, a type that must hold) is a rule of the caller's validator (`PatchValidator`, the schema validation of `JsonSchema`, the request rules of the server), in the form of `jam_JsonValidator.cpp:52-78`. The caller then reads the member with no test.
- `JsonWriter`: one walk over elements. The dispatch keys on the element id (`Function::Map` form, `jam_JsonWriter.cpp:31-46`). `appendDocument` renders the root's first child; `appendValue (json, document, const Document::Element&)` replaces the token form (`jam_JsonWriter.h:207`). A parsed number writes its span; a computed number (no span) writes `Format::appendNumber`. The `Layout` struct (`jam_JsonWriter.h:74-83`) goes (CODING.md "No fake-carrier structs"): the writer keeps `formatOptions` (`:630`) and takes the tab, sorted-key and colour settings of the jq surface as constructor arguments stored as `const` members beside it. The `Layout` templates (`:91-177`) become the one walk's steps. The `Number` template parameter (`:165-171`) goes.
- `JsonLanguageDocument`: `addMemberKeys` moves each member value element under its key element (no container test, `jam_JsonLanguageDocument.cpp:81-86`); `addFlatTokens` walks the elements and adds each range token and key token; `setElementIndices` and `setContainerIndices` read the elements.
- Validation: no `!= nullptr` or `== nullptr` test on an element of a validated JSON document remains in the scope; each public signature change is named here and has no caller left on the old form.

**Step 44f: Rules of the filter.** Scope: `jam_JsonValidator.{h,cpp}`, `jam_JsonDocument.{h,cpp}`. Action:
- `Id::link` (`jam_JsonValidator.cpp:18-50`): a `call` with no `Id::to` is defined when its function key is a `JsonFilterBuiltin` value, a `JsonFilterDefinition` name, or in `externalNames`. The `JsonBuiltins::find` test (`:35-36`) goes.
- The builtin definitions are one memoized filter document: `getOrCreate` (`jam_JsonDocument.cpp:682-692`) over the `JsonFilterDefinition` rows, built once. A reference to one of its definitions in another filter holds `Id::to` only. The builtin document is not changed after its creation (Complete at Creation).
- Imports: `readFilter` takes the search paths of `-L` and the `externalNames`, loads each `import` and `include` module through `getOrCreate` keyed by its full path, and links each `module::name` reference to the definition element with `Id::to` only. A module that is absent or fails its rules makes the failed result of `readFilter`. The rule set runs once (`Document::Validator::isValid (rules, document)`, `jam_Document.cpp:172-188`).
- Validation: no reference to a deleted type; one rule set per filter document; each failure text is a row.

**Step 44g: The run.** Scope: `jam_JsonDocument.{h,cpp}` (inside `#if JAM_USING_JSON_FILTER`). Action:
- The run document is a value-stream `JsonDocument` (`getValueStream`, `jam_JsonDocument.cpp:711-720`). It creates each computed value: `addValue (double)`, `addValue (std::string_view)` (`:694-708`) return the new `Element&` under the root, with the facts of Step 44d; overloads add a boolean, a null, an array and an object. A child of a computed container is either a new element, or an element that lists an existing value under `Id::to` (`addLinkEndpoint`, one direction). The existing value does not change. A reader of a child reads the `Id::to` target when present (one reader, in `JsonDocument`).
- The owner of a value (read correction): an element does not name its document, and a string's text is a span of its owner's source. A run reads values of several documents (the input, the variables, the filter literals, the definitions document, the run document). The run holds them in one local list of `JsonDocument*`, passed by reference as the link walk's `Bindings` is; `getDocument (value, documents)` walks `parent` to the root and returns the document whose `getRoot()` it is (the registry form of MANIFESTO E: identity through the registry's own API). `compare (documents, left, right)` orders values as the jq manual states (`sort`). The creators are `addValue (parent, …)` for each value type, `addValue (parent, existing)` for a link, and `addKey (object, name)` for a member key (Step 44g part 1).
- `juce::Result applyToResults (const JsonDocument& filter, const Document::Element& input, const std::function<bool (const Document::Element&)>& function)`: runs the filter root over `input` and calls `function` with each result element. A `false` return stops the run (the prune form of `applyFunctionRecursively`, `jam_Document.h:383-396`). The run inputs (`input`, `inputs`, `debug`, `stderr`, `input_filename`, `input_line_number`), and the values of `$ENV`, `$ARGS` and each `--arg` name, are arguments of the entry: `std::function` arguments in the form of `getOrCreate (name, getText)` (`jam_JsonDocument.h:54`), and a variables `JsonDocument` whose root children hold the named values with `Id::nameTokens`. JAM knows nothing of saw (MANIFESTO E, Encapsulation).
- `getResults()`: a `static constexpr LookupTable` of member function pointers keyed by `map::JsonFilterType`, in the form of `getLinks` (`:1634-1682`). Each entry takes the filter element, the input element, the binding values and the callable, and returns `juce::Result`.
- Bindings: a reference reads its binding through `Id::to` (`addReference`, `:2515-2528`). The value of a binding in this run is transient state of the creation (`MANIFESTO.md:288`): a local `jam::Array` of the entry, passed by reference through the calls, that maps a binding element to its value element, or to its argument element and the binding depth of the call (a function parameter is a filter of the caller). No member holds it.
- A raised error is a failed `juce::Result`. Its value is an element of the run document under `Id::error` (`getChildByID`, `jam_Document.h:275-286`); `try` and `catch` read it. `break` is a failed result whose error element lists its `label` element under `Id::to` (`addBreakLink`, `:2509-2513`); its own `label` returns ok.
- Paths (`path`, `paths`, `getpath`, `setpath`, `delpaths`, `|=`, `=`, the arithmetic updates, `del`, `to_entries` family): the same table runs in path mode, and the callable receives each value with its path, an array element of the run document. A construct that is not a path expression fails with its row in path mode.
- `getBuiltins()`: a table of member function pointers keyed by `JsonFilterBuiltin` (Step 44c). The builtins are grouped by the jq manual: types and `length`; keys, `has`, `in`, `getpath` family; arithmetic and math (`std::` functions); strings (`split`, `join`, `ltrimstr`, `ascii_downcase`, `explode`, `implode`, `@` formats; `tojson` through `JsonWriter`, `fromjson` through `readValues`); regular expressions through the JAM regular expression API (read before its brief); dates (Step 44h); I/O through the entry arguments; sorting and grouping through one compare function in the jq order (null, false, true, number, string, array, object); `limit`, `first`, `until`, `repeat`, `range` through the callable's `false`; `tostream`, `fromstream`, `truncate_stream`. Every other builtin is a `JsonFilterDefinition` row.
- A filter literal (number, string, constant array or object; `isConstantElement`, `:2304-2327`) is read from the filter element: its value is created once in the run document from the filter's span (Step 44d facts), not parsed again.
- Framework replacements (inventory read, 2026-11-26): `@base64` through `juce::Base64::convertToBase64` into a `juce::MemoryOutputStream` (`juce_Base64.h:50`); `@base64d` through `juce::Base64::convertFromBase64` (`:56`) only if the jq witness rejects unpadded input, because JUCE rejects it (`juce_Base64.cpp:91-101`); `@urid` through `Format::setPercentDecoding` (`jam_Format.h:930`, `.cpp:1173-1191`); the `@csv` and `@sh` field quotes through `Format::appendQuoted` (`jam_Format.h:736`); `implode`'s codepoint check through `juce::CharacterFunctions::isNonSurrogateCodePoint` (`juce_CharacterFunctions.h:211`); string division (`addStringQuotient`) is `split/1` (`addSplitPieces`), and `addCharacters` goes (DRY with `addCodepointPieces`). The one-sided Unicode trims stay: `Format::trim` is ASCII and two-sided (`jam_Format.cpp:613-629`). One base64 decoder (ARCHITECT: *"move the decoder to core and make it stateless without breaking consumer"*): `base64Values`, `invalidSextet`, `bitsPerSextet`, `bitsPerByte` and `forEachSextet` move from `TerminalITerm2Decoder` (`jam_TerminalITerm2Decoder.h:196-230`, `:259-272`) to `jam::Format` as `applyToSextets (std::string_view, Function&&)` and `applyToBase64Bytes (std::string_view, std::pair<juce::uint32, int> carry, Function&&)`, which returns the new carry. The decoder keeps `setPayload` and `getPayloadSize` with the same signatures and owns its carry. `@base64d` validates through `base64Values` and decodes through `applyToBase64Bytes`; `getBase64dText`'s own decoder goes.
- Validation: no struct; no member that holds a value or an element; every `switch` or `if` chain over a construct or a builtin is a table; no copy of an input value (a computed container links it); one entry function.

**Step 44h: Dates.** Scope: `jam_JsonDocument.cpp` (the date builtins of Step 44g). Action: `now`, `mktime`, `gmtime`, `localtime`, `strftime`, `strptime`, `todate`, `fromdate`, `dateadd`, `datesub`, `date` through `juce::Time` where a read of `juce_Time.h` proves equal results; the platform calls (`strftime`, `strptime`, `timegm`) only where `juce::Time` has no API. Validation: the date rows of the jq test files pass (Step 46).

**Step 44i: The saw lane on JsonDocument.** Ruling 25 governs. Kept: the verb `Json.{h,cpp}` (saw's verb form, `Page.h:7-12`, `Fields.h:9-30`) and `JsonOptions.{h,cpp}` (saw's option form, `PageOptions.{h,cpp}`). Deleted: `JsonInputs.{h,cpp}`, `JsonOutput.{h,cpp}`, `JsonRunner.{h,cpp}`, `JsonVariables.{h,cpp}`, `JsonTests.{h,cpp}`, `JsonTestReport.{h,cpp}`. No type replaces them. Action:
- Delete first: @Engineer reads the six pairs only for the user surface (options, separators, exit codes, report lines), then `rm` them.
- `Json.cpp` is the seam, in static functions of the verb: `JsonOptions` → `JsonDocument::readFilter (text, names, getModuleFile)` → `applyToResults` for each input value → `JsonWriter::appendValue` into the output. A failed `readFilter` result is the compile failure (`JsonOptions::exitCompile`).
- Inputs: each source is a `JsonDocument` from `readValues`; each root child is a value, read in place. A raw line, a slurp and the `null` of `-n` are values that a value-stream document creates (`addValue`). `--stream` reads with `isFinal` false and `getConsumedLength`; each value runs through the `tostream` filter (`readFilter`), and each event is an input of the filter.
- Variables: one `JsonDocument`. Each `--arg`, `--argjson`, `--slurpfile`, `--rawfile`, `--args`, `--jsonargs`, `$ENV` and `$ARGS` value is a member made with `addKey` and `addValue`. A parsed value is linked, not copied.
- The run: one run document for each input; the registry span holds the input, variables and run documents; the `Input`, `Message`, `FileName` and `Emit` callables are lambdas of the verb.
- Module files: `getModuleFile` returns the first file that exists, over the `-L` directories (`JsonOptions::libraryPaths`): `<dir>/<relativePath><extension>`, then `<dir>/<relativePath>/<last component><extension>` (the folder form of `import`, case `json-mod-folder`). JAM holds no search.
- `--run-tests`: the same calls. Each output is compared with the expected value by `JsonDocument::compare`. A failure case passes when `readFilter` fails (Step 36). Each report line is a text row.
- Validation: the json lane holds no saw type other than `Json` and `JsonOptions`; no struct; no class member holds a value or an element; no jq structure (Rulings 23, 26); no bail-out guard.

**Step 44j: Cases.** Scope: `test/cases.md` `json` rows, `benchmark/matrix-json.md`. Action: each row that expects a jq message text expects saw's row text (Ruling 23). Validation: the rows read their texts from `cast/core.md` `## english`.

Validation of Stage 7C: no type other than `Document`, its parts and the five kept JSON types holds a JSON value or a filter; no carrier struct; no cache; the consumers of `JsonDocument` keep their behaviour (blast-radius check); the 949 programs and the 7 jq test files give the same output values (Step 46).

### Stage 7D — JsonDocument follows the Document pattern (Ruling 27)

ARCHITECT, verbatim (Ruling 27): *"Rules, Vocabulary and Token are Document contract"*, *"we have so many OOTB language parser all using the exact consistent pattern adherence to /contract … why now json change must have its own pattern and break all consumer?"*, *"what's wrong with just implement verbatim per codebase established pattern?"*

The sibling parsers keep their scanned token list on the root: `jam_Css.cpp:206`, `jam_Xml.cpp:320`, `jam_MermaidDiagram.cpp:160`, `jam_MarkdownDocument.cpp:396`. JSON at JAM HEAD had the same form: the scan reads the Vocabulary (`getVocabulary`, HEAD `jam_JsonDocument.cpp:247-250`); its token constructions take `(const int& type, JsonDocument& document, Cursor& cursor)` (`skipGap`, `addContainer`, HEAD `:275-297`); the root and each container hold their `Id::tokens` list (HEAD `:255`, `:271-272`, `:292`); and its public readers are `getValue`, `get`, `contains`, `getChildByToken`, `getString`, `getBoolean` (HEAD `jam_JsonDocument.h:184-246`). The consumers call these readers (whatdbg `Session.cpp:253`, `:389`; ling HEAD `Relay.cpp:167-173`).

**Step 44k: Read.** Scope: none (read, by @Pathfinder with exact targets, outputs to the scratchpad `rt/` folder). Targets: the Document base parse API (`jam_Document.h`: Vocabulary, `getToken`, the token construction table, Cursor, Validator); the sibling builds `jam_Css.cpp` (around `:206`), `jam_Xml.cpp` (around `:320`), `jam_MermaidDiagram.cpp:124-163`; JAM HEAD `jam_JsonDocument.{h,cpp}` whole (`rt/head-JsonDocument.{h,cpp}`); the working-tree JSON parse and every reader of the element layout in JAM and saw (the scope of Step 44e); each consumer working-tree diff (`rt/<project>-json-hits.txt`). Action: COUNSELOR writes the briefs of Steps 44l to 44n from the read, with sibling lines pasted.

**Step 44l: The parse.** Scope: `jam_JsonDocument.{h,cpp}`. Action: `JsonDocument` scans through its Vocabulary and its token constructions, in the form of the siblings and of HEAD; the root and each container hold their token list; containers and keys are elements. The public readers that the consumers call keep their HEAD signatures. Validation: no consumer source changes; each consumer reader call compiles unchanged.

**Step 44m: The JAM and saw readers.** Scope: the JAM and saw files of Step 44e. Action: each caller reads the token tree through the readers of Step 44l. Validation: no element-per-value reader remains.

**Step 44n: Consumers.** Scope: read only, then each consumer line that this sprint changed. Action: COUNSELOR reads each consumer working-tree diff; a line that an agent of this sprint changed returns to its own text; ARCHITECT's work in progress is not touched. Validation: each consumer builds with its own spell.

**Step 44o: The run reads the token tree.** Scope: `jam_JsonDocument.{h,cpp}` inside `#if JAM_USING_JSON_FILTER`, and the callers of the run in saw `Source/Json.cpp`. ARCHITECT, verbatim (Ruling 27): *"what's wrong with just implement verbatim per codebase established pattern?"*, *"compareNumbers << foreign semantics"*. Correction of Step 44g: under Ruling 27 a parsed scalar is a token with no element (`jam_JsonDocument.cpp:535-561`), and a link endpoint is an element (`jam_Document.h:1641`). So a link to a parsed scalar cannot name the scalar itself. Action:
- A run value is the pair of a container element and a token in its `Id::tokens` list. This is the HEAD form: `JsonPointer::get` returns `std::pair<const Element*, const Token*>`, and `JsonWriter::appendValue (json, document, token, container)` takes the same pair. Each run function takes `Element& container, const Document::Token& value` in place of `Element& value`. The `Emit` callable takes the same pair. A container value gets its element from `getChildByToken (container, value)` (`cpp:154-165`).
- The run document has the parse layout (`cpp:535-588`). Its root and each container hold `Id::tokens`. An object holds key, value, key, value, and each member has an `Id::key` element.
- A new scalar is a creation. It is a token in its container list, and it holds its value as a property: `Id::number` (`double`), `Id::text` (`std::string`), `Id::value` (`bool`). This is the form of `cpp:850-872`, moved from an element to the token (`Token::Value`, `jam_Document.h:96-103`).
- A computed value that no computed container holds goes under its own `Id::value` element, a child of the run root that holds `Id::tokens` with that one token. The run root list grows with each value, and `jam::Array` moves its tokens when it grows, so a reference into it does not stay valid. An `Id::value` element never grows, and an element does not move (`jam::Owner<Element>`, `jam_Document.h:1613`). A computed container is complete before the run reads its items (MANIFESTO.md:250), so the references into its list stay valid.
- An existing value in a computed container is a link. This is the form that the Document base uses to name a token: an element and a token index (`nameTokens`, `jam_Document.h:1649`, `jam_Document.cpp:392-394`, `:413`). The item token has the type of the target and holds the target token index under `Id::to` (`int`). The item slot holds an `Id::to` element that lists the owner container (`addLinkEndpoint`, `jam_Document.h:1641`). The value stays in its owner, and the link is a handle (MANIFESTO.md:280). `getChildByToken` counts a token that holds `Id::to` as it counts a container token. One reader, `getValue (container, value)`, follows the link and returns the pair of the owner container and the target token.
- `getDocument (container, documents)` finds the owner document from the root of the container (`cpp:902-914`). Each byte read of a value goes through its owner document.
- A value that a run function takes or emits is never a link. Each read of an item out of a container goes through `getValue (container, item)`, and the run passes on the pair that it returns. So only that reader knows the link form.
- The run reads a container element through a mutable overload, `Element* getChildByToken (Element& container, const Document::Token& value) const noexcept`. This is the const and mutable pair form of the old `getValue (child)` (`cpp:167-179`). A walk from `firstChild` gives `Element*` (`jam_Document.h:230`), as `findModuleDefinition` does from a const reader (`jam_JsonDocument.h:716`). So a link endpoint (`addLinkEndpoint (…, Element& linked)`, `jam_Document.h:1641`) needs no cast.
- The key order of an object is a creation fact. Each object element holds `Id::index` (`Integers`, `jam_Document.h:188`): the token positions of its keys, sorted by the decoded key. The parse sets it when it closes the object (`closeContainer`). The run sets it when it completes an object that it creates. `compareObjects`, `keys`, the sort builtins and the sorted-key output of `JsonWriter` read `Id::index`. A sort or a key list built while a value is read is a temporary container in transit (MANIFESTO.md:263, :267), so none remains in `compareObjects` (`cpp:998-1009`) or in `JsonWriter` (`jam_JsonWriter.cpp:124`).
- The run's failure element is `Id::failure`, a row of `jam/cast/core.md` (the `error` row at `core.md:93` becomes `failure`). Only the run used `Id::error` (16 sites in `jam_JsonDocument.cpp`), and whatdbg declares its own `Id::error` (`whatdbg Generated.cpp:48`), so the JAM row made a duplicate symbol at link time. No project under `dev/` declares `failure`. ARCHITECT, verbatim: *"DONT BREAK OTHER PROJECT, means if genuinely no need to change consumer contract, only internal correction, DONT FUCKING TOUCH OTHER PROJECT."*
- `fromjson`: the parse is the creation. `readValues` parses the text, and it borrows the text (`jam_JsonDocument.cpp:792-798`). The run document adopts that parse under a new `Id::value` holder through the Document base join API `appendChildren (Element& parent, Document&& child)` (`jam_Document.cpp:714-755`). That API adds the source bytes once, moves the root children and the root tokens, and adopts the elements. A rebuild of the parsed value in the run document would be "a second copy of the same complete data" (MANIFESTO.md:282).
- Validation: no run function takes a value as `Element&`; no copy of an input value; no struct; one reader follows `Id::to`; no index outside the creators and that reader.

### Stage 8 — One build, profile

**Step 45: The one build.** ling, then saw, `--no-sign`, through @Pathfinder. All findings are one list, fixed wholesale from the code.

**Step 46: Parity and speed.** Help frames of the consumers' trees equal (rendered by the built HelpFrames of saw and ling). Suite on both lanes; each failing case is fixed from the code (known before this build: `find-direct-binary` — SPEC.md:18 governs, so the message is SPEC.md:397 `saw: binary file: <path>` and the `readAsText` suffix (`cast/text.md:26`) leaves that message; `patch-bad-json`, `write-verb-hard-link`, `write-undo-created`, `write-undo-overwrite`, `lines-create-absent`, `page-squeeze-limit`; the MCP lane stops at `test/run.lua:453`). The 949 jq programs and the 7 jq test files (`saw json --run-tests`). `otool -L` of saw: no GUI framework. `matrix-*` rows: no row of `find`, `replace`, `lines`, `fields`, `patch`, `excerpt`, `json` or `--mcp` is slower than before the change; `page` plain against `cat`, page lane against `bat`. A lost row: read the cause, change it, time it with its reference. The element per value of Step 44d is measured on the `--mcp` and `json` rows; an index read of a large array is measured on the `json` rows.

**Step 46 progress (2026-11-27, build fix19, `scratchpad/build-saw-fix19.txt` exit 0):**
- Done: catch body reads the resolved failure value (`jam_JsonDocument.cpp` `addAttemptResults`, `addAttemptPaths`, `getValue`). Main thread stack 256 MiB (`project-info.md` row `mainThreadStackSize`; LC_MAIN `stacksize 268435456`); jq.test runs whole with no crash. `getModuleFile` reaches every run function (`jam_JsonDocument.h:73`, `:774`; `Json.cpp` `runFilter`, `runCase`). Six ruled cases are baseline rows (`test/cases.md:562`, `:680`, `:682`, `:698`, `:870`, `:898`; `test/baseline/<case>/1/stdout`). `--seq` input per RFC 7464 (`Json.cpp` `getRecordFailure`, `addRecordDocument`, `addRecordDocuments`; rows `jsonRecordSkipped`, `jsonRecordTruncated`, `jsonRecordOutside` in `cast/text.md`). `@urid` requires valid UTF-8 (`jam_JsonDocument.cpp:6204`). `have_decnum` is true (`:5195`).
- Results: suite CLI 2 FAIL (`json-read-duplicate`, `json-stream-errors`); MCP lane 3 FAIL (adds `page-list-languages`). jq.test 483 of 607 (15 malformed). man 233/240, uri 14/20, base64 8/10, onig 50/50, manonig 19/19, optional 3/3.
- Open: `json-stream-errors` (`--stream-errors` is only an alias, `JsonOptions.cpp:80`); `json-read-duplicate` (ARCHITECT: "a" — last key wins in `setKeyOrder`, `jam_JsonDocument.cpp:1146-1160`); `page-list-languages` MCP lane; `modulemeta`, `get_search_list`, `get_prog_origin`, `get_jq_origin` handlers (no `getBuiltins` row); uri L85/L90 still fail after the `@urid` change; man L25 fails after the `have_decnum` change; jq.test 124 failures not classified; help frames; `otool -L`; scout `matrix-*`; 949 programs.

### Stage 9 — Runtime, audit, close

**Step 47: Runtime validation.** ARCHITECT runs the consumers. COUNSELOR fixes each report from the code, in this sprint.

**Step 48: Audit** (@Auditor, once), after Step 47. Findings go through the Design by Contract Filter.

**Step 49: Doxygen pass** (@Pathfinder, "doxygen" in the description) on the changed files of saw, JAM and ling. Each stale block of the JSON files (for example `jam_JsonDocument.h:241-252`, `Json.h:7-11`, `JsonVariables.h:6-9`) states the code of Stage 7C.

**Step 50: Final check.** Build, suite, 949 programs, SPEC against code. The log waits for *"log sprint"*. The JAM commit message holds ARCHITECT's glyph text (scratchpad `jam-commit-from-architect.md`).

## BLESSED Alignment

- **B:** ling owns the library and its path. `PageLibrary` owns the dynamic library. `TerminalReader` owns the display and the window. `SharedDocuments` owns each module document and the builtin document. `JsonInputs` owns the input documents, `JsonVariables` the variables document, `JsonRunner` the run document of one input.
- **L:** languages, colours, metrics, options, jq lexemes, operator tables, builtin names and builtin definitions are rows. Code goes out: about 2,700 lines of saw page code, 10 JAM files, `jam_JsonFilter`, `jam_JsonFilterValidator`, and the eight files of Ruling 25. `jam_JsonDocument.cpp` is 2,647 lines before Stage 7C and holds two families of one object (CODING.md "One object, one translation unit"); Step 48 judges the crossing with the Lean questions.
- **E (Explicit):** every dependency is an argument: the run inputs and variables of `applyToResults`, the walk state of `addFilterTree`. A failure is a `juce::Result`, never a throw.
- **S (SSOT):** one line builder, one code block builder, one renderer, one style consumer, one Git reader, one JSON model (the element tree), one jq grammar (the rows), one scope walk (the build), one rule set, one writer walk, one compare function.
- **S (Stateless):** no cache in saw. The run state lives and dies in one `applyToResults` call. No member holds an element.
- **E (Encapsulation):** JAM does not know ling or saw. The page knows no language. saw knows no GUI. Each reader of JSON trusts its validator.
- **D:** same bytes on both lanes; same paint code on every graphics context; the same parse and validator path for every JAM grammar; the same element tree for every JSON reader.

## Risks / Open Questions

- Two facts need the run of Step 45 and have a defined outcome: `TerminalWindow` has no host before Step 17, so Step 17 is proved at run time; a GUI JUCE copy beside saw's copy is proved at run time. A failure in either is fixed in its step.
- The element per value of Step 44d adds one allocation per scalar at creation. Step 46 measures it on the `--mcp` and `json` rows. A lost row is fixed from the code in Step 46.

## Verification

- `cast cast/spell.md --no-sign` in ling, then `cast cast/spell.md --no-sign` in saw: 0 warnings.
- `nvim --headless -l test/run.lua ~/.local/bin/saw`, and with `mcp`: every row PASS.
- `saw page <file>` on a terminal: header, gutter, colours, change marks, scroll, quit. Move ling's library away: plain bytes, exit 0.
- `saw json --run-tests` on the 7 test files of jq; the 949 programs.
- `scout` on the `matrix-*` manifests.
