class Solution {
public:
	vector<int> spiralOrder(vector<vector<int>>& matrix) {
		int rows, columns;
		int rmin, cmin, rmax, cmax;
		int r, c;

		result.reserve(10000);
		result.clear();
		if (matrix.empty())
			goto final;
		if (matrix.front().empty())
			goto final;

		rows = matrix.size();
		columns = matrix.front().size();

		rmin = 0;
		cmin = 0;
		rmax = rows - 1;
		cmax = columns - 1;

		while (rmin < rmax && cmin < cmax) {
			// [rmin, cmin] -> [rmin, cmax - 1]
			r = rmin;
			for (c = cmin; c < cmax; ++c)
				result.push_back(matrix[r][c]);
			// [rmin, cmax] -> [rmax - 1, cmax]
			c = cmax;
			for (r = rmin; r < rmax; ++r)
				result.push_back(matrix[r][c]);
			// [rmax, cmax] -> [rmax, cmin + 1]
			r = rmax;
			for (c = cmax; c > cmin; --c)
				result.push_back(matrix[r][c]);
			// [rmax, cmin] -> [rmin + 1, cmin]
			c = cmin;
			for (r = rmax; r > rmin; --r)
				result.push_back(matrix[r][c]);
			++rmin;
			++cmin;
			--rmax;
			--cmax;
		}

		if (rmin == rmax) {
			while (cmin <= cmax) {
				result.push_back(matrix[rmin][cmin]);
				++cmin;
			}
		} else if (cmin == cmax) {
			while (rmin <= rmax) {
				result.push_back(matrix[rmin][cmin]);
				++rmin;
			}
		}

final:
		return result;
	}
};
