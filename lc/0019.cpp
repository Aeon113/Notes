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
	static int index;
	static int aux(ListNode *node, ListNode *parent) {
		if (!node)
			return 0;
		int val = aux(node->next, node) + 1;
		if (val == index) {
			parent->next = node->next;
		}

		return val;
	}

	ListNode* removeNthFromEnd(ListNode* head, int n) {
		ListNode node = ListNode(0, head);
		index = n;
		aux(node.next, &node);
		return node.next;
	}
};

int Solution::index = 0;
