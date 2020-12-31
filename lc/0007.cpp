class Solution {
public:
	int reverse(int x) {
		int negative = x < 0;
		long val = abs((long) x), res = 0;
		while (val) {
			res *= 10;
			res += val % 10;
			val /= 10;
		}

		res = negative ? -res : res;
		if (res < -(1l << 31l) || res > (1l << 31l) - 1)
			res = 0;
		return res;
	}
};
