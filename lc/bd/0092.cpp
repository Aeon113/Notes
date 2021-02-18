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
	// Must ensure begin != last
	static void reverse_aux(ListNode *begin, ListNode *last) {
		ListNode *parent = nullptr, *ptr = begin, *des = nullptr;
		
		for (;;) {
			des = ptr->next;

			ptr->next = parent;
			if (ptr == last)
				break;
			parent = ptr;
			ptr = des;
		}
	}

	ListNode* reverseBetween(ListNode* head, int m, int n) {
		ListNode anchor(0, head);
		ListNode *parent = &anchor,
			 *begin = nullptr,
			 *last = nullptr,
			 *des = nullptr;

		if (m == n)
			return head;

		int index = 1;
		for (index = 1; index < m; ++index)
			parent = parent->next;
		begin = parent->next;
		last = begin;

		for (; index < n; ++index)
			last = last->next;
		des = last->next;

		reverse_aux(begin, last);
		parent->next = last;
		begin->next = des;

		return anchor.next;
	}
};
