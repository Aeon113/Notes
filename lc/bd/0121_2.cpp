struct devide_result {
	int max_profit;
	int min;
	int max;
};

class Solution {
public:
	static devide_result devide(const std::vector<int> &prices, int begin, int end) {
		devide_result result;
		if (begin >= end) {
			result.max_profit = 0;
			result.min = 0x3f3f3f3f;
			result.max = -0x3f3f3f3f;
		} else if (begin == end - 1) {
			result.max_profit = 0;
			result.min = result.max = prices[begin];
		} else {
			int mid = begin + (end - begin) / 2;
			devide_result left_result = devide(prices, begin, mid);
			devide_result right_result = devide(prices, mid, end);

			result.min = std::min(left_result.min, right_result.min);
			result.max = std::max(left_result.max, right_result.max);

			result.max_profit = std::max(left_result.max_profit, right_result.max_profit);
			result.max_profit = std::max(result.max_profit, right_result.max - left_result.min);
		}

		return result;
	}
	int maxProfit(vector<int>& prices) {
		int size = prices.size();

		if (size <= 1)
			return 0;

		devide_result result = devide(prices, 0, size);

		return result.max_profit;
	}
};
