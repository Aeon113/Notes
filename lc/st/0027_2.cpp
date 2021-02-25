/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */

struct bfs_staging {
	TreeNode *arr[510];
	int size;
};

bfs_staging s1, s2;
class Solution {
public:
	TreeNode* mirrorTree(TreeNode* root) {
		if (!root)
			return root;
		bfs_staging *from = &s1, *to = &s2;
		to->arr[0] = root;
		to->size = 1;
		
		while (to->size) {
			std::swap(from, to);
			to->size = 0;

			for (int i = 0; i < from->size; ++i) {
				std::swap(from->arr[i]->left, from->arr[i]->right);
				if (from->arr[i]->left)
					to->arr[to->size++] = from->arr[i]->left;
				if (from->arr[i]->right)
					to->arr[to->size++] = from->arr[i]->right;
			}
		}

		return root;
	}
};
