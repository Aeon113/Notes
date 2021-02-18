/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
class Solution {
public:
	static void line_traverse(std::vector<std::vector<int>> &result,
				  TreeNode *root) {
		std::queue<TreeNode *> q1, q2;
		std::queue<TreeNode *> *qfrom = &q1, *qto = &q2;

		if (nullptr == root)
			return;

		qfrom->push(root);

		while (!qfrom->empty()) {
			std::vector<int> tmp;

			while (!qfrom->empty()) {
				TreeNode *node = qfrom->front();
				qfrom->pop();

				tmp.push_back(node->val);
				if (nullptr != node->left)
					qto->push(node->left);
				if (nullptr != node->right)
					qto->push(node->right);
			}

			result.emplace_back(std::move(tmp));
			std::swap(qfrom, qto);
		}
	}

	vector<vector<int>> levelOrder(TreeNode* root) {
		std::vector<std::vector<int>> result;
		line_traverse(result, root);

		return result;
	}
};
