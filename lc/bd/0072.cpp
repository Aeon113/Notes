int dp_states[2][510];
class Solution {
public:
	static int dp(const std::string &word1, const std::string &word2) {
		int size1 = word1.size();
		int size2 = word2.size();
		if (!size1)
			return size2;
		if (!size2)
			return size1;

		int (*dp_states_prev)[510] = dp_states + 0;
		int (*dp_states_cur)[510] = dp_states + 1;

		for (int i = 0; i <= size1; ++i)
			(*dp_states_cur)[i] = i;

		for (int l2 = 1; l2 <= size2; ++l2) {
			std::swap(dp_states_cur, dp_states_prev);
			(*dp_states_cur)[0] = l2;
			for (int i = 1; i <= size1; ++i) {
				int val = std::min((*dp_states_prev)[i],
						   (*dp_states_cur)[i - 1]) + 1;
				val = std::min(val, (*dp_states_prev)[i - 1] +	\
						    ((word1[i - 1] == word2[l2 - 1]) ? 0 : 1));
				(*dp_states_cur)[i] = val;
			}
		}

		return (*dp_states_cur)[size1];
	}

	int minDistance(string word1, string word2) {
		return dp(word1, word2);
	}
};
