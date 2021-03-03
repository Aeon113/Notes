std::stack<int> stk;
class Solution {
public:
	bool validateStackSequences(vector<int>& pushed, vector<int>& popped) {
		int pushsz = pushed.size(), popsz = popped.size();
		if (pushsz != popsz)
			return false;
		if (!pushsz || !popsz)
			return true;

		while (!stk.empty())
			stk.pop();
		int pushi, popi = 0;

		for (pushi = 0; pushi < pushsz; ++pushi) {
			stk.push(pushed[pushi]);
			while (!stk.empty() && stk.top() == popped[popi]) {
				stk.pop();
				++popi;
			}
		}

		return stk.empty();
	}
};
