int dpstates0[60];
int dpstates1[60];
bool first = true;
class Solution {
public:
	void gen_dpstates() {
		dpstates0[0] = 1;
		dpstates1[0] = 0;
		for (int i = 1; i <= 57; ++i) {
			int max0 = i + 1;
			int max1 = -0x3f3f3f3f;
			for (int j = 0; j < i; ++j) {
				max0 = std::max(max0, dpstates0[j] * (i - j));
				max1 = std::max(max1, dpstates0[j] * (i - j));
			}
			dpstates0[i] = max0;
			dpstates1[i] = max1;
		}
	}

	int cuttingRope(int n) {
		if (first) {
			gen_dpstates();
			first = false;
		}

		return dpstates1[n - 1];
	}
};
