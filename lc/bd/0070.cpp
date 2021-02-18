class Solution {
public:
	int climbStairs(int n) {
		if (n < 2)
			return 1;

		int i = 2;
		int si = 2;
		int sip1 = 1;

		while (i < n) {
			int tmp = sip1;
			sip1 = si;
			si = sip1 + tmp;
			++i;
		}

		return si;
	}
};
