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

print(Fibonacci.naive(35))
