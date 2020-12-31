class Solution {
public:
	string convert(string s, int numRows) {
		int length = s.length();
		string result(s);

		if (numRows == 1)
			return result;

		int i = 0;
		for (int row = 0; row < numRows && row < length; ++row) {
			int s_index = row;
			int s_gap = 2 * (numRows - 1 - row);

			for (;;) {
				result[i] = s[s_index];
				if (!s_gap)
					s_gap = 2 * (numRows - 1) - s_gap;
				s_index += s_gap;
				++i;
				if (s_index >= length)
					break;
				s_gap = 2 * (numRows - 1) -s_gap;
			}
		}

		return result;
	}
};
