/**
 * Definition for a binary tree node.
 * struct TreeNode {
 *	 int val;
 *	 TreeNode *left;
 *	 TreeNode *right;
 *	 TreeNode(int x) : val(x), left(NULL), right(NULL) {}
 * };
 */
struct bfs_staging{
	TreeNode *arr[5050];
	int size;
	bfs_staging() : size(0) {};
};

bfs_staging bfs_a1, bfs_a2;
bfs_staging bfs_b1, bfs_b2;

bool subtree_match(TreeNode *aroot, TreeNode *b) {
	bfs_staging *afrom = &bfs_a2, *ato = &bfs_a1;
	bfs_staging *bfrom = &bfs_b2, *bto = &bfs_b1;
	bfs_a1.size = 1;
	bfs_a1.arr[0] = aroot;
	bfs_a2.size = 0;
	bfs_b1.size = 1;
	bfs_b1.arr[0] = b;
	bfs_b2.size = 0;

	while(bto->size) {
		std::swap(afrom, ato);
		std::swap(bfrom, bto);
		ato->size = 0;
		bto->size = 0;

		for (int i = 0; i < bfrom->size; ++i) {
			TreeNode *bnode = bfrom->arr[i];
			TreeNode *anode = afrom->arr[i];
			if (bnode->left) {
				if (!anode->left || anode->left->val != bnode->left->val)
					return false;
				ato->arr[ato->size++] = anode->left;
				bto->arr[bto->size++] = bnode->left;
			}
			if (bnode->right) {
				if (!anode->right || anode->right->val != bnode->right->val)
					return false;
				ato->arr[ato->size++] = anode->right;
				bto->arr[bto->size++] = bnode->right;
			}
		}
	}

	return true;
}

bfs_staging aroot1, aroot2;
class Solution {
public:
	bool isSubStructure(TreeNode* A, TreeNode* B) {
		if (!B || !A)
			return false;
		aroot1.size = 0;
		aroot2.arr[0] = A;
		aroot2.size = 1;
		bfs_staging *from = &aroot1, *to = &aroot2;
		while(to->size) {
			std::swap(from, to);
			to->size = 0;
			for (int i = 0; i < from->size; ++i) {
				if (from->arr[i]->val == B->val &&	\
						subtree_match(from->arr[i], B))
					return true;
				if (from->arr[i]->left)
					to->arr[to->size++] = from->arr[i]->left;
				if (from->arr[i]->right)
					to->arr[to->size++] = from->arr[i]->right;
			}
		}
		return false;
	}
};
