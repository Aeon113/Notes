class Solution {
public:
	int threeSumClosest(vector<int>& nums, int target) {
		int min_diff = 0x3f3f3f3f;
		int min_diff_abs = 0x3f3f3f3f;
		int size = nums.size();
		if (size < 3)
			goto final;
		std::sort(std::begin(nums), std::end(nums));
		for (int i = 0; i < size - 2; ++i) {
			int j, k;
			j = i + 1;
			k = size - 1;
			while (j < k) {
				int sum = nums[i] + nums[j] + nums[k];
				int diff = sum - target;
				int diff_abs = abs(diff);
				if (diff_abs == 0) {
					min_diff = 0;
					// min_diff_abs = 0;
					break;
				} else if (diff_abs < min_diff_abs) {
					min_diff_abs = diff_abs;
					min_diff = diff;
				}
				if (diff > 0) {
					do {
						--k;
					 } while (k > j && nums[k] == nums[k + 1]);
				} else {
					do {
						++j;
					} while (j < k && nums[j] == nums[j - 1]);
				}
			}
			if (min_diff) {
				while (i + 1 < size - 2 && nums[i] == nums[i + 1])
					++i;
			} else
				break;
		}

final:
		return target + min_diff;
	}
};
