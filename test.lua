if jit and jit.logger then jit.logger('all') end

local a = 0
for i = 1, 100 do a = a + i end
print(a)
