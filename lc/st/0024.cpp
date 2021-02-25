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
	ListNode* reverseList(ListNode* head) {
		if (!head)
			return nullptr;
		ListNode anchor(0);
		anchor.next = head;
		ListNode *pnt = &anchor, *ptr = head, *des = nullptr;

		while (ptr) {
			des = ptr->next;
			ptr->next = pnt;
			
			pnt = ptr;
			ptr = des;
		}

		head->next = nullptr;

		return pnt;
	}
};
