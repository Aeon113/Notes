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
	ListNode* addTwoNumbers(ListNode* l1, ListNode* l2) {
		ListNode node, *ptr = &node;
		int carry = 0;
		
		for (;;) {
			if (l1 == nullptr) {
				if (l2 != nullptr) {
					while (l2) {
						carry += l2->val;
						ptr->next = new ListNode(carry % 10);
						carry /= 10;
						ptr = ptr->next;
						l2 = l2->next;
					}
				}
				if (carry)
					ptr->next = new ListNode(carry);
				break;
			} else if (l2 == nullptr) {
				while (l1) {
					carry += l1->val;
					ptr->next = new ListNode(carry % 10);
					carry /= 10;
					ptr = ptr->next;
					l1 = l1->next;
				}
				if (carry)
					ptr->next = new ListNode(carry);
				break;
			} else {
				carry += l1->val + l2->val;
				ptr->next = new ListNode(carry % 10);
				carry /= 10;
				ptr = ptr->next;
				l1 = l1->next;
				l2 = l2->next;
			}
		}
		return node.next;
	}
};
