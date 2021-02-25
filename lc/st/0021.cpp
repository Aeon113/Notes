class Solution {
public:
	vector<int> exchange(vector<int>& nums) {
		int size = nums.size();
		if (!size)
			return nums;
		int left = 0, right = size - 1;

		for (;;) {
			while (left < right && nums[left] % 2)
				++left;
			while (left < right && !(nums[right] % 2))
				--right;
			if (left < right) {
				int tmp = nums[left];
				nums[left] = nums[right];
				nums[right] = tmp;

				++left;
				--right;
			} else
				break;
		}

		return nums;
	}
};
