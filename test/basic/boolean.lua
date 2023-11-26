
local assert = assert

local low = 1
local mid = 10
local high = 100

assert((low < low) == false, "test low < low")
assert((low < mid) == true, "test low < mid")
assert((low < high) == true, "test low < high")
assert((mid < low) == false, "test mid < low")
assert((mid < mid) == false, "test mid < mid")
assert((mid < high) == true, "test mid < high")
assert((high < low) == false, "test high < low")
assert((high < mid) == false, "test high < mid")
assert((high < high) == false, "test high < high")

assert((low <= low) == true, "test low <= low")
assert((low <= mid) == true, "test low <= mid")
assert((low <= high) == true, "test low <= high")
assert((mid <= low) == false, "test mid <= low")
assert((mid <= mid) == true, "test mid <= mid")
assert((mid <= high) == true, "test mid <= high")
assert((high <= low) == false, "test high <= low")
assert((high <= mid) == false, "test high <= mid")
assert((high <= high) == true, "test high <= high")

assert((low > low) == false, "test low > low")
assert((low > mid) == false, "test low > mid")
assert((low > high) == false, "test low > high")
assert((mid > low) == true, "test mid > low")
assert((mid > mid) == false, "test mid > mid")
assert((mid > high) == false, "test mid > high")
assert((high > low) == true, "test high > low")
assert((high > mid) == true, "test high > mid")
assert((high > high) == false, "test high > high")

assert((low >= low) == true, "test low >= low")
assert((low >= mid) == false, "test low >= mid")
assert((low >= high) == false, "test low >= high")
assert((mid >= low) == true, "test mid >= low")
assert((mid >= mid) == true, "test mid >= mid")
assert((mid >= high) == false, "test mid >= high")
assert((high >= low) == true, "test high >= low")
assert((high >= mid) == true, "test high >= mid")
assert((high >= high) == true, "test high >= high")

assert((low == low) == true, "test low == low")
assert((low == mid) == false, "test low == mid")
assert((low == high) == false, "test low == high")
assert((mid == low) == false, "test mid == low")
assert((mid == mid) == true, "test mid == mid")
assert((mid == high) == false, "test mid == high")
assert((high == low) == false, "test high == low")
assert((high == mid) == false, "test high == mid")
assert((high == high) == true, "test high == high")

assert((low ~= low) == false, "test low == low")
assert((low ~= mid) == true, "test low == mid")
assert((low ~= high) == true, "test low == high")
assert((mid ~= low) == true, "test mid == low")
assert((mid ~= mid) == false, "test mid == mid")
assert((mid ~= high) == true, "test mid == high")
assert((high ~= low) == true, "test high == low")
assert((high ~= mid) == true, "test high == mid")
assert((high ~= high) == false, "test high == high")