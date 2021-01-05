class Solution {
public:
    int myAtoi(string s) {
        int len = s.length();
        long result = 0;
        int negative = 0;
        int i = 0;
        
        // 1. Discard whitespaces
        while (i < len && s[i] == ' ')
            ++i;
        if (i == len)
            return result;
        
        // 2. Check '+' and '-'
        if (s[i] == '+')
            ++i;
        else if (s[i] == '-') {
            ++i;
            negative = 1;
        }
        // if (i == len)
            // return result;
        
        // 3. Parse Numners
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            result *= 10;
            result += s[i] - '0';
            if (negative && -result <= INT_MIN)
                return INT_MIN;
            else if (!negative && result >= INT_MAX)
                return INT_MAX;
            ++i;
        }
        
        return negative ? -result : result;
    }
};
