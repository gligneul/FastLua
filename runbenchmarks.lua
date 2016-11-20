#!/bin/lua

-- Configuration:
local n_runs = 3
local supress_errors = true 

local binaries = {
    { 'lua-5.3.3', 'lua' },
    { 'luajit-2.0.3', 'luajit' },
    { 'fastlua', 'src/lua' },
}

local tests_root = 'benchmarks/'
local tests = {
--[[
    { 'ack', 'ack.lua' },
--]]
---[[
    { 'ack', 'ack.lua 3 10' },
    { 'fixpoint-fact', 'fixpoint-fact.lua 3000' },
    { 'heapsort', 'heapsort.lua 10 250000' },
    { 'mandelbrot', 'mandel.lua' },
    { 'juliaset', 'qt.lua' },
    { 'queen', 'queen.lua 12' },
    { 'sieve', 'sieve.lua 5000' }, -- Sieve of Eratosthenes
---[[
    -- benchmarksgame:
    { 'binary', 'binary-trees.lua 17' },
    { 'n-body', 'n-body.lua 5000000' },
    { 'fannkuch', 'fannkuch-redux.lua 10' },
    { 'fasta', 'fasta.lua 2500000' },
    { 'k-nucleotide', 'k-nucleotide.lua < fasta2500000.txt' },
    { 'regex-dna', 'regex-dna.lua < fasta2500000.txt' },
    { 'spectral-norm', 'spectral-norm.lua 2000' },
--]]
}

-- Runs the binary a single time and returns the time elapsed
local function measure(binary, test)
    local cmd = binary .. ' ' .. test
    local time_cmd = '{ TIMEFORMAT=\'%3R\'; time ' ..  cmd ..
            ' > /dev/null; } 2>&1'
    local handle = io.popen(time_cmd)
    local result = handle:read("*a")
    local time_elapsed = tonumber(result)
    handle:close()
    if not time_elapsed then
        error('Invalid output for "' .. cmd .. '":\n' .. result)
    end
    return time_elapsed
end

-- Runs the binary $n_runs and returns the fastest time
local function benchmark(binary, test)
    local min = 999
    for _ = 1, n_runs do
        local time = measure(binary, test)
        min = math.min(min, time)
    end
    return min
end

-- Measures the time for each binary and test
-- Returns a matrix with the result (test x binary)
local function run_all()
    local results = {}
    for _, test in ipairs(tests) do
        io.write('running "' .. test[1] .. '"... ')
        local rline = {}
        for j, binary in ipairs(binaries) do
            local time = 0
            local ok, msg = pcall(function()
                local test_path = tests_root .. test[2]
                time = benchmark(binary[2], test_path)
            end)
            if not ok and not supress_errors then
                io.write('error:\n' .. msg .. '\n---\n')
            end
            table.insert(rline, time)
        end
        table.insert(results, rline)
        io.write('done\n')
    end
    return results 
end

-- Normalize the results given the base_index
local function normalize_results(results, base_index)
    for _, line in ipairs(results) do
        local base = line[base_index]
        for i = 1, #line do
            line[i] = line[i] / base
        end
    end
end

-- Creates and saves the gnuplot data file
local function create_data_file(results, fname)
    local data = 'test\t'
    for _, binary in ipairs(binaries) do
        data = data .. binary[1] .. '\t'
    end
    data = data .. '\n'
    for i, test in ipairs(tests) do
        data = data .. test[1] .. '\t'
        for j, _ in ipairs(binaries) do
            data = data .. results[i][j] .. '\t' 
        end
        data = data .. '\n'
    end
    local f = io.open(fname, 'w')
    f:write(data)
    f:close()
end

-- Generates the output image with gnuplot
local function generate_image(results, suffix)
    suffix = suffix or ''
    local basename = 'benchmark_out' .. suffix
    local data_fname = basename .. '.txt'
    local image_fname = basename .. '.png'
    create_data_file(results, data_fname)
    os.execute('gnuplot -e "datafile=\'' .. data_fname .. '\'" ' ..
                       '-e "outfile=\'' .. image_fname .. '\'" ' ..
                       'benchmarks/plot.gpi')
end

local function setup()
    os.execute('luajit benchmarks/fasta.lua 2500000 > fasta2500000.txt')
end

local function teardown()
    os.execute('rm fasta2500000.txt')
end

local function main()
    setup()
    local results = run_all()
    teardown()
    generate_image(results)
    normalize_results(results, 1)
    generate_image(results, '_normalized')
    print('final done')
end

main()
