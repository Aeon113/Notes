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
	ListNode* swapPairs(ListNode* head) {
		ListNode anchor;
		anchor.next = head;

		ListNode *ptr = head, *parent = &anchor;
		
		while (ptr) {
			ListNode *des = ptr->next;
			if (!des)
				break;
			ListNode *des_des = des->next;

			parent->next = des;
			des->next = ptr;
			ptr->next = des_des;

			parent = ptr;
			ptr = des_des;
		}

		return anchor.next;
	}
};
