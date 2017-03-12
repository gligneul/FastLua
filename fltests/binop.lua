if jit and jit.logger then jit.logger('none') end

do
print('add r k')
local a = 0
for i = 0, 100 do a = a + 1 end
print(a)
end

do
print('add k r')
local a = 0
for i = 0, 100 do a = 1 + a end
print(a)
end

do
print('add r r')
local a, b = 0, 10
for i = 0, 100 do a = b + a end
print(a)
end

do
print('add r r')
local a, b = 0, 10
for i = 0, 100 do a = b + a end
print(a)
end

do
print('add cast kb')
local a = 0.5
for i = 0, 100 do a = 1 + a end
print(a)
end

do
print('add cast kc')
local a = 0.5
for i = 0, 100 do a = a + 1 end
print(a)
end
