constexpr long long int mod = 1000000007;
class Solution {
public:
	long long int my_pow(int base, int exp) {
		long long val = 1;
		while (exp--) {
			val *= base;
			while (val >= mod)
				val -= mod;
		}

		return val;
	}

	int cuttingRope(int n) {
		if (n == 2)
			return 1;
		if (n == 3)
			return 2;
		unsigned long long t = n / 3, r = n % 3;
		if (!r)
			return my_pow(3, t);
		else if (r == 1)
			return my_pow(3, (t - 1)) * 4 % mod;
		else
			return my_pow(3, t) * 2 % mod;
	}
};
