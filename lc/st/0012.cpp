struct rec_record {
	int row;
	int column;
} rec[40100];

int mtrace[210][210];

class Solution {
public:
	bool aux_loop(const std::vector<std::vector<char>> &board,
		      const std::string &word,
		      int first_row,
		      int first_column,
		      int rows,
		      int columns,
		      int word_size) {
		int s_i = 0;
		rec[0] = {.row = first_row, .column = first_column};
		bool found = false;

		while (s_i >= 0) {
			if (s_i == word_size - 1) {
				found = true;
				break;
			}
			int row = rec[s_i].row;
			int column = rec[s_i].column;
			switch(mtrace[row][column]) {
			case 0:
				++mtrace[row][column];
				if (row > 0 &&	\
				    !mtrace[row - 1][column] &&	\
				    board[row - 1][column] == word[s_i + 1]) {
					rec[s_i + 1] = {
						.row = row - 1,
						.column = column
					};
					++s_i;
					break;
				}
			case 1:
				++mtrace[row][column];
				if (row < rows - 1 &&	\
				    !mtrace[row + 1][column] &&	\
				    board[row + 1][column] == word[s_i + 1]) {
					rec[s_i + 1] = {
						.row = row + 1,
						.column = column
					};
					++s_i;
					break;
				}
			case 2:
				++mtrace[row][column];
				if (column > 0 &&	\
				    !mtrace[row][column - 1] &&	\
				    board[row][column - 1] == word[s_i + 1]) {
					rec[s_i + 1] = {
						.row = row,
						.column = column - 1
					};
					++s_i;
					break;
				}
			case 3:
				++mtrace[row][column];
				if (column < columns - 1 &&	\
				    !mtrace[row][column + 1] &&	\
				    board[row][column + 1] == word[s_i + 1]) {
					rec[s_i + 1] = {
						.row = row,
						.column = column + 1
					};
					++s_i;
					break;
				}
			default:
				mtrace[row][column] = 0;
				--s_i;
			}
		}

		while (s_i >= 0) {
			mtrace[rec[s_i].row][rec[s_i].column] = 0;
			--s_i;
		}
		return found;
	}

	bool exist(vector<vector<char>>& board, string word) {
		int rows = board.size();
		int columns = board.front().size();
		int word_size = word.size();

		if (word_size > rows * columns)
			return false;

		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < columns; ++c) {
				if (board[r][c] == word.front() &&	\
				    aux_loop(board, word, r, c, rows, columns, word_size))
					return true;
			}
		}

		return false;
	}
};

