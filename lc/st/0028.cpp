/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
struct bfs_stage {
	TreeNode *arr[1010];
	int size;
};
bfs_stage b1, b2;

class Solution {
public:
	bool isSymmetric(TreeNode* root) {
		if (!root)
			return true;
		bfs_stage *from = &b1, *to = &b2;
		from->size = 0;
		to->arr[0] = root;
		to->size = 1;

		while (to->size) {
			std::swap(from, to);
			int l = 0, r = from->size - 1;
			while (l < r) {
				if (!from->arr[l]) {
					if (from->arr[r])
						return false;
				} else if (!from->arr[r]) {
					return false;
				} else if (from->arr[l]->val != from->arr[r]->val) {
					return false;
				}
				++l;
				--r;
			}

			to->size = 0;
			for (int i = 0; i < from->size; ++i) {
				TreeNode *node = from->arr[i];
				if (node) {
					to->arr[to->size++] = node->left;
					to->arr[to->size++] = node->right;
				}
			}
		}

		return true;
	}
};
