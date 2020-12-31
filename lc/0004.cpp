#include <vector>
#include <algorithm>

using namespace std;
class Solution {
public:
	double findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
		int len1 = nums1.size(), len2 = nums2.size();
		int target_left_len = (len1 + len2) / 2;
		int left_count1, left_count2;
		int left_max1, left_max2, right_min1, right_min2;
		int left_count1_min = 0, left_count1_max = len1;

		if (!len1) {
			if (!len2)
				return 0;
			if (len2 % 2)
				return nums2[len2 / 2];
			else
				return (nums2[(len2 - 1) / 2] + nums2[len2 / 2]) / 2.0;
		} else if (!len2) {
			if (len1 % 2)
				return nums1[len1 / 2];
			else
				return (nums1[(len1 - 1) / 2] + nums1[len1 / 2]) / 2.0;
		}
		
		for (;;) {
			left_count1 = (left_count1_min + left_count1_max) / 2;
			left_count2 = target_left_len - left_count1;

			if (left_count2 < 0) {
				left_count1_max = left_count1;
				continue;
			}
			if (left_count2 > len2) {
				left_count1_min = left_count1 + 1;
				continue;
			}
			
			if (left_count1 == 0) {
				left_max1 = -0x3f3f3f3f;
				right_min1 = nums1.front();
			} else if (left_count1 == len1) {
				left_max1 = nums1.back();
				right_min1 = 0x3f3f3f3f;
			} else {
				left_max1 = nums1[left_count1 - 1];
				right_min1 = nums1[left_count1];
			}
			
			if (left_count2 == 0) {
				left_max2 = -0x3f3f3f3f;
				right_min2 = nums2.front();
			} else if (left_count2 == len2) {
				left_max2 = nums2.back();
				right_min2 = 0x3f3f3f3f;
			} else {
				left_max2 = nums2[left_count2 - 1];
				right_min2 = nums2[left_count2];
			}
			
			if (left_max1 > right_min2)
				left_count1_max = left_count1;
			else if (left_max2 > right_min1)
				left_count1_min = left_count1 + 1;
			else
				break;
		}
		
		if ((len1 + len2) % 2)
			return min(right_min1, right_min2);
		else {
			int left_max = max(left_max1, left_max2);
			int right_min = min(right_min1, right_min2);
			return (left_max + right_min) / 2.0;
		}
	}
};
