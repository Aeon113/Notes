class Solution {
public:
	static constexpr char digit_map[][4] = {
		{0, 0, 0, 0},
		{0, 0, 0, 0},
		{'a', 'b', 'c', 0},
		{'d', 'e', 'f', 0},
		{'g', 'h', 'i', 0},
		{'j', 'k', 'l', 0},
		{'m', 'n', 'o', 0},
		{'p', 'q', 'r', 's'},
		{'t', 'u', 'v', 0},
		{'w', 'x', 'y', 'z'}
	};

	static void dfs(const std::string &digits, int index, int len,
			std::string &tmp, std::vector<std::string> &result) {
		if (index == len) {
			result.push_back(tmp);
			return;
		}

		int i = digits[index] - '0';
		for (auto c : digit_map[i]) {
			if (c == 0)
				break;
			tmp.push_back(c);
			dfs(digits, index + 1, len, tmp, result);
			tmp.pop_back();
		}
	}
	vector<string> letterCombinations(string digits) {
		std::vector<std::string> result;
		std::string tmp;
		int len = digits.length();
		if (len)
			dfs(digits, 0, len, tmp, result);
		return result;
	}
};
