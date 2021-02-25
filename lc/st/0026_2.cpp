/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
bool match(TreeNode *aroot, TreeNode *b) {
	if (b->left) {
		if (!aroot->left || aroot->left->val != b->left->val)
			return false;
		if (!match(aroot->left, b->left))
			return false;
	}
	if (b->right) {
		if (!aroot->right || aroot->right->val != b->right->val)
			return false;
		if (!match(aroot->right, b->right))
			return false;
	}
	return true;
}

bool aux(TreeNode *a, TreeNode *b) {
	if (a->val == b->val && match(a, b))
		return true;
	if (a->left && aux(a->left, b))
		return true;
	return a->right && aux(a->right, b);
}

class Solution {
public:
	bool isSubStructure(TreeNode* A, TreeNode* B) {
		if (!A || !B)
			return false;
		return aux(A, B);
	}
};
