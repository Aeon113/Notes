class Solution {
private:
	bool binsearch(std::vector<int> &nums, int begin, int end, int target) {
		if (begin >= end)
			return false;

		int mid = (begin + end) / 2;
		if (target < nums[mid])
			return binsearch(nums, begin, mid, target);
		if (target > nums[mid])
			return binsearch(nums, mid + 1, end, target);
		return true;
	}

public:
	vector<vector<int>> threeSum(vector<int>& nums) {
		std::vector<std::vector<int>> result;
		int size = nums.size();
		int vals[3];
		if (size < 3)
			goto final;
		std::sort(std::begin(nums), std::end(nums));

		for (int i = 0; i < size - 2; ++i) {
			if (i > 0 && nums[i] == nums[i - 1])
				continue;
			vals[0] = nums[i];
			for (int j = i + 1; j < size - 1; ++j) {
				if (j > i + 1 && nums[j] == nums[j - 1])
					continue;
				vals[1] = nums[j];
				vals[2] = 0 - vals[0] - vals[1];
				if (vals[2] < vals[1])
					break;
				if (binsearch(nums, j + 1, size, vals[2]))
					result.emplace_back(std::cbegin(vals), std::cend(vals));
			}
		}

final:
		return result;
	}
};
