class Solution {
public:
	int removeElement(vector<int>& nums, int val) {
		int size = nums.size();
		if (!size)
			return 0;

		int index = 0;
		for (int i = 0; i < size; ++i) {
			if (nums[i] == val)
				continue;
			nums[index++] = nums[i];
		}

		return index;
	}
};
