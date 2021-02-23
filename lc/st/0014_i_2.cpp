class Solution {
public:
	int cuttingRope(int n) {
		if (n == 2)
			return 1;
		if (n == 3)
			return 2;
		int t = n / 3, r = n % 3;
		if (!r)
			return pow(3, t);
		else if (r == 1)
			return pow(3, (t - 1)) * 4;
		else
			return pow(3, t) * 2;
	}
};
