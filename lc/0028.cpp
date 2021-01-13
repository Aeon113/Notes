static int p[50010];
class Solution {
public:
	static void create_pattern(const std::string &needle, int size) {
		p[0] = 0;

		int matched = 0;
		int i = 1;
		while (i < size) {
			if (needle[i] == needle[matched] || matched == -1)
				p[i] = (++matched);
			else {
				if (!matched)
					p[i] = 0;
				else {
					matched = p[matched - 1];
					continue;
				}
			}

			++i;
		}

		// for (auto c : needle)
		//         std::cout << c << '\t';
		// std::cout << std::endl;
                //
		// for (int j = 0; j < size; ++j)
		//         std::cout << p[j] << '\t';
		// std::cout << std::endl;

		for (int j = size - 1; j > 0; --j)
			p[j] = p[j - 1];
	}

	int strStr(string haystack, string needle) {
		int hsize = haystack.size();
		int nsize = needle.size();

		if (!nsize)
			return 0;
		if (!hsize)
			return -1;
		if (nsize > hsize)
			return -1;
		create_pattern(needle, nsize);

		int matched = 0;

		for (int i = 0; i < hsize; ++i) {
			if (haystack[i] == needle[matched]) {
				++matched;
				if (matched == nsize)
					return i - nsize + 1;
			} else if (matched) {
				--i;
				matched = p[matched];
			}
		}

		return -1;
	}
};
