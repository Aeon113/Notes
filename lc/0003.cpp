#include <string>

using namespace std;

class Solution {
public:
	int lengthOfLongestSubstring(string s) {
		int i0 = -1, i1=0, len = s.length(), max_sublen = 0;
		char alphabet[1 << 8];
		memset(alphabet, 0x0, sizeof(alphabet));

		for (i1 = 0; i1 < len; ++i1) {
			while (alphabet[s[i1]] && i0 < i1) {
				++i0;
				alphabet[s[i0]] = 0;
			}
			alphabet[s[i1]] = 1;
			if (max_sublen < i1 - i0)
				max_sublen = i1 - i0;
		}

		return max_sublen;
	}
};
