class Solution {
public:
	string intToRoman(int num) {
		string result;
		if (num >= 1000) {
			result.append(num / 1000, 'M');
			num %= 1000;
		}

		if (num >= 100) {
			if (num >= 900) {
				result.push_back('C');
				result.push_back('M');
				num -= 900;
			} else if (num >= 500) {
				result.push_back('D');
				num -= 500;
				while (num >= 100) {
					result.push_back('C');
					num -= 100;
				}
			} else if (num >= 400) {
				result.push_back('C');
				result.push_back('D');
				num -= 400;
			} else {
				while (num >= 100) {
					result.push_back('C');
					num -= 100;
				}
			}
		}

		if (num >= 10) {
			if (num >= 90) {
				result.push_back('X');
				result.push_back('C');
				num -= 90;
			} else if (num >= 50) {
				result.push_back('L');
				num -= 50;
				while (num >= 10) {
					result.push_back('X');
					num -= 10;
				}
			} else if (num >= 40) {
				result.push_back('X');
				result.push_back('L');
				num -= 40;
			} else {
				while (num >= 10) {
					result.push_back('X');
					num -= 10;
				}
			}
		}

		if (num == 9) {
			result.push_back('I');
			result.push_back('X');
		} else if (num >= 5) {
			result.push_back('V');
			num -= 5;
			while (num--)
				result.push_back('I');
		} else if (num == 4) {
			result.push_back('I');
			result.push_back('V');
		} else {
			while (num--)
				result.push_back('I');
		}

		return result;
	}
};
