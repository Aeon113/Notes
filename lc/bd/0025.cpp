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
	static void reverse_nodes(ListNode *head, ListNode *tail) {
		ListNode *parent = nullptr, *ptr = head, *des = nullptr;

		for (;;) {
			des = ptr->next;
			ptr->next = parent;

			if (ptr == tail)
				break;
			parent = ptr;
			ptr = des;
		}
	}

	ListNode* reverseKGroup(ListNode* head, int k) {
		if (k <= 1 || head == nullptr)
			return head;

		ListNode anchor(0, head);

		ListNode *_parent = &anchor,
			 *_tail = &anchor;
		int count = 0;

		while (_tail->next) {
			_tail = _tail->next;
			++count;

			if (count == k) {
				ListNode *_head = _parent->next;
				ListNode *_des = _tail->next;
				reverse_nodes(_head, _tail);

				_parent->next = _tail;
				_head->next = _des;

				_parent = _head;
				_tail = _head;
				count = 0;
			}
		}

		return anchor.next;
	}
};
