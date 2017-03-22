if jit and jit.logger then jit.logger('none') end

print('testing move')

function testmove(v)
  local f =
    'local a, b = nil, ' .. v .. '\n' ..
    'for i = 1, 100 do a = b end\n' ..
    'print(a)'
  load(f)()
end

testmove('nil')
testmove('true')
testmove('false')
testmove('0')
testmove('1')
testmove('0xFFFFFFFFFFFFFFFF')
testmove('0.001')
testmove('123.456')
testmove('0/0')
testmove('"hello"')
-- testmove('{}')
-- testmove('function() end')

-- TODO: test c-functions, threads, uservalues, etc
-- TODO: test type changes that yield a trace exit

--------------------------------------------------------------------------------

print('testing loadk')

function testloadk(k)
  local f =
    'local a\n' ..
    'for i = 1, 100 do a = ' .. k .. ' end\n' ..
    'print(a)'
  load(f)()
end

testloadk('nil')
testloadk('true')
testloadk('false')
testloadk('0')
testloadk('1')
testloadk('0xFFFFFFFFFFFFFFFF')
testloadk('0.001')
testloadk('123.456')
testloadk('0/0')
testloadk('"hello"')

