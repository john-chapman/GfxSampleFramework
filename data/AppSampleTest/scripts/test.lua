bool        = true
number      = 13
str         = "Hello, world!"
numberArray = { 1, 2, 3, 4 }
miscArray   = { 1, "one", true, 3 }
arrayArray  = { { 1, 2, 3, 4 }, { "a", "b", "c", "d" } }
struct =
{
	number       = 12,
	str          = "Inside 'struct'",
	numberArray  = { 1, 2, 3, 4 },
}

-- The following code illustrates how to make a script which can be called multiple times while maintaining a global state.
if (globalVal == nil) then
	globalVal = 0; -- init globalVal only during the first execution
else
	globalVal = globalVal + 1
end
return globalVal