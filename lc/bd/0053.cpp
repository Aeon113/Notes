int right_most_max[30010];

class Solution {
public:
	int maxSubArray(vector<int>& nums) {
		int size = nums.size();
		if (!size)
			return 0;

		right_most_max[0] = nums[0];
		int max = right_most_max[0];

		for (int i = 1; i < size; ++i) {
			int val = nums[i];
			right_most_max[i] = std::max(right_most_max[i - 1] + val, val);
			max = std::max(max, right_most_max[i]);
		}

		return max;
	}
};
