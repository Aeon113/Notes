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
	vector<int> reversePrint(ListNode* head) {
		ListNode *ptr = head;
		std::vector<int> result;
		while (ptr) {
			result.push_back(ptr->val);
			ptr = ptr->next;
		}

		std::reverse(std::begin(result), std::end(result));
		return result;
	}
};
