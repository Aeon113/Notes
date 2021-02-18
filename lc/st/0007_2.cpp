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
	static std::unordered_map<int, int> m;
	static bool first;
	static TreeNode *build_sub_tree(std::vector<int> &preorder, std::vector<int> &inorder,
					int inorder_begin, int inorder_end,
					int &preorder_index, int size) {
		if (inorder_begin >= inorder_end || preorder_index >= size)
			return nullptr;

		int val = preorder[preorder_index];
		auto inorder_index_iter = Solution::m.find(val);
		if (inorder_index_iter == Solution::m.end())
			return nullptr;
		int inorder_index = inorder_index_iter->second;
		if (inorder_index < inorder_begin || inorder_index >= inorder_end)
			return nullptr;

		TreeNode *node = new TreeNode(val);
		++preorder_index;

		node->left = build_sub_tree(preorder, inorder,
					    inorder_begin, inorder_index,
					    preorder_index, size);
		node->right = build_sub_tree(preorder, inorder,
					     inorder_index + 1, inorder_end,
					     preorder_index, size);

		return node;
	}

	TreeNode* buildTree(vector<int>& preorder, vector<int>& inorder) {
		int size = preorder.size();
		if (!size)
			return nullptr;
		if (Solution::first) {
			Solution::m.reserve(5000);
			Solution::first = false;
		}
		Solution::m.clear();

		for (int i = 0; i < size; ++i)
			Solution::m.emplace(inorder[i], i);

		int preorder_index = 0;
		TreeNode *tree = build_sub_tree(preorder, inorder,
						0, size,
						preorder_index, size);

		return tree;
	}
};

std::unordered_map<int, int> Solution::m;
bool Solution::first = true;
