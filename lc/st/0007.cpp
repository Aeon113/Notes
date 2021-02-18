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
	static int binsearch(const std::vector<int> &arr,
			     int begin,
			     int last,
			     int target) {
		while (begin <= last) {
			int mid = begin + (last - begin) / 2;
			if (arr[mid] < target)
				begin = mid + 1;
			else if (arr[mid] > target)
				last = mid - 1;
			else
				return mid;
		}

		return -1;
	}

	static int seqsearch(const std::vector<int> &arr,
			     int begin,
			     int last,
			     int target) {
		while (begin <= last) {
			if (arr[begin] == target)
				return begin;
			++begin;
		}
		return -1;
	}

	static TreeNode *aux(const std::vector<int> &preorder,
			     const std::vector<int> &inorder,
			     int inorder_begin,
			     int inorder_last,
			     int &preorder_index,
			     int size) {
		if (preorder_index >= size || inorder_begin > inorder_last)
			return nullptr;

		int val = preorder[preorder_index];
		int inorder_index = seqsearch(inorder,
					       inorder_begin,
					       inorder_last,
					       val);
		if (inorder_index < 0)
			return nullptr;

		TreeNode *node = new TreeNode(val);
		++preorder_index;

		node->left = aux(preorder, inorder,
				 inorder_begin, inorder_index - 1,
				 preorder_index, size);
		node->right = aux(preorder, inorder,
				  inorder_index + 1, inorder_last,
				  preorder_index, size);

		return node;
	}

	TreeNode* buildTree(vector<int>& preorder, vector<int>& inorder) {
		int size = preorder.size();
		if (!size)
			return nullptr;

		int preorder_index = 0;
		TreeNode *tree = aux(preorder, inorder, 0, size - 1, preorder_index, size);

		return tree;
	}
};
