if jit and jit.logger then jit.logger('none') end

print('basic test with integers')
local a = 0
for i = 1, 100 do a = a + i end
print(a)
for i = 1, 100 do a = a + 1 end
print(a)

--------------------------------------------------------------------------------

print('values that are not phi')
local function f(n)
  local a = 0
  for i = 1, n do a = 123 end
  print(a)
end
f(100)
f(1)
f(0)


