class Solution {
public:
	vector<vector<int>> merge(vector<vector<int>>& intervals) {
		std::vector<std::vector<int>> results;
		int size = intervals.size();
		int left_start, right_most;
		if (!size)
			goto final;

		std::sort(std::begin(intervals), std::end(intervals),
			  [](const std::vector<int> &lhs, const std::vector<int> &rhs) {
				if (lhs.front() < rhs.front())
					return true;
				else if (lhs.front() == rhs.front())
					return lhs.back() < rhs.back();
				return false;
			  });

		left_start = intervals.front().front();
		right_most = intervals.front().back();

		for (int i = 1; i < size; ++i) {
			const std::vector<int> &interval = intervals[i];

			if (interval.front() <= right_most)
				right_most = std::max(right_most, interval.back());
			else {
				std::vector<int> tmp;
				tmp.push_back(left_start);
				tmp.push_back(right_most);
				results.emplace_back(std::move(tmp));
				left_start = interval.front();
				right_most = interval.back();
			}
		}

		results.emplace_back();
		results.back().push_back(left_start);
		results.back().push_back(right_most);
final:
		return results;
	}
};
