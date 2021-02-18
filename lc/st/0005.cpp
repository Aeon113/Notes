class Solution {
public:
	string replaceSpace(string s) {
		std::string result;
		for (char c : s) {
			if (c == ' ') {
				result.push_back('%');
				result.push_back('2');
				result.push_back('0');
			} else
				result.push_back(c);
		}

		return result;
	}
};
