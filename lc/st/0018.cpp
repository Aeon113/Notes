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
	ListNode* deleteNode(ListNode* head, int val) {
		if (!head)
			return head;
		ListNode anchor(0, head);
		ListNode *pnt = nullptr, *ptr = nullptr, *des = nullptr;
		pnt = &anchor;
		ptr = head;

		while (ptr) {
			des = ptr->next;
			if (ptr->val == val) {
				pnt->next = des;
				break;
			}

			pnt = ptr;
			ptr = des;
		}

		return anchor.next;
	}
};
