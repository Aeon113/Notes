/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
void aux(TreeNode *node) {
	std::swap(node->left, node->right);
	if (node->left)
		aux(node->left);
	if (node->right)
		aux(node->right);
}

class Solution {
public:
	TreeNode* mirrorTree(TreeNode* root) {
		if (root)
			aux(root);

		return root;
	}
};
