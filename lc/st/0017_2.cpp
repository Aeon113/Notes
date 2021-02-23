class Solution {
public:
	vector<int> printNumbers(int n) {
		std::vector<int> result;
		int max;

		if (!n)
			goto final;

		max = 9;
		while (--n)
			max = max * 10 + 9;

		for (int i = 1; i <= max; ++i)
			result.push_back(i);

final:
		return result;
	}
};
