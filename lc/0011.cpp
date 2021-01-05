class Solution {
public:
    int maxArea(vector<int>& height) {
        int l = 0, r = height.size() - 1;
        int max_water = 0;
        
        while(l < r) {
           int water = (r - l) * min(height[l], height[r]);
            max_water = max(max_water, water);
            if (height[l] < height[r])
                ++l;
            else
                --r;
        }
        
        return max_water;
    }
};
