/**
 * Definition for singly-linked list.
 * struct ListNode {
 *	 int val;
 *	 ListNode *next;
 *	 ListNode(int x) : val(x), next(NULL) {}
 * };
 */
class Solution {
public:
	ListNode* mergeTwoLists(ListNode* l1, ListNode* l2) {
		ListNode anchor(0);
		ListNode *ptr = &anchor, *lhs = l1, *rhs = l2;

		for (;;) {
			if (!l1) {
				if (l2)
					ptr->next = l2;
				break;
			}
			if (!l2) {
				ptr->next = l1;
				break;
			}
			if (l1->val < l2->val) {
				ptr->next = l1;
				l1 = l1->next;
			} else {
				ptr->next = l2;
				l2 = l2->next;
			}
			ptr = ptr->next;
		}

		return anchor.next;
	}
};
