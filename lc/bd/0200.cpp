int adjm[310][310];
int rows;
int columns;

void search(const std::vector<std::vector<char>> &grid, int adjcnt, int row, int column) {
	adjm[row][column] = adjcnt;
	if (row && grid[row - 1][column] == '1' && !adjm[row - 1][column])
		search(grid, adjcnt, row - 1, column);
	if (row < rows - 1 && grid[row + 1][column] == '1' && !adjm[row + 1][column])
		search(grid, adjcnt, row + 1, column);
	if (column && grid[row][column - 1] == '1' && !adjm[row][column - 1])
		search(grid, adjcnt, row, column - 1);
	if (column < columns - 1 && grid[row][column + 1] == '1' && !adjm[row][column + 1])
		search(grid, adjcnt, row, column + 1);
}

class Solution {
public:
	int numIslands(vector<vector<char>>& grid) {
		rows = grid.size();
		if (!rows)
			return 0;
		columns = grid.front().size();
		if (!columns)
			return 0;

		for (int i = 0; i < rows; ++i)
			memset(adjm + i, 0, columns * sizeof(int));

		int adjcnt = 0;
		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < columns; ++j) {
				if (grid[i][j] == '1' && !adjm[i][j]) {
					++adjcnt;
					search(grid, adjcnt, i, j);
				}
			}
		}

		return adjcnt;
	}
};
