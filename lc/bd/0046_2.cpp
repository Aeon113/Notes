void the_reverse(std::vector<int> &nums, int begin, int end) {
	int last = end - 1;
	while (begin < last) {
		int tmp = nums[begin];
		nums[begin] = nums[last];
		nums[last] = tmp;
		++begin;
		--last;
	}
}

void the_next_permutation(std::vector<int> &nums, int size) {
	int sentinel;

	for (sentinel = size - 2; sentinel >= 0; --sentinel) {
		if (nums[sentinel] < nums[sentinel + 1]) {
			int to_swap;
			for (to_swap = size - 1; to_swap > sentinel && nums[to_swap] < nums[sentinel]; --to_swap) {}
			//swap
			int tmp = nums[to_swap];
			nums[to_swap] = nums[sentinel];
			nums[sentinel] = tmp;

			// reverse
			the_reverse(nums, sentinel + 1, size);
			return;
		}
	}

	the_reverse(nums, 0, size);
}

int the_factor(int n) {
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
		int size = nums.size();
		std::vector<std::vector<int>> result;
		int result_size;

		if (size <= 1) {
			result.push_back(nums);
			goto final;
		}

		result_size = the_factor(size);

		result.push_back(nums);
		--result_size;

		while (result_size-- > 0) {
			the_next_permutation(nums, size);
			result.push_back(nums);
		}

final:
		return result;
	}
};
