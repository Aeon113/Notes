class Solution {
public:
	int hammingWeight(uint32_t n) {
		int res = 0;
		while (n) {
			switch(n & 0x3u) {
			case 0:
				break;
			case 1:
			case 2:
				++res;
				break;
			default:
				res += 2;
				break;
			}

			n = (n >> 2u);
		}

		return res;
	}
};
