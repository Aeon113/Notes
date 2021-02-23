double exptable[32];
class Solution {
public:
	double aux(double x, unsigned long n) {
		if (n == 1)
			return x;

		unsigned long bits;
		double res = 1.0;
		exptable[0] = x;
		for (bits = 1; (1ul << bits) <= n; ++bits)
			exptable[bits] = exptable[bits - 1] * exptable[bits - 1];
		--bits;
		while (n) {
			unsigned long v = (1u << bits);
			if (v <= n) {
				res *= exptable[bits];
				n -= v;
			}
			--bits;
		}

		return res;
	}

	double myPow(double x, int n) {
		if (!n)
			return 1;
		if (n < 0)
			return 1.0 / aux(x, -((long)n));
		else
			return aux(x, n);
	}
};
