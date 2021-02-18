class Solution {
public:
	bool findNumberIn2DArray(vector<vector<int>>& matrix, int target) {
		int rows, columns;
		rows = matrix.size();
		if (!rows)
			return false;
		columns = matrix.front().size();
		if (!columns)
			return false;
		this->up_row = 0;
		this->down_row = rows - 1;
		this->left_column = 0;
		this->right_column = columns - 1;

		int found = false;
		for (;;) {
			if (this->up_row > this->down_row || this->left_column > this->right_column)
				break;

			if (matrix[this->up_row][this->left_column] == target ||
			    matrix[this->up_row][this->right_column] == target ||
			    matrix[this->down_row][this->left_column] == target ||
			    matrix[this->down_row][this->right_column] == target) {
				found = true;
				break;
			}

			this->seqdel(matrix, target);
		}

		return found;
	}

private:
	int up_row;
	int down_row;
	int left_column;
	int right_column;

	void seqdel(const std::vector<std::vector<int>> &matrix, int target) {
		while (this->up_row <= this->down_row) {
			if (matrix[this->up_row][this->right_column] < target)
				++this->up_row;
			else
				break;
		}

		while (this->up_row <= this->down_row) {
			if (matrix[this->down_row][this->left_column] > target)
				--this->down_row;
			else
				break;
		}

		if (this->up_row > this->down_row)
			return;

		while (this->left_column <= this->right_column) {
			if (matrix[this->down_row][this->left_column] < target)
				++this->left_column;
			else
				break;
		}

		while (this->left_column <= this->right_column) {
			if (matrix[this->up_row][this->right_column] > target)
				--this->right_column;
			else
				break;
		}
	}

};
