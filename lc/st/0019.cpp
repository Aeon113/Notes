#include <vector>
#include <unordered_set>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <string>

struct stage {
	char c;
	bool r;
};

inline bool match(char c, char p) {
	if (p == '.')
		return true;
	return c == p;
}

void print_stages(const std::vector<stage> &stages) {
	int stgsize = stages.size();

	std::cout << "Stages: count == " << stgsize << std::endl;
	for (int i = 0; i < stgsize; ++i)
		std::cout << "stage " << i << ":\tc: \'" << stages[i].c << "\'\tr: " << (stages[i].r ? "True" : "False") << std::endl;
	std::cout << std::endl;
}

class Solution {
public:
	bool isMatch(std::string s, std::string p) {
		int strsize = s.size();
		int patsize = p.size();

		std::cout << "s == \"" << s << "\"" << ", size == " << strsize << std::endl;
		std::cout << "p == \"" << p << "\"" << ", size == " << patsize << std::endl;
		std::cout << std::endl;

		if (!patsize)
			return !strsize;

		std::vector<stage> stages;
		for (int i = 0; i < patsize; ++i) {
			stage stg;
			stg.c = p[i];
			if (i + 1 < patsize && p[i + 1] == '*') {
				++i;
				stg.r = true;
			} else
				stg.r = false;

			stages.push_back(stg);
		}
		int stgsize = stages.size();

		print_stages(stages);

		std::unordered_set<int> traces1, traces2, *tlhs = &traces2, *trhs = &traces1;
		trhs->insert(0);
		int si;

		auto get_next_stages = [&s,strsize, &stages, &stgsize, &si, &trhs](int stage) {
			if (stage == stgsize)
				return;
			if (stages[stage].r) {
				int tmp = stage;
				do {
					if (match(s[si], stages[tmp].c))
						trhs->insert(tmp);
					++tmp;
				} while (tmp < stgsize && stages[tmp].r);
				trhs->insert(tmp);
			} else if (match(s[si], stages[stage].c))
				trhs->insert(stage + 1);
		};

		auto print_num = [](int v) {std::cout << v << ' '; };
		for (si = 0; si < strsize && !trhs->empty(); ++si) {
			std::cout << "si == " << si << ", with c == \'" << s[si] << "\':\tstages are: ";
			for_each(std::cbegin(*trhs), std::cend(*trhs), print_num);
			std::cout << std::endl;

			std::swap(tlhs, trhs);
			trhs->clear();
			for_each(std::cbegin(*tlhs), std::cend(*trhs), get_next_stages);
		}

		std::cout << "Loop stopped, with si == " << si << std::endl;
		std::cout << "Final stages: ";
		for_each(std::cbegin(*trhs), std::cend(*trhs), print_num);

		return si == strsize && trhs->find(stgsize) != std::cend(*trhs);
	}
};

int main() {
	std::string s = "mississippi", p = "mis*is*p*.";
	bool result = Solution().isMatch(s, p);
	std::cout << (result ? "True" : "False") << std::endl;

	return 0;
}
