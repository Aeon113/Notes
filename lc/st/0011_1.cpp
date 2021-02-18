class Solution {
public:
	int minArray(vector<int>& numbers) {
		int m = numbers.front();

		for (int i = 1; i < numbers.size(); ++i)
			m = std::min(m, number[i]);

		return m;
	}
};
