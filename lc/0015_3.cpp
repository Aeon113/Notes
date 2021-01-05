class Solution {
public:
	vector<vector<int>> threeSum(vector<int>& nums) {
		std::vector<std::vector<int>> result;
		int size = nums.size();
		if (size < 3)
			goto final;
		std::sort(std::begin(nums), std::end(nums));
		for (int i = 0; i < size - 2; ++i) {
			int target = 0 - nums[i];
			int j, k;
			j = i + 1;
			k = size - 1;
			for (;;) {
				while (j > i + 1 && j < k && nums[j] == nums[j - 1])
					++j;
				while (k < size - 1 && k > j && nums[k] == nums[k + 1])
					--k;
				if (j >= k)
					break;
				int sum = nums[j] + nums[k];
				if (sum > target)
					--k;
				else if (sum < target)
					++j;
				else {
					std::vector<int> tmp;
					tmp.push_back(nums[i]);
					tmp.push_back(nums[j]);
					tmp.push_back(nums[k]);
					result.push_back(tmp);
					++j;
					--k;
				}
			}
			while (i + 1 < size - 2 && nums[i] == nums[i + 1])
				++i;
		}

final:
		return result;
	}
};
