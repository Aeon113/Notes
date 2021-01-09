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
	static void aux(ListNode *head, ListNode *tail) {
		ListNode anchor(0, head);
		ListNode *parent = &anchor, *ptr = head, *dec;

		for (;;) {
			if (!ptr)
				break;
			dec = ptr->next;
			ptr->next = parent;
			parent = ptr;
			ptr = dec;
		}
	}

	ListNode* reverseKGroup(ListNode* head, int k) {
		if (k == 1 || !head)
			return head;
		ListNode anchor(0, head);
		ListNode *p1 = head, *p2 = head, *parent = &anchor;
		int count = 1;
		for (;;) {
			if (!p2->next) {
				if (p1 != p2) {
					aux(p1, p2);
					parent->next = p2;
					p1->next = nullptr;
					ListNode *ptr = anchor.next;
					while (ptr) {
						std::cout << ptr->val << '\t';
						ptr = ptr->next;
					}
					std::cout << std::endl;
				}
				break;
			} else if (count == k) {
				ListNode *dec = p2->next;
				aux(p1, p2);
				parent->next = p2;
				p1->next = dec;
				ListNode *ptr = anchor.next;
				while (ptr != dec) {
					std::cout << ptr->val << '\t';
					ptr = ptr->next;
				}
				std::cout << std::endl;
				if (!dec)
					break;
				parent = p1;
				p1 = dec;
				p2 = dec;
				count = 1;
			} else {
				++count;
				p2 = p2->next;
			}
		}

		return anchor.next;
	}
};
