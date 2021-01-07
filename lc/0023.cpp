/**
 * Definition for singly-linked list.
 * struct ListNode {
 *	 int val;
 *	 ListNode *next;
 *	 ListNode() : val(0), next(nullptr) {}
 *	 ListNode(int x) : val(x), next(nullptr) {}
 *	 ListNode(int x, ListNode *next) : val(x), next(next) {}
 * };
 */
class Solution {
public:
	class node_heap {
	public:
		int count;
		ListNode* nodes[10010];

		void insert(ListNode * node) {
			this->nodes[this->count] = node;
			++count;
			this->heap_up(this->count - 1);
		}

		void change_head(ListNode *node) {
			this->nodes[0] = node;
			this->heap_down(0);

			while (count > 0 && this->nodes[count - 1] == nullptr)
				--count;
		}

		void clear() {
			this->count = 0;
		}

	private:
		static bool compare(ListNode *lhs, ListNode *rhs) {
			if (!rhs)
				return true;
			if (!lhs)
				return false;
			return lhs->val < rhs->val;
		}

		void swap(int lhs, int rhs) {
			ListNode *tmp = this->nodes[lhs];
			this->nodes[lhs] = this->nodes[rhs];
			this->nodes[rhs] = tmp;
		}

		void heap_up(int index) {
			if(!index)
				return;
			int parent = (index - 1) / 2;
			if (compare(this->nodes[index], this->nodes[parent])) {
				this->swap(parent, index);
				this->heap_up(parent);
			}
		}

		void heap_down(int index) {
			int left = index * 2 + 1;
			int right = index * 2 + 2;
			if (left >= count)
				return;
			int target = index;
			if (compare(this->nodes[left], this->nodes[index])) {
				if (right < count &&
						compare(this->nodes[right], this->nodes[left]))
					target = right;
				else
					target = left;
			} else if (right < count &&
					compare(this->nodes[right], this->nodes[index]))
				target = right;

			if (target != index) {
				this->swap(target, index);
				this->heap_down(target);
			}
		}
	};

	static node_heap h;

	ListNode* mergeKLists(vector<ListNode*>& lists) {
		ListNode anchor, *ptr = &anchor;
		h.clear();
		for (ListNode *node : lists)
			h.insert(node);

		while (h.count && h.nodes[0]) {
			ptr->next = h.nodes[0];
			ptr = ptr->next;
			h.change_head(h.nodes[0]->next);
		}

		return anchor.next;
	}
};
