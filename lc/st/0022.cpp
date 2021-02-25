/**
 * Definition for singly-linked list.
 * struct ListNode {
 *	 int val;
 *	 ListNode *next;
 *	 ListNode(int x) : val(x), next(NULL) {}
 * };
 */
std::vector<ListNode *> nodes;
int nodes_size = 0;

class Solution {
public:
	ListNode* getKthFromEnd(ListNode* head, int k) {
		nodes_size = std::max(nodes_size, (int) nodes.size());
		nodes.reserve(nodes_size);
		nodes.clear();

		ListNode *ptr = head;

		while (ptr) {
			nodes.push_back(ptr);
			ptr = ptr->next;
		}

		int size = nodes.size();
		if (k > size || !k)
			return nullptr;
		else
			return nodes[size - k];
	}
};
