if jit and jit.logger then jit.logger('none') end

function testloadk(k)
  local fun = 'local a = 0; for i = 1, 100 do a = ' .. k .. '; end; print(a)'
  load(fun)()
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

