local sawBinary = arg[1]
assert (sawBinary ~= nil and #sawBinary > 0, "usage: nvim --headless -l test/run.lua <saw-binary>")
sawBinary = vim.fn.fnamemodify (sawBinary, ":p")

local testDir = vim.fn.fnamemodify (arg[0], ":p:h")
local casesPath = vim.fs.joinpath (testDir, "cases.md")
local fixtureRoot = vim.fs.joinpath (testDir, "fixture")
local stdinRoot = vim.fs.joinpath (testDir, "stdin")
local baselineRoot = vim.fs.joinpath (testDir, "baseline")

local directoryMode = 493
local expectedHeaders = { "name", "fixture", "command", "stdin", "exit", "reference", "compare" }

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

    local step = {
        fixture = cells[2],
        command = cells[3],
        stdin = cells[4],
        exit = exitCode,
        reference = cells[6],
        compare = cells[7],
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
    assert (#offsets == #expectedHeaders + 1, "expected 7 columns in cases table")

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

local function compareReferenceStdout (step, stepIndex, sawResult, refResult)
    if refResult.code ~= step.exit then
        return false, "step " .. tostring (stepIndex) .. " reference exit: expected " .. tostring (step.exit) .. ", got " .. tostring (refResult.code)
    end

    if (sawResult.stdout or "") ~= (refResult.stdout or "") then
        return false, "step " .. tostring (stepIndex) .. " stdout differs from reference"
    end

    return true, nil
end

local function compareReferenceResult (step, stepIndex, tempRoot, sawResult, refResult)
    if step.compare == "stdout" then
        return compareReferenceStdout (step, stepIndex, sawResult, refResult)
    end

    assert (step.compare:sub (1, 5) == "file:", "unsupported compare mode: " .. step.compare)
    local relPath = step.compare:sub (6)

    if readBinaryFile (vim.fs.joinpath (tempRoot, relPath)) ~= (refResult.stdout or "") then
        return false, "step " .. tostring (stepIndex) .. " file differs from reference: " .. relPath
    end

    return true, nil
end

local function runReferenceStep (step, stepIndex, initialFixture, stdinBytes, tempRoot, sawResult)
    local refRoot = makeTempDirectory ()

    local ok, matched, reason = pcall (function ()
        copyFixture (initialFixture, refRoot)
        local refResult = vim.system (splitCommand (step.reference), {
            stdin = stdinBytes,
            text = false,
            cwd = refRoot,
            clear_env = true,
            env = { LC_ALL = "C", PATH = os.getenv ("PATH") or "", HOME = os.getenv ("HOME") or "" },
        }):wait ()

        return compareReferenceResult (step, stepIndex, tempRoot, sawResult, refResult)
    end)

    vim.fn.delete (refRoot, "rf")
    assert (ok, matched)
    return matched, reason
end

local function runStep (caseEntry, stepIndex, initialFixture, tempRoot)
    local step = caseEntry.steps[stepIndex]

    if stepIndex > 1 and step.fixture ~= "" and step.fixture ~= initialFixture then
        copyFixture (step.fixture, tempRoot)
    end

    local sawCmd = { sawBinary }
    local sawArgs = splitCommand (step.command)
    for index = 1, #sawArgs do sawCmd[#sawCmd + 1] = sawArgs[index] end

    local stdinBytes = step.stdin ~= "" and assert (readBinaryFile (vim.fs.joinpath (stdinRoot, step.stdin)), "missing stdin file: " .. step.stdin) or ""
    local sawResult = vim.system (sawCmd, { stdin = stdinBytes, text = false, cwd = tempRoot }):wait ()

    if sawResult.code ~= step.exit then
        return false, "step " .. tostring (stepIndex) .. " exit: expected " .. tostring (step.exit) .. ", got " .. tostring (sawResult.code)
    end

    if step.reference == "" then
        return runBaselineStep (caseEntry, stepIndex, tempRoot, sawResult)
    end

    return runReferenceStep (step, stepIndex, initialFixture, stdinBytes, tempRoot, sawResult)
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

    local lastStep = caseEntry.steps[#caseEntry.steps]

    if lastStep.reference == "" then
        return compareTrees (tempRoot, getExpectedFilesDir (caseEntry.name, initialFixture))
    end

    return true, nil
end

local function runCase (caseEntry)
    local tempRoot = makeTempDirectory ()
    local ok, matched, reason = pcall (executeCase, caseEntry, tempRoot)
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

local appDataSaw = vim.fn.has ("win32") == 1
    and vim.fs.joinpath (os.getenv ("APPDATA") or "", "saw")
    or vim.fs.joinpath (os.getenv ("HOME") or "", "Library", "Application Support", "saw")
local undoDir = vim.fs.joinpath (appDataSaw, "undo")
local savedUndoDir = vim.fs.joinpath (appDataSaw, "undo-runner-" .. tostring (vim.uv.os_getpid ()))
local hadUndo = false

if vim.uv.fs_stat (undoDir) ~= nil then
    assert (vim.uv.fs_rename (undoDir, savedUndoDir))
    hadUndo = true
end

local ok, resultOrError = xpcall (runAll, debug.traceback)

if vim.uv.fs_stat (undoDir) ~= nil then
    vim.fn.delete (undoDir, "rf")
end

if hadUndo then
    assert (vim.uv.fs_rename (savedUndoDir, undoDir))
end

if not ok then
    print (resultOrError)
    os.exit (1)
end

os.exit (resultOrError and 0 or 1)
