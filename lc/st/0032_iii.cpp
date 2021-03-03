/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */

std::vector<TreeNode *> v1(510, nullptr), v2(510, nullptr);
std::vector<int> level;

class Solution {
public:
	vector<vector<int>> levelOrder(TreeNode* root) {
		std::vector<std::vector<int>> result;
		int fromsz = 0, tosz = 0;
		std::vector<TreeNode *> *from = &v2, *to = &v1;
		unsigned int levels = 0;
		if (!root)
			goto final;
		level.reserve(510);
		level.clear();

		(*to)[0] = root;
		tosz = 1;

		while (tosz) {
			std::swap(from, to);
			fromsz = tosz;
			tosz = 0;

			std::vector<int> tmp;

			for (int i = 0; i < fromsz; ++i) {
				tmp.push_back((*from)[i]->val);
			}
			result.emplace_back(std::move(tmp));
			for (int i = fromsz - 1; i >= 0; --i) {
				if (levels & 1u) {
					if ((*from)[i]->left)
						(*to)[tosz++] = (*from)[i]->left;
					if ((*from)[i]->right)
						(*to)[tosz++] = (*from)[i]->right;
				} else {
					if ((*from)[i]->right)
						(*to)[tosz++] = (*from)[i]->right;
					if ((*from)[i]->left)
						(*to)[tosz++] = (*from)[i]->left;
				}
			}

			++levels;
		}
final:
		return result;
	}
};
