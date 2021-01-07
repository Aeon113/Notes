int length;
class Solution {
public:
	static void aux(std::vector<std::string> &results, int i, int debt, std::string &s, int si) {
		//std::cout << "debt == " << debt << std::endl;
		if(i == length - 1) {
			s[si++] = '(';
			for(int j = debt + 1; j > 0; --j)
				s[si++] = ')';
			results.push_back(s);
			return;
		}
		debt += 1;
		s[si++] = '(';
		for(int paid = 0; paid <= debt; ++paid) {
			aux(results, i + 1, debt - paid, s, si);
			s[si++] = ')';
		}
	}
	
	vector<string> generateParenthesis(int n) {
		std::vector<std::string> results;
		if(0 == n)
			return results;
		length = n;
		string s;
		s.assign(2 * n, 0);
		aux(results, 0, 0, s, 0);
		
		return results;
	}
};
