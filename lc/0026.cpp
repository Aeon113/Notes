class Solution {
public:
	int removeDuplicates(vector<int>& nums) {
		int size = nums.size();
		if (size < 2)
			return size;
		int prev = 0;
		int index;
		for (index = 1; index < size; ++index) {
			if (nums[index] == nums[index - 1])
				continue;
			++prev;
			nums[prev] = nums[index];
        }

		return prev + 1;
	}
};
