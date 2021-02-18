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
	static void aux(std::vector<int> &result, ListNode *node) {
		if (node == nullptr)
			return;
		aux(result, node->next);
		result.push_back(node->val);
	}

	vector<int> reversePrint(ListNode* head) {
		std::vector<int> result;
		aux(result, head);

		return result;
	}
};
