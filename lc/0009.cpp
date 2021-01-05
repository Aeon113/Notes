class Solution {
public:
    bool isPalindrome(int x) {
        if (x < 0)
            return false;
        
        int tmp = 1;
        while (x / tmp > 9)
            tmp *= 10;
        
        while (tmp > 9) {
            if (x / tmp != x % 10)
                return false;
            x -= x / tmp * tmp;
            x /= 10;
            tmp /= 100;
        }
        
        return true;
    }
};
