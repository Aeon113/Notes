class Solution {
public:
	bool isValid(string s) {
		std::stack<char> paratheses;
		
		for (char c : s) {
			switch (c) {
			case '(':
			case '[':
			case '{':
				paratheses.push(c);
				break;
			case ')':
				if (paratheses.empty())
					return false;
				c = paratheses.top();
				paratheses.pop();
				if (c != '(')
					return false;
				break;
			case ']':
				if (paratheses.empty())
					return false;
				c = paratheses.top();
				paratheses.pop();
				if (c != '[')
					return false;
				break;
			case '}':
				if (paratheses.empty())
					return false;
				c = paratheses.top();
				paratheses.pop();
				if (c != '{')
					return false;
				break;
			default:
				return false;
			}
		}
		return paratheses.empty();
	}
};
