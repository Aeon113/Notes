class Solution {
	static void _reverse(std::vector<int> &nums, int begin, int end) {
		int last = end - 1;
		int tmp;
		while (begin < last) {
			tmp = nums[begin];
			nums[begin] = nums[last];
			nums[last] = tmp;
			++begin;
			--last;
		}
	}

	static void _next_permutation(std::vector<int> &nums, int size) {
		int i = size - 2;
		while (i >= 0) {
			if (nums[i] < nums[i + 1]) {
				int j;
				for (j = size - 1; j > i && nums[j] <= nums[i]; --j) {}
				std::swap(nums[i], nums[j]);
				_reverse(nums, i + 1, size);
				return;
			}
			--i;
		}

		_reverse(nums, 0, size);
	}

public:
	void nextPermutation(vector<int>& nums) {
		int size = nums.size();
		if (size < 2)
			return;

		_next_permutation(nums, size);
	}
};
