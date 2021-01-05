class Solution {
public:
	int romanToInt(string s) {
		int result = 0;
		int len = s.length(), i = 0;
		if (!len)
			return result;

		// s >= 1000
		if (s[i] == 'M') {
			do {
				result += 1000;
				++i;
			} while (i < len && s[i] == 'M');
		}
		if (i == len)
			return result;

		// s >= 100
		if (s[i] == 'C') {
			if (i + 1 < len) {
				switch (s[i + 1]) {
					case 'M':
						result += 900;
						i += 2;
						break;
					case 'D':
						result += 400;
						i += 2;
						break;
					default:
						do {
							result += 100;
							++i;
						} while (i < len && s[i] == 'C');
				}
			} else {
				result += 100;
				++i;
			}
		} else if (s[i] == 'D') {
			result += 500;
			++i;
			while (i < len && s[i] == 'C') {
				result += 100;
				++i;
			}
		}
		if (i == len)
			return result;

		// s >= 10
		if (s[i] == 'X') {
			if (i + 1 < len) {
				switch (s[i + 1]) {
					case 'C':
						result += 90;
						i += 2;
						break;
					case 'L':
						result += 40;
						i += 2;
						break;
					default:
						do {
							result += 10;
							++i;
						} while (i < len && s[i] == 'X');
				}
			} else {
				result += 10;
				++i;
			}
		} else if (s[i] == 'L') {
			result += 50;
			++i;
			while (i < len && s[i] == 'X') {
				result += 10;
				++i;
			}
		}
		if (i == len)
			return result;

		// s >= 1
		if (s[i] == 'I') {
			if (i + 1 < len) {
				switch (s[i + 1]) {
					case 'X':
						result += 9;
						i += 2;
						break;
					case 'V':
						result += 4;
						i += 2;
						break;
					default:
						do {
							result += 1;
							++i;
						} while (i < len && s[i] == 'I');
				}
			} else {
				result += 1;
				++i;
			}
		} else if (s[i] == 'V') {
			result += 5;
			++i;
			while (i < len && s[i] == 'I') {
				result += 1;
				++i;
			}
		}

		return result;
	}
};
