class Solution {
public:
	int minArray(vector<int>& numbers) {
		int size = numbers.size();
		int left = 0, right = size - 1;
		while (left < right) {
			if (numbers[left] == numbers[right]) {
				--right;
				continue;
			}

			int mid = left + (right - left) / 2;
			if (numbers[left] > numbers[mid]) {
				right = mid;
				++left;
			}
			else if (numbers[mid] > numbers[right])
				left = mid + 1;
			else
				break;
		}

		return numbers[left];
	}
};
