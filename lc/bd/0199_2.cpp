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
struct recursion_state {
	TreeNode *node;
	int child;

	recursion_state(): node(nullptr), child(0) {}
	explicit recursion_state(TreeNode *n): node(n), child(0) {}
};

class Solution {
public:
	static std::vector<recursion_state> rstates;
	static bool first;
	vector<int> rightSideView(TreeNode* root) {
		std::vector<int> result;
		int row;
		int max_row;
		if (!root)
			goto final;

		if (first) {
			first = false;
			rstates.reserve(10000);
		}

		rstates.clear();
		row = 0;
		max_row = -1;
		rstates.emplace_back(root);

		while (!rstates.empty()) {
			recursion_state &state = rstates.back();

			switch (state.child) {
			case 0:
				++state.child;
				if (row > max_row) {
					max_row = row;
					result.push_back(state.node->val);
				}
				break;

			case 1:
				++state.child;
				if (state.node->right) {
					rstates.emplace_back(state.node->right);
					++row;
				}
				break;

			case 2:
				++state.child;
				if (state.node->left) {
					rstates.emplace_back(state.node->left);
					++row;
				}
				break;

			default:
				rstates.pop_back();
				--row;
			}
		}

final:
		return result;
	}
};

std::vector<recursion_state> Solution::rstates;
bool Solution::first = true;
