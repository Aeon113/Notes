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
    ListNode* mergeTwoLists(ListNode* l1, ListNode* l2) {
	    ListNode node, *ptr = &node, *ptr1 = l1, *ptr2 = l2;
	    for (;;) {
		    if (!ptr1) {
			    if (ptr2)
				    ptr->next = ptr2;
			    break;
		    } else if (!ptr2) {
			    ptr->next = ptr1;
			    break;
		    } else {
			    if (ptr1->val < ptr2->val) {
				    ptr->next = ptr1;
				    ptr = ptr->next;
				    ptr1 = ptr1->next;
			    } else {
				    ptr->next = ptr2;
				    ptr = ptr->next;
				    ptr2 = ptr2->next;
			    }
		    }
	    }

	    return node.next;
    }
};
