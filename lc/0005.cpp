#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
public:
	string longestPalindrome(string s) {
           string result;
	   if (s.empty())
		   return result;

	   vector<char> aug(s.length() * 2 + 3, '\0');
	   vector<int> radius(s.length() * 2 + 3, 1);
	   int j = 0;

	   aug[j++] = '^';
	   for (int i = 0; i < s.length(); ++i) {
		   aug[j++] = '#';
		   aug[j++] = s[i];
	   }
	   aug[j++] = '#';
	   aug[j++] = '$';
	   int right_most_bound = 0;
	   int right_most_center = 0;
	   int r;
	   int max_palindrome_center = 0;

	   for (int i = 1; i < aug.size(); ++i) {
		   if (i < right_most_bound)
			   r =	\
					min(radius[2 * right_most_center - i],	\
						right_most_bound - i + 1);	\
		   else
			   r = 1;

		   while (i + r < aug.size() && aug[i + r] == aug[i - r])
			   ++r;
		   radius[i] = r;
		   if (r > radius[max_palindrome_center])
			   max_palindrome_center = i;
		   if (i + r - 1 > right_most_bound) {
			   right_most_bound = i + r - 1;
			   right_most_center = i;
		   }
	   }

	   // std::cout << "Original: " << s << '\n' << "Augmented: ";
	   // for (auto c : aug)
	   //		 std::cout << c;
	   // std::cout << std::endl;
	   // std::cout << "max palindrome center: " << max_palindrome_center << ", radius: " << radius[max_palindrome_center] << std::endl;
	   //
	   // return "";
	   for (int i = max_palindrome_center - radius[max_palindrome_center] + 1; i < max_palindrome_center + radius[max_palindrome_center]; ++i) {
		   if (aug[i] == '#')
			   continue;
		   else
			   result.push_back(aug[i]);
	   }
	   return result;
	}
};

int main() {
	const char str[] = "cbaqabcbaqad";
	Solution s;
	std::cout << "Original:\t" << str << '\n' << "Result:\t";
	std::cout << s.longestPalindrome(str) << std::endl;

	return 0;
}
