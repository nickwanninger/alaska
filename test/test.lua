Fibonacci = {}

-- Naive recursive
function Fibonacci.naive(n)
  local function inner(m)
    if m < 2 then
      return m
    end
    return inner(m-1) + inner(m-2)
  end
  return inner(n)
end

n = 12
print(Fibonacci.naive(n))
collectgarbage("collect")
print(Fibonacci.naive(n))

