class Solution {
public:
	int divide(int dividend, int divisor) {
		long ldividend = dividend, ldivisor = divisor;
		long tmp, result = 0;
		int negative = 0;
		long shifts = 0;
		
		if (ldividend < 0) {
			negative = !negative;
			ldividend = -ldividend;
		}
		
		if (ldivisor < 0) {
			negative = !negative;
			ldivisor = -ldivisor;
		}
		
		if (ldividend < ldivisor)
			return 0;
		
		while ((ldivisor << shifts) <= ldividend)
			++shifts;

		while (ldividend >= ldivisor && shifts >= 0) {
			if ((ldivisor << shifts) <= ldividend) {
				result += (1l << shifts);
				ldividend -= (ldivisor << shifts);
			}
			--shifts;
		}

		if (negative)
			result = -result;

		if (result < INT_MIN || result > INT_MAX)
			result = INT_MAX;
		return result;
	}
};
