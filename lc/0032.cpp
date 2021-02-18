int pstack[30010];
int plength[30010];
class Solution {
public:
	int longestValidParentheses(string s) {
		int size = s.size();
		int pstack_i = 0;
		int max_length = 0;

		for (int i = 0; i < size; ++i) {
			if (s[i] == '(') {
				pstack[pstack_i++] = i;
				plength[i] = 0;
			} else {
				plength[i] = 0;
				if (pstack_i) {
					int left = pstack[--pstack_i];
					plength[left] = i - left + 1;
				}
			}
		}

		// for (int i = 0; i < size; ++i)
		//         std::cout << plength[i] << '\t';
		// std::cout << std::endl;

		int index = 0;
		int maxlen = 0;
		int concat = 0;
		int len = 0;
		while (index < size) {
			if (plength[index]) {
				if (concat)
					len += plength[index];
				else
					len = plength[index];
				concat = 1;
				if (len > maxlen)
					maxlen = len;
				index += plength[index];
			} else {
				concat = 0;
				++index;
			}
		}

		return maxlen;
	}
};
