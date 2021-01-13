static int needle_commonfix[50010];

class Solution {
public:
	static void build_commonfix(const std::string &needle, int size) {
		int matched = 0;
		needle_commonfix[0] = 0;

		int i = 1;
		while (i < size) {
			if (needle[i] == needle[matched])
				needle_commonfix[i] = (++matched);
			else {
				if (!matched)
					needle_commonfix[i] = 0;
				else {
					matched = needle_commonfix[matched - 1];
					continue;
				}
			}
			++i;
		}
		
		for (auto c : needle)
			std::cout << c << '\t';
		std::cout << std::endl;
		for (int i = 0; i < size; ++i)
			std::cout << needle_commonfix[i] << '\t';
		std::cout << std::endl;
	}

	int strStr(std::string haystack, std::string needle) {
		int hsize = haystack.size();
		int nsize = needle.size();

		if (!nsize)
			return 0;
		if (!hsize)
			return -1;
		if (nsize > hsize)
			return -1;
		
		build_commonfix(needle, nsize);
		int matched = 0;
		for (int i = 0; i < hsize; ++i) {
			if (haystack[i] == needle[matched]) {
				++matched;
				if (matched == nsize)
					return i - nsize + 1;
			} else {
				if (matched) {
					matched = needle_commonfix[matched - 1];
					--i;
				}
			}
		}

		return -1;
	}
};
