struct fib_matrix_t {
	long fn;
	long fnm1;
};

fib_matrix_t table[7];
long mod = 1000000007;
static bool first = true;

inline void fib_matrix_mult(fib_matrix_t &result,
			    const fib_matrix_t &lhs,
			    const fib_matrix_t &rhs) {
	long lhs_fnp1 = (lhs.fn + lhs.fnm1) % mod;
	// long rhs_fnp1 = (rhs.fn + rhs.fnm1) % mod;

	result.fn = (lhs_fnp1 * rhs.fn + lhs.fn * rhs.fnm1) % mod;
	result.fnm1 = (lhs.fn * rhs.fn + lhs.fnm1 * rhs.fnm1) % mod;
}

void build_table() {
	table[0].fn = 1;
	table[0].fnm1 = 0;

	for (int i = 1; i < 7; ++i)
		fib_matrix_mult(table[i], table[i - 1], table[i - 1]);
}

class Solution {
public:
	int fib(int n) {
		fib_matrix_t mat;
		fib_matrix_t tmp;
		if (!n)
			return 0;
		if (first) {
			build_table();
			first = false;
		}
		int shift = 6;
		while ((1 << shift) > n) {
			--shift;
		}

		mat = table[shift];
		n -= (1 << shift);
		--shift;

		while (n) {
			if ((1 << shift) <= n) {
				fib_matrix_mult(tmp, mat, table[shift]);
				mat = tmp;
				n -= (1 << shift);
			}
			--shift;
		}

		return mat.fn;
	}
};
