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
class Solution {
public:
	static int max_row;
	static void right_view_dfs(std::vector<int> &result, TreeNode *node, int row) {
		if (max_row < row) {
			max_row = row;
			result.push_back(node->val);
		}
		if (node->right)
			right_view_dfs(result, node->right, row + 1);
		if (node->left)
			right_view_dfs(result, node->left, row + 1);
	}

	vector<int> rightSideView(TreeNode* root) {
		std::vector<int> result;
		if (!root)
			goto final;

		Solution::max_row = -1;
		Solution::right_view_dfs(result, root, 0);

final:
		return result;
	}
};

int Solution::max_row;
