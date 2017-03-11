if jit and jit.logger then jit.logger('none') end

do
print('empty loop')
for i = 0, 100 do end
end

print('-----------------------------------------------------------------------')

do
print('basic test with integers')
local a = 0
for i = 1, 100 do a = a + i end
print(a)
for i = 1, 100 do a = a + 1 end
print(a)
end

print('-----------------------------------------------------------------------')

do
print('values that are not phi')
local function f(n)
  local a = 0
  for i = 1, n do a = 123 end
  print(a)
end
f(100)
f(1)
f(0)
end

print('-----------------------------------------------------------------------')

do
print('change index')
local a = 0
for i = 1, 100 do
  i = 10
  a = a + i
end
print(a)
end

print('-----------------------------------------------------------------------')

do
print('change step direction')
local function f(start, limit, step)
  local a = 0
  for i = start, limit, step do a = a + i end
  print(a)
end
f(0, 100, 1)
f(100, 0, -1)
end

