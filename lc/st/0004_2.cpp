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

		this->left_column = 0;
		this->right_column = columns - 1;
		this->up_row = 0;
		this->down_row = rows - 1;

		int result = false;
		for(;;) {
			if (this->left_column > this->right_column || this->up_row > this->down_row)
				break;

			if (matrix[this->up_row][this->left_column] == target ||
			    matrix[this->up_row][this->right_column] == target ||
			    matrix[this->down_row][this->left_column] == target ||
			    matrix[this->down_row][this->right_column] == target) {
				result = true;
				break;
			}

			this->bindel(matrix, target);
		}

		return result;
	}

private:
	int left_column;
	int right_column;
	int up_row;
	int down_row;

	void bindel(const std::vector<std::vector<int>> &matrix, int target) {
		int sub_left_column, sub_right_column;
		int sub_up_row, sub_down_row;
		sub_left_column = this->left_column;
		sub_right_column = this->right_column;
		while (sub_left_column <= sub_right_column) {
			int mid_column = (sub_left_column + sub_right_column) / 2;
			if (matrix[this->down_row][mid_column] < target)
				sub_left_column = mid_column + 1;
			else
				sub_right_column = mid_column - 1;
		}
		this->left_column = sub_left_column;

		sub_left_column = this->left_column;
		sub_right_column = this->right_column;
		while (sub_left_column <= sub_right_column) {
			int mid_column = (sub_left_column + sub_right_column) / 2;
			if (matrix[this->up_row][mid_column] > target)
				sub_right_column = mid_column - 1;
			else
				sub_left_column = mid_column + 1;
		}
		this->right_column = sub_right_column;

		if (this->left_column > this->right_column)
			return;

		sub_up_row = this->up_row;
		sub_down_row = this->down_row;
		while (sub_up_row <= sub_down_row) {
			int mid_row = (sub_up_row + sub_down_row) / 2;
			if (matrix[mid_row][right_column] < target)
				sub_up_row = mid_row + 1;
			else
				sub_down_row = mid_row - 1;
		}
		this->up_row = sub_up_row;

		sub_up_row = this->up_row;
		sub_down_row = this->down_row;
		while (sub_up_row <= sub_down_row) {
			int mid_row = (sub_up_row + sub_down_row) / 2;
			if (matrix[mid_row][left_column] > target)
				sub_down_row = mid_row - 1;
			else
				sub_up_row = mid_row + 1;
		}
		this->down_row = sub_down_row;
	}
};
