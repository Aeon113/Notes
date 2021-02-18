class Solution {
public:
	void nextPermutation(vector<int>& nums) {
		int size = nums.size();
		if (!size || size == 1)
			return;
		if (size == 2) {
			std::swap(nums[0], nums[1]);
			return;
		}
		
		auto ibegin = std::begin(nums);
		auto iend = std::end(nums);
		decltype(ibegin) iter1 = iend - 2;

		while (iter1 >= ibegin) {
			if (*iter1 < *(iter1 + 1)) {
				auto iter2 = iter1 + 1;
				while (iter2 < iend && *iter2 > *iter1) {
					++iter2;
				}
				--iter2;
				std::swap(*iter1, *iter2);
				std::reverse(iter1 + 1, iend);
				return;
			}
			--iter1;
		}
		std::reverse(std::begin(nums), std::end(nums));
	}
};
