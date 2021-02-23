int digits[] = {1, 2 ,3 ,4 ,5 ,6 ,7 ,8 ,9};
class Solution {
public:
	vector<int> printNumbers(int n) {
		std::vector<int> result;
		if (!n)
			return result;
		for (auto i : digits)
			result.push_back(i);

		int bases = 1;
		while (--n) {
			bases *= 10;
			int size = result.size();
			for (auto hbit : digits) {
				result.push_back(hbit * bases);
				for (int i = 0; i < size; ++i)
					result.push_back(hbit * bases + result[i]);
			}
		}

		return result;
	}
};
