class Solution {
public:
	vector<vector<int>> fourSum(vector<int>& nums, int target) {
		std::vector<std::vector<int>> result;
		int size = nums.size();
		if (size < 4)
			goto final;
		std::sort(std::begin(nums), std::end(nums));
		int i, j, k, l;
		i = 0;
		while (i < size - 3) {
			j = i + 1;
			while (j < size - 2) {
				k = j + 1;
				l = size - 1;
				while (k < l) {
					int sum = nums[i] + nums[j] + nums[k] + nums[l];
					if (sum > target) {
						do { --l; } while (k < l && nums[l] == nums[l + 1]);
					} else if (sum < target) {
						do { ++k; } while (k < l && nums[k] == nums[k - 1]);
					} else {
						std::vector<int> tmp;
						tmp.push_back(nums[i]);
						tmp.push_back(nums[j]);
						tmp.push_back(nums[k]);
						tmp.push_back(nums[l]);
						result.push_back(std::move(tmp));
						++k;
						--l;
						while (k < l && nums[k] == nums[k - 1]) ++k;
						while (k < l && nums[l] == nums[l + 1]) --l;
					}
				}
				do { ++j; } while (j < size - 2 && nums[j] == nums[j - 1]);
			}
			do { ++i; } while (i < size - 3 && nums[i] == nums[i - 1]);
		}

final:
		return result;
	}
};
