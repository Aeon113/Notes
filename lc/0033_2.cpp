class Solution {
public:
	static int find_cliff(std::vector<int> &nums, int size) {
		if (nums[0] < nums[size - 1])
			return 0;
		int left = 0, right = size - 1;
		while(left < right) {
			if (nums[left] < nums[right])
				return left;
			if (right == left + 1)
				return right;
			int mid = left + (right - left) / 2;
			if (nums[left] > nums[mid])
				right = mid;
			else
				left = mid;
		}

		return left;
	}

	static int bin_search(std::vector<int> &nums, int size, int target, int cliff) {
		int begin = 0, last = size - 1;

		while (begin < last) {
			int mid = begin + (last - begin) / 2;
			int val_mid = nums[(mid + cliff) % size];
			if (target < val_mid)
				last = mid - 1;
			else if (target > val_mid)
				begin = mid + 1;
			else
				return (mid + cliff) % size;
		}

		if (nums[(begin + cliff) % size] == target)
			return (begin + cliff) % size;
		return -1;
	}

	int search(vector<int>& nums, int target) {
		int size = nums.size();
		if (size == 0)
			return -1;
		if (size == 1)
			return nums[0] == target ? 0 : -1;
		int cliff = find_cliff(nums, size);

		return bin_search(nums, size, target, cliff);
	}
};
