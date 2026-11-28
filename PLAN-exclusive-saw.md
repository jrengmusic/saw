# PLAN: exclusive-saw

**RFC:** `RFC-exclusive-saw.md` (Scaffold items 1-12, Handoff rulings 1-11)
**Date:** 2026-11-28
**BLESSED Compliance:** verified against the reads cited below; every question is closed (§9, §10)
**Language Constraints:** C++17 / JUCE 8.0.17 / JAM (`jam_core`, `jam_data_structures`, `jam_git`, `jam_syntax_tree`, `jam_terminal`, `jam_model_context_protocol`; `project-info.md:117-162`)

## 0. Update 2026-11-28 (approved; supersedes the §2 tracker; adds Part J)

ARCHITECT approved this update with `/goplan` "update the plan comprehensively". The sections below §0 stay in force, except the §2 tracker, which §0.2 replaces.

### 0.1 Findings

- Step 0.3 cannot run. The JSON workload hangs in `saw json`: `[range(0; 200000) | {...}]` ran 13 minutes and wrote 0 bytes.
- Timing grid: each doubling of N multiplies the time of an array-collect filter by 3x to 4x; `reduce` stays at 0.00 s. Sampling (`benchmark/exclusive-json-collect-sample.md`): `JsonWriter::appendValue` 1472 of 1482 samples, `JsonDocument::getValue` 1297.
- Cause, by read: `getValue` (`jam_JsonDocument.cpp:183-192`) calls `getChildByToken` for each `Id::to` link token. `getChildByToken` counts the earlier tokens (`std::count_if`, `:161` before J1) and walks `nextSibling` (`:164-165`). One lookup costs O(position), so an array costs O(n²).
- ARCHITECT: *"it's always /contract violations that cause slowness"*. MANIFESTO E, Complete at Creation: *"no downstream consumer … re-derives what that operation already produced."* The creation sites hold the token and its child at the same time (`:609`+`:618`, `:963`+`:965`, `:974`+`:977`) and keep no link.
- 13 Document types read (Document, Json, JsonLanguage, Language, Xml, Markdown, Html, Css, Git, Code, Config, Reflow, Mermaid). Only `JsonDocument` holds a token list per container with child elements in token order and looks up a child from a token. ARCHITECT: *"JsonDocument is an outlier, its pattern is inconsistent with other Document which have O(1) lookup"*.
- J1 (registry index in `Token::elementIndex`) failed and is withdrawn: callers call `getChildByToken` through a document that does not own the element (`:5834` with `:5870`).
- `Token::elementIndex` came in with commit `638031e8d`, one writer, no reader in any repository. ARCHITECT: *"delete dead codes"*. Done (tx `20261127T234857-058571`, `20261127T234900-015fc4`, `20261127T234906-07a2a3`, `20261127T234909-01bbfd`, `20261127T234911-0a8442`, `20261127T234918-026585`). The two `getChildByToken` bodies still name the field; Part J replaces them.

### 0.2 Tracker (replaces §2)

| ID | Step | Depends on | Status |
|---|---|---|---|
| U0 | Insert this update into `PLAN-exclusive-saw.md` | lock | done |
| 0.1 | Apply RFC corrections | lock | done (tx `20261127T222705-0bde25`) |
| 0.2 | Measure the agy message limit | lock | done (64 KiB whole, last line `line-001024`) |
| J1 | `JsonDocument` child list at creation, O(1) `getChildByToken` | U0 | done (with the `parseImport` pointer fix, the `addParsedValue` list move, and `SwapReader::readElement`) |
| J2 | `JsonLanguageDocument` flat tokens keep the child list aligned | J1 | done (`setFlatTokens`) |
| J3 | Build, jq test files, both lanes, timings | J2 | done (0 warnings; jq counts equal the baseline; both lanes pass; 0.09 s, 1.37 s, 5.11 s) |
| 0.3 | scout baseline (five manifests) | J3 | done (medians: find-walk 1.02 s, find-files 0.158 s, replace 1.34 s, page 8.94 ms, json 0.417 s; `benchmark/exclusive-*-before-report.md`) |
| A1, A2, A3 | MCP `status`; `saw fields` ranges; `write --append` | 0.3 | A1 done (0 warnings, both lanes pass); A2 done (0 warnings, both lanes pass, 5 range cases PASS); A3 code done (6 cases `test/cases.md:510-521`; run checks in V1) |
| C6, C1, C2, C3, C4, C5, C7, C8 | 0 rules | 0.3 (C2: C1; C4: C6) | C6 code done (`fromBlock`, parameter `newBlock` per sibling `fromInput (newInput)`); C1 code done (`Entry` variant of `ListedFile`, `NamedFile`, `Input::StandardInput`); C2 code done (path view member; output documents carry no path; `FindOutcome` keyed by `Entry`; Patch groups outlive the transaction); C3 code done (`std::string_view` canonical key); C4 code done (stream into owned block, `Stdio::getDocuments (juce::MemoryBlock&&)`); C5 code done (`getLineNumbers` deleted; changes moved through on equal length); C7 code done (`UndoCache::applyToTransactions` parses each record once); C8 code done (theme `std::unique_ptr<Input>`; config bytes own the unquoted option views) |
| C9 | scout after-measure + report | C1-C8 | run (`benchmark/exclusive-*-after-report.md`; epoch: find-walk −6.8% p 0.62, find-files +0.05% p 0.20, replace +5.4% p 0.0009, page −1.0% p 0.77, json −7.0% p 0.45); replace fixed: `Replacement` carries the one `juce::File` (`Replace.h:36`), epoch +0.94% inside error 1.28% (`benchmark/exclusive-replace-fix-report.md`); replace system +4.2% p 0.0003 unexplained |
| A4 | Page colour branch returns documents | C6, C9 | code done (`Page.cpp:551-563` one block through `MemoryOutputStream (page, false)`, colour branch `Stdio::getDocuments (std::move (page))`; `StandardStream.h` include deleted; tx `20261128T051959-050937`, `20261128T052002-06daf2`) |
| B1-B4 | Output bound | A4 | code done (`noLimitOption`, `summaryOutputLimit`/`linesPrinted`/`omittedToPrintAll`, `outputLimitBytes` `CommandLine.cpp:18`; `Arguments.cpp:221-222`; `TextDocument::fromDocument` `TextDocument.cpp:176-182`; `getKeptLength`/`getLimitedOutput` `CommandLine.cpp:70-112`; help honours `--no-limit` per rulings 6 and 8, `runGlobalOption` accepts it as the one extra argument; line counts by `PageRanges::getLineCount`; cases `output-limit-cut`, `-long-line`, `-off`, `cli-help-no-limit`; run checks in V1) |
| D1-D5 | New capabilities | B4 (D1: C1) | D2 code done (`Count.{h,cpp}`, GNU `wc` layout from `wc.c:756-813`, words by `juce::CharacterFunctions::isWhitespace (char)`, `Id::totalValue`); D3 code done (`Sort.{h,cpp}`, GNU 9.12 `sort.c`/`strnumcmp-in.h` rules, `-u` in place by `std::unique`); D4 code done (`Diff.{h,cpp}`, `LineDiff::appendUnifiedDiff (…, plannedPath, plannedHeaderEnd, …)`, `StagedFile` bytes unchanged, `-q` one read per file); cases and baselines per §10 item 6; D1 code done (`FindFiles.{h,cpp}` with `EntryFilter::create`/`isMatching`; listing slot `Paths::ListedEntry` with `EntryType` and `Metadata`, read only when `isListingEntries`; fd grammar per §10 item 4; `Paths::defaultPaths` one declaration; dangling link reports time 0); D5 code done (`PageFollow.{h,cpp}` JUCEApplicationBase + `File::Watcher::Listener`; `Main.cpp` route `PageFollow::isRequested`; bound moved to `Stdio::getLimitedOutput (Output, size_t, int)` and `Stdio::outputLimitBytes`, shared by `CommandLine` and the follow lane; follow ends only when the bound cuts, listener removed first) |
| E1 | Rendered page crash at the jam owner | — | code done: whatdbg shows the row has `propertyType` `text`; `Xml::getOrCreate` returns nullptr (`jam_Xml.h:21`) because libpage's `BinaryData` held only `style.css`, `page.css` (`page.cast:62`, `jam_Raw.cpp:20-33`); `page.cast` now embeds `${CAST_USER_MODULE_PATH}/resources/svg/*.svg` (form of `end/project-info.md:429`), ARCHITECT: *"go"*; libpage rebuild by ARCHITECT (*"we'll continue after i rebuild ling"*); rendered cases in V1 |
| V1 | Final validation: one build, both lanes, the run checks of every step, the ling libpage rebuild of E1 | A1-E1 | pending |
| F1-F3 | Auditor, doxygen, log | V1 | pending |

Order reason: every 0.3 manifest rebuilds saw (`cast ../cast/spell.md --no-sign`, `benchmark/exclusive-*.md`), so no source edit runs during 0.3. C9 runs before B4, because the bound cuts the measured output and the baseline binary has no `--no-limit`.

### 0.3 Inventory additions (Part J)

| Need | API / pattern | Source | Step |
|---|---|---|---|
| Element-pointer list on an element | `Document::Elements`, an `Element::Value` alternative | `jam_Document.h` (`Elements`, `Value`) | J1, J2 |
| Element property add / read | `Element::add<ValueType>`, `Element::get<ValueType>` | `jam_Document.h:262-500` | J1, J2 |
| Element address stable after adoption | `appendChild` *"relocates ownership only, never the Element's address"* | `jam_Document.h` (`appendChild` doc) | J1 |
| Child creation / removal | `addChild`, `removeChild` | `jam_Document.cpp:657-695` | J1 |
| Token position in its list | `&value - tokens.begin()` | `jam_JsonDocument.cpp:161` (object path) | J1 |
| `Elements` serialized | `SwapWriter::writePayload (const Elements&)`, `SwapReader::readPayload` | `jam_Document.cpp:1288-1291`, `:1130-1136` | J1 |
| Pattern: `Elements` index on an element, built at creation, read directly | `addDefinitionIndex` / `findDefinition` | `jam_JsonDocument.cpp:3176-3205` | J1 |
| Pattern: element link as an `Elements` list | `Document::addLinkEndpoint` | `jam_Document.cpp:302-314` | J1 |
| Pattern: element index from the registry loop | `getFlatTokens` | `jam_JsonLanguageDocument.cpp:63-86` | J2 |
| Name: plural key of an `Elements` list | `images` (`jam/cast/core.md:126`), `index` (`:129`) | → `children` (`@id`, "Child elements of a JSON container, one for each token or member.") | J1 |
| Name: `set*` writes onto a target | `setKeyOrder` (`jam_JsonDocument.cpp:1303`), `setKeyRangeEnds` (`jam_JsonLanguageDocument.cpp:56`) | → `setFlatTokens` | J2 |
| Absence: base member token → element | read of `jam_Document.h:1150-1737` | none | J1 |
| Absence: `@id` for child elements | `jam/cast/core.md:40-215` | none | J1 |
| Absence: `Token::Value` pointer | `jam_Document.h:101-108` | none | J1 |
| jq conformance | `saw json --run-tests`; `~/Documents/Poems/dev/jq/tests/*.test` (7 files) | `Source/help/json.md:11`, `:47` | J3 |

`children` passes NAMES.md Rule -1: it states the content, joins the plural `Elements`-key family of `core.md`, its readers and writers hold CONTRACT, and no rule forbids it.

### 0.4 Part J steps

#### J1: Child list at creation
**Scope:** `jam/cast/core.md` (new `@id` row `children`), `jam/jam_core/json/jam_JsonDocument.cpp`, `jam_JsonDocument.h` only if a declaration changes.
**Action:**
1. Invariant: every element of `jam_JsonDocument.cpp`/`.h` that holds an `Id::tokens` list also holds `Document::Elements` under `Id::children`, with one entry per token, in the same order. The entry is the element that the token opens (a container token's container, a link token's `Id::to` child), or `nullptr`. Object key tokens hold `nullptr`; the value token of a member holds the value's element. (Updated at U0: `addKey` adds the key through `addValue (object, name)` → `addToken`, `:985-986`, so a per-member list would need `addToken` to tell keys from values.)
2. The list is created with the tokens list: `addContainer` (`:619`), `addToken` root case (`:917`), `addValue (Identifier)` (`:965`), `getValueStream` (`:1220`), `addParsedValue` (`:5025`).
3. Each token add into a JSON container adds its entry in the same function, after the child exists: scalars `addValue (int, …)` (`:601`), `addToken` (`:921`) → `nullptr`; containers `addContainer` (`:609` token, `:618` child), `addValue (Identifier)` (`:963`, `:965`) → the child; links `addValue (container, owner, existing)` (`:974`, `:977`) → the `Id::to` child; one entry per object member.
4. `getChildByToken` (both overloads): delete the bodies first. New body: `position` = `&value - tokens.begin()`; for an object, the value slot of that member (`position - position % numMemberTokens + numMemberTokens - 1`, the same member arithmetic as the old `:161`); the result is `container.get<Document::Elements> (Id::children)->at (…)`. Signature and meaning unchanged; reads only `container`.
5. Keep the list aligned: `setKeyOrder` `:1357` with `:1363-1369` (move the entry), `:1383-1384` (remove entries); `closePartialContainers` `:1464-1469` (remove entries); `rollBack` `:1477-1494` (remove root entries of the removed root tokens).
6. Filter-AST lists (`appendToken` `:1880-1882`, `parseImport` `:2012-2019`, `parseNumber` `:2372-2373`, `addError` `:3944`, `:3952`, `addHaltCode` `:5413`, `addHaltResults` `:5422`, `addHaltMessage` `:5430`) are not JSON value containers. @Engineer reads the `appendToken` callers not yet read (`:2044`, `2086`, `2089`, `2347`, `2384`, `2395`, `2423`, `2445`, `2475`, `2666`, `2683`, `2705`, `2822`, `2876`) and reports any `getChildByToken` call on those lists; such a call goes back to COUNSELOR.
**Validation:** no count and no walk in `getChildByToken`; no caller changes; no `Token` property added; COUNSELOR reads every site of items 2, 3, and 5 after the edit.

#### J2: `JsonLanguageDocument` keeps the list aligned
**Scope:** `jam/jam_core/json/jam_JsonLanguageDocument.h`, `.cpp`.
**Action:** `getFlatTokens` (`:63-86`) is replaced by `setFlatTokens()`. It writes the flat tokens onto root `Id::tokens` and the aligned root `Id::children` list: the element of each container token (registry loop index, `:68-76`), `nullptr` for each scalar. The sort (`:84`) orders both lists the same way. `read` (`:17`) and `addLanguageElements` (`:52`) call it.
**Validation:** root tokens and root children have the same size on both paths.

#### J3: Verify
1. `cast cast/spell.md --no-sign` in saw: exit 0, 0 warnings.
2. The 7 jq test files (`cd ~/Documents/Poems/dev/jq && saw json --run-tests tests/<name>.test`). Baseline before J1: jq 592/607 (12 malformed), man 238/240, manonig 19/19, onig 50/50, optional 3/3, base64 10/10, uri 20/20. After: each count equal or higher, no abort.
3. Both suite lanes pass.
4. `/usr/bin/time -p` of `[range(0; 16000)]`, `[range(0; 200000)]`, and the 200000 object filter of `benchmark/prepare-exclusive.sh`: each finishes.

### 0.5 Risks

- Stored `JsonDocument` images (`Document::read (sourceText, stored)`) written before J1 hold no `Id::children`. J3 runs the read path through the suite. Whether any image persists on disk across builds is not read.

## 1. Context

ARCHITECT's goal: *"we need to be able to use saw deterministically"*, *"what feature actually missing that could be implemented so we could use exclusive saw"*, *"fold into RFC, need to be planned"*, and *"design by /contract, especially 0 rules, no new pattern, no new semantics allowed"*.
The RFC lists seven capability gaps, three defects, the MCP `status` member, the output bound, and the materialised copies in transit (§4).
This plan executes them in six parts, 0 and A to F, with a tracker. Each step cites the read that decides it.

## 2. Tracker

| ID | Step | Depends on | Blocked by | Status |
|---|---|---|---|---|
| 0.1 | Apply RFC corrections (§3) | lock | — | pending |
| 0.2 | Measure the `agy` message limit | lock | — | pending |
| 0.3 | scout baseline manifests (before any C step) | lock | — | pending |
| A1 | MCP `status` member | 0.1 | — | pending |
| A2 | `saw fields` ranges | 0.1 | — | pending |
| A3 | `saw write --append` | 0.1 | — | pending |
| A4 | Page colour branch returns documents | C6 | — | pending |
| B1 | Output bound vocabulary | 0.2 | — | pending |
| B2 | `--no-limit` common option | B1 | — | pending |
| B3 | `TextDocument::fromDocument` | B2 | — | pending |
| B4 | Bound in `CommandLine` + tests | B3, A4 | — | pending |
| C1 | `Paths::Entry` handle to listing slot | 0.3 | — | pending |
| C2 | `TextDocument` display path view | C1 | — | pending |
| C3 | Patch grouping key | 0.3 | — | pending |
| C4 | `Json` output without copy | C6 | — | pending |
| C5 | Page line numbers and changes, one copy | 0.3 | — | pending |
| C6 | `TextDocument::fromBlock` owner of stream bytes | 0.3 | — | pending |
| C7 | Undo double read and pending reparse | 0.3 | — | pending |
| C8 | Page configuration copies | 0.3 | — | pending |
| C9 | scout after-measure + report | C1-C8 | — | pending |
| D1 | `find --files` metadata + `--json` | C1 | — | pending |
| D2 | `saw count` | B4 | — | pending |
| D3 | `saw sort` | B4 | — | pending |
| D4 | `saw diff` | B4 | — | pending |
| D5 | `page --follow` / `--follow-name\|-F` | B4 | — | pending |
| E1 | Rendered page crash, fixed at the jam owner | — | — | pending |
| F1 | Auditor sweep (once) | all | — | pending |
| F2 | Doxygen pass (Pathfinder) | F1 | — | pending |
| F3 | Sprint log | F2 | — | pending |

RFC Scaffold 10 (CAROL gate) is MACHINIST's downstream step (RFC Handoff Notes, "Downstream handoff"). It is not a COUNSELOR step.

## 3. Facts that correct the RFC (applied in Step 0.1)

Each correction names the superseded text and the read that replaces it.

1. **Page crash is not specific to standard input** (supersedes RFC §1 item 1 and Scaffold 1 "Source/Page.cpp:580-648").
   - Every rendered run exits 139: `--color=always`, `--decorations=always`, `--paging=always`, and a file path (`SPEC.md -r 1:2 --color=always`), with stdout piped or to `/dev/null`. The plain lane exits 0.
   - The crash report (`~/Library/Logs/DiagnosticReports/saw-2026-11-27-105002.ips`, `EXC_BAD_ACCESS at 0x88`) shows `getPathOutput` → `ling_write_page` (libpage) → `TerminalPage::setFiles` → `MarkdownGraphics::addPropertyRow` at `jam_MarkdownGraphics.cpp:213`.
   - `addPropertyRow` reads `line.get<int> (Id::propertyType)` at line 201 and dereferences it at lines 203 and 213 with no owner check.
   - The saw entry signature matches the library: `Page.cpp:16-19` against `ling/Builds/languages/page/page.cpp:196-199`.
   - The report predates the current binary (`~/.local/bin/saw`, 27 Nov 23:00). macOS wrote no newer report.
2. **RFC §4 item 3 (edit bytes) is creation, not transit.** `replacedLine` (`Replace.cpp:204`, `220-226`) and `lineText` (`Patch.cpp:237-250`) are work buffers of the planned bytes. MANIFESTO E: *"Creation can use multiple passes, look-ahead, reverse steps, work buffers, and memory allocation"*. No API exists to write into `plannedText` (`LineDiff.h:180-185`, `301`).
3. **RFC §4 item 4 bullet 2 (page strip) is a transform, which is creation.** `NonPrintable::getWithoutSequences` (`NonPrintable.cpp:345-354`) builds new text. It is also the `--strip-ansi` transform (`Page.cpp:243`).
   - New finding: the colour branch writes to `StandardStream` inside the verb (`Page.cpp:562-567`). This contradicts `Stdio.h:8-9`: *"A verb returns an Output, so that no verb writes to a process stream."* This output also bypasses the bound (Step A4).
4. **RFC §4 item 5 bullets 4 and 5 are creation.** The `PatchValidator` sorted values (`PatchValidator.cpp:406-465`) and `getValueLocation` (`Json.cpp:685-693`) are derived results built during creation. They are not a second copy of complete data.
5. **RFC §4 item 2 bullets 1, 3 (`juce::File` builds) are materialisation at use.**
   - `Find.cpp:86` builds the `juce::File` that `Search` opens. `Undo.cpp:196`, `225`, and `175-183` build the files that undo writes and deletes.
   - MANIFESTO E: *"The owner builds the concrete object only when it needs the capability."* Only the `Entry` build for `--files` (`Paths.cpp:504`, read at `Find.cpp:215` with `juce::ignoreUnused (file, isExplicit)`) materialises without a capability.
6. **Harness limits (RFC Scaffold 11, ruling 10):**
   - OpenCode v2.2.5: 16,777,216 bytes per message (`V=16777216`, minified `McpClient`, bundle line 265398).
   - Pi 1.6.0: `DEFAULT_MAX_MESSAGE_BYTES = 16 * 1024 * 1024` (`@earendil-works/pi-mcp/dist/transports/transport.js:2`). Pi has no saw MCP entry (`~/.carol/pi/`).
   - agy 1.5.18: the Go MCP SDK `frameLimitReader` is present. Its value is not recovered from strings or objdump. Step 0.2 measures it.

## 4. Language / Framework Constraints

- LANGUAGE.md C++/JUCE: one object, one translation unit. Never split one object over more `.cpp` files to meet 300/30/3.
- CODING.md CRITICAL RULES apply to every step: brace initialisation (parentheses for a same-type container), alternative tokens, no bail-out guards, `.at()`, structured bindings, no `==` on strings, no anonymous namespace, no fake carrier, no out-parameters, framework API first.
- CAROL Code Hygiene: no comments and no doxygen before F2.
- Builds run only through cast (`project-info.md:233-257`). ARCHITECT: *"can we not abuse build for each implementation? only build for final validation/profile"*. A build runs only in C9 (its scout manifests build saw) and in V1 (final validation). No implementation step builds.

## 5. Framework Inventory

### 5.1 API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| Listing owner of names and prefixes | saw | `Folder`, `Listing`, `ListedFolder`, `FolderRange` | `Paths.cpp:120-124`, `189-201` | C1 |
| Arena of strings, views | jam_core | `jam::Strings::add (std::string_view)`, `getView (int)` | `jam_Strings.h:217-224`, `570-578` | C1, C2 |
| View of a string or block | jam_core | `Strings::getView (const std::string&)`, `(const juce::MemoryBlock&)` | `jam_Strings.h:657`, `667` | C4, C6 |
| Byte find, byte count | jam_core | `jam::simd::findByte`, `countByte` | `jam_Simd.h:283`, `398` | B4, D2 |
| Append bytes | jam_core | `Format::appendBytes` (std::string, MemoryBlock, OutputStream) | `jam_Format.h:408-422` | A3 |
| Element value types | jam_core | `Document::Value` (closed set, no view) | `jam_Document.h:201-216` | C2 |
| Document byte owner | jam_core | `arena`, `source` | `jam_Document.h:1648-1650` | C6 |
| Stream into caller block | JUCE | `MemoryOutputStream (MemoryBlock&, bool)` | `juce_MemoryOutputStream.h:68-69`, `.cpp:44-46` | C4, C6, A4 |
| Block move | JUCE | `MemoryBlock (MemoryBlock&&)` | `juce_MemoryBlock.h:78` | C6 |
| Copy (to avoid) | JUCE | `MemoryOutputStream::getMemoryBlock` copies | `juce_MemoryOutputStream.cpp:155-157` | C4 |
| Text document factories | saw | `fromText`, `fromView`, `fromInput (Input, path, view)` | `TextDocument.h:66-104` | A3, B3, C6 |
| Planned-document transaction | saw | `Transaction::addFile (target, original, planned, isExisting, count)` | `Transaction.h:62-66` | A3 |
| Common option for all verbs | saw | `Arguments::getMatchingOption` help fallback | `Arguments.cpp:218-219` | B2 |
| Diagnostic line | saw | `Stdio::getDiagnostic (pieces...)` | `Stdio.h:101-110` | B4 |
| MCP text member in pieces | jam_mcp | `Content::setText (key, applyText)`, `setMember (key, int)` | `jam_ModelContextServer.h:57`, `70-76` | A1 |
| MCP chunking and ring | jam_mcp | `writeText` chunk loop; ring 1 MiB | `jam_ModelContextServer.cpp:109-124`; `.h:472-473` | B4 |
| Myers diff, unified diff | saw | `LineDiff (original, target)`, `appendUnifiedDiff` | `LineDiff.h:68`, `100-104` | D4 |
| Byte equality of files | JUCE | `File::hasIdenticalContentTo` | `juce_File.cpp:805-837` | D4 |
| File watch | jam_core | `File::Watcher` (`addFolder`, `addListener`, `Listener::fileChanged`, `Event`) on `AsyncUpdater` + `Timer` | `jam_FileWatcher.h:25-109` | D5 |
| Message loop host | JUCE | `JUCEApplicationBase::main` | `Main.cpp:40-46` | D5 |
| Read from offset | JUCE | `FileInputStream::setPosition`, `read`, `getTotalLength` | `juce_FileInputStream.h:83-87` | D5 |
| Entry metadata | JUCE | `DirectoryEntry::getFileSize`, `getModificationTime`, `isDirectory`, `isHidden`, `isReadOnly` | `juce_RangedDirectoryIterator.h:48-68` | D1 |
| Link facts | JUCE | `File::isSymbolicLink`, `getNativeLinkedTarget` | `juce_File.h:1085`, `1103` | D1 |
| Time text | JUCE | `Time::toISO8601`, `toMilliseconds` | `juce_Time.h:116`, `279` | D1 |
| Sort lines | jam_core | `Strings::sort (bool)`, `sortNatural()` | `jam_Strings.h:515-525` | D3 |
| Natural compare | JUCE | `String::compareNatural` | `juce_String.h:392` | D3 |
| Integer parse | saw | `Arguments::getInteger` (`std::optional<int>`) | `Arguments.h:183` | A2, D3 |
| Open range sentinel | saw | `Arguments::openRangeEnd` | `Arguments.h:11` | A2 |
| Range helpers | JUCE | `Range::getIntersectionWith`, `intersects` | `juce_Range.h:244-255` | A2 |
| String move keeps text | JUCE | `String::String (String&&)` takes `other.text` | `juce_String.cpp:265`, `juce_String.h:1376` | C3 |

### 5.2 Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| Verb table: `(Id, help frame, help document, getArguments, run)` | `CommandLine.cpp:82-93` | 10 verbs | D2-D4 |
| Verb pair `getArguments` / `run` returning `Stdio::Output` | `Write.cpp:45-65` | every verb (`Fields.cpp:374`, `Lines.cpp:352`, …) | A3, D2-D4 |
| Help binary table and frames | `project-info.md:207-224`; `cast/files.md:18-40` | 11 help documents | D2-D4 |
| Cast vocabulary rows | `cast/identifiers.md:18`, `70`, `80-82`, `196`, `416-426`; `cast/text.md:250-274` | all Id and text names | A1, A3, B1, D2-D5 |
| Summary line assembly | `Transaction.cpp:175-181` | dry-run, applied, undo | B4 |
| MCP pieces callable | `ToolServer.cpp:99-120` | one tool | A1 |
| Planned document write | `Write.cpp:35-42` | write | A3 |
| Parallel walk callback, shared listing | `Paths.cpp:205-214`, `493-506` | find, replace, fields, lines | C1, D1 |
| Documents moved, never copied | `Stdio.cpp:33-54` | every verb | C4, C6 |
| Common flag for every verb | `Arguments.cpp:218-219`, read at `CommandLine.cpp:115` | help | B2, B4 |
| Owner-and-view document | `TextDocument.h:91-104` | page ranges (`Page.cpp:336`) | B3 |
| Message loop for a long-lived mode | `Main.cpp:35-46`, `ToolServer.h:10` | MCP | D5 |
| Case table row | `test/cases.md:3-6` | 915-line table | every step |
| MCP lane decode | `test/run.lua:452-463`, `518-524` | MCP lane | A1, B4 |
| scout manifest | `benchmark/saw-find-literal-vs-rg.md:1-23` | benchmark folder | 0.3, C9 |

### 5.3 Names

| Family | Members | Intent | Step |
|---|---|---|---|
| MCP result keys | `exitCode`, `output`, `outputBase64`, `errors`, `errorsBase64` (`cast/identifiers.md:416-424`) | key of a result member | A1: `status` replaces `exitCode` |
| `*Value` | `autoValue` (`cast/identifiers.md:196`) | a value text | A1: `okValue` (`ok`), `mismatchValue` (`mismatch`), `errorValue` (`error`) |
| `no*Option` | `noIgnoreOption` (`cast/identifiers.md:70`), `noLineNumberOption` (`Find.cpp:303`) | negative flag | B1: `noLimitOption` (`--no-limit`) |
| `*Option` with short form | `helpOption` `--help\|-h` (`cast/identifiers.md:18`) | flag text | A3: `appendOption` (`--append\|-a`); D5: `followOption` (`--follow`), `followNameOption` (`--follow-name\|-F`) |
| verb ids | `find`, `replace`, `lines`, `fields`, `patch`, `undo`, `write` (`Identifiers.h:159-165`) | verb name | D2-D4: `count`, `sort`, `diff` |
| help keys | `findHelp`/`findDocument` (`cast/files.md:20`, `40`) | frame and document | D2-D4: `countHelp`, `countDocument`, … |
| summary pieces | `summaryDryRun`, `editsIn`, `expectToWrite`, `toWrite`, `applyToRestore` (`cast/text.md:250-272`) | opener, then connective pieces | B1: `summaryOutputLimit` (`output limit — `), `linesPrinted` (` line(s) printed, `), `omittedToPrintAll` (` line(s) omitted. Pass --no-limit to print all.`) |
| `TextDocument` factories | `fromFile`, `fromTextFile`, `fromInput`, `fromText`, `fromView` (`TextDocument.h:40-104`) | creates a document over one owner | B3: `fromDocument`; C6: `fromBlock` |
| `CommandLine` static `get*` | `getBannerAndHelp`, `getHelpDocuments` (`CommandLine.cpp:36`, `52`) | returns an Output or documents | B4: `getLimitedOutput` |
| verb classes | `Write`, `Fields`, `Page` (`getArguments`, `run`) | one verb | D2-D4: `Count`, `Sort`, `Diff` |

### 5.4 Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| "No `==` / `!=` on strings" | `compare (…) == 0`; a verb table, not an `if` chain | `CommandLine.cpp:101` |
| "3 branches max → lookup" | `static const std::array` of tuples | `CommandLine.cpp:82-93` |
| "No bail-out guards; result returns permitted" | failure result returns at the top of `run` | `Write.cpp:52-64` |
| "Use `.at()`" | `tokens.at (slotIndex)` | `Fields.cpp:184` |
| "Same container type uses parentheses" | `const jam::Array<int> documentChanges (…)` | `Page.cpp:582` |
| "Structured bindings, descriptive names" | `const auto& [exitCode, documents, failures, errorDocuments]` | `ToolServer.cpp:98` |
| "Alternative tokens" | `not`, `and`, `or` | `Page.cpp:182` |
| "Consume the payload / tell" | listener receives the changed file and its event | `jam_FileWatcher.h:59` |

### 5.5 Absence

| Need | Query | Result |
|---|---|---|
| JUCE file-change watcher | ling `workspaceSymbol` `FileWatcher`, `FileChangeWatcher`, `DirectoryWatcher` | `[]` (JUCE only gives `setChangeNotificationOnlyOnRelease`, `juce_Slider.h:665`) |
| jam unified diff, line/word count, mtime, walker, byte compare, line sort | ling `workspaceSymbol` `unifiedDiff`, `countLines`, `countWords`, `lastModificationTime`, `directoryWalk`, `bytesEqual`, `sortLines` | `[]` |
| jam "has ANSI escape" helper | `containsEscape`, `hasEscape`, `stripAnsi` | `[]`; markdown and saw-local names only |
| jam Element view member | `Document::Value` variant | no `std::string_view` alternative (`jam_Document.h:201-216`) |
| Write into `plannedText` from a callable | `LineDiff.h` public API | none (`LineDiff.h:70-162`) |
| JUCE canonical path with `..` after links | `File::getLinkedTarget` body | one link level only (`juce_File.cpp:1020-1026`); saw keeps `std::filesystem::canonical` (`Paths.cpp:315-324`, reason `Paths.h:86-91`) |
| agy message limit | strings of `/Users/jreng/.local/bin/agy`; `go tool objdump` | value not recovered |

## 6. Validation Gate

COUNSELOR validates each step before the next. The checks are MANIFESTO.md (BLESSED, 0 rules), NAMES.md Rule -1, CODING.md CRITICAL RULES, the Inventory rows of the step, and this locked plan (no deviation, no scope drift).
Each @Engineer prompt restates the CODING.md CRITICAL RULES, MANIFESTO E (Complete at Creation, verbatim), and the doxygen-first instruction, and pastes the Inventory rows of the step verbatim.
No step and no part ends with a build (§4, ARCHITECT's ruling). A step validation has read checks and run checks. COUNSELOR does the read checks at each step, by a read of the changed code. The run checks ("cases pass", "both lanes pass", "exits 0") run one time, in V1: one cast build (Release toolchain rows), then `nvim --headless -l test/run.lua ~/.local/bin/saw` and `... mcp`.
@Auditor runs once, after the final implementation step (F1).

## 7. Steps

### Part 0 — Pre-flight

#### Step 0.1: RFC corrections
**Scope:** `RFC-exclusive-saw.md`.
**Action:** COUNSELOR adds §3 items 1-6 to the RFC as explicit corrections, each naming the superseded text.
**Validation:** Every correction has its citation. The RFC keeps every ARCHITECT quote.

#### Step 0.2: Measure the agy limit (ruling 10)
**Scope:** read-only; scratchpad fixture only.
**Action:** @Pathfinder reads `agy --help` for a mode that runs one prompt and exits (§9 item 6). It builds a 64 KiB text fixture in the scratchpad, and runs one agy call to saw MCP `page` on that fixture through the saw entry of `~/.carol/agy/mcp_config.json`. It records whether the 64 KiB response arrives whole. If agy has no such mode, that read is the evidence, and the item goes to ARCHITECT as a scope fact (ruling 10).
**Validation:** A whole response proves the agy limit is above 32 KiB plus the trailer. A failure stops Part B (stop set: ruling 7 against the measured limit).

#### Step 0.3: scout baseline
**Scope:** new manifests under `benchmark/` (form of `benchmark/saw-find-literal-vs-rg.md:1-23`).
**Action:** COUNSELOR writes manifests for `find` over a large tree, `find --files`, `replace` with many matches, `page` with large output, and `json` with large output. @Pathfinder runs them with the `## toolchain` rows.
**Validation:** Each report exists, with epochs and samples, before any C step starts.

### Part A — Existing verbs

#### Step A1: MCP `status` member (ruling 5)
**Scope:** `cast/identifiers.md` (row `exitCode` → `status`; new `okValue`, `mismatchValue`, `errorValue`), `Source/ToolServer.cpp:115-120`, `SPEC.md:802`, `test/run.lua:452-463`, `540-548`.
**Action:**
1. Delete `Id::exitCode` first. The compiler lists every use.
2. In `runTool`, write `status` with `content.setText (Id::status, …)`. The text comes from a `static constexpr std::array<std::string_view, 3> statusValues { Id::okValue, Id::mismatchValue, Id::errorValue }`, read with `.at (juce::jlimit (Stdio::exitSuccess, Stdio::exitError, exitCode))`. Exit codes 2-5 give `error` (ruling 5).
3. In `run.lua`, `decodeToolReply` returns `status`. On the MCP lane, `compareExitCodes` compares the status of `step.exit` (0 → `ok`, 1 → `mismatch`, 2 or more → `error`) with the decoded status.
4. Update `SPEC.md:802`.
**Inventory:** `Content::setText`; MCP result keys family; `*Value` family; lookup-table synthesis.
**Validation:** No `exitCode` member remains on the MCP lane. The CLI lane keeps integer exit codes (`Stdio.cpp:146`). Both lanes pass.

#### Step A2: `saw fields` ranges (Scaffold 4; type per §10 item 3)
**Scope:** `Source/Fields.cpp:11-51`, `178-224`, `Source/Fields.h` (member `fieldIndices`), `Source/help/fields.md`, `SPEC.md:568`, `test/cases.md`.
**Action:**
1. Replace `jam::Array<int> fieldIndices` with `jam::Array<jam::Union<int, int>> fieldRanges`. Each element packs the first and the last field index, both inclusive (`jam::Union::pack`, `jam_Union.h`).
   - A single index `N` gives `jam::Union<int, int>::pack (N, N)`.
   - `A-B` gives `pack (A, B)`. `B < A` is a descending range: `3-1` gives fields 3, 2, 1.
   - `A-` gives `pack (A, Arguments::openRangeEnd)` (`Arguments.h:9-11`).
2. `getParsedFieldIndex` keeps its zero and integer checks (`Fields.cpp:11-25`). Range bounds are positive. A token that starts with `-` and a digit stays a negative single index, as `SPEC.md:568` defines. So `-1` keeps its meaning, and a token such as `-1-3` is malformed (`failFieldSpecInvalid`).
3. `appendFormattedLine` reads each element with a structured binding (`const auto [firstIndex, lastIndex] { fieldRange };`, `jam_Union.h` `get` and `tuple_size`). It iterates from `firstIndex` to `lastIndex` per line, in either direction, with the open end capped at `tokens.size()`. It writes `outputDelimiter` between fields and keeps the strict failure for a closed bound outside the line (`Fields.cpp:211-217`).
4. The iteration is bounded by the token count of the line, so no spec expands without a bound (RFC Principles, Bounds).
**Inventory:** `Arguments::getInteger`, `Arguments::openRangeEnd`, `jam::Union<int, int>` (`pack`, structured binding), `.at()` synthesis.
**Validation:** No range expands at parse time. `1-2`, `2-`, `3-1` (fields 3, 2, 1), `0-2` (reject), and `1-x` (reject) cases pass. Both lanes pass.

#### Step A3: `saw write --append` (Scaffold 8, ruling 3)
**Scope:** `cast/identifiers.md` (`appendOption` `--append|-a`), `Source/Write.cpp:18-48`, `Source/help/write.md`, `SPEC.md:853-862`, `test/cases.md`.
**Action:**
1. Add `Id::appendOption` to the flag list of `Write::getArguments`.
2. When the target exists and `--append` is set, build the planned bytes once: the original bytes, then the exact standard input bytes, through `jam::Format::appendBytes`. Wrap them with `TextDocument::fromText`, and pass them to the planned-document `addFile` overload (`Transaction.h:62-66`). Add no newline (ruling 3).
3. With no existing target, `--append` writes as `write` does.
4. Binary refusal (`Write.cpp:25-26`), dry-run, `--apply --expect 1`, and undo are unchanged.
**Inventory:** `Format::appendBytes`, `TextDocument::fromText`, `Transaction::addFile`, `*Option` family.
**Validation:** The dry-run diff shows only the appended lines. Undo restores the original. Both lanes pass.

#### Step A4: Page colour branch returns documents
**Scope:** `Source/Page.cpp:543-573`.
**Action:** Delete the `StandardStream` write (`Page.cpp:562-567`). Both branches write through `juce::MemoryOutputStream (MemoryBlock&, false)` into one block. The colour branch wraps the block with `TextDocument::fromBlock` (C6). The non-colour branch keeps `getWithoutSequences` (a transform) on the block view.
**Inventory:** `MemoryOutputStream (MemoryBlock&, bool)`, `fromBlock`.
**Validation:** No verb writes a process stream (`Stdio.h:8-9`). Rendered output reaches the bound in B4.

### Part B — Output bound (rulings 6-10; site per §9 item 5)

#### Step B1: Vocabulary
**Scope:** `cast/identifiers.md` (`noLimitOption` `--no-limit`), `cast/text.md` (`summaryOutputLimit`, `linesPrinted`, `omittedToPrintAll`), `static constexpr size_t outputLimitBytes { 32 * 1024 }` in `CommandLine.cpp` (the one translation unit that reads it; CODING.md narrowest scope).
**Validation:** The trailer is `saw: output limit — <P> line(s) printed, <O> line(s) omitted. Pass --no-limit to print all.` (ruling 8). The `saw: ` prefix and the newline come from `Stdio::getDiagnostic` (`Stdio.h:101-110`).

#### Step B2: `--no-limit` for every verb
**Scope:** `Source/Arguments.cpp:207-222`.
**Action:** `getMatchingOption` returns `Id::noLimitOption` as a flag, in the same form as `Id::helpOption` (`Arguments.cpp:218-219`).
**Validation:** Every verb accepts `--no-limit`. No verb table changes.

#### Step B3: `TextDocument::fromDocument`
**Scope:** `Source/TextDocument.h`, `Source/TextDocument.cpp`.
**Action:**
1. Add `static std::unique_ptr<TextDocument> fromDocument (std::unique_ptr<TextDocument> viewedDocument, std::string_view output)` and a private owner member `std::unique_ptr<TextDocument> viewedDocument`.
2. The new document owns the viewed document and views a prefix of its bytes. This is the form of `fromInput (Input, path, output)` (`TextDocument.h:91-104`): one owner, one view, no copy.
**Validation:** Name passes Rule -1 (the `from*` family, the owner-and-view form). No byte copied.

#### Step B4: Bound in `CommandLine`, both lanes, tests
**Scope:** `Source/CommandLine.cpp:52-133`, `test/cases.md`.
**Action:**
1. Add `static Stdio::Output getLimitedOutput (Stdio::Output output, bool isLimited)` in `CommandLine.cpp`. Without the limit, it returns the Output unchanged.
2. With the limit, it moves the documents in order. It keeps each whole document while the total stays within `outputLimitBytes`. At the boundary document, it cuts at the last `\n` inside the budget (`jam::simd::findByte`), and wraps the kept prefix with `TextDocument::fromDocument`. A line longer than the budget keeps nothing (ruling 7). It drops the rest.
3. It counts printed and omitted lines with `jam::simd::countByte` on the kept and the dropped views. It appends the trailer document (`TextDocument::fromText (Stdio::getDiagnostic (...), {})`, form of `Transaction.cpp:175-181`).
4. The exit code, the failures, and the error documents pass unchanged (ruling 9).
5. `CommandLine::run` returns `getLimitedOutput (…)` on each path. `runVerb` reads `verbArguments.contains (Id::noLimitOption)` (form of `CommandLine.cpp:115`). The global paths are always limited.
6. `Stdio::write` and `ToolServer::runTool` do not change, so both lanes write the same bytes.
7. Before the step, read the byte size of each help document and frame (`project-info.md:207-224`, `cast/files.md:18-40`). A help text over 32 KiB is a stop-set item: ruling 6 against `runGlobalOption` refusing extra arguments (`CommandLine.cpp:70-71`).
8. Tests: add `test/cases.md` rows on each lane: output over 32 KiB, one line over 32 KiB, and `--no-limit` (RFC Scaffold 11).
**Inventory:** `findByte`, `countByte`, `getDiagnostic`, `fromDocument`, `fromText`, the common-flag form, and the summary-assembly form.
**Validation:** Identical bytes on both lanes (ruling 9). The verb exit code does not change when output is cut. No byte is copied.

### Part C — 0 rules (RFC §4 as corrected by §3)

#### Step C1: `Paths::Entry` is a handle to its listing slot (§9 item 4)
**Scope:** `Source/Paths.h:12`, `40-74`, `161-168`, `Source/Paths.cpp:146-187`, `217-279`, `438-548`, the consumers `Find.cpp:58-115`, `186-227`, `Replace.cpp:320-362`, and every `getReadableFiles` caller.
**Action:**
1. Delete `Entry` first. The compiler lists every consumer.
2. `getDirectoryListing` writes each slot's display path (folder display prefix and name) contiguously into the listing's one buffer at creation. The slot keeps the name offset. This is MANIFESTO permitted copy 4, which C9 measures.
3. An entry is `std::pair<std::shared_ptr<const ListedFolder>, int>`, a handle (MANIFESTO: *"A scalar, a handle, or a view passed by value does not copy"*). It shares the listing in the form of `FolderRange` (`Paths.cpp:123`).
4. An explicit file and standard input get their own explicit entry forms. These replace the `juce::File {}` marker (`Paths.cpp:453`, `455`, `Replace.cpp:324`).
5. A verb builds the `juce::File` from the listing's full prefix and the name only when it opens the file (materialisation at use). The sort and dedupe read the slot views (`Paths.cpp:525-545`).
6. The `workerEntries` staging of data (`Paths.cpp:491-519`) is deleted. The per-worker arrays hold `Entry` handles only, so the walk takes no lock. MANIFESTO E: *"A scalar, a handle, or a view passed by value does not copy the data it names."*
**Validation:** No `juce::ignoreUnused` on an entry field. No `std::string` stored per entry; `getFile` builds the path with `jam::Format` at use. ARCHITECT: *"use jam::Format, only use juce::String for juce native API (args)"*. The `find`, `--files`, `replace`, `fields`, and `lines` outputs are byte-identical.

#### Step C2: One owner of the display path (§9 item 4)
**Scope:** `Source/TextDocument.h:27-29`, `311`, `Source/TextDocument.cpp:5-11`, `178-181`, `Source/Stdio.h:55`, `Source/Stdio.cpp:33-41`, `Source/FindOutcome.cpp:24-56`, and the 11 `getDisplayPath` consumers (`FindOutcome.cpp:47`, `51`; `LineDiff.cpp:551`, `554`; `PatchValidator.cpp:388`, `572`; `Replace.cpp:285`; `StagedFile.cpp:38`; `TextDocument.cpp:77`; `Transaction.cpp:32`, `56`).
**Action:**
1. `TextDocument` stores the display path as a `std::string_view` member, in the form of `dominantLineEnding` (`TextDocument.h:415`). It does not store an Element `juce::String` (`TextDocument.cpp:10`).
2. Input documents view their owner (the listing slot, the argument list, or the undo record) within the verb.
3. Output documents carry no path. `Stdio::getDocuments` loses its `displayPath` parameter.
4. `FindOutcome` sorts and deduplicates by the walk order key (the entry handle or the listing slot view) that the verb keeps alive, not by a document path (`FindOutcome.cpp:47`, `51`).
**Validation:** No document outlives the owner of its path. No path copy per document. Diagnostics and diff headers are byte-identical.

#### Step C3: Patch grouping key
**Scope:** `Source/Patch.cpp:30-69`.
**Action:**
1. Delete `groupIndexByCanonicalPath` (`juce::String` key, `Patch.cpp:37`, `51`). Its key repeats the path of `groupFiles` (`Patch.cpp:56`).
2. Dedupe canonical targets with a `jam::HashMap<std::string_view, int>` keyed by `jam::Strings::getView (groupFiles.at (index).getFullPathName())`.
3. The key view stays valid while `groupFiles` lives. A moved `juce::String` keeps the same text pointer (`juce_String.cpp:265`: `String::String (String&& other) noexcept : text (other.text)`; member `text` at `juce_String.h:1376`).
**Validation:** One path text per group. The patch cases pass.

#### Step C4: `Json` output without copy
**Scope:** `Source/Json.cpp:944`, `992`.
**Action:** `buffer` becomes `juce::MemoryOutputStream` over a `juce::MemoryBlock` that the function owns (`MemoryOutputStream (MemoryBlock&, false)`). The output document is `TextDocument::fromBlock (std::move (block), {})` (C6). Delete the `std::string { … }` copy at line 992.
**Validation:** The output bytes are built once. The json cases pass on both lanes.

#### Step C5: Page numbers and changes
**Scope:** `Source/Page.cpp:468-483`, `582-603`, `Source/PageRanges.cpp:251-319`.
**Action:**
1. `getOriginalNumbers` returns an empty array when not squeezing. `getNumberOfLine` already gives `lineIndex + firstLine` (`PageRanges.cpp:256`).
2. Build the shown numbers once, in `getShownNumbers`.
3. Pass `documentChanges` through when `originalNumbers` is empty and `documentChanges.size()` equals the line count of the shown text, in place of `getShownChanges` (`PageRanges.cpp:308-319`). A UTF-16 file decodes to a different line count (`Page.cpp:286-288`), and `getShownChanges` then sizes the array to that count (`PageRanges.cpp:315-316`).
**Validation:** One array per derived truth. Page cases pass. Rendered cases wait for E1.

#### Step C6: `TextDocument::fromBlock`
**Scope:** `Source/TextDocument.h`, `Source/TextDocument.cpp`.
**Action:** Add the factory `static std::unique_ptr<TextDocument> fromBlock (juce::MemoryBlock&& block, std::string_view displayPath)` and a private owner member `juce::MemoryBlock block`. The factory moves the block in (`juce_MemoryBlock.h:78`) and views it with `jam::Strings::getView (const juce::MemoryBlock&)` (`jam_Strings.h:667`). It builds no line index, as `fromText` does (`TextDocument.h:69-78`).
**Validation:** No copy. Name passes Rule -1: the `from*` family, one owner per factory.

#### Step C7: Undo reads
**Scope:** `Source/Undo.cpp:116-217`, `Source/UndoCache.cpp:461-478`.
**Action:**
1. No change. The target is read at most once: `Undo.cpp:129` runs only when the blob hash differs from `preHash`, and that path returns at `:131-135`, then at `:200-204`, before `getCurrentState` reads `:142`. The `:129` document is only hashed, so the reason of `Input.h:80` (a later write) does not apply to it.
2. `UndoCache::getTransactionIds` keeps its validity check. `runList` reuses the parsed pending documents through the API that the step defines from the read of `UndoCache.h`.
**Validation:** Each file and record is read once per command. Undo cases pass.

#### Step C8: Page configuration copies
**Scope:** `Source/PageConfiguration.cpp:80-96`, `202-212`, `264-270`, `Source/Page.cpp:681`.
**Action:**
1. `getStyleSheetText` returns the owning `std::unique_ptr<Input>`, not `std::string { input->getBytes() }`. The theme view reads it.
2. `getFileOptionTexts` keeps the file text as the owner of the option views.
**Validation:** No second copy of a theme or config text.

#### Step C9: scout after-measure
**Action:** Run the 0.3 manifests again. COUNSELOR reports each delta with the report files.
**Validation:** Each report cites its baseline.

### Part D — New capabilities

#### Step D1: `find --files` metadata (Scaffold 3, ruling 2; form per §10 item 1)
**Scope:** `Source/Paths.cpp:146-187` (listing slot gains the metadata), `Source/Find.cpp:186-227`, `Source/help/find.md`, `SPEC.md:353`, `test/cases.md`.
**Action:**
1. The listing slot holds the metadata from the same `directory_entry` read (`Paths.cpp:161-164`).
2. Walk options add `maxDepth`, `entryTypes`, the `mtime` and `size` filters, `sortKey`, and `isReverse` (Scaffold 3).
3. `--json` writes one object per entry (`path`, `type`, `size`, `mtime`, `mode`, `target`) in the text form of §10 item 1. Write it with the jam JSON writer that `saw json` uses, not by hand.
**Validation:** No second carrier (RFC Scaffold 3 dependency). Output is identical on macOS and Windows.

#### Step D2: `saw count` (Scaffold 6; layout per §10 item 2)
**Scope:** new `Source/Count.h`, `Source/Count.cpp`; `CommandLine.cpp:82-93`; `project-info.md` `## binary`; `cast/files.md`; `cast/identifiers.md`; `Source/help/count.md`; `SPEC.md`; `test/cases.md`.
**Action:**
1. Read through `Input::create` or `Paths::getReadableFiles`.
2. Count lines with `jam::simd::countByte`, and bytes with the view size.
3. Count words by the whitespace rule of `Fields.cpp:97-116` (`jam::Document::isWhitespace`).
4. The output layout matches GNU coreutils `wc` byte for byte on every platform (§10 item 2). `test/cases.md` reference rows check it.

#### Step D3: `saw sort` (Scaffold 7; layout per §10 item 2)
**Scope:** new `Source/Sort.h`, `Source/Sort.cpp`, and the same registration rows as D2.
**Action:**
1. Sort line views by UTF-8 byte order (default), numeric order (`-n`), reverse (`-r`), unique (`-u`), and duplicate counts (`-c`, the GNU `uniq -c` layout of §10 item 2). Each mode matches the reference tool output through `test/cases.md` reference rows (the `reference` and `compare` columns, `test/cases.md:3-6`), which is the established parity pattern (`SPEC.md:572`).
2. Output is a view per line in sorted order. No line text is copied.

#### Step D4: `saw diff` (Scaffold 5)
**Scope:** new `Source/Diff.h`, `Source/Diff.cpp`; `Source/LineDiff.h:100-104`, `Source/LineDiff.cpp:540-560`; the same registration rows as D2.
**Action:**
1. Accept two paths, or one path and `-`.
2. `-q` / `--quiet` compares raw bytes. Two files use `File::hasIdenticalContentTo`. A file and standard input compare the two `Input` views. Exit 0 identical, 1 different, 2 error.
3. The default mode builds `TextDocument::fromTextFile` for both, then `LineDiff (original, target)`, then `appendUnifiedDiff`.
4. `appendUnifiedDiff` takes the planned display path as a parameter, because the `+++` header names the original today (`LineDiff.cpp:551`, `554`). Existing callers pass the original path.

#### Step D5: `page --follow` / `--follow-name|-F` (Scaffold 2; §9 items 2, 3)
**Scope:** `cast/identifiers.md` (`followOption` `--follow`, `followNameOption` `--follow-name|-F`), `Source/PageOptions.h`, `Source/PageOptions.cpp:50-76`, `Source/Page.cpp`, `Source/Main.cpp`, `Source/help/page.md`, `SPEC.md:826-851`.
**Action:**
1. The MCP lane rejects both flags with exit 2 (Scaffold 2: `not std::holds_alternative<Input::StandardInput> (input)`).
2. On the CLI lane, follow runs under a `JUCEApplicationBase` loop in the form of `runToolServer` (`Main.cpp:35-46`), because `File::Watcher` needs the message thread (`jam_FileWatcher.h:25-26`, `52`). JUCE has no watcher (§5.5).
3. The follow application owns one `File::Watcher` on the parent folder (`addFolder`, `jam_FileWatcher.h:95`) and listens (`Listener::fileChanged`, `jam_FileWatcher.h:59`).
4. On `fileUpdated` for the target, it reads from the last offset with `FileInputStream::setPosition` and `read` (`juce_FileInputStream.h:83-87`). It builds one `Stdio::Output` per event, and `Stdio::write` writes it. So the only stream writer stays `Stdio::write` (`Stdio.h:8-9`).
5. `--follow-name` reopens on `fileRenamedNewName` or `fileCreated` for the target name.
6. The bound applies (ruling 6). The follow application counts the bytes that it has written. At the bound it writes the trailer and quits, unless `--no-limit` is given.
**Validation:** No verb writes a process stream directly. The MCP lane refuses with exit 2. The process ends on SIGINT by the platform action (JUCE installs no SIGINT handler on macOS, `juce_MessageManager_mac.mm:62-66`; on Linux JUCE calls `quit()`, `juce_Messaging_linux.cpp:288-292`) and on `systemRequestedQuit` (`ToolServer.cpp:45-48` form). Correction 2026-11-28: the earlier text said SIGINT reaches `systemRequestedQuit`; the JUCE reads above contradict it.

### Part E — Rendered page crash

#### Step E1: Fix at the owner (§9 item 1)
**Scope:** `jam/jam_markdown_graphics/graphics/jam_MarkdownGraphics.cpp:197-218`, the jam owner that sets `Id::propertyType` on property lines, and the rebuild of ling's libpage (`ling/Builds/languages/page/page.cpp`), which runs in V1.
**Action:**
1. @Pathfinder traces which owner assigns `Id::propertyType` to the rows that `MarkdownDocument::addProperties` builds (`jam_MarkdownDocument.cpp:1466-1503`). It reproduces the crash under a PTY with a fresh crash report, because the cited report predates the current binary (§3 item 1).
2. The fix sets the invariant at that owner. No guard goes into `addPropertyRow` (MANIFESTO D, pessimistic programming).
3. Add regression rows for rendered `page` with a file and with standard input (`--color=always`, `--decorations=always`).
**Validation:** Every rendered variant of §3 item 1 exits 0. The plain lane is unchanged.

### Part F — Close

- **V1:** Final validation, after E1. One cast build of saw (Release toolchain rows) and the ling libpage rebuild of E1. Then both lanes, sequentially: `nvim --headless -l test/run.lua ~/.local/bin/saw` and `... mcp`. Every run check of A1-E1 passes here. A failure goes back to its step; the fix is code only, and V1 runs again.
- **F1:** @Auditor once, against SPEC, MANIFESTO, NAMES, CODING, and this plan. Each finding goes through the counselor.md DCF filter.
- **F2:** @Pathfinder doxygen pass on every changed header and implementation (doxygen-writer skill).
- **F3:** Sprint log per `/log` after ARCHITECT's instruction. Commit messages go in chat.

## 8. BLESSED Alignment

- **B:** One owner per path, per byte buffer, and per document (C1, C2, B3, C6). The follow watcher is owned by the follow application object (D5).
- **L:** Each new verb is one object, one header, and one `.cpp` (D2-D4). Status and verb dispatch use lookup tables (A1, D2-D4).
- **E:** The standard-input marker becomes explicit (C1). Every value is named in cast (A1, A3, B1, D5).
- **S (SSOT):** One bound function (B4). One limit value (B1). One status table (A1).
- **S (Stateless):** Read-only verbs hold no state. The bound function holds none.
- **E (Encapsulation):** No verb writes a process stream (A4, D5). Data in transit has 0 copy (C1-C8).
- **D:** Byte-identical outputs on both lanes and platforms. Measured with scout (0.3, C9).

## 9. Questions closed by CONTRACT or by a read

Each candidate question was filtered against MANIFESTO.md, CODING.md, and NAMES.md, read in full this session (counselor.md Options & Recommendations).

1. **Crash scope → in scope (E1).** counselor.md: *"Bugs ARCHITECT surfaces are fixed now — related to the sprint or not"*. The RFC surfaced the crash (RFC §1 item 1). The fix lives at the owner in jam (§3 item 1).
2. **Bound on follow → applies (D5).** Ruling 6: *"`saw` bounds the output of every call by default"*. `--no-limit` lifts it for one call (ruling 8).
3. **Follow short forms → `--follow` has none; `--follow-name` is `-F`.** `-f` is `--force-colorization` (`Source/help/page.md:19`). `-F` is free in `page.md` (search for `` `-F` `` returned no line). NAMES.md Rule -1: a name that collides shows a wrong design, so the design drops the collision.
4. **Path owner → the listing (C1, C2).** The candidates were filtered as follows:
   - A command arena in `Stdio::Output` is rejected. It copies each name from the listing again (MANIFESTO E: *"A copy per hop"*). Documents would also view a sibling member, which is a B violation: *"I'm not sure what outlives what"*.
   - `TextDocument` co-owning `ListedFolder` is rejected. `Paths.cpp:4` includes `TextDocument.h`, so it is a layer violation (MANIFESTO E, *"Lower layers never know about higher layers"*).
   - The remaining design: the listing stores each slot's display path contiguously at creation (MANIFESTO permitted copy 4, measured in C9). Input documents view it within the verb. Output documents carry no path (§7 C2).
5. **Bound site → CommandLine (B3, B4).** A writer-side bound needs `--no-limit` from a second parse outside `Arguments`. The verb's parse exists only in `runVerb` (`CommandLine.cpp:113-115`), so that is a hand-rolled parse where the framework answers. That leaves the CommandLine site.
6. **agy method → read first (0.2).** `agy --help` is readable, so it is a read, not a question.

## 10. ARCHITECT rulings of 2026-11-28 (plan session)

1. **Q4 `--json` entry form (D1): *"Text form"*.**
   - `mtime` is ISO 8601 text from `juce::Time::toISO8601` (`juce_Time.h:279`).
   - `mode` is POSIX octal text, such as `"0644"`. The Windows value comes from `std::filesystem::perms`.
   - `target` is `""` for a non-link.
   - `type` is `"file"`, `"dir"`, or `"symlink"` (Scaffold 3).
2. **Q5 Reference layout (D2, D3): *"GNU (MSYS2)"*.** `saw count` and `saw sort -c` match GNU coreutils `wc` and `uniq -c` byte for byte, on every platform.
3. **Field range type (A2): *"Union it is"*; order per *"it's just fucking 2 integers why overcomplicate"*.**
   - ARCHITECT: *"jam::Array<std::pair<int, int>> instead of using pair of int, probably better using jam::Union, Document::Span? or probably juce::Range"*, *"my point is, dont just use arbitrary pair. Union docs is inaccurate, it's not transport only. definitely can be used or any packing"*, and *"isnt Union is lighter? trivially copyable, not needed all the bell and whistles of range"*.
   - `fieldRanges` is `jam::Array<jam::Union<int, int>>`: the first and the last field index, both inclusive. Fields uses no range operation (union, intersection, merge), only the two bounds.
   - Order is the order of the two integers: `3-1` gives fields 3, 2, 1 (RFC Scaffold 4, *"ascending or descending"*). ARCHITECT: *"why is it matter ascending or descending, it's just fucking 2 integers why overcomplicate"*.

4. **D1 filter grammar: *"crossplatfrom"* (answer to GNU find against fd, 2026-11-28).**
   - fd is the reference, because its grammar is the same on macOS, Windows and Linux. The Windows `find.exe` is another program.
   - Flags, from fd v10.5.2 (`cli.rs`, `size.rs`, `time.rs`; Librarian brief): `--max-depth <N>` (`-d`; the walk root is depth 0, its children are depth 1), `--type file|dir|symlink` (`-t`; short forms `f`, `d`, `l`; repeatable; values combine with OR; a link to a folder is `symlink`), `--size <+|-><N><unit>` (`-S`; `+` at least, `-` at most, no sign exactly; units `b`, `k`, `m`, `g`, `t` base 1000 and `ki`, `mi`, `gi`, `ti` base 1024, any case; a unit is required; repeatable, combined with AND; regular files only), `--changed-within <date|dur>` and `--changed-before <date|dur>` (modification time; exclusive bounds; combined with AND).
   - Sort: fd has none. Scaffold 3 adds `--sort name|mtime|size` and `--reverse`.
5. **D4 diff header (closed by RFC Principles, Deterministic, `RFC-exclusive-saw.md:154`).** *"Every walk, sort, count, diff, and raw-byte append produces identical output on macOS and Windows"*. File times differ between checkouts, so the header carries no time. `saw diff a b` prints `--- a` and `+++ b`, with no `(planned)` suffix. The hunks follow `diff -u`.
6. **D2, D3 test references.** GNU coreutils is not installed on the macOS machine (no `gwc`, `gsort`, `guniq` under `/opt/homebrew` or `/usr/local/bin`; `/usr/bin/wc` is BSD). The D2 and D3 cases use baselines from the GNU rules in `coreutils-src/wc.c` and `uniq.c`.

No open question remains.
