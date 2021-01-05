class Solution {
private:
	static std::unordered_map<int, int> numsmap;
	static bool first;

	static void init_map() {
		numsmap.reserve(3000);
		first = false;
	}
public:
	vector<vector<int>> threeSum(vector<int>& nums) {
		if (first)
			Solution::init_map();

		std::vector<std::vector<int>> result;
		decltype(numsmap.begin()) mapiter;
		int size = nums.size();
		int vals[3];
		if (size < 3)
			goto final;
		numsmap.clear();

		std::sort(nums.begin(), nums.end());
		for(auto n : nums) {
			mapiter = numsmap.find(n);
			if (mapiter == numsmap.end())
				numsmap.emplace(n, 1);
			else
				++(mapiter->second);
		}

		for (int i = 0; i < size - 2; ++i) {
			if (i > 0 && nums[i] == nums[i - 1])
				continue;
			vals[0] = nums[i];
			--numsmap[vals[0]];
			for (int j = i + 1; j < size - 1; ++j) {
				if (j > i + 1 && nums[j] == nums[j - 1])
					continue;
				vals[1] = nums[j];
				vals[2] = 0 - vals[0] - vals[1];
				if (vals[2] < vals[1])
					break;
				--numsmap[vals[1]];
				mapiter = numsmap.find(vals[2]);
				if (mapiter != numsmap.end() && mapiter->second > 0) {
					std::vector<int> r(3, 0);
					result.emplace_back(std::cbegin(vals), std::cend(vals));
				}
				++numsmap[vals[1]];
			}
			++numsmap[vals[0]];
		}

final:
		return result;
	}
};

bool Solution::first = true;
std::unordered_map<int, int> Solution::numsmap;
