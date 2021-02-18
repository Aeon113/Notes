class Solution {
public:
	int maxProfit(vector<int>& prices) {
		int size = prices.size();
		if (size <= 1)
			return 0;

		int max_profit = 0;
		int min = prices[0];

		for (int i = 1; i < size; ++i) {
			int profit = prices[i] - min;
			if (profit < 0)
				min = prices[i];
			else if (profit > max_profit)
				max_profit = profit;
		}

		return max_profit;
	}
};
