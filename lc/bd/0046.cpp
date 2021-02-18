int factor(int n) {
	int result = 1;
	while (n > 1) {
		result *= n;
		--n;
	}

	return result;
}
class Solution {
public:
	vector<vector<int>> permute(vector<int>& nums) {
		std::vector<std::vector<int>> result;
		int size = nums.size();
		int result_size;

		if (!size)
			goto final;

		result_size = factor(size);
		while (result_size-- > 0) {
			result.push_back(nums);
			std::next_permutation(std::begin(nums), std::end(nums));
		}
final:
		return result;
	}
};
