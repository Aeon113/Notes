#include <iostream>
#include <vector>
#include <unordered_set>
#include <algorithm>

using std::string;

using nfa_step_set=std::unordered_set<int>;
struct path {
	char c;
	int dest;

	path(char ch = ' ', int destination = 0) :
		c(ch), dest(destination) {}
};

struct stage {
	char c;
	bool r;
	std::vector<path> paths;
};

inline bool match(char c, char p) {
	return p == '.' || c == p;
}

void build_stages(std::vector<stage> &stages, const std::string &pattern, int patsize) {
	stages.push_back(stage{ .c = ' ', .r = false });

	for (int i = 0; i < patsize; ++i) {
		stage stg;
		stg.c = pattern[i];
		if (i + 1 < patsize && pattern[i + 1] == '*') {
			stg.r = true;
			++i;
		}
		stages.push_back(stg);
	}
	stages.push_back(stage{ .c = '\0', .r = true });

	for (size_t from = 0; from < stages.size() - 1; ++from) {
		if (stages[from].r)
			stages[from].paths.emplace_back(stages[from].c, from);

		size_t to = from;
		do {
			++to;
			stages[from].paths.emplace_back(stages[to].c, to);
		} while (to < stages.size() - 1 && stages[to].r);
	}

	std::cout << "Stages built:" << std::endl;
	for (size_t i = 0; i < stages.size(); ++i) {
		std::cout << "stage " << i << ":\t" << "c == \'"
			<< stages[i].c << "\', r == "
			<< (stages[i].r ? "true" : "false") << std::endl;
		for (auto &p : stages[i].paths)
			std::cout << '\'' << p.c << "\': " << p.dest << std::endl;
	}
	std::cout << "Total " << stages.size() << " stages." << std::endl;
	std::cout << std::endl << std::endl;
}

void nfa_step(const nfa_step_set &from, nfa_step_set &to, char c, const std::vector<stage> &stages) {
	for (auto cur_i : from) {
		const stage &cur = stages[cur_i];
		for (auto &p : cur.paths) {
			if (match(c, p.c))
				to.insert(p.dest);
		}
	}
}

bool nfa_walk(const std::string &str, const std::vector<stage> &stages, int strsize, int stgsize) {
	nfa_step_set nss1, nss2, *lhs = &nss1, *rhs = &nss2;

	rhs->insert(0);
	int stri;
	for (stri = 0; stri < strsize && !rhs->empty(); ++stri) {
		std::swap(lhs, rhs);
		rhs->clear();
		nfa_step(*lhs, *rhs, str[stri], stages);
		
		std::cout << "Stepped for str[" << stri << "] \'" << str[stri] << "\':" << std::endl;
		std::cout << "Current stages:\t";
		for (auto stg: *lhs)
			std::cout << stg << ' ';
		std::cout << std::endl;
		std::cout << "Next stages:\t";
		for (auto stg: *rhs)
			std::cout << stg << ' ';
		std::cout << std::endl;
		std::cout << std::endl;
	}

	bool result = (stri == strsize && rhs->find(stgsize - 1) != std::cend(*rhs));
	std::cout << "Walk finished, with result as \"" << (result ? "true" : "false") << '\"' << std::endl;
	return result;
}

class Solution {
public:
	bool isMatch(string s, string p) {
		int strsize = s.size();
		int ptnsize = p.size();

		if (!ptnsize)
			return !strsize;

		std::cout << "str: \"" << s << "\", with strsize == " << strsize << std::endl;
		std::cout << "pattern: \"" << p << "\", with ptnsize == " << ptnsize << std::endl;
		std::cout << std::endl;

		std::vector<stage> stages;
		build_stages(stages, p, ptnsize);

		int stgsize = stages.size();

		return nfa_walk(s, stages, strsize, stgsize);
	}
};

int main() {
	Solution sol;

	struct test_case {
		std::string s;
		std::string p;
	};

	test_case cases[] = {
		{"aa", "a"},
		{"aa", "a*"},
		{"ab", ".*"},
		{"aab", "c*a*b"},
		{"mississippi", "mis*is*p*."},
		{"aaa", "ab*a"},
		{"aaa", "b"},
		{"aaa", "b*"},
		{"aaa", "b*b*"},
		{"aaa", ".*"},
		{"aaa", "a*b*"}
	};

	for (auto &c : cases) {
		sol.isMatch(c.s, c.p);
		std::cout << "==========================================================" << std::endl;
	}

	return 0;
}
