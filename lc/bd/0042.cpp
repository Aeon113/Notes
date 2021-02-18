int lefts[30010];
int rights[30010];
class Solution {
public:
	int trap(vector<int>& height) {
		int size = height.size();
		if (!size)
			return 0;
		int max;

		max = 0;
		for (int i = 0; i < size; ++i) {
			max = std::max(max, height[i]);
			lefts[i] = max;
		}
		
		max = 0;
		for (int i = size - 1; i >= 0; --i) {
			max = std::max(max, height[i]);
			rights[i] = max;
		}

		int volumn = 0;
		for (int i = 0; i < size; ++i)
			volumn += std::min(lefts[i], rights[i]) - height[i];

		return volumn;
	}
};
