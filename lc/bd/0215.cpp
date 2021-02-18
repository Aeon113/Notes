#define get_left_child(i) ((i) * 2 + 1)
#define get_right_child(i) ((i) * 2 + 2)
#define get_parent(i) (((i) - 1) / 2)

class Solution {
public:
	static std::vector<int> h;
	static int hsize;
	static bool first;

	static void lift_up(int index) {
		if (!index)
			return;
		int parent = get_parent(index);
		if (h[parent] > h[index])
			std::swap(h[parent], h[index]);
		lift_up(parent);
	}

	static void sink_down(int index) {
		int smallest = index;
		int left = get_left_child(index);
		int right = get_right_child(index);
		if (left >= hsize)
			return;
		if (h[left] < h[smallest])
			smallest = left;
		if (right < hsize && h[right] < h[smallest])
			smallest = right;

		if (smallest != index) {
			std::swap(h[smallest], h[index]);
			lift_up(index);
			sink_down(smallest);
		}
	}

	static void insert_to_heap(int num) {
		int index;
	}

	int findKthLargest(vector<int>& nums, int k) {
		if (first) {
			first = false;
			h.reserve(10000);
		}
		h.clear();
		hsize = 0;

		for (auto n : nums) {
			if (hsize < k) {
				h.push_back(n);
				++hsize;
				lift_up(hsize - 1);
			} else {
				if (n >= h.front()) {
					h.front() = n;
					sink_down(0);
					// h.front() = h.back();
					// h.pop_back();
					// --hsize;
					// sink_down(0);
					// h.push_back(n);
					// ++hsize;
					// lift_up(hsize - 1);
				}
			}
		}

		return h.front();
	}
};

std::vector<int> Solution::h;
int Solution::hsize;
bool Solution::first = true;
