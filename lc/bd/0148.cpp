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
	static void range_sort(ListNode *lhs_begin,
			       ListNode *lhs_last,
			       ListNode *rhs_begin,
			       ListNode *rhs_last,
			       ListNode * &new_begin,
			       ListNode * &new_last) {
		ListNode anchor;
		ListNode *ptr = &anchor;

		for (;;) {
			if (lhs_begin->val < rhs_begin->val) {
				ptr->next = lhs_begin;
				if (lhs_begin == lhs_last) {
					lhs_last->next = rhs_begin;
					new_last = rhs_last;
					break;
				} else
					lhs_begin = lhs_begin->next;
			} else {
				ptr->next = rhs_begin;
				if (rhs_begin == rhs_last) {
					rhs_last->next = lhs_begin;
					new_last = lhs_last;
					break;
				} else
					rhs_begin = rhs_begin->next;
			}

			ptr = ptr->next;
		}

		new_begin = anchor.next;
	}

	ListNode* sortList(ListNode* head) {
		ListNode anchor(0, head);

		if (nullptr == head)
			return head;

		ListNode *pnt = nullptr, *des = nullptr;
		ListNode *lhs_begin = nullptr, *lhs_last = nullptr;
		ListNode *rhs_begin = nullptr, *rhs_last = nullptr;
		ListNode *new_begin = nullptr, *new_last = nullptr;

		for (int sortee_size = 1;; sortee_size *= 2) {
			pnt = &anchor;
			for (;;) {
				lhs_begin = pnt->next;
				if (nullptr == lhs_begin)
					break;
				lhs_last = lhs_begin;
				for (int lhs_size = 1; lhs_size < sortee_size; ++lhs_size) {
					lhs_last = lhs_last->next;
					if (nullptr == lhs_last)
						break;
				}
				if (nullptr == lhs_last)
					break;

				rhs_begin = lhs_last->next;
				rhs_last = rhs_begin;

				if (nullptr == rhs_begin)
					break;

				for (int rhs_size = 1; rhs_size < sortee_size; ++rhs_size) {
					if (nullptr == rhs_last->next)
						break;
					rhs_last = rhs_last->next;
				}
				des = rhs_last->next;

				range_sort(lhs_begin, lhs_last,
					   rhs_begin, rhs_last,
					   new_begin, new_last);
				pnt->next = new_begin;
				new_last->next = des;
				pnt = new_last;
			}
			if (&anchor == pnt)
				break;
		}

		return anchor.next;
	}
};
