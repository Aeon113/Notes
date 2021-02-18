char str[5];
void myitoa(int n, char *s) {
	int i = 0;

	do {
		s[i] = (n % 10) + '0';
		n /= 10;
		++i;
	} while (n);
	std::reverse(s + 0, s + i);
	s[i] = '\0';
}

class Solution {
public:
	static std::vector<int> nums;
	static bool first;
	static void fill_ips(std::vector<std::string> &results) {
		std::string result;
		for (int n : nums) {
			myitoa(n, str);
			result.append(str);
			result.push_back('.');
		}
		result.pop_back();
		results.emplace_back(std::move(result));
	}

	static void parse_nums(const std::string &s,
			       int ssize,
			       std::vector<std::string> &results,
			       int si,
			       int ni) {
		if (ni == 4) {
			if (si == ssize)
				fill_ips(results);
			return;
		}

		if (si == ssize)
			return;

		int n = 0;
		do {
			n *= 10;
			n += s[si] - '0';
			if (n > 255)
				break;
			nums[ni] = n;
			parse_nums(s, ssize, results, si + 1, ni + 1);
			++si;
		} while (n > 0 && n < 256 && si < ssize);
	}

	vector<string> restoreIpAddresses(string s) {
		if (first) {
			first = false;
			nums.push_back(0);
			nums.push_back(0);
			nums.push_back(0);
			nums.push_back(0);
		}

		std::vector<std::string> results;
		int ssize = s.size();

		parse_nums(s, ssize, results, 0, 0);

		return results;
	}
};

std::vector<int> Solution::nums;
bool Solution::first = true;
