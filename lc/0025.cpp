/**
 * Definition for singly-linked list.
 * struct ListNode {
 *     int val;
 *     ListNode *next;
 *     ListNode() : val(0), next(nullptr) {}
 *     ListNode(int x) : val(x), next(nullptr) {}
 *     ListNode(int x, ListNode *next) : val(x), next(next) {}
 * };
 */
class Solution {
public:
	static void aux(ListNode *head, ListNode *tail) {
		if (head == tail) {
			head->next = nullptr; // TODO: delete this
			return;
		}

		ListNode anchor(0, head);
		ListNode *parent = &anchor, *ptr = head, *des = head->next;
		for (;;) {
			ptr->next = parent;
            if (ptr == tail)
				break;
			parent = ptr;
			ptr = des;
			
			des = des->next;
		}

		head->next = nullptr; // TODO: delete this
	}
	
	ListNode* reverseKGroup(ListNode* head, int k) {
		if (!head || !head->next || k == 1)
			return head;
		ListNode anchor(0, head);
		ListNode *parent = &anchor, *p1 = head, *p2 = head;
		int count = 1;

		for (;;) {
			if (!p2->next) {
				if (count == k) {
					aux(p1, p2);
					parent->next = p2;
					p1->next = nullptr;
				}
				break;
			} else if (count < k) {
				++count;
				p2 = p2->next;
			} else {
				ListNode *des = p2->next;
				aux(p1, p2);
				parent->next = p2;
				p1->next = des;
				parent = p1;
				p1 = des;
				p2 = des;
				count = 1;
			}
		}

		return anchor.next;
	}
};
