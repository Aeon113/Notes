int matrix[100][100];
struct rec_record {
	int r;
	int c;
} rec[10010];
class Solution {
public:
	int get_dsum(int num) {
		return num / 10 + num % 10;
	}

	int movingCount(int m, int n, int k) {
		memset(matrix, 0, sizeof(matrix));
		int cnt = 0;
		int si = 0;
		rec[0] = {.r = 0, .c = 0};
		while (si >= 0) {
			int r = rec[si].r;
			int c = rec[si].c;

			switch (matrix[r][c]) {
			case 0:
				++cnt;
				++matrix[r][c];
				if (r > 0 &&	\
				    !matrix[r - 1][c] &&	\
				    get_dsum(r - 1) + get_dsum(c) <= k) {
					++si;
					rec[si] = {.r = r - 1, .c = c};
					break;
				}
			case 1:
				++matrix[r][c];
				if (r < m - 1 &&	\
				    !matrix[r + 1][c] &&	\
				    get_dsum(r + 1) + get_dsum(c) <= k) {
					++si;
					rec[si] = {.r = r + 1, .c = c};
					break;
				}
			case 2:
				++matrix[r][c];
				if (c > 0 &&	\
				    !matrix[r][c - 1] &&	\
				    get_dsum(r) + get_dsum(c - 1) <= k) {
					++si;
					rec[si] = {.r = r, .c = c - 1};
					break;
				}
			case 3:
				++matrix[r][c];
				if (c < n - 1 &&	\
				    !matrix[r][c + 1] &&	\
				    get_dsum(r) + get_dsum(c + 1) <= k) {
					++si;
					rec[si] = {.r = r, .c = c + 1};
					break;
				}
			default:
				--si;
			}
		}

		return cnt;
	}
};
