/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode() : val(0), left(nullptr), right(nullptr) {}
 *	 TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
 *	 TreeNode(int x, TreeNode *left, TreeNode *right) : val(x), left(left), right(right) {}
 * };
 */

int max_path;
int path_search(TreeNode *node) {
	int left_root_val = -0x3f3f3f3f;
	int right_root_val = -0x3f3f3f3f;
	if (node->left)
		left_root_val = path_search(node->left);
	if (node->right)
		right_root_val = path_search(node->right);

	left_root_val = std::max(left_root_val, 0);
	right_root_val = std::max(right_root_val, 0);

	max_path = std::max(max_path, left_root_val + right_root_val + node->val);
	return std::max(left_root_val + node->val, right_root_val + node->val);
}

class Solution {
public:
	int maxPathSum(TreeNode* root) {
		if (!root)
			return 0;
		max_path = -0x3f3f3f3f;
		path_search(root);

		return max_path;
	}
};
