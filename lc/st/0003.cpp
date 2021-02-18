class Solution {
	static int count[100010];
public:
	int findRepeatNumber(vector<int>& nums) {
		memset(Solution::count, 0, sizeof(Solution::count));
		for (int i : nums) {
			if (count[i])
				return i;
			++count[i];
		}

		return -1;
	}
};

int Solution::count[100010];
