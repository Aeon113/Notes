class Solution {
public:
	string longestCommonPrefix(vector<string>& strs) {
		int i = 0;
		int done = 0;
		int count = strs.size();

		if (count == 0)
			return "";

		for (;;++i) {
			char c;
			if (i == strs[0].length())
				break;
			c = strs[0][i];

			for (int j = 1; j < count; ++j)
				if (i == strs[j].length() || strs[j][i] != c)
					return strs[0].substr(0, i);
		}

		return strs[0];
	}
};
