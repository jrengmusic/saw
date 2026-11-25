# PLAN: native MCP and garbage clean-up

**RFC:** RFC-live-to-premise.md (its work is in the tree and stays)
**Date:** 2026-11-25
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM. macOS and Windows.

## Context

ARCHITECT: *"CLEAN UP YOUR GARBAGE. DESIGN BY CONTRACT, NON NEGOTIABLE NO EXCEPTION"*, *"DELETE ALL GARBAGE BINARY, ALL GARBAGE PLAN, SPEC AGAINST MY RULES, AGAINST CONTRACT"*, *"IMPLEMENT MCP. CORRECTLY … DONT FUCKING CREATE NEW PATTERN. NO FUCKING HANDROLL."*, *"there are no saw_ToolServe anymore, we use native mcp. all carol CLI use --mcp"*, *"we have 3 working pattern."*

COUNSELOR kept a second binary against *"mcp isn't new pattern, module is"* and wrote it into the PLAN and the SPEC. This plan deletes it, puts MCP into `saw` in the form of `scout`, and fixes the listed smells at their owners.

## Overview

1. `saw --mcp`: one binary, same code as `scout` (`Main.cpp:100-111`, `:262-263`).
2. A verb stays as fast as today. JAM and JUCE compile their GUI parts only when the GUI modules are linked, as `jam_core` does now: *"what if we have explicit macro to flag CLI/TUI so they can be more SLIM?"* (SPEC:59).
3. Each smell is fixed at its owner, or rejected with the read that refutes it.

**Build ruling:** *"NO FUCKING BUILD. you can validate after all done."*, *"YOU DONT BUILD TO VALIDATTE CODE"*, *"dont audit witohut runtime test"*. No build and no compile check before Step 12. Code is validated by reading. One validation after Step 11: `cast cast/spell.md --no-sign`, foreground, by Pathfinder.

**Endpoint:** the Step 13 report. No log and no commit message before ARCHITECT's word.

## Language / Framework Constraints

- One object, one translation unit. Generated files come from cast only. No comment and no doxygen before Step 12.
- JAM module declarations come from `jam/user-modules-info.md` through `jam/cast/spell.md`.

## Framework Inventory

### API

| Need | Module | API | Source | Step |
|---|---|---|---|---|
| MCP server over stdio | jam_model_context_protocol | `jam::ModelContextServer` | `jam_ModelContextServer.h:11-63` | 4 |
| Application and message loop | juce_events | `juce::JUCEApplicationBase::main`, `createInstance` | `juce_ApplicationBase.cpp:235-281` | 4 |
| Message loop with no AppKit | juce_events | JUCE's POSIX loop `InternalMessageQueue`, `InternalRunLoop`, `runDispatchLoop` | `juce_Messaging_linux.cpp:39-402`; `juce_MessageManager.cpp:95-161` | 2 |
| Input readiness with no main queue | jam_core | `jam::AsyncUpdater::Source`, null context gives a worker queue | `jam_AsyncUpdater.cpp:15-18`, `:131-144` | 2 |
| Command line | saw | `CommandLine::run` | `CommandLine.cpp:96` | 4 |
| Binary data | saw | `juce_add_binary_data (${PROJECT_NAME}_BinaryData)` | `cmake.cast:274-280` | 4 |
| Digit and whitespace tests | jam_core | `jam::Document::isDigit`, `isWhitespace` | `jam_Document.h:1263`, `:1284` | 7 |
| Literal search | jam_core | `jam::LiteralSet::create`, `findNext` | `jam_LiteralSet.h:48`, `:59` | 7 |
| Parsed pattern tree | jam_core | `jam::RegExp::Node` | `jam_RegExp.h:53-96` | 10 |
| Replace a file on Windows with its attributes | juce_core | `juce::File::replaceFileIn` | `juce_File.cpp:323-336` | 10 |

### Pattern usage

| Pattern | Exemplar | Use sites | Step |
|---|---|---|---|
| `--mcp` dispatch in `main` | `scout/Source/Main.cpp:95-111`, `:262-263` | `whatdbg/Source/Main.cpp:221-231`, `:262-263`; `ling/Source/Main.cpp:302` | 4 |
| `ToolServer` is a `JUCEApplicationBase` that owns the server | `scout/Source/ToolServer.h:16`, `:159` | `whatdbg/Source/ToolServer.h:18`, `:150` | 4 |
| `tools.json` in `Source/` as binary data | `scout/Source/tools.json` | `whatdbg`, `ling` | 4 |
| GUI part compiles only with its module | `jam_core.h:52-60`; row `jam_core` depends on `juce_core` only (`user-modules-info.md:50`) | 30 guards in `jam_core` | 3 |
| Feature macro in a JUCE patch, default keeps every project | `juce-posix-file-hook.patch:10-24` | `project-info.md:213` | 2 |
| Settings read one time into the object that runs them | `Search.h:6-22`, `Search.cpp:133-147` | `MatchWriter` | 8 |
| Native part behind `struct Native` and `unique_ptr` | `jam_AsyncOutputStream.h:80-82`, `jam_TerminalFIFO.h:217-234` | `AsyncUpdater`, `Source`, `Work` | 6 |

### Names

| Family | Members | Intent | Step |
|---|---|---|---|
| JUCE patches | `juce-posix-file-hook`, `juce-sha256-hook`; new `juce-posix-event-hook` | one patch, one hook | 2 |
| Option identifiers | `helpOption`, `versionOption`; new `mcpOption` (as `whatdbg/cast/identifiers.md:77`) | option word | 4 |
| `TextDocument` status set | `getBytesStatus`, `getReadFailure`, `getBinaryFailure`; new `getBinaryStatus` | returns a result | 7 |
| `apply` family | `applyMessages`, `applyPendingData`; new `TextDocument::applyToLines` | takes a callable and calls it | 7 |
| `Stdio` failure set | `getFailure`; `getFirstFailure` moved from `Search.cpp:122` | returns the failure | 7 |
| `Arguments` integers | `getInteger (option, …)`; new overload `getInteger (std::string_view)` | returns the number | 7 |
| Native part | `struct Native` | platform half | 6 |
| `jam::File` | `moveFileTo`, `swapFiles`, `isSwappable`; new `replaceFileIn`, `isExecutable` (the JUCE name, the `Writer` name) | file calls | 10 |
| `jam::RegExp` | `getNumCaptures`; `getRequiredLiterals` moved from `Pattern.cpp:160` | returns from the tree | 10 |
| Text structs | `text::Diagnostics`; new `text::Formats` | format strings | 11 |

### Synthesis

| Rule | Established form | Exemplar |
|---|---|---|
| *"Use the framework API fully"* | in-house MCP as it is | `scout/Source/ToolServer.cpp:31-42` |
| *"Uniform signature in the type"* | the verb table, one signature for all verbs | `CommandLine.cpp:14-23` |
| *"No fake-carrier structs"* | settings as members of the object that runs them | `Search.h:149-160` |
| *"No non-owning naked pointers as members"* | view from the owner on demand | `Input.h:87` |

### Absence

| Need | Query | Result |
|---|---|---|
| Map of standard input | `juce_MemoryMappedFile.h:72-96` | file paths only; the map stays, its owner becomes RAII (Step 6) |
| UTF-8 printf formatting | SPRINT-LOG Sprint 1, `juce_String.cpp:1848` | `formattedRaw` encodes non-ASCII text wrong; `Stdio::getFormatted` stays (smell 29 rejected) |
| Wildcard match on a view | `juce_String.h:513` | needs a `juce::String` for each file; a copy in transit; `matchesWildcard` stays (smell 28 rejected) |
| Help frame with no GUI module | RFC:245, `jam_TerminalPage` | the build-time frames stay; `saw_HelpFrames` is a build tool and is not installed |

**Smells rejected by read:** 5 (the swap-back failure is returned as the rollback failure, `Transaction.cpp:30-34`, `Undo.cpp:281-289`; a record that cannot be moved back stays listed, SPEC:793; git also drops an invalid pattern), 8 (a view is not a copy; the text lives in `runTool` for the whole run), 9 and 10 (the folder and the input are arguments of the call; one signature in the verb table), 24 (two uses, one line each, replaced by the JAM test in Step 7 where the semantics match), 26 (one owner, `getEntryShapeKey`, read three times), 33 (value replacement in place, CODING exception), 34 (settings, the `Search` form).

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against
MANIFESTO.md (BLESSED), NAMES.md, ~/.carol/CODING.md, and the locked PLAN decisions
(no deviation, no scope drift). @Auditor runs ONCE, after the final step, covering
the whole sprint — never per step.

## Steps

### Step 1: Delete the garbage
**Scope:** saw `server/Main.cpp`, `PLAN-live-to-premise.md`, `cast/cmake.cast`, `cast/spell.md:89`, `:97`, `project-info.md:148-192`, `SPEC.md`, `CLAUDE.md`, `carol/SPRINT-LOG.md`; `~/.local/bin/saw_ToolServer`, `saw_ToolServer.old`, `saw.old`
**Action:** COUNSELOR writes this plan to `PLAN-native-mcp.md` and deletes `PLAN-live-to-premise.md`. Engineer deletes the target (`cmake.cast:188-190`, `:282-332`, and the `_ToolServer` words of `:337`, `:353`, `:385`, `:400`, `:409-413`), its two tables, its two spell rows, `server/Main.cpp` and the installed files. COUNSELOR edits the SPEC: `:63`, `:78`, `:768-780`, `:903`, `:906`, `:929`, `:977` (one binary, `--mcp`, ARCHITECT's words); `CLAUDE.md:21`; and the Sprint 2 log entry: no ✅, reopened, lines 161-166 corrected.
**Inventory:** none
**Validation:** no `saw_ToolServer` outside the log history and the RFC.

### Step 2: JUCE message loop with no AppKit
**Scope:** jam `patch/juce-posix-event-hook.patch` (new), its row in the patch table of `user-modules-info.md` after line 1424
**Action:** With `JUCE_USE_FOUNDATION=0` on macOS, `juce_events` compiles JUCE's own POSIX loop in place of `juce_MessageManager_mac.mm`: conditions at `juce_events.h:110`, `juce_events.cpp:57-118`, `juce_MessageManager.cpp:95`, `juce_ApplicationBase.cpp:194`, `:242`, `juce_ScopedLowPowerModeDisabler.cpp:38`; the native headers of `juce_core` give `poll`, `socketpair` and `sigaction`. No line of the loop is written again. Default 1 keeps every project.
**Inventory:** API loop, readiness; Pattern feature macro
**Validation:** `git apply --check` against the canonical JUCE copy in scratch; default preprocessed code unchanged.

### Step 3: Slim JAM modules
**Scope:** jam `user-modules-info.md` rows `jam_data_structures`, `jam_terminal`; `jam_data_structures/{jam_data_structures.h,model/jam_Model.h,model/jam_ModelUtils.cpp}`; `jam_terminal/{jam_terminal.h,jam_terminal.cpp,jam_terminal.mm,processor/jam_TerminalFIFO.h,processor/jam_TerminalFIFO.cpp}`
**Action:** Guards `#if JUCE_MODULE_AVAILABLE_juce_gui_basics` in the `jam_core` form. `jam_data_structures`: the `juce::Component` parts of `Model` (`jam_Model.h:74-234`, `:364-401`, `:569-619`, `:653-717`; `jam_ModelUtils.cpp:157-236`) and the GUI includes. `jam_terminal`: everything except `TerminalClock`, `AsyncOutputStream`, `TerminalFIFO`, `StdioServer`, `StdioClient`, `StdioProtocol`; the two `TerminalTTY` members of `TerminalFIFO`; the generated vocabulary `.cpp`, so that no initializer runs. Declared dependencies: `jam_data_structures` = `jam_core`, `juce_data_structures`; `jam_terminal` = `juce_core`, `jam_core`. Each consumer (`cast`, `end`, `eve`, `ggwp`, `ling`, `sandbox`, `scout`, `stamp`, `tit`, `whatdbg`, `whelmed`) is read against the removed dependencies. A missing row goes into its `## juce module` or `## user module` table.
**Inventory:** Pattern module guard
**Validation:** guards only; no body changes; each consumer still lists every module it had.

### Step 4: `saw --mcp`
**Scope:** saw `Source/Main.cpp`, `Source/ToolServer.{h,cpp}` and `Source/tools.json` (moved from `server/`), `cast/identifiers.md`, `cast/text.md`, `cast/cmake.cast:274-280`, `project-info.md` (`program module`: `juce_data_structures`, `juce_events`, `jam_data_structures`, `jam_terminal`, `jam_model_context_protocol`; `user module`: `jam_model_context_protocol`), `Source/HELP.md`, `test/run.lua`, `benchmark/matrix-rows.sh`
**Action:** `createToolServer` and `runToolServer` move into `Main.cpp`, behind `Id::mcpOption` as the only argument, as in `scout`. The words of `ToolServer.cpp:4-12` move to the vocabulary and text tables. `tools.json` joins `_BinaryData`. HELP gets the row of `scout/Source/HELP.md:15`. The runner and the row script start `saw --mcp`; the lane word of the runner is `mcp`.
**Inventory:** API server, application, command line, data; Patterns 1-3; Names `mcpOption`
**Validation:** line for line against `scout`; no new name.

### Step 5: Runtime check (runs in Step 12, after all code steps)
**Scope:** saw, jam
**Action:** Pathfinder keeps a copy of today's `saw` in scratch and builds. Then: `saw --mcp` answers `server/discover`, `tools/list` and `tools/call`, and ends at end of input; the runner, CLI then `mcp`, one after the other: 173 PASS each; `otool -L` shows `libc++` and `libSystem` only; one scout run: `findLiteral` 1K, `lines` 1K, `findLiteral` tree, `mcp findLiteral` 1K, each against today's copy and its reference. Only these rows. A verb that is slower than today's copy is read, fixed and timed again in this step. The consumers of Step 3 are read, not built: each one lists the modules itself.
**Inventory:** all of Steps 2-4
**Validation:** the results above.

### Step 6: Ownership (smells 6, 7)
**Scope:** saw `Source/TextDocument.{h,cpp}`, `Source/Input.{h,cpp}`, `Source/Undo.cpp:172`
**Action:** `TextDocument` keeps `input` only: `ownedBytes`, `data`, `size` and `fromString` are deleted; a null input is the empty document. The standard-input map goes into `Input::Native`, its constructor maps and its destructor unmaps; `mappedAddress`, `mappedSize`, `~Input` body and the header `#ifdef` are deleted.
**Inventory:** Pattern native part; Synthesis no naked pointers
**Validation:** no raw pointer member; no manual release.

### Step 7: One owner for repeated logic (smells 17, 19-22, 25, 27)
**Scope:** saw `Source/{TextDocument,Stdio,Arguments,Paths,Find,Search,Fields,Lines,Replace,Excerpt,PatchValidator,UndoCache}`
**Action:** `TextDocument::applyToLines`, `TextDocument::getBinaryStatus`, `Stdio::getFirstFailure`, `Arguments::getInteger (std::string_view)`; one path classification and one unreadable failure in `Paths`, used by `Find` and `getFiles`; `excerpt` searches with `jam::LiteralSet`; digit tests use `jam::Document::isDigit`.
**Inventory:** API digits, literal search; Names rows 3-6
**Validation:** each listed copy is gone; hot loops keep their SIMD calls.

### Step 8: Settings live in their object (smells 11, 15)
**Scope:** saw `Source/{Fields,Lines,Replace}.{h,cpp}`
**Action:** `Fields`, `Lines` and `Replace` hold their settings as members, read one time from `Arguments`, in the form of `Search`. The static `run` builds the object. The one-caller chains that only pass settings are deleted.
**Inventory:** Pattern settings; Synthesis no fake carrier
**Validation:** no setting passes through a second signature; output unchanged.

### Step 9: One object for a staged file (smells 3, 12, 14, 18)
**Scope:** saw `Source/{Transaction,Undo,UndoCache}.{h,cpp}`
**Action:** The restore and the rollback exist one time, in `Transaction`, and `Undo` uses them. The parallel arrays become one array of the staged-file object that does the commit, the restore and the diff; the 7-field tuples and each `std::get` are deleted. A checksum difference reports `failChecksumMismatch`, not a JSON key.
**Inventory:** Synthesis no fake carrier
**Validation:** SPEC §6 order of operations unchanged; every write case of the runner; no `std::get<` in `Undo.cpp`.

### Step 10: JAM owners (smells 30, 31)
**Scope:** jam `jam_core/text/jam_RegExp.{h,cpp}`, `jam_core/file/jam_File.{h,cpp}`; saw `Source/Pattern.cpp:37-210`, `Source/Writer.{h,cpp}`
**Action:** `getRequiredLiterals` reads the parsed tree in `jam::RegExp`; the second scanner is deleted. `Writer.cpp:73-77` becomes `jam::File::replaceFileIn`; `Writer::isExecutable` becomes `jam::File::isExecutable`.
**Inventory:** API parsed tree, replace; Names `jam::File`, `jam::RegExp`
**Validation:** the same literals for every regex of the cases; no platform branch in `Writer.cpp`.

### Step 11: Walk and form (smells 2, 4, 13, 16, 23, 32, 35-37)
**Scope:** saw `Source/{Paths,Find,Jobs.h,Pattern,IgnoreRules,LineDiff,Writer,Input,Main,StandardStream}`, `cast/identifiers.md`, `cast/text.md`, `SPEC.md:83`, `Input.h`
**Action:** The walk item states its type (folder or batch), and `runRecursive` takes the first items, so the empty-path sentinel is deleted. `runFilesFind` keeps the merge only. `Items::add` is `noexcept`, so a failed allocation ends the program and no worker spins. `JUCE_WINDOWS` is the one platform macro. The regex escape set and the other literals go to the vocabulary, the formats to `text::Formats`. The null-device text names `/dev/null`. `<filesystem>` leaves `Writer.cpp`.
**Inventory:** Names text structs
**Validation:** each smell row is closed at its file:line.

### Step 12: The one validation, audit, doxygen
**Scope:** Steps 1-11
**Action:** One build and the checks of Step 5. When they pass: @Auditor once; each finding is filtered, then resolved; then Pathfinder writes the doxygen of the changed headers, and COUNSELOR reads each block against the code.
**Validation:** 173 PASS on both lanes; no verb slower than today's copy; 0 open findings.

### Step 14: In-house containers, and no temporary container (runs before the last build of Step 12)
**ARCHITECT:** *"std::vector<std::pair<std::string, juce::File>> why using vector? not jam::Array?"*, *"and there should not be ANY temp containers at all @carol/MANIFESTO.md /go"*
**Scope:** saw `Source/` (not `Source/generated`)
**Action:** Each container is a creation, a state update, or transit (MANIFESTO **E**, *The diagnostic*). A container that stages, copies or joins complete data is deleted. A container that stays is `jam::Array` (`jam_Array.h:15-28`); `std::vector` stays only where `jam::LiteralSet::create` and `RegExp::getRequiredLiterals` take or give it.
1. `Arguments`: the option lists are `std::initializer_list`; the values and positionals are views into the argument list, held one time, for each option; a caller gets a `juce::Span`, so `rawPaths`, pattern lists and glob lists are not copied.
2. `Stdio::Output` holds blocks (`Stdio::Blocks`: display path and text of each file). No verb joins its blocks. `Stdio::write` writes them through one fixed buffer (permitted copy 1). `ToolServer` appends them to its JSON.
3. `Paths`: one entry holds the display path, the file and the explicit flag; `explicitSet` and the flag array are deleted. A folder listing is published one time and its entry ranges are the items of the walk; `pendingNames` and the name copies of `FileBatch` are deleted.
4. `Find`: one gather of the worker slots into the blocks and the failures; `printedMessages` and the two merges are deleted.
5. `LineDiff`: the planned text is created in one pass; a change holds ranges of it. `getLines`, `getLinesPreservingEmpty`, `insertedLines`, `getDiffChanges` and the hunk array are deleted. `Replace`, `Lines` and `Patch` append to the planned text.
6. `Transaction` owns the duplicate-target test; the `std::set` of `Lines` and `Replace` is deleted. `PatchValidator` reads the JSON in place: the key lists are constants, `seenKeys` is deleted.
7. `IgnoreRules::create` recurses; the ancestor list is deleted. `UndoCache::getTransactionIds` sorts one time; the reversed copy is deleted. `Pattern::literal` (a second copy of the text in `LiteralSet`) becomes its size.
**Inventory:** API `jam::Array`, `juce::Span`; Synthesis MANIFESTO **E** (*no temporary container*)
**Validation:** each deleted container is gone (`saw find`); the runner passes on both lanes; no row slower than before the step.

### Step 15: JAM text and containers, everywhere (runs before Step 13)
**ARCHITECT:** *"jam::Strings is direct replacement of juce::StringArray, a container to hold string"*, *"jam::Format is direct replacement of juce::String API, because when dont communicate directly with juce native API, we always either using std::string/string_view"*, *"why handroll, copy, convert, half assed framework usage"*, *"I DONT GIVE A FUCK IF BLAST RADIUS IS 100 files with 1000 functions need to be refactored. why the fuck CONTRACT should be violated?"*
**Scope:** saw `Source/` (not `Source/generated`), `cast/text.md`
**Action:**
1. Text is `std::string` or `std::string_view`, built with `jam::Format` (sibling form: `ling/Source/Main.cpp:38-55`, `:136-146`, `tit/Source/Main.cpp:17-34`). `juce::String` appears only at a JUCE or JAM call that needs it.
2. A list of strings is `jam::Strings` (sibling form: `cast/Source/Writer.cpp:18`, `jam_JsonValidator.cpp:22`). `Blocks`, the string pairs, and the joins are deleted. The text of one file or one chunk is one `std::string`, built with `jam::Format` and moved into its `TextDocument` (Step 18). A first form put this one text into a `jam::Strings`. The scout rows of that form: `find -r .` on 1 MB 4138 µs before and 7769 µs after, `lines 1:` on 1 MB 3480 µs before and 5052 µs after.
3. `Stdio::getFormatted`, `getLine` and the printf texts are deleted. `cast/text.md` holds text pieces.
4. Each handroll goes to its JAM owner: `jam::Utf8::isValid`, `jam::Format::trim`, `jam::Document::isWhitespace`, `jam::Strings::fromView` and `getView`, `jam::Format::appendUtf8`.
**Inventory:** API `jam::Strings`, `jam::Format`, `jam::Utf8`, `jam::Document`; Synthesis CODING (*Use the framework API fully*), MANIFESTO **E**
**Validation:** no `getFormatted`, `toStdString`, `isValidString`, `juce::String::fromUTF8`, `Blocks` in `Source/`; the runner passes on both lanes with identical bytes; the speed rows, with the rows against grep, sed, awk and rg.

### Step 16: Code smells, start cost, audit, doxygen, handoff, log (runs before Step 13)
**ARCHITECT:** *"fix all code smells"*, *"fix loses at 1k-100k, must win all"*, *"/audit"*, *"comprehensive doxygen pass"*, *"ensure machinist handoff ~/.carol/ saw enforcement updated and thorough"*, *"/log"*, *"scrap comparison with old data"*, *"NON NEGOTIABLE, NO EXCEPTIONS DESIGN BY CONTRACT"*
**Scope:** saw `Source/` (not `Source/generated`), `cast/identifiers.md`; jam `patch/juce-child-process-hook.patch` (new) and its row, `jam_core/text/jam_RegExp.{h,cpp}`; `~/.carol/HANDOFF-saw-enforcement.md`
**Start cost (scout, one session, µs):** empty program of ours 1947; 124 `libSystem` imports +338; 141 `libc++` imports +100 to +200; initializers 82; verb 37; `saw lines` 2456. `sed` 1K 1923 and 1773, `grep` 1K 1814 and 1879 in the same run. The signature costs nothing measurable (`cast` 6931 signed, 6887 ad hoc).
**Action:**
1. Regression: the binary has 41 initializers. 38 of them are one guard of `juce::ChildProcessManager::singletonHolder`, one in each translation unit, because the header declares the singleton inline (`juce_ChildProcessManager.h:60`). The patch declares it with the JUCE macro that has a `.cpp` definition.
2. Search side: one sort in `getFailureDiagnostics`; the outcome of a search job is an object (`FindOutcome`), not a tuple of six; `getSortedText` and the writer output do not mutate under a `get` name; the first line number of each chunk is computed one time; one reserve for each writer; one owner for the regex escape (`jam::RegExp::appendEscaped`); `Rule` owns its expression; the parse names join the verb set; one byte is appended with `appendByte`; `isLiteral` states its meaning.
3. Write side: one owner for the four change shapes (`LineDiff`); the replacement template is parsed one time; the first candidate is searched one time; the swap is one operation (`StagedFile::swap`); the line lengths are read from the bytes; the UTC calendar is computed one time; the patch order is computed one time; the line endings are named; the path is viewed, not copied.
**Rejected by read:** gmtime (the framework API; a calendar of our own is a handroll, CODING *Use the framework API fully*); JSON numbers (a documented JAM parameter, `jam_JsonDocument.h:111`, `:132`); `AsyncOutputStream` and the `Model` timer in the server (the in-house form of `scout`, ARCHITECT: *"we have 3 working pattern"*); one text for each file or chunk (Step 15 item 2, measured); fields tokens and capture arrays (creation has no limit, MANIFESTO **E**); three strings for each path (three values, not one); `Lines` reading a file of up to 256 KB (SPEC:82); undo record read after validation (trust of an established invariant, MANIFESTO **D**); `Folder` and `Listing` (published values of the walk; a struct would be a fake carrier); `runLines` length and the null-pointer modes of `Pattern` (a number alone, and the presence of a capability); the backtracker before the program (no measurement supports a change); named literals inside one function (CODING *Narrowest scope that holds every use*).
**Inventory:** API `JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL`, `JUCE_IMPLEMENT_SINGLETON` (`juce_Singleton.h:225`, `:273`), `jam::Format::appendByte` (`jam_Format.h:424`), `Stdio::getFirstFailure`; Pattern JUCE patch; Names `FindOutcome`, `StagedFile::swap` (`jam::File::swapFiles`), `removeOutput` (`Jobs::Items::removeNext`), `appendEscaped` (`JsonWriter::appendEscaped`), `LineDiff::getDeletion`, `getReplacement`, `getInsertion`
**Validation:** one build, 182 PASS on both lanes, the initializer count, the affected rows against `sed`, `awk`, `grep`, `rg`; the audit; the doxygen read; the handoff; the log.

### Step 17: Start and write as the references do
ARCHITECT: *"fix loses at 1k-100k, must win all"*, *"YOU CAN NEVER ARGUE the spec cannot hold"*, *"you know exactly what we're doing differently"*.
**Scope:** jam `jam_core/json/jam_JsonWriter.{h,cpp}` (done, build 11), `jam_core/text/jam_Format.{h,cpp}`, `jam_data_structures/model/jam_Model.{h,cpp}`, `jam_terminal/display/jam_StdioServer.{h,cpp}`, `jam_terminal/jam_terminal.{h,cpp}`, `patch/juce-posix-event-hook.patch`; saw `project-info.md:308`
**Action:**
1. `JsonWriter::appendEscaped` finds the next byte to escape with `simd::findFingerprint`. Ripgrep escapes with one pass (`json.rs:549-560`).
2. With `JUCE_USE_APPKIT` 0 on macOS, the standard input of the server is read on JUCE's POSIX loop: `juce::LinuxEventLoop::registerFdCallback` (`juce_EventLoop_linux.h:51`). The dispatch source, its thread and its clock leave the binary. `poll` reports the end of a regular file, so `saw --mcp < file` ends.
3. With `JUCE_USE_APPKIT` 0 on macOS, `jam::Model` has no flush timer, because the input is on the message thread and each attachment flushes there. `juce::Timer` and `juce::Thread` leave the binary.
4. With `JUCE_USE_APPKIT` 0 on macOS, the patch switches off `JUCE_HANDLE_MULTIPLE_INSTANCES`, as on iOS (`juce_ApplicationBase.cpp:89`).
5. `Format::appendDecimal` of a double writes with the C locale call of the platform in `jam_Format.cpp`. `std::num_put`, `std::ostream` and the locale code leave the binary.
6. Full link-time optimization, as ripgrep (`Cargo.toml`, `release-lto`).
**Inventory:** API `registerFdCallback`, `unregisterFdCallback`, `simd::findFingerprint`; Pattern `JUCE_USE_APPKIT` (`juce-posix-event-hook.patch:40-50`); Names none new
**Validation:** one build, 182 PASS on both lanes, the import count, the rows.

### Step 18: No copy in transit (done, build 14; the plan `PLAN-document-output.md` is complete and deleted)
ARCHITECT: *"why still copying? @MANIFESTO.md 0 rules fix all violation"*, *"why not the AST just using jam::Document"*, *"i have never approved copying"*.

### Step 13: Report
**Action:** The numbers, each smell with its file:line or its rejection, each row that still loses to Apple's tools, and the entry for ARCHITECT: `"saw": { "command": "saw", "args": ["--mcp"] }`.

## Verification

Steps 5 and 12. Windows is read, not run; ARCHITECT builds it.

## BLESSED Alignment

- **B:** one owner for each byte and each map; no raw pointer member.
- **L:** one binary; no settings chain.
- **E (Explicit):** no sentinel; every literal is named; failures say what failed.
- **S (SSOT):** one copy of each rule; one regex parser.
- **S (Stateless):** each MCP call carries its folder, arguments and input.
- **E (Encapsulation):** the in-house MCP form, unchanged; platform code in JAM and the patch lane.
- **D:** CLI and `--mcp` give the same bytes from the same code.

## Risks / Open Questions

None. The message-loop patch is the part with no measurement yet. Step 12 measures it.
