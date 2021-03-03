#include <vector>
using std::vector;

/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */

struct dfs_track {
	TreeNode *node;
	int state;
};

dfs_track dfs[10010];
int dfs_size;

inline void dfs_clear() {
	dfs_size = 0;
}

inline dfs_track& dfs_top() {
	return dfs[dfs_size - 1];
}

inline void dfs_pop() {
	--dfs_size;
}

inline bool dfs_empty() {
	return !dfs_size;
}

inline void dfs_push(TreeNode *node) {
	dfs[dfs_size].node = node;
	dfs[dfs_size].state = 0;
	++dfs_size;
}

void update_result(std::vector<std::vector<int>> &result) {
	std::vector<int> tmp;

	for (int i = 0; i < dfs_size; ++i)
		tmp.push_back(dfs[i].node->val);

	result.emplace_back(std::move(tmp));
}

class Solution {
public:
	vector<vector<int>> pathSum(TreeNode* root, int sum) {
		std::vector<std::vector<int>> result;
		int cur_sum;
		if (!root)
			goto final;
		
		dfs_clear();
		dfs_push(root);
		cur_sum = 0;

		while (!dfs_empty()) {
			auto &dfs_cur = dfs_top();
			if (!dfs_cur.node->left && !dfs_cur.node->right) {
				if (cur_sum + dfs_cur.node->val == sum)
					update_result(result);
				dfs_pop();
				continue;
			}

			switch (dfs_cur.state) {
			case 0:
				cur_sum += dfs_cur.node->val;
				++dfs_cur.state;
				if (dfs_cur.node->left) {
					dfs_push(dfs_cur.node->left);
					break;
				}
			case 1:
				++dfs_cur.state;
				if (dfs_cur.node->right) {
					dfs_push(dfs_cur.node->right);
					break;
				}
			case 2:
				cur_sum -= dfs_cur.node->val;
				dfs_pop();
				break;
			}
		}

final:
		return result;
	}
};
