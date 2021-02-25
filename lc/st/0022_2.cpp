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
	ListNode* getKthFromEnd(ListNode* head, int k) {
		if (!head)
			return nullptr;
		int n = 1;
		ListNode *ptr = head;

		while (ptr->next) {
			++n;
			ptr = ptr->next;
		}
		if (k > n)
			return nullptr;
		ptr = head;
		while (n > k) {
			ptr = ptr->next;
			--n;
		}
		return ptr;
	}
};
