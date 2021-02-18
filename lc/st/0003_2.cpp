class Solution {
	static unsigned char bitmap[100010 / 8 + 1];
public:
	int findRepeatNumber(vector<int>& nums) {
		memset(Solution::bitmap, 0, sizeof(Solution::bitmap));
		for (int i : nums) {
			int cnt = i / 8;
			unsigned char mask = ((unsigned char ) 1u) << ((unsigned char)(i % 8));
			if (bitmap[cnt] & mask)
				return i;
			bitmap[cnt] = bitmap[cnt] | mask;
		}

		return -1;
	}
};

unsigned char Solution::bitmap[100010 / 8 + 1];
