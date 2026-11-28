local sawBinary = arg[1]
assert (sawBinary ~= nil and #sawBinary > 0, "usage: nvim --headless -l test/run.lua <saw-binary> [mcp]")
sawBinary = vim.fn.fnamemodify (sawBinary, ":p")

local isMcpLane = arg[2] == "mcp"

local mcpProtocolVersion = "2026-07-28"
local mcpProtocolVersionMember = "io.modelcontextprotocol/protocolVersion"
local mcpClientCapabilitiesMember = "io.modelcontextprotocol/clientCapabilities"
local serverToolName = "run"

local testDir = vim.fn.fnamemodify (arg[0], ":p:h")
local casesPath = vim.fs.joinpath (testDir, "cases.md")
local fixtureRoot = vim.fs.joinpath (testDir, "fixture")
local stdinRoot = vim.fs.joinpath (testDir, "stdin")
local baselineRoot = vim.fs.joinpath (testDir, "baseline")

local directoryMode = 493
local permissionBits = 511
local expectedHeaders = { "name", "fixture", "command", "stdin", "exit", "reference", "compare", "setup", "check", "parallel", "platform" }

local appDataSaw = vim.fn.has ("win32") == 1
    and vim.fs.joinpath (os.getenv ("APPDATA") or "", "saw")
    or vim.fs.joinpath (os.getenv ("HOME") or "", "Library", "Application Support", "saw")
local undoDir = vim.fs.joinpath (appDataSaw, "undo")
local pendingDir = vim.fs.joinpath (appDataSaw, "pending")
local blobDir = vim.fs.joinpath (appDataSaw, "blobs")
local recordSuffix = ".json"

local function readBinaryFile (path)
    local file = io.open (path, "rb")

    if file ~= nil then
        local content = file:read ("*a")
        file:close ()
        return content
    end

    return nil
end

local function readFileOrEmpty (path)
    return readBinaryFile (path) or ""
end

local function makeTempDirectory ()
    local template = vim.fs.joinpath (vim.uv.os_tmpdir (), "saw-test-XXXXXX")
    local created = assert (vim.uv.fs_mkdtemp (template))
    return assert (vim.uv.fs_realpath (created))
end

local function copyFile (srcPath, dstPath)
    local dstStat = vim.uv.fs_lstat (dstPath)

    if dstStat ~= nil and dstStat.type == "link" then
        assert (vim.uv.fs_unlink (dstPath))
    end

    assert (vim.uv.fs_copyfile (srcPath, dstPath))
    local srcStat = assert (vim.uv.fs_lstat (srcPath))
    assert (vim.uv.fs_chmod (dstPath, srcStat.mode))
end

local function copySymlink (srcPath, dstPath)
    local dstStat = vim.uv.fs_lstat (dstPath)

    if dstStat ~= nil then
        assert (vim.uv.fs_unlink (dstPath))
    end

    local linkTarget = assert (vim.uv.fs_readlink (srcPath))
    assert (vim.uv.fs_symlink (linkTarget, dstPath))
end

local copyTree

local entryCopiers = {
    directory = function (srcPath, dstPath) copyTree (srcPath, dstPath) end,
    link = copySymlink,
    file = copyFile,
}

copyTree = function (sourceDir, targetDir)
    if vim.uv.fs_stat (targetDir) == nil then
        assert (vim.uv.fs_mkdir (targetDir, directoryMode))
    end

    local handle = assert (vim.uv.fs_scandir (sourceDir))
    local name, entryType = vim.uv.fs_scandir_next (handle)

    while name ~= nil do
        local srcPath = vim.fs.joinpath (sourceDir, name)
        local dstPath = vim.fs.joinpath (targetDir, name)
        local itemType = entryType or assert (vim.uv.fs_lstat (srcPath)).type
        local copyEntry = entryCopiers[itemType]

        if copyEntry ~= nil then
            copyEntry (srcPath, dstPath)
        end

        name, entryType = vim.uv.fs_scandir_next (handle)
    end
end

local function copyFixture (fixtureName, targetDir)
    if fixtureName ~= "" then
        local fixtureDir = vim.fs.joinpath (fixtureRoot, fixtureName)
        assert (vim.uv.fs_stat (fixtureDir) ~= nil, "missing fixture: " .. fixtureDir)
        copyTree (fixtureDir, targetDir)
    end
end

local function listRelativeEntries (rootDir)
    local entries = {}
    local pending = { { dir = rootDir, prefix = "" } }

    while #pending > 0 do
        local current = table.remove (pending)
        local handle = assert (vim.uv.fs_scandir (current.dir))
        local name, entryType = vim.uv.fs_scandir_next (handle)

        while name ~= nil do
            local fullPath = vim.fs.joinpath (current.dir, name)
            local relativePath = current.prefix == "" and name or (current.prefix .. "/" .. name)
            local itemType = entryType or assert (vim.uv.fs_lstat (fullPath)).type

            if itemType == "directory" then
                pending[#pending + 1] = { dir = fullPath, prefix = relativePath }
            elseif itemType == "file" or itemType == "link" then
                entries[#entries + 1] = { path = relativePath, type = itemType }
            end

            name, entryType = vim.uv.fs_scandir_next (handle)
        end
    end

    table.sort (entries, function (left, right) return left.path < right.path end)
    return entries
end

local function compareEntry (actualRoot, expectedRoot, actualEntry, expectedEntry)
    if actualEntry.path ~= expectedEntry.path then
        return false, "tree path mismatch: expected " .. expectedEntry.path .. ", got " .. actualEntry.path
    end

    if actualEntry.type ~= expectedEntry.type then
        return false, "tree entry type mismatch at " .. actualEntry.path
    end

    local actualPath = vim.fs.joinpath (actualRoot, actualEntry.path)
    local expectedPath = vim.fs.joinpath (expectedRoot, expectedEntry.path)

    if actualEntry.type == "link" then
        local isSameTarget = assert (vim.uv.fs_readlink (actualPath)) == assert (vim.uv.fs_readlink (expectedPath))
        return isSameTarget, isSameTarget and nil or ("symlink target mismatch at " .. actualEntry.path)
    end

    local isSameBytes = readBinaryFile (actualPath) == readBinaryFile (expectedPath)
    return isSameBytes, isSameBytes and nil or ("file content mismatch at " .. actualEntry.path)
end

local function compareTrees (actualRoot, expectedRoot)
    local actualEntries = listRelativeEntries (actualRoot)
    local expectedEntries = expectedRoot ~= nil and listRelativeEntries (expectedRoot) or {}
    local sharedCount = #actualEntries < #expectedEntries and #actualEntries or #expectedEntries

    for index = 1, sharedCount do
        local matched, reason = compareEntry (actualRoot, expectedRoot, actualEntries[index], expectedEntries[index])

        if not matched then
            return false, reason
        end
    end

    if #actualEntries < #expectedEntries then
        return false, "missing entry: " .. expectedEntries[#actualEntries + 1].path
    end

    if #actualEntries > #expectedEntries then
        return false, "unexpected entry: " .. actualEntries[#expectedEntries + 1].path
    end

    return true, nil
end

local function splitCommand (text)
    local args, current, inToken, inQuotes = {}, {}, false, false

    for index = 1, #text do
        local byte = text:sub (index, index)

        if byte == '"' then
            inToken, inQuotes = true, not inQuotes
        elseif not inQuotes and (byte == " " or byte == "\t") then
            if inToken then
                args[#args + 1] = table.concat (current)
                current, inToken = {}, false
            end
        else
            inToken = true
            current[#current + 1] = byte
        end
    end

    assert (not inQuotes, "unterminated quote in command: " .. text)
    if inToken then args[#args + 1] = table.concat (current) end
    return args
end

local function getRowCells (line, offsets)
    assert (#line >= offsets[#offsets], "malformed grid table row")
    local cells = {}

    for column = 1, #offsets - 1 do
        cells[column] = (line:sub (offsets[column] + 1, offsets[column + 1] - 1):match ("^%s*(.-)%s*$"))
    end

    return cells
end

local function addCaseStep (cases, cells)
    assert (#cells[1] > 0, "empty case name in cases.md")
    local exitCode = tonumber (cells[5])
    assert (exitCode ~= nil and cells[5]:match ("^%d+$") ~= nil, "invalid exit code for case " .. cells[1])

    local parallelText = cells[10] == "" and "1" or cells[10]
    assert (parallelText:match ("^[1-9]%d*$") ~= nil, "invalid parallel count for case " .. cells[1])
    local parallelCount = tonumber (parallelText)
    assert (parallelCount == 1 or cells[6] == "", "parallel case cannot compare a reference: " .. cells[1])

    local step = {
        fixture = cells[2],
        command = cells[3],
        stdin = cells[4],
        exit = exitCode,
        reference = cells[6],
        compare = cells[7],
        setup = cells[8],
        check = cells[9],
        parallel = parallelCount,
        platform = cells[11],
    }

    if #cases > 0 and cases[#cases].name == cells[1] then
        local steps = cases[#cases].steps
        steps[#steps + 1] = step
        return
    end

    cases[#cases + 1] = { name = cells[1], steps = { step } }
end

local function parseCases (markdownText)
    local lines = {}
    for line in (markdownText .. "\n"):gmatch ("([^\n]*)\n") do
        lines[#lines + 1] = line:gsub ("\r$", "")
    end

    local start = 1
    while start <= #lines and lines[start]:match ("^%s*##%s+cases%s*$") == nil do start = start + 1 end
    assert (start <= #lines, "missing '## cases' heading in cases.md")

    local offsets = {}
    while start <= #lines and lines[start]:match ("^%+[%-%+]+$") == nil do start = start + 1 end
    assert (start <= #lines, "missing grid table border in cases.md")
    for pos in lines[start]:gmatch ("()%+") do offsets[#offsets + 1] = pos end
    assert (#offsets == #expectedHeaders + 1, "expected " .. #expectedHeaders .. " columns in cases table")

    while start <= #lines and lines[start]:sub (1, 1) ~= "|" do start = start + 1 end
    assert (start <= #lines, "missing header row in cases.md")
    local headers = getRowCells (lines[start], offsets)
    for col = 1, #expectedHeaders do assert (headers[col] == expectedHeaders[col], "unexpected header: " .. headers[col]) end

    local cases = {}
    for index = start + 1, #lines do
        local firstByte = lines[index]:sub (1, 1)
        if firstByte == "|" then addCaseStep (cases, getRowCells (lines[index], offsets))
        elseif firstByte ~= "+" then return cases end
    end

    return cases
end

local function normalizeOutput (bytes, tempRoot)
    local escapedRoot = tempRoot:gsub ("([%(%)%.%%%+%-%*%?%[%]%^%$])", "%%%1")
    bytes = bytes:gsub (escapedRoot, "<root>")
    bytes = bytes:gsub ("%d%d%d%d%d%d%d%dT%d%d%d%d%d%d%-%x%x%x%x%x%x", "<tx-id>")
    return bytes
end

local function makeTreeReadable (rootDir)
    if vim.fn.has ("win32") == 0 then
        local result = vim.system ({ "chmod", "-R", "u+rwX", rootDir }):wait ()
        assert (result.code == 0, "chmod failed: " .. rootDir)
    end

    if vim.fn.has ("mac") == 1 then
        local result = vim.system ({ "chmod", "-R", "-N", rootDir }):wait ()
        assert (result.code == 0, "chmod failed: " .. rootDir)
    end
end

local function countTransactions ()
    if vim.uv.fs_stat (undoDir) == nil then return 0 end

    local count = 0
    local handle = assert (vim.uv.fs_scandir (undoDir))
    local name, entryType = vim.uv.fs_scandir_next (handle)

    while name ~= nil do
        local itemType = entryType or assert (vim.uv.fs_lstat (vim.fs.joinpath (undoDir, name))).type

        if itemType == "file" and vim.endswith (name, recordSuffix) then
            count = count + 1
        end

        name, entryType = vim.uv.fs_scandir_next (handle)
    end

    return count
end

local function getNewestTransactionId ()
    if vim.uv.fs_stat (undoDir) == nil then return "" end

    local newestId = ""
    local handle = assert (vim.uv.fs_scandir (undoDir))
    local name, entryType = vim.uv.fs_scandir_next (handle)

    while name ~= nil do
        local itemType = entryType or assert (vim.uv.fs_lstat (vim.fs.joinpath (undoDir, name))).type

        if itemType == "file" and vim.endswith (name, recordSuffix) and name > newestId then
            newestId = name
        end

        name, entryType = vim.uv.fs_scandir_next (handle)
    end

    return newestId:sub (1, -#recordSuffix - 1)
end

local setupActions = {
    mode = function (rootDir, fields)
        assert (vim.uv.fs_chmod (vim.fs.joinpath (rootDir, fields[3]), tonumber (fields[2], 8)))
    end,
    link = function (rootDir, fields)
        assert (vim.uv.fs_link (vim.fs.joinpath (rootDir, fields[2]), vim.fs.joinpath (rootDir, fields[3])))
    end,
    mkdir = function (rootDir, fields)
        assert (vim.uv.fs_mkdir (vim.fs.joinpath (rootDir, fields[2]), directoryMode))
    end,
    xattr = function (rootDir, fields)
        local targetPath = vim.fs.joinpath (rootDir, fields[4])
        local result = vim.system ({ "xattr", "-w", fields[2], fields[3], targetPath }):wait ()
        assert (result.code == 0, "xattr write failed: " .. targetPath)
    end,
    acl = function (rootDir, fields)
        local targetPath = vim.fs.joinpath (rootDir, fields[3])
        local result = vim.system ({ "chmod", "+a", fields[2], targetPath }):wait ()
        assert (result.code == 0, "acl write failed: " .. targetPath)
    end,
    copy = function (rootDir, fields)
        copyFile (vim.fs.joinpath (rootDir, fields[2]), vim.fs.joinpath (rootDir, fields[3]))
    end,
    pipe = function (rootDir, fields)
        local result = vim.system ({ "mkfifo", vim.fs.joinpath (rootDir, fields[2]) }):wait ()
        assert (result.code == 0, "mkfifo failed: " .. fields[2])
        vim.system ({ "sh", "-c", 'printf "%s\\n" "$1" > "$2"', "sh", fields[3], vim.fs.joinpath (rootDir, fields[2]) })
    end,
}

local checkActions = {
    mode = function (rootDir, fields)
        local stat = assert (vim.uv.fs_stat (vim.fs.joinpath (rootDir, fields[3])))
        return bit.band (stat.mode, permissionBits) == tonumber (fields[2], 8)
    end,
    links = function (rootDir, fields)
        local stat = assert (vim.uv.fs_stat (vim.fs.joinpath (rootDir, fields[3])))
        return stat.nlink == tonumber (fields[2])
    end,
    xattr = function (rootDir, fields)
        local result = vim.system ({ "xattr", "-p", fields[2], vim.fs.joinpath (rootDir, fields[4]) }):wait ()
        return (result.stdout:gsub ("\n$", "")) == fields[3]
    end,
    bytes = function (rootDir, fields)
        local firstBytes = readBinaryFile (vim.fs.joinpath (rootDir, fields[2]))
        return firstBytes ~= nil and firstBytes == readBinaryFile (vim.fs.joinpath (rootDir, fields[3]))
    end,
    transactions = function (rootDir, fields)
        return countTransactions () == tonumber (fields[2])
    end,
}

local function getActionFields (actions, token)
    local fields = vim.split (token, ":", { plain = true })
    local action = actions[fields[1]]
    assert (action ~= nil, "unknown action: " .. token)
    return action, fields
end

local function runSetup (rootDir, setupText)
    local tokens = splitCommand (setupText)

    for index = 1, #tokens do
        local action, fields = getActionFields (setupActions, tokens[index])
        action (rootDir, fields)
    end
end

local function runChecks (stepIndex, checkText, rootDir)
    local tokens = splitCommand (checkText)

    for index = 1, #tokens do
        local check, fields = getActionFields (checkActions, tokens[index])

        if not check (rootDir, fields) then
            return false, "step " .. tostring (stepIndex) .. " check failed: " .. tokens[index]
        end
    end

    return true, nil
end

local platformPredicates = {
    [""] = function () return true end,
    posix = function () return vim.fn.has ("win32") == 0 end,
    mac = function () return vim.fn.has ("mac") == 1 end,
    cli = function () return not isMcpLane end,
}

local function isPlatformApplicable (platformName)
    local predicate = platformPredicates[platformName]
    assert (predicate ~= nil, "unknown platform in cases.md: " .. platformName)
    return predicate ()
end

local function hasNonUtf8Byte (text)
    local utf16Text = vim.iconv (text, "utf-8", "utf-16le")
    return utf16Text == nil or vim.iconv (utf16Text, "utf-16le", "utf-8") ~= text
end

local function decodeToolText (structuredContent, plainName, encodedName)
    local encodedText = structuredContent[encodedName]

    if encodedText ~= nil then
        return vim.base64.decode (encodedText)
    end

    return structuredContent[plainName] or ""
end

local function decodeToolReply (processResult)
    local reply = vim.json.decode (vim.split (processResult.stdout, "\n", { plain = true })[1])
    assert (reply.result ~= nil and reply.result.isError ~= true, "saw server call failed: " .. processResult.stdout)
    local structuredContent = reply.result.structuredContent

    return {
        status = structuredContent.status,
        signal = 0,
        stdout = decodeToolText (structuredContent, "output", "outputBase64"),
        stderr = decodeToolText (structuredContent, "errors", "errorsBase64"),
    }
end

local function startToolCall (sawArgs, stdinBytes, tempRoot)
    local toolArguments = { arguments = sawArgs, directory = tempRoot }

    if stdinBytes ~= nil then
        toolArguments.stdin = stdinBytes
    end

    local request = {
        jsonrpc = "2.0",
        id = 1,
        method = "tools/call",
        params = {
            name = serverToolName,
            arguments = toolArguments,
            _meta = {
                [mcpProtocolVersionMember] = mcpProtocolVersion,
                [mcpClientCapabilitiesMember] = vim.empty_dict (),
            },
        },
    }
    local serverProcess
    local replyBytes = ""

    serverProcess = vim.system ({ sawBinary, "--mcp" }, {
        stdin = true,
        text = false,
        cwd = tempRoot,
        stdout = function (_, data)
            replyBytes = replyBytes .. (data or "")
            if data ~= nil and data:find ("\n", 1, true) ~= nil then serverProcess:write (nil) end
        end,
    })
    serverProcess:write (vim.json.encode (request) .. "\n")

    return {
        wait = function ()
            serverProcess:wait ()
            return decodeToolReply ({ stdout = replyBytes })
        end,
    }
end

local function startSawProcesses (step, stdinBytes, tempRoot)
    local processes = {}
    local newestTransactionId = getNewestTransactionId ()

    for processIndex = 1, step.parallel do
        local commandText = step.command:gsub ("{n}", function () return tostring (processIndex) end)
        commandText = commandText:gsub ("{tx}", function () return newestTransactionId end)
        commandText = commandText:gsub ("{newline}", function () return "\n" end)
        commandText = commandText:gsub ("{xff}", function () return "\255" end)
        local sawArgs = splitCommand (commandText)

        if isMcpLane and not hasNonUtf8Byte (commandText) and (stdinBytes == nil or not hasNonUtf8Byte (stdinBytes)) and step.stdin ~= "{closed}" then
            processes[processIndex] = startToolCall (sawArgs, stdinBytes, tempRoot)
        else
            local sawCmd = step.stdin == "{closed}" and { "sh", "-c", 'exec "$@" <&-', "sh", sawBinary } or { sawBinary }
            for index = 1, #sawArgs do sawCmd[#sawCmd + 1] = sawArgs[index] end
            processes[processIndex] = vim.system (sawCmd, { stdin = stdinBytes, text = false, cwd = tempRoot })
        end
    end

    return processes
end

local function waitSawProcesses (processes)
    local results = {}

    for index = 1, #processes do
        results[index] = processes[index]:wait ()
    end

    return results
end

local statusValues = { [0] = "ok", [1] = "mismatch" }

local function getExpectedStatus (exitCode)
    return statusValues[exitCode] or "error"
end

local function isExpectedExit (step, result)
    if result.status ~= nil then
        return result.status == getExpectedStatus (step.exit)
    end

    return result.code == step.exit
end

local function compareExitCodes (step, stepIndex, results)
    for processIndex = 1, #results do
        if not isExpectedExit (step, results[processIndex]) or results[processIndex].signal ~= 0 then
            return false, "step " .. tostring (stepIndex) .. " process " .. tostring (processIndex) .. " exit: expected " .. tostring (results[processIndex].status ~= nil and getExpectedStatus (step.exit) or step.exit) .. ", got " .. tostring (results[processIndex].status or results[processIndex].code) .. (results[processIndex].signal ~= 0 and (", signal " .. tostring (results[processIndex].signal)) or "")
        end
    end

    return true, nil
end

local function runBaselineStep (caseEntry, stepIndex, tempRoot, sawResult)
    local stepBaselineDir = vim.fs.joinpath (baselineRoot, caseEntry.name, tostring (stepIndex))
    local expectedStdout = readFileOrEmpty (vim.fs.joinpath (stepBaselineDir, "stdout"))
    local expectedStderr = readFileOrEmpty (vim.fs.joinpath (stepBaselineDir, "stderr"))

    if normalizeOutput (sawResult.stdout or "", tempRoot) ~= expectedStdout then
        return false, "step " .. tostring (stepIndex) .. " stdout mismatch"
    end

    if normalizeOutput (sawResult.stderr or "", tempRoot) ~= expectedStderr then
        return false, "step " .. tostring (stepIndex) .. " stderr mismatch"
    end

    return true, nil
end

local function compareReferenceExit (step, stepIndex, refResult)
    if refResult.code ~= step.exit then
        return false, "step " .. tostring (stepIndex) .. " reference exit: expected " .. tostring (step.exit) .. ", got " .. tostring (refResult.code)
    end

    return true, nil
end

local function compareReferenceStdout (step, stepIndex, tempRoot, sawResult, refResult)
    local matchedExit, exitReason = compareReferenceExit (step, stepIndex, refResult)

    if not matchedExit then
        return false, exitReason
    end

    if (sawResult.stdout or "") ~= (refResult.stdout or "") then
        return false, "step " .. tostring (stepIndex) .. " stdout differs from reference"
    end

    return true, nil
end

local function getSortedLinesText (text)
    local lines = vim.split (text, "\n", { plain = true })
    table.sort (lines)
    return table.concat (lines, "\n")
end

local function compareReferenceLines (step, stepIndex, tempRoot, sawResult, refResult)
    local matchedExit, exitReason = compareReferenceExit (step, stepIndex, refResult)

    if not matchedExit then
        return false, exitReason
    end

    if getSortedLinesText (sawResult.stdout or "") ~= getSortedLinesText (refResult.stdout or "") then
        return false, "step " .. tostring (stepIndex) .. " sorted lines differ from reference"
    end

    return true, nil
end

local function compareReferenceFile (step, stepIndex, tempRoot, sawResult, refResult, relPath)
    if readBinaryFile (vim.fs.joinpath (tempRoot, relPath)) ~= (refResult.stdout or "") then
        return false, "step " .. tostring (stepIndex) .. " file differs from reference: " .. relPath
    end

    return true, nil
end

local compareModes = {
    stdout = compareReferenceStdout,
    lines = compareReferenceLines,
    file = compareReferenceFile,
}

local function compareReferenceResult (step, stepIndex, tempRoot, sawResult, refResult)
    local fields = vim.split (step.compare, ":", { plain = true })
    local compareMode = compareModes[fields[1]]
    assert (compareMode ~= nil, "unsupported compare mode: " .. step.compare)
    return compareMode (step, stepIndex, tempRoot, sawResult, refResult, fields[2])
end

local function runReferenceStep (step, stepIndex, initialFixture, stdinBytes, tempRoot, sawResult)
    local refRoot = makeTempDirectory ()

    local ok, matched, reason = pcall (function ()
        copyFixture (initialFixture, refRoot)
        runSetup (refRoot, step.setup)
        local refResult = vim.system (vim.list_extend (step.stdin == "{closed}" and { "sh", "-c", 'exec "$@" <&-', "sh" } or {}, splitCommand ((step.reference:gsub ("{project}", function () return vim.fs.dirname (testDir) end)))), {
            stdin = stdinBytes,
            text = false,
            cwd = refRoot,
            clear_env = true,
            env = { LC_ALL = "C", PATH = os.getenv ("PATH") or "", HOME = os.getenv ("HOME") or "" },
        }):wait ()

        return compareReferenceResult (step, stepIndex, tempRoot, sawResult, refResult)
    end)

    makeTreeReadable (refRoot)
    vim.fn.delete (refRoot, "rf")
    assert (ok, matched)
    return matched, reason
end

local function runStep (caseEntry, stepIndex, initialFixture, tempRoot)
    local step = caseEntry.steps[stepIndex]

    if stepIndex > 1 and step.fixture ~= "" and step.fixture ~= initialFixture then
        copyFixture (step.fixture, tempRoot)
    end

    runSetup (tempRoot, step.setup)

    local stdinBytes = ""

    if step.stdin == "{null}" then
        stdinBytes = nil
    elseif step.stdin ~= "" and step.stdin ~= "{closed}" then
        stdinBytes = assert (readBinaryFile (vim.fs.joinpath (stdinRoot, step.stdin)), "missing stdin file: " .. step.stdin)
    end

    local results = waitSawProcesses (startSawProcesses (step, stdinBytes, tempRoot))

    local matchedExit, exitReason = compareExitCodes (step, stepIndex, results)

    if not matchedExit then
        return false, exitReason
    end

    local matchedChecks, checkReason = runChecks (stepIndex, step.check, tempRoot)

    if not matchedChecks then
        return false, checkReason
    end

    if step.parallel > 1 then
        return true, nil
    end

    if step.reference == "" then
        return runBaselineStep (caseEntry, stepIndex, tempRoot, results[1])
    end

    return runReferenceStep (step, stepIndex, initialFixture, stdinBytes, tempRoot, results[1])
end

local function getExpectedFilesDir (caseName, initialFixture)
    local baselineFilesDir = vim.fs.joinpath (baselineRoot, caseName, "files")

    if vim.uv.fs_stat (baselineFilesDir) ~= nil then
        return baselineFilesDir
    end

    if initialFixture ~= "" then
        return vim.fs.joinpath (fixtureRoot, initialFixture)
    end

    return nil
end

local function executeCase (caseEntry, tempRoot)
    local initialFixture = caseEntry.steps[1].fixture
    copyFixture (initialFixture, tempRoot)

    for stepIndex = 1, #caseEntry.steps do
        local matched, reason = runStep (caseEntry, stepIndex, initialFixture, tempRoot)

        if not matched then
            return false, reason
        end
    end

    makeTreeReadable (tempRoot)
    local lastStep = caseEntry.steps[#caseEntry.steps]

    if lastStep.reference == "" then
        return compareTrees (tempRoot, getExpectedFilesDir (caseEntry.name, initialFixture))
    end

    return true, nil
end

local function runCase (caseEntry)
    if not isPlatformApplicable (caseEntry.steps[1].platform) then
        print ("SKIP " .. caseEntry.name)
        return true
    end

    local tempRoot = makeTempDirectory ()
    local ok, matched, reason = pcall (executeCase, caseEntry, tempRoot)
    makeTreeReadable (tempRoot)
    vim.fn.delete (tempRoot, "rf")
    assert (ok, matched)

    if matched then
        print ("PASS " .. caseEntry.name)
        return true
    end

    print ("FAIL " .. caseEntry.name .. ": " .. reason)
    return false
end

local function runAll ()
    local casesMarkdown = assert (readBinaryFile (casesPath), "missing cases file: " .. casesPath)
    local cases = parseCases (casesMarkdown)
    local allPassed = true

    for index = 1, #cases do
        if not runCase (cases[index]) then
            allPassed = false
        end
    end

    return allPassed
end

local function moveAside (folderPath)
    local savedPath = folderPath .. "-runner-" .. tostring (vim.uv.os_getpid ())

    if vim.uv.fs_stat (folderPath) ~= nil then
        assert (vim.uv.fs_rename (folderPath, savedPath))
        return savedPath
    end

    return nil
end

local isolatedFolders = { undoDir, pendingDir, blobDir }
local savedFolders = {}

for index = 1, #isolatedFolders do
    savedFolders[index] = moveAside (isolatedFolders[index])
end

local ok, resultOrError = xpcall (runAll, debug.traceback)

for index = 1, #isolatedFolders do
    if vim.uv.fs_stat (isolatedFolders[index]) ~= nil then
        vim.fn.delete (isolatedFolders[index], "rf")
    end

    if savedFolders[index] ~= nil then
        assert (vim.uv.fs_rename (savedFolders[index], isolatedFolders[index]))
    end
end

if not ok then
    print (resultOrError)
    os.exit (1)
end

os.exit (resultOrError and 0 or 1)
