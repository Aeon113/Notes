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
	static ListNode *nodes[31];
public:
	ListNode* removeNthFromEnd(ListNode* head, int n) {
		ListNode *ptr = head;
		int count = 0;
		memset(nodes, 0, sizeof(nodes));
		while (ptr) {
			nodes[count] = ptr;
			ptr = ptr->next;
			++count;
		}

		if (n >= count)
			return head->next;
		nodes[count - n - 1]->next = nodes[count - n + 1];
		return head;
	}
};
ListNode *Solution::nodes[31];
